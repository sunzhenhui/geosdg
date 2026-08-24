# Agent Planner Skill — Full Execution Flow

> This file is the complete layer of the agent-planner Skill, loaded on demand by the SKILL.md summary layer.
> The summary layer only contains the planning-mode table and termination conditions; this file contains the full ReAct loop, parameter references, fallback strategies, and examples.

---

## Prerequisites

- `agent/tools/manifest.json` — available Tool list
- `agent/skills/agent-planner/references/plan-strategies_en.md` — planning-strategy reference
- `agent/skills/agent-executor/SKILL.md` — tool executor (executes Tool calls)
- `agent/skills/agent-memory/SKILL.md` — session memory (saves context and results)
- `agent/memory/current/` — current session state

---

## ReAct Planning Paradigm

The planner works in a **Thought → Action → Observation** loop:

```
Cycle 1: Thought → First look up the formula definition of SDG 15.3.1
          Action → Query the sdg-indicator-knowledge Skill
          Observation → SDG 15.3.1 = land-conversion area ratio, inputs two-period LUCC + transitions

Cycle 2: Thought → Formula and parameters known; confirm which data the user provided and its semantic metadata
          Action → Check L0 working memory + L3 data registry / ask the user
          Observation → User provided paths lucc_2010.tif and lucc_2030.tif
          ⚠️ Key follow-up → "Please confirm: which year's data are these two files? Which region?"
          Observation → User: "Both are Hangzhou, 2010 and 2030 LUCC data"
          → Call L3 registerFile(path, region="Hangzhou", year=2010/2030, {category:"LUCC"})

Cycle 3: Thought → Data complete and metadata confirmed; run sdg-land-conversion
          Action → Call agent-executor → sdg-land-conversion --init-lucc ... --curr-lucc ... --transitions 2:3,4:5
          Observation → Score = 72.3 (positive conversion 45%, negative 12%)
          ★ validation → { status: "WARN", confidence: 0.82, diagnosis: "Bias +15% vs last run 68.0" }

Cycle 4: Thought → Computation done, score 72.3, validation status WARN (history bias high but result usable)
          → Attach ⚠️ marker and diagnostic suggestions
          Action → Interpret the result → save to Memory (including validation) → return to the user
          Observation → Complete (final reply carries confidence 0.82 and a history-bias hint)
```

---

## Execution Flow

### Step 1: Receive Intent + Load Context

1. Receive the intent category + the user's original query from agent-router
2. Read L0 working memory `agent/memory/current/working.json`, restore session context (status/plan/current_step)
3. Read L5 user preferences `agent/memory/preferences.json` (focus_regions / preferred_indicators / default_params)
4. If the user mentions a region → call L3 `detectReusableFiles(region)` to check for reusable registered data
5. Read `agent/tools/manifest.json` to determine the available Tool list

### Step 2: Select a Planning Mode

Choose a planning mode per `references/plan-strategies_en.md` based on intent category and complexity:

| Intent | Default mode | Description |
|--------|--------------|-------------|
| sdg-calc (single indicator) | linear | Knowledge query → data check → compute → interpret |
| sdg-calc (multi-region comparison) | parallel | Each region computes independently → aggregate and compare |
| ca-precision | linear | Data check → ca-precision → optional statistical test → interpret |
| priority | linear + loop | Run rules 1-6 sequentially → merge → interpret |
| composite | recursive + report generation | Decompose into sub-intents → plan each → auto-trigger report generation after assessment |
| pipeline | DAG orchestration | Parse Pipeline JSON → DAG topological sort → parallel by level → checkpoint/resume |
| knowledge | no planning | Pure retrieval, return directly |

### Step 3: Generate the Execution Plan

Output an ordered plan; each step contains:

```json
{
  "step": 1,
  "action": "knowledge_lookup | data_check | tool_call | interpret | ask_user",
  "target": "skill-name / tool-name / filesystem / user / self",
  "params": { ... },
  "expected_output": "expected output description",
  "depends_on": [preceding step number] or null,
  "on_failure": "fallback strategy on failure",
  "condition": "condition expression (optional)",
  "branch": "branch identifier (parallel mode, optional)"
}
```

**Parameter reference**: later steps can reference earlier steps' output via `${step_N.output.field}`.

### Step 4: ReAct Loop Execution

For each step in the plan, run Thought → Action → Observation:

