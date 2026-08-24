# GeoSDG Project Structure Quick Reference (Function → Code File Mapping)

## Project Overview

GeoSDG is a cross-platform CLI spatial Sustainable Development Goals (SDG) assessment tool based on C++17 / CMake / GDAL. Its main features include: CA simulation accuracy assessment, spatial SDG indicator computation, and priority-area identification & ranking. The code is located in `cli/src/`. **It no longer depends on Qt.**

---

## Module Overview

| Module | Header | Source | Core function |
|--------|--------|--------|---------------|
| CA accuracy assessment | `CalculateCAPrecision.h` | `CalculateCAPrecision.cpp` | CA simulation accuracy metrics, correlation analysis, t-test |
| SDG indicator computation | `CalculateSDG.h` | `CalculateSDG.cpp` | 5 types of spatial SDG indicators and normalization |
| Priority-area extraction | `ExtractPriorityAreas.h` | `ExtractPriorityAreas.cpp` | 6 rules to identify priority areas + ranking-map generation |
| Logging system | `Logger.h` | `Logger.cpp` | File + console dual-channel logging, four levels, checkpoint resume |
| Program entry | — | `main.cpp` | CLI tool, 16 sub-commands + help, main entry `main.cpp:674` |

---

## Function → File Detailed Mapping

### 1. CA Simulation Accuracy Assessment Module (`CalculateCAPrecision`)

| Function | Function name | File | Description |
|----------|---------------|------|-------------|
| CA simulation accuracy assessment | `calculatePrecision()` | `CalculateCAPrecision.cpp:45` | Computes FoM, PA, UA, Kappa, OA five accuracy metrics |
| Pearson correlation coefficient | `calculateCorrelationCoefficient()` | `CalculateCAPrecision.cpp:338` | Computes linear correlation of two SDG-score datasets |
| Independent-samples t-test | `tTestIndependent()` | `CalculateCAPrecision.cpp:415` | Tests whether two SDG-score groups differ significantly |
| Mean computation | `mean()` | `CalculateCAPrecision.cpp:393` | Private helper, computes the sample mean |
| Variance computation | `variance()` | `CalculateCAPrecision.cpp:399` | Private helper, computes the sample variance |

### 2. SDG Indicator Computation Module (`CalculateSDG`)

| Function | Function name | File | Indicator category | Corresponding SDG |
|----------|---------------|------|--------------------|-------------------|
| Land-proportion indicator | `calculateLandProportionIndicator()` | `CalculateSDG.cpp:34` | Land Proportion | SDG 2.1.2 / 6.6.1 / 15.1.1 |
| Land-conversion indicator | `calculateLandConversionIndicator()` | `CalculateSDG.cpp:108` | Land Conversion | SDG 14.5.1 / 15.2.1 / 15.3.1 |
| Buffer-zone indicator | `calculateBufferZoneIndicator()` | `CalculateSDG.cpp:233` | Buffer Zone | SDG 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1 |
| SDG 11.3.1 indicator | `calculateSDG1131Indicator()` | `CalculateSDG.cpp:429` | Total Statistics | SDG 11.3.1 (urban-land growth rate / population growth rate) |
| SDG 13.2.2 indicator | `calculateSDG1322Indicator()` | `CalculateSDG.cpp:549` | Total Statistics | SDG 13.2.2 (carbon-emission peaking assessment) |
| Positive normalization | `normalization()` | `CalculateSDG.cpp:614` | Helper | Higher value → higher score (0~100) |
| Negative normalization | `normalizationNegative()` | `CalculateSDG.cpp:628` | Helper | Lower value → higher score (0~100) |
| Population sum | `getPopuSum()` | `CalculateSDG.cpp:641` | Helper | Reads the Float32 population raster and sums it |
| Urban-land statistics | `getUrbanSum()` | `CalculateSDG.cpp:686` | Helper | Reads the Byte land-use raster and counts pixels of the given type |
| Total carbon emission | `getEmissionSum()` | `CalculateSDG.cpp:732` | Helper | Computes land-use carbon emission from emission coefficients |

### 3. Priority-Area Extraction Module (`ExtractPriorityAreas`)

