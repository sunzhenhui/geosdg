# Agent Planner Skill — 完整执行流程

> 本文件为 agent-planner Skill 的完整层，由 SKILL.md 摘要层按需加载。
> 摘要层仅包含规划模式表和终止条件，本文件包含完整 ReAct 循环、参数引用、降级策略和示例。

---

## Prerequisites

- `agent/tools/manifest.json` — 可用 Tool 列表
- `agent/skills/agent-planner/references/plan-strategies.md` — 规划策略参考
- `agent/skills/agent-executor/SKILL.md` — 工具执行器（执行 Tool 调用）
- `agent/skills/agent-memory/SKILL.md` — 会话记忆（保存上下文和结果）
- `agent/memory/current/` — 当前会话状态

---

## ReAct 规划范式

规划器在 **Thought → Action → Observation** 循环中工作：

```
Cycle 1: Thought → 需要先查 SDG 15.3.1 的公式定义
          Action → 查询 sdg-indicator-knowledge Skill
          Observation → SDG 15.3.1 = 土地转换面积比, 输入两期 LUCC + transitions

Cycle 2: Thought → 已有公式和参数，需要确认用户提供了哪些数据及其语义元信息
          Action → 检查 L0 工作记忆 + L3 数据注册表 / 询问用户
          Observation → 用户提供了 lucc_2010.tif 和 lucc_2030.tif 路径
          ⚠️ 关键追问 → "请确认：这两个文件是哪一年的数据？属于哪个地区？"
          Observation → 用户："都是杭州的，2010 和 2030 年的 LUCC 数据"
          → 调用 L3 registerFile(path, region="杭州", year=2010/2030, {category:"LUCC"})

Cycle 3: Thought → 数据齐全且元信息已确认，执行 sdg-land-conversion 计算
          Action → 调用 agent-executor → sdg-land-conversion --init-lucc ... --curr-lucc ... --transitions 2:3,4:5
          Observation → 得分 = 72.3（正向转换 45%、负向 12%）
          ★ validation → { status: "WARN", confidence: 0.82, diagnosis: "偏差 +15% vs 上次 68.0" }

Cycle 4: Thought → 计算完成，得分 72.3，验证状态 WARN（历史偏差偏大但结果可用）
          → 附带 ⚠️ 标记和诊断建议
          Action → 解读结果 → 保存到 Memory（含 validation 字段）→ 返回给用户
          Observation → 完成（最终回复附带置信度 0.82 和历史偏差提示）
```

---

## 执行流程

### Step 1：接收意图 + 加载上下文

1. 接收 agent-router 传来的意图类别 + 用户原始查询
2. 读取 L0 工作记忆 `agent/memory/current/working.json`，恢复会话上下文（status/plan/current_step）
3. 读取 L5 用户偏好 `agent/memory/preferences.json`（focus_regions / preferred_indicators / default_params）
4. 如用户提及地区 → 调用 L3 `detectReusableFiles(region)` 检查是否有可复用的已注册数据
5. 读取 `agent/tools/manifest.json`，确定可用 Tool 列表

### Step 2：选择规划模式

根据意图类别和需求复杂度，参考 `references/plan-strategies.md` 选择规划模式：

| 意图 | 默认模式 | 说明 |
|------|---------|------|
| sdg-calc（单一指标） | linear | 知识查询 → 数据检查 → 计算 → 解读 |
| sdg-calc（多区域对比） | parallel | 各区域独立计算 → 汇总对比 |
| ca-precision | linear | 数据检查 → ca-precision → 可选统计检验 → 解读 |
| priority | linear + loop | 规则 1-6 逐个执行 → merge → 解读 |
| composite | 递归 + 报告生成 | 拆解为子意图 → 分别规划 → 综合评估完成后自动触发报告生成 |
| pipeline | DAG 编排 | 解析 Pipeline JSON → DAG 拓扑排序 → 按层级并行执行 → 检查点/恢复 |
| knowledge | 无规划 | 纯检索，直接返回 |