```
for step in plan:
    # Thought: analyze the current state
    thought = analyze_current_state(step, memory, observation)

    # Action: perform the action
    if step.action == "knowledge_lookup":
        observation = load_skill(step.target, step.params)
    elif step.action == "data_check":
        observation = check_files(step.params)
        # ⚠️ Key: a data file existing does not mean its semantic metadata is complete
        # region/year/category must be provided by the user; the Agent must not auto-infer
        # See agent/shared/data-semantics.md
        if observation.has_files and not observation.has_region_year:
            observation = ask_user({
                "question": "Please confirm the data semantic info: 1) which region? 2) which year? 3) type (LUCC/POP/INFRA)?",
                "confirmations": ["region", "year", "category"]
            })
            if user_responded:
                agent_memory.registerFile(path, region, year, {category})
    elif step.action == "tool_call":
        # ★ Data safety: agent-executor auto-copies input files to tmp/ in Step 3.5
        # Step 4 computes on the copies, Step 7 auto-cleans up, keeping user original data untouched
        observation = agent_executor.execute(step.target, step.params)

        # ★ Validation-aware (produced by Executor Step 5c)
        # When observation.result contains a validation field, the Planner adjusts follow-up planning by its status
        if observation.result and observation.result.validation:
            validation = observation.result.validation

            if validation.status == "FAIL":
                # FAIL: result untrustworthy; check if it can be auto-fixed
                if validation.auto_fix_suggestion and not validation.auto_fix_attempted:
                    # Has an auto-fix suggestion and not yet attempted → plan a retry Action
                    retry_step = {
                        "step": step.step + 0.5,
                        "action": "tool_call",
                        "target": step.target,
                        "params": merge_params(step.params, validation.auto_fix_suggestion),
                        "expected_output": "result recomputed after the fix",
                        "depends_on": [step.step],
                        "on_failure": "report diagnostic info to the user",
                        "_retry_reason": validation.diagnosis
                    }
                    plan.insert_after(step, retry_step)
                    agent_memory.updateWorkingMemory({ auto_fix_count: auto_fix_count + 1 })
                elif validation.auto_fix_attempted:
                    # Auto-fix attempted but still failing → report diagnostics to the user
                    observation = {
                        "status": "fail",
                        "diagnosis": validation.diagnosis,
                        "suggestions": validation.suggestions,
                        "message": "Auto-fix attempt failed; manual troubleshooting recommended"
                    }
                else:
                    # Not auto-fixable → report diagnostic info to the user
                    observation = {
                        "status": "fail",
                        "diagnosis": validation.diagnosis,
                        "suggestions": validation.suggestions
                    }

            elif validation.status == "WARN":
                # WARN: result usable but needs a warning marker; attach ⚠️ and diagnosis in the final reply
                observation.warnings = observation.warnings or []
                observation.warnings.append({
                    "diagnosis": validation.diagnosis,
                    "confidence": validation.confidence,
                    "suggestions": validation.suggestions
                })
                # Continue with later steps; do not interrupt the flow

            elif validation.status == "PASS":
                # PASS: normal flow; attach confidence to the result
                observation.confidence = validation.confidence

    elif step.action == "interpret":
        observation = interpret_result(step.params)
    elif step.action == "ask_user":
        observation = ask_user(step.params)
        if user_not_responded:
            break

    # Observation: record the result into layered Memory
    agent_memory.updateWorkingMemory({ current_step: step.step })

    # Check termination conditions
    if step.on_failure and observation.failed:
        apply_degradation_strategy(step.on_failure)
    if max_steps_reached or timeout:
        break
```

### Step 5: Summarize Results

After all steps complete (or a termination condition is reached), summarize:

```json
{
  "intent": "sdg-calc",
  "original_query": "Assess SDG 15.3.1 for Hangzhou",
  "plan_summary": {
    "total_steps": 4,
    "completed_steps": 4,
    "status": "completed"
  },
  "results": [
    { "step": 1, "action": "knowledge_lookup", "output": { ... } },
    { "step": 2, "action": "data_check", "output": { ... } },
    { "step": 3, "action": "tool_call", "output": { "score": 72.3, "validation": { "status": "PASS", "confidence": 0.91 } } },
    { "step": 4, "action": "interpret", "output": "Hangzhou SDG 15.3.1 = 72.3, moderate land degradation..." }
  ],
  "validation_summary": {
    "status": "PASS",
    "confidence": 0.91,
    "warnings": []
  },
  "final_answer": "Hangzhou's SDG 15.3.1 land-degradation indicator score is 72.3 (out of 100, confidence 0.91). ..."
}
```

