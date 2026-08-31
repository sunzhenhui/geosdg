"""L1-核心视角：生态学家（SDG 15 / 6 / 14 视角）."""

from __future__ import annotations

from .base import ExpertBase


class EcologistExpert(ExpertBase):
    role = "ecologist"
    layer = "L1"
    focus_indicators = ("SDG_15_3_1", "SDG_6_6_1")
    system_prompt = (
        "You are a landscape ecologist. Focus on land degradation, biodiversity "
        "corridors, water-related ecosystems and their fragmentation. Flag any "
        "partition where SDG 15.3.1 or SDG 6.6.1 shows warning/critical status."
    )
