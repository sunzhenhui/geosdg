"""GeoSDG-Agent 入口：空间可持续发展决策 Copilot.

三段式主流程 HIE → DKI → PEDA：
    HIE  读研究区栅格 → 分区 + 指标 + UN 标签（"现状长什么样"）
    DKI  按任务召回 UN 阈值 / 政策红线 / 气候情景 / 模拟可信度（"有哪些约束"）
    PEDA 专家团合议 → 带证据链的决策建议（"该怎么办"）

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
    PartitionExpertDecisionAssembly,
    SDGKnowledgeInjection,
)
from .utils import common, prompt

# 单例（三段式主流程的三个模块实例）
hie = HierarchicalPartitionExtraction()
dki = SDGKnowledgeInjection()
peda = PartitionExpertDecisionAssembly()


def sdg_copilot(
    region_data: dict[str, Any],
    task: str,
    task_type: str,
    copilot_modes: list[str] | None = None,
    llm_provider: Any | None = None,
) -> Any:
    """GeoSDG-Agent 三段式决策会诊主流程.

    Args:
        region_data: {"name","bbox","lucc_path",...}.
        task: 决策任务描述（自然语言）.
        task_type: 决策任务 key，见 utils/prompt.py::task_ability2type.
        copilot_modes: 需要启用的阶段集合，默认全开.
        llm_provider: LLM 提供者（实现 api.LLMProvider 协议），
                      由宿主平台注入；未提供时走 MockProvider.

    Returns:
        决策结论（若合议结果是 JSON 则解析为 dict）.
    """
    if llm_provider is not None:
        from .utils import api
        api.set_provider(llm_provider)

    copilot_modes = copilot_modes or ["HIE", "DKI", "PEDA"]

    if common.rai_filter(task):
        return "I can't help you with that."

    # HIE — 分区 + 指标 + UN 标签
    information = hie.digitalize(region_data) if "HIE" in copilot_modes else None

    # DKI — UN/政策/气候/模拟知识注入
    knowledge = dki.consult(task, information) if "DKI" in copilot_modes else None

    # PEDA — 专家团合议
    raw = peda.consult(
        information,
        knowledge,
        "PEDA" in copilot_modes,
        region_data,
        task,
        task_type,
    )

    try:
        parsed = json.loads(raw)
        final = prompt.get_final_answer(parsed, task_type)
    except Exception:
        final = raw

    if common.echo:
        print("Selected knowledge:", list(knowledge.keys()) if knowledge else knowledge)
        print("Raw Verdict:", raw)
        print("Final Decision:", final)
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
    demo_task = "在当前分区中，哪几个应被列为优先干预区？"
    demo_type = "analyzing-priority_area"
    decision = sdg_copilot(demo_region, demo_task, demo_type)
    print(f"\n[DECISION] {decision!r}")