| Function | Function name | File | Rule | Description |
|----------|---------------|------|------|-------------|
| Land-encroached area | `PriorityAreasExtractLUCCLoss()` | `ExtractPriorityAreas.cpp:31` | Rule 1 | Identifies areas where a specific land-use type is encroached by others |
| Land-transition area | `PriorityAreasExtractLUCCTransition()` | `ExtractPriorityAreas.cpp:131` | Rule 2 | Identifies areas with a specific land-use transition type |
| Outside-buffer area | `PriorityAreasExtractOutsideBufferArea()` | `ExtractPriorityAreas.cpp:237` | Rule 3/4 | Identifies infrastructure-uncovered + specific land-use/population-threshold areas |
| Emission-not-peaked area | `PriorityAreasExtractEmissionNoPeak()` | `ExtractPriorityAreas.cpp:277` | Rule 5 | Identifies areas where carbon emission is still rising in the neighborhood |
| Human-land imbalance area | `PriorityAreasExtractHumanLandRelationship()` | `ExtractPriorityAreas.cpp:320` | Rule 6 | Identifies areas where urban expansion and population change are mismatched |
| Generate ranking map | `generatePriorityAreas()` | `ExtractPriorityAreas.cpp:415` | Merge | Overlays the 6 priority-area files to generate the ranking map |
| Extract land-use priority area (outside buffer) | `extractLUCC()` | `ExtractPriorityAreas.cpp:495` | Private helper | Specific LUCC types outside the buffer |
| Extract population priority area (outside buffer) | `extractPOPU()` | `ExtractPriorityAreas.cpp:573` | Private helper | Population areas over the threshold outside the buffer |
| Compute emission prefix sum | `calculatePrefixEmisiion()` | `ExtractPriorityAreas.cpp:657` | Private helper | 2D prefix sum to accelerate neighborhood emission computation |
| Extract emission-increase area | `extractEmissionIncreaseLand()` | `ExtractPriorityAreas.cpp:740` | Private helper | Uses the prefix sum to judge whether neighborhood emission increases |
| Remove NoData | `removeNoDataFromSecondRaster()` | `ExtractPriorityAreas.cpp:820` | Private helper | Masks the target raster with the reference raster's NoData |

### 4. Program Entry (`main.cpp`)

A CLI tool supporting 16 sub-commands; parses arguments via `parseArgs()` and routes to the corresponding computation module via `dispatch()`.

| Sub-command | Module | Function |
|-------------|--------|----------|
| `ca-precision` | CalculateCAPrecision | CA simulation accuracy assessment |
| `correlation` | CalculateCAPrecision | Pearson correlation coefficient |
| `t-test` | CalculateCAPrecision | Independent-samples t-test |
| `sdg-land-proportion` | CalculateSDG | Land-proportion indicator |
| `sdg-land-conversion` | CalculateSDG | Land-conversion indicator |
| `sdg-buffer-zone` | CalculateSDG | Buffer-zone indicator |
| `sdg-1131` | CalculateSDG | SDG 11.3.1 |
| `sdg-1322` | CalculateSDG | SDG 13.2.2 |
| `priority-loss` | ExtractPriorityAreas | Rule 1: land encroached |
| `priority-transition` | ExtractPriorityAreas | Rule 2: specific transition |
| `priority-buffer` | ExtractPriorityAreas | Rule 3/4: outside buffer |
| `priority-emission` | ExtractPriorityAreas | Rule 5: emission not peaked |
| `priority-human-land` | ExtractPriorityAreas | Rule 6: human-land imbalance |
| `priority-merge` | ExtractPriorityAreas | Merge ranking map |
| `demo` | All | 8-step full-workflow demo, supports `--resume` checkpoint resume |
| `help` | — | Show help information |

---

## Data Dependency Mapping

| Data directory | Data type | Format | Used by module |
|----------------|-----------|--------|----------------|
| `data/LUCC/` | Land use / cover | GeoTIFF (GDT_Byte) | `CalculateSDG`, `ExtractPriorityAreas`, `CalculateCAPrecision` |
| `data/POPU/` | Population distribution | GeoTIFF (GDT_Float32/64) (Rule 6 is Float32-only) | `CalculateSDG`, `ExtractPriorityAreas` |
| `data/INFRA/` | Infrastructure coverage | GeoTIFF (GDT_Byte) | `CalculateSDG`, `ExtractPriorityAreas` |
| `data/Simulation.tif` | CA simulation result | GeoTIFF (GDT_Byte) | `CalculateCAPrecision` |

---

## Dependency Libraries

