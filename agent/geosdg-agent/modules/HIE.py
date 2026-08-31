"""Hierarchical Partition Extraction (HIE-SDG).

对标 PEACE/modules/HIE.py。
干的事：把研究区分区 → 每区计算 SDG 指标 → 每区打 UN 状态标签 → 输出 sdg_meta.json.
"""

from __future__ import annotations

import json
from dataclasses import asdict
from pathlib import Path
from typing import Any

from ..tool_pool import IndicatorCalculator, PartitionDetector, UnThresholdDB
from ..utils import common, vision


class HierarchicalPartitionExtraction:
    """PEACE 里的 `hierarchical_information_extraction` 类的 SDG 版本."""

    def __init__(
        self,
        detector: PartitionDetector | None = None,
        calculator: IndicatorCalculator | None = None,
        threshold_db: UnThresholdDB | None = None,
    ) -> None:
        self.detector = detector or PartitionDetector()
        self.calculator = calculator or IndicatorCalculator()
        self.threshold_db = threshold_db or UnThresholdDB()

    def digitalize(
        self,
        region_data: dict[str, Any],
        indicators: list[str] | None = None,
    ) -> dict[str, Any]:
        """对研究区做分区+指标+标签，产出 meta（对标 PEACE hie.digitalize）.

        Args:
            region_data: {"name","bbox","lucc_path",...}.
            indicators: 想算的 SDG 指标；默认取阈值库里全部.

        Returns:
            {
              "region": {...},
              "partitions": {A1: {...}, ...},
              "legend":     {A1: {SDG_11_3_1: {value, unit, un_status}, ...}, ...},
              "information": {...}
            }
        """
        indicators = indicators or self.threshold_db.all_indicators()

        cache = common.ensure_cache_dir("hie")
        partitions = self.detector.detect(region_data)

        # 1) 对每个分区裁剪出小 tif（骨架版写占位文件）
        crops: list[tuple[str, str]] = []
        for p in partitions:
            crop = vision.crop_by_bbox(
                src_path=region_data.get("lucc_path", "mock.tif"),
                bbox=p.bbox,
                dst_path=cache / f"{p.id}.tif",
            )
            p.crop_path = crop
            crops.append((p.id, crop))

        # 2) 每分区计算指标
        values = self.calculator.compute_batch(crops, indicators)

        # 3) 每指标打 UN 状态标签
        legend: dict[str, dict[str, Any]] = {}
        for pid, indv_map in values.items():
            legend[pid] = {}
            for ind, iv in indv_map.items():
                legend[pid][ind] = {
                    "value": iv.value,
                    "unit": iv.unit,
                    "year": iv.year,
                    "un_status": self.threshold_db.classify(ind, iv.value),
                }

        meta: dict[str, Any] = {
            "region": {
                "name": region_data.get("name", "unnamed"),
                "bbox": region_data.get("bbox"),
            },
            "partitions": {p.id: asdict(p) for p in partitions},
            "legend": legend,
            "information": {
                "indicators": indicators,
                "resolution": region_data.get("resolution", "unknown"),
                "time": region_data.get("year", None),
            },
        }

        # 落盘一份，方便下游/调试
        (Path(cache) / "sdg_meta.json").write_text(
            json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        return meta
