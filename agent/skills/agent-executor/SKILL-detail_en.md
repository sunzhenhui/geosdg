# Agent Executor Skill — Full Execution Flow

> This file is the complete layer of the agent-executor Skill, loaded on demand by the SKILL.md summary layer.

---

## Prerequisites

- `agent/tools/manifest.json` — global tool index
- `agent/tools/<category>/<tool_name>.json` — JSON Schema definition of the corresponding Tool
- `geosdg-cli` compiled and executable (path: `cli/build/bin/geosdg-cli`)
- `agent/skills/agent-memory/SKILL.md` — layered memory (L0-L5)

**Data semantic metadata constraints**: see `agent/shared/data-semantics.md`

---

## Execution Flow

### Step 1: Load Tool Schema

1. Read `agent/tools/manifest.json` and look up the requested `tool_name` under `categories`
2. If not found → return a `tool_not_found` error
3. Determine the Schema file path from the category: `agent/tools/<category>/<tool_name>.json`
4. Read and parse the JSON Schema

### Step 2: Parameter Validation

| Check | Rule | On failure |
|-------|------|------------|
| Required parameters | Check that all parameters in `parameters.required` are provided | Return `missing_required_param` |
| Type match | Type check `string`/`number`/`integer`/`boolean` | Return `type_mismatch` |
| File existence | For `format: "filepath"` parameters, check the file exists | Return `file_not_found` |
| Extension | Check against the allowed extensions in the `extensions` field | Return `invalid_extension` |

**Validation pseudocode**:
```
for param in schema.parameters.required:
    if param not in user_params:
        return error(missing_required_param, missing=[param])
for param, value in user_params:
    def = schema.parameters.properties[param]
    if def.format == "filepath" and not file_exists(value):
        return error(file_not_found, path=value)
```

### Step 3: Build the CLI Command

Convert JSON parameters to CLI arguments following the parameter-mapping rules in `agent/shared/cli-mapping.md`.

**Conversion rules**:
1. JSON property names use `snake_case` → CLI arguments use `kebab-case` (e.g., `init_lucc` → `--init-lucc`)
2. Special mapping: `output` → `-o`
3. Boolean parameters: `positive: true` → `--positive`; `positive: false` → `--negative`
4. Numeric/string parameters: `--<flag> <value>`

**Build example**:

Input parameters:
```json
{ "ori": "/data/2010.tif", "sim": "/data/Simulation.tif", "real": "/data/2030.tif" }
```

Generated command:
```bash
geosdg-cli ca-precision --ori /data/2010.tif --sim /data/Simulation.tif --real /data/2030.tif
```

### Step 3.5: Data Safety Isolation

> **Iron rule**: the `data/` directory is **strictly read-only** — no tool may write to it. Any user-provided data directory is equally protected.
> All computation runs on `tmp/` copies; original data must not be modified under any circumstances (success / failure / timeout / crash).

#### 3.5a. Data directory protection (pre-check, runs first)

**Check the "no-go zones" before copying**:

```
# No-go zone definition
READONLY_DIRS = [
    "data/",                          # bundled sample data directory
    user_provided_data_dirs,          # any data directory the user specifies
]

# Pre-check: output paths must never point into a no-go zone
for param_name, param_value in user_params:
    if param_name == "output" or param_name.endswith("_output"):
        for readonly_dir in READONLY_DIRS:
            if param_value starts with readonly_dir:
                # Force-redirect the output to a safe location
                safe_output = "tmp/" + basename(param_value)
                LOG_WARN("Output redirected: " + param_value + " -> " + safe_output)
                user_params[param_name] = safe_output
```

**Special handling for tools that hard-code writes to `data/`** (e.g., `demo`):

When a Tool Schema is marked `data_safety.outputs_pollute_data: true`:
1. **Back up**: `cp -r data/ tmp/data_backup/`
2. **Execute**: run the CLI command normally
3. **Restore**: after execution (whether success or failure):
   ```
   # Move demo output files out (without losing computation results)
   mv data/PriorityAreas-*.tif tmp/  2>/dev/null
   mv data/PriorityAreasRankingMap.tif tmp/  2>/dev/null

   # Verify original data integrity (compare against backup)
   for original_file in data_backup/*.tif:
       if hash(original_file) != hash(data/{basename(original_file)}):
           cp data_backup/{basename} data/{basename}
           LOG_ERROR("Restored damaged file: " + basename)

   # Clean up the backup
   rm -rf tmp/data_backup/
   ```

