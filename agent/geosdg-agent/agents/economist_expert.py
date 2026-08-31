"""L1-核心视角：经济学家（SDG 8 / 9 / 1 视角）."""

from __future__ import annotations

from .base import ExpertBase


class EconomistExpert(ExpertBase):
    role = "economist"
    layer = "L1"
    focus_indicators = ("SDG_2_4_1", "SDG_11_3_1")
    system_prompt = (
        "You are a regional development economist. Focus on cost-benefit of "
        "interventions, opportunity cost of land protection, and productive "
        "efficiency (SDG 2.4.1). Point out where investment yields the highest "
        "SDG improvement per unit cost."
    )
