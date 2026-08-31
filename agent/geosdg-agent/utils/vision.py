"""栅格 IO 桩（对标 PEACE/utils/vision.py，但 PEACE 处理的是普通图像，我们处理 GeoTIFF）.

骨架阶段所有函数返回假数据；后续接 GDAL / rasterio.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class RasterInfo:
    """栅格基本信息."""
    path: str
    width: int
    height: int
    bbox: tuple[float, float, float, float]  # (xmin, ymin, xmax, ymax) in lon/lat
    crs: str
    nodata: float | None


def read_raster_info(path: str | Path) -> RasterInfo:
    """读取栅格元信息（骨架版返回假数据）.

    TODO: 使用 GDAL/rasterio 真实读取.
    """
    return RasterInfo(
        path=str(path),
        width=1024,
        height=768,
        bbox=(113.0, 22.0, 114.5, 23.5),
        crs="EPSG:4326",
        nodata=-9999.0,
    )


def crop_by_bbox(
    src_path: str | Path,
    bbox: tuple[float, float, float, float],
    dst_path: str | Path,
) -> str:
    """按 bbox 裁剪栅格并保存（骨架版仅记录路径）.

    TODO: 使用 gdal.Warp 或 rasterio.mask 实现.
    """
    dst = Path(dst_path)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(f"# mock crop of {src_path} @ bbox={bbox}\n", encoding="utf-8")
    return str(dst)


def raster_to_prompt_asset(path: str | Path) -> dict[str, Any]:
    """把栅格转成可塞进 messages 的资源描述（骨架版返回文本占位）.

    真实实现可返回 base64 缩略图或对象存储 URL.
    """
    info = read_raster_info(path)
    return {
        "type": "raster_ref",
        "path": info.path,
        "bbox": info.bbox,
        "crs": info.crs,
        "size": [info.width, info.height],
    }
