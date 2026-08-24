#!/usr/bin/env python3
"""
validate_structure.py — Validate a data directory against manifest-schema.json.

Usage:
    python validate_structure.py <data_dir> [--manifest manifest.json] [--strict]

Checks:
    1. manifest.json exists and is valid JSON
    2. manifest.json conforms to manifest-schema.json
    3. All files referenced in manifest exist on disk
    4. All .tif files in data_dir are referenced in manifest (strict mode)
    5. CRS consistency across datasets

Supports two manifest formats:
    - List format: "datasets": [{"id": "...", "files": [{"path": "..."}]}]
    - Object format: "datasets": {"key": {"files": [{"file": "..."}]}}
"""

import argparse
import json
import os
import sys
from pathlib import Path

try:
    import jsonschema
    HAS_JSONSCHEMA = True
except ImportError:
    HAS_JSONSCHEMA = False


def load_json(filepath: str) -> dict | list:
    """Load and parse a JSON file."""
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)


def normalize_datasets(manifest: dict) -> list:
    """Normalize datasets to a list of dicts, handling both list and object formats.

    Returns list of: {"id": str, "files": [{"path": str, ...}], "subcategories": [...]}
    """
    raw = manifest.get("datasets", [])
    result = []

    if isinstance(raw, list):
        for ds in raw:
            if not isinstance(ds, dict):
                continue
            ds_id = ds.get("id", "unknown")
            files = [_normalize_file_entry(f, ds_id) for f in ds.get("files", [])]
            subs = []
            for sub in ds.get("subcategories", []):
                sub_id = sub.get("id", "unknown")
                sub_files = [_normalize_file_entry(f, sub_id) for f in sub.get("files", [])]
                subs.append({"id": sub_id, "files": sub_files})
            result.append({"id": ds_id, "files": files, "subcategories": subs})
    elif isinstance(raw, dict):
        for ds_id, ds in raw.items():
            if not isinstance(ds, dict):
                continue
            files = [_normalize_file_entry(f, ds_id) for f in ds.get("files", [])]
            result.append({"id": ds_id, "files": files, "subcategories": []})

    return result


def _normalize_file_entry(f: dict, parent_id: str) -> dict:
    """Normalize a file entry to use 'path' key."""
    if not isinstance(f, dict):
        return {"path": str(f), "parent": parent_id}
    path = f.get("path") or f.get("file") or f.get("name", "")
    normalized = dict(f)
    normalized["path"] = path
    normalized["parent"] = parent_id
    return normalized


def validate_schema(manifest: dict, schema: dict) -> list:
    """Validate manifest against JSON Schema. Returns list of errors."""
    errors = []
    if not HAS_JSONSCHEMA:
        errors.append("WARNING: jsonschema not installed, skipping schema validation")
        return errors
    try:
        jsonschema.validate(manifest, schema)
    except jsonschema.ValidationError as e:
        errors.append(f"Schema validation error: {e.message} at path {'/'.join(str(p) for p in e.path)}")
    return errors


def validate_file_references(data_dir: str, datasets: list) -> list:
    """Check that all files in manifest exist on disk."""
    errors = []
    for ds in datasets:
        for f in ds.get("files", []):
            fpath = f["path"]
            full_path = os.path.join(data_dir, fpath)
            # Also try rasters/ subdirectory
            if not os.path.isfile(full_path):
                alt_path = os.path.join(data_dir, "rasters", fpath)
                if not os.path.isfile(alt_path):
                    errors.append(f"MISSING: {fpath} (referenced in dataset '{ds['id']}')")
        for sub in ds.get("subcategories", []):
            for f in sub.get("files", []):
                fpath = f["path"]
                full_path = os.path.join(data_dir, fpath)
                if not os.path.isfile(full_path):
                    alt_path = os.path.join(data_dir, "rasters", fpath)
                    if not os.path.isfile(alt_path):
                        errors.append(f"MISSING: {fpath} (referenced in subcategory '{sub['id']}')")
    return errors


