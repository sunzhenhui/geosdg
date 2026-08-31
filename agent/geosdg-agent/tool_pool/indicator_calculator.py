"""指标计算器（对标 PEACE/tool_pool/map_legend_detector）.

PEACE 从图例里读岩性/年代，我们对每个分区调用 geosdg-cli 计算 SDG 指标.

真实实现：subprocess 调 geosdg-cli（当前已接入 sdg-1131 → SDG_11_3_1），
CLI 不支持或调用失败时回退 mock，保证骨架 pipeline 始终可跑通.
"""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
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
    source: str = "mock"    # "cli" 表示真实计算，"mock" 表示兜底假值


class IndicatorCalculator:
    """对分区批量计算 SDG 指标.

    - 对已接入 CLI 的指标（当前仅 SDG_11_3_1）走真实 geosdg-cli 计算.
    - 其余指标或 CLI 调用失败时回退 mock 值.
    """

    #: 支持的指标白名单
    SUPPORTED = (
        "SDG_2_4_1",   # 农业可持续性
        "SDG_6_6_1",   # 水体范围变化
        "SDG_11_3_1",  # 土地消耗率 / 人口增长率
        "SDG_13_2_2",  # 温室气体排放
        "SDG_15_3_1",  # 土地退化
    )

    #: 已接入真实 CLI 计算的指标 → CLI 子命令
    CLI_SUBCOMMANDS = {
        "SDG_11_3_1": "sdg-1131",
    }

    #: 缺省真实数据（整区双时相），供未显式传 extra 时使用
    DEFAULT_INPUTS = {
        "init_lucc": "rasters/lucc_demo_2010.tif",
        "curr_lucc": "rasters/lucc_demo_2020.tif",
        "init_popu": "rasters/pop_demo_2010.tif",
        "curr_popu": "rasters/pop_demo_2020.tif",
        "types": "1",
    }

    #: mock 兜底值
    MOCK_VALUES = {
        "SDG_2_4_1":   (0.65, "ratio"),
        "SDG_6_6_1":   (-0.12, "ratio"),
        "SDG_11_3_1":  (2.31, "ratio"),
        "SDG_13_2_2":  (18.4, "t_CO2/ha"),
        "SDG_15_3_1":  (0.08, "ratio"),
    }

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
            partition_crop_path: 分区裁剪片路径（真实 CLI 计算目前用整区双时相数据，此参数暂作占位）.
            indicator: SDG 指标 ID.
            extra: 额外参数（init_lucc/curr_lucc/init_popu/curr_popu/types/year 等）.

        Returns:
            IndicatorValue（source 字段标注 "cli" 或 "mock"）.
        """
        assert indicator in self.SUPPORTED, f"unsupported indicator: {indicator}"
        extra = extra or {}

        # 已接入 CLI 的指标：尝试真实计算，失败回退 mock
        if indicator in self.CLI_SUBCOMMANDS:
            iv = self._compute_via_cli(indicator, extra)
            if iv is not None:
                return iv

        # 兜底 mock
        v, u = self.MOCK_VALUES[indicator]
        return IndicatorValue(
            indicator=indicator, value=v, unit=u,
            year=extra.get("year", 2020), source="mock",
        )

    def _compute_via_cli(
        self, indicator: str, extra: dict[str, Any]
    ) -> IndicatorValue | None:
        """调 geosdg-cli 真实计算指标，返回 None 表示应回退 mock."""
        subcmd = self.CLI_SUBCOMMANDS[indicator]

        def _resolve(key: str) -> Path:
            raw = extra.get(key, self.DEFAULT_INPUTS[key])
            p = Path(raw)
            return p if p.is_absolute() else (common.DATA_DIR / raw)

        init_lucc = _resolve("init_lucc")
        curr_lucc = _resolve("curr_lucc")
        init_popu = _resolve("init_popu")
        curr_popu = _resolve("curr_popu")
        types = str(extra.get("types", self.DEFAULT_INPUTS["types"]))

        # 缺任一真实输入文件 → 回退 mock
        for f in (init_lucc, curr_lucc, init_popu, curr_popu):
            if not f.exists():
                if common.echo:
                    print(f"[IndicatorCalculator] missing input, fallback mock: {f}")
                return None
        if not Path(self.cli_binary).exists():
            if common.echo:
                print(f"[IndicatorCalculator] CLI not found, fallback mock: {self.cli_binary}")
            return None

        cmd = [
            self.cli_binary, subcmd,
            "--init-lucc", str(init_lucc),
            "--curr-lucc", str(curr_lucc),
            "--init-popu", str(init_popu),
            "--curr-popu", str(curr_popu),
            "--types", types,
        ]
        try:
            proc = subprocess.run(
                cmd, capture_output=True, text=True, timeout=300, check=False,
            )
        except (subprocess.SubprocessError, OSError) as exc:
            if common.echo:
                print(f"[IndicatorCalculator] CLI error, fallback mock: {exc}")
            return None

        if proc.returncode != 0:
            if common.echo:
                print(f"[IndicatorCalculator] CLI rc={proc.returncode}, fallback mock. "
                      f"stderr={proc.stderr.strip()[:200]}")
            return None

        value = self._parse_score(proc.stdout)
        if value is None:
            if common.echo:
                print(f"[IndicatorCalculator] cannot parse score, fallback mock. "
                      f"stdout={proc.stdout.strip()[:200]}")
            return None

        return IndicatorValue(
            indicator=indicator, value=value, unit="ratio",
            year=extra.get("year", 2020), source="cli",
        )

    @staticmethod
    def _parse_score(stdout: str) -> float | None:
        """从 CLI stdout 解析指标值（优先 ratio，其次 score）."""
        for key in ("ratio", "score"):
            m = re.search(rf"{key}\s*=\s*([-+]?\d+(?:\.\d+)?)", stdout)
            if m:
                return float(m.group(1))
        return None

    def compute_batch(
        self,
        partitions_with_crops: list[tuple[str, str]],
        indicators: list[str],
        extra_per_partition: dict[str, dict[str, Any]] | None = None,
    ) -> dict[str, dict[str, IndicatorValue]]:
        """批量计算.

        Args:
            partitions_with_crops: [(partition_id, crop_path), ...].
            indicators: 指标列表.
            extra_per_partition: {partition_id: extra_dict}，为每个分区提供
                真实 CLI 输入（init_lucc/curr_lucc/init_popu/curr_popu/types 等）；
                缺省时该分区走默认整区数据或回退 mock.

        Returns:
            {partition_id: {indicator: IndicatorValue}}.
        """
        extra_per_partition = extra_per_partition or {}
        result: dict[str, dict[str, IndicatorValue]] = {}
        for pid, crop in partitions_with_crops:
            extra = extra_per_partition.get(pid)
            result[pid] = {
                ind: self.compute(crop, ind, extra) for ind in indicators
            }
        return result
