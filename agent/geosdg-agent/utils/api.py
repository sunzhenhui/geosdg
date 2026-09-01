"""LLM 调用入口（Protocol + Provider 注入）.

设计原则：
- Agent 自身不管理 API Key / HTTP 调用，只定义 LLMProvider 协议
- 宿主平台（WorkBuddy 等）通过 set_provider() 注入真实 LLM 实现
- 未注入时走 MockProvider（骨架零依赖可跑）
- 内置 OpenAICompatibleProvider 供独立运行时使用（可选）

用法示例：
    # 宿主注入
    from agent.geosdg_agent.utils import api
    api.set_provider(MyWorkBuddyProvider())

    # 独立运行
    from agent.geosdg_agent.utils.api import OpenAICompatibleProvider
    api.set_provider(OpenAICompatibleProvider(api_key="sk-..."))

    # copilot 层注入
    from agent.geosdg_agent.copilot import sdg_copilot
    sdg_copilot(region_data, task, task_type, llm_provider=my_provider)
"""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any, Protocol, runtime_checkable

from . import common


# ============================================================================
# Protocol
# ============================================================================

@runtime_checkable
class LLMProvider(Protocol):
    """LLM 调用协议——宿主平台注入 LLM 能力时需实现此接口."""

    def chat(self, messages: list[dict[str, Any]], **kwargs: Any) -> str:
        """完成一轮对话，返回 LLM 原始文本（约定为 JSON 字符串）.

        Args:
            messages: OpenAI 风格的 messages 列表.
            **kwargs: 透传字段（model / temperature / max_tokens 等）.

        Returns:
            LLM 返回的原始文本.
        """
        ...


# ============================================================================
# Provider 注册与调用入口
# ============================================================================

_provider: LLMProvider | None = None


def set_provider(provider: LLMProvider) -> None:
    """注入 LLM 提供者（由宿主平台或 copilot 入口调用）."""
    global _provider
    _provider = provider


def get_provider() -> LLMProvider:
    """获取当前 LLM 提供者（未注入时返回 MockProvider 单例）."""
    global _provider
    if _provider is None:
        _provider = MockProvider()
    return _provider


def chat(messages: list[dict[str, Any]], **kwargs: Any) -> str:
    """调用 LLM 完成一轮对话（委托给当前 provider）.

    Args:
        messages: OpenAI 风格的 messages 列表.
        **kwargs: 透传给 provider（model / temperature / max_tokens 等）.

    Returns:
        LLM 返回的原始文本（约定为 JSON 字符串，供上层 json.loads 解析）.
    """
    return get_provider().chat(messages, **kwargs)


def select(question: str, candidates: dict[str, Any]) -> dict[str, Any]:
    """让 LLM 从 candidates 里挑出与 question 相关的子集.

    未接入 LLM 时全量返回；此处保持全量返回，不影响主流程.
    """
    _ = question
    return dict(candidates)


# ============================================================================
# 工具函数
# ============================================================================

def clean_json_text(text: str) -> str:
    """清洗 LLM 输出：剥离 ```json 代码块围栏，尽量返回可 json.loads 的纯 JSON.

    未遵守 JSON mode 时，模型可能带围栏或前后杂言，这里做一层容错;
    无法提取时原样返回（上层 _parse 仍有退化分支兜底）.
    """
    s = text.strip()
    if s.startswith("```"):
        s = s.split("\n", 1)[-1] if "\n" in s else s
        if s.endswith("```"):
            s = s[: -3]
        s = s.strip()
        if s.lower().startswith("json"):
            s = s[4:].strip()
    if not s.startswith("{"):
        start = s.find("{")
        end = s.rfind("}")
        if start != -1 and end != -1 and end > start:
            s = s[start: end + 1]
    return s


# ============================================================================
# 内置 Provider：Mock（默认兜底）
# ============================================================================

class MockProvider:
    """Mock 提供者——未注入真实 LLM 时的兜底，返回可 json.loads 的占位 JSON."""

    def chat(self, messages: list[dict[str, Any]], **kwargs: Any) -> str:
        last_user = ""
        for msg in reversed(messages):
            if msg.get("role") == "user":
                content = msg.get("content", "")
                last_user = content if isinstance(content, str) else str(content)
                break
        mock = {
            "answer": "[mock] agent 骨架尚未接入 LLM",
            "reason": f"received {len(messages)} messages; "
                      f"last user preview: {last_user[:80]!r}",
        }
        return json.dumps(mock, ensure_ascii=False)


# ============================================================================
# 内置 Provider：OpenAI 兼容接口（独立运行时可选）
# ============================================================================

class OpenAICompatibleProvider:
    """OpenAI 兼容接口提供者——独立运行时可用.

    混元/通义/文心/DeepSeek 等均有 OpenAI 兼容端点，均可通过此 Provider 接入.
    依赖仅标准库 urllib，无需第三方 SDK.

    Args:
        api_key: API Key.
        base_url: 兼容端点根地址（如 ``https://api.deepseek.com/v1``）.
        model: 默认模型名.
        temperature: 采样温度.
        max_tokens: 单次回复最大 token 数.
        timeout: 请求超时（秒）.
    """

    def __init__(
        self,
        api_key: str,
        base_url: str = "https://api.openai.com/v1",
        model: str = "gpt-4o-mini",
        temperature: float = 0.2,
        max_tokens: int = 1024,
        timeout: float = 60.0,
    ) -> None:
        self.api_key = api_key
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.temperature = temperature
        self.max_tokens = max_tokens
        self.timeout = timeout

    def chat(self, messages: list[dict[str, Any]], **kwargs: Any) -> str:
        real_model = kwargs.get("model", self.model)
        if real_model and real_model.startswith("mock"):
            real_model = self.model

        payload: dict[str, Any] = {
            "model": real_model,
            "messages": messages,
            "temperature": kwargs.get("temperature", self.temperature),
            "max_tokens": kwargs.get("max_tokens", self.max_tokens),
            "response_format": {"type": "json_object"},
        }
        url = self.base_url + "/chat/completions"
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.api_key}",
            },
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=self.timeout) as resp:
            body = json.loads(resp.read().decode("utf-8"))
        content = body["choices"][0]["message"]["content"]
        return clean_json_text(content if isinstance(content, str) else str(content))
