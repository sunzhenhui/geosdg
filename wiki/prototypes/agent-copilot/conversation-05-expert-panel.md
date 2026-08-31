# 对话样例 05 — 多专家会诊（Analyzing）

> **评估维度**：综合决策（跨四维） | **能力类别**：analyzing
> **PEACE 对标**：3 专家 → 5 专家 + **冲突暴露**（GeoSDG 独有）
>
> **本样例目的**：演示 GeoSDG-Agent 的**最高价值场景** —— 一句话触发全面评估，产出可交付的决策报告。
> **执行成本**：首次 2-3 分钟（跑 5 专家全套）；session 复用后 30 秒可迭代。

---

## 🎬 场景

领导拍板："下周三给市长汇报，你把这个研究区的可持续发展态势讲清楚。"
小李没时间一步步问了，直接甩给 Copilot 一句话。

---

## 💬 对话

### Turn 1（一句话触发全评估）

**👤 小李**：
> 帮我评估 demo 研究区 2050 年 SSP2 情景下的可持续发展态势，下周三要给市长汇报。

**🤖 GeoSDG Copilot**：

📋 收到综合评估请求。识别为 **analyzing 类问题**，将启动**五专家会诊**流程。

```
[00:00] router: analyzing | sdg_targets=all_ready | scenario=ssp2 | year=2050
[00:01] planner: 分派任务给 5 位专家
[00:02] land_use_expert   [▶ 运行中]
[00:02] population_expert [▶ 运行中]     ┐
[00:02] infra_expert      [▶ 运行中]     ├─ 并行执行
[00:02] climate_expert    [▶ 运行中]     ┘
[01:47] 4 位专家完成
[01:48] policy_expert [▶ 综合裁决 + 冲突识别]
[02:31] ✅ 完成
```

---

## 📊 五专家会诊报告

### 🌳 land_use_expert（土地利用专家）

**关注 SDG**：11.3.1, 15.3.1
**核心结论**（`confidence: high`）：

> **SSP2 情景下研究区呈"扩张型城镇化"特征，2050 年 SDG 11.3.1 恶化至 2.1，需在 A 区东南象限（45.2 km²）划定城市增长边界。**

关键数据：
- LCR/PGR：1.8 → 2.1（继续恶化，仍在警戒区内）
- 城市用地扩张：225.9 km² → 312.5 km²（+38%）
- 主要转化源：cropland（-52 km²）、grassland（-24 km²）

---

### 👥 population_expert（人口专家）

**关注 SDG**：1, 10, 11 人地关系
**核心结论**（`confidence: medium`）：

> **人口增长（+18%）显著低于土地扩张（+38%），呈现"造城不聚人"风险，可能出现局部空城。**

关键数据：
- 总人口：1.2M → 1.42M（+18%）
- 新增城市用地人口密度：仅 1200 人/km²（对比现状 5400 人/km²）
- 新城区人口密度分布 p95：3800 人/km²，远低于健康城市门槛（≥6000 人/km²）

⚠️ 提示：**A 区东南象限新增的 45 km² 城市用地，2050 年预测入住人口仅 8 万，形成潜在空城**

---

### 🏥 infra_expert（基础设施专家）

**关注 SDG**：3.8, 4.a, 6.1, 9.c, 11.2, 11.7
**核心结论**（`confidence: high`）：

> **公共服务设施覆盖持续改善（3/4 项 SDG 上升），但空间分布不均——中心城区过剩，A 区新城严重不足。**

关键数据：

| SDG | 2020 | 2050 (SSP2) | 变化 | 空间不均等 |
|-----|------|------------|------|-----------|
| 3.8.1 医疗可及 | 76% | 84% | ↑ | ⚠️ A 区仅 45% |
| 4.a.1 教育可及 | 78% | 85% | ↑ | ⚠️ A 区仅 51% |
| 11.2.1 公交可及 | 71% | 88% | ↑ | ✅ 覆盖较均衡 |
| 11.7.1 公共空间 | 41% | 48% | ↑ | ❌ 仍低于 UN 建议 50% |

---

### 🌍 climate_expert（气候专家）

**关注 SDG**：13.2.2
**核心结论**（`confidence: medium`）：

