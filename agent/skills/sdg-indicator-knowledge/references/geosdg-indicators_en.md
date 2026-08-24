# GeoSDG Spatialized SDG Indicator Detailed Specification

> Source: GeoSDG paper Table 1 (New Pathways for UN SDGs Spatialization)
> 10 SDGs · 17 targets · 27 concrete indicators

---

## Calculation Type Quick Reference

| Symbol | Type name | CLI command | Core formula |
|------|---------|---------|---------|
| `*` | Land Proportion | `sdg-land-proportion` | `score = norm(target_pixels / total_pixels, min, max)` × 100 |
| `**` | Land Conversion | `sdg-land-conversion` | `score = norm(converted_pixels / initial_pixels, min, max)` × 100 |
| `***` | Buffer Zone | `sdg-buffer-zone` | `score = norm(covered_target / total_target, min, max)` × 100 |
| `****` | Total Statistics | `sdg-1131` / `sdg-1322` | Aggregate statistics for a specific indicator |

### Normalization Formula

$$X_{norm} = \begin{cases} \frac{X - X_{min}}{X_{max} - X_{min}}, & \text{Positive indicator} \\ 1 - \frac{|X - X_{opt}|}{\max(|X - X_{min}|, |X - X_{max}|)}, & \text{Neutral indicator} \\ \frac{X_{max} - X}{X_{max} - X_{min}}, & \text{Negative indicator} \end{cases}$$

Final score: $Score = X_{norm} \times 100$

---

## SDG 2: Zero Hunger

### Target 2.1.2

| Attribute | Value |
|------|-----|
| Indicator No. | 1 |
| Indicator name | Proportion of land for agriculture use |
| Calculation type | `*` Land Proportion |
| CLI command | `sdg-land-proportion` |
| Input | LUCC GeoTIFF (GDT_Byte), cropland code (e.g., `{1}`) |
| Max threshold | 0.8 (≥80% cropland = 100) |
| Min threshold | 0.1 (≤10% cropland = 0) |
| Normalization direction | Positive (higher proportion = higher score) |
| Score range | [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 2 |
| Indicator name | Grain yield |
| Calculation type | `****` Total Statistics |
| Input | LUCC + population data + grain yield coefficient |
| Output | Per-capita grain yield, normalized to [0, 100] |
| Status | 🔄 Planned |

### Target 2.4.1

| Attribute | Value |
|------|-----|
| Indicator No. | 3 |
| Indicator name | Proportion of land for easy accessibility |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | LUCC GeoTIFF (GDT_Byte, cropland code) + road buffer (GDT_Byte) |
| Output | Proportion of cropland within road coverage, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 4 |
| Indicator name | Proportion of land for efficient irrigation practices |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | LUCC GeoTIFF (GDT_Byte, cropland code) + irrigation facility buffer (GDT_Byte) |
| Output | Proportion of cropland within irrigation facility coverage, normalized to [0, 100] |

---

## SDG 3: Good Health and Well-being

### Target 3.8.1

| Attribute | Value |
|------|-----|
| Indicator No. | 5 |
| Indicator name | Proportion of health facility coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + health facility buffer (GDT_Byte) |
| Output | Proportion of population served by health facilities, normalized to [0, 100] |
| Normalization direction | Positive (higher coverage = higher score) |

| Attribute | Value |
|------|-----|
| Indicator No. | 6 |
| Indicator name | Proportion of elderly care facility coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + elderly care facility buffer (GDT_Byte) |
| Output | Proportion of population served by elderly care facilities, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 7 |
| Indicator name | Proportion of sport and cultural facility coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + sport and cultural facility buffer (GDT_Byte) |
| Output | Proportion of population served by sport and cultural facilities, normalized to [0, 100] |

### Target 3.c.1

| Attribute | Value |
|------|-----|
| Indicator No. | 8 |
| Indicator name | Proportion of community healthcare institution coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + community healthcare institution buffer (GDT_Byte) |
| Output | Proportion of population served by community healthcare institutions, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 9 |
| Indicator name | Proportion of city-level hospital coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + city-level hospital buffer (GDT_Byte) |
| Output | Proportion of population served by city-level hospitals, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 10 |
| Indicator name | Proportion of tiered hospital coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + tiered hospital buffer (GDT_Byte) |
| Output | Proportion of population served by tiered hospitals, normalized to [0, 100] |

---

## SDG 4: Quality Education

### Target 4.1.2

| Attribute | Value |
|------|-----|
| Indicator No. | 11 |
| Indicator name | Proportion of preschool coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + preschool buffer (GDT_Byte) |
| Output | Proportion of population served by preschools, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 12 |
| Indicator name | Proportion of primary school coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + primary school buffer (GDT_Byte) |
| Output | Proportion of population served by primary schools, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 13 |
| Indicator name | Proportion of secondary school coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + secondary school buffer (GDT_Byte) |
| Output | Proportion of population served by secondary schools, normalized to [0, 100] |

---

## SDG 6: Clean Water and Sanitation

### Target 6.6.1

| Attribute | Value |
|------|-----|
| Indicator No. | 14 |
| Indicator name | Trends in water-related ecosystems over time |
| Calculation type | `*` Land Proportion |
| CLI command | `sdg-land-proportion` |
| Input | LUCC GeoTIFF (GDT_Byte), water body code (e.g., `{4}`) |
| Max threshold | 0.3 (≤30% water = 100) |
| Min threshold | 0.05 (≤5% water = 0) |
| Normalization direction | Positive (higher proportion = higher score) |
| Score range | [0, 100] |

---

## SDG 7: Affordable and Clean Energy

### Target 7.2.1

| Attribute | Value |
|------|-----|
| Indicator No. | 15 |
| Indicator name | Proportion of community clean energy facility coverage |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + clean energy facility buffer (GDT_Byte) |
| Output | Proportion of population served by clean energy facilities, normalized to [0, 100] |

---

## SDG 9: Industry, Innovation and Infrastructure

### Target 9.1.1

| Attribute | Value |
|------|-----|
| Indicator No. | 16 |
| Indicator name | Percentage of population residing within 2km of a road |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + road buffer (GDT_Byte, radius=2km) |
| Output | Proportion of population within the 2 km road buffer, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 17 |
| Indicator name | Proportion of population served by subway stations |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + subway station buffer (GDT_Byte) |
| Output | Proportion of population served by subway stations, normalized to [0, 100] |

### Target 9.c.1

| Attribute | Value |
|------|-----|
| Indicator No. | 18 |
| Indicator name | Proportion of population served by a mobile network |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + mobile base station buffer (GDT_Byte) |
| Output | Proportion of population served by a mobile network, normalized to [0, 100] |

---

## SDG 11: Sustainable Cities and Communities

### Target 11.2.1

| Attribute | Value |
|------|-----|
| Indicator No. | 19 |
| Indicator name | Proportion of population served by subway stations |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + subway station buffer (GDT_Byte) |
| Output | Proportion of population served by subway stations, normalized to [0, 100] |

| Attribute | Value |
|------|-----|
| Indicator No. | 20 |
| Indicator name | Proportion of population served by bus stations |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | Population GeoTIFF (Float32) + bus station buffer (GDT_Byte) |
| Output | Proportion of population served by bus stations, normalized to [0, 100] |

### Target 11.3.1

| Attribute | Value |
|------|-----|
| Indicator No. | 21 |
| Indicator name | Ratio of land consumption rate to population growth rate |
| Calculation type | `****` Total Statistics |
| CLI command | `sdg-1131` |
| Input | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) + Initial Population (Float32) + Current Population (Float32) |
| Urban land code | e.g., `{5}` (urban land) |
| Optimal threshold | 1.12 (based on the UN-Habitat recommended value) |
| Normalization direction | Neutral (deviation from optimal is penalized) |
| Score range | [0, 100]; over-rapid urban growth or over-rapid population growth both reduce the score |

