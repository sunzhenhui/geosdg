# GeoSDG-Agent 决策 Benchmark

评测 GeoSDG-Agent **决策会诊质量**的可运行评测集与打分器。

> 注意：这套 benchmark 评的是"Agent 给的决策建议好不好"，**不是**"做题对不对"。
> 它与主流程一样，围绕研究区 → 分区 → 指标 → 决策建议展开。

## 评什么：三个质量维度

| 维度 | 权重 | 含义 | 打分方式 |
|------|------|------|---------|
| **accuracy**（决策命中） | 0.4 | `answer` 是否覆盖标准答案的关键结论（分区名/布尔/关键词） | 关键词命中比例；无关键词约束时只要非空即满分 |
| **evidence**（证据覆盖） | 0.4 | `evidence` 是否引用了 gold 要求的分区/指标，且 `confidence` 达标 | 分区覆盖率、指标覆盖率、置信度是否达下限，取均值 |
| **consistency**（结论一致） | 0.2 | 同一任务多次会诊结论是否稳定 | `answer` 众数占比（需 `--runs > 1`） |

总分 = 三维加权平均。**证据覆盖权重最高**——决策 Copilot 的核心是"有据可依"。

## 目录结构

```
benchmark/
├── cases/                     # 评测样例（研究区 + 决策任务 + 标准答案 gold）
│   ├── case_001_priority_area.json      # ranking：优先干预区排序
│   ├── case_002_indicator_value.json    # point_estimate：现状值诊断
│   └── case_003_policy_conflict.json    # boolean_judgment：政策冲突判定
├── scorer.py                  # 三维打分器
├── run_benchmark.py           # 跑批器
└── README.md
```

## 怎么跑

```bash
# 跑全部 case（每个 1 次）
python agent/geosdg-agent/benchmark/run_benchmark.py

# 每个 case 跑 3 次，测决策一致性
python agent/geosdg-agent/benchmark/run_benchmark.py --runs 3
```

输出示例：

```
[case_001_priority_area] total=0.2
    accuracy     0.000  (hit 0/2: [])
    evidence     0.000  (partitions 0%; indicators 0%; conf 0.50>=0.60:N)
    consistency  1.000  (mode 2/2)
...
SUMMARY  cases=3  overall=0.4
  by dimension: {'accuracy': 0.33, 'evidence': 0.17, 'consistency': 1.0}
```

> 骨架版专家返回 mock（`conf=0.50`、空 evidence），因此 accuracy/evidence 偏低是**正常**的——
> 这恰好说明打分器在真实生效。后续给专家填真实逻辑后，分数会随之上升。

## 一个 case 长什么样

```json
{
  "id": "case_001_priority_area",
  "description": "优先干预区排序：城镇分区 SDG 11.3.1 失衡最严重",
  "region": { "name": "...", "bbox": [...], "lucc_path": "...", "year": 2020 },
  "task": "在当前分区中，哪几个应被列为优先干预区？",
  "task_type": "analyzing-priority_area",
  "gold": {
    "answer_keywords": ["A1", "A3"],
    "must_cite_partitions": ["A1"],
    "must_cite_indicators": ["SDG_11_3_1"],
    "expected_un_status": "critical",
    "min_confidence": 0.6
  }
}
```

## 加新 case

1. 在 `cases/` 下新增一个 `*.json`，字段同上
2. `task_type` 必须是 `utils/prompt.py::task_ability2type` 里的合法 key
3. 直接重跑 `run_benchmark.py` 即可，无需改代码