> **LUCC 转化贡献的碳排放 2050 年将达 26.4 万 tCO2eq/年（较 2020 年 +41%），主要来自 cropland/forest → urban 的转化。**

⚠️ **专家分歧点**：
> "本估算仅覆盖 **土地转化贡献**，未计入建筑运行 + 交通排放。若纳入这两部分（估算 3-5 倍），研究区实际总排放的增幅可能达 +80% ~ +120%。**建议在给市长汇报中明确说明本次评估的边界。**"

关键数据：
- 2020 年 LUCC 贡献排放：18.7 万 tCO2eq
- 2050 年 SSP2：26.4 万 tCO2eq（+41%）
- 排放热点：A 区东南 + B 区北部（转化最活跃）

---

### 📜 policy_expert（政策综合专家）

**关注**：综合裁决 + 报告生成
**核心结论**（`confidence: high`）：

> **五位专家意见汇总——研究区面临"三高两低"结构性挑战：**
> - **三高**：土地扩张高、碳排放高、空间不均等高
> - **两低**：新城人口密度低、公共空间覆盖低

---

## 🤝 专家共识（4/5 一致）

| # | 共识内容 | 支持专家 |
|---|---------|---------|
| 1 | A 区东南象限（45.2 km²）是**最大风险源**，需 P0 干预 | 4/5（除 infra） |
| 2 | 应在 SSP2 路径基础上叠加**紧凑城市**政策，向 SSP1 靠拢 | 5/5 |
| 3 | 存量更新优于增量扩张 | 5/5 |

## ⚠️ 专家分歧（明确暴露）

| # | 分歧内容 | 分歧方 | 处理建议 |
|---|---------|-------|---------|
| 1 | 碳排放边界（仅 LUCC vs 含建筑/交通） | climate_expert vs 其他 | ✅ 汇报时**明确说明本次评估边界**，另立后续任务扩展 |
| 2 | A 区东南是"预留发展空间"还是"空城风险" | land_use vs population | ⚠️ 需**人工政策判断**，Copilot 无法替决策 |

---

## 🎯 决策级行动清单（可直接抄进汇报稿）

| 优先级 | 行动 | 目标 SDG | 预期影响 | 责任建议 |
|:------:|-----|---------|---------|---------|
| **P0** | 在 A 区东南 45.2 km² 划定城市增长边界 | 11.3.1, 13.2.2, 15.3.1 | 抑制最大风险源，多目标共赢 | 自然资源局 |
| **P0** | 在 B 区北部 12.3 km² 建生态廊道综合体 | 11.7.1, 13.2.2, 15.3.1 | 4 项 SDG 同步改善 | 规自局 + 园林局 |
| **P1** | 优化 A 区新城人口导入策略（TOD + 政策洼地） | 人地匹配 | 缓解"造城不聚人" | 发改委 + 规自局 |
| **P1** | 在 E 区推进老工业区 TOD 综合更新 | 3.8, 4.a, 11.2 | 3 项 SDG 同步改善 | 发改委 |
| **P2** | 补齐能源部门排放数据，重跑 SDG 13.2.2 完整版 | 13.2.2 | 消除评估边界分歧 | 生态环境局 |

---

## 📎 交付物清单（自动生成）

- 📊 `data/outputs/one_pager_ssp2_2050.pdf` — **决策一页纸**（给市长）
- 📈 `data/outputs/radar_chart_ssp2_2050.png` — 5 项 SDG 雷达图
- 🗺 `data/outputs/priority_map_ssp2_2050.png` — 优先干预空间图
- 📝 `data/outputs/executive_summary_ssp2_2050.md` — **汇报稿初稿**（3000 字，含图）
- 📋 `data/outputs/full_report_ssp2_2050.pdf` — 完整评估报告（15 页）
- 🧾 `data/outputs/evidence_chain_ssp2_2050.json` — 全部证据链（可追溯）

---

<details>
<summary>📎 完整证据对象（符合 evidence-schema.json）</summary>

