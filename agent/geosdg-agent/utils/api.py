"""LLM 调用桩（对标 PEACE/utils/api.py）.

骨架阶段返回 mock JSON，后续可接入真实的 OpenAI/Claude/Hunyuan 等 SDK.
"""

from __future__ import annotations

import json
from typing import Any


def chat(messages: list[dict[str, Any]], model: str = "mock", **kwargs: Any) -> str:
    """调用 LLM 完成一轮对话.

    Args:
        messages: OpenAI 风格的 messages 列表.
        model: 模型名，骨架阶段忽略.
        **kwargs: 保留字段（temperature / max_tokens 等）.

    Returns:
        LLM 返回的原始文本（约定为 JSON 字符串）.
    """
    _ = model, kwargs
    last_user = ""
    for msg in reversed(messages):
        if msg.get("role") == "user":
            content = msg.get("content", "")
            last_user = content if isinstance(content, str) else str(content)
            break

    mock = {
        "answer": "[mock] agent 骨架尚未接入 LLM",
        "reason": f"received {len(messages)} messages; last user preview: {last_user[:80]!r}",
    }
    return json.dumps(mock, ensure_ascii=False)


def select(question: str, candidates: dict[str, Any]) -> dict[str, Any]:
    """让 LLM 从 candidates 里挑出与 question 相关的子集（对标 PEACE 的 select）.

    骨架阶段：全部返回.
    """
    _ = question
    return dict(candidates)
