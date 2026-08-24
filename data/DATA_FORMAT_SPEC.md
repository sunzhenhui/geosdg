# GeoSDG 通用数据格式规范 v2.0

> 本文档定义了 GeoSDG 评估系统的标准化数据输入格式。采用 **Manifest 驱动** 架构，
> 将所有数据维度的元信息声明在 `manifest.json` 中，替代原有的隐式目录约定。

---

## 设计理念

### 原有问题

当前数据组织依赖**隐式约定**——用目录名承载元信息：

```
INFRA (2020-2050)/2025/SSP1/culture.tif
```

这种方式的问题：

| 问题 | 影响 |
|------|------|
| 年份硬编码 | 换一个研究区域（如 2000-2030）需重建目录 |
| 场景硬编码 | 用户可能使用 RCP、本地政策情景而非 SSP |
| 类别硬编码 | 基础设施分类因区域不同而不同（如农村需"水井"，城市需"地铁"） |
| 无区域维度 | 多城市/多国对比需复制多份目录 |
| 无优先级配置 | 优先区域规则散布在代码中，无法按项目定制 |
| 不可发现 | 引擎需遍历硬编码路径，新增数据类型需改代码 |

### 新方案：Manifest 驱动

```
data/
├── manifest.json          ← 唯一入口：声明所有数据的元信息
├── rasters/               ← 所有 GeoTIFF 扁平存放
│   ├── lucc_beijing_2020.tif
│   ├── pop_shanghai_2030_ssp2.tif
│   └── ...
└── configs/               ← 项目级配置
    ├── priority_areas.json
    └── indicator_params.json
```

引擎读取 `manifest.json` → 自动发现数据 → 构建计算 DAG。

---

## 一、Manifest 文件结构

### 1.1 顶层 Schema

```jsonc
{
  "$schema": "./manifest-schema.json",
  "project": {
    "name": "string",               // 项目名称
    "description": "string",        // 项目描述
    "region": {                     // 目标区域
      "name": "string",
      "bbox": [xmin, ymin, xmax, ymax],  // 经纬度或投影坐标
      "crs": "EPSG:4326"            // 坐标参考系
    },
    "time_range": {                 // 评估时间范围
      "baseline": 2020,             // 基准年
      "horizon": 2050,              // 终期年
      "step": 5                     // 时间步长（年）
    }
  },
  "datasets": [...],
  "scenarios": [...],
  "indicators": [...],
  "priority_areas": {...}
}
```

### 1.2 Dataset 定义

每个数据集对应一种**数据类型**（如人口、土地利用、基础设施），包含多个文件实例：

```jsonc
{
  "id": "pop",                      // 唯一标识
  "name": "Population",             // 显示名
  "category": "population",         // 分类: population | landuse | infrastructure | auxiliary
  "description": "...",
  "resolution": { "value": 1000, "unit": "m" },
  "citation": {                     // 数据来源引用（可选）
    "text": "Li et al. (2022)",
    "doi": "10.1088/1748-9326/ac8755"
  },
  "files": [                        // 数据文件列表
    {
      "path": "rasters/pop_beijing_2020.tif",
      "year": 2020,
      "scenario": null,
      "region": "beijing",
      "tags": ["baseline"]
    },
    {
      "path": "rasters/pop_beijing_2030_ssp2.tif",
      "year": 2030,
      "scenario": "ssp2",
      "region": "beijing"
    }
  ],
  "subcategories": [                // 有子类别的数据集（如基础设施细分）
    {
      "id": "healthcare",
      "name": "Healthcare Facilities",
      "sdg_targets": [3.8],
      "files": [...]
    }
  ]
}
```

### 1.3 Scenario 定义

用户可以定义**任意场景**，不限于 SSP：

```jsonc
{
  "id": "ssp2",
  "name": "SSP2 - Middle of the Road",
  "family": "SSP",                  // 场景族（可选，用于分组）
  "description": "..."
}
```

支持的场景类型：

