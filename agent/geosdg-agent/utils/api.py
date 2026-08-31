"""LLM 调用封装（对标 PEACE/utils/api.py）.

单点收口：所有专家/模块通过 chat() / select() 调用 LLM.
- 未配置 API Key（common.llm_enabled() 为 False）时返回 mock JSON，骨架仍可跑;
- 配置后走 OpenAI 兼容接口（混元/通义/文心/DeepSeek 等兼容端点均可）;
- 真实调用异常（网络/超时/鉴权/解析失败）自动回退 mock，保证 pipeline 不中断.

依赖仅标准库 urllib，无需第三方 SDK.
"""

from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any

from . import common


def chat(messages: list[dict[str, Any]], model: str = "mock", **kwargs: Any) -> str:
    """调用 LLM 完成一轮对话.

    Args:
        messages: OpenAI 风格的 messages 列表.
        model: 模型名；传入的 "mock-*" 占位名会被 common.LLM_MODEL 覆盖.
        **kwargs: 透传字段（temperature / max_tokens 等），覆盖默认配置.

    Returns:
        LLM 返回的原始文本（约定为 JSON 字符串，供上层 json.loads 解析）.
    """
    if not common.llm_enabled():
        return _mock_response(messages)
    try:
        return _chat_openai_compatible(messages, model, **kwargs)
    except Exception as exc:  # 网络/超时/鉴权/解析失败 → 回退 mock，pipeline 不中断
        if common.echo:
            print(f"[api.chat] real LLM failed, fallback to mock: {exc}")
        return _mock_response(messages)


def select(question: str, candidates: dict[str, Any]) -> dict[str, Any]:
    """让 LLM 从 candidates 里挑出与 question 相关的子集（对标 PEACE 的 select）.

    未接入 LLM 时全量返回；此处保持全量返回，不影响主流程（知识过滤为可选优化）.
    """
    _ = question
    return dict(candidates)


# ============================================================================
# 真实 LLM：OpenAI 兼容 Chat Completions
# ============================================================================

def _chat_openai_compatible(
    messages: list[dict[str, Any]], model: str, **kwargs: Any
) -> str:
    """走 OpenAI 兼容 /chat/completions 端点，返回 assistant 文本内容."""
    # 传入的占位模型名（mock / mock-*）一律用配置的真实模型
    real_model = model if model and not model.startswith("mock") else common.LLM_MODEL

    payload: dict[str, Any] = {
        "model": real_model,
        "messages": messages,
        "temperature": kwargs.get("temperature", common.LLM_TEMPERATURE),
        "max_tokens": kwargs.get("max_tokens", common.LLM_MAX_TOKENS),
        # 约定专家输出为 JSON，请求 JSON mode 以兼容上层 json.loads
        "response_format": {"type": "json_object"},
    }
    url = common.LLM_BASE_URL.rstrip("/") + "/chat/completions"
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {common.LLM_API_KEY}",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=common.LLM_TIMEOUT) as resp:
        body = json.loads(resp.read().decode("utf-8"))
    content = body["choices"][0]["message"]["content"]
    return _clean_json_text(content if isinstance(content, str) else str(content))


def _clean_json_text(text: str) -> str:
    """清洗 LLM 输出：剥离 ```json 代码块围栏，尽量返回可 json.loads 的纯 JSON.

    未开启/未遵守 JSON mode 时，模型可能带围栏或前后杂言，这里做一层容错;
    无法提取时原样返回（上层 _parse 仍有退化分支兜底）.
    """
    s = text.strip()
    if s.startswith("```"):
        # 去掉首行 ```json / ``` 与尾部 ```
        s = s.split("\n", 1)[-1] if "\n" in s else s
        if s.endswith("```"):
            s = s[: -3]
        s = s.strip()
        if s.lower().startswith("json"):
            s = s[4:].strip()
    # 若首尾非 JSON，尝试截取第一个 { 到最后一个 }
    if not s.startswith("{"):
        start = s.find("{")
        end = s.rfind("}")
        if start != -1 and end != -1 and end > start:
            s = s[start: end + 1]
    return s


# ============================================================================
# Mock 兜底
# ============================================================================

def _mock_response(messages: list[dict[str, Any]]) -> str:
    """未配置 LLM 或真实调用失败时的占位返回（可被 json.loads 解析）."""
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
