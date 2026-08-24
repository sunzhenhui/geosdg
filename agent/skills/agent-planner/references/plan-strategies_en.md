# Task Planning Strategies

> This document defines the agent-planner's planning modes, templates, and decision rules.
> The planner selects an appropriate planning mode based on user intent and the available Tool list to generate an execution plan.

---

## 1. Planning Mode Overview

| Mode | Applicable scenario | Typical use case | Dependency |
|------|---------------------|------------------|------------|
| **linear** | Steps have strong dependencies and must run in order | Full SDG assessment chain: knowledge query → data check → compute → interpret | step N depends on step N-1 |
| **parallel** | Multiple independent computations with no interdependence | Compare SDG 11.3.1 of Hangzhou and Shanghai | No dependency between branches |
| **conditional** | The follow-up path is decided by a preceding result | Data missing → guide the user; data complete → compute directly | Branch decided by Observation |
| **loop** | Repeatedly process multiple files/regions | Compute SDG indicators one by one for [Hangzhou, Shanghai, Suzhou] | The loop body is linear |

---

## 2. Linear Mode

### Applicable scenario
Strict dependency between steps; a later step needs the previous step's output as input.

### Template
```
Step 1: knowledge_lookup → query the indicator definition and formula
Step 2: data_check → check whether the data is complete
Step 3: tool_call → run the computation (depends on Step 1 parameters + Step 2 data)
Step 4: interpret → interpret the result (depends on Step 3 output)
```

### Example: assess SDG 15.3.1 for Hangzhou
```json
[
  {
    "step": 1,
    "action": "knowledge_lookup",
    "target": "sdg-indicator-knowledge",
    "params": { "sdg_id": "15.3.1" },
    "expected_output": "formula: land-conversion area ratio; required params: two-period LUCC + transitions",
    "depends_on": null
  },
  {
    "step": 2,
    "action": "data_check",
    "target": "filesystem",
    "params": { "pattern": "lucc*.tif", "directory": "user workspace" },
    "expected_output": "available data path list",
    "depends_on": null
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
    "expected_output": "land-degradation score (0-100)",
    "depends_on": [1, 2]
  },
  {
    "step": 4,
    "action": "interpret",
    "target": "self",
    "params": { "result": "${step_3.output}", "sdg_id": "15.3.1" },
    "expected_output": "natural-language interpretation + action suggestions",
    "depends_on": [3]
  }
]
```

---

## 3. Parallel Mode

### Applicable scenario
Multiple independent sub-tasks with no interdependence; execute simultaneously and then aggregate/compare.

### Template
```
Branch A: tool_call → compute the indicator for region A
Branch B: tool_call → compute the indicator for region B
        ↓ (wait for A and B to finish)
Aggregate: interpret → compare the two regions' results
```

### Example: compare SDG 11.3.1 of Hangzhou and Shanghai
```json
[
  {
    "step": 1,
    "action": "tool_call",
    "target": "sdg-1131",
    "params": {
      "init_lucc": "/data/hangzhou/lucc_2010.tif",
      "curr_lucc": "/data/hangzhou/lucc_2020.tif",
      "init_popu": "/data/hangzhou/popu_2010.tif",
      "curr_popu": "/data/hangzhou/popu_2020.tif",
      "types": "5"
    },
    "expected_output": "Hangzhou SDG 11.3.1 score",
    "depends_on": null,
    "branch": "A"
  },
  {
    "step": 2,
    "action": "tool_call",
    "target": "sdg-1131",
    "params": {
      "init_lucc": "/data/shanghai/lucc_2010.tif",
      "curr_lucc": "/data/shanghai/lucc_2020.tif",
      "init_popu": "/data/shanghai/popu_2010.tif",
      "curr_popu": "/data/shanghai/popu_2020.tif",
      "types": "5"
    },
    "expected_output": "Shanghai SDG 11.3.1 score",
    "depends_on": null,
    "branch": "B"
  },
  {
    "step": 3,
    "action": "interpret",
    "target": "self",
    "params": {
      "result_a": "${step_1.output}",
      "result_b": "${step_2.output}",
      "compare_mode": "regional"
    },
    "expected_output": "Hangzhou vs Shanghai SDG 11.3.1 comparison analysis",
    "depends_on": [1, 2]
  }
]
```

