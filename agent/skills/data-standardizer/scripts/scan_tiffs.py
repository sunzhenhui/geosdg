#!/usr/bin/env python3
"""
scan_tiffs.py — Scan a directory for GeoTIFF files and extract metadata.

Usage:
    python scan_tiffs.py <directory> [--output json|csv] [--recursive]

Output: list of dicts with keys:
    path, filename, size_mb, width, height, bands, dtype,
    crs, nodata, resolution_x, resolution_y, category_hint
"""

import argparse
import json
import os
import sys
from pathlib import Path

try:
    from osgeo import gdal, osr
except ImportError:
    print("ERROR: GDAL Python bindings not found. Install with: pip install GDAL", file=sys.stderr)
    sys.exit(1)


def classify_filename(filename: str, category_map: dict) -> str:
    """Classify a filename into a category using keyword matching."""
    fname_lower = filename.lower().replace("-", "_").replace(" ", "_")
    best_category = "unknown"
    best_score = 0

    for cat_id, cat_info in category_map.items():
        score = 0
        for kw in cat_info.get("keywords_en", []):
            if kw in fname_lower:
                score += len(kw)
        for kw in cat_info.get("keywords_cn", []):
            if kw in filename:
                score += len(kw) * 2  # Chinese keywords weighted higher
        if score > best_score:
            best_score = score
            best_category = cat_id

    return best_category


def scan_single_tiff(filepath: str) -> dict:
    """Extract metadata from a single GeoTIFF file."""
    ds = gdal.OpenEx(filepath, gdal.GA_ReadOnly)
    if ds is None:
        return {"path": filepath, "error": "cannot_open"}

    try:
        width = ds.RasterXSize
        height = ds.RasterYSize
        bands = ds.RasterCount
        band = ds.GetRasterBand(1)
        dtype = gdal.GetDataTypeName(band.DataType)
        nodata = band.GetNoDataValue()

        gt = ds.GetGeoTransform()
        res_x = abs(gt[1])
        res_y = abs(gt[5])

        proj = ds.GetProjection()
        crs = "unknown"
        if proj:
            srs = osr.SpatialReference()
            srs.ImportFromWkt(proj)
            srs.AutoIdentifyEPSG()
            crs_code = srs.GetAuthorityCode(None)
            if crs_code:
                crs = f"EPSG:{crs_code}"

        return {
            "path": filepath,
            "filename": os.path.basename(filepath),
            "size_mb": round(os.path.getsize(filepath) / (1024 * 1024), 2),
            "width": width,
            "height": height,
            "bands": bands,
            "dtype": dtype,
            "crs": crs,
            "nodata": nodata,
            "resolution_x": round(res_x, 6),
            "resolution_y": round(res_y, 6),
        }
    finally:
        ds = None  # GDALClose


def scan_directory(directory: str, recursive: bool = False) -> list:
    """Scan directory for .tif/.tiff files and extract metadata."""
    pattern = "**/*.tif*" if recursive else "*.tif*"
    tiff_files = sorted(Path(directory).glob(pattern))

    # Load category keywords
    script_dir = Path(__file__).parent.parent
    cat_file = script_dir / "references" / "category_keywords.json"
    category_map = {}
    if cat_file.exists():
        with open(cat_file, "r", encoding="utf-8") as f:
            category_map = json.load(f)

    results = []
    for tf in tiff_files:
        info = scan_single_tiff(str(tf))
        if "error" not in info:
            info["category_hint"] = classify_filename(info["filename"], category_map)
        results.append(info)

    return results


def main():
    parser = argparse.ArgumentParser(description="Scan GeoTIFF files and extract metadata")
    parser.add_argument("directory", help="Directory to scan")
    parser.add_argument("--output", choices=["json", "csv"], default="json", help="Output format")
    parser.add_argument("--recursive", action="store_true", help="Scan subdirectories")
    args = parser.parse_args()

    if not os.path.isdir(args.directory):
        print(f"ERROR: {args.directory} is not a directory", file=sys.stderr)
        sys.exit(1)

    results = scan_directory(args.directory, args.recursive)

    if args.output == "json":
        print(json.dumps(results, indent=2, ensure_ascii=False))
    else:
        if not results:
            return
        keys = list(results[0].keys())
        print(",".join(keys))
        for r in results:
            print(",".join(str(r.get(k, "")) for k in keys))


if __name__ == "__main__":
    main()