```json
{
  "question": "帮我评估 demo 研究区 2050 年 SSP2 情景下的可持续发展态势",
  "intent": {
    "sdg_targets": ["11.3.1", "11.7.1", "11.2.1", "13.2.2", "15.3.1", "3.8.1", "4.a.1"],
    "time_axis": "cross",
    "scenario": "ssp2",
    "ability": "analyzing"
  },
  "answer": "研究区在 SSP2 情景下面临'三高两低'结构性挑战，A 区东南象限是最大风险源，建议 P0 干预。...（详见上方报告）",
  "confidence": "medium",
  "evidence": [
    {"type": "expert_opinion", "expert": "land_use_expert", "stance": "...", "confidence": "high"},
    {"type": "expert_opinion", "expert": "population_expert", "stance": "...", "confidence": "medium"},
    {"type": "expert_opinion", "expert": "infra_expert", "stance": "...", "confidence": "high"},
    {"type": "expert_opinion", "expert": "climate_expert", "stance": "...", "confidence": "medium"},
    {"type": "expert_opinion", "expert": "policy_expert", "stance": "...", "confidence": "high"},
    {"type": "cli_output", "tool": "sdg-1131", "result_file": "data/outputs/sdg1131_2050_ssp2.tif"},
    {"type": "cli_output", "tool": "sdg-1322", "result_file": "data/outputs/sdg1322_2050_ssp2.tif"},
    {"type": "cli_output", "tool": "sdg-1171", "result_file": "data/outputs/sdg1171_2050_ssp2.tif"},
    {"type": "cli_output", "tool": "priority-merge", "result_file": "data/outputs/priority_multi_sdg_ssp2.tif"}
  ],
  "conflicts": [
    {
      "between": ["climate_expert", "land_use_expert"],
      "topic": "碳排放核算边界（仅 LUCC vs 含建筑/交通）",
      "resolution": "escalate_to_human",
      "note": "建议汇报中明确评估边界"
    },
    {
      "between": ["land_use_expert", "population_expert"],
      "topic": "A 区东南是'预留发展空间'还是'空城风险'",
      "resolution": "escalate_to_human",
      "note": "涉及价值判断，需政策决策"
    }
  ],
  "visualizations": [
    "data/outputs/one_pager_ssp2_2050.pdf",
    "data/outputs/radar_chart_ssp2_2050.png",
    "data/outputs/priority_map_ssp2_2050.png"
  ],
  "action_items": [
    {"priority": "P0", "action": "在 A 区东南 45.2 km² 划定城市增长边界", "target_sdg": ["11.3.1", "13.2.2", "15.3.1"], "expected_impact": "抑制最大风险源，多目标共赢"},
    {"priority": "P0", "action": "在 B 区北部 12.3 km² 建生态廊道综合体", "target_sdg": ["11.7.1", "13.2.2", "15.3.1"], "expected_impact": "4 项 SDG 同步改善"},
    {"priority": "P1", "action": "优化 A 区新城人口导入策略", "target_sdg": ["11.3.1"], "expected_impact": "缓解'造城不聚人'"},
    {"priority": "P1", "action": "在 E 区推进老工业区 TOD 综合更新", "target_sdg": ["3.8.1", "4.a.1", "11.2.1"], "expected_impact": "3 项 SDG 同步改善"},
    {"priority": "P2", "action": "补齐能源部门排放数据，重跑 SDG 13.2.2 完整版", "target_sdg": ["13.2.2"], "expected_impact": "消除评估边界分歧"}
  ],
  "caveats": [
    "SSP2 情景来自 CA 模拟外推，2050 年边界含 15% 不确定性",
    "SDG 13.2.2 仅覆盖 LUCC 转化贡献，未含建筑/交通排放",
    "SDG 15.3.1 就绪度不足，本次以 NDVI 代理估算",
    "'A/B/E 区'为分析用命名，非行政边界，交付前需与规划分区对齐",
    "冲突项 #2 涉及价值判断，Copilot 提供依据但不代替决策"
  ],
  "session": {"session_id": "demo_20260831_001", "turn": 1, "cached_evidence_ids": []}
}
```

</details>

---

### Turn 2（What-if 追问）

**👤 小李**：
> 如果我强推 SSP1 情景配套政策，市长会看到什么不同？

