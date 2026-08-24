# Agent Executor Skill — 完整执行流程

> 本文件为 agent-executor Skill 的完整层，由 SKILL.md 摘要层按需加载。

---

## Prerequisites

- `agent/tools/manifest.json` — 全局工具索引
- `agent/tools/<category>/<tool_name>.json` — 对应 Tool 的 JSON Schema 定义
- `geosdg-cli` 已编译且可执行（路径在 `cli/build/bin/geosdg-cli`）
- `agent/skills/agent-memory/SKILL.md` — 分层记忆（L0-L5）

**数据语义元信息约束**：详见 `agent/shared/data-semantics.md`

---

## 执行流程

### Step 1：加载 Tool Schema

1. 读取 `agent/tools/manifest.json`，在 `categories` 中查找请求的 `tool_name`
2. 如果找不到 → 返回 `tool_not_found` 错误
3. 根据分类确定 Schema 文件路径：`agent/tools/<category>/<tool_name>.json`
4. 读取并解析 JSON Schema

### Step 2：参数校验

| 校验项 | 规则 | 失败处理 |
|--------|------|---------|
| 必填参数 | 检查 `parameters.required` 数组中所有参数是否已提供 | 返回 `missing_required_param` |
| 类型匹配 | `string`/`number`/`integer`/`boolean` 类型检查 | 返回 `type_mismatch` |
| 文件存在性 | `format: "filepath"` 的参数，检查文件是否存在 | 返回 `file_not_found` |
| 扩展名 | `extensions` 字段定义的允许扩展名 | 返回 `invalid_extension` |

**校验伪代码**：
```
for param in schema.parameters.required:
    if param not in user_params:
        return error(missing_required_param, missing=[param])
for param, value in user_params:
    def = schema.parameters.properties[param]
    if def.format == "filepath" and not file_exists(value):
        return error(file_not_found, path=value)
```

### Step 3：构建 CLI 命令

按 `agent/shared/cli-mapping.md` 的参数映射规则，将 JSON 参数转换为 CLI 参数。

**转换规则**：
1. JSON 属性名 `snake_case` → CLI 参数 `kebab-case`（如 `init_lucc` → `--init-lucc`）
2. 特殊映射：`output` → `-o`
3. 布尔参数：`positive: true` → `--positive`；`positive: false` → `--negative`
4. 数值/字符串参数：`--<flag> <value>`

**构建示例**：

输入参数：
```json
{ "ori": "/data/2010.tif", "sim": "/data/Simulation.tif", "real": "/data/2030.tif" }
```

生成命令：
```bash
geosdg-cli ca-precision --ori /data/2010.tif --sim /data/Simulation.tif --real /data/2030.tif
```

### Step 3.5：数据安全隔离（Data Safety Isolation）

> **铁律**：`data/` 目录**绝对只读**，任何工具不得写入。用户提供的任何数据目录同样受保护。
> 所有计算使用 `tmp/` 副本执行，原始数据在任何情况下（成功/失败/超时/崩溃）都不可修改。

#### 3.5a. 数据目录保护（Pre-check，最先执行）

**在拷贝之前，先检查"禁区"**：

```
# 禁区定义
READONLY_DIRS = [
    "data/",                          # 项目内置样例数据目录
    user_provided_data_dirs,          # 用户指定的任何数据目录
]

# 预检：输出路径绝不允许指向禁区
for param_name, param_value in user_params:
    if param_name == "output" or param_name.endswith("_output"):
        for readonly_dir in READONLY_DIRS:
            if param_value starts with readonly_dir:
                # 强制重定向输出到安全位置
                safe_output = "tmp/" + basename(param_value)
                LOG_WARN("Output redirected: " + param_value + " -> " + safe_output)
                user_params[param_name] = safe_output
```

**工具硬编码输出到 `data/` 的特殊处理**（如 `demo`）：

当 Tool Schema 标记了 `data_safety.outputs_pollute_data: true`：
1. **备份**：`cp -r data/ tmp/data_backup/`
2. **执行**：正常执行 CLI 命令
3. **恢复**：执行完成后（无论成功失败）：
   ```
   # 移出 demo 输出文件（不丢失计算结果）
   mv data/PriorityAreas-*.tif tmp/  2>/dev/null
   mv data/PriorityAreasRankingMap.tif tmp/  2>/dev/null
   
   # 检查原始数据完整性（对比备份）
   for original_file in data_backup/*.tif:
       if hash(original_file) != hash(data/{basename(original_file)}):
           cp data_backup/{basename} data/{basename}
           LOG_ERROR("Restored damaged file: " + basename)
   
   # 清理备份
   rm -rf tmp/data_backup/
   ```

