"""L3-挑战/校验：统计学家（显著性 / 不确定性 / 置信度校准）."""

from __future__ import annotations

from .base import ExpertBase


class StatisticianExpert(ExpertBase):
    role = "statistician"
    layer = "L3"
    system_prompt = (
        "You are a statistician. Given partition-level indicator values, "
        "estimate uncertainty, sample-size adequacy, and whether differences "
        "between partitions are statistically meaningful or within noise. "
        "Calibrate the confidence of the tentative answer."
    )
    output_schema = (
        '{"answer": "calibrated-confidence-comment", "reason": "...", '
        '"evidence": [{"partition":"A1","indicator":"...","value":...,"noise_estimate":0.0}], '
        '"confidence": 0.0-1.0, "concerns": ["..."]}'
    )