### Step 3：生成执行计划

输出有序的执行计划，每步包含：

```json
{
  "step": 1,
  "action": "knowledge_lookup | data_check | tool_call | interpret | ask_user",
  "target": "skill-name / tool-name / filesystem / user / self",
  "params": { ... },
  "expected_output": "预期输出描述",
  "depends_on": [前置步骤号] 或 null,
  "on_failure": "失败时的降级策略",
  "condition": "条件表达式（可选）",
  "branch": "分支标识（并行模式，可选）"
}
```

**参数引用**：后续步骤可引用前置步骤的输出，使用 `${step_N.output.field}` 语法。

### Step 4：ReAct 循环执行

对计划中的每一步执行 Thought → Action → Observation：

```
for step in plan:
    # Thought：分析当前状态
    thought = analyze_current_state(step, memory, observation)

    # Action：执行动作
    if step.action == "knowledge_lookup":
        observation = load_skill(step.target, step.params)
    elif step.action == "data_check":
        observation = check_files(step.params)
        # ⚠️ 关键：数据文件存在不等于语义元信息齐全
        # region/year/category 必须由用户提供，Agent 不可自动推断
        # 详见 agent/shared/data-semantics.md
        if observation.has_files and not observation.has_region_year:
            observation = ask_user({
                "question": "请确认数据语义信息：1) 覆盖哪个地区？2) 哪一年的数据？3) 数据类型（LUCC/POP/INFRA）？",
                "confirmations": ["region", "year", "category"]
            })
            if user_responded:
                agent_memory.registerFile(path, region, year, {category})
    elif step.action == "tool_call":
        # ★ 数据安全：agent-executor 在 Step 3.5 自动将输入文件拷贝到 tmp/
        # Step 4 使用副本计算，Step 7 自动清理，确保用户原始数据不受影响
        observation = agent_executor.execute(step.target, step.params)

        # ★ 验证结果感知（Executor Step 5c 产出）
        # 当 observation.result 包含 validation 字段时，Planner 需根据验证状态调整后续规划
        if observation.result and observation.result.validation:
            validation = observation.result.validation

            if validation.status == "FAIL":
                # FAIL：结果不可信，需检查是否可自动修复
                if validation.auto_fix_suggestion and not validation.auto_fix_attempted:
                    # 有自动修复建议且尚未尝试 → 规划重试 Action
                    retry_step = {
                        "step": step.step + 0.5,
                        "action": "tool_call",
                        "target": step.target,
                        "params": merge_params(step.params, validation.auto_fix_suggestion),
                        "expected_output": "修复后重新计算的结果",
                        "depends_on": [step.step],
                        "on_failure": "向用户报告诊断信息",
                        "_retry_reason": validation.diagnosis
                    }
                    plan.insert_after(step, retry_step)
                    agent_memory.updateWorkingMemory({ auto_fix_count: auto_fix_count + 1 })
                elif validation.auto_fix_attempted:
                    # 自动修复已尝试但仍失败 → 向用户报告诊断
                    observation = {
                        "status": "fail",
                        "diagnosis": validation.diagnosis,
                        "suggestions": validation.suggestions,
                        "message": "自动修复尝试失败，建议手动排查"
                    }
                else:
                    # 不可自动修复 → 向用户报告诊断信息
                    observation = {
                        "status": "fail",
                        "diagnosis": validation.diagnosis,
                        "suggestions": validation.suggestions
                    }

            elif validation.status == "WARN":
                # WARN：结果可用但需标记警告，在最终回复中附带 ⚠️ 和诊断建议
                observation.warnings = observation.warnings or []
                observation.warnings.append({
                    "diagnosis": validation.diagnosis,
                    "confidence": validation.confidence,
                    "suggestions": validation.suggestions
                })
                # 继续执行后续步骤，不中断流程

            elif validation.status == "PASS":
                # PASS：正常流程，附加置信度到结果
                observation.confidence = validation.confidence

    elif step.action == "interpret":
        observation = interpret_result(step.params)
    elif step.action == "ask_user":
        observation = ask_user(step.params)
        if user_not_responded:
            break

    # Observation：记录结果到分层 Memory
    agent_memory.updateWorkingMemory({ current_step: step.step })

    # 检查终止条件
    if step.on_failure and observation.failed:
        apply_degradation_strategy(step.on_failure)
    if max_steps_reached or timeout:
        break
```

