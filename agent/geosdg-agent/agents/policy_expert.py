"""L2-领域知识：政策红线专家（对标 PEACE seismologist，改为继承 ExpertBase）."""

from __future__ import annotations

from typing import Any

from .base import ExpertBase


class PolicyExpert(ExpertBase):
    role = "policy"
    layer = "L2"
    system_prompt = (
        "You are a Chinese land-use policy analyst. Check every proposed action "
        "against the three redlines (生态保护红线 / 永久基本农田 / 城镇开发边界). "
        "Do NOT propose new actions; only flag policy conflicts."
    )

    def get_knowledge(self, bbox: tuple[float, float, float, float]) -> dict[str, Any]:
        """按 bbox 召回政策图层的相关信息（DKI 阶段调用）."""
        _ = bbox
        return {
            "ecological_redline_coverage": 0.32,
            "permanent_farmland_coverage": 0.21,
            "urban_growth_boundary_coverage": 0.18,
            "conflict_hint": "partition A1 possibly overlaps ecological redline",
        }
