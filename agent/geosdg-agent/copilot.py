"""GeoSDG-Agent 入口（对标 PEACE/copilot.py）.

调用方式：
    from agent.geosdg_agent.copilot import sdg_copilot
    ans = sdg_copilot(region_data, "该优先干预哪个分区？", "analyzing-priority_area")

或直接：
    python agent/geosdg-agent/copilot.py
"""

from __future__ import annotations

import json
from typing import Any

from .modules import (
    HierarchicalPartitionExtraction,
    PromptEnhancedPartitionQA,
    SDGKnowledgeInjection,
)
from .utils import common, prompt

# 单例（对标 PEACE 顶层 hie/dki/peqa 三个模块实例）
hie = HierarchicalPartitionExtraction()
dki = SDGKnowledgeInjection()
peqa = PromptEnhancedPartitionQA()


def sdg_copilot(
    region_data: dict[str, Any],
    question: str,
    question_type: str,
    copilot_modes: list[str] | None = None,
) -> Any:
    """GeoSDG-Agent 三段式主流程.

    Args:
        region_data: {"name","bbox","lucc_path",...}.
        question: 用户问题.
        question_type: 题型 key，见 utils/prompt.py::question_ability2type.
        copilot_modes: 需要启用的模块集合，默认全开.

    Returns:
        最终答案（若 LLM 返回是 JSON 则解析为 dict）.
    """
    copilot_modes = copilot_modes or ["HIE", "DKI", "PEQA"]

    if common.rai_filter(question):
        return "I can't help you with that."

    # HIE — 分区+指标+标签
    information = hie.digitalize(region_data) if "HIE" in copilot_modes else None

    # DKI — 政策/UN 知识注入
    knowledge = dki.consult(question, information) if "DKI" in copilot_modes else None

    # PEQA — 自动 prompt + LLM
    raw = peqa.answer(
        information,
        knowledge,
        "PEQA" in copilot_modes,
        region_data,
        question,
        question_type,
    )

    try:
        parsed = json.loads(raw)
        final = prompt.get_final_answer(parsed, question_type)
    except Exception:
        final = raw

    if common.echo:
        print("Selected knowledge:", list(knowledge.keys()) if knowledge else knowledge)
        print("Raw Answer:", raw)
        print("Final Answer:", final)
        print("======================================================")
    return final


if __name__ == "__main__":
    # 空流程 smoke test
    demo_region = {
        "name": "Demo City",
        "bbox": (113.0, 22.0, 114.5, 23.5),
        "lucc_path": "data/rasters/lucc/2020.tif",
        "resolution": "30m",
        "year": 2020,
    }
    demo_question = "在当前分区中，哪几个应被列为优先干预区？"
    demo_type = "analyzing-priority_area"
    answer = sdg_copilot(demo_region, demo_question, demo_type)
    print(f"\n[FINAL] {answer!r}")