#### 3.5b. Confirm the tmp isolation directory

```
workspace_root = user's workspace root directory
tmp_dir = workspace_root + "/tmp/"

if not exists(tmp_dir):
    mkdir -p tmp_dir
    LOG_INFO("Created tmp/ directory for data safety isolation")
```

For `priority-emission` (Rule 5), additionally confirm that `geosdg-cli` can find `../tmp/`:
```
cli_tmp_dir = geosdg_cli_build_dir + "/../tmp/"
if not exists(cli_tmp_dir):
    mkdir -p cli_tmp_dir
```

#### 3.5c. Identify input files and perform the copy

Scan all `format: "filepath"` parameters:

| Parameter role | How to identify | Copy? |
|----------------|-----------------|-------|
| Input file | `format: "filepath"` and not an `output` parameter | ✅ Must copy |
| Output file | Parameter named `output` / `-o` | ❌ No copy (but redirect to a safe location) |

**For tools without explicit filepath parameters (such as `demo`)**:
- Check the `input_constraints.data_files` field of the Tool Schema
- Locate the data directory from the description and copy the whole directory to `tmp/`
- Adjust the working directory (`cd` into `tmp/`) so that `../data/` points to the copy

```bash
# demo special handling: copy all input data into tmp/
cp -r data/LUCC tmp/LUCC
cp -r data/POPU tmp/POPU
cp -r data/INFRA tmp/INFRA

# Then run the CLI in tmp/'s parent directory so that ../data -> tmp/
# i.e., cd tmp/.. && geosdg-cli demo
# Now the CLI's ../data/ actually points to the copies in tmp/
```

```bash
# Copy each explicit input file
for input_param in input_file_params:
    original_path = user_params[input_param]
    basename = os.path.basename(original_path)
    tmp_path = "tmp/" + basename

    if already_copied[basename]:
        tmp_path = "tmp/" + unique_prefix + "_" + basename

    cp original_path tmp_path
    param_mapping[input_param] = tmp_path
```

#### 3.5d. Replace CLI arguments and run path

Replace all input-file paths in the CLI command built in Step 3 with paths under `tmp/`:

```
# Before
geosdg-cli sdg-land-conversion --init-lucc /data/hangzhou/lucc_2010.tif --curr-lucc /data/hangzhou/lucc_2030.tif

# After
geosdg-cli sdg-land-conversion --init-lucc tmp/lucc_2010.tif --curr-lucc tmp/lucc_2030.tif
```

**For the demo tool**: run in `tmp/`'s parent directory so the CLI's `../data/` points to the copy:
```
cd tmp/..  &&  geosdg-cli demo   # ../data/ resolves to tmp/../data -> data/
# OR: use a symlink
# ln -sf tmp data_symlink  &&  cd data_symlink/..  &&  geosdg-cli demo
```

#### 3.5e. Record the copy mapping (for Step 7 cleanup and restore)

```
copy_registry = {
    "tmp/lucc_2010.tif": "/data/hangzhou/lucc_2010.tif",
    "tmp/lucc_2030.tif": "/data/hangzhou/lucc_2030.tif"
}

# demo special: record the data backup
demo_backup = "tmp/data_backup/"
```

#### 3.5f. Output path safety check (post-check)

```
# Ensure all output paths do not point into a protected directory
for output_path in all_output_paths:
    if output_path starts_with "data/" or output_path starts_with user_data_dir:
        LOG_ERROR("Output path violates data protection: " + output_path)
        # Redirect the output
        safe_path = "tmp/" + basename(output_path)
        LOG_WARN("Redirecting output to: " + safe_path)
        output_path = safe_path
```

#### Special tool handling

| Tool | Handling |
|------|----------|
| `demo` | Hard-codes writes to `../data/PriorityAreas-*.tif`. Handle by: back up the whole `data/` → `tmp/data_backup/`, then restore original files after execution and move output artifacts out. Or copy the whole `data/` to `tmp/` and run in the isolated environment |
| `priority-emission` | Copy input files to `tmp/`; ensure the `cli/build/../tmp/` directory exists |
| `sdg-1322` | The C++ layer already protects via a map copy; the Agent layer isolates the files |
| `sdg-land-proportion` | Input-file copy is mandatory |
| `check` | Pure read-only tool, but still copies to keep the flow uniform |