### Step 5：结果汇总

全部步骤完成后（或达到终止条件），汇总结果：

```json
{
  "intent": "sdg-calc",
  "original_query": "评估杭州 SDG 15.3.1",
  "plan_summary": {
    "total_steps": 4,
    "completed_steps": 4,
    "status": "completed"
  },
  "results": [
    { "step": 1, "action": "knowledge_lookup", "output": { ... } },
    { "step": 2, "action": "data_check", "output": { ... } },
    { "step": 3, "action": "tool_call", "output": { "score": 72.3, "validation": { "status": "PASS", "confidence": 0.91 } } },
    { "step": 4, "action": "interpret", "output": "杭州 SDG 15.3.1 = 72.3，土地退化程度中等..." }
  ],
  "validation_summary": {
    "status": "PASS",
    "confidence": 0.91,
    "warnings": []
  },
  "final_answer": "杭州 SDG 15.3.1 土地退化指标得分为 72.3（满分 100，置信度 0.91）。..."
}
```

---

## 规划模式详解

### Linear（线性）
步骤严格按序执行，后一步依赖前一步输出。详见 `references/plan-strategies.md` §2。

### Parallel（并行）
多个独立子任务同时执行，完成后汇总。详见 `references/plan-strategies.md` §3。

### Conditional（条件）
根据前置步骤的 Observation 决定后续路径。详见 `references/plan-strategies.md` §4。

### Loop（循环）
对多个项目重复执行相同流程。详见 `references/plan-strategies.md` §5。

### Pipeline（DAG 编排）
多步骤流水线编排，支持依赖解析、并行执行、检查点/恢复。详见 `agent/pipeline/` 模块。

**触发条件**：用户请求包含"流水线"、"编排"、"pipeline"、"批量"、"多步骤"等关键词，或需要按 DAG 顺序执行多个计算步骤。

**执行流程**：
1. 解析 Pipeline JSON 配置文件（`agent/pipeline/schema.py` 验证）
2. 构建 DAG 依赖图，拓扑排序分层（`get_step_order()`）
3. 按层级执行：同层步骤并行（`ThreadPoolExecutor`），跨层串行
4. 每步调用 `geosdg-cli` 子命令（`execute_step()`）
5. 检查点自动保存（`CheckpointManager`），支持 `--resume` 续跑
6. 变量替换 `${VAR}`、条件跳过 `condition`、失败重试 `retry`

**Pipeline 配置格式**：
```json
{
  "name": "sdg-full",
  "version": "1.0",
  "variables": { "DATA_DIR": "/data/tianmu" },
  "steps": [
    {
      "id": "ca-sim",
      "tool": "ca-simulate",
      "params": { "init-lucc": "${DATA_DIR}/lucc_2010.tif" },
      "depends_on": []
    },
    {
      "id": "sdg-1531",
      "tool": "sdg-land-conversion",
      "params": { "init-lucc": "${DATA_DIR}/lucc_2010.tif" },
      "depends_on": ["ca-sim"],
      "condition": "ca-sim.success"
    }
  ]
}
```

**与 Agent Planner 的集成**：
- Planner 识别 pipeline 意图后，调用 `pipeline run <config.json>` 执行
- Pipeline 内部的 DAG 编排由 `PipelineOrchestrator` 独立完成，不经过 ReAct 循环
- 执行结果返回 Planner，由 Planner 解读并汇总给用户
- 支持 `pipeline dry-run` 预览执行计划、`pipeline validate` 验证配置

