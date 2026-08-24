---
name: sdg-indicator-knowledge
description: SDG 指标体系知识库。当用户需要了解、设计、扩展 SDG 空间化指标时使用此技能。涵盖：联合国 SDG 指标框架（17 目标 + 169 子目标 + 231 指标）、已实现的 GeoSDG 空间指标详情、新指标设计方法论。触发关键词：SDG 指标、SDG 目标、可持续发展目标、新增指标、指标设计、空间化指标、UN SDG、SDG indicator、indicator design、空间化 SDG。
---

# SDG Indicator Knowledge Skill

## Purpose

提供联合国 SDG 指标框架知识，帮助 AI 理解：已实现的指标范围、空间化可行性、指标设计方法论。

## When to Use

- 用户提出新增 SDG 指标需求
- 用户询问某个 SDG 目标/子目标/指标的含义
- 用户需要判断指标是否适合空间化计算
- 用户需要了解 GeoSDG 已支持的指标

## 4 种计算类型速查

| 类型 | 符号 | CLI 命令 | 覆盖 SDG |
|------|------|---------|---------|
| Land Proportion | `*` | `sdg-land-proportion` | 2.1.2 / 6.6.1 / 15.1.1 |
| Land Conversion | `**` | `sdg-land-conversion` | 14.5.1 / 15.2.1 / 15.3.1 |
| Buffer Zone | `***` | `sdg-buffer-zone` | 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1 |
| Total Statistics | `****` | `sdg-1131` / `sdg-1322` | 11.3.1 / 13.2.2 |

## 覆盖统计

- 覆盖 **10 个 SDG、17 个 Target、27 个具体指标**
- 4 种计算类型下 **26/27 已完成**（96%），1 个规划中（SDG 2.1.2 Grain yield）

## References（按需读取，不自动加载）

| 文件 | 何时读取 |
|------|---------|
| `references/sdg-indicators-full.md` | 用户询问特定 SDG 指标时 |
| `references/geosdg-indicators.md` | 需要 GeoSDG 27 个指标详细规范时 |

## Shared（跨 Skill 共享，按需读取）

| 文件 | 何时读取 |
|------|---------|
| `agent/shared/sdg-tool-map.md` | 需要 Tool → SDG 映射和维度分类时 |

## Detail

完整知识库（指标体系详情、设计方法论、拓展路线图）见 `SKILL-detail.md`，按需 `read_file` 加载。
