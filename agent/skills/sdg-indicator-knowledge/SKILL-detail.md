# SDG Indicator Knowledge Skill — 完整知识库

> 本文件为 sdg-indicator-knowledge Skill 的完整层，由 SKILL.md 摘要层按需加载。
> 摘要层仅包含 4 种计算类型和覆盖统计，本文件包含完整指标体系和设计方法论。

---

## SDG 指标框架概述

联合国 2030 议程包含三级指标体系：

| 层级 | 数量 | 说明 |
|------|------|------|
| Goals（目标） | 17 | 顶层主题 |
| Targets（子目标） | 169 | 每个目标下的具体子目标 |
| Indicators（指标） | 231 | 可量化的衡量指标 |

### 17 个 SDG 目标速览

| SDG | 主题 | 空间化可行性 | GeoSDG 覆盖 |
|-----|------|-------------|------------|
| SDG 1 | 无贫困 | 低 | — |
| SDG 2 | 零饥饿 | 中高 | ✅ 4 个指标 |
| SDG 3 | 良好健康与福祉 | 中 | ✅ 6 个指标 |
| SDG 4 | 优质教育 | 中 | ✅ 3 个指标 |
| SDG 5 | 性别平等 | 低 | — |
| SDG 6 | 清洁饮水和卫生设施 | 高 | ✅ 1 个指标 |
| SDG 7 | 经济适用的清洁能源 | 中 | ✅ 1 个指标 |
| SDG 8 | 体面工作和经济增长 | 低 | — |
| SDG 9 | 产业、创新和基础设施 | 高 | ✅ 3 个指标 |
| SDG 10 | 减少不平等 | 低 | — |
| SDG 11 | 可持续城市和社区 | 高 | ✅ 4 个指标 |
| SDG 12 | 负责任消费和生产 | 低 | — |
| SDG 13 | 气候行动 | 高 | ✅ 1 个指标 |
| SDG 14 | 水下生物 | 中 | ✅ 1 个指标 |
| SDG 15 | 陆地生物 | 高 | ✅ 3 个指标 |
| SDG 16 | 和平、正义与强大机构 | 低 | — |
| SDG 17 | 促进目标实现的伙伴关系 | 低 | — |

---

## 指标设计方法论

### Step 1：指标可行性判断

| 判断维度 | 问题 | 通过条件 |
|---------|------|---------|
| 空间化可行性 | 指标是否涉及地理位置信息？ | 能用栅格/矢量数据表达 |
| 数据可获得性 | 所需数据是否可获取？ | 有公开的遥感/统计/GIS 数据 |
| 计算可行性 | 能否用 GDAL 栅格运算实现？ | 公式可转为逐像素/区域统计 |
| 归一化可行性 | 能否定义合理阈值？ | 有学术参考或政策标准 |

### Step 2：指标公式设计

参考已实现的 4 种计算类型：

- **Land Proportion (`*`)**：`proportion = target_pixels / total_pixels` → `normalization(proportion, min, max)` → [0, 100]
- **Land Conversion (`**`)**：`rate = converted_pixels / initial_pixels` → positive: `normalization(rate, min, max)`, negative: `normalizationNegative(rate, min, max)` → [0, 100]
- **Buffer Zone (`***`)**：`coverage = covered_target / total_target` → `normalization(coverage, min, max)` → [0, 100]
- **Total Statistics (`****`)**：`ratio = change_A / change_B` → `normalization(ratio, min, max)` with optional best-value triangular normalization → [0, 100]

### Step 3：数据需求定义

| 数据项 | 格式 | 类型 | 来源 |
|--------|------|------|------|
| {数据名称} | GeoTIFF / CSV / 矢量 | GDT_Byte / Float32 | {卫星遥感 / 统计年鉴 / 开放数据} |

### Step 4：代码定位

在 `CalculateSDG.h` 中新增方法声明，在 `CalculateSDG.cpp` 中实现，在 `main.cpp` 中注册子命令。参考现有 5 个计算命令的实现模式。

### 各类型输入数据详解

#### Land Proportion (`*`)

| 参数 | 类型 | 说明 |
|------|------|------|
| LUCC raster | GeoTIFF, GDT_Byte | 土地利用分类数据 |
| Selected LUCC types | `unordered_set<int>` | 目标地类编码集合 |
| Max threshold | `double` | 归一化上限 |
| Min threshold | `double` | 归一化下限 |

#### Land Conversion (`**`)

| 参数 | 类型 | 说明 |
|------|------|------|
| Initial LUCC | GeoTIFF, GDT_Byte | 初始期土地利用 |
| Changed LUCC | GeoTIFF, GDT_Byte | 变化期土地利用 |
| Transition types | `unordered_map<int, vector<int>>` | 转换类型映射 |
| Direction flag | `bool` | `true`=正向, `false`=负向 |

#### Buffer Zone (`***`)

| 参数 | 类型 | 说明 |
|------|------|------|
| Input data | GeoTIFF, GDT_Byte or Float32 | 地类分布或人口分布 |
| Buffer zone data | GeoTIFF, GDT_Byte | 基础设施覆盖区栅格 |
| Selected types | `unordered_set<int>` | 目标地类编码（仅 Byte 模式） |

#### Total Statistics (`****`)

| 参数 | 类型 | 说明 |
|------|------|------|
| Initial/Current LUCC | GeoTIFF, GDT_Byte | 两期土地利用 |
| Initial/Current Population | GeoTIFF, Float32 | 两期人口数据（SDG 11.3.1） |
| Emission scheme | `unordered_map<int, double>` | 各地类排放系数（SDG 13.2.2） |

---

## 指标拓展路线图

### 高优先级（空间化可行 + 数据易获取）

| SDG 指标 | 指标名称 | 计算类型 | 与现有模块的关系 |
|----------|---------|---------|----------------|
| 2.1.2 | Grain yield | `****` | 可扩展 11.3.1/13.2.2 模式 |
| 6.3.2 | Water quality | `*` / `**` | 可复用 Land Proportion 模式 |
| 6.4.2 | Water stress | `****` | 需用水量 + 水资源数据 |

### 中优先级（需额外数据或算法）

| SDG 指标 | 指标名称 | 难点 |
|----------|---------|------|
| 11.6.2 | PM2.5 年均浓度 | 需大气污染遥感数据 |
| 12.2.2 | 物质足迹 | 需投入产出表，空间化困难 |

### 低优先级（空间化困难或数据稀缺）

| SDG 指标 | 原因 |
|----------|------|
| SDG 1.x | 贫困为社会经济指标，难以空间化 |
| SDG 4.x | 教育质量难以用空间数据独立衡量 |
| SDG 5.x | 性别平等为制度性指标 |
| SDG 8.x | 经济增长统计依赖，非空间问题 |

---

## Important Caveats

- 新增指标必须参考现有 `CalculateSDG` 的方法风格和接口约定
- 指标阈值设定需学术论文或 UN 官方文档支持
- 地类编码体系因数据源不同而异
- **Buffer Zone 指标**通过不同基础设施缓冲区栅格输入即可覆盖多种指标
- **Land Proportion / Land Conversion** 通过不同地类编码和转换类型参数即可覆盖
