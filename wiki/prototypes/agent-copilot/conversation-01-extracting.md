# 对话样例 01 — 现状提取（Extracting）

> **评估维度**：现在（基线） | **能力类别**：extracting | **PEACE 对标**：直接查 meta
>
> **本样例目的**：演示"最基础的一问一答" —— 用户想知道"这个研究区现在到底啥样"。
> **执行成本**：**秒级**，全部从 `sdg_meta.json` 缓存读取，不触发任何 CLI 重算。

---

## 🎬 场景

规划科员小李刚接到一个新研究区 `data/`，还没看过数据，先想问几个基础问题热身。

---

## 💬 对话

### Turn 1

**👤 小李**：
> 我这个 demo 研究区有多大？覆盖哪里？

**🤖 GeoSDG Copilot**：

研究区 **demo** 概况：

| 项 | 值 |
|----|-----|
| 面积 | 约 **1,234.5 km²** |
| 经度范围 | 116.20°E ~ 116.65°E |
| 纬度范围 | 39.75°N ~ 40.05°N |
| 坐标系 | EPSG:4326 |
| 空间分辨率 | 30 m |
| 数据时相 | 2010 / 2020（LUCC、人口双时相齐备） |
| 未来情景 | SSP1 ~ SSP5，年份 2025-2050 |

> ⏱️ 本次回答 0.2 秒，从 `sdg_meta.json` 直接读取，未触发 CLI 重算。

<details>
<summary>📎 证据链（点开查看）</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "meta_lookup",
      "field": "extent",
      "value": {"min_lon": 116.20, "min_lat": 39.75, "max_lon": 116.65, "max_lat": 40.05}
    },
    {
      "type": "meta_lookup",
      "field": "area_km2",
      "value": 1234.5
    },
    {
      "type": "meta_lookup",
      "field": "layers.future.scenarios",
      "value": ["ssp1", "ssp2", "ssp3", "ssp4", "ssp5"]
    }
  ],
  "caveats": ["面积按等积投影估算，误差 < 0.5%"]
}
```

</details>

---

### Turn 2

**👤 小李**：
> 2020 年城市用地占比多少？

**🤖 GeoSDG Copilot**：

**2020 年研究区 LUCC 类别占比**：

```
cropland     ████████████████░░░░░░░░░░  42.1%
urban        ███████░░░░░░░░░░░░░░░░░░░  18.3%   ← 你问的这项
forest       █████████░░░░░░░░░░░░░░░░░  22.7%
grassland    █████░░░░░░░░░░░░░░░░░░░░░  11.2%
water        ██░░░░░░░░░░░░░░░░░░░░░░░░   4.1%
other        █░░░░░░░░░░░░░░░░░░░░░░░░░   1.6%
```

- **城市用地面积**：约 **225.9 km²**（占比 18.3%）
- **同比 2010**：城市用地占比从 12.4% 上升到 18.3%（**↑ 5.9 个百分点**，10 年增幅 47%）

> 🔍 想深入看这 10 年的扩张趋势？可以问："过去 10 年 SDG 表现趋势如何？"（→ 触发**纵向对比**评估）

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "meta_lookup",
      "field": "layers.lucc.class_ratio_2020",
      "value": {"1": 0.421, "5": 0.183, "2": 0.227, "3": 0.112, "4": 0.041, "6": 0.016}
    },
    {
      "type": "meta_lookup",
      "field": "layers.lucc.class_ratio_2010.5",
      "value": 0.124
    }
  ],
  "caveats": ["LUCC 分类沿用中科院一级分类体系；'urban' 对应类别码 5"]
}
```

</details>

---

### Turn 3

**👤 小李**：
> 我能算哪些 SDG 指标？哪些数据不够？

**🤖 GeoSDG Copilot**：

基于当前研究区数据，SDG 就绪度盘点：

| SDG 指标 | 名称 | 就绪 | 说明 |
|---------|------|:-----:|------|
| **11.3.1** | 土地消耗率 vs 人口增长率 (LCR/PGR) | ✅ | LUCC + POP 双时相齐备 |
| **11.7.1** | 公共开敞空间人均份额 | ✅ | infra + pop 齐备 |
| **11.2.1** | 便捷公共交通覆盖率 | ✅ | infra (metro/bus) 齐备 |
| **11.a.1** | 城乡人口人均绿地 | ✅ | 依赖 aux_density_parkland |
| **13.2.2** | 温室气体排放（LUCC 转化贡献） | ✅ | LUCC 双时相 + 排放系数表 |
| **3.8.1** | 医疗设施可及性 | ✅ | aux_density_medical + pop |
| **4.a.1** | 教育设施可及性 | ✅ | aux_density_school + pop |
| **6.1.1** | 安全饮用水覆盖 | ⚠️ | 仅有距离图层，缺水质数据 |
| **9.c.1** | ICT 覆盖 | ❌ | 缺少通信基站/宽带图层 |
| **15.3.1** | 土地退化面积占比 | ❌ | 缺 NDVI / 生产力数据 |

**建议**：优先跑就绪度 ✅ 的 8 个指标，另外 3 个后续补数据。

> 🔍 想一次跑完 8 个指标？可以问："帮我评估研究区当前 SDG 表现"（→ 触发**多专家会诊**）

<details>
<summary>📎 证据链</summary>

```json
{
  "confidence": "high",
  "evidence": [
    {
      "type": "meta_lookup",
      "field": "sdg_readiness",
      "value": {
        "sdg_11_3_1": {"ready": true},
        "sdg_11_7_1": {"ready": true},
        "sdg_15_3_1": {"ready": false, "reason": "缺 NDVI"}
      }
    },
    {
      "type": "knowledge",
      "source": "UN SDG Indicators Metadata Repository",
      "quote": "Indicator 11.3.1 requires land consumption rate and population growth rate over a common time period."
    }
  ],
  "caveats": ["就绪度判定基于当前 manifest.json；用户可补数据后重跑 geosdg-cli inspect 更新"]
}
```

</details>

---

## 🎯 本样例的关键设计点

| 设计点 | 说明 | PEACE 对照 |
|-------|------|-----------|
| **零算力回答** | 全部从 `sdg_meta.json` 秒级返回 | PEACE HIE 缓存机制 |
| **证据可点开** | 每个数字都能追溯到 meta 字段路径 | PEACE few-shot JSON |
| **引导式追问** | 每轮末尾提示下一步能问什么 | ✅ **GeoSDG 增强项** |
| **就绪度报告** | 主动告诉用户"能做什么、不能做什么" | ✅ **GeoSDG 增强项** |

## 🔗 下一步

→ [`conversation-02-longitudinal.md`](./conversation-02-longitudinal.md) 演示**纵向对比**：过去 10 年趋势。
