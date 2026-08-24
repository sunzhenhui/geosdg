---
name: agent-executor
description: GeoSDG Agent 工具执行器。接收 Planner 发来的 Tool 名称和参数，加载 JSON Schema 校验，构建 CLI 命令，调用 geosdg-cli 执行，解析输出为结构化 JSON。触发关键词：执行工具、调用CLI、运行计算、tool call、执行命令、agent-executor
---

# Agent Executor Skill

## Purpose

作为 GeoSDG Agent 和 `geosdg-cli` 之间的桥梁。负责：加载 Tool Schema → 校验参数 → 构建 CLI 命令 → 执行 → 解析输出 → 返回结构化 JSON → 写入 Memory。

## When to Use

- agent-planner 发出 `tool_call` 动作请求时
- 用户直接指定要执行某个 CLI 子命令时
- 需要校验 Tool 参数合法性时

## 7 步执行流程

1. 加载 Tool Schema 2.参数校验 3.构建 CLI 命令（snake_case→kebab-case，详见 `agent/shared/cli-mapping.md`） **3.5.数据安全拷贝**（将输入文件 cp 到 `tmp/`，使用副本计算） 4.执行 `geosdg-cli` 5.解析输出（5a 成功解析 / 5b 错误模式匹配 / **5c 验证结果**） 6.分层写入 Memory **7.清理临时副本**

## 验证步骤（Step 5c）

计算类工具执行成功后，自动对结果执行合理性校验、异常检测、置信度评分和诊断修复。详见 `agent/skills/agent-validator/SKILL.md`。

## 错误模式速查

| stderr 关键词 | 错误类型 |
|--------------|---------|
| `GDALOpen failed` | `file_access_error` |
| `dimension` / `size mismatch` | `dimension_mismatch` |
| `projection` / `CRS` | `projection_mismatch` |
| `NoData` / `nodata` | `nodata_error` |
| `../tmp/` | `temp_dir_missing` |
| `Float32` / `Float64` | `type_mismatch` |
| `stoi` / `stod` | `param_format_error` |
| `copy failed` / `cp:` | `data_copy_error` |

## References（按需读取，不自动加载）

| 文件 | 何时读取 |
|------|---------|
| `agent/tools/manifest.json` | 加载 Tool 列表时 |
| `agent/tools/<category>/<tool_name>.json` | 加载特定 Tool Schema 时 |

## 数据安全原则

**铁律：`data/` 目录绝对只读，任何工具不得写入。用户指定的数据目录同样受保护。**

计算前将输入文件拷贝到 `tmp/` → 使用副本执行 → 完成后清理副本 + 恢复被污染的目录。详见 Step 3.5 + Step 7。

**对于 demo 等硬编码写入 `data/` 的工具**：执行前备份 data/ → 执行后恢复原始文件 + 移出输出产物。

## Shared（跨 Skill 共享，按需读取）

| 文件 | 何时读取 |
|------|---------|
| `agent/shared/cli-mapping.md` | Step 3 构建 CLI 命令参数映射时 |
| `agent/shared/data-semantics.md` | Step 6 L3 注册数据文件前检查 region/year 时 |

## Detail

完整执行流程（校验伪代码、解析示例、边界情况、错误详情）见 `SKILL-detail.md`，按需 `read_file` 加载。
