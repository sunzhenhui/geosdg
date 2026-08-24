# SDG 指标 → Tool 映射表（跨 Skill 共享）

> 本文件从 agent-memory L2 报告映射和 sdg-indicator-knowledge 抽取的 SDG → Tool 映射关系。
> agent-memory 和 sdg-indicator-knowledge 引用本文件而非各自维护副本。

---

## Tool → SDG 映射

| Tool | SDG | 简称 | 计算类型 |
|------|-----|------|---------|
| `sdg-land-proportion` | 2.1.2 / 6.6.1 / 15.1.1 | 土地比例 | Land Proportion (`*`) |
| `sdg-land-conversion` | 14.5.1 / 15.2.1 / 15.3.1 | 土地转换 | Land Conversion (`**`) |
| `sdg-buffer-zone` | 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1 | 缓冲区覆盖 | Buffer Zone (`***`) |
| `sdg-1131` | 11.3.1 | 城市增长 | Total Statistics (`****`) |
| `sdg-1322` | 13.2.2 | 碳排放达峰 | Total Statistics (`****`) |
| `ca-precision` | — | CA精度评估 | — |
| `correlation` | — | 相关性检验 | — |
| `t-test` | — | t检验 | — |
| `priority-loss` | — | 优先区域 Rule 1 | — |
| `priority-transition` | — | 优先区域 Rule 2 | — |
| `priority-buffer` | — | 优先区域 Rule 3/4 | — |
| `priority-emission` | — | 优先区域 Rule 5 | — |
| `priority-human-land` | — | 优先区域 Rule 6 | — |
| `priority-merge` | — | 优先区域合并 | — |

---

## 维度分类（UN 官方三维度）

用于综合评估报告中的维度归类和雷达图生成。

| 维度 | 包含 SDG | 说明 |
|------|---------|------|
| 经济 | SDG 2, 9 | 零饥饿、产业与基础设施 |
| 社会 | SDG 3, 4, 7, 11 | 健康、教育、能源、城市 |
| 环境 | SDG 6, 13, 14, 15 | 水资源、气候、海洋、陆地 |

> 同一 Tool 覆盖多 SDG 时，按用户评估时指定的 SDG 目标编号归类。
> 综合得分 = 三维度得分的等权平均；各维度得分 = 该维度下各指标得分的算术平均。

---

## 计算类型说明

| 类型 | 符号 | CLI 命令 | 核心逻辑 |
|------|------|---------|---------|
| Land Proportion | `*` | `sdg-land-proportion` | 指定地类像元数 / 总像元数，正向归一化到 [0, 100] |
| Land Conversion | `**` | `sdg-land-conversion` | 两期 LUCC 指定转换类型像元数 / 初始期总像元数，正/负向归一化 |
| Buffer Zone | `***` | `sdg-buffer-zone` | 基础设施覆盖区内目标像元 / 总目标像元，归一化 |
| Total Statistics | `****` | `sdg-1131` / `sdg-1322` | 比值/总量统计，三角归一化或比例扣分 |

---

## 得分解读规则

| 方向 | 得分范围 | 等级 |
|------|---------|------|
| negative | ≥80 | 良好 |
| negative | ≥60 | 中等需关注 |
| negative | ≥40 | 较重建议采取措施 |
| negative | <40 | 严重亟需干预 |
| positive | ≥80 | 优秀 |
| positive | ≥60 | 良好 |
| positive | ≥40 | 一般有提升空间 |
| positive | <40 | 较差亟需改善 |
