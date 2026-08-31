# 对话样例 02 — 纵向对比（Longitudinal）

> **评估维度**：过去（时间趋势） | **能力类别**：reasoning
> **PEACE 对标**：❌ **PEACE 做不了** —— 一张地质图没有"过去 10 年"这个维度
>
> **本样例目的**：演示 GeoSDG-Agent 独有的**双时相趋势对比**能力，回答"这里正在变好还是变差"。
> **执行成本**：中等，触发 `sdg-land-proportion` × 2 时相 + `sdg-1131` 一次，约 30 秒。

---

## 🎬 场景

小李看完基线，接到领导追问："这地方是不是这几年变得越来越糟？"

---

## 💬 对话

### Turn 1

**👤 小李**：
> 过去 10 年（2010→2020）这个研究区的 SDG 表现是什么趋势？

**🤖 GeoSDG Copilot**：

📊 **研究区 2010→2020 SDG 十年趋势总览**

```
指标              2010      2020      变化         判定
─────────────────────────────────────────────────────────
SDG 11.3.1       1.2       1.8       ↑ +0.6      ⚠️  恶化（越过 UN 警戒 1.5）
  LCR/PGR
SDG 11.7.1       32%       41%       ↑ +9pp      ✅  改善
  公共空间覆盖
SDG 13.2.2       12.1      18.7      ↑ +54%      ❌  显著恶化
  年碳排放(万tCO2eq)
SDG 3.8.1        68%       76%       ↑ +8pp      ✅  改善
  医疗可及性
SDG 11.2.1       54%       71%       ↑ +17pp     ✅  显著改善
  公交可及性
─────────────────────────────────────────────────────────
综合判定：城市扩张失控、碳排放同步失控；公共服务勉强跟上
```

**核心结论**：

> **过去 10 年该研究区呈现"服务提升 vs 生态承压"的典型城镇化模式**：
> - 5 项指标中 **3 项改善**（公共空间、医疗、公交）
> - **2 项恶化**（土地/人口失衡、碳排放）
> - 恶化项恰是**土地治理领域**，改善项集中在**基础设施建设**
> - 判断：**该区享受了城市化红利，但没管住城市扩张的边界**

<details>
<summary>📎 证据链（4 条）</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "sdg-1131",
      "params": {"lucc_t0": "lucc/2010.tif", "lucc_t1": "lucc/2020.tif",
                 "pop_t0": "pop/2010.tif", "pop_t1": "pop/2020.tif"},
      "result_file": "data/outputs/sdg1131_2010_2020.tif",
      "key_metrics": {"LCR": 0.047, "PGR": 0.026, "LCR_over_PGR": 1.81},
      "reproduce_cmd": "geosdg-cli sdg-1131 --lucc-t0 data/rasters/lucc/2010.tif --lucc-t1 data/rasters/lucc/2020.tif --pop-t0 data/rasters/pop/2010.tif --pop-t1 data/rasters/pop/2020.tif -o data/outputs/sdg1131_2010_2020.tif"
    },
    {
      "type": "cli_output",
      "tool": "sdg-1322",
      "params": {"lucc_t0": "lucc/2010.tif", "lucc_t1": "lucc/2020.tif"},
      "result_file": "data/outputs/sdg1322_2010_2020.tif",
      "key_metrics": {"emission_2010": 121000, "emission_2020": 187000, "delta_pct": 54.5}
    },
    {
      "type": "knowledge",
      "source": "UN SDG 11.3.1 Metadata",
      "quote": "A ratio of LCR to PGR greater than 1 indicates that land is being consumed at a faster rate than population growth."
    },
    {
      "type": "meta_lookup",
      "field": "layers.lucc.class_ratio_2010.5 & class_ratio_2020.5",
      "value": {"2010": 0.124, "2020": 0.183}
    }
  ],
  "caveats": [
    "SDG 11.3.1 依赖 LUCC 城市类边界；边界定义变化会影响 LCR 计算",
    "SDG 13.2.2 排放系数来自 IPCC 2006 通用系数，未做本地化校正"
  ]
}
```

</details>

---

### Turn 2

**👤 小李**：
> LCR/PGR 从 1.2 到 1.8 到底意味着什么？能给领导讲清楚吗？

**🤖 GeoSDG Copilot**：

给领导讲的一段话（可直接抄进汇报）：

> **"过去 10 年，我们研究区的城市用地扩张速度是人口增长速度的 1.8 倍——也就是说，多长出来 1 个人，就多消耗 1.8 份土地。**
>
> **联合国 SDG 11.3.1 指标把 1.0 作为"人地匹配"基准线，超过 1.5 就进入警戒区。**
>
> **我们从 2010 年的 1.2（略高但可控）恶化到 2020 年的 1.8（已进入警戒），且趋势仍在加剧。**
>
> **这意味着：**
> - **要么人口没跟上土地扩张的速度**（人口流出？规划过度乐观？）
> - **要么土地扩张失去了紧凑度**（摊大饼式发展？）
>
> **建议下一步做情景分析：如果不加干预，2050 年 SSP2/SSP3 情景下这个数字会变成多少？"**

> 🔍 想直接看未来推演？可以问："2050 年 SSP2 情景下 SDG 11.3.1 会怎么变？"（→ 触发**情景对比**）

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "knowledge",
      "source": "UN SDG 11.3.1 Metadata Repository",
      "quote": "LCRPGR = (ln(URBt/URB0) / y) / (ln(POPt/POP0) / y). Values > 1 indicate land consumption outpacing population growth.",
      "url_or_path": "https://unstats.un.org/sdgs/metadata/files/Metadata-11-03-01.pdf"
    },
    {
      "type": "knowledge",
      "source": "UN-Habitat World Cities Report 2020",
      "quote": "Sustainable urban growth requires LCRPGR to remain close to 1.0; ratios above 1.5 signal inefficient land use.",
      "url_or_path": "agent/knowledge/policy/un_habitat_2020.md"
    },
    {
      "type": "expert_opinion",
      "expert": "land_use_expert",
      "stance": "1.2→1.8 属于'从预警到警戒'的显著恶化，建议立即启动情景推演",
      "confidence": "high"
    }
  ],
  "caveats": ["LCR/PGR 是相对速率指标，不反映绝对规模；建议配合总面积一起解读"]
}
```