---

## Planning Modes in Detail

### Linear
Steps run strictly in order; each step depends on the previous step's output. See `references/plan-strategies_en.md` §2.

### Parallel
Multiple independent sub-tasks run simultaneously, then are aggregated. See `references/plan-strategies_en.md` §3.

### Conditional
The follow-up path is decided by the Observation of a preceding step. See `references/plan-strategies_en.md` §4.

### Loop
The same flow is repeated across multiple items. See `references/plan-strategies_en.md` §5.

### Pipeline (DAG orchestration)
Multi-step pipeline orchestration with dependency resolution, parallel execution, and checkpoint/resume. See the `agent/pipeline/` module.

**Trigger conditions**: the user request contains keywords such as "pipeline", "orchestration", "batch", "multi-step", or requires running multiple computation steps in DAG order.

**Execution flow**:
1. Parse the Pipeline JSON config file (validated by `agent/pipeline/schema.py`)
2. Build the DAG dependency graph, topologically sort into levels (`get_step_order()`)
3. Execute by level: same-level steps in parallel (`ThreadPoolExecutor`), across levels serially
4. Each step invokes a `geosdg-cli` sub-command (`execute_step()`)
5. Checkpoints auto-save (`CheckpointManager`), supporting `--resume`
6. Variable substitution `${VAR}`, conditional skip `condition`, failure retry `retry`

**Pipeline config format**:
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

**Integration with the Agent Planner**:
- After recognizing the pipeline intent, the Planner calls `pipeline run <config.json>` to execute
- The DAG orchestration inside the pipeline is handled independently by `PipelineOrchestrator`, not through the ReAct loop
- Execution results return to the Planner, which interprets and summarizes them for the user
- Supports `pipeline dry-run` to preview the plan and `pipeline validate` to validate the config

---

## ReAct Loop Termination Conditions

| Condition | Threshold | Behavior |
|-----------|-----------|----------|
| Max steps | 8 steps (default) | Stop when exceeded; summarize completed steps |
| Timeout | 600 seconds | Abort on timeout; return partial results |
| Error retries | 2 times | After 2 failures of the same step, degrade or ask the user |
| User interrupt | — | Stop immediately, save progress to Memory |
| All complete | all steps = done | End normally, summarize results |

---

## Fallback Strategies

| Failure scenario | Fallback strategy |
|------------------|-------------------|
| Data file not found | ask_user → guide the user to provide the data path |
| Data file exists but region/year missing | ask_user → "Which region does this file cover? Which year?" → register to L3 after confirmation |
| Tool failure (parameter error) | Check parameters → fix and retry (up to 2 times) |
| Tool failure (data format mismatch) | Interpret the error → tell the user the specific issue → suggest a fix |
| Knowledge-base query empty | Tell the user this indicator is unsupported → recommend similar indicators |
| Timeout | Tell the user the estimated duration → ask whether to continue or cancel |
| Validation FAIL (auto-fixable) | Fix parameters per auto_fix_suggestion → retry (up to 3 times) → still failing then report diagnosis |
| Validation FAIL (not fixable) | Report diagnosis + suggest manual troubleshooting → do not continue later computation |
| Validation WARN | Continue → attach ⚠️ marker and diagnosis in the final reply |

---

## Full Examples

### Example: Assess SDG 15.3.1 for Hangzhou

**User input**: "Assess SDG 15.3.1 for Hangzhou"

**Router recognition**: intent = sdg-calc

**Planner-generated plan**:
```json
{
  "intent": "sdg-calc",
  "original_query": "Assess SDG 15.3.1 for Hangzhou",
  "plan": [
    {
      "step": 1,
      "action": "knowledge_lookup",
      "target": "sdg-indicator-knowledge",
      "params": { "sdg_id": "15.3.1" },
      "expected_output": "calculation formula and required parameters",
      "depends_on": null,
      "on_failure": "tell the user this indicator is unsupported, recommend similar indicators"
    },
    {
      "step": 2,
      "action": "data_check",
      "target": "filesystem",
      "params": { "pattern": "lucc*.tif", "directory": "user's workspace" },
      "expected_output": "available data path list + user-confirmed semantic metadata",
      "required_confirmations": {
        "region": "the geographic region the data covers (e.g., Hangzhou); the Agent cannot auto-infer from GeoTIFF",
        "year": "the year the data corresponds to",
        "category": "data type (LUCC/POP/INFRA)"
      },
      "depends_on": null,
      "on_failure": "degrade: guide the user to provide the data file path, region name, and year"
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
      "expected_output": "land-degradation score",
      "depends_on": [1, 2],
      "on_failure": "detect error type; projection mismatch → suggest fix, missing data → degrade"
    },
    {
      "step": 4,
      "action": "interpret",
      "target": "self",
      "params": { "result": "${step_3.output}", "sdg_id": "15.3.1" },
      "expected_output": "natural-language interpretation + action suggestions",
      "depends_on": [3],
      "on_failure": "output raw values + suggest consulting an expert"
    }
  ]
}
```