**Calculation formula**:
- Urban land growth rate = (Current_Urban - Initial_Urban) / Initial_Urban
- Population growth rate = (Current_Pop - Initial_Pop) / Initial_Pop
- Ratio = urban land growth rate / population growth rate
- Score = triangular_norm(Ratio, min, max, optimal)

### Target 11.7.1

| Attribute | Value |
|------|-----|
| Indicator No. | 22 |
| Indicator name | Coverage rate of urban open public spaces |
| Calculation type | `***` Buffer Zone |
| CLI command | `sdg-buffer-zone` |
| Input | LUCC GeoTIFF (GDT_Byte, green space / square codes) + urban area buffer (GDT_Byte) |
| Output | Proportion of open public spaces within urban areas, normalized to [0, 100] |

---

## SDG 13: Climate Action

### Target 13.2.2

| Attribute | Value |
|------|-----|
| Indicator No. | 23 |
| Indicator name | Status of achieving "Carbon Peak" |
| Calculation type | `****` Total Statistics |
| CLI command | `sdg-1322` |
| Input | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) + carbon emission coefficient per land type |
| Emission coefficient | `unordered_map<int, double>` — key=land type code, value=emission coefficient (positive=emission, negative=absorption) |
| Emission reduction ratio | `dRatio`, range (0, 1) |
| Scoring rule | Emission decrease = 100 points (peak reached), emission increase = linearly normalized to [0, 100) |

**Emission coefficient reference** (from Yao et al., 2023):

| Land type | Code | Emission coefficient (tC/ha/yr) |
|------|------|-------------------|
| Cropland | 1 | ~0.5 |
| Forest | 2 | ~-1.2 (carbon sink) |
| Grassland | 3 | ~0.1 |
| Water | 4 | ~0 |
| Urban/Built-up | 5 | ~50.0 |
| Barren | 6 | ~0 |

> Actual coefficients must be calibrated for the study area.

---

## SDG 14: Life below Water

### Target 14.5.1

| Attribute | Value |
|------|-----|
| Indicator No. | 24 |
| Indicator name | Changes in water ecosystem within protected areas |
| Calculation type | `**` Land Conversion |
| CLI command | `sdg-land-conversion` |
| Input | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| Conversion type | Water body → non-water (within protected area boundaries) |
| Normalization direction | Negative (more water loss = lower score) |
| Scoring rule | The greater the water area loss ratio, the lower the score; stable or increased water area = 100 |
| Max threshold | 0.3 (≥30% water loss = 0) |
| Min threshold | 0 (no loss = 100) |