#### Data safety markers

| data_safety.copy_inputs | Meaning | Behavior |
|-------------------------|---------|----------|
| `"required"` | Known to modify input files | Force copy + log warning |
| `"mandatory"` | Tool hard-codes writes to `data/`, needs backup-and-restore | Back up data/ → execute → restore + move artifacts out |
| undefined | No known risk, but as a safety convention | Copy by default |

That is: **all tools involving file operations must run in a `tmp/`-isolated environment**.

#### Pre-execution checklist

Before running the CLI in Step 4, confirm all of the following pass:

- [ ] `data/` original files are backed up or confirmed not to be written
- [ ] All input-file paths replaced with `tmp/` copy paths
- [ ] All output paths do not point into `data/` or a user data directory
- [ ] For demo: `data/` backed up to `tmp/data_backup/`
- [ ] `tmp/` directory exists and has enough disk space (> 2x input data size)

### Step 4: Execute the Command

Execute the built CLI command using the `execute_command` tool:

```
execute_command(command="<built full command>", requires_approval=true)
```

**Timeout handling**: each Tool's Schema defines `timeout_seconds` (default 300 seconds). For large files (>1GB), estimate the duration before execution; if it is expected to exceed 120 seconds, ask the user first.

**Capture output**: `stdout` (normal output), `stderr` (error information), `exit_code` (0 = success, non-zero = failure)

### Step 5: Parse Output

#### 5a. Success parsing (exit_code == 0)

**Numeric output** (ca-precision / correlation / t-test / sdg-* commands):

CLI stdout format: `key=value` (space-separated); parse regex: `(\w+)=([-\d.]+)`

```json
{
  "tool": "ca-precision",
  "status": "success",
  "result": { "FoM": 0.32, "PA": 0.71, "UA": 0.68, "Kappa": 0.76, "OA": 0.87 },
  "raw_stdout": "FoM=0.320000 PA=0.710000 ...",
  "exit_code": 0
}
```

**Special parsing**:
- `sdg-land-conversion` outputs `score(positive)=72.3` or `score(negative)=28.5`
- `correlation` outputs `R=0.456789`
- `t-test` outputs `t=2.345678`
- `sdg-*` (except conversion) outputs `score=72.3`

**File output** (priority-* / priority-merge commands):

CLI stdout format: `Output: <path>`

```json
{
  "tool": "priority-loss",
  "status": "success",
  "result": { "output_file": "/data/PriorityAreas-1.tif", "status": "success" },
  "exit_code": 0
}
```

#### 5b. Failure parsing (exit_code != 0)

| stderr keyword | Error type | User hint |
|----------------|------------|-----------|
| `GDALOpen failed` / `Cannot open` | `file_access_error` | File missing or unreadable; check the path |
| `dimension` / `size mismatch` | `dimension_mismatch` | Input files have inconsistent row/column counts |
| `projection` / `CRS` | `projection_mismatch` | Projection/CRS mismatch |
| `NoData` / `nodata` | `nodata_error` | Invalid values present in the data |
| `../tmp/` / `tmp directory` | `temp_dir_missing` | The `../tmp/` temporary directory is missing |
| `Float32` / `Float64` | `type_mismatch` | Rule 6 only supports Float32 |
| `stoi` / `stod` | `param_format_error` | Parameter format error |

#### 5c. Validate the result (computation-type tools only)

> **New step**: inserted after Step 5b and before Step 6. Runs reasonableness checks, anomaly detection, confidence scoring, and automatic diagnostic repair on the computation result.

**Tools that skip validation**: `demo`, `help`, `check`, `priority-stats` — non-computation tools.

**Execution condition**: `result.status == "success" AND tool in [sdg-*, ca-precision, correlation, t-test, priority-loss, priority-transition, priority-buffer, priority-emission, priority-human-land, priority-merge]`

