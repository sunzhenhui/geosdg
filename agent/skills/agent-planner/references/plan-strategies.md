# 任务规划策略参考 / Task Planning Strategies

> 本文档定义 agent-planner 的规划模式、模板和决策规则。
> 规划器根据用户意图和可用 Tool 列表，选择合适的规划模式生成执行计划。

---

## 1. 规划模式总览

| 模式 | 适用场景 | 典型用例 | 依赖关系 |
|------|---------|---------|---------|
| **linear**（线性） | 步骤有强依赖，必须顺序执行 | 完整 SDG 评估链：知识查询 → 数据检查 → 计算 → 解读 | step N 依赖 step N-1 |
| **parallel**（并行） | 多个独立计算，互不依赖 | 比较杭州和上海的 SDG 11.3.1 | 分支间无依赖 |
| **conditional**（条件） | 根据前置结果决定后续路径 | 数据缺失 → 引导用户提供；数据齐全 → 直接计算 | 分支由 Observation 决定 |
| **loop**（循环） | 重复处理多个文件/地区 | 对 [杭州, 上海, 苏州] 逐个计算 SDG 指标 | 循环体内部为 linear |

---

## 2. Linear 模式（线性规划）

### 适用场景
步骤间有严格依赖关系，后一步需要前一步的输出作为输入。

### 模板
```
Step 1: knowledge_lookup → 查询指标定义和公式
Step 2: data_check → 检查数据是否齐全
Step 3: tool_call → 执行计算（依赖 Step 1 的参数 + Step 2 的数据）
Step 4: interpret → 解读结果（依赖 Step 3 的输出）
```

### 示例：评估杭州 SDG 15.3.1
```json
[
  {
    "step": 1,
    "action": "knowledge_lookup",
    "target": "sdg-indicator-knowledge",
    "params": { "sdg_id": "15.3.1" },
    "expected_output": "计算公式：土地转换面积比，所需参数：两期 LUCC + transitions",
    "depends_on": null
  },
  {
    "step": 2,
    "action": "data_check",
    "target": "filesystem",
    "params": { "pattern": "lucc*.tif", "directory": "用户工作区" },
    "expected_output": "可用数据路径列表",
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
    "expected_output": "土地退化得分（0-100）",
    "depends_on": [1, 2]
  },
  {
    "step": 4,
    "action": "interpret",
    "target": "self",
    "params": { "result": "${step_3.output}", "sdg_id": "15.3.1" },
    "expected_output": "自然语言解读 + 行动建议",
    "depends_on": [3]
  }
]
```

---

## 3. Parallel 模式（并行规划）

### 适用场景
多个独立子任务互不依赖，可同时执行后汇总对比。

### 模板
```
分支 A: tool_call → 计算区域 A 的指标
分支 B: tool_call → 计算区域 B 的指标
        ↓（等待 A、B 完成）
汇总: interpret → 对比两区域结果
```

### 示例：比较杭州和上海的 SDG 11.3.1
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
    "expected_output": "杭州 SDG 11.3.1 得分",
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
    "expected_output": "上海 SDG 11.3.1 得分",
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
    "expected_output": "杭州 vs 上海 SDG 11.3.1 对比分析",
    "depends_on": [1, 2]
  }
]
```

---

## 4. Conditional 模式（条件规划）

### 适用场景
根据前置步骤的结果（Observation）决定后续执行路径。

### 模板
```
Step 1: data_check → 检查数据
  ├── 成功 → Step 2a: tool_call → 执行计算
  └── 失败 → Step 2b: ask_user → 引导用户提供数据
                  └── 用户提供后 → Step 2a
```

### 示例：数据缺失降级策略
```json
[
  {
    "step": 1,
    "action": "data_check",
    "target": "filesystem",
    "params": { "pattern": "*苏州*lucc*.tif" },
    "expected_output": "苏州 LUCC 数据路径列表",
    "depends_on": null,
    "on_success": "step_2a",
    "on_failure": "step_2b"
  },
  {
    "step": "2a",
    "action": "tool_call",
    "target": "sdg-land-conversion",
    "params": { "...": "使用 Step 1 找到的数据" },
    "depends_on": [1],
    "condition": "step_1.output.files.length > 0"
  },
  {
    "step": "2b",
    "action": "ask_user",
    "target": "user",
    "params": {
      "message": "未找到苏州地区的 LUCC 数据，请提供两期土地利用 GeoTIFF 文件路径",
      "required": ["init_lucc", "curr_lucc"]
    },
    "depends_on": [1],
    "condition": "step_1.output.files.length == 0",
    "on_user_response": "step_2a"
  }
]
```

---

## 5. Loop 模式（循环规划）

### 适用场景
对多个文件或地区重复执行相同的计算流程。

### 模板
```
for each item in list:
    tool_call → 执行计算 → 保存结果