---

## ReAct 循环终止条件

| 条件 | 阈值 | 行为 |
|------|------|------|
| 最大步数 | 8 步（默认） | 超出则停止，汇总已完成步骤 |
| 超时 | 600 秒 | 超时则中止，返回部分结果 |
| 错误重试 | 2 次 | 同一步骤失败 2 次后降级或询问用户 |
| 用户中断 | — | 立即停止，保存进度到 Memory |
| 全部完成 | 所有 step = done | 正常结束，汇总结果 |

---

## 降级策略

| 失败场景 | 降级策略 |
|---------|---------|
| 数据文件不存在 | ask_user → 引导用户提供数据路径 |
| 数据文件存在但缺少 region/year | ask_user → 追问"该文件覆盖哪个地区？哪一年的数据？" → 用户确认后注册到 L3 |
| Tool 执行失败（参数错误） | 检查参数 → 修正后重试（最多 2 次） |
| Tool 执行失败（数据格式不符） | 解读错误 → 告知用户具体问题 → 建议修复方式 |
| 知识库查询无结果 | 告知用户该指标暂不支持 → 推荐相近指标 |
| 超时 | 告知用户预计耗时 → 询问是否继续或取消 |
| 验证 FAIL（可自动修复） | 按 auto_fix_suggestion 修正参数 → 重试（最多 3 次）→ 仍失败则报告诊断 |
| 验证 FAIL（不可修复） | 向用户报告诊断信息 + 建议手动排查步骤 → 不继续后续计算 |
| 验证 WARN | 继续执行 → 在最终回复中附带 ⚠️ 标记和诊断建议 |

---

## 完整示例

### 示例：评估杭州 SDG 15.3.1

**用户输入**："评估杭州 SDG 15.3.1"

**Router 识别**：intent = sdg-calc

**Planner 生成计划**：
```json
{
  "intent": "sdg-calc",
  "original_query": "评估杭州 SDG 15.3.1",
  "plan": [
    {
      "step": 1,
      "action": "knowledge_lookup",
      "target": "sdg-indicator-knowledge",
      "params": { "sdg_id": "15.3.1" },
      "expected_output": "计算公式和所需参数",
      "depends_on": null,
      "on_failure": "告知用户该指标暂不支持，推荐相近指标"
    },
    {
      "step": 2,
      "action": "data_check",
      "target": "filesystem",
      "params": { "pattern": "lucc*.tif", "directory": "用户工作区" },
      "expected_output": "可用数据路径列表 + 用户确认的语义元信息",
      "required_confirmations": {
        "region": "数据覆盖的地理区域（如杭州），Agent 无法从 GeoTIFF 自动推断",
        "year": "数据的对应年份",
        "category": "数据类型（LUCC/POP/INFRA）"
      },
      "depends_on": null,
      "on_failure": "降级：引导用户提供数据文件路径、区域名称和年份"
    },
    {
      "step": 3,
      "action": "tool_call",
      "target": "sdg-land-conversion",
      "params": {
        "init_lucc": "${step_2.output.init_file}",
        "curr_lucc": "${step_2.output.curr_file}",
        "transitions": "${step_1.output.default_transitions}",
        "positive": false
      },
      "expected_output": "土地退化得分",
      "depends_on": [1, 2],
      "on_failure": "检测错误类型，投影不匹配则提示修复，数据缺失则降级"
    },
    {
      "step": 4,
      "action": "interpret",
      "target": "self",
      "params": { "result": "${step_3.output}", "sdg_id": "15.3.1" },
      "expected_output": "自然语言解读 + 行动建议",
      "depends_on": [3],
      "on_failure": "原始数值输出 + 建议咨询专家解读"
    }
  ]
}
```

