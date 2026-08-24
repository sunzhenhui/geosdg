---
name: agent-executor
description: GeoSDG Agent tool executor. Receives Tool names and parameters from the Planner, loads the JSON Schema for validation, builds the CLI command, invokes geosdg-cli to execute, and parses the output into structured JSON. Trigger keywords: execute tool, invoke CLI, run calculation, tool call, run command, agent-executor
---

# Agent Executor Skill

## Purpose

Acts as the bridge between the GeoSDG Agent and `geosdg-cli`. Responsibilities: load Tool Schema → validate parameters → build CLI command → execute → parse output → return structured JSON → write to Memory.

## When to Use

- When agent-planner issues a `tool_call` action request
- When the user directly specifies a CLI sub-command to execute
- When Tool parameter validity needs to be checked

## 7-Step Execution Flow

1. Load Tool Schema 2. Validate parameters 3. Build the CLI command (snake_case → kebab-case; see `agent/shared/cli-mapping.md`) **3.5. Safe data copy** (copy input files to `tmp/` and compute on the copies) 4. Execute `geosdg-cli` 5. Parse output (5a success parsing / 5b error-pattern matching / **5c result validation**) 6. Write to Memory in layers **7. Clean up temporary copies**

## Validation Step (Step 5c)

After a computation-type tool succeeds, it automatically runs reasonableness checks, anomaly detection, confidence scoring, and diagnostic repair on the result. See `agent/skills/agent-validator/SKILL.md`.

## Error Pattern Quick Reference

| stderr keyword | Error type |
|----------------|------------|
| `GDALOpen failed` | `file_access_error` |
| `dimension` / `size mismatch` | `dimension_mismatch` |
| `projection` / `CRS` | `projection_mismatch` |
| `NoData` / `nodata` | `nodata_error` |
| `../tmp/` | `temp_dir_missing` |
| `Float32` / `Float64` | `type_mismatch` |
| `stoi` / `stod` | `param_format_error` |
| `copy failed` / `cp:` | `data_copy_error` |

## References (loaded on demand, not auto-loaded)

| File | When to read |
|------|--------------|
| `agent/tools/manifest.json` | When loading the Tool list |
| `agent/tools/<category>/<tool_name>.json` | When loading a specific Tool Schema |

## Data Safety Principle

**Iron rule: the `data/` directory is strictly read-only — no tool may write to it. User-specified data directories are equally protected.**

Before computing, copy input files to `tmp/` → execute on the copies → clean up copies afterwards + restore any polluted directory. See Step 3.5 + Step 7.

**For tools (such as `demo`) that hard-code writes to `data/`**: back up `data/` before execution → restore original files after execution + move output artifacts out.

## Shared (cross-Skill, loaded on demand)

| File | When to read |
|------|--------------|
| `agent/shared/cli-mapping.md` | When building CLI command parameter mappings in Step 3 |
| `agent/shared/data-semantics.md` | When checking region/year before registering data files in Step 6 L3 |

## Detail

For the full execution flow (validation pseudocode, parsing examples, edge cases, error details), see `SKILL-detail_en.md`; load it on demand via `read_file`.
