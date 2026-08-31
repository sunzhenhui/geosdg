# 对话样例 03 — 情景对比（Scenario）

> **评估维度**：未来（情景推演） | **能力类别**：reasoning
> **PEACE 对标**：❌ **PEACE 做不了** —— 地质图没有"情景"概念
>
> **本样例目的**：演示 GeoSDG-Agent **最独特的资产** —— SSP1-5 五套 CA 模拟数据的"what-if"推演能力。
> **执行成本**：首次 60 秒（跑 2 情景 × 3 指标）；追问后续情景 3 秒（增量执行）。

---

## 🎬 场景

小李已经知道过去 10 年在恶化。领导追问："那如果不管，2050 年会怎样？"

---

## 💬 对话

### Turn 1

**👤 小李**：
> 如果按 SSP2（中速发展）和 SSP3（区域竞争）两种情景走，2050 年 SDG 11.3.1 会分别变成什么样？

**🤖 GeoSDG Copilot**：

🔮 **2050 年 SDG 11.3.1 情景推演**

```
情景                LCR/PGR       vs 2020(1.8)    判定
──────────────────────────────────────────────────────
2020（历史基线）    1.8            —              ⚠️  当前警戒
SSP2 中速发展       2.1            +0.3           ⚠️  持续恶化但可控
SSP3 区域竞争       2.9            +1.1           ❌  严重恶化（超出所有历史阈值）
──────────────────────────────────────────────────────
差距：SSP3 比 SSP2 恶化 38%
```

**核心结论**：

> **两个情景差异悬殊，且拐点决定命运**：
> - **SSP2 下**：仍在恶化，但斜率放缓（10 年 +0.3 vs 过去 10 年 +0.6），说明"中速发展 + 常规治理"能踩下刹车
> - **SSP3 下**：斜率反而加速（10 年 +1.1），说明"区域竞争 + 无序扩张"会突破治理能力
> - **关键洞察**：SSP3 的恶化 **不是均匀分布**——差异 90% 来自 A 区东南象限的失控扩张（见空间对比图）

📊 **空间对比可视化**：
- `data/outputs/sdg1131_2050_ssp2.png` — SSP2 恶化区（分散、可控）
- `data/outputs/sdg1131_2050_ssp3.png` — SSP3 恶化区（集中在 A 区东南，形成连片风险）
- `data/outputs/sdg1131_ssp2_vs_ssp3_diff.png` — 差异图（哪里差异最大）

**给规划的建议**：

> **即便无法完全避开 SSP3 路径，也应在 A 区东南象限（45.2 km²）提前划定城市增长边界或生态红线，把 SSP3 情景的最坏影响锁定在 SSP2 水平内。**

<details>
<summary>📎 证据链（5 条）</summary>