---

## 4. Conditional Mode

### Applicable scenario
Decide the follow-up execution path based on a preceding step's result (Observation).

### Template
```
Step 1: data_check → check the data
  ├── success → Step 2a: tool_call → run the computation
  └── failure → Step 2b: ask_user → guide the user to provide data
                  └── after the user provides it → Step 2a
```

### Example: data-missing degradation strategy
```json
[
  {
    "step": 1,
    "action": "data_check",
    "target": "filesystem",
    "params": { "pattern": "*suzhou*lucc*.tif" },
    "expected_output": "Suzhou LUCC data path list",
    "depends_on": null,
    "on_success": "step_2a",
    "on_failure": "step_2b"
  },
  {
    "step": "2a",
    "action": "tool_call",
    "target": "sdg-land-conversion",
    "params": { "...": "use the data found in Step 1" },
    "depends_on": [1],
    "condition": "step_1.output.files.length > 0"
  },
  {
    "step": "2b",
    "action": "ask_user",
    "target": "user",
    "params": {
      "message": "No LUCC data found for the Suzhou region; please provide the paths of two-period land-use GeoTIFF files",
      "required": ["init_lucc", "curr_lucc"]
    },
    "depends_on": [1],
    "condition": "step_1.output.files.length == 0",
    "on_user_response": "step_2a"
  }
]
```

---

## 5. Loop Mode

### Applicable scenario
Repeat the same computation flow over multiple files or regions.

### Template
```
for each item in list:
    tool_call → run the computation → save the result
Aggregate: interpret → compare all results
```

### Example: compute SDG 11.3.1 for 3 cities one by one
```json
[
  {
    "step": 1,
    "action": "loop",
    "loop_var": "city",
    "loop_items": ["hangzhou", "shanghai", "suzhou"],
    "loop_body": [
      {
        "action": "tool_call",
        "target": "sdg-1131",
        "params": {
          "init_lucc": "/data/${city}/lucc_2010.tif",
          "curr_lucc": "/data/${city}/lucc_2020.tif",
          "init_popu": "/data/${city}/popu_2010.tif",
          "curr_popu": "/data/${city}/popu_2020.tif",
          "types": "5"
        }
      }
    ],
    "depends_on": null
  },
  {
    "step": 2,
    "action": "interpret",
    "target": "self",
    "params": { "results": "${step_1.output.all_results}" },
    "expected_output": "3-city SDG 11.3.1 comparison ranking",
    "depends_on": [1]
  }
]
```

---

## 6. Full Assessment Chain Template (Priority-area identification)

Priority-area identification is a typical linear + loop combination:

```json
[
  {
    "step": 1,
    "action": "knowledge_lookup",
    "target": "geosdg-assistant",
    "params": { "topic": "priority-area-rules" },
    "expected_output": "parameter requirements of the 6 rules",
    "depends_on": null
  },
  {
    "step": 2,
    "action": "data_check",
    "target": "filesystem",
    "params": { "required_files": ["init_lucc", "curr_lucc", "init_popu", "curr_popu", "buffer", "emission_data"] },
    "depends_on": null
  },
  {
    "step": 3,
    "action": "tool_call",
    "target": "priority-loss",
    "params": { "init_lucc": "...", "curr_lucc": "...", "output": ".../PA-1.tif", "types": "2,3" },
    "depends_on": [2]
  },
  {
    "step": 4,
    "action": "tool_call",
    "target": "priority-transition",
    "params": { "...": "..." },
    "depends_on": [2]
  },
  {
    "step": 5,
    "action": "tool_call",
    "target": "priority-buffer",
    "params": { "...": "..." },
    "depends_on": [2]
  },
  {
    "step": 6,
    "action": "tool_call",
    "target": "priority-emission",
    "params": { "...": "..." },
    "depends_on": [2]
  },
  {
    "step": 7,
    "action": "tool_call",
    "target": "priority-human-land",
    "params": { "...": "..." },
    "depends_on": [2]
  },
  {
    "step": 8,
    "action": "tool_call",
    "target": "priority-merge",
    "params": {
      "files": "${step_3.output},${step_4.output},${step_5.output},${step_6.output},${step_7.output}",
      "output": ".../RankingMap.tif"
    },
    "depends_on": [3, 4, 5, 6, 7]
  },
  {
    "step": 9,
    "action": "interpret",
    "target": "self",
    "params": { "ranking_map": "${step_8.output}" },
    "expected_output": "priority-area ranking interpretation + protection suggestions",
    "depends_on": [8]
  }
]
```

