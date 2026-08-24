# Tool Registry — GeoSDG Agent 工具注册规范

> 本文档定义 GeoSDG Agent 中 Tool 的注册规范、Schema 结构、参数映射规则与调用协议。
> 所有 Tool 对应 `geosdg-cli` 的子命令，Agent 通过读取这些定义自主决定使用哪个 Tool 及如何传参。

---

## 1. 目录结构

```
agent/tools/
├── manifest.json              # 全局索引（版本 + 分类 + 触发关键词 + Tool 列表）
├── tool-registry.md           # 本文件（注册规范文档）
├── ca-precision/              # CA 精度评估类
│   ├── ca-precision.json
│   ├── correlation.json
│   └── t-test.json
├── sdg-calc/                  # SDG 指标计算类
│   ├── sdg-land-proportion.json
│   ├── sdg-land-conversion.json
│   ├── sdg-buffer-zone.json
│   ├── sdg-1131.json
│   └── sdg-1322.json
├── priority-area/             # 优先区域识别类
│   ├── priority-loss.json
│   ├── priority-transition.json
│   ├── priority-buffer.json
│   ├── priority-emission.json
│   ├── priority-human-land.json
│   └── priority-merge.json
└── demo-help/                 # 演示与帮助类
    ├── demo.json
    └── help.json
```

---

## 2. Tool JSON Schema 规范

每个 Tool 定义为独立 JSON 文件，路径为 `agent/tools/<category>/<tool_name>.json`。

### 必需字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 工具唯一标识，与 CLI 子命令名一致（如 `ca-precision`） |
| `category` | string | 工具分类，对应 `manifest.json` 中的分类键 |
| `cli_command` | string | CLI 可执行文件名，固定为 `geosdg-cli` |
| `cli_subcommand` | string | CLI 子命令名（如 `ca-precision`） |
| `description` | string | 中文功能描述，供 LLM 理解用途 |
| `parameters` | object | JSON Schema 定义的参数列表 |
| `returns` | object | 返回结果的结构说明 |

### 可选字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `input_constraints` | object | 输入数据约束（数据类型、投影、尺寸等） |
| `timeout_seconds` | number | 超时阈值（秒），默认 300 |
| `examples` | array | 调用示例列表 |

### parameters 结构

遵循 JSON Schema `object` 类型规范：

```json
"parameters": {
  "type": "object",
  "properties": {
    "param_name": {
      "type": "string | number / integer / boolean / array",
      "format": "filepath | comma-list | transition-map | emission-map",
      "description": "参数说明",
      "default": "默认值（可选）",
      "required": true / false
    }
  },
  "required": ["param1", "param2"]
}
```

#### 特殊 format 值

| format | 说明 | CLI 映射示例 |
|--------|------|-------------|
| `filepath` | 文件路径，执行前校验文件是否存在 | `--init-lucc /data/a.tif` |
| `comma-list` | 逗号分隔列表 | `--types 1,2,3` |
| `transition-map` | 转换规则映射 `src:t1:t2,...` | `--transitions 2:5:6,4:5` |
| `emission-map` | 排放系数映射 `type:factor,...` | `--emission 1:-1,2:-20` |
| `file-list` | 逗号分隔文件路径列表 | `--files a.tif,b.tif` |

---

## 3. 参数映射规则（JSON → CLI）

agent-executor 将 JSON Schema 参数名转换为 CLI 参数名，采用 **snake_case → kebab-case** 转换：

### 标准转换

| JSON 属性名 | CLI 参数 | 说明 |
|------------|---------|------|
| `ori` | `--ori` | 原始 LUCC |
| `sim` | `--sim` | 模拟 LUCC |
| `real` | `--real` | 真实 LUCC |
| `data1` | `--data1` | 第一组数据 |
| `data2` | `--data2` | 第二组数据 |
| `init_lucc` | `--init-lucc` | 初始 LUCC |
| `curr_lucc` | `--curr-lucc` | 变化期 LUCC |
| `init_popu` | `--init-popu` | 初始人口 |
| `curr_popu` | `--curr-popu` | 变化期人口 |
| `buffer` | `--buffer` | 缓冲区数据 |
| `types` | `--types` | 地类编码集合 |
| `transitions` | `--transitions` | 转换规则 |
| `emission` | `--emission` | 排放系数 |
| `max` | `--max` | 上限阈值 |
| `min` | `--min` | 下限阈值 |
| `best` | `--best` | 最优值阈值 |
| `ratio` | `--ratio` | 比例参数 |
| `radius` | `--radius` | 邻域半径 |
| `pop_threshold` | `--pop-threshold` | 人口阈值 |
| `files` | `--files` | 文件列表 |
| `resume` | `--resume` | 断点续跑（布尔） |

### 特殊映射

| JSON 属性名 | CLI 参数 | 说明 |
|------------|---------|------|
| `output` | `-o` | 输出路径（使用短选项，CLI 同时支持 `-o` 和 `--output`） |

### 布尔参数处理

布尔型参数（`type: "boolean"`）不接收值，仅作为 flag 出现：

| JSON 属性 | 值 | CLI 生成 |
|-----------|-----|---------|
| `positive` | `true` | `--positive` |
| `positive` | `false` | `--negative` |
| `resume` | `true` | `--resume` |
| `resume` | `false` | （省略） |

> 注意：`positive`/`negative` 互斥，CLI 中 `--positive` 为默认。当 JSON 中 `positive=false` 时生成 `--negative`。

### 命令构建示例