```json
{
  "confidence": "medium",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "sdg-1131",
      "params": {"lucc_t0": "lucc/2020.tif", "lucc_t1": "future/ssp2/lucc_2050.tif",
                 "pop_t0": "pop/2020.tif", "pop_t1": "future/ssp2/pop_2050.tif"},
      "result_file": "data/outputs/sdg1131_2050_ssp2.tif",
      "key_metrics": {"LCR_over_PGR": 2.13, "urban_area_km2": 312.5},
      "reproduce_cmd": "geosdg-cli sdg-1131 --lucc-t0 data/rasters/lucc/2020.tif --lucc-t1 data/rasters/future/ssp2/lucc_2050.tif --pop-t0 data/rasters/pop/2020.tif --pop-t1 data/rasters/future/ssp2/pop_2050.tif -o data/outputs/sdg1131_2050_ssp2.tif"
    },
    {
      "type": "cli_output",
      "tool": "sdg-1131",
      "params": {"scenario": "ssp3", "target_year": 2050},
      "result_file": "data/outputs/sdg1131_2050_ssp3.tif",
      "key_metrics": {"LCR_over_PGR": 2.91, "urban_area_km2": 478.9}
    },
    {
      "type": "cli_output",
      "tool": "priority-human-land",
      "params": {"scenario": "ssp3"},
      "result_file": "data/outputs/priority_1131_ssp3_2050.tif",
      "key_metrics": {"A_zone_contribution_to_worsening": 0.90, "hotspot_area_km2": 45.2}
    },
    {
      "type": "knowledge",
      "source": "IPCC SSP Database (Riahi et al. 2017)",
      "quote": "SSP2 represents a 'middle of the road' scenario; SSP3 represents 'regional rivalry' with slow economic growth and high inequality.",
      "url_or_path": "agent/knowledge/policy/ipcc_ssp_scenarios.md"
    },
    {
      "type": "expert_opinion",
      "expert": "land_use_expert",
      "stance": "SSP3 情景下 A 区东南失控是主要风险源，建议提前预警",
      "confidence": "high",
      "supporting_evidence_ids": ["ev_ssp3_run", "ev_a_zone_hotspot"]
    }
  ],
  "conflicts": [],
  "action_items": [
    {
      "priority": "P0",
      "action": "在 A 区东南象限（45.2 km²）划定城市增长边界，作为 SSP3 情景的兜底防线",
      "target_sdg": ["11.3.1"],
      "expected_impact": "预计可将 SSP3 情景下 LCR/PGR 从 2.9 拉回到 2.3 附近，接近 SSP2 水平"
    }
  ],
  "caveats": [
    "SSP2/SSP3 情景来自 CA 模拟外推，2050 年边界含约 15% 不确定性",
    "未考虑政策干预情景（如强制城市边界）；干预情景需另行建模",
    "'A 区东南象限'为分析用命名，非行政边界"
  ]
}
```

</details>

---

### Turn 2（What-if 追问 - 增量执行）

**👤 小李**：
> 那 SSP1（可持续发展）呢？如果我们走得比较理想，能拉回到什么水平？

**🤖 GeoSDG Copilot**：

⚡ 检测到增量追问，只重跑 SSP1 情景（Turn 1 的其他证据全部复用）。

```
情景                LCR/PGR       vs 2020(1.8)    判定
──────────────────────────────────────────────────────
SSP1 可持续         1.3            -0.5           ✅  显著改善
SSP2 中速发展       2.1            +0.3           ⚠️  持续恶化
SSP3 区域竞争       2.9            +1.1           ❌  严重恶化
──────────────────────────────────────────────────────
```

**关键洞察**：

> **SSP1 是唯一能让指标 U 型反转的情景** —— 从 2020 年的 1.8 一路改善到 1.3，接近 UN 建议的"人地匹配"基准（1.0）。
>
> **前提条件**（来自 SSP1 情景设定）：
> - 人口向紧凑城市集中（避免摊大饼）
> - 土地节约集约使用（存量更新为主，新增开发受严格约束）
> - 强绿色基础设施投入
>
> **政策抓手**：这三条基本对应我国"十四五规划"中的"节约集约用地"和"存量更新"导向。