**执行结果**（4 步全部完成）：
```
Step 1: SDG 15.3.1 = 土地退化指标，使用 sdg-land-conversion 计算，负向指标
Step 2: 用户提供 /data/hangzhou/lucc_2010.tif 和 lucc_2030.tif
        → Agent 追问 region/year → 用户确认"杭州，2010 和 2030 年 LUCC"
        → L3 registerFile 注册两条数据记录
Step 3: 执行 sdg-land-conversion → 得分 72.3
Step 4: 杭州 SDG 15.3.1 = 72.3，土地退化程度中等，正向转换占主导...
```

---

### 示例：Pipeline 编排 SDG 全流程

**用户输入**："用流水线跑一遍天目山 SDG 全流程"

**Router 识别**：intent = pipeline

**Planner 生成计划**：
```json
{
  "intent": "pipeline",
  "original_query": "用流水线跑一遍天目山 SDG 全流程",
  "plan": [
    {
      "step": 1,
      "action": "tool_call",
      "target": "pipeline-run",
      "params": {
        "config": "agent/pipeline/examples/sdg-full.json",
        "var": ["DATA_DIR=/data/tianmu", "REGION=tianmu"]
      },
      "expected_output": "Pipeline 执行结果（各步骤状态和输出）",
      "depends_on": null,
      "on_failure": "报告失败的步骤和错误信息"
    },
    {
      "step": 2,
      "action": "interpret",
      "target": "self",
      "params": { "result": "${step_1.output}" },
      "expected_output": "自然语言汇总：哪些步骤成功/失败，关键指标值",
      "depends_on": [1],
      "on_failure": "原始 Pipeline 结果输出"
    }
  ]
}
```

**执行结果**：
```
Step 1: Pipeline 'sdg-full' 执行完成
        Level 1: ca-pg (OK), ca-markov (OK) — 并行
        Level 2: ca-simulate (OK) — 依赖 ca-pg + ca-markov
        Level 3: sdg-land-conversion (OK), sdg-1131 (OK) — 并行
        Level 4: priority-merge (OK) — 依赖所有 sdg 步骤
        总耗时: 45.2s

Step 2: 天目山 SDG 全流程完成。CA 模拟精度 FoM=0.72，
        SDG 15.3.1=72.3，SDG 11.3.1=68.5，优先区域已合并输出。
```

---

## Important Notes

- 规划器是 Agent 的核心，但不是唯一入口——用户也可直接调用 agent-executor 执行单个 Tool
- 规划结果不是不可变的——执行过程中可根据 Observation 动态调整后续步骤
- 最大步数限制为 8 步，避免规划链条过长超出 LLM 上下文窗口
- 中间结果摘要压缩：超过 3 步时，早期步骤的 Observation 仅保留关键字段
- 跨会话恢复时，规划器从 L0 working.json 读取未完成的计划，从断点继续
- `knowledge` 意图**不经过规划器**，Router 直接路由到知识检索 Skill
- **数据安全**：所有 `tool_call` 动作由 agent-executor 自动执行 Step 3.5 数据拷贝，Planner 无需显式处理。用户原始文件不会被修改
- **数据语义元信息（region/year）必须由用户提供**：详见 `agent/shared/data-semantics.md`
- **综合评估后自动触发报告生成**：当意图为 `composite` 时，规划器在规划链末尾自动追加报告生成步骤。详见 `references/plan-strategies.md` §9。
- **验证结果感知**：Executor Step 5c 产出的 `validation` 字段会被 Planner 在 ReAct 循环中感知。FAIL 状态触发重试或诊断报告；WARN 状态在最终回复中附带 ⚠️ 标记；PASS 状态附加置信度。详见 `agent/skills/agent-validator/SKILL.md`。
- **Pipeline 编排模式**：当意图为 `pipeline` 时，Planner 调用 `pipeline run` 执行预定义的 DAG 配置。Pipeline 内部的步骤编排由 `PipelineOrchestrator` 独立完成，不经过 ReAct 循环。支持 `--resume` 续跑、`--dry-run` 预览、`--var` 变量覆盖。详见 `agent/pipeline/` 模块。
