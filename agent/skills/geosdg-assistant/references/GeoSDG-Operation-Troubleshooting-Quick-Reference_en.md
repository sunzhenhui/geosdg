# GeoSDG Operation Troubleshooting Quick Reference (Operation → Input/Output/Expected Result/Issues)

> Generated from the CLI source code (`cli/src/`) and the PDF document "Introduction for GeoSDG.pdf".

---

## Operation 1: CA Simulation Accuracy Assessment

| Item | Detail |
|------|--------|
| **Operation name** | Assess the accuracy of CA simulation results |
| **CLI example** | `geosdg-cli ca-precision --ori ../data/LUCC/2010.tif --sim ../data/LUCC/Simulation.tif --real ../data/LUCC/2020.tif` |
| **Paper reference** | Section 3.2: Implementation results (Land use and infrastructure coverage) |
| **Input** | ① Original land-use data (`2010.tif`, GDT_Byte) ② Simulated result (`Simulation.tif`, GDT_Byte) ③ Real land-use data (`2020.tif`, GDT_Byte) |
| **Output** | FoM (Figure of Merit), PA (Producer's Accuracy), UA (User's Accuracy), Kappa coefficient, OA (Overall Accuracy) |
| **Expected result** | FoM and OA values measure the accuracy of a CA model (e.g., FLUS/PLUS) simulation |
| **Code location** | `CalculateCAPrecision.cpp:45` (`calculatePrecision`) |
| **Call example** | `main.cpp:26-29` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| File cannot be opened, returns all zeros | `GDALOpen returns NULL` | `CalculateCAPrecision.cpp:33-36` — wrong input path or file does not exist |
| Data type mismatch, function silently returns | `GDT_Byte check failed` | `CalculateCAPrecision.cpp:41-44` — input must be Byte type and single-band |
| Raster size mismatch, function silently returns | `size mismatch` | `CalculateCAPrecision.cpp:58-65` — the three images must have identical row/column counts |
| FoM value is NaN or abnormal | `denominator is zero` | `CalculateCAPrecision.cpp:140` — if A+B+C+D=0 (no change area), FoM divides by zero |
| Kappa value abnormal | `_pe close to 1` | `CalculateCAPrecision.cpp:199` — if chance agreement is extremely high, the Kappa denominator approaches 0 |

---

## Operation 2: Pearson Correlation Coefficient

| Item | Detail |
|------|--------|
| **Operation name** | Compute the Pearson correlation coefficient of two SDG-score groups |
| **CLI example** | `geosdg-cli correlation --data1 34.7,34.4,51.8,51.1,44.1 --data2 45.2,45.5,47.6,52.7,48.7` |
| **Paper reference** | Section 3.2: Implementation results for SDG scores (Table 3) |
| **Input** | ① Traditional statistical SDG score array `data1` ② GeoSDG spatial SDG score array `data2` |
| **Output** | Pearson correlation coefficient (-1 ~ 1) |
| **Expected result** | Verifies the consistency between the spatial SDG index and the traditional statistical method |
| **Code location** | `CalculateCAPrecision.cpp:338` (`calculateCorrelationCoefficient`) |
| **Call example** | `main.cpp:30-33` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Throws invalid_argument | `vector length mismatch / empty` | `CalculateCAPrecision.cpp:212-214` — the two groups must have the same, non-empty length |
| Returns 0 | `denominator is zero` | `CalculateCAPrecision.cpp:226-228` — data variance is 0 (constant series), cannot compute |

---

## Operation 3: Independent-Samples t-Test

| Item | Detail |
|------|--------|
| **Operation name** | Test whether two SDG-score groups differ significantly |
| **CLI example** | `geosdg-cli t-test --data1 34.7,34.4,51.8,51.1,44.1 --data2 45.2,45.5,47.6,52.7,48.7` |
| **Paper reference** | Section 3.2: Table 3 |
| **Input** | ① Sample-1 SDG scores `data1` ② Sample-2 SDG scores `data2` |
| **Output** | t statistic |
| **Expected result** | Combined with degrees of freedom and significance level, judge whether the two groups differ significantly |
| **Code location** | `CalculateCAPrecision.cpp:415` (`tTestIndependent`) |
| **Call example** | `main.cpp:34-35` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Variance computation divides by zero | `sample size = 1` | `CalculateCAPrecision.cpp:244` — variance divides by (n-1); when n=1 it divides by zero (unprotected) |
| t value abnormally large | `variance extremely small` | `CalculateCAPrecision.cpp:258` — when both variances are extremely small, the t statistic may overflow |

---

## Operation 4: Land-Proportion Indicator (SDG 2.1.2 / 6.6.1 / 15.1.1)

| Item | Detail |
|------|--------|
| **Operation name** | Compute the normalized score of the proportion of a specific land-use type |
| **CLI example** | `geosdg-cli sdg-land-proportion --init-lucc ../data/LUCC/2025.tif --types 2,3 --max 100 --min 0` |
| **Paper reference** | Figure 9: Land Proportion Indicators |
| **Input** | ① Land-use data (GeoTIFF, GDT_Byte) ② Max threshold `dMaxThreshold` ③ Min threshold `dMinThreshold` ④ Observed land-type set `setLuccTypeSDG` |
| **Output** | Normalized score (0~100) |
| **Expected result** | The higher the proportion of the specified type (positive indicator), the higher the score |
| **Code location** | `CalculateSDG.cpp:34` (`calculateLandProportionIndicator`) |
| **Call example** | `main.cpp:41-48` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Returns 0 | `GDALOpen failed` | `CalculateSDG.cpp:58-61` — wrong file path or unsupported format |
| Opens with GA_Update | `read-only data modified` | `CalculateSDG.cpp:58` — opens with `GA_Update`, which may accidentally modify the input file |
| Score always 0 or 100 | `thresholds set incorrectly` | `CalculateSDG.cpp:517-518` — returns 0 if the actual proportion <= dMinThreshold, 100 if >= dMaxThreshold |
| NoData judgment abnormal | `NoData is not an integer` | `CalculateSDG.cpp:71` — comparing a Byte value with a double NoData may fail if NoData is not an integer in 0~255 |