> ⚡ 本次调用命中增量缓存：SSP2/SSP3 结果复用，只新跑 SSP1，耗时 3 秒。

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "medium",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "sdg-1131",
      "params": {"scenario": "ssp1", "target_year": 2050},
      "result_file": "data/outputs/sdg1131_2050_ssp1.tif",
      "key_metrics": {"LCR_over_PGR": 1.28}
    },
    {
      "type": "knowledge",
      "source": "IPCC SSP1 Narrative (O'Neill et al. 2017)",
      "quote": "SSP1 emphasizes sustainability, compact urban form, and reduced material intensity.",
      "url_or_path": "agent/knowledge/policy/ipcc_ssp_scenarios.md"
    },
    {
      "type": "knowledge",
      "source": "国家十四五规划纲要 §33",
      "quote": "推进以人为核心的新型城镇化，坚持节约集约用地，严守耕地和生态保护红线。",
      "url_or_path": "agent/knowledge/policy/china_14th_5yr_plan.md"
    }
  ],
  "caveats": ["SSP1 假设条件较理想，实际达成需强政策约束"],
  "session": {
    "session_id": "demo_20260831_001",
    "turn": 5,
    "cached_evidence_ids": ["ev_ssp2_run", "ev_ssp3_run", "ev_a_zone_hotspot"]
  }
}
```

</details>

---

### Turn 3（跨情景 + 跨指标）

**👤 小李**：
> 如果 SSP1 能把 11.3.1 拉回来，那对 13.2.2（碳排放）也是好事吧？会不会有反效果？

**🤖 GeoSDG Copilot**：

🔍 好问题——这是**目标间协同/权衡**问题。让我调 land_use_expert 和 climate_expert 协同回答。

**结论：SSP1 情景下，SDG 11.3.1 和 13.2.2 呈现强协同**

```
情景        SDG 11.3.1(LCR/PGR)   SDG 13.2.2(万tCO2eq/y)   协同性
─────────────────────────────────────────────────────────────
2020        1.8                    18.7                     —
SSP1 2050   1.3   (↓ 改善)         14.2  (↓ 改善)           ✅ 强协同
SSP2 2050   2.1   (↑ 恶化)         26.4  (↑ 恶化)           ⚠️ 强共变
SSP3 2050   2.9   (↑ 严重恶化)      41.8  (↑ 严重恶化)       ❌ 强共变
─────────────────────────────────────────────────────────────
关联系数：ρ(LCR/PGR, 排放) = 0.94（三情景样本）
```

**land_use_expert 观点**（high confidence）：
> "土地扩张速度是碳排放的主要驱动。SSP1 通过紧凑城市把土地扩张压下来，同时压下了 LUCC 转化贡献的排放。这是**同源同解**问题。"

**climate_expert 观点**（medium confidence）：
> "同意 land_use 的判断，但要提示：SDG 13.2.2 只计算了 **土地转化贡献** 的排放，未计入建筑运行、交通排放。若纳入这些，SSP1 的碳减排效果可能被高估——因为 SSP1 通常伴随更高的能耗水平提升。"

**冲突暴露**：两位专家在"SSP1 碳减排幅度"上存在方法论分歧，但对"协同方向"一致。

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "medium",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "sdg-1322",
      "params": {"scenario": "ssp1", "target_year": 2050},
      "result_file": "data/outputs/sdg1322_2050_ssp1.tif",
      "key_metrics": {"emission_2050": 142000, "delta_vs_2020_pct": -24.1}
    },
    {
      "type": "expert_opinion",
      "expert": "land_use_expert",
      "stance": "11.3.1 与 13.2.2 强协同，同源同解",
      "confidence": "high"
    },
    {
      "type": "expert_opinion",
      "expert": "climate_expert",
      "stance": "协同方向对，但幅度可能被高估，因未计入建筑与交通排放",
      "confidence": "medium"
    }
  ],
  "conflicts": [
    {
      "between": ["land_use_expert", "climate_expert"],
      "topic": "SSP1 情景下碳减排幅度的估算方法",
      "resolution": "conditional",
      "note": "两位专家在方向上一致，在幅度上有分歧。用户可根据是否需要考虑建筑/交通排放选择相应结论。"
    }
  ],
  "caveats": [
    "SDG 13.2.2 计算仅覆盖 LUCC 转化贡献，未含能源部门排放",
    "ρ=0.94 仅基于 3 个情景样本，统计显著性有限"
  ]
}
```

</details>

---

## 🎯 本样例的关键设计点

| 设计点 | 说明 | PEACE 对照 |
|-------|------|-----------|
| **SSP1-5 情景推演** | GeoSDG 独有资产的核心用法 | ❌ PEACE 无此维度 |
| **U 型反转分析** | 不只是"变差"，还能量化"最好能拉回多少" | ✅ GeoSDG 决策级洞察 |
| **增量执行** | Turn 2 只跑 SSP1，节省 30 秒 | ❌ PEACE 无 |
| **跨指标协同** | Turn 3 引出**目标间**问题的自然过渡 | 引出 conversation-04 |
| **冲突不藏起来** | 两位专家分歧明确暴露 | ✅ GeoSDG 独有 |
| **政策抓手** | 把 SSP1 的假设翻译成具体政策条款 | ✅ GeoSDG 决策导向 |

## 🔗 下一步

→ [`conversation-04-tradeoff.md`](./conversation-04-tradeoff.md) 演示**目标间**：多 SDG 协同/权衡矩阵。
