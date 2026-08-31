"""分区检测器（对标 PEACE/tool_pool/map_component_detector）.

PEACE 把地质图切成 8 类视觉组件（title/main_map/legend/...），
我们把研究区栅格切成 N 个"治理分区"（按 LUCC 类型或行政区）.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Literal

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

    骨架实现：直接吐 5 个 mock 分区，方便下游 pipeline 跑通.
    """

    def __init__(
        self,
        strategy: str = "lucc_kind",
        cache_dir: str | Path | None = None,
    ) -> None:
        """
        Args:
            strategy: "lucc_kind" | "admin" | "fishnet"，骨架阶段仅识别字符串.
            cache_dir: 裁剪产物落盘目录.
        """
        self.strategy = strategy
        self.cache_dir = Path(cache_dir) if cache_dir else None

    def detect(self, region_data: dict) -> list[Partition]:
        """扫描研究区数据，返回分区列表.

        Args:
            region_data: 至少包含 {"name", "bbox", "lucc_path", ...}.

        Returns:
            Partition 列表.
        """
        # TODO: 用 GDAL 读 lucc_path，按 strategy 聚类/掩膜，
        #       再调 vision.crop_by_bbox 落盘裁剪片.
        bbox = region_data.get("bbox", (113.0, 22.0, 114.5, 23.5))
        xmin, ymin, xmax, ymax = bbox
        mid_x = (xmin + xmax) / 2
        mid_y = (ymin + ymax) / 2

        mock: list[Partition] = [
            Partition("A1", "urban",    (xmin, mid_y, mid_x, ymax), 42.0),
            Partition("A2", "cropland", (mid_x, mid_y, xmax, ymax), 65.0),
            Partition("A3", "forest",   (xmin, ymin, mid_x, mid_y), 88.0),
            Partition("A4", "rural",    (mid_x, ymin, xmax, mid_y), 33.0),
            Partition("A5", "water",    (xmin + 0.1, ymin + 0.1, xmin + 0.3, ymin + 0.3), 5.0),
        ]
        return mock
