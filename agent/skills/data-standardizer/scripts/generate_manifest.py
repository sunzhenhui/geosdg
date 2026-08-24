#!/usr/bin/env python3
"""
generate_manifest.py — Generate a manifest.json from scan results.

Usage:
    python generate_manifest.py <scan_results.json> --project-name <name> --region <name> --bbox <w,s,e,n> --baseline <year> --horizon <year>

Reads scan results (from scan_tiffs.py output), groups files by category,
and generates a manifest.json following the manifest-schema.json spec.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict


def extract_year(filename: str) -> int | None:
    """Extract a 4-digit year from filename."""
    match = re.search(r"(?:^|[_\-\s])(\d{4})(?:[_\-\s.]|$)", filename)
    if match:
        year = int(match.group(1))
        if 1900 <= year <= 2100:
            return year
    return None


def extract_scenario(filename: str) -> str | None:
    """Extract scenario identifier from filename."""
    fname_lower = filename.lower().replace("-", "_").replace(" ", "_")
    patterns = [
        (r"ssp1[_\-]?26", "ssp1_26"),
        (r"ssp2[_\-]?45", "ssp2_45"),
        (r"ssp3[_\-]?70", "ssp3_70"),
        (r"ssp5[_\-]?85", "ssp5_85"),
        (r"ssp1", "ssp1_26"),
        (r"ssp2", "ssp2_45"),
        (r"ssp3", "ssp3_70"),
        (r"ssp5", "ssp5_85"),
        (r"baseline", "baseline"),
    ]
    for pattern, scenario_id in patterns:
        if re.search(pattern, fname_lower):
            return scenario_id
    return None


def group_by_category(scan_results: list) -> dict:
    """Group scan results by category_hint."""
    groups = defaultdict(list)
    for item in scan_results:
        cat = item.get("category_hint", "unknown")
        groups[cat].append(item)
    return groups


def make_relative_path(raw_path: str, data_dir: str) -> str:
    """Make path relative to data_dir if possible."""
    p = Path(raw_path)
    # Try absolute path first
    try:
        return str(p.relative_to(data_dir))
    except ValueError:
        pass
    # Try resolving relative path against CWD
    try:
        resolved = p.resolve()
        return str(resolved.relative_to(Path(data_dir).resolve()))
    except ValueError:
        pass
    # Try stripping data_dir prefix from string
    data_dir_name = Path(data_dir).name
    prefix = data_dir_name + "/"
    if raw_path.startswith(prefix):
        return raw_path[len(prefix):]
    return raw_path


def build_dataset_entry(category: str, files: list, category_map: dict, data_dir: str = "") -> dict:
    """Build a dataset entry for manifest.json."""
    cat_info = category_map.get(category, {})
    display = cat_info.get("display", category)

    file_entries = []
    for f in files:
        if "error" in f:
            continue
        raw_path = f["path"]
        rel_path = make_relative_path(raw_path, data_dir) if data_dir else raw_path
        entry = {"path": rel_path}
        filename = f.get("filename", os.path.basename(f["path"]))
        year = extract_year(filename)
        if year is not None:
            entry["year"] = year
        else:
            entry["year"] = 2020  # default when year not extractable
        scenario = extract_scenario(filename)
        if scenario is not None:
            entry["scenario"] = scenario
        tags = []
        if year and scenario == "baseline":
            tags.append("baseline")
        if category in ("elevation", "slope"):
            tags.append("static")
        if tags:
            entry["tags"] = tags
        file_entries.append(entry)

    # Determine resolution from first valid file
    valid_files = [f for f in files if "error" not in f]
    res_x = valid_files[0].get("resolution_x", 0) if valid_files else 0
    res_y = valid_files[0].get("resolution_y", 0) if valid_files else 0
    res_val = round((res_x + res_y) / 2, 2) if res_x and res_y else 0

    schema_category = cat_info.get("schema_category", "custom")

    dataset = {
        "id": category,
        "name": display,
        "category": schema_category,
        "description": "",
        "resolution": {"value": 0, "unit": "m"},
        "files": file_entries,
    }

    if res_val > 0:
        if res_val >= 1:
            dataset["resolution"] = {"value": int(res_val), "unit": "m"}
        else:
            dataset["resolution"] = {"value": round(res_val, 6), "unit": "degree"}

    return dataset


def generate_manifest(
    scan_results: list,
    project_name: str,
    region_name: str,
    bbox: list,
    baseline: int,
    horizon: int,
    step: int,
    data_dir: str = "",
) -> dict:
    """Generate manifest.json from scan results."""
    script_dir = Path(__file__).parent.parent
    cat_file = script_dir / "references" / "category_keywords.json"
    category_map = {}
    if cat_file.exists():
        with open(cat_file, "r", encoding="utf-8") as f:
            category_map = json.load(f)

    groups = group_by_category(scan_results)

    datasets = []
    for category in sorted(groups.keys()):
        entry = build_dataset_entry(category, groups[category], category_map, data_dir)
        if entry["files"]:  # skip empty datasets
            datasets.append(entry)

    manifest = {
        "$schema": "../manifest-schema.json",
        "project": {
            "name": project_name,
            "description": "",
            "region": {
                "name": region_name,
                "bbox": bbox,
                "crs": "EPSG:4326",
            },
            "time_range": {
                "baseline": baseline,
                "horizon": horizon,
                "step": step,
            },
        },
        "scenarios": [],
        "datasets": datasets,
        "indicators": [],
        "priority_areas": {
            "rules": [],
            "output": {
                "n_classes": 5,
                "labels": ["Very Low", "Low", "Medium", "High", "Very High"],
                "classification": "quantile",
            },
        },
    }

    return manifest


def main():
    parser = argparse.ArgumentParser(description="Generate manifest.json from scan results")
    parser.add_argument("scan_results", help="Path to scan results JSON file")
    parser.add_argument("--project-name", default="", help="Project name")
    parser.add_argument("--region", default="", help="Region name")
    parser.add_argument("--bbox", default="0,0,0,0", help="Bounding box: w,s,e,n")
    parser.add_argument("--baseline", type=int, default=2020, help="Baseline year")
    parser.add_argument("--horizon", type=int, default=2050, help="Horizon year")
    parser.add_argument("--step", type=int, default=5, help="Time step")
    parser.add_argument("--data-dir", default="", help="Data directory for relative paths")
    parser.add_argument("--output", default="manifest.json", help="Output file path")
    args = parser.parse_args()

    with open(args.scan_results, "r", encoding="utf-8") as f:
        scan_results = json.load(f)

    bbox = [float(x) for x in args.bbox.split(",")]

    manifest = generate_manifest(
        scan_results, args.project_name, args.region, bbox,
        args.baseline, args.horizon, args.step, args.data_dir,
    )

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)

    print(f"Generated {args.output} with {len(manifest['datasets'])} datasets")


if __name__ == "__main__":
    main()