</details>

---

### Turn 3（增量追问）

**👤 小李**：
> 这个恶化在空间上是均匀的，还是集中在某些区域？

**🤖 GeoSDG Copilot**：

📍 **空间分解结果**：

将研究区按 1km 网格分区，2010→2020 LCR/PGR 变化分布：

```
分区                 面积占比   Δ(LCR/PGR)   判定
──────────────────────────────────────────────────
A 区（东南象限）     18%       +1.4         ❌  重度恶化
B 区（北部）         22%       +0.6         ⚠️  中度恶化
C 区（中心城区）     15%       +0.1         ✅  基本稳定
D 区（西部）         30%       -0.2         ✅  改善
其他                  15%       ±0.1         —   波动
──────────────────────────────────────────────────
```

**关键发现**：
- 恶化 **不均匀**，主要集中在 **A 区东南象限**（占面积 18%，贡献了全区 60% 的恶化量）
- A 区特征：**LUCC 转化率高**（cropland→urban 达 32%）、**人口密度低**（推测：新城开发但入住不足）
- 治理建议：**A 区是优先干预对象**，不必对全区一刀切

> 🔍 本次调用命中**增量缓存**：Turn 1 的 sdg-1131 结果被复用，只新增了 `priority-human-land` 空间分解，耗时 8 秒（对比：全量重跑 45 秒）。

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "medium",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "priority-human-land",
      "params": {"sdg1131_raster": "data/outputs/sdg1131_2010_2020.tif", "grid_size_m": 1000},
      "result_file": "data/outputs/priority_1131_delta_2010_2020.tif",
      "key_metrics": {"hotspot_area_km2": 222.2, "hotspot_share_of_worsening": 0.60}
    },
    {
      "type": "meta_lookup",
      "field": "layers.lucc.transition_2010_2020.cropland_to_urban.A_zone",
      "value": 0.32
    }
  ],
  "conflicts": [],
  "caveats": [
    "空间分区基于 1km 网格聚合，边界效应可能低估局部热点",
    "'A/B/C/D 区'为分析用命名，非行政边界；生产使用时需与规划分区叠加"
  ],
  "session": {
    "session_id": "demo_20260831_001",
    "turn": 3,
    "cached_evidence_ids": ["ev_1131_full_run", "ev_meta_extent"]
  }
}
```

</details>

---

## 🎯 本样例的关键设计点

| 设计点 | 说明 | PEACE 对照 |
|-------|------|-----------|
| **双时相对比是 GeoSDG 独有能力** | 地质图没有"过去 10 年"这个概念 | ❌ PEACE 做不了 |
| **给领导的一段话** | Copilot 直接产出可复制粘贴的汇报文本 | ✅ GeoSDG 决策导向 |
| **空间热点识别** | 不只算总数，而是告诉用户"恶化集中在哪" | PEACE grounding 类的增强 |
| **增量缓存** | Turn 3 复用 Turn 1 的重计算结果 | ❌ PEACE 无（一图一问一答） |
| **强制引导下一步** | Turn 2 末尾主动引出情景分析 | ✅ GeoSDG 对话式设计 |

## 🔗 下一步

→ [`conversation-03-scenario.md`](./conversation-03-scenario.md) 演示**情景对比**：SSP2 vs SSP3 未来推演。