#### 3.5b. 确认 tmp 隔离目录

```
workspace_root = 用户工作区根目录
tmp_dir = workspace_root + "/tmp/"

if not exists(tmp_dir):
    mkdir -p tmp_dir
    LOG_INFO("Created tmp/ directory for data safety isolation")
```

对于 `priority-emission`（Rule 5），还需额外确认 `geosdg-cli` 能找到 `../tmp/`：
```
cli_tmp_dir = geosdg_cli_build_dir + "/../tmp/"
if not exists(cli_tmp_dir):
    mkdir -p cli_tmp_dir
```

#### 3.5c. 识别输入文件并执行拷贝

扫描所有 `format: "filepath"` 的参数：

| 参数角色 | 识别方式 | 是否拷贝 |
|---------|---------|---------|
| 输入文件 | `format: "filepath"` 且非 `output` 参数 | ✅ 必须拷贝 |
| 输出文件 | 参数名为 `output` / `-o` | ❌ 不拷贝（但重定向到安全位置） |

**对于 `demo` 等无显式 filepath 参数的工具**：
- 检查 Tool Schema 的 `input_constraints.data_files` 字段
- 按描述找到数据目录，将整个数据目录拷贝到 `tmp/`
- 调整工作目录（`cd` 到 `tmp/`）使 `../data/` 指向副本

```bash
# demo 特殊处理：将所有输入数据拷贝到 tmp/
cp -r data/LUCC tmp/LUCC
cp -r data/POPU tmp/POPU
cp -r data/INFRA tmp/INFRA

# 然后在 tmp/ 的父目录执行 CLI，使 ../data -> tmp/
# 即：cd tmp/.. && geosdg-cli demo
# 此时 CLI 的 ../data/ 实际指向 tmp/ 中的副本
```

```bash
# 对每个显式输入文件执行拷贝
for input_param in input_file_params:
    original_path = user_params[input_param]
    basename = os.path.basename(original_path)
    tmp_path = "tmp/" + basename
    
    if already_copied[basename]:
        tmp_path = "tmp/" + unique_prefix + "_" + basename
    
    cp original_path tmp_path
    param_mapping[input_param] = tmp_path
```

#### 3.5d. 替换 CLI 参数和运行路径

将 Step 3 构建的 CLI 命令中所有输入文件路径替换为 `tmp/` 下路径：

```
# 替换前
geosdg-cli sdg-land-conversion --init-lucc /data/hangzhou/lucc_2010.tif --curr-lucc /data/hangzhou/lucc_2030.tif

# 替换后
geosdg-cli sdg-land-conversion --init-lucc tmp/lucc_2010.tif --curr-lucc tmp/lucc_2030.tif
```

**对于 demo 工具**：在 `tmp/` 的上级目录执行，使 CLI 的 `../data/` 指向副本：
```
cd tmp/..  &&  geosdg-cli demo   # ../data/ 实际解析为 tmp/../data -> data/
# OR: 使用符号链接
# ln -sf tmp data_symlink  &&  cd data_symlink/..  &&  geosdg-cli demo
```

#### 3.5e. 记录拷贝映射（供 Step 7 清理和恢复）

```
copy_registry = {
    "tmp/lucc_2010.tif": "/data/hangzhou/lucc_2010.tif",
    "tmp/lucc_2030.tif": "/data/hangzhou/lucc_2030.tif"
}

# demo 特殊：记录数据备份
demo_backup = "tmp/data_backup/"
```

#### 3.5f. 输出路径安全检查（Post-check）

```
# 确保所有输出路径不指向受保护目录
for output_path in all_output_paths:
    if output_path starts_with "data/" or output_path starts_with user_data_dir:
        LOG_ERROR("Output path violates data protection: " + output_path)
        # 重定向输出
        safe_path = "tmp/" + basename(output_path)
        LOG_WARN("Redirecting output to: " + safe_path)
        output_path = safe_path
```

#### 特殊工具处理

| 工具 | 处理方式 |
|------|---------|
| `demo` | 硬编码写入 `../data/PriorityAreas-*.tif`。处理：先备份整个 `data/` → `tmp/data_backup/`，执行后恢复原始文件并移出输出产物。或者将整个 `data/` 拷贝到 `tmp/` 并在隔离环境中执行 |
| `priority-emission` | 输入文件拷贝到 `tmp/`；确保 `cli/build/../tmp/` 目录存在 |
| `sdg-1322` | C++ 层已做 map 副本保护，Agent 层做文件隔离 |
| `sdg-land-proportion` | 输入文件拷贝强制必须执行 |
| `check` | 纯只读工具，但仍执行拷贝以统一流程 |

