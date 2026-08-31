"""分区检测器（对标 PEACE/tool_pool/map_component_detector）.

PEACE 把地质图切成 8 类视觉组件（title/main_map/legend/...），
我们把研究区栅格切成 N 个"治理分区"（按 LUCC 类型或行政区）.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal

from ..utils import vision
from .legend_db import LegendDB

PartitionKind = Literal[
    "urban", "rural", "cropland", "forest",
    "water", "grassland", "wetland", "others",
]


@dataclass
class Partition:
    """一个治理分区."""
    id: str                                             # 分区编号，如 "A1"
    kind: PartitionKind                                 # 分区类型
    bbox: tuple[float, float, float, float]             # (xmin, ymin, xmax, ymax) lon/lat
    area_km2: float                                     # 面积
    crop_path: str | None = None                        # 裁剪出的小 tif 路径
    tags: dict[str, str] = field(default_factory=dict)  # 附加标签（行政区名等）


class PartitionDetector:
    """按规则把研究区切成若干 Partition.

    优先用 GDAL 读 lucc 真实四至，按 2x2 网格 + 1 个中心块切出 5 个落在数据
    范围内的分区（保证裁剪片有效）；无 lucc_path 或读取失败时回退 mock 分区.
    分区类型（kind）在初始阶段为占位 "others"，由 HIE 裁剪后按主导地类回填.
    """

    def __init__(
        self,
        strategy: str = "lucc_kind",
        cache_dir: str | Path | None = None,
        legend_db: LegendDB | None = None,
        legend_set: str = "lucc",
    ) -> None:
        """
        Args:
            strategy: "lucc_kind" | "admin" | "fishnet"，骨架阶段仅识别字符串.
            cache_dir: 裁剪产物落盘目录.
            legend_db: 图例库实例（地类码→Kind 映射）；缺省时按 legend_set 新建.
            legend_set: 当 legend_db 未提供时，用哪套图例初始化 LegendDB.
        """
        self.strategy = strategy
        self.cache_dir = Path(cache_dir) if cache_dir else None
        self.legend_db = legend_db or LegendDB(legend_set=legend_set)

    def assign_kind(self, class_code: int | None) -> PartitionKind:
        """把 LUCC 主导地类码映射为 PartitionKind，未知/None 归 "others"."""
        return self.legend_db.assign_kind(class_code)

    def detect(self, region_data: dict) -> list[Partition]:
        """扫描研究区数据，返回分区列表.

        Args:
            region_data: 至少包含 {"name", "bbox"?, "lucc_path", ...}.

        Returns:
            Partition 列表（5 个，A1..A5，kind 初始为占位待回填）.
        """
        bbox = self._resolve_bbox(region_data)
        return self._grid_partitions(bbox)

    def _resolve_bbox(self, region_data: dict) -> tuple[float, float, float, float]:
        """确定研究区四至：优先读 lucc 真实范围，其次 region_data.bbox，最后默认."""
        lucc_path = region_data.get("lucc_path")
        if lucc_path and Path(lucc_path).exists():
            info = vision.read_raster_info(lucc_path)
            return info.bbox
        return region_data.get("bbox", (113.0, 22.0, 114.5, 23.5))

    def _grid_partitions(
        self, bbox: tuple[float, float, float, float]
    ) -> list[Partition]:
        """把研究区四至切成 2x2 象限 + 1 个中心块，共 5 个分区."""
        xmin, ymin, xmax, ymax = bbox
        mid_x = (xmin + xmax) / 2
        mid_y = (ymin + ymax) / 2
        dx = xmax - xmin
        dy = ymax - ymin

        # 粗略面积估算（deg -> km，1deg≈111km），仅供展示
        def area(bx: tuple[float, float, float, float]) -> float:
            w = (bx[2] - bx[0]) * 111.0
            h = (bx[3] - bx[1]) * 111.0
            return round(abs(w * h), 1)

        boxes = [
            (xmin, mid_y, mid_x, ymax),                                     # A1 左上
            (mid_x, mid_y, xmax, ymax),                                     # A2 右上
            (xmin, ymin, mid_x, mid_y),                                     # A3 左下
            (mid_x, ymin, xmax, mid_y),                                     # A4 右下
            (xmin + dx * 0.35, ymin + dy * 0.35,
             xmin + dx * 0.65, ymin + dy * 0.65),                           # A5 中心
        ]
        # kind 先置 "others"，待 HIE 裁剪后按主导地类回填
        return [
            Partition(f"A{i + 1}", "others", b, area(b))
            for i, b in enumerate(boxes)
        ]