**🤖 GeoSDG Copilot**：

⚡ 增量执行：复用 Turn 1 的专家团判断框架，只重跑 SSP1 数据。耗时 30 秒。

**关键差异表（SSP1 vs SSP2 @ 2050）**：

```
指标                     SSP2       SSP1      差距         谁最受益
──────────────────────────────────────────────────────────────
SDG 11.3.1 (LCR/PGR)     2.1        1.3       -0.8 ✅✅   土地治理
SDG 13.2.2 (万tCO2eq)    26.4       14.2      -12.2 ✅✅  气候
SDG 15.3.1 (退化面积)    +8%        -3%       -11pp ✅✅  生态
SDG 11.7.1 (公共空间)    48%        52%       +4pp ✅    宜居
SDG 3.8.1 (医疗)         84%        86%       +2pp ✅    健康
SDG 4.a.1 (教育)         85%        87%       +2pp ✅    教育
SDG 11.2.1 (公交)        88%        84%       -4pp ⚠️    交通
──────────────────────────────────────────────────────────────
```

**给市长可用的对比一句话**：

> **"走 SSP1 路径，7 项 SDG 中 6 项改善、1 项略降；土地/碳/生态三项显著改善（-0.8 / -12万吨 / -11pp）。**
> **代价：公交可及性下降 4pp（因紧凑城市减少远距离交通投资）。**
> **净收益判断：值。"**

📎 更新的交付物：
- `data/outputs/one_pager_ssp1_vs_ssp2.pdf` — SSP1/SSP2 对比一页纸
- `data/outputs/what_if_ssp1_2050.md` — What-if 简报

<details>
<summary>📎 证据链（增量）</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {"type": "cli_output", "tool": "sdg-1131", "params": {"scenario": "ssp1"}, "key_metrics": {"LCR_over_PGR": 1.28}},
    {"type": "cli_output", "tool": "sdg-1322", "params": {"scenario": "ssp1"}, "key_metrics": {"emission": 142000}}
  ],
  "session": {
    "session_id": "demo_20260831_001",
    "turn": 2,
    "cached_evidence_ids": ["ev_ssp2_full_panel", "ev_expert_frame"]
  }
}
```

</details>

---

## 🎯 本样例的关键设计点（也是整个原型的高潮）

| 设计点 | 说明 | PEACE 对照 |
|-------|------|-----------|
| **一句话触发全评估** | 用户不用一步步问，直接甩需求 | ✅ 决策 Copilot 的标志能力 |
| **5 专家并行 + 综合裁决** | 对标 PEACE 3 专家，覆盖更广的 SDG 视角 | PEACE 3 专家 |
| **共识/分歧显式暴露** | 4/5 一致 vs 明确的冲突项 | ✅ **GeoSDG 独有** |
| **P0/P1/P2 行动清单** | 每项标注责任部门，可执行 | ✅ 决策级输出 |
| **6 类交付物一次产出** | 一页纸/雷达图/空间图/汇报稿/完整报告/证据链 | ✅ 直接可交付 |
| **What-if 增量执行** | Turn 2 复用 Turn 1 的专家框架，30 秒出对比 | ❌ PEACE 无 |
| **对边界诚实** | climate_expert 主动声明"未含建筑/交通排放" | ✅ **建立信任的关键** |

---

## 🏆 五段对话样例总结

|# | 样例 | 维度 | PEACE 能否做 | 独特价值 |
|--|-----|------|-------------|---------|
| 01 | Extracting | 现在 | ✅（弱） | 零算力秒回，引导追问 |
| 02 | Longitudinal | 过去 | ❌ | 双时相趋势 |
| 03 | Scenario | 未来 | ❌ | SSP1-5 五套 CA 数据 |
| 04 | Trade-off | 目标间 | ❌ | SDG 协同/权衡矩阵 |
| **05** | **Panel** | **综合** | ⚠️（PEACE 有 3 专家但无冲突） | **一句话产出可交付决策报告** |

**总结**：GeoSDG-Agent 不是把 CLI 套壳，而是把"研究区"变成"可对话、可决策、可追溯"的活对象。这是 PEACE 的"读图"永远到不了的价值层。