#### 数据安全标记

| data_safety.copy_inputs | 含义 | 行为 |
|------------------------|------|------|
| `"required"` | 已知会修改输入文件 | 强制拷贝 + 日志警告 |
| `"mandatory"` | 工具硬编码写入 `data/`，需备份恢复 | 备份 data/ → 执行 → 恢复 + 移出产物 |
| 未定义 | 无已知风险，但作为安全惯例 | 默认拷贝 |

即：**所有涉及文件操作的工具，一律在 `tmp/` 隔离环境中执行**。

#### 执行前确认清单（Checklist）

在 Step 4 执行 CLI 之前，确认以下全部通过：

- [ ] `data/` 目录原始文件已备份或已确认不会被写入
- [ ] 所有输入文件路径已替换为 `tmp/` 副本路径
- [ ] 所有输出路径不指向 `data/` 或用户数据目录
- [ ] 对于 demo：`data/` 已备份到 `tmp/data_backup/`
- [ ] `tmp/` 目录存在且有足够磁盘空间（> 2x 输入数据量）

### Step 4：执行命令

使用 `execute_command` 工具执行构建的 CLI 命令：

```
execute_command(command="<构建的完整命令>", requires_approval=true)
```

**超时处理**：每个 Tool 的 Schema 中定义了 `timeout_seconds`（默认 300 秒）。大文件（>1GB）执行前预估耗时，预计 > 120 秒先询问用户。

**捕获输出**：`stdout`（正常输出）、`stderr`（错误信息）、`exit_code`（0=成功，非0=失败）

### Step 5：解析输出

#### 5a. 成功解析（exit_code == 0）

**数值类输出**（ca-precision / correlation / t-test / sdg-* 命令）：

CLI stdout 格式：`key=value`（空格分隔），解析正则：`(\w+)=([-\d.]+)`

```json
{
  "tool": "ca-precision",
  "status": "success",
  "result": { "FoM": 0.32, "PA": 0.71, "UA": 0.68, "Kappa": 0.76, "OA": 0.87 },
  "raw_stdout": "FoM=0.320000 PA=0.710000 ...",
  "exit_code": 0
}
```

**特殊解析**：
- `sdg-land-conversion` 输出 `score(positive)=72.3` 或 `score(negative)=28.5`
- `correlation` 输出 `R=0.456789`
- `t-test` 输出 `t=2.345678`
- `sdg-*`（除 conversion）输出 `score=72.3`

**文件类输出**（priority-* / priority-merge 命令）：

CLI stdout 格式：`Output: <path>`

```json
{
  "tool": "priority-loss",
  "status": "success",
  "result": { "output_file": "/data/PriorityAreas-1.tif", "status": "success" },
  "exit_code": 0
}
```

#### 5b. 失败解析（exit_code != 0）

| stderr 关键词 | 错误类型 | 用户提示 |
|--------------|---------|---------|
| `GDALOpen failed` / `Cannot open` | `file_access_error` | 文件不存在或无法读取，请检查路径 |
| `dimension` / `size mismatch` | `dimension_mismatch` | 输入文件行列数不一致 |
| `projection` / `CRS` | `projection_mismatch` | 投影坐标系不一致 |
| `NoData` / `nodata` | `nodata_error` | 数据中存在无效值 |
| `../tmp/` / `tmp directory` | `temp_dir_missing` | 缺少 ../tmp/ 临时目录 |
| `Float32` / `Float64` | `type_mismatch` | Rule 6 仅支持 Float32 |
| `stoi` / `stod` | `param_format_error` | 参数格式错误 |

#### 5c. 验证结果（仅对计算类工具执行）

> **新增步骤**：在 Step 5b 后、Step 6 前插入。对计算结果执行合理性校验、异常检测、置信度评分和自动诊断修复。

**跳过验证的工具**：`demo`、`help`、`check`、`priority-stats` — 非计算类工具。

**执行条件**：`result.status == "success" AND tool in [sdg-*, ca-precision, correlation, t-test, priority-loss, priority-transition, priority-buffer, priority-emission, priority-human-land, priority-merge]`

