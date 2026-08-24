# SDG Indicator Knowledge Skill — Full Knowledge Base

> This file is the complete layer of the sdg-indicator-knowledge Skill, loaded on demand by the SKILL.md summary layer.
> The summary layer only contains the 4 computation types and coverage statistics; this file contains the full indicator framework and design methodology.

---

## SDG Indicator Framework Overview

The UN 2030 Agenda contains a three-level indicator system:

| Level | Count | Description |
|-------|-------|-------------|
| Goals | 17 | Top-level themes |
| Targets | 169 | Specific targets under each goal |
| Indicators | 231 | Quantifiable measurement indicators |

### Overview of the 17 SDGs

| SDG | Theme | Spatialization feasibility | GeoSDG coverage |
|-----|-------|----------------------------|-----------------|
| SDG 1 | No Poverty | Low | — |
| SDG 2 | Zero Hunger | Medium-high | ✅ 4 indicators |
| SDG 3 | Good Health and Well-being | Medium | ✅ 6 indicators |
| SDG 4 | Quality Education | Medium | ✅ 3 indicators |
| SDG 5 | Gender Equality | Low | — |
| SDG 6 | Clean Water and Sanitation | High | ✅ 1 indicator |
| SDG 7 | Affordable and Clean Energy | Medium | ✅ 1 indicator |
| SDG 8 | Decent Work and Economic Growth | Low | — |
| SDG 9 | Industry, Innovation and Infrastructure | High | ✅ 3 indicators |
| SDG 10 | Reduced Inequalities | Low | — |
| SDG 11 | Sustainable Cities and Communities | High | ✅ 4 indicators |
| SDG 12 | Responsible Consumption and Production | Low | — |
| SDG 13 | Climate Action | High | ✅ 1 indicator |
| SDG 14 | Life Below Water | Medium | ✅ 1 indicator |
| SDG 15 | Life on Land | High | ✅ 3 indicators |
| SDG 16 | Peace, Justice and Strong Institutions | Low | — |
| SDG 17 | Partnerships for the Goals | Low | — |

---

## Indicator Design Methodology

### Step 1: Indicator feasibility judgment

| Dimension | Question | Pass condition |
|-----------|----------|----------------|
| Spatialization feasibility | Does the indicator involve geospatial information? | Expressible with raster/vector data |
| Data availability | Is the required data obtainable? | Public remote-sensing / statistical / GIS data exists |
| Computation feasibility | Can it be implemented with GDAL raster operations? | The formula can be converted to per-pixel / zonal statistics |
| Normalization feasibility | Can a reasonable threshold be defined? | Supported by academic references or policy standards |

### Step 2: Indicator formula design

Refer to the 4 implemented computation types:

- **Land Proportion (`*`)**: `proportion = target_pixels / total_pixels` → `normalization(proportion, min, max)` → [0, 100]
- **Land Conversion (`**`)**: `rate = converted_pixels / initial_pixels` → positive: `normalization(rate, min, max)`, negative: `normalizationNegative(rate, min, max)` → [0, 100]
- **Buffer Zone (`***`)**: `coverage = covered_target / total_target` → `normalization(coverage, min, max)` → [0, 100]
- **Total Statistics (`****`)**: `ratio = change_A / change_B` → `normalization(ratio, min, max)` with optional best-value triangular normalization → [0, 100]

### Step 3: Data requirement definition

| Data item | Format | Type | Source |
|-----------|--------|------|--------|
| {data name} | GeoTIFF / CSV / vector | GDT_Byte / Float32 | {satellite remote sensing / statistical yearbook / open data} |

### Step 4: Code location

Add the method declaration in `CalculateSDG.h`, implement it in `CalculateSDG.cpp`, and register the sub-command in `main.cpp`. Refer to the implementation patterns of the existing 5 computation commands.

### Input data details for each type

#### Land Proportion (`*`)

| Parameter | Type | Description |
|-----------|------|-------------|
| LUCC raster | GeoTIFF, GDT_Byte | Land-use classification data |
| Selected LUCC types | `unordered_set<int>` | Set of target land-cover codes |
| Max threshold | `double` | Normalization upper bound |
| Min threshold | `double` | Normalization lower bound |

#### Land Conversion (`**`)

| Parameter | Type | Description |
|-----------|------|-------------|
| Initial LUCC | GeoTIFF, GDT_Byte | Initial-period land use |
| Changed LUCC | GeoTIFF, GDT_Byte | Change-period land use |
| Transition types | `unordered_map<int, vector<int>>` | Transition-type mapping |
| Direction flag | `bool` | `true`=positive, `false`=negative |

#### Buffer Zone (`***`)

| Parameter | Type | Description |
|-----------|------|-------------|
| Input data | GeoTIFF, GDT_Byte or Float32 | Land-cover distribution or population distribution |
| Buffer zone data | GeoTIFF, GDT_Byte | Infrastructure coverage-area raster |
| Selected types | `unordered_set<int>` | Target land-cover codes (Byte mode only) |

#### Total Statistics (`****`)

| Parameter | Type | Description |
|-----------|------|-------------|
| Initial/Current LUCC | GeoTIFF, GDT_Byte | Two-period land use |
| Initial/Current Population | GeoTIFF, Float32 | Two-period population data (SDG 11.3.1) |
| Emission scheme | `unordered_map<int, double>` | Per-land-type emission coefficients (SDG 13.2.2) |

---

## Indicator Extension Roadmap

### High priority (spatially feasible + data readily available)

| SDG indicator | Indicator name | Computation type | Relationship to existing modules |
|---------------|----------------|------------------|----------------------------------|
| 2.1.2 | Grain yield | `****` | Extends the 11.3.1/13.2.2 pattern |
| 6.3.2 | Water quality | `*` / `**` | Reuses the Land Proportion pattern |
| 6.4.2 | Water stress | `****` | Requires water-use + water-resource data |

### Medium priority (requires additional data or algorithms)

| SDG indicator | Indicator name | Difficulty |
|---------------|----------------|------------|
| 11.6.2 | Annual mean PM2.5 concentration | Requires atmospheric-pollution remote-sensing data |
| 12.2.2 | Material footprint | Requires input-output tables; hard to spatialize |

### Low priority (hard to spatialize or data scarce)

| SDG indicator | Reason |
|---------------|--------|
| SDG 1.x | Poverty is a socio-economic indicator and hard to spatialize |
| SDG 4.x | Education quality is hard to measure independently with spatial data |
| SDG 5.x | Gender equality is an institutional indicator |
| SDG 8.x | Economic growth depends on statistics, not a spatial issue |

---

## Important Caveats

- New indicators must follow the method style and interface conventions of the existing `CalculateSDG`
- Indicator thresholds must be supported by academic papers or official UN documents
- Land-cover coding systems vary by data source
- **Buffer Zone indicators** can cover multiple indicators by feeding different infrastructure buffer rasters
- **Land Proportion / Land Conversion** can be covered via different land-cover codes and transition-type parameters
