"""L1-核心视角：国土空间规划师（对标 PEACE geologist，改为继承 ExpertBase）."""

from __future__ import annotations

from .base import ExpertBase


class PlannerExpert(ExpertBase):
    role = "planner"
    layer = "L1"
    focus_indicators = ("SDG_11_3_1",)
    system_prompt = (
        "You are a Chinese territorial-spatial planner. Focus on urban expansion, "
        "development boundaries, and land-consumption efficiency. Prioritize areas "
        "where urban sprawl outpaces population growth (SDG 11.3.1)."
    )
