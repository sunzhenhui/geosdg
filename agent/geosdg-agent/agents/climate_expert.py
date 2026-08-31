"""L2-领域知识：气候变化专家（SDG 13 视角）."""

from __future__ import annotations

from typing import Any

from .base import ExpertBase


class ClimateExpert(ExpertBase):
    role = "climate"
    layer = "L2"
    focus_indicators = ("SDG_13_2_2",)
    system_prompt = (
        "You are a climate scenario analyst. Interpret partitions under SSP1-5 "
        "trajectories, carbon peaking targets and land-use carbon sinks (SDG 13). "
        "Flag interventions inconsistent with 2030/2060 climate goals."
    )

    def get_knowledge(self, indicators: list[str]) -> dict[str, Any]:
        """DKI 可调；骨架版返回 mock 情景数据."""
        _ = indicators
        return {
            "carbon_peak_year_target": 2030,
            "carbon_neutrality_target": 2060,
            "scenarios_available": ["SSP1-RCP2.6", "SSP2-RCP4.5", "SSP5-RCP8.5"],
            "current_trajectory": "SSP2-RCP4.5",
        }
