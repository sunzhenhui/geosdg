"""指标计算器（对标 PEACE/tool_pool/map_legend_detector）.

PEACE 从图例里读岩性/年代，我们对每个分区调用 geosdg-cli 计算 SDG 指标.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from ..utils import common


@dataclass
class IndicatorValue:
    """一个分区上某个 SDG 指标的取值."""
    indicator: str          # e.g. "SDG_11_3_1"
    value: float
    unit: str               # e.g. "ratio", "t/ha", "%"
    year: int | None = None
    scenario: str | None = None  # e.g. "SSP1-RCP2.6"


class IndicatorCalculator:
    """对分区批量计算 SDG 指标.

    骨架实现：不调 CLI，直接返回 mock 值.
    真实实现应 subprocess.run([common.CLI_BINARY, "sdg-1131", ...]).
    """

    #: 支持的指标白名单
    SUPPORTED = (
        "SDG_2_4_1",   # 农业可持续性
        "SDG_6_6_1",   # 水体范围变化
        "SDG_11_3_1",  # 土地消耗率 / 人口增长率
        "SDG_13_2_2",  # 温室气体排放
        "SDG_15_3_1",  # 土地退化
    )

    def __init__(self, cli_binary: str | None = None) -> None:
        self.cli_binary = cli_binary or str(common.CLI_BINARY)

    def compute(
        self,
        partition_crop_path: str,
        indicator: str,
        extra: dict[str, Any] | None = None,
    ) -> IndicatorValue:
        """对单个分区计算单个指标.

        Args:
            partition_crop_path: 分区裁剪片路径.
            indicator: SDG 指标 ID.
            extra: 额外参数（年份/情景/辅助栅格等）.

        Returns:
            IndicatorValue.
        """
        _ = partition_crop_path, extra
        assert indicator in self.SUPPORTED, f"unsupported indicator: {indicator}"

        # TODO: subprocess 调 geosdg-cli
        mock_values = {
            "SDG_2_4_1":   (0.65, "ratio"),
            "SDG_6_6_1":   (-0.12, "ratio"),
            "SDG_11_3_1":  (2.31, "ratio"),
            "SDG_13_2_2":  (18.4, "t_CO2/ha"),
            "SDG_15_3_1":  (0.08, "ratio"),
        }
        v, u = mock_values[indicator]
        return IndicatorValue(indicator=indicator, value=v, unit=u, year=2020)

    def compute_batch(
        self,
        partitions_with_crops: list[tuple[str, str]],
        indicators: list[str],
    ) -> dict[str, dict[str, IndicatorValue]]:
        """批量计算.

        Args:
            partitions_with_crops: [(partition_id, crop_path), ...].
            indicators: 指标列表.

        Returns:
            {partition_id: {indicator: IndicatorValue}}.
        """
        result: dict[str, dict[str, IndicatorValue]] = {}
        for pid, crop in partitions_with_crops:
            result[pid] = {ind: self.compute(crop, ind) for ind in indicators}
        return result
