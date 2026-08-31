"""UN 阈值/达标口径知识库（对标 PEACE/tool_pool/rock_type_and_age_db）.

PEACE 用它给每个图例色块打上"岩性+年代"知识标签，
我们用它给每个分区的每个指标打上"UN 状态"标签.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

UnStatus = Literal["healthy", "warning", "critical", "unknown"]


@dataclass
class Threshold:
    """单个指标的 UN 阈值定义."""
    indicator: str
    warning: float       # 进入警戒的值
    critical: float      # 进入危险的值
    direction: Literal["higher_worse", "lower_worse"]  # 高/低哪端更差
    unit: str
    source: str          # UN 文档出处
    interpretation: str  # 一句话解读


class UnThresholdDB:
    """内置一份最小 UN 阈值表.

    骨架阶段用硬编码，后续可换 YAML/JSON 外挂.
    """

    _TABLE: dict[str, Threshold] = {
        "SDG_11_3_1": Threshold(
            indicator="SDG_11_3_1",
            warning=1.0, critical=2.0,
            direction="higher_worse", unit="ratio",
            source="UN Habitat SDG 11.3.1 metadata",
            interpretation="Land consumption rate to population growth rate; "
                           "value > 1 means urban sprawl outpaces population growth.",
        ),
        "SDG_15_3_1": Threshold(
            indicator="SDG_15_3_1",
            warning=0.05, critical=0.10,
            direction="higher_worse", unit="ratio",
            source="UNCCD SDG 15.3.1 Good Practice Guidance",
            interpretation="Proportion of land that is degraded over total land area.",
        ),
        "SDG_13_2_2": Threshold(
            indicator="SDG_13_2_2",
            warning=10.0, critical=20.0,
            direction="higher_worse", unit="t_CO2/ha",
            source="IPCC AR6 WGIII",
            interpretation="Per-hectare GHG emissions from land use.",
        ),
        "SDG_2_4_1": Threshold(
            indicator="SDG_2_4_1",
            warning=0.5, critical=0.3,
            direction="lower_worse", unit="ratio",
            source="FAO SDG 2.4.1 methodology",
            interpretation="Proportion of agricultural area under productive and sustainable agriculture.",
        ),
        "SDG_6_6_1": Threshold(
            indicator="SDG_6_6_1",
            warning=-0.05, critical=-0.15,
            direction="lower_worse", unit="ratio",
            source="UN-Water SDG 6.6.1",
            interpretation="Change in extent of water-related ecosystems.",
        ),
    }

    def get(self, indicator: str) -> Threshold | None:
        return self._TABLE.get(indicator)

    def classify(self, indicator: str, value: float) -> UnStatus:
        """根据阈值把数值划入 healthy / warning / critical."""
        th = self.get(indicator)
        if th is None:
            return "unknown"
        if th.direction == "higher_worse":
            if value >= th.critical:
                return "critical"
            if value >= th.warning:
                return "warning"
            return "healthy"
        # lower_worse
        if value <= th.critical:
            return "critical"
        if value <= th.warning:
            return "warning"
        return "healthy"

    def all_indicators(self) -> list[str]:
        return list(self._TABLE.keys())
