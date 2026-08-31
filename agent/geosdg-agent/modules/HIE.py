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

        # 四期源栅格：优先用 region_data 指定，否则用 calculator 默认真实数据
        srcs = self._resolve_sources(region_data)

        # 1) 对每个分区裁剪四期栅格（lucc/pop 各两期），组装真实 CLI 输入
        crops: list[tuple[str, str]] = []
        extra_per_partition: dict[str, dict[str, Any]] = {}
        for p in partitions:
            pcache = common.ensure_cache_dir(f"hie/{p.id}")
            clipped: dict[str, str] = {}
            for key, src in srcs.items():
                clipped[key] = vision.crop_by_bbox(
                    src_path=src, bbox=p.bbox, dst_path=pcache / f"{key}.tif",
                )
            # 主裁剪片（lucc 现状）挂到分区上，供展示/下游引用
            p.crop_path = clipped.get("curr_lucc")
            # 按裁剪片主导地类回填分区类型（kind）
            dom = vision.dominant_class(p.crop_path) if p.crop_path else None
            p.kind = self.detector.assign_kind(dom)
            if dom is not None:
                p.tags["dominant_lucc_code"] = str(dom)
            crops.append((p.id, p.crop_path or ""))
            extra_per_partition[p.id] = {
                "init_lucc": clipped["init_lucc"],
                "curr_lucc": clipped["curr_lucc"],
                "init_popu": clipped["init_popu"],
                "curr_popu": clipped["curr_popu"],
                "types": region_data.get("types", "1"),
                "year": region_data.get("year"),
            }

        # 2) 每分区计算指标（带真实裁剪片路径 → 走真实 CLI）
        values = self.calculator.compute_batch(
            crops, indicators, extra_per_partition
        )

        # 3) 每指标打 UN 状态标签
        legend: dict[str, dict[str, Any]] = {}
        for pid, indv_map in values.items():
            legend[pid] = {}
            for ind, iv in indv_map.items():
                legend[pid][ind] = {
                    "value": iv.value,
                    "unit": iv.unit,
                    "year": iv.year,
                    "source": iv.source,
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

    def _resolve_sources(self, region_data: dict[str, Any]) -> dict[str, str]:
        """确定四期源栅格的绝对路径.

        优先用 region_data 指定（lucc_init_path/lucc_curr_path/pop_init_path/
        pop_curr_path），否则回退到 IndicatorCalculator.DEFAULT_INPUTS 的默认真实数据.
        相对路径按 data/ 解析.
        """
        defaults = self.calculator.DEFAULT_INPUTS
        mapping = {
            "init_lucc": region_data.get("lucc_init_path", defaults["init_lucc"]),
            "curr_lucc": region_data.get("lucc_curr_path", defaults["curr_lucc"]),
            "init_popu": region_data.get("pop_init_path", defaults["init_popu"]),
            "curr_popu": region_data.get("pop_curr_path", defaults["curr_popu"]),
        }
        resolved: dict[str, str] = {}
        for key, raw in mapping.items():
            p = Path(raw)
            resolved[key] = str(p if p.is_absolute() else (common.DATA_DIR / raw))
        return resolved
