"""Prompt-Enhanced Partition QA (PEQA-SDG).

对标 PEACE/modules/PEQA.py，但已升级为「多专家合议」模式：
不再单独调用 PlannerExpert，而是把决策权交给 Moderator，由后者按
routing.ROUTING_TABLE 召唤对应专家池、汇总多视角意见.
"""

from __future__ import annotations

import json
from typing import Any

from ..agents import Moderator
from ..utils import api, prompt


class PromptEnhancedPartitionQA:
    """PEACE 里的 `prompt_enhanced_QA` 类的 SDG 版本（多专家合议版）."""

    def __init__(self, moderator: Moderator | None = None) -> None:
        self.moderator = moderator or Moderator()

    def answer(
        self,
        sdg_meta: dict[str, Any] | None,
        knowledge: dict[str, Any] | None,
        use_prompt_enhancement: bool,
        region_data: dict[str, Any],
        question: str,
        question_type: str,
    ) -> str:
        """产出最终答案（返回 JSON 字符串以对齐 PEACE 契约）."""
        # Baseline：无增强 → 直接问 LLM，一句话答案
        if not use_prompt_enhancement:
            messages = [
                {"role": "system", "content": prompt.system_prompt},
                {"role": "user", "content": question},
            ]
            return api.chat(messages, model="mock-baseline")

        # 1) 挑选参与本次问答的分区（骨架版：全选）
        selected = self._select_partitions(sdg_meta, question_type)
        meta_slim: dict[str, Any] = {
            "region": (sdg_meta or {}).get("region", region_data),
            "partitions": {pid: (sdg_meta or {}).get("partitions", {}).get(pid) for pid in selected},
            "legend":     {pid: (sdg_meta or {}).get("legend", {}).get(pid) for pid in selected},
            "information": (sdg_meta or {}).get("information", {}),
        }

        # 2) 交给 Moderator 组织合议
        verdict = self.moderator.deliberate(
            question=question,
            question_type=question_type,
            sdg_meta=meta_slim,
            knowledge=knowledge or {},
        )

        # 3) 输出 JSON 字符串（copilot 层负责解析）
        return json.dumps(verdict, ensure_ascii=False, default=str)

    # ------------------------------------------------------------------
    # helpers
    # ------------------------------------------------------------------

    def _select_partitions(
        self,
        sdg_meta: dict[str, Any] | None,
        question_type: str,
    ) -> list[str]:
        """按题型挑选相关分区。骨架版：全选。"""
        _ = question_type
        if not sdg_meta:
            return []
        return list(sdg_meta.get("partitions", {}).keys())
