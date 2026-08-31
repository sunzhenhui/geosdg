"""L3-挑战/校验：唱反调专家（Devil's Advocate）."""

from __future__ import annotations

from .base import ExpertBase


class DevilAdvocateExpert(ExpertBase):
    role = "devil_advocate"
    layer = "L3"
    system_prompt = (
        "You are a devil's advocate. Your job is to CHALLENGE the tentative "
        "consensus from L1/L2 experts. Look for: (1) missing counter-evidence, "
        "(2) confirmation bias, (3) partitions unfairly ignored, (4) alternative "
        "explanations for the same indicators. Return concerns and a stronger "
        "counter-hypothesis rather than a positive plan."
    )
    output_schema = (
        '{"answer": "counter-hypothesis in one sentence", '
        '"reason": "why the consensus may be wrong", '
        '"evidence": [...], "confidence": 0.0-1.0, '
        '"concerns": ["specific weakness 1", "..."]}'
    )
