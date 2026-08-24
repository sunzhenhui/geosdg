# GeoSDG Assistant Skill — 完整参考资料

> 本文件为 geosdg-assistant Skill 的完整层，由 SKILL.md 摘要层按需加载。
> 摘要层仅包含 CLI 命令速查表，本文件包含架构、函数映射、构建指南和故障排查。

---

## Project Architecture

The GeoSDG project has 4 core modules and 1 entry point:

| Module | Header | Source | Purpose |
|--------|--------|--------|---------|
| CA Precision | `CalculateCAPrecision.h` | `CalculateCAPrecision.cpp` | Simulation accuracy metrics + statistical tests |
| SDG Calculation | `CalculateSDG.h` | `CalculateSDG.cpp` | 5 types of spatial SDG indicators + normalization |
| Priority Areas | `ExtractPriorityAreas.h` | `ExtractPriorityAreas.cpp` | 6 rules for priority identification + ranking map |
| Logger | `Logger.h` | `Logger.cpp` | Dual-channel (file + console) logging + checkpoint/resume |
| Entry Point | — | `main.cpp` | CLI tool with 16 sub-commands |

**Dependencies**: C++17 + CMake 3.10+ + GDAL, 跨平台（Windows / macOS / Linux）. No Qt dependency.

---

## Quick Reference: Function → File Mapping

> Line numbers correspond to `cli/src/` (snapshot version). Check actual source for latest positions.

### CalculateCAPrecision
- `calculatePrecision()` → `CalculateCAPrecision.cpp:45` — FoM, PA, UA, Kappa, OA
- `calculateCorrelationCoefficient()` → `CalculateCAPrecision.cpp:338` — Pearson r
- `mean()` → `CalculateCAPrecision.cpp:393` — Sample mean (private helper)
- `variance()` → `CalculateCAPrecision.cpp:399` — Sample variance (private helper)
- `tTestIndependent()` → `CalculateCAPrecision.cpp:415` — Independent t-test

### CalculateSDG
- `calculateLandProportionIndicator()` → `CalculateSDG.cpp:34` — SDG 2.1.2/6.6.1/15.1.1
- `calculateLandConversionIndicator()` → `CalculateSDG.cpp:108` — SDG 14.5.1/15.2.1/15.3.1
- `calculateBufferZoneIndicator()` → `CalculateSDG.cpp:233` — SDG 2.4.1/3.8.1/3.c.1/4.1.2/7.2.1/9.1.1/9.c.1/11.2.1/11.7.1
- `calculateSDG1131Indicator()` → `CalculateSDG.cpp:429` — SDG 11.3.1
- `calculateSDG1322Indicator()` → `CalculateSDG.cpp:549` — SDG 13.2.2
- `normalization()` → `CalculateSDG.cpp:614` — Positive indicator normalization (0-100)
- `normalizationNegative()` → `CalculateSDG.cpp:628` — Negative indicator normalization (0-100)
- `getPopuSum()` → `CalculateSDG.cpp:641` — Sum population across Float32 rasters
- `getUrbanSum()` → `CalculateSDG.cpp:686` — Sum urban pixel count across Byte rasters
- `getEmissionSum()` → `CalculateSDG.cpp:732` — Compute total carbon emission from LUCC

### ExtractPriorityAreas
- `PriorityAreasExtractLUCCLoss()` → `ExtractPriorityAreas.cpp:31` — Rule 1
- `PriorityAreasExtractLUCCTransition()` → `ExtractPriorityAreas.cpp:131` — Rule 2
- `PriorityAreasExtractOutsideBufferArea()` → `ExtractPriorityAreas.cpp:237` — Rule 3/4
- `PriorityAreasExtractEmissionNoPeak()` → `ExtractPriorityAreas.cpp:277` — Rule 5
- `PriorityAreasExtractHumanLandRelationship()` → `ExtractPriorityAreas.cpp:320` — Rule 6
- `generatePriorityAreas()` → `ExtractPriorityAreas.cpp:415` — Merge into ranking map
- `extractLUCC()` → `ExtractPriorityAreas.cpp:495` — Rule 3 helper (private)
- `extractPOPU()` → `ExtractPriorityAreas.cpp:573` — Rule 4 helper (private)
- `calculatePrefixEmisiion()` → `ExtractPriorityAreas.cpp:657` — 2D prefix sum (private)
- `extractEmissionIncreaseLand()` → `ExtractPriorityAreas.cpp:740` — Detect emission increase (private)
- `removeNoDataFromSecondRaster()` → `ExtractPriorityAreas.cpp:820` — Mask with NoData (private)

---

## Logger Module

| Feature | Description |
|---------|-------------|
| Pattern | Singleton (`Logger::instance()`) |
| Levels | DEBUG (0) / INFO (1) / WARN (2) / ERROR (3) |
| Output | Dual-channel — file + console |
| Checkpoint format | `[CHECKPOINT] step=X total=Y message=...` |

### Convenience Macros