**Execution result** (all 4 steps complete):
```
Step 1: SDG 15.3.1 = land-degradation indicator, computed via sdg-land-conversion, a negative indicator
Step 2: User provides /data/hangzhou/lucc_2010.tif and lucc_2030.tif
        → Agent asks region/year → user confirms "Hangzhou, 2010 and 2030 LUCC"
        → L3 registerFile registers two data records
Step 3: Run sdg-land-conversion → score 72.3
Step 4: Hangzhou SDG 15.3.1 = 72.3, moderate land degradation, positive conversion dominant...
```

---

### Example: Pipeline-orchestrated full SDG workflow

**User input**: "Run the full Tianmu Mountain SDG workflow via a pipeline"

**Router recognition**: intent = pipeline

**Planner-generated plan**:
```json
{
  "intent": "pipeline",
  "original_query": "Run the full Tianmu Mountain SDG workflow via a pipeline",
  "plan": [
    {
      "step": 1,
      "action": "tool_call",
      "target": "pipeline-run",
      "params": {
        "config": "agent/pipeline/examples/sdg-full.json",
        "var": ["DATA_DIR=/data/tianmu", "REGION=tianmu"]
      },
      "expected_output": "pipeline execution result (per-step status and output)",
      "depends_on": null,
      "on_failure": "report the failed step and its error message"
    },
    {
      "step": 2,
      "action": "interpret",
      "target": "self",
      "params": { "result": "${step_1.output}" },
      "expected_output": "natural-language summary: which steps succeeded/failed, key indicator values",
      "depends_on": [1],
      "on_failure": "output the raw pipeline result"
    }
  ]
}
```

**Execution result**:
```
Step 1: Pipeline 'sdg-full' completed
        Level 1: ca-pg (OK), ca-markov (OK) — parallel
        Level 2: ca-simulate (OK) — depends on ca-pg + ca-markov
        Level 3: sdg-land-conversion (OK), sdg-1131 (OK) — parallel
        Level 4: priority-merge (OK) — depends on all sdg steps
        Total time: 45.2s

Step 2: Tianmu Mountain full SDG workflow complete. CA simulation accuracy FoM=0.72,
        SDG 15.3.1=72.3, SDG 11.3.1=68.5, priority areas merged and output.
```

---

## Important Notes

- The planner is the Agent's core, but not the only entry — users can also call agent-executor directly to run a single Tool
- The plan is not immutable — later steps can be adjusted dynamically based on Observations during execution
- The max-step limit is 8, avoiding an over-long planning chain exceeding the LLM context window
- Intermediate-result compression: beyond 3 steps, early-step Observations keep only key fields
- On cross-session recovery, the planner reads the unfinished plan from L0 working.json and resumes from the breakpoint
- The `knowledge` intent **does not go through the planner**; the Router routes it directly to the knowledge-retrieval Skill
- **Data safety**: every `tool_call` action has agent-executor auto-run Step 3.5 data copy; the Planner needs no explicit handling. User original files are never modified
- **Data semantic metadata (region/year) must be provided by the user**: see `agent/shared/data-semantics.md`
- **Auto-trigger report generation after composite assessment**: when the intent is `composite`, the planner auto-appends a report-generation step at the end of the plan chain. See `references/plan-strategies_en.md` §9.
- **Validation-aware**: the `validation` field produced by Executor Step 5c is perceived by the Planner in the ReAct loop. FAIL triggers retry or a diagnosis report; WARN attaches a ⚠️ marker in the final reply; PASS attaches confidence. See `agent/skills/agent-validator/SKILL.md`.
- **Pipeline orchestration mode**: when the intent is `pipeline`, the Planner calls `pipeline run` to execute a predefined DAG config. Step orchestration inside the pipeline is handled independently by `PipelineOrchestrator`, not through the ReAct loop. Supports `--resume`, `--dry-run` preview, and `--var` variable overrides. See the `agent/pipeline/` module.