汇总: interpret → 对比所有结果
```

### 示例：对 3 个城市逐个计算 SDG 11.3.1
```json
[
  {
    "step": 1,
    "action": "loop",
    "loop_var": "city",
    "loop_items": ["杭州", "上海", "苏州"],
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
    "expected_output": "三城市 SDG 11.3.1 对比排名",
    "depends_on": [1]
  }
]
```

---

## 6. 完整评估链模板（Priority 区域识别）

优先区域识别是典型的线性+循环组合：

```json
[
  {
    "step": 1,
    "action": "knowledge_lookup",
    "target": "geosdg-assistant",
    "params": { "topic": "priority-area-rules" },
    "expected_output": "6 条规则的参数需求",
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
    "expected_output": "优先区域排名解读 + 保护建议",
    "depends_on": [8]
  }
]
```

> 注意：Step 3-7 之间无依赖，实际执行时可并行。

---

## 7. ReAct 循环终止条件

| 条件 | 阈值 | 行为 |
|------|------|------|
| 最大步数 | 8 步（默认） | 超出则停止规划，已完成的步骤结果汇总返回 |
| 超时 | 600 秒 | 超时则中止当前步骤，返回部分结果 |
| 错误重试 | 2 次 | 同一步骤失败 2 次后降级或询问用户 |
| 用户中断 | — | 用户取消时立即停止，保存当前进度到 Memory |
| 全部完成 | 所有 step 状态 = done | 正常结束，汇总结果 |

---

## 8. 规划决策流程

```
输入：意图 + 用户原始查询 + Memory 上下文
  │
  ├── 意图 = sdg-calc？
  │     ├── 单一指标 → linear 模式（知识查询 → 数据检查 → 计算 → 解读）
  │     └── 多区域对比 → parallel 模式（各区域独立计算 → 汇总对比）
  │
  ├── 意图 = ca-precision？
  │     └── linear 模式（数据检查 → ca-precision → 可选 correlation/t-test → 解读）
  │
  ├── 意图 = priority？
  │     └── linear + loop 模式（规则 1-6 逐个执行 → merge → 解读）
  │
  ├── 意图 = knowledge？
  │     └── 无规划（纯检索，直接返回知识库内容）
  │
  └── 意图 = composite？
        ├── 能拆解为子意图 → 递归规划各子意图
        └── 无法拆解 → ask_user（引导用户澄清需求）
```

---

## 9. 综合评估规划模板（composite）

### 适用场景

当 agent-router 识别意图为 `composite`（综合评估）时，规划器执行所有 SDG 指标计算和优先区域识别。

### 触发条件

- agent-router 识别意图为 `composite`
- 用户表达"完整评估""综合分析""帮我做全面评估"等意图

### 规划模板

```
综合评估规划模板（计算阶段，不含报告生成）：

Step 1-N:   执行 SDG 指标计算（并行/线性，取决于数据依赖）
Step N+1:   执行优先区域识别（Rule 1-6 + merge）
Step N+2:   执行 priority-stats（面积统计）
```

> **报告生成已解耦**：报告生成（图表 + 报告）由 `agent-report` skill 独立触发，
> 不在 composite 规划模板中强制追加。用户可在计算完成后通过 `生成报告`、
> `导出报告` 等意图触发报告生成流程。详见 `agent/skills/agent-report/`。

### 示例：杭州综合评估

```json
[
  {
    "step": 1,
    "action": "tool_call",
    "target": "sdg-land-conversion",
    "params": { "init_lucc": "...", "curr_lucc": "...", "transitions": "2:3,4:5", "positive": false },
    "expected_output": "SDG 15.3.1 土地退化得分",
    "depends_on": null
  },
  {
    "step": 2,
    "action": "tool_call",
    "target": "sdg-1131",
    "params": { "init_lucc": "...", "curr_lucc": "...", "init_popu": "...", "curr_popu": "...", "types": "5" },
    "expected_output": "SDG 11.3.1 城市增长得分",
    "depends_on": null
  },
  {
    "step": 3,
    "action": "tool_call",
    "target": "sdg-1322",
    "params": { "...": "..." },
    "expected_output": "SDG 13.2.2 碳排放达峰得分",
    "depends_on": null
  },
  {
    "step": 4,
    "action": "tool_call",
    "target": "priority-merge",
    "params": { "files": "...", "output": ".../RankingMap.tif" },
    "expected_output": "优先区域排名图",
    "depends_on": null
  },
  {
    "step": 5,
    "action": "tool_call",
    "target": "priority-stats",
    "params": { "ranking": "${step_4.output.output_file}" },
    "expected_output": "各等级面积统计数据",
    "depends_on": [4]
  }
]
```

> 注意：Step 1-3 之间无依赖，实际执行时可并行。计算完成后，用户可通过
> `agent-report` skill 触发报告生成（图表生成 + 模板渲染 + 多格式导出）。
