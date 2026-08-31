"""栅格 IO（对标 PEACE/utils/vision.py，但 PEACE 处理普通图像，我们处理 GeoTIFF）.

已接入 GDAL（osgeo.gdal 3.x）做真实裁剪 / 读元信息；GDAL 不可用或出错时回退占位实现，
保证骨架 pipeline 始终可跑通.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from . import common

try:
    from osgeo import gdal as _gdal

    _gdal.UseExceptions()
    _HAS_GDAL = True
except Exception:  # pragma: no cover - GDAL 缺失时降级
    _gdal = None  # type: ignore[assignment]
    _HAS_GDAL = False


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
    """读取栅格元信息.

    优先用 GDAL 真实读取；文件不存在或 GDAL 不可用时返回占位信息.
    """
    p = Path(path)
    if _HAS_GDAL and p.exists():
        try:
            ds = _gdal.Open(str(p))
            if ds is not None:
                gt = ds.GetGeoTransform()
                w, h = ds.RasterXSize, ds.RasterYSize
                xmin = gt[0]
                ymax = gt[3]
                xmax = xmin + gt[1] * w
                ymin = ymax + gt[5] * h
                band = ds.GetRasterBand(1)
                nodata = band.GetNoDataValue()
                crs = ds.GetProjection() or "EPSG:4326"
                ds = None
                return RasterInfo(
                    path=str(p), width=w, height=h,
                    bbox=(xmin, ymin, xmax, ymax),
                    crs=crs, nodata=nodata,
                )
        except Exception as exc:  # pragma: no cover
            if common.echo:
                print(f"[vision] read_raster_info fallback: {exc}")

    return RasterInfo(
        path=str(p), width=1024, height=768,
        bbox=(113.0, 22.0, 114.5, 23.5),
        crs="EPSG:4326", nodata=-9999.0,
    )


def crop_by_bbox(
    src_path: str | Path,
    bbox: tuple[float, float, float, float],
    dst_path: str | Path,
) -> str:
    """按 bbox（经纬度）裁剪栅格并保存.

    优先用 gdal.Translate（projWin 裁窗）真实裁剪；GDAL 不可用、源文件缺失或
    裁剪失败时写占位文本文件并返回其路径（保证 pipeline 不中断）.

    Args:
        src_path: 源栅格路径.
        bbox: (xmin, ymin, xmax, ymax) 经纬度.
        dst_path: 输出裁剪片路径（.tif）.

    Returns:
        实际写出的文件路径.
    """
    dst = Path(dst_path)
    dst.parent.mkdir(parents=True, exist_ok=True)
    src = Path(src_path)
    xmin, ymin, xmax, ymax = bbox

    if _HAS_GDAL and src.exists():
        try:
            # gdal projWin 需要 (ulx, uly, lrx, lry) = (xmin, ymax, xmax, ymin)
            out = _gdal.Translate(
                str(dst), str(src),
                projWin=[xmin, ymax, xmax, ymin],
                projWinSRS="EPSG:4326",
            )
            if out is not None:
                out = None  # flush & close
                return str(dst)
            if common.echo:
                print(f"[vision] crop returned None, fallback mock: {src} @ {bbox}")
        except Exception as exc:
            if common.echo:
                print(f"[vision] crop error, fallback mock: {exc}")

    # 回退：写占位文本
    placeholder = dst.with_suffix(dst.suffix + ".mock")
    placeholder.write_text(
        f"# mock crop of {src_path} @ bbox={bbox}\n", encoding="utf-8"
    )
    return str(placeholder)


def class_histogram(
    path: str | Path,
    ignore_values: tuple[float, ...] = (),
) -> dict[int, int]:
    """统计分类栅格各整数类别的像素计数（忽略 NoData 与 ignore_values）.

    优先用 GDAL 读取；GDAL 不可用、文件缺失或读取失败时返回空 dict.

    Args:
        path: 分类栅格（如 LUCC）路径.
        ignore_values: 额外要忽略的类别值（如背景 0）.

    Returns:
        {class_value: pixel_count}，已剔除 NoData 与 ignore_values.
    """
    p = Path(path)
    if not (_HAS_GDAL and p.exists()):
        return {}
    try:
        ds = _gdal.Open(str(p))
        if ds is None:
            return {}
        band = ds.GetRasterBand(1)
        nodata = band.GetNoDataValue()
        arr = band.ReadAsArray()
        ds = None
        if arr is None:
            return {}
        import numpy as _np

        flat = arr.ravel()
        skip = set(ignore_values)
        if nodata is not None:
            skip.add(nodata)
        vals, counts = _np.unique(flat, return_counts=True)
        hist: dict[int, int] = {}
        for v, c in zip(vals.tolist(), counts.tolist()):
            if v in skip:
                continue
            hist[int(v)] = int(c)
        return hist
    except Exception as exc:  # pragma: no cover
        if common.echo:
            print(f"[vision] class_histogram error: {exc}")
        return {}


def dominant_class(
    path: str | Path,
    ignore_values: tuple[float, ...] = (0,),
) -> int | None:
    """返回分类栅格的主导类别（像素最多的类别值），无有效像素时返回 None.

    默认忽略背景值 0 与 NoData.
    """
    hist = class_histogram(path, ignore_values=ignore_values)
    if not hist:
        return None
    return max(hist, key=lambda k: hist[k])


def raster_to_prompt_asset(path: str | Path) -> dict[str, Any]:
    """把栅格转成可塞进 messages 的资源描述.

    真实实现可返回 base64 缩略图或对象存储 URL；这里返回结构化元信息.
    """
    info = read_raster_info(path)
    return {
        "type": "raster_ref",
        "path": info.path,
        "bbox": info.bbox,
        "crs": info.crs,
        "size": [info.width, info.height],
    }
