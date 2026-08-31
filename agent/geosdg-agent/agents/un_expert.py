"""L2-领域知识：UN 官方口径专家（对标 PEACE geographer，改为继承 ExpertBase）."""

from __future__ import annotations

from typing import Any

from ..tool_pool import UnThresholdDB
from .base import ExpertBase


class UnExpert(ExpertBase):
    role = "un"
    layer = "L2"
    system_prompt = (
        "You are a UN SDG monitoring specialist. Align every indicator value "
        "with UN official thresholds and metadata. Do NOT propose actions; only "
        "certify whether the assessment methodology and interpretation follow "
        "UN documents."
    )

    def __init__(self) -> None:
        self.db = UnThresholdDB()

    def get_knowledge(self, indicators: list[str]) -> dict[str, Any]:
        """按指标返回 UN 阈值定义与解读（DKI 阶段调用）."""
        out: dict[str, Any] = {}
        for ind in indicators:
            th = self.db.get(ind)
            if th is None:
                continue
            out[ind] = {
                "warning": th.warning,
                "critical": th.critical,
                "direction": th.direction,
                "unit": th.unit,
                "source": th.source,
                "interpretation": th.interpretation,
            }
        return out