---

## SDG 15: Life on Land

### Target 15.1.1

| Attribute | Value |
|------|-----|
| Indicator No. | 25 |
| Indicator name | Percentage of forest area relative to total land area |
| Calculation type | `*` Land Proportion |
| CLI command | `sdg-land-proportion` |
| Input | LUCC GeoTIFF (GDT_Byte), forest code (e.g., `{2}`) |
| Max threshold | 0.5 (≥50% forest = 100) |
| Min threshold | 0.05 (≤5% forest = 0) |
| Normalization direction | Positive (higher proportion = higher score) |
| Score range | [0, 100] |

### Target 15.2.1

| Attribute | Value |
|------|-----|
| Indicator No. | 26 |
| Indicator name | Percentage of progress towards sustainable forest management |
| Calculation type | `**` Land Conversion |
| CLI command | `sdg-land-conversion` |
| Input | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| Conversion type | Forest → non-forest (deforestation) |
| Normalization direction | Negative (more deforestation = lower score) |
| Scoring rule | The greater the forest area loss, the lower the score; forest increase = 100 |

### Target 15.3.1

| Attribute | Value |
|------|-----|
| Indicator No. | 27 |
| Indicator name | Percentage of degraded land compared to total land area |
| Calculation type | `**` Land Conversion |
| CLI command | `sdg-land-conversion` |
| Input | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| Conversion type | Vegetation (forest/grassland) → barren/urban (degradation) |
| Normalization direction | Negative (more degradation = lower score) |
| Scoring rule | The greater the degraded area ratio, the lower the score; zero degradation = 100 |
| Max threshold | 0.5 (≥50% degraded = 0) |
| Min threshold | 0 (no degradation = 100) |

---

## Priority Area Identification Rules

Based on SDG indicator calculation results, six rules identify areas requiring priority intervention:

| Rule | Type | Trigger condition |
|------|------|---------|
| Rule 1 | Land Proportion decline | Proportion of key land types (cropland/forest/water) falls below threshold |
| Rule 2 | Land Conversion negative | Land degradation / deforestation / water loss ratio exceeds threshold |
| Rule 3 | Buffer Zone insufficient | Proportion of population covered by key facility services falls below threshold |
| Rule 4 | SDG 11.3.1 imbalance | Urban land growth / population growth ratio deviates from the optimal value |
| Rule 5 | SDG 13.2.2 peak not reached | Carbon emissions keep rising with no sign of peaking |
| Rule 6 | Comprehensive ranking overlay | Multiple indicators trigger simultaneously in the same area, the higher the grade |

**Priority grading**:
- **Low** (monitoring only): <10% of SDG indicators triggered
- **Moderate** (moderate attention): 20–30% of indicators triggered
- **High** (key attention): 40–50% of indicators triggered
- **Critical** (major measures): >50% of indicators triggered

Code location: `ExtractPriorityAreas::extractRuleN()` (N=1~6) + `mergePriorityAreas()`

---

## Data Source Reference

| Data | Format | Resolution | Source |
|------|------|--------|------|
| Land use | GeoTIFF | 30m | CNLUCC (Resource and Environment Science and Data Center) |
| Population distribution | GeoTIFF | 1km | WorldPop Hub |
| POI (infrastructure) | Shapefile Point | — | OpenStreetMap / Gaode |
| Road network | Shapefile Line | — | OpenStreetMap |
| DEM | GeoTIFF | 30m | ASTER GDEM |
| Carbon emission coefficient | CSV/Text | — | Yao et al., 2023 |
| Ecological protected areas | Shapefile Polygon | — | Protected Planet |

---

## Complete CLI Examples

### Land Proportion (calculate forest area proportion)

```bash
geosdg-cli sdg-land-proportion \
  --lucctype 2 \
  --input lucc_2020.tif \
  --max 0.5 --min 0.05
```

### Land Conversion (calculate forest degradation)

```bash
geosdg-cli sdg-land-conversion \
  --ori lucc_2010.tif --new lucc_2020.tif \
  --conv "2->5,6" \
  --max 0.3 --min 0 --state false
```

### Buffer Zone (calculate hospital service coverage)

```bash
geosdg-cli sdg-buffer-zone \
  --input population_2020.tif \
  --buffer hospital_buffer.tif \
  --max 0.9 --min 0.1
```

### SDG 11.3.1 (urban coordination)

```bash
geosdg-cli sdg-1131 \
  --lucctype 5 \
  --lucc-initial lucc_2010.tif --lucc-current lucc_2020.tif \
  --pop-initial pop_2010.tif --pop-current pop_2020.tif \
  --best 1.12 --max 5.0 --min 0.5
```

### SDG 13.2.2 (carbon emission peaking)

```bash
geosdg-cli sdg-1322 \
  --ori lucc_2010.tif --new lucc_2020.tif \
  --ratio 0.1 \
  --scheme "1:0.5,2:-1.2,3:0.1,4:0,5:50,6:0"
```