| 场景族 | 示例 ID | 说明 |
|--------|---------|------|
| `SSP` | `ssp1` ~ `ssp5` | IPCC 共享社会经济路径 |
| `RCP` | `rcp2.6`, `rcp4.5`, `rcp8.5` | 代表性浓度路径 |
| `LOCAL` | `bau`, `green`, `compact` | 本地政策情景 |
| `CUSTOM` | 用户自定义任意 ID | 完全灵活 |

### 1.4 Indicator 定义

声明要计算的 SDG 指标及其参数：

```jsonc
{
  "id": "sdg_11_3_1",
  "sdg_goal": 11,
  "sdg_target": 11.3,
  "sdg_indicator": "11.3.1",
  "name": "Land consumption rate to population growth rate",
  "required_datasets": ["lucc", "pop"],
  "params": {
    "threshold": 0.5,
    "buffer_distance": 500
  }
}
```

### 1.5 Priority Areas 定义

优先级区域不再硬编码，而是通过配置声明：

```jsonc
{
  "rules": [
    {
      "id": "low_sdg_score",
      "name": "SDG 得分低于阈值",
      "type": "threshold",
      "condition": {
        "indicator": "sdg_11_3_1",
        "operator": "<",
        "value": 0.3
      },
      "weight": 1.0
    },
    {
      "id": "high_pop_density",
      "name": "人口密度高于中位数",
      "type": "ranking",
      "condition": {
        "dataset": "pop",
        "ranking": "top",
        "percentile": 30
      },
      "weight": 0.8
    },
    {
      "id": "composite",
      "name": "综合优先级",
      "type": "composite",
      "aggregation": "weighted_sum",
      "sub_rules": ["low_sdg_score", "high_pop_density"]
    }
  ],
  "output": {
    "n_classes": 5,
    "labels": ["Very Low", "Low", "Medium", "High", "Very High"]
  }
}
```

规则类型：

| 类型 | 说明 |
|------|------|
| `threshold` | 单指标阈值判定 |
| `ranking` | 基于排序（top N% / bottom N%） |
| `spatial` | 空间邻近性（距某要素的缓冲区） |
| `composite` | 多规则加权组合 |
| `change_rate` | 变化速率判定 |

---

## 二、目录结构

### 2.1 推荐结构

```
data/
├── manifest.json                # 数据清单（必须）
├── manifest-schema.json         # JSON Schema 定义
│
├── rasters/                     # GeoTIFF 文件（扁平存放）
│   ├── {dataset}_{region}_{year}_{scenario}.tif
│   └── ...
│
├── configs/                     # 项目配置
│   ├── priority_areas.json      # 优先区域规则
│   ├── indicator_params.json    # 指标计算参数
│   └── legends.json             # 图例/分类映射
│
├── legacy/                      # （可选）原有数据目录，通过 legacy-mapping 引用
│   ├── INFRA (2020-2050)/
│   ├── LUCC/
│   └── ...
│
└── outputs/                     # （可选）计算输出目录
    ├── indicators/
    ├── priority_areas/
    └── reports/
```

### 2.2 文件命名约定

```
{category}_{region}_{year}[_{scenario}].tif
```

| 片段 | 说明 | 示例 |
|------|------|------|
| `{category}` | 数据类型 | `lucc`, `pop`, `infra_healthcare` |
| `{region}` | 区域标识 | `beijing`, `yangtze_delta` |
| `{year}` | 年份 | `2020`, `2030` |
| `{scenario}` | 场景（可选） | `ssp2`, `bau` |

示例：
- `pop_shanghai_2020.tif`
- `lucc_beijing_2030_ssp5.tif`
- `infra_education_guangzhou_2025_bau.tif`

---

## 三、多维数据映射

对于具有**多个维度**的数据集（如 年份 × 场景 × 类别），在 manifest 中按文件逐一声明，而非通过目录嵌套。这样每个文件清晰标注了所有维度的值。

### 示例：基础设施未来预测（300+ 文件）

