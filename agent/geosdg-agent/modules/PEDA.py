"""Partition Expert Decision Assembly (PEDA-SDG).

三段式主流程的第三段：拿到分区现状（HIE）与注入知识（DKI）后，
把决策任务交给 Moderator 组织**多专家合议**，产出带证据链的决策建议。

不是"解题"，而是"会诊"：Moderator 按 routing.ROUTING_TABLE 召唤对应专家池，
汇总多视角意见（核心建议 / 约束红线 / 反方挑战），返回结构化裁决.
"""

from __future__ import annotations

import json
from typing import Any

from ..agents import Moderator
from ..utils import api, prompt


class PartitionExpertDecisionAssembly:
    """决策会诊装配：把一个决策任务交给专家团合议."""

    def __init__(self, moderator: Moderator | None = None) -> None:
        self.moderator = moderator or Moderator()

    def consult(
        self,
        sdg_meta: dict[str, Any] | None,
        knowledge: dict[str, Any] | None,
        use_expert_panel: bool,
        region_data: dict[str, Any],
        task: str,
        task_type: str,
    ) -> str:
        """产出决策会诊结果（返回 JSON 字符串，copilot 层负责解析）."""
        # Baseline：不召集专家团 → 直接问 LLM，一句话建议
        if not use_expert_panel:
            messages = [
                {"role": "system", "content": prompt.system_prompt},
                {"role": "user", "content": task},
            ]
            return api.chat(messages, model="mock-baseline")

        # 1) 挑选参与本次会诊的分区（骨架版：全选）
        selected = self._select_partitions(sdg_meta, task_type)
        meta_slim: dict[str, Any] = {
            "region": (sdg_meta or {}).get("region", region_data),
            "partitions": {pid: (sdg_meta or {}).get("partitions", {}).get(pid) for pid in selected},
            "legend":     {pid: (sdg_meta or {}).get("legend", {}).get(pid) for pid in selected},
            "information": (sdg_meta or {}).get("information", {}),
        }

        # 2) 交给 Moderator 组织合议
        verdict = self.moderator.deliberate(
            task=task,
            task_type=task_type,
            sdg_meta=meta_slim,
            knowledge=knowledge or {},
        )

        # 3) 输出 JSON 字符串
        return json.dumps(verdict, ensure_ascii=False, default=str)

    # ------------------------------------------------------------------
    # helpers
    # ------------------------------------------------------------------

    def _select_partitions(
        self,
        sdg_meta: dict[str, Any] | None,
        task_type: str,
    ) -> list[str]:
        """按决策任务挑选相关分区。骨架版：全选。"""
        _ = task_type
        if not sdg_meta:
            return []
        return list(sdg_meta.get("partitions", {}).keys())
