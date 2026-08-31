"""L1-核心视角：社会学家（SDG 1 / 5 / 10 视角）."""

from __future__ import annotations

from .base import ExpertBase


class SociologistExpert(ExpertBase):
    role = "sociologist"
    layer = "L1"
    focus_indicators = ()  # 关注全指标，因社会影响横跨维度
    system_prompt = (
        "You are a social equity analyst. Focus on vulnerable groups, "
        "accessibility to services, and distributive fairness of interventions. "
        "Flag any partition where 'best-value' actions harm rural or low-income "
        "residents (SDG 1 / 5 / 10)."
    )
