# data-standardizer

GeoSDG data standardization Skill — organizes scattered GeoTIFF files into the standard `data/` directory structure and generates `manifest.json`.

**Trigger keywords**: data standardization, organize data, organize tiffs, generate manifest, data directory structure, scan tiffs

**When to use**: when the user has a batch of GeoTIFF files that need to be organized into the standard structure consumable by GeoSDG.

---

## 6-Stage Flow Quick Reference

| Stage | Name | Core action |
|-------|------|-------------|
| 1 | Scan & Inventory | `scan_tiffs.py` scans the directory and groups files by category |
| 2 | Classify & Rename | Generate the standard naming scheme `<cat>_<src>_<year>[_<scenario>].tif` |
| 3 | Metadata Check | Check CRS / NoData / resolution consistency |
| 4 | Generate Manifest | `generate_manifest.py` produces `manifest.json` |
| 5 | Validate Structure | `validate_structure.py` verifies directory and manifest consistency |
| 6 | Finalize | Confirm completion and output CLI usage guidance |

## Script Quick Reference

| Script | Purpose |
|--------|---------|
| `scripts/scan_tiffs.py <dir> --recursive` | Scan GeoTIFF metadata |
| `scripts/generate_manifest.py <scan.json>` | Generate manifest.json |
| `scripts/validate_structure.py <dir> --strict` | Validate structural consistency |
| `scripts/progress_manager.py <cmd> <dir>` | Cross-session progress management |

## Reference Index

- `references/category_keywords_en.json` — 16-category keyword mapping
- `references/ssp_scenarios_en.json` — SSP scenario mapping
- `references/manifest_template.json` — manifest template
- `@see ../../data/manifest-schema.json` — JSON Schema
- `@see ../../data/DATA_FORMAT_SPEC.md` — data format specification

→ Full execution flow: [SKILL-detail_en.md](./SKILL-detail_en.md)
