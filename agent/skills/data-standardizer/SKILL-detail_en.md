# data-standardizer — Full Execution Flow

> GeoSDG data standardization Skill. Guides the user to organize scattered GeoTIFF files into the standard `data/` directory structure and generate `manifest.json`, making the data directly consumable by the GeoSDG CLI.

---

## 🔄 Interruption Recovery Mechanism

At the start of each session, check `<data_dir>/.standardize-progress.json`:

- **Exists** → read `current_stage`, show the user a progress summary, and ask "Continue from Stage N?"
- **Does not exist** → start from Stage 1

The progress file is managed by `scripts/progress_manager.py`, and the 6-stage status is persisted.

---

## 📋 6-Stage Full Conversation Flow

### Stage 1: Scan & Inventory

**Goal**: scan the user's data directory and build a complete file inventory.

**Steps**:

1. Ask the user for the data directory path (default `data/`)
2. Run `scripts/scan_tiffs.py <data_dir> --recursive --output json`
3. Parse the output and group statistics by `category_hint`
4. Show the user a scan summary:

```
📊 Scan results:
- Total files: 42 GeoTIFF
- Total size: 1.2 GB
- Category distribution:
  · lucc: 8 files (2020-2050, 5yr step)
  · pop: 7 files (2020-2050, 5yr step)
  · carbon: 6 files (2020-2050, 5yr step)
  · elevation: 1 file (static)
  · slope: 1 file (static)
  · unknown: 19 files ← require manual confirmation
```

5. For files in the `unknown` category, ask the user one by one to confirm the category
6. Update progress: `progress_manager.py update <data_dir> --stage 1 --status done`

**Interruption recovery**: simply rescan; no side effects.

---

### Stage 2: Classify & Rename

**Goal**: rename files to the standard naming scheme `<category>_<source>_<year>[_<scenario>].tif`.

**Standard naming scheme**:

| Component | Description | Example |
|-----------|-------------|---------|
| category | Data category abbreviation | `lucc`, `pop`, `carbon` |
| source | Data source / project name | `globe30`, `worldpop`, `edgar` |
| year | 4-digit year | `2020`, `2050` |
| scenario | SSP scenario (optional) | `ssp1_26`, `ssp2_45` |

**Examples**:
- `LUCC_China_2020.tif` → `lucc_globe30_2020.tif`
- `pop_ssp2_2050.tif` → `pop_worldpop_2050_ssp2_45.tif`

**Steps**:

1. Based on the Stage 1 scan results, generate rename suggestions for each file
2. Show the rename plan to the user (table: original name → new name)
3. The user can modify item by item or batch-confirm
4. **Do not auto-rename**; only generate the plan. After the user confirms, generate a shell script `rename.sh` for the user to run
5. Update progress

**Interruption recovery**: the rename plan can be regenerated; an already-run `rename.sh` is irreversible (remind the user to back up).

---

### Stage 3: Metadata Check

**Goal**: check CRS, NoData, and resolution consistency, and flag problem files.

**Checks**:

| Check item | Pass criteria | Problem handling |
|------------|---------------|------------------|
| CRS consistency | All files within the same dataset share the same CRS | Flag inconsistent files; suggest unifying to the project CRS |
| NoData consistency | Same NoData value within the same dataset | Flag inconsistent files; suggest unifying |
| Resolution reasonableness | Resolution > 0 and within a reasonable range | Flag outliers |
| Band count | Single band (bands=1) | Flag multi-band files for confirmation |

**Steps**:

1. Read metadata from the Stage 1 scan results
2. Group by category and check item by item
3. Generate a check report:

```
🔍 Metadata check report:

✅ lucc: CRS=EPSG:4326, NoData=-9999, Resolution=0.000278° — OK
⚠️  pop: CRS inconsistent — 5 files EPSG:4326, 2 files EPSG:32650
❌ carbon: NoData inconsistent — 4 files -9999, 2 files -3.4e38
```

4. For problem items, suggest a fix (e.g., `gdalwarp` reprojection)
5. Update progress

**Interruption recovery**: checking is read-only and repeatable.

---

### Stage 4: Generate Manifest

**Goal**: generate `manifest.json` describing the complete structure of the data directory.

**Steps**:

1. Collect information from the previous 3 stages: file inventory, classification results, metadata
2. Ask for project metadata (if not yet provided):
   - Project name
   - Region name and bbox
   - Baseline year / target year / time step
3. Run `scripts/generate_manifest.py <scan_results.json> --project-name <name> --region <name> --bbox <w,s,e,n> --baseline <year> --horizon <year>`
4. Show a summary of the generated `manifest.json`; the user confirms
5. Write to `<data_dir>/manifest.json`
6. Update progress

**Key structure of manifest.json**:

```json
{
  "$schema": "../manifest-schema.json",
  "project": { "name": "...", "region": {...}, "time_range": {...} },
  "scenarios": [...],
  "datasets": [
    { "id": "lucc", "name": "...", "category": "landuse", "files": [...] }
  ],
  "indicators": [],
  "priority_areas": { "rules": [], "output": {...} }
}
```

**Interruption recovery**: manifest.json can be regenerated and overwritten.

---

### Stage 5: Validate Structure

**Goal**: verify that the `data/` directory structure is fully consistent with `manifest.json`.

**Steps**:

1. Run `scripts/validate_structure.py <data_dir> --strict`
2. Checks:
   - manifest.json exists and is valid JSON
   - manifest.json conforms to manifest-schema.json
   - All files referenced by the manifest exist
   - All .tif files are in the manifest (strict mode)
   - CRS consistency
3. Show the validation results; if there are problems, return to the corresponding stage to fix them
4. Update progress

**Interruption recovery**: validation is read-only and repeatable.

---

### Stage 6: Finalize

**Goal**: confirm the final structure, clean up temporary files, and output usage guidance.

**Steps**:

1. Confirm the final version of `manifest.json`
2. Delete `.standardize-progress.json` (optional, user confirms)
3. Output usage guidance:

```
✅ Data standardization complete!

📂 Directory structure:
data/
├── manifest.json
├── rasters/
│   ├── lucc_globe30_2020.tif
│   ├── lucc_globe30_2050_ssp2_45.tif
│   ├── pop_worldpop_2020.tif
│   └── ...
└── _template/

🚀 Next steps:
  1. Compute an indicator with the GeoSDG CLI:
     geosdg-cli --command calc-sdg1131 --lucc data/rasters/lucc_globe30_2020.tif ...
  2. Or run it through the Agent:
     "Calculate SDG 11.3.1 for me"
```

4. Update progress to fully complete

---

## 🔗 Script Index

| Script | Purpose | Invocation |
|--------|---------|------------|
| `scripts/scan_tiffs.py` | Scan GeoTIFF files and extract metadata | `python scan_tiffs.py <dir> --recursive` |
| `scripts/generate_manifest.py` | Generate manifest.json from scan results | `python generate_manifest.py <scan.json> --project-name <name>` |
| `scripts/validate_structure.py` | Validate directory-structure consistency | `python validate_structure.py <dir> --strict` |
| `scripts/progress_manager.py` | Cross-session progress management | `python progress_manager.py <init\|status\|update\|resume> <dir>` |

## 🔗 Reference File Index

| File | Purpose |
|------|---------|
| `references/category_keywords_en.json` | 16-category land-cover keyword mapping |
| `references/ssp_scenarios_en.json` | SSP scenario standard-name mapping |
| `references/manifest_template.json` | Blank manifest.json template |
| `@see ../../data/manifest-schema.json` | manifest JSON Schema definition |
| `@see ../../data/DATA_FORMAT_SPEC.md` | data directory format specification |

## ⚠️ Edge-Case Handling

| Case | Handling |
|------|----------|
| Data directory is empty | Prompt the user to add GeoTIFF files and rescan |
| No GDAL Python bindings | Prompt installation: `pip install GDAL`; degrade to filename-only classification |
| Filename cannot extract a year | Mark as `year: null`; prompt the user to fill in manually |
| manifest-schema.json missing | Skip schema validation; only check file references |
| Resume after interruption | Read `.standardize-progress.json` and continue from `current_stage` |
| Rename conflict (target file already exists) | Add a numeric suffix `_2`; prompt the user to confirm |
| Multi-band GeoTIFF | Mark for confirmation; take band-1 metadata by default |
