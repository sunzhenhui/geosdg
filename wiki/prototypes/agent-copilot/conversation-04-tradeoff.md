# 对话样例 04 — 目标间协同/权衡（Trade-off）

> **评估维度**：目标间 | **能力类别**：analyzing
> **PEACE 对标**：❌ **PEACE 做不了** —— 地质图上没有"目标间权衡"概念
>
> **本样例目的**：演示 SDG 学术界公认的核心议题 —— **协同与权衡（Synergies & Trade-offs）** 的自动化分析。
> **执行成本**：40 秒（跑多指标空间叠加 + priority-merge）。

---

## 🎬 场景

小李已经知道 SSP2 下大局会恶化。领导拍板："那我们就重点保 SDG 11 城市可持续。"
小李追问 Copilot："只保 11 会不会顾此失彼？"

---

## 💬 对话

### Turn 1

**👤 小李**：
> 如果我们只优先保 SDG 11（可持续城市），会不会牺牲其他 SDG？帮我看看 SSP2 2050 情景下的协同/权衡关系。

**🤖 GeoSDG Copilot**：

🔀 **SSP2 2050 情景下 SDG 交叉影响矩阵**

（数值 = 空间相关系数 ρ，绝对值越大说明关联越强；正值协同，负值权衡）

```
                 11.3.1  11.7.1  13.2.2  15.3.1  3.8.1   4.a.1   11.2.1
                 (人地)  (公共)  (排放)  (退化)  (医疗)  (教育)  (公交)
    ┌────────────────────────────────────────────────────────────────
11.3.1  (人地) │   -    +0.31   +0.94   +0.68   -0.12   -0.09   -0.42
11.7.1  (公共) │ +0.31    -     +0.15   -0.55   +0.28   +0.31   +0.19
13.2.2  (排放) │ +0.94  +0.15     -     +0.71   -0.08   -0.05   -0.38
15.3.1  (退化) │ +0.68  -0.55   +0.71     -     -0.11   -0.07   -0.34
3.8.1   (医疗) │ -0.12  +0.28   -0.08   -0.11    -      +0.62   +0.51
4.a.1   (教育) │ -0.09  +0.31   -0.05   -0.07   +0.62    -      +0.48
11.2.1  (公交) │ -0.42  +0.19   -0.38   -0.34   +0.51   +0.48    -
```

**关键发现**：

🟢 **强协同（改一个自动带动另一个改善）**：
- SDG 11.3.1 ↔ 13.2.2（ρ=+0.94）：**保城市扩张边界 = 保碳排放**（同源同解）
- SDG 3.8.1 ↔ 4.a.1（ρ=+0.62）：**保医疗 = 保教育**（服务设施同步布局）

🔴 **强权衡（改一个会牺牲另一个）**：
- SDG 11.7.1 ↔ 15.3.1（ρ=**-0.55**）：**建城市公共空间 vs 保生态用地** —— 城市公园的"公共空间"属性和"生态用地"属性在土地上争地
- SDG 11.3.1 ↔ 11.2.1（ρ=-0.42）：**紧凑城市 vs 公交可及** —— 有点反直觉，需专家解读

**给决策的一句话**：

