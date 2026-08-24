---
name: sdg-indicator-knowledge
description: SDG indicator framework knowledge base. Use this skill when the user needs to understand, design, or extend spatial SDG indicators. Covers: the UN SDG indicator framework (17 goals + 169 targets + 231 indicators), details of implemented GeoSDG spatial indicators, and the methodology for designing new indicators. Trigger keywords: SDG indicator, SDG goal, Sustainable Development Goals, new indicator, indicator design, spatial indicator, UN SDG, SDG indicator, indicator design, spatial SDG.
---

# SDG Indicator Knowledge Skill

## Purpose

Provides knowledge of the UN SDG indicator framework to help the AI understand: the scope of implemented indicators, spatialization feasibility, and indicator design methodology.

## When to Use

- When the user requests a new SDG indicator
- When the user asks about the meaning of a specific SDG goal / target / indicator
- When the user needs to judge whether an indicator is suitable for spatialized computation
- When the user needs to know which indicators GeoSDG already supports

## 4 Computation Types Quick Reference

| Type | Symbol | CLI command | Covered SDGs |
|------|--------|-------------|--------------|
| Land Proportion | `*` | `sdg-land-proportion` | 2.1.2 / 6.6.1 / 15.1.1 |
| Land Conversion | `**` | `sdg-land-conversion` | 14.5.1 / 15.2.1 / 15.3.1 |
| Buffer Zone | `***` | `sdg-buffer-zone` | 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1 |
| Total Statistics | `****` | `sdg-1131` / `sdg-1322` | 11.3.1 / 13.2.2 |

## Coverage Statistics

- Covers **10 SDGs, 17 targets, and 27 specific indicators**
- Under the 4 computation types, **26/27 are complete** (96%), with 1 in planning (SDG 2.1.2 Grain yield)

## References (loaded on demand, not auto-loaded)

| File | When to read |
|------|--------------|
| `references/sdg-indicators-full_en.md` | When the user asks about a specific SDG indicator |
| `references/geosdg-indicators_en.md` | When the detailed specification of GeoSDG's 27 indicators is needed |

## Shared (cross-Skill, loaded on demand)

| File | When to read |
|------|--------------|
| `agent/shared/sdg-tool-map.md` | When the Tool → SDG mapping and dimension classification are needed |

## Detail

For the full knowledge base (indicator framework details, design methodology, extension roadmap), see `SKILL-detail_en.md`; load it on demand via `read_file`.