| Library | Minimum version | Purpose |
|---------|-----------------|---------|
| CMake | 3.10 | Cross-platform build system, with automatic platform detection (`GEOSDG_PLATFORM_WINDOWS/MACOS/LINUX`) |
| GDAL | — | Raster data read/write, spatial-reference processing (no specific version binding) |

**Note**: The CLI version **no longer depends on Qt**. The Qt 5.10 used by the legacy UI version has been removed. See `geosdg-cli/CMakeLists.txt` for the build approach.

---

## Function → Common Issues Index

> For detailed troubleshooting, see [GeoSDG-Operation-Troubleshooting-Quick-Reference_en.md](./GeoSDG-Operation-Troubleshooting-Quick-Reference_en.md)

### CalculateCAPrecision module issues

| Function | Common issue | Keyword |
|----------|--------------|---------|
| `calculatePrecision()` | File-open failure returns all zeros | `GDALOpen returns NULL` |
| `calculatePrecision()` | Non-Byte data type silently returns | `GDT_Byte check failed` |
| `calculatePrecision()` | Three images have inconsistent dimensions | `size mismatch` |
| `calculatePrecision()` | FoM denominator is zero | `A+B+C+D=0` |
| `calculateCorrelationCoefficient()` | Unequal vector lengths throw an exception | `vector length mismatch / empty` |
| `tTestIndependent()` | Sample size = 1, variance division by zero | `sample size = 1` |

### CalculateSDG module issues

| Function | Common issue | Keyword |
|----------|--------------|---------|
| `calculateLandProportionIndicator()` | Opening with GA_Update may modify the source file | `GA_Update` |
| `calculateLandProportionIndicator()` | NoData value compared with Byte behaves abnormally | `NoData not an integer` |
| `calculateLandConversionIndicator()` | Two-period data dimension mismatch | `dimension mismatch` |
| `calculateLandConversionIndicator()` | nAllCount is 0, causing division by zero | `nAllCount is 0` |
| `calculateBufferZoneIndicator()` | Population data does not check land-use type | `Float type skips type filtering` |
| `calculateBufferZoneIndicator()` | dOriSum is 0, causing division by zero | `dOriSum is 0` |
| `calculateSDG1131Indicator()` | Population growth rate is 0, division by zero | `_dRatioPOPU=0` |
| `calculateSDG1131Indicator()` | Multiple periods only use the first pair | `vRatios[0]` |
| `calculateSDG1322Indicator()` | Emission-coefficient map modified in-place | `vLUCCEmissionScheme modified` |
| `calculateSDG1322Indicator()` | Normalization denominator is 0 | `min=0` |

### ExtractPriorityAreas module issues

| Function | Common issue | Keyword |
|----------|--------------|---------|
| `PriorityAreasExtractLUCCLoss()` | Output file creation failed | `dst=nullptr` |
| `PriorityAreasExtractLUCCLoss()` | GeoTransform retrieval failed | `CE_Failure` |
| `PriorityAreasExtractOutsideBufferArea()` | Data-type auto-recognition dispatch | `GDT_Byte→extractLUCC` |
| `PriorityAreasExtractEmissionNoPeak()` | tmp directory missing | `../tmp/` |
| `PriorityAreasExtractEmissionNoPeak()` | Temp-file cleanup incomplete | `duplicate ChangedPrefix removal` |
| `PriorityAreasExtractHumanLandRelationship()` | Population supports only Float32 | `GDT_Float32 hard-coded` |
| `PriorityAreasExtractHumanLandRelationship()` | Boundary neighborhood incomplete | `boundary clipping` |
| `generatePriorityAreas()` | Input-file size mismatch, out of bounds | `row/column count differ` |
| `generatePriorityAreas()` | Different resolutions cause differences | `resolution mismatch` |

### General issues

| Issue description | Keyword |
|-------------------|---------|
| Cross-platform build failure (CMake/GDAL not found) | `CMake/GDAL/brew install gdal` |
| GDAL Chinese-path anomaly | `GDAL_FILENAME_IS_UTF8` |
| Data coordinate systems must be consistent | `coordinate system / row-col / resolution` |
| Population and LUCC resolutions differ | `resample / align` |
| Administrative-boundary clipping | `shapefile clipping` |
| Log file not created | `Logger/init/logPath` |
| Checkpoint cannot resume | `readLastCheckpoint/CHECKPOINT` |
