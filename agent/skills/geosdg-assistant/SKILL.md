---
name: geosdg-assistant
description: GeoSDG（空间化可持续发展目标评估）项目助手。当用户需要理解、使用、调试或扩展GeoSDG项目时使用此技能。适用场景包括：CA模拟精度评估、空间SDG指标计算、优先区域识别、项目代码解读、操作流程指导、问题排查等。触发关键词：GeoSDG、空间SDG、土地利用模拟、CA精度、FoM/Kappa、优先区域、碳排放达峰、人地关系、SDG 2/6/11/13/15等。
---

# GeoSDG Assistant Skill

## Purpose

提供 GeoSDG 项目的专业知识 — C++17/CMake/GDAL CLI 工具。> 仅针对 CLI 版本（`geosdg-cli/`），旧 Qt UI 已废弃。

## When to Use

- 用户询问 GeoSDG 项目结构、代码或功能
- 用户需要计算空间 SDG 指标或 CA 精度评估
- 用户遇到错误或意外结果时需要排查
- 用户需要数据准备、格式要求或构建指导

## CLI Commands Quick Reference

| Sub-command | Module | Key Parameters |
|-------------|--------|---------------|
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

## References（按需读取，不自动加载）

| 文件 | 何时读取 |
|------|---------|
| `references/GeoSDG-项目结构速查表.md` | 需要函数→文件映射和问题索引时 |
| `references/GeoSDG-操作问题速查表.md` | 用户遇到错误需要排查时 |
| `assets/source-code/` | 需要查看源码时 |

## Detail

完整参考资料（架构、函数映射、构建指南、数据要求、故障排查速查、Logger 模块、Important Caveats）见 `SKILL-detail.md`，按需 `read_file` 加载。
