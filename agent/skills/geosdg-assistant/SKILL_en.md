---
name: geosdg-assistant
description: GeoSDG (spatial Sustainable Development Goals assessment) project assistant. Use this skill when the user needs to understand, use, debug, or extend the GeoSDG project. Applicable scenarios include: CA simulation accuracy assessment, spatial SDG indicator computation, priority-area identification, project source-code interpretation, operational workflow guidance, and troubleshooting. Trigger keywords: GeoSDG, spatial SDG, land-use simulation, CA accuracy, FoM/Kappa, priority area, carbon emission peaking, human-environment relationship, SDG 2/6/11/13/15, etc.
---

# GeoSDG Assistant Skill

## Purpose

Provides expert knowledge of the GeoSDG project — a C++17/CMake/GDAL CLI tool. > Targets the CLI version only (`geosdg-cli/`); the legacy Qt UI is deprecated.

## When to Use

- When the user asks about the GeoSDG project structure, code, or features
- When the user needs to compute a spatial SDG indicator or CA accuracy assessment
- When the user encounters errors or unexpected results and needs troubleshooting
- When the user needs data preparation, format requirements, or build guidance

## CLI Commands Quick Reference

| Sub-command | Module | Key Parameters |
|-------------|--------|----------------|
| `ca-precision` | CalculateCAPrecision | `--ori` `--sim` `--real` |
| `correlation` | CalculateCAPrecision | `--data1` `--data2` |
| `t-test` | CalculateCAPrecision | `--data1` `--data2` |
| `sdg-land-proportion` | CalculateSDG | `--init-lucc` `--types` `--max` `--min` |
| `sdg-land-conversion` | CalculateSDG | `--init-lucc` `--curr-lucc` `--transitions` `--positive/--negative` |
| `sdg-buffer-zone` | CalculateSDG | `--init-lucc` `--buffer` `--types` `--max` `--min` |
| `sdg-1131` | CalculateSDG | `--init-lucc` `--curr-lucc` `--init-popu` `--curr-popu` `--types` `--best` |
| `sdg-1322` | CalculateSDG | `--init-lucc` `--curr-lucc` `--emission` `--ratio` |
| `priority-loss` | ExtractPriorityAreas | `--init-lucc` `--curr-lucc` `-o` `--types` |
| `priority-transition` | ExtractPriorityAreas | `--init-lucc` `--curr-lucc` `-o` `--transitions` |
| `priority-buffer` | ExtractPriorityAreas | `--init-lucc` `--buffer` `-o` `--types` `--pop-threshold` |
| `priority-emission` | ExtractPriorityAreas | `--init-lucc` `--curr-lucc` `-o` `--emission` `--ratio` `--radius` |
| `priority-human-land` | ExtractPriorityAreas | `--init-lucc` `--curr-lucc` `--init-popu` `--curr-popu` `-o` `--types` `--radius` |
| `priority-merge` | ExtractPriorityAreas | `--files` `-o` |
| `demo` | All | `--resume` |

## References (loaded on demand, not auto-loaded)

| File | When to read |
|------|--------------|
| `references/GeoSDG-Project-Structure-Quick-Reference_en.md` | When a function → file mapping and issue index are needed |
| `references/GeoSDG-Operation-Troubleshooting-Quick-Reference_en.md` | When the user encounters an error and needs troubleshooting |
| `assets/source-code/` | When the source code needs to be inspected |

## Detail

For the full reference material (architecture, function mapping, build guide, data requirements, troubleshooting quick reference, Logger module, important caveats), see `SKILL-detail_en.md`; load it on demand via `read_file`.