def validate_orphan_files(data_dir: str, datasets: list) -> list:
    """Check for .tif files not referenced in manifest."""
    errors = []
    referenced = set()
    for ds in datasets:
        for f in ds.get("files", []):
            referenced.add(os.path.normpath(f["path"]))
        for sub in ds.get("subcategories", []):
            for f in sub.get("files", []):
                referenced.add(os.path.normpath(f["path"]))

    for tf in Path(data_dir).rglob("*.tif*"):
        rel = os.path.normpath(str(tf.relative_to(data_dir)))
        if rel not in referenced:
            errors.append(f"ORPHAN: {rel} (exists on disk but not in manifest)")
    return errors


def validate_crs_consistency(datasets: list) -> list:
    """Check CRS consistency across datasets."""
    errors = []
    crs_map = {}
    for ds in datasets:
        for f in ds.get("files", []):
            crs = f.get("crs")
            if crs:
                crs_map.setdefault(ds["id"], set()).add(crs)
    for ds_id, crs_set in crs_map.items():
        if len(crs_set) > 1:
            errors.append(f"CRS_MISMATCH: dataset '{ds_id}' has multiple CRS: {crs_set}")
    return errors


def validate_structure(data_dir: str, manifest_path: str | None, strict: bool) -> list:
    """Run all validation checks. Returns list of issues."""
    issues = []

    # 1. Find manifest
    if manifest_path is None:
        manifest_path = os.path.join(data_dir, "manifest.json")
    if not os.path.isfile(manifest_path):
        issues.append(f"CRITICAL: manifest.json not found at {manifest_path}")
        return issues

    # 2. Parse manifest
    try:
        manifest = load_json(manifest_path)
    except json.JSONDecodeError as e:
        issues.append(f"CRITICAL: manifest.json is not valid JSON: {e}")
        return issues

    # 3. Schema validation (skip if manifest uses different format)
    # Script is at agent/skills/data-standardizer/scripts/validate_structure.py
    # Project root is 5 levels up
    script_dir = Path(__file__).resolve().parent.parent.parent.parent.parent
    schema_path = os.path.join(str(script_dir), "data", "manifest-schema.json")
    if os.path.isfile(schema_path):
        try:
            schema = load_json(schema_path)
            issues.extend(validate_schema(manifest, schema))
        except Exception as e:
            issues.append(f"WARNING: Could not load schema: {e}")
    else:
        issues.append("WARNING: manifest-schema.json not found, skipping schema validation")

    # Normalize datasets to common format
    datasets = normalize_datasets(manifest)

    # 4. File references
    issues.extend(validate_file_references(data_dir, datasets))

    # 5. Orphan files
    if strict:
        issues.extend(validate_orphan_files(data_dir, datasets))

    # 6. CRS consistency
    issues.extend(validate_crs_consistency(datasets))

    return issues


def main():
    parser = argparse.ArgumentParser(description="Validate data directory structure")
    parser.add_argument("data_dir", help="Data directory to validate")
    parser.add_argument("--manifest", default=None, help="Path to manifest.json")
    parser.add_argument("--strict", action="store_true", help="Check for orphan files")
    args = parser.parse_args()

    issues = validate_structure(args.data_dir, args.manifest, args.strict)

    if not issues:
        print("✅ All validation checks passed")
    else:
        critical = [i for i in issues if i.startswith("CRITICAL")]
        warnings = [i for i in issues if i.startswith("WARNING")]
        others = [i for i in issues if not i.startswith("CRITICAL") and not i.startswith("WARNING")]

        for i in critical:
            print(f"❌ {i}")
        for i in others:
            print(f"⚠️  {i}")
        for i in warnings:
            print(f"💡 {i}")

        if critical:
            sys.exit(1)


if __name__ == "__main__":
    main()