```jsonc
{
  "id": "infra",
  "category": "infrastructure",
  "subcategories": [
    {
      "id": "healthcare",
      "name": "Healthcare",
      "files": [
        {"path": "rasters/infra_healthcare_beijing_2025_ssp1.tif", "year": 2025, "scenario": "ssp1", "region": "beijing"},
        {"path": "rasters/infra_healthcare_beijing_2025_ssp2.tif", "year": 2025, "scenario": "ssp2", "region": "beijing"},
        {"path": "rasters/infra_healthcare_beijing_2030_ssp1.tif", "year": 2030, "scenario": "ssp1", "region": "beijing"}
      ]
    },
    {
      "id": "education_primary",
      "name": "Primary Schools",
      "files": [...]
    }
  ]
}
```

引擎通过 manifest 即可构建完整索引，无需遍历目录。

---

## 四、从旧格式迁移

### 4.1 兼容模式

可以在 manifest 中声明 `legacy_mapping`，引擎自动将旧目录映射为新格式：

```jsonc
{
  "legacy_mapping": {
    "INFRA (2020-2050)": {
      "category": "infrastructure",
      "pattern": "{year}/{scenario}/{indicator}.tif",
      "scenario_map": {
        "SSP1": "ssp1",
        "SSP2": "ssp2"
      },
      "indicator_map": {
        "Culture.tif": "culture",
        "Education.tif": "education"
      }
    }
  }
}
```

### 4.2 迁移工具

提供 `geosdg-cli migrate-data` 命令，自动：
1. 扫描旧目录结构
2. 生成带 legacy_mapping 的 manifest.json
3. 可选：重命名文件为扁平命名并为原文件创建软链接

---

## 五、可扩展性设计

### 5.1 自定义基础设施类别

不同区域的基础设施类型不同：

| 区域类型 | 可能的基础设施类别 |
|----------|-------------------|
| 发达城市 | 地铁、5G基站、三甲医院、图书馆、充电桩 |
| 发展中城镇 | 公交站、社区卫生中心、中小学、农贸市场 |
| 农村地区 | 水井、村卫生室、小学、农村公路 |

在 manifest 中，这些通过 `subcategories` 自由定义，无须修改引擎代码。

### 5.2 自定义场景

```jsonc
// 一个本地城市开发场景
{
  "scenarios": [
    {
      "id": "sprawl",
      "name": "Urban Sprawl Scenario",
      "family": "LOCAL",
      "description": "城市以低密度方式向外扩张"
    },
    {
      "id": "compact",
      "name": "Compact City Scenario", 
      "family": "LOCAL",
      "description": "高密度填充式发展"
    }
  ]
}
```

### 5.3 自定义优先区域规则

```jsonc
// 生态保护优先区域
{
  "priority_areas": {
    "rules": [
      {
        "id": "ecological_vulnerability",
        "type": "composite",
        "aggregation": "geometric_mean",
        "sub_rules": [
          {
            "id": "high_slope",
            "type": "threshold",
            "condition": {"dataset": "slope", "operator": ">", "value": 15}
          },
          {
            "id": "near_water",
            "type": "spatial",
            "condition": {"dataset": "dis_water", "operator": "<", "value": 500}
          }
        ]
      }
    ]
  }
}
```

---

## 六、验证

### 6.1 JSON Schema 验证

使用 `manifest-schema.json` 对 manifest 进行结构验证：

```bash
geosdg-cli validate manifest.json
```

### 6.2 完整性检查

引擎启动时自动验证：
- [ ] 所有 `files[].path` 指向的文件存在
- [ ] 所有 `indicator.required_datasets` 引用的 dataset ID 已定义
- [ ] 所有 `priority_areas.rules` 引用的 indicator/dataset 已定义
- [ ] 年份不超出 `time_range` 范围
- [ ] 场景 ID 在 `scenarios` 中已定义

---

## 七、完整示例

参见：
- `manifest.example.json` — 基于现有 535 个 .tif 数据的示例 manifest
- `manifest-schema.json` — JSON Schema 定义文件
- `_template/` — 新项目初始化模版目录