**验证伪代码**：
```
// 1. 加载验证规则
rules = loadValidationRules(tool)  // 从 agent/validation/rules/<tool>.json

// 2. 执行合法区间检查
range_check = checkValidRange(result.score, rules.valid_range)

// 3. 执行异常模式匹配
anomaly = matchAnomalyPatterns(result.score, result.metadata, rules.anomaly_patterns)

// 4. 加载历史数据对比
history = loadHistoryFromMemory(tool, region)  // 从 L1 读取

// 5. 计算置信度
confidence = calcConfidence(dataQuality, range_check, history, paramCompleteness)
// 权重来自 agent/validation/confidence-policy.json

// 6. 判定最终状态
if range_check == FAIL:
    status = "FAIL"
    if canAutoFix(failureReason):
        attemptAutoFixAndRetry()  // 最多 3 次
elif anomaly.severity == "WARN" OR history_bias > threshold:
    status = "WARN"
    diagnosis = generateDiagnosis(anomaly, history)
    // 诊断模式来自 agent/validation/diagnosis-patterns.json
else:
    status = "PASS"

// 7. 附加到结果
result.validation = { status, confidence, checks, diagnosis, suggestions }
```

**验证结果附加到输出 JSON**：
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
        "anomaly": { "status": "WARN", "detail": "得分偏高，types占比偏低", "suggestion": "检查 --types 参数" },
        "history": { "status": "WARN", "detail": "偏差 +15% vs 上次 68.0" },
        "params": { "status": "PASS", "detail": "所有必填参数已提供" }
      },
      "diagnosis": "本次得分比历史评估偏高15%，可能因额外包含地类3。建议确认 --types 参数。",
      "auto_fix_attempted": false
    }
  }
}
```

**验证技能引用**：详见 `agent/skills/agent-validator/SKILL-detail.md`

### Step 6：返回结果 + 分层写入 Memory

1. 将结构化结果 JSON 返回给 agent-planner
2. 调用 agent-memory 分层写入：

#### L0 — 更新工作记忆
```
updateWorkingMemory({ current_step, active_task_id, last_result_ref, status })
```

#### L1 — 创建/更新任务记录
- 执行前：`taskId = createTask({ intent, query, tool, params, session_id })`
- 执行后：`updateTask(taskId, { status, result, log_ref })`

#### L2 — 生成评估报告（仅计算类 Tool 成功时）
```
reportPath = generateReport(taskId, logPath, indicatorKnowledge)
```

#### L3 — 注册数据文件（仅文件类参数）
详见 `agent/shared/data-semantics.md` 中的交互流程。

#### L5 — 更新用户偏好（仅首次使用时）
```
updatePreferences({ default_params, focus_regions })
```

### Step 7：清理临时副本 + 数据完整性验证

**目的**：计算完成后删除 `tmp/` 临时拷贝，恢复被工具污染的 `data/` 目录，确保原始数据原封不动。

**执行时机**：
- Step 4 执行成功（exit_code == 0）→ 立即清理
- Step 4 执行失败（exit_code != 0）→ 立即清理
- Step 4 超时/中断 → 立即清理

**清理流程**：

```
# 7a. 清理输入文件副本
for tmp_path in copy_registry:
    if exists(tmp_path):
        rm tmp_path
        LOG_DEBUG("Cleaned up temp copy: " + tmp_path)

# 7b. demo 特殊处理：恢复 data/ 目录
if tool == "demo" and demo_backup:
    # 移出 demo 输出产物（不丢失结果）
    mv data/PriorityAreas-*.tif tmp/  2>/dev/null
    mv data/PriorityAreasRankingMap.tif tmp/  2>/dev/null
    
    # 校验原始数据完整性
    for backup_file in tmp/data_backup/**/*.tif:
        original = backup_file.replace("tmp/data_backup/", "data/")
        if exists(original) and hash(backup_file) != hash(original):
            cp backup_file original  # 恢复被修改的原始文件
            LOG_ERROR("Restored damaged file from backup: " + original)
        elif not exists(original):
            LOG_WARN("Original file missing after demo, restoring from backup: " + original)
            cp backup_file original
    
    # 清理备份
    rm -rf tmp/data_backup/
    LOG_INFO("data/ directory integrity verified and restored")

# 7c. 移除 demo 写入 data/ 的输出文件
for pattern in ["data/PriorityAreas-*.tif", "data/PriorityAreasRankingMap.tif"]:
    rm -f pattern  2>/dev/null

# 7d. 如果 tmp/ 目录为空，删除目录
if tmp_dir_is_empty("tmp/"):
    rmdir "tmp/"
