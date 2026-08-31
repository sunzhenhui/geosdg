"""分类图例知识库（对标 PEACE/tool_pool/rock_type_and_age_db，与 un_threshold_db 同构）.

把散落在 partition_detector 里的"地类码→名称→PartitionKind"映射抽成一个
可初始化、可复用的工具。支持从 data/configs/legends.json 外挂加载多套图例
（lucc / lucc_alternative / ...），加载失败时回退内置默认，保证 pipeline 不中断.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Literal

from ..utils import common

#: PartitionKind 与 partition_detector 保持一致（此处仅作类型标注用途）.
PartitionKind = Literal[
    "urban", "rural", "cropland", "forest",
    "water", "grassland", "wetland", "others",
]

#: legend 名称 → PartitionKind 的语义映射.
#: 覆盖 lucc（FROM-GLC）与 lucc_alternative（ESA CCI）两套图例的名称;
#: 未收录的名称统一归 "others".
NAME_TO_KIND: dict[str, PartitionKind] = {
    # --- lucc (FROM-GLC 2017) ---
    "cropland": "cropland",
    "forest": "forest",
    "grassland": "grassland",
    "water": "water",
    "urban": "urban",
    "bareland": "others",
    "rural": "rural",
    "wetland": "wetland",
    # --- lucc_alternative (ESA CCI) ---
    "rainfed_cropland": "cropland",
    "irrigated_cropland": "cropland",
    "mosaic_cropland": "cropland",
    "mosaic_vegetation": "grassland",
    "broadleaf_evergreen_forest": "forest",
    "broadleaf_deciduous_forest": "forest",
    "needleleaf_evergreen_forest": "forest",
    "needleleaf_deciduous_forest": "forest",
    "mixed_forest": "forest",
    "shrubland": "grassland",
    "sparse_vegetation": "grassland",
    "flooded_vegetation": "wetland",
    "bare_area": "others",
    "permanent_snow_ice": "others",
}

#: legends.json 缺失/损坏时的内置默认图例集（FROM-GLC 2017）.
DEFAULT_LEGENDS: dict[str, dict[int, str]] = {
    "lucc": {
        1: "cropland", 2: "forest", 3: "grassland",
        4: "water", 5: "urban", 6: "bareland",
    },
}


class LegendDB:
    """分类图例库：地类码 ↔ 名称 ↔ PartitionKind.

    可初始化时选定图例集（默认 "lucc"），从 data/configs/legends.json 加载;
    加载失败或图例集不存在时回退内置 DEFAULT_LEGENDS.
    """

    def __init__(
        self,
        legend_set: str = "lucc",
        legends_file: str | Path | None = None,
    ) -> None:
        """
        Args:
            legend_set: 使用哪套图例（如 "lucc" / "lucc_alternative"）.
            legends_file: 图例文件路径；默认 data/configs/legends.json.
        """
        self.legend_set = legend_set
        self.legends_file = (
            Path(legends_file)
            if legends_file
            else common.DATA_DIR / "configs" / "legends.json"
        )
        self._all: dict[str, dict[int, str]] = self._load_all()
        self._legend: dict[int, str] = self._resolve_set(legend_set)

    # ------------------------------------------------------------------
    # 加载
    # ------------------------------------------------------------------
    def _load_all(self) -> dict[str, dict[int, str]]:
        """加载 legends.json 里的所有图例集（剔除 _description/_source 等元字段）."""
        try:
            if self.legends_file.exists():
                raw = json.loads(self.legends_file.read_text(encoding="utf-8"))
                parsed: dict[str, dict[int, str]] = {}
                for set_name, mapping in raw.items():
                    if set_name.startswith("_") or not isinstance(mapping, dict):
                        continue
                    codes = {
                        int(k): str(v)
                        for k, v in mapping.items()
                        if k.lstrip("-").isdigit()
                    }
                    if codes:
                        parsed[set_name] = codes
                if parsed:
                    return parsed
        except Exception as exc:
            if common.echo:
                print(f"[LegendDB] load legends fallback: {exc}")
        return {k: dict(v) for k, v in DEFAULT_LEGENDS.items()}

    def _resolve_set(self, legend_set: str) -> dict[int, str]:
        """取出指定图例集，缺失时回退默认 lucc（再无则空表）."""
        if legend_set in self._all:
            return self._all[legend_set]
        if common.echo:
            print(
                f"[LegendDB] legend set '{legend_set}' not found, "
                f"available={list(self._all.keys())}, fallback to defaults"
            )
        return dict(DEFAULT_LEGENDS.get(legend_set, DEFAULT_LEGENDS["lucc"]))

    # ------------------------------------------------------------------
    # 查询
    # ------------------------------------------------------------------
    def code_to_name(self, class_code: int | None) -> str | None:
        """地类码 → 名称（当前图例集），未知/None 返回 None."""
        if class_code is None:
            return None
        return self._legend.get(int(class_code))

    def assign_kind(self, class_code: int | None) -> PartitionKind:
        """地类码 → PartitionKind，未知/None 归 "others"."""
        name = self.code_to_name(class_code)
        if name is None:
            return "others"
        return NAME_TO_KIND.get(name, "others")

    def name_to_kind(self, name: str | None) -> PartitionKind:
        """名称 → PartitionKind，未知/None 归 "others"."""
        if not name:
            return "others"
        return NAME_TO_KIND.get(name, "others")

    def available_sets(self) -> list[str]:
        """返回已加载的所有图例集名称."""
        return list(self._all.keys())

    def as_dict(self) -> dict[int, str]:
        """返回当前图例集的地类码→名称映射（副本）."""
        return dict(self._legend)

    def use(self, legend_set: str) -> None:
        """切换当前图例集."""
        self.legend_set = legend_set
        self._legend = self._resolve_set(legend_set)