---

## Operation 5: Land-Conversion Indicator (SDG 14.5.1 / 15.2.1 / 15.3.1)

| Item | Detail |
|------|--------|
| **Operation name** | Compute the normalized score of a specific land-use transition proportion |
| **CLI example** | `geosdg-cli sdg-land-conversion --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --transitions 2:5:6,4:5 --positive` |
| **Paper reference** | Figure 9: Land Conversion Indicators |
| **Input** | ① Initial-period land use ② Change-period land use ③ Transition-type mapping ④ Max threshold ⑤ Min threshold ⑥ Positive/negative flag `bState` |
| **Output** | Normalized score (0~100); higher is better for positive indicators, lower is better for negative indicators |
| **Expected result** | Normalization direction is determined by `bState` |
| **Code location** | `CalculateSDG.cpp:108` (`calculateLandConversionIndicator`) |
| **Call example** | `main.cpp:52-60` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Returns 0 | `dimension mismatch` | `CalculateSDG.cpp:154-158` — the two-period data must have identical row/column counts |
| Transition-proportion denominator abnormal | `nAllCount is 0` | `CalculateSDG.cpp:195` — if no transition type matches, division by zero yields NaN |
| Normalization direction wrong | `bState set backwards` | `CalculateSDG.cpp:200-201` — positive uses `normalization`, negative uses `normalizationNegative` |
| Transition-mapping logic complex | `targetLuccTypesSet contains first` | `CalculateSDG.cpp:187` — when targetTypes contains the source type, nAllCount also counts "unchanged" pixels |

---

## Operation 6: Buffer-Zone Indicator (SDG 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1)

| Item | Detail |
|------|--------|
| **Operation name** | Compute the coverage score of a specific land type / population inside vs outside a buffer (infrastructure coverage) |
| **CLI example** | `geosdg-cli sdg-buffer-zone --init-lucc ../data/LUCC/2025.tif --buffer ../data/INFRA/roads.tif --types 2,3 --max 100 --min 0` |
| **Paper reference** | Figure 9: Buffer Zone Indicators |
| **Input** | ① Land-use or population data (auto-detected type) ② Buffer (infrastructure) coverage data ③ Observed land-type set ④ Max threshold ⑤ Min threshold |
| **Output** | Normalized score (0~100) |
| **Expected result** | The higher the proportion of the specified type inside the infrastructure coverage, the higher the score |
| **Code location** | `CalculateSDG.cpp:233` (`calculateBufferZoneIndicator`) |
| **Call example** | `main.cpp:64-76` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Returns 0 | `data type unsupported` | `CalculateSDG.cpp:249-252` — only GDT_Byte/Float32/Float64 are supported |
| Population data not filtered by LUCC type | `Float type skips type filtering` | `CalculateSDG.cpp:317-318` — population data (Float) accumulates values directly, without checking the LUCC type set |
| Division by zero | `dOriSum is 0` | `CalculateSDG.cpp:368` — if there is no valid data, dOriSum=0 causes division by zero |
| Buffer data has no valid band | `nLayers <= 0` | `CalculateSDG.cpp:270-274` — the buffer file has no raster band |

---

## Operation 7: SDG 11.3.1 Indicator

| Item | Detail |
|------|--------|
| **Operation name** | Compute the ratio of urban-land growth rate to population growth rate, assessing urbanization coordination |
| **CLI example** | `geosdg-cli sdg-1131 --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --init-popu ../data/POPU/2025.tif --curr-popu ../data/POPU/2050.tif --types 2,3 --max 3.0 --min 0 --best 1.12` |
| **Paper reference** | SDG 11.3.1 |
| **Input** | ① Initial LUCC ② Current LUCC ③ Initial population ④ Current population ⑤ Urban land-type set ⑥ Max threshold ⑦ Min threshold ⑧ Optimal threshold |
| **Output** | Normalized score (0~100); the closer to the optimal value, the higher the score |
| **Expected result** | The ratio falls in [min, max]; the closer to best, the higher the score; out of range yields 0 |
| **Code location** | `CalculateSDG.cpp:429` (`calculateSDG1131Indicator`) |
| **Call example** | `main.cpp:81-90` |

### Possible issues

| Issue | Keyword | Module / file / rationale |
|-------|---------|---------------------------|
| Population growth rate 0 causes division by zero | `_dRatioPOPU=0` | `CalculateSDG.cpp:431` — if initial population is 0 or the two periods are equal, the ratio divides by zero |
| Urban-land growth rate 0 | `_dRatioLUCC=0` | `CalculateSDG.cpp:429` — initial urban land 0 causes division by zero |
| Only the first ratio pair is computed | `vRatios[0]` | `CalculateSDG.cpp:452` — multi-period data only uses the first pair; the rest is ignored |
| Optimal threshold equals min/max threshold | `best=min or best=max` | `CalculateSDG.cpp:442,448` — distance divides by 0, yielding NaN |

<!-- CONTINUE -->