> **"只保 SDG 11" 是模糊指令，Copilot 需要澄清：**
> - **保 11.3.1（土地）** = 顺便保 13.2.2 排放 + 15.3.1 生态；**但要牺牲一部分 11.7.1 公共空间**
> - **保 11.7.1（公共空间）** = 会加速 15.3.1 生态退化
> - **保 11.2.1（公交）** = 与 11.3.1 紧凑城市有一定冲突（可能因为公交建设推动了土地扩张）
>
> **建议：领导要的应该是"保 11.3.1 + 13.2.2 + 15.3.1 三角"，而不是笼统的"保 11"。**

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "medium",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "priority-merge",
      "params": {"indicators": ["sdg1131", "sdg1171", "sdg1322", "sdg1531", "sdg381", "sdg4a1", "sdg1121"], "scenario": "ssp2", "year": 2050, "method": "spatial_correlation"},
      "result_file": "data/outputs/sdg_correlation_matrix_ssp2_2050.csv",
      "key_metrics": {"strongest_synergy": "11.3.1 ↔ 13.2.2 (0.94)", "strongest_tradeoff": "11.7.1 ↔ 15.3.1 (-0.55)"}
    },
    {
      "type": "knowledge",
      "source": "Nilsson et al. 2016, Nature - Policy: Map the interactions between Sustainable Development Goals",
      "quote": "SDG interactions can be scored on a scale from -3 (cancelling) to +3 (indivisible). Trade-offs must be explicitly managed.",
      "url_or_path": "agent/knowledge/policy/nilsson_2016_sdg_interactions.md"
    },
    {
      "type": "expert_opinion",
      "expert": "land_use_expert",
      "stance": "11.7.1 ↔ 15.3.1 权衡本质是'城市公园算不算生态用地'的定义问题，规划层面可通过'口袋公园+郊野公园'组合缓解",
      "confidence": "high"
    }
  ],
  "caveats": [
    "空间相关系数不等于因果关系；本分析仅识别共变模式，机制解读需专家",
    "样本为研究区内所有 1km 网格，未去除空间自相关"
  ]
}
```

</details>

---

### Turn 2

**👤 小李**：
> 那哪些地方是"多目标共赢"区？在这些地方投资最划算？

**🤖 GeoSDG Copilot**：

🎯 **多目标共赢热点识别**

以"同时改善 ≥ 3 个 SDG"为门槛，跑 priority-merge 空间叠加：

```
共赢区                面积      改善 SDG 数量    主导改善
──────────────────────────────────────────────────────────
B 区北部（近郊过渡带）  12.3 km²     4 项          11.3.1 / 13.2.2 / 15.3.1 / 11.7.1
E 区（老工业区更新地）   5.8 km²     3 项          11.2.1 / 3.8.1 / 4.a.1
──────────────────────────────────────────────────────────
共计                   18.1 km²    占研究区 1.5%
```

**空间语义解读**：

- **B 区北部**：**"生态-城市过渡带"**，投资绿色基础设施（生态廊道、郊野公园）能同时带动 4 项 SDG 改善
- **E 区老工业区**：**"存量更新区"**，改造公交 + 补齐医疗教育设施能同时带动 3 项 SDG 改善

**同时暴露"冲突区"**（改一个牺牲另一个）：

```
冲突区              面积       核心冲突                     需人工权衡
──────────────────────────────────────────────────────────────────
C 区（中心城区）    8.2 km²    11.7.1(建公园) vs 15.3.1(保生态)   ⚠️ 需政策取舍
A 区东南（新城）    45.2 km²   11.2.1(修地铁) vs 11.3.1(控扩张)   ⚠️ 需政策取舍
──────────────────────────────────────────────────────────────────
```

**决策建议**：

> **P0（优先投资）**：B 区北部 + E 区（18.1 km²）—— 花小钱办大事，多目标共赢
> **P1（政策取舍）**：C 区 + A 区东南（53.4 km²）—— 需人工权衡，Copilot 无法替决策者做价值判断
> **P2（观望）**：其余区域——协同/权衡不显著

📎 **可视化产物**：
- `data/outputs/priority_multi_sdg_ssp2.png` — 共赢区/冲突区/中性区三色图
- `data/outputs/one_pager_multi_sdg.pdf` — 决策一页纸（直接给领导）

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "cli_output",
      "tool": "priority-merge",
      "params": {"indicators": ["sdg1131", "sdg1171", "sdg1322", "sdg1531", "sdg381", "sdg4a1", "sdg1121"], "scenario": "ssp2", "year": 2050, "method": "win_win_overlay", "min_improved_count": 3},
      "result_file": "data/outputs/priority_multi_sdg_ssp2.tif",
      "key_metrics": {"win_win_area_km2": 18.1, "conflict_area_km2": 53.4, "neutral_area_km2": 1163.0}
    },
    {
      "type": "knowledge",
      "source": "UN-Habitat Sustainable Development Framework 2023",
      "quote": "Integrated territorial approach identifies co-benefit hotspots to maximize investment efficiency.",
      "url_or_path": "agent/knowledge/policy/un_habitat_2023_framework.md"
    },
    {
      "type": "expert_opinion",
      "expert": "policy_expert",
      "stance": "18.1 km² 共赢区 + 53.4 km² 冲突区的划分符合国土空间规划'双评价'（资源环境承载力 + 国土空间开发适宜性）逻辑，可直接对接",
      "confidence": "high"
    }
  ],
  "action_items": [
    {
      "priority": "P0",
      "action": "在 B 区北部 12.3 km² 布局生态廊道 + 郊野公园综合体",
      "target_sdg": ["11.3.1", "11.7.1", "13.2.2", "15.3.1"],
      "expected_impact": "4 项 SDG 同步改善；投资强度中等；风险低"
    },
    {
      "priority": "P0",
      "action": "在 E 区 5.8 km² 老工业区推进 TOD 综合更新（公交 + 医疗 + 教育）",
      "target_sdg": ["3.8.1", "4.a.1", "11.2.1"],
      "expected_impact": "3 项 SDG 同步改善；对接现有旧改政策；执行阻力小"
    },
    {
      "priority": "P1",
      "action": "C 区公园化 vs 生态化：召开专项会议决策；建议采用'口袋公园'折中方案",
      "target_sdg": ["11.7.1", "15.3.1"],
      "expected_impact": "取决于取舍；Copilot 无法自动决定"
    }
  ],
  "caveats": [
    "共赢/冲突区识别基于 SSP2 情景；SSP1/SSP3 下分区可能重排",
    "P1 行动需人工价值判断，Copilot 提供依据但不代替决策"
  ]
}
```

</details>

---

## 🎯 本样例的关键设计点

| 设计点 | 说明 | PEACE 对照 |
|-------|------|-----------|
| **相关矩阵可视化** | 一眼看出协同/权衡结构 | ❌ PEACE 无 |
| **共赢区识别** | 用 GIS 空间叠加找"多目标交集" | ✅ GeoSDG 独有 |
| **冲突区暴露** | 明确告诉用户"这里 Copilot 决定不了" | ✅ 尊重人类决策权 |
| **P0/P1/P2 优先级** | 给决策者可执行的行动清单 | ✅ 决策级输出 |
| **对接国土空间规划术语** | "双评价"直接呼应现有政策语言 | ✅ 本地化 |

## 🔗 下一步

→ [`conversation-05-expert-panel.md`](./conversation-05-expert-panel.md) 演示**多专家会诊**：五专家协作 + 冲突暴露完整闭环。