**Validation pseudocode**:
```
// 1. Load validation rules
rules = loadValidationRules(tool)  // from agent/validation/rules/<tool>.json

// 2. Run the valid-range check
range_check = checkValidRange(result.score, rules.valid_range)

// 3. Run anomaly-pattern matching
anomaly = matchAnomalyPatterns(result.score, result.metadata, rules.anomaly_patterns)

// 4. Load historical data for comparison
history = loadHistoryFromMemory(tool, region)  // read from L1

// 5. Compute confidence
confidence = calcConfidence(dataQuality, range_check, history, paramCompleteness)
// weights from agent/validation/confidence-policy.json

// 6. Determine the final status
if range_check == FAIL:
    status = "FAIL"
    if canAutoFix(failureReason):
        attemptAutoFixAndRetry()  // up to 3 attempts
elif anomaly.severity == "WARN" OR history_bias > threshold:
    status = "WARN"
    diagnosis = generateDiagnosis(anomaly, history)
    // diagnosis patterns from agent/validation/diagnosis-patterns.json
else:
    status = "PASS"

// 7. Attach to the result
result.validation = { status, confidence, checks, diagnosis, suggestions }
```

**Validation result attached to the output JSON**:
```json
{
  "tool": "sdg-land-proportion",
  "status": "success",
  "result": {
    "score": 78.3,
    "validation": {
      "status": "WARN",
      "confidence": 0.82,
      "checks": {
        "range": { "status": "PASS", "detail": "78.3 ∈ [0, 100]" },
        "anomaly": { "status": "WARN", "detail": "Score on the high side, types proportion on the low side", "suggestion": "Check the --types parameter" },
        "history": { "status": "WARN", "detail": "Bias +15% vs last run 68.0" },
        "params": { "status": "PASS", "detail": "All required parameters provided" }
      },
      "diagnosis": "This score is 15% higher than the historical assessment, possibly due to the extra inclusion of land class 3. Recommend confirming the --types parameter.",
      "auto_fix_attempted": false
    }
  }
}
```

**Validation skill reference**: see `agent/skills/agent-validator/SKILL-detail.md`

### Step 6: Return the Result + Layered Memory Write

1. Return the structured result JSON to agent-planner
2. Call agent-memory for the layered write:

#### L0 — Update working memory
```
updateWorkingMemory({ current_step, active_task_id, last_result_ref, status })
```

#### L1 — Create/update task record
- Before execution: `taskId = createTask({ intent, query, tool, params, session_id })`
- After execution: `updateTask(taskId, { status, result, log_ref })`

#### L2 — Generate an assessment report (computation-type tools, success only)
```
reportPath = generateReport(taskId, logPath, indicatorKnowledge)
```

#### L3 — Register data files (file-type parameters only)
See the interaction flow in `agent/shared/data-semantics.md`.

#### L5 — Update user preferences (first use only)
```
updatePreferences({ default_params, focus_regions })
```

### Step 7: Clean Up Temporary Copies + Data Integrity Verification

**Purpose**: after computation, delete the `tmp/` temporary copies, restore any `data/` directory polluted by the tool, and ensure the original data is untouched.

**When to run**:
- Step 4 succeeded (exit_code == 0) → clean up immediately
- Step 4 failed (exit_code != 0) → clean up immediately
- Step 4 timed out / interrupted → clean up immediately

**Cleanup flow**:

```
# 7a. Clean up input-file copies
for tmp_path in copy_registry:
    if exists(tmp_path):
        rm tmp_path
        LOG_DEBUG("Cleaned up temp copy: " + tmp_path)

# 7b. demo special handling: restore the data/ directory
if tool == "demo" and demo_backup:
    # Move demo output artifacts out (without losing results)
    mv data/PriorityAreas-*.tif tmp/  2>/dev/null
    mv data/PriorityAreasRankingMap.tif tmp/  2>/dev/null

    # Verify original data integrity
    for backup_file in tmp/data_backup/**/*.tif:
        original = backup_file.replace("tmp/data_backup/", "data/")
        if exists(original) and hash(backup_file) != hash(original):
            cp backup_file original  # restore the modified original file
            LOG_ERROR("Restored damaged file from backup: " + original)
        elif not exists(original):
            LOG_WARN("Original file missing after demo, restoring from backup: " + original)
            cp backup_file original

    # Clean up the backup
    rm -rf tmp/data_backup/
    LOG_INFO("data/ directory integrity verified and restored")

# 7c. Remove demo output files written into data/
for pattern in ["data/PriorityAreas-*.tif", "data/PriorityAreasRankingMap.tif"]:
    rm -f pattern  2>/dev/null

# 7d. If the tmp/ directory is empty, remove it
if tmp_dir_is_empty("tmp/"):
    rmdir "tmp/"
```