```

**清理验证**：
```
# 最终检查：data/ 目录不能有任何 demo 输出产物
for path in ["data/PriorityAreas-1.tif", ..., "data/PriorityAreasRankingMap.tif"]:
    if exists(path):
        LOG_ERROR("FAILED TO CLEAN: " + path + " still exists in data/!")
        rm -f path  # 最后兜底删除

# 验证 data/ 目录纯净
for original_file in original_data_files:
    if hash(original_file) != hash_before_execution:
        LOG_CRITICAL("DATA CORRUPTION DETECTED: " + original_file)
        restore_from_backup(original_file)
```

**注意**：
- `priority-emission` 的中间文件（`../tmp/OriginalPrefix.tif` 等）由 geosdg-cli 内部管理
- 如果同一会话中多个 Tool 串行执行，每次 Step 3.5 独立，Step 7 仅清理当次
- 系统崩溃导致清理未执行 → 下次对话开始时自动检测并清理 `tmp/` 过期文件
- **demo 执行后绝对不允许 `data/` 目录下出现 PriorityAreas-*.tif**

---

## 错误处理

### Tool 不存在
```json
{
  "error": "tool_not_found",
  "message": "工具 '{name}' 不存在。可用工具列表见 agent/tools/manifest.json",
  "available_categories": ["ca-precision", "sdg-calc", "priority-area", "demo-help"]
}
```

### 必填参数缺失
```json
{
  "error": "missing_required_param",
  "tool": "ca-precision",
  "missing": ["ori", "sim"],
  "message": "工具 'ca-precision' 缺少必填参数：ori, sim"
}
```

### 文件不存在
```json
{
  "error": "file_not_found",
  "tool": "ca-precision",
  "param": "ori",
  "path": "/data/2010.tif",
  "message": "文件不存在或无法读取：/data/2010.tif"
}
```

---

## 边界情况处理

| 场景 | 处理方式 |
|------|---------|
| 路径含空格 | 用引号包裹路径值 |
| 大文件（>1GB） | 执行前告知预计耗时和拷贝耗时，询问是否继续 |
| CLI 可执行文件找不到 | 提示用户先编译 geosdg-cli |
| stdout 为空但 exit_code=0 | 返回 `{"status": "success", "result": {}, "warning": "CLI 未输出结果"}` |
| 参数值含特殊字符 | 转义或引号包裹 |
| 并行执行多个 Tool | 每个 Tool 独立调用，独立 Step 3.5 拷贝 + Step 7 清理 |
| Rule 5 缺少 ../tmp/ | 自动创建目录 |
| tmp/ 磁盘空间不足 | 拷贝前检查可用空间（需 > 2x 输入文件大小），不足则拒绝执行 |
| 同名文件冲突 | 添加来源目录前缀避免覆盖（如 `tmp/data1_lucc_2010.tif`） |
| 拷贝失败 | 返回 `data_copy_error`，不执行后续计算 |
| **demo 硬编码写入 data/** | 备份 data/ → 执行 → 恢复原始文件 + 移出输出产物 |
| **输出路径指向 data/ 或用户数据目录** | 强制重定向输出到 `tmp/`，日志警告 |
| **工具无 filepath 参数但内部访问 data/** | 检查 `input_constraints.data_files`，执行完整 data/ 备份恢复 |

---

## Important Notes

- 本 Skill **不修改任何 C++ 代码**，仅通过 `execute_command` 调用已编译的 `geosdg-cli`
- **`data/` 目录是禁区**：任何工具不得写入 `data/`。demo 等硬编码写入的工具通过备份恢复机制保护
- **数据安全是最高优先级**：Step 3.5 强制隔离（备份 + 拷贝 + 路径替换），Step 7 清理 + 恢复 + 完整性校验
- **用户数据目录同样受保护**：用户通过参数传入的任何数据文件路径，其所在目录视为只读
- `tmp/` 目录位于 workspace 根目录，与 `priority-emission` 内部使用的 `../tmp/`（相对于 cli/）是不同的概念
- `priority-emission`（Rule 5）：输入文件拷贝到 workspace `tmp/`；CLI 内部的中间文件写入 `cli/build/../tmp/`，由 CLI 自行管理
- 所有文件路径建议使用绝对路径
- `priority-human-land`（Rule 6）仅支持 `GDT_Float32` 人口数据
- **数据语义元信息（region/year）必须由用户提供**：详见 `agent/shared/data-semantics.md`
- **Step 3.5 + Step 7 保证**：原始数据在任何情况下（成功/失败/超时/系统崩溃）都不会被修改或残留输出文件