**输入 JSON**：
```json
{
  "name": "sdg-land-conversion",
  "parameters": {
    "init_lucc": "/data/lucc_2010.tif",
    "curr_lucc": "/data/lucc_2030.tif",
    "transitions": "2:5:6,4:5",
    "max": 100,
    "min": 0,
    "positive": true
  }
}
```

**生成 CLI 命令**：
```bash
geosdg-cli sdg-land-conversion \
  --init-lucc /data/lucc_2010.tif \
  --curr-lucc /data/lucc_2030.tif \
  --transitions 2:5:6,4:5 \
  --max 100 \
  --min 0 \
  --positive
```

---

## 4. 调用协议

### 4.1 调用流程

```
agent-planner 发出 tool_call 请求
  │
  ├── 1. 加载 Tool Schema
  │      读取 agent/tools/<category>/<tool_name>.json
  │
  ├── 2. 参数校验
  │      a. 检查所有 required 参数是否存在
  │      b. 检查 filepath 类型参数文件是否存在（execute_command 前）
  │      c. 检查类型匹配（string/number/boolean）
  │      d. dimensions 约束（Phase 2 自动检查）
  │
  ├── 3. 构建 CLI 命令
  │      按 §3 参数映射规则转换
  │
  ├── 4. 执行命令
  │      execute_command(cmd, requires_approval=true)
  │      捕获 stdout / stderr / exit_code
  │
  ├── 5. 解析输出
  │      exit_code != 0 → 匹配错误模式表，返回友好提示
  │      exit_code == 0 → 按 returns schema 解析 stdout 为结构化 JSON
  │
  └── 6. 返回结果 + 写入 Memory
```

### 4.2 结构化输出解析

CLI stdout 采用 `key=value` 格式输出（如 `FoM=0.32 PA=0.87 ...`）。
executor 使用正则 `(\w+)=([-\d.]+)` 提取键值对，按 `returns` schema 组装为 JSON：

**CLI stdout**：
```
FoM=0.320000 PA=0.710000 UA=0.680000 Kappa=0.760000 OA=0.870000
```

**解析为 JSON**：
```json
{
  "FoM": 0.32,
  "PA": 0.71,
  "UA": 0.68,
  "Kappa": 0.76,
  "OA": 0.87
}
```

输出文件类命令（priority-* / priority-merge）的 stdout 为 `Output: <path>`，
解析为：
```json
{
  "output_file": "/data/PriorityAreas-1.tif",
  "status": "success"
}
```

### 4.3 错误模式匹配表

| CLI stderr 关键词 | 用户友好提示 |
|-------------------|------------|
| `GDALOpen failed` / `Cannot open` | 文件不存在或无法读取，请检查路径：{path} |
| `dimension` / `size mismatch` | 输入文件的行列数不一致，所有 GeoTIFF 必须具有相同的尺寸 |
| `projection` / `CRS` | 输入文件的投影坐标系不一致，请使用同一投影重新投影 |
| `NoData` / `nodata` | 数据中存在无效值，请检查 NoData 标记是否正确设置 |
| `../tmp/` / `tmp directory` | 缺少 ../tmp/ 临时目录，请手动创建后重试（Rule 5 需要） |
| `Float32` / `Float64` | 数据类型不匹配，Rule 6 仅支持 Float32 人口数据 |
| `stoi` / `stod` | 参数格式错误，请检查数值型参数是否合法 |

### 4.4 不存在的 Tool

当请求的 tool_name 在 `manifest.json` 中找不到时，返回：
```json
{
  "error": "tool_not_found",
  "message": "工具 '{name}' 不存在。可用工具列表见 agent/tools/manifest.json",
  "available_categories": ["ca-precision", "sdg-calc", "priority-area", "demo-help"]
}
```

### 4.5 必填参数缺失

当 required 参数缺失时，返回：
```json
{
  "error": "missing_required_param",
  "tool": "ca-precision",
  "missing": ["ori", "sim"],
  "message": "工具 'ca-precision' 缺少必填参数：ori, sim"
}
```

---

## 5. 新增 Tool 规范

新增 CLI 子命令后，按以下步骤注册 Tool：

1. 在 `agent/tools/<category>/` 下创建 `<tool_name>.json`，按 §2 规范编写
2. 在 `agent/tools/manifest.json` 的对应分类 `tools` 数组中添加 tool_name
3. 如需新增分类，在 `manifest.json` 的 `categories` 中添加分类定义（含 label + trigger_keywords）
4. 更新本文件 §3 参数映射表（如有新参数类型）
5. 在 agent-router 的 `references/intent-map.yaml` 中补充触发关键词（如需要）

---

## 6. Tool 清单总览

| 分类 | Tool 名称 | CLI 子命令 | 数量 |
|------|----------|-----------|------|
| CA 精度评估 | `ca-precision`, `correlation`, `t-test` | 同名 | 3 |
| SDG 指标计算 | `sdg-land-proportion`, `sdg-land-conversion`, `sdg-buffer-zone`, `sdg-1131`, `sdg-1322` | 同名 | 5 |
| 优先区域识别 | `priority-loss`, `priority-transition`, `priority-buffer`, `priority-emission`, `priority-human-land`, `priority-merge` | 同名 | 6 |
| 演示与帮助 | `demo`, `help` | 同名 | 2 |
| **合计** | | | **16** |

> 注：`geosdg-cli` 实际有 16 个子命令（含 `help`）。priority-buffer 覆盖 Rule 3/4 两条规则，故 6 个命令覆盖 6 条规则 + 合并。