**Cleanup verification**:
```
# Final check: the data/ directory must not contain any demo output artifacts
for path in ["data/PriorityAreas-1.tif", ..., "data/PriorityAreasRankingMap.tif"]:
    if exists(path):
        LOG_ERROR("FAILED TO CLEAN: " + path + " still exists in data/!")
        rm -f path  # last-resort delete

# Verify the data/ directory is clean
for original_file in original_data_files:
    if hash(original_file) != hash_before_execution:
        LOG_CRITICAL("DATA CORRUPTION DETECTED: " + original_file)
        restore_from_backup(original_file)
```

**Notes**:
- `priority-emission`'s intermediate files (`../tmp/OriginalPrefix.tif`, etc.) are managed internally by geosdg-cli
- If multiple Tools run serially in the same session, each Step 3.5 is independent; Step 7 only cleans up the current one
- If a system crash prevents cleanup → auto-detect and clean up stale `tmp/` files at the start of the next conversation
- **After demo runs, `PriorityAreas-*.tif` must absolutely not appear under `data/`**

---

## Error Handling

### Tool not found
```json
{
  "error": "tool_not_found",
  "message": "Tool '{name}' does not exist. See agent/tools/manifest.json for the available tool list",
  "available_categories": ["ca-precision", "sdg-calc", "priority-area", "demo-help"]
}
```

### Missing required parameter
```json
{
  "error": "missing_required_param",
  "tool": "ca-precision",
  "missing": ["ori", "sim"],
  "message": "Tool 'ca-precision' is missing required parameters: ori, sim"
}
```

### File not found
```json
{
  "error": "file_not_found",
  "tool": "ca-precision",
  "param": "ori",
  "path": "/data/2010.tif",
  "message": "File does not exist or is unreadable: /data/2010.tif"
}
```

---

## Edge-Case Handling

| Scenario | Handling |
|----------|----------|
| Path contains spaces | Wrap the path value in quotes |
| Large file (>1GB) | Tell the user the estimated duration and copy time before execution, ask whether to continue |
| CLI executable not found | Prompt the user to compile geosdg-cli first |
| stdout empty but exit_code=0 | Return `{"status": "success", "result": {}, "warning": "CLI produced no output"}` |
| Parameter value contains special characters | Escape or wrap in quotes |
| Multiple Tools run in parallel | Each Tool is an independent call with its own Step 3.5 copy + Step 7 cleanup |
| Rule 5 missing `../tmp/` | Create the directory automatically |
| tmp/ out of disk space | Check available space before copying (need > 2x input file size); refuse to run if insufficient |
| Same-name file conflict | Add a source-directory prefix to avoid overwrite (e.g., `tmp/data1_lucc_2010.tif`) |
| Copy failure | Return `data_copy_error`; do not run subsequent computation |
| **demo hard-codes writes into data/** | Back up data/ → execute → restore original files + move output artifacts out |
| **Output path points into data/ or a user data directory** | Force-redirect the output to `tmp/`, log a warning |
| **Tool has no filepath parameter but internally accesses data/** | Check `input_constraints.data_files`; perform a full data/ backup-restore |

---

## Important Notes

- This Skill **does not modify any C++ code**; it only invokes the compiled `geosdg-cli` via `execute_command`
- **The `data/` directory is a no-go zone**: no tool may write into `data/`. Tools that hard-code writes (such as demo) are protected via the backup-restore mechanism
- **Data safety is the top priority**: Step 3.5 enforces isolation (backup + copy + path replacement), Step 7 cleans up + restores + verifies integrity
- **User data directories are equally protected**: the directory containing any data-file path passed by the user is treated as read-only
- The `tmp/` directory lives at the workspace root and is a different concept from the `../tmp/` used internally by `priority-emission` (relative to `cli/`)
- `priority-emission` (Rule 5): input files are copied to the workspace `tmp/`; the CLI's internal intermediate files are written to `cli/build/../tmp/`, managed by the CLI itself
- Prefer absolute paths for all file paths
- `priority-human-land` (Rule 6) only supports `GDT_Float32` population data
- **Data semantic metadata (region/year) must be provided by the user**: see `agent/shared/data-semantics.md`
- **Step 3.5 + Step 7 guarantee**: original data is never modified and no output files are left behind under any circumstances (success / failure / timeout / system crash)