| Macro | Usage |
|-------|-------|
| `LOG_DEBUG(msg)` | Debug-level information |
| `LOG_INFO(msg)` | General information |
| `LOG_WARN(msg)` | Warning condition |
| `LOG_ERROR(msg)` | Error condition |
| `LOG_RESULT(fn, key, val)` | Record intermediate computation results |
| `LOG_PROGRESS(step, total, msg)` | Track execution progress |
| `LOG_CHECKPOINT(step, total, msg)` | Write checkpoint for resume |

### Checkpoint Resume (Demo)

- `demo` sub-command has 8 steps; each writes `LOG_CHECKPOINT(i, 8, "...")` upon completion.
- `demo --resume` reads last checkpoint via `Logger::instance().readLastCheckpoint()`.
- If log file doesn't exist or has no checkpoint, demo starts from step 1.
- Global `--log <path>` customizes the log file path.

---

## Build

**Requirements**: C++17 compiler, CMake 3.10+, GDAL

```bash
cd geosdg-cli
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

**Platform-specific**:
- **Windows**: Bundled `gdal2.0.2/`, MSVC compiler
- **macOS**: `brew install gdal`; GDAL detected via `gdal-config --datadir`
- **Linux**: System GDAL via `find_package(GDAL)`

**Output**: `build/bin/geosdg-cli`

---

## Data Requirements

| Data Type | Format | GDAL DataType | Pixel Values |
|-----------|--------|---------------|--------------|
| Land Use (LUCC) | GeoTIFF | GDT_Byte | 1-6 (land categories) |
| Population (POPU) | GeoTIFF | GDT_Float32/64 | Population count (Rule 6 only supports Float32) |
| Infrastructure (INFRA) | GeoTIFF | GDT_Byte | Covered=valid, Uncovered=NoData |
| CA Simulation Result | GeoTIFF | GDT_Byte | Same as LUCC |

**Critical constraint**: All input rasters must have identical dimensions and coordinate systems.

---

## Operation Workflow

1. **Data Preparation**: Prepare LUCC, POPU, INFRA GeoTIFFs with consistent dimensions and CRS
2. **CA Simulation & Validation**: Run CA model, then `geosdg-cli ca-precision --ori ... --sim ... --real ...`
3. **SDG Indicator Calculation**: Use appropriate sub-command (e.g. `geosdg-cli sdg-1131 --init-lucc ...`)
4. **Consistency Validation**: `geosdg-cli correlation --data1 ... --data2 ...` and `t-test`
5. **Priority Area Identification**: Apply rules, then `geosdg-cli priority-merge --files ... -o ranking.tif`
6. **Visualization**: Use QGIS/ArcGIS to display results

---

## Troubleshooting Quick Guide

| Problem Category | Key Symptoms | Reference File |
|-----------------|--------------|----------------|
| File I/O failure | Functions return 0 silently | `references/GeoSDG-操作问题速查表.md` |
| Data type mismatch | GDT_Byte/Float check fails | `references/GeoSDG-操作问题速查表.md` |
| Dimension mismatch | Silent return or crash | `references/GeoSDG-操作问题速查表.md` |
| Division by zero | NaN or inf in results | `references/GeoSDG-操作问题速查表.md` |
| Modified input parameters | Emission scheme altered after call | `references/GeoSDG-操作问题速查表.md` |
| Temp file issues | Rule 5 fails (no ../tmp/ dir) | `references/GeoSDG-操作问题速查表.md` |
| Resolution inconsistency | Ranking map artifacts | `references/GeoSDG-操作问题速查表.md` |
| Logger issues | Log file not created, checkpoint not found | `references/GeoSDG-操作问题速查表.md` |
| Build issues | CMake or GDAL not found | `references/GeoSDG-操作问题速查表.md` |

---

## Bundled Resources

### References (`references/`)
- **GeoSDG-项目结构速查表.md** — Complete function→file mapping with problem index
- **GeoSDG-操作问题速查表.md** — Complete operation→problem→module mapping with troubleshooting

### Assets (`assets/`)
- **Introduction for GeoSDG.pdf** — Original project documentation (historical reference)
- **source-code/** — Current CLI source code for direct reference
- **Readme.md** — Project overview and resource links

---

## Important Caveats

- **数据安全**：Agent 框架通过 `agent-executor` Step 3.5 自动将所有输入文件拷贝到 `tmp/` 目录，CLI 对副本执行计算，Step 7 自动清理。用户原始数据**绝对不会被修改**
- `calculateLandProportionIndicator()` 底层使用 `GA_Update` 模式，但 Agent 层通过 `tmp/` 副本隔离，实际不会影响用户文件
- `calculateSDG1322Indicator()` 底层会修改 emission 系数映射（in-place），但 C++ 层已做 map 副本保护（`CalculateSDG.cpp:575`），且 Agent 层有 `tmp/` 隔离
- `PriorityAreasExtractEmissionNoPeak()` 底层修改 emission 系数映射，且需要 `../tmp/` 目录存放中间文件。Agent 层通过 `tmp/` 隔离输入数据，并自动创建 `../tmp/` 目录
- `PriorityAreasExtractHumanLandRelationship()` only supports Float32 population data
- `generatePriorityAreas()` does not validate dimension consistency across input files
- NoData handling varies across functions — strict equality may fail for floating-point NoData
- Logger's `readLastCheckpoint()` parses the last `[CHECKPOINT]` line from log