> Note: Steps 3-7 have no mutual dependency and can run in parallel during execution.

---

## 7. ReAct Loop Termination Conditions

| Condition | Threshold | Behavior |
|-----------|-----------|----------|
| Max steps | 8 steps (default) | Stop planning when exceeded; summarize the completed steps' results |
| Timeout | 600 seconds | Abort the current step on timeout; return partial results |
| Error retries | 2 times | Degrade or ask the user after a step fails 2 times |
| User interrupt | — | Stop immediately when the user cancels; save progress to Memory |
| All complete | all steps = done | End normally and summarize results |

---

## 8. Planning Decision Flow

```
Input: intent + user original query + Memory context
  │
  ├── intent = sdg-calc?
  │     ├── single indicator → linear mode (knowledge query → data check → compute → interpret)
  │     └── multi-region comparison → parallel mode (each region computes independently → aggregate and compare)
  │
  ├── intent = ca-precision?
  │     └── linear mode (data check → ca-precision → optional correlation/t-test → interpret)
  │
  ├── intent = priority?
  │     └── linear + loop mode (run rules 1-6 sequentially → merge → interpret)
  │
  ├── intent = knowledge?
  │     └── no planning (pure retrieval, return the knowledge-base content directly)
  │
  └── intent = composite?
        ├── can be decomposed into sub-intents → recursively plan each sub-intent
        └── cannot be decomposed → ask_user (guide the user to clarify the requirement)
```

---

## 9. Composite Assessment Planning Template (composite)

### Applicable scenario

When agent-router recognizes the intent as `composite`, the planner runs all SDG indicator computations and priority-area identification.

### Trigger conditions

- agent-router recognizes the intent as `composite`
- The user expresses intents such as "full assessment", "comprehensive analysis", or "do a comprehensive assessment for me"

### Planning template

```
Composite assessment planning template (computation stage, excluding report generation):

Step 1-N:   run SDG indicator computations (parallel/linear, depending on data dependencies)
Step N+1:   run priority-area identification (Rule 1-6 + merge)
Step N+2:   run priority-stats (area statistics)
```

> **Report generation is decoupled**: report generation (charts + report) is triggered independently by the `agent-report` skill,
> and is not forcibly appended to the composite planning template. After computation, the user can trigger report
> generation via intents such as `generate report` or `export report`. See `agent/skills/agent-report/`.

### Example: composite assessment of Hangzhou

```json
[
  {
    "step": 1,
    "action": "tool_call",
    "target": "sdg-land-conversion",
    "params": { "init_lucc": "...", "curr_lucc": "...", "transitions": "2:3,4:5", "positive": false },
    "expected_output": "SDG 15.3.1 land-degradation score",
    "depends_on": null
  },
  {
    "step": 2,
    "action": "tool_call",
    "target": "sdg-1131",
    "params": { "init_lucc": "...", "curr_lucc": "...", "init_popu": "...", "curr_popu": "...", "types": "5" },
    "expected_output": "SDG 11.3.1 urban-growth score",
    "depends_on": null
  },
  {
    "step": 3,
    "action": "tool_call",
    "target": "sdg-1322",
    "params": { "...": "..." },
    "expected_output": "SDG 13.2.2 carbon-peaking score",
    "depends_on": null
  },
  {
    "step": 4,
    "action": "tool_call",
    "target": "priority-merge",
    "params": { "files": "...", "output": ".../RankingMap.tif" },
    "expected_output": "priority-area ranking map",
    "depends_on": null
  },
  {
    "step": 5,
    "action": "tool_call",
    "target": "priority-stats",
    "params": { "ranking": "${step_4.output.output_file}" },
    "expected_output": "per-level area statistics",
    "depends_on": [4]
  }
]
```

> Note: Steps 1-3 have no mutual dependency and can run in parallel during execution. After computation, the user can
> trigger report generation (chart generation + template rendering + multi-format export) via the `agent-report` skill.
