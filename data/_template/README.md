# GeoSDG 通用数据模版

> 将本目录复制到新项目后，按实际情况修改 `manifest.json` 中的项目信息、数据文件列表、场景定义和优先区域规则即可。

---

## 快速开始（4 步）

### 1. 复制模版

```bash
cp -r data/_template/ my_project/data/
```

### 2. 放入你的 GeoTIFF 数据

将所有 TIFF 文件放入 `rasters/` 目录，遵循命名约定：

```
{category}_{region}_{year}[_{scenario}].tif
```

示例：
- `pop_beijing_2020.tif`
- `lucc_shanghai_2030_ssp2.tif`
- `infra_healthcare_guangzhou_2025_bau.tif`

### 3. 编辑 manifest.json

打开 `manifest.json`，修改以下部分：

#### 3.1 项目信息

```jsonc
"project": {
  "name": "你的项目名称",
  "region": {
    "name": "北京市",
    "bbox": [115.4, 39.4, 117.5, 41.1],
    "crs": "EPSG:4326"
  },
  "time_range": {
    "baseline": 2020,
    "horizon": 2050,
    "step": 5
  }
}
```

#### 3.2 声明场景（如果不使用默认 SSP）

```jsonc
"scenarios": [
  { "id": "bau", "name": "Business as Usual", "family": "LOCAL" },
  { "id": "green", "name": "Green Development", "family": "LOCAL" }
]
```

#### 3.3 列出数据文件

```jsonc
"datasets": [
  {
    "id": "pop",
    "name": "Population",
    "category": "population",
    "resolution": { "value": 1000, "unit": "m" },
    "files": [
      { "path": "rasters/pop_mysite_2020.tif", "year": 2020 },
      { "path": "rasters/pop_mysite_2030_bau.tif", "year": 2030, "scenario": "bau" }
    ]
  }
]
```

#### 3.4 定义你自己的基础设施类别

不同区域的基础设施不同，在 `subcategories` 中自由定义：

```jsonc
{
  "id": "infra",
  "category": "infrastructure",
  "subcategories": [
    { "id": "metro_station", "name": "Metro Station", "files": [...] },
    { "id": "charging_pile", "name": "EV Charging Station", "files": [...] },
    { "id": "wetland_park",  "name": "Wetland Park", "files": [...] }
  ]
}
```

#### 3.5 配置优先区域规则

```jsonc
"priority_areas": {
  "rules": [
    {
      "id": "your_rule",
      "name": "你的识别规则",
      "type": "composite",
      "aggregation": "weighted_sum",
      "sub_rules": ["rule_a", "rule_b"]
    }
  ],
  "output": { "n_classes": 5, "labels": ["Very Low", "Low", "Medium", "High", "Very High"] }
}
```

### 4. 验证

```bash
geosdg-cli validate manifest.json
```

---

## 目录结构

```
_template/
├── README.md                   ← 本文件
├── .gitkeep                    ← 确保空目录被 git 跟踪
├── manifest.json               ← 唯一入口（必须）
├── manifest-schema.json        ← JSON Schema（从上级复制）
├── rasters/                    ← 所有 GeoTIFF 数据
│   └── .gitkeep
├── configs/                    ← 项目配置文件
│   ├── priority_areas.json     ← 优先区域规则
│   ├── indicator_params.json   ← 指标参数
│   └── legends.json            ← 分类图例
└── outputs/                    ← 计算结果
    ├── indicators/
    ├── priority_areas/
    └── reports/
```

---

## 常见自定义场景

### 场景 A：多区域对比

```jsonc
{
  "datasets": [
    {
      "id": "pop",
      "files": [
        {"path": "rasters/pop_beijing_2020.tif",  "year": 2020, "region": "beijing"},
        {"path": "rasters/pop_shanghai_2020.tif", "year": 2020, "region": "shanghai"},
        {"path": "rasters/pop_guangzhou_2020.tif","year": 2020, "region": "guangzhou"}
      ]
    }
  ]
}
```

### 场景 B：非标准年份范围

```jsonc
{
  "project": {
    "time_range": { "baseline": 2000, "horizon": 2035, "step": 5 }
  }
}
```

### 场景 C：本地政策情景（不用 SSP）

```jsonc
{
  "scenarios": [
    {"id": "low_growth",    "name": "Low Growth",     "family": "LOCAL"},
    {"id": "medium_growth", "name": "Medium Growth",   "family": "LOCAL"},
    {"id": "high_growth",   "name": "High Growth",     "family": "LOCAL"}
  ]
}
```

### 场景 D：结合 RCP 和 SSP

```jsonc
{
  "scenarios": [
    {"id": "ssp2_rcp45", "name": "SSP2 + RCP4.5", "family": "RCP", "parent_scenario": "ssp2"},
    {"id": "ssp5_rcp85", "name": "SSP5 + RCP8.5", "family": "RCP", "parent_scenario": "ssp5"}
  ]
}
```

---

## 与旧格式共存

如果已有旧格式目录（如 `INFRA (2020-2050)/`），可以在 `manifest.json` 中添加 `legacy_mapping`：

```jsonc
"legacy_mapping": {
  "INFRA (2020-2050)": {
    "category": "infrastructure",
    "pattern": "{year}/{scenario}/{indicator}.tif",
    "scenario_map": {"SSP1": "ssp1", "SSP2": "ssp2", "SSP3": "ssp3", "SSP4": "ssp4", "SSP5": "ssp5"},
    "indicator_map": {
      "bus_stop.tif": "bus_stop",
      "clean_energy.tif": "clean_energy",
      "healthcare.tif": "healthcare"
    }
  }
}
```

引擎会自动按此映射解析旧目录，无需重命名文件。
