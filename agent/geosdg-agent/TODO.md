# GeoSDG-Agent 实现补全 TODO

> 版本: `0.1.0-skeleton` → `0.2.0-functional`
> 生成时间: 2026-09-01
> 状态标记: 🔴 未实现 / 🟡 Mock/硬编码 / 🟢 已实现

---

## 总览

当前骨架阶段存在 **三层 mock 级联**，需逐层填肉：

```
LLM 层（最根本）  →  知识层（专家 get_knowledge）  →  工具层（CLI/GDAL）
     ↓                      ↓                            ↓
 MockProvider          4/5 专家返回硬编码            4/5 指标走 MOCK_VALUES
```

**优先级原则**：LLM 注入 > 指标计算真实化 > 专家知识填充 > 高级合议机制

---

## 一、LLM 层

### 1.1 接入真实 LLM

- [ ] **`utils/api.py`** — 默认 Provider 从 MockProvider 切换为 OpenAICompatibleProvider
  - 当前: `_provider = MockProvider()` (L70-71)
  - 目标: 支持环境变量 `GEOSDG_LLM_API_KEY` / `GEOSDG_LLM_BASE_URL` 自动初始化
  - 预估: 小

- [ ] **`utils/api.py:88-94`** — `select()` 实现真正的相关性过滤
  - 当前: `_ = question`，忽略问题直接全量返回
  - 目标: 用 LLM 从 candidates 中挑出与 question 相关的子集
  - 预估: 中

- [ ] **`agents/base.py:79`** — 专家 `assess()` 传真实 model 名
  - 当前: `api.chat(messages, model=f"mock-{self.role}")`
  - 目标: 从配置读取默认 model，或由 Moderator 按专家层级分配不同 model
  - 预估: 小

### 1.2 RAI 安全过滤

- [ ] **`utils/common.py:47-53`** — `rai_filter()` 实现真实内容安全检测
  - 当前: 永远返回 `False`（放行），`_ = question`
  - 目标: 接入敏感词/政治红线检测
  - 预估: 中
  - 优先级: 低（功能不受阻）

---

## 二、专家层

### 2.1 纯壳专家（无 `get_knowledge`，仅有 `role/layer/system_prompt/focus_indicators`）

#### 2.1.1 L1 核心视角专家

- [ ] **`agents/planner_expert.py`** — PlannerExpert 补全
  - 当前: 17 行纯壳，`focus_indicators = ("SDG_11_3_1",)`
  - 缺失: `get_knowledge()` 方法 — 应返回国土空间规划相关政策与标准
  - 缺失: 专属 `system_prompt` — 当前用基类通用 prompt，应注入规划师视角
  - 缺失: 专属 `output_schema` — 当前用基类通用 schema
  - 预估: 中

- [ ] **`agents/ecologist_expert.py`** — EcologistExpert 补全
  - 当前: 17 行纯壳，`focus_indicators = ("SDG_15_3_1", "SDG_6_6_1")`
  - 缺失: `get_knowledge()` — 应返回生态保护红线、生物多样性阈值
  - 缺失: 专属 `system_prompt` — 生态学家视角
  - 缺失: 专属 `output_schema`
  - 预估: 中

- [ ] **`agents/economist_expert.py`** — EconomistExpert 补全
  - 当前: 17 行纯壳，`focus_indicators = ("SDG_2_4_1", "SDG_13_2_2")`
  - 缺失: `get_knowledge()` — 应返回经济效益评估标准、碳交易价格
  - 缺失: 专属 `system_prompt` — 经济学家视角
  - 缺失: 专属 `output_schema`
  - 预估: 中

- [ ] **`agents/sociologist_expert.py`** — SociologistExpert 补全
  - 当前: 17 行纯壳，`focus_indicators = ()`（关注全指标）
  - 缺失: `get_knowledge()` — 应返回社会公平指标、人口统计参考
  - 缺失: 专属 `system_prompt` — 社会学家视角
  - 缺失: 专属 `output_schema`
  - 预估: 中

#### 2.1.2 L3 挑战校验专家

- [ ] **`agents/devil_advocate_expert.py`** — DevilAdvocateExpert 补全
  - 当前: 24 行，有 `output_schema`，无 `get_knowledge`
  - 缺失: `get_knowledge()` — 应返回常见逻辑谬误清单、证据链审查标准
  - 缺失: 专属 `system_prompt` — 反方视角
  - 预估: 小

- [ ] **`agents/statistician_expert.py`** — StatisticianExpert 补全
  - 当前: 22 行，有 `output_schema`，无 `get_knowledge`
  - 缺失: `get_knowledge()` — 应返回统计显著性标准、样本量要求
  - 缺失: 专属 `system_prompt` — 统计学家视角
  - 预估: 小

### 2.2 Mock 知识专家（有 `get_knowledge` 但返回硬编码假数据）

- [ ] **`agents/climate_expert.py:22-28`** — ClimateExpert 真实化
  - 当前: `_ = indicators` 忽略输入，返回硬编码碳峰2030/碳中和2060/SSP情景
  - 目标: 按指标查询真实气候情景数据（可从 `data/rasters/` 或外部 API 获取）
  - 预估: 大

- [ ] **`agents/data_quality_expert.py:26-28`** — DataQualityExpert 真实化
  - 当前: 返回固定 coverage=0.94 / grade="B"
  - 目标: 从 sdg_meta 中计算真实数据覆盖率、时间对齐度、质量评级
  - 预估: 中

- [ ] **`agents/policy_expert.py:21-27`** — PolicyExpert 真实化
  - 当前: `_ = bbox` 忽略输入，返回固定红线覆盖率 0.32/0.21/0.18
  - 目标: 按 bbox 查询政策图层（三线一单矢量数据），计算真实覆盖率
  - 预估: 大（需政策矢量数据源）

- [ ] **`agents/simulation_expert.py:45-54`** — SimulationExpert 真实化
  - 当前: `_ = indicators` 忽略输入，返回固定 FoM=0.21/Kappa=0.83/OA=0.88
  - 目标: 从 CA 模拟结果中读取真实精度指标（调 `geosdg-cli ca-*` 命令）
  - 预估: 大

### 2.3 已有真实逻辑的专家

- [ ] **`agents/un_expert.py`** — UnExpert 优化
  - 当前: 🟢 唯一有真实 `get_knowledge`，查询 `UnThresholdDB`
  - 优化: `UnThresholdDB` 数据源从硬编码迁移到外部 YAML/JSON（见 3.3）
  - 预估: 小

---

## 三、工具层

### 3.1 指标计算器（🔴 重灾区）

- [ ] **`tool_pool/indicator_calculator.py:48-50`** — CLI_SUBCOMMANDS 补全
  - 当前: 仅 `SDG_11_3_1` 接了真实 CLI (`sdg-1131`)
  - 缺失:
    - `SDG_2_4_1` → `sdg-agriculture`
    - `SDG_6_6_1` → `sdg-water-extent`
    - `SDG_13_2_2` → `sdg-emission`
    - `SDG_15_3_1` → `sdg-land-degradation`
  - 预估: 中（需确认 CLI 子命令参数格式）

- [ ] **`tool_pool/indicator_calculator.py:62-68`** — 移除 MOCK_VALUES
  - 当前: 5 个指标硬编码假值
  - 目标: 所有指标走真实 CLI，MOCK_VALUES 仅作 CLI 不可用时的 fallback
  - 预估: 小（依赖 3.1 上一条）

- [ ] **`tool_pool/indicator_calculator.py:122-161`** — 减少 fallback mock 路径
  - 当前: 6 条 fallback 到 mock 的路径（缺文件/CLI不存在/异常/返回码非零/解析失败/缺裁剪片）
  - 目标: 缺文件和 CLI 不存在应抛异常而非静默 mock；解析失败保留 fallback
  - 预估: 中

### 3.2 分区检测器

- [ ] **`tool_pool/partition_detector.py:99-111`** — 智能分区策略
  - 当前: `_grid_partitions` 永远切成 2×2+中心=5 个固定分区(A1-A5)，kind 全部为 `"others"`
  - 目标: 按 LUCC 类型/形态学聚类/行政区智能分区
  - 预估: 大

- [ ] **`tool_pool/partition_detector.py:81`** — 移除硬编码默认 bbox
  - 当前: `return region_data.get("bbox", (113.0, 22.0, 114.5, 23.5))`
  - 目标: 无 bbox 时应从 LUCC 栅格读取真实四至
  - 预估: 小

### 3.3 UN 阈值知识库

- [ ] **`tool_pool/un_threshold_db.py:33-70`** — 数据源外挂
  - 当前: `_TABLE` 硬编码在代码中
  - 目标: 迁移到 `data/knowledge/un_thresholds.json` 或 YAML，代码从文件加载
  - 预估: 小

### 3.4 栅格 IO

- [ ] **`utils/vision.py:65-69`** — 移除 GDAL 不可用时的硬编码 fallback
  - 当前: 返回固定 1024×768 / bbox珠三角 / EPSG:4326
  - 目标: GDAL 不可用时应抛异常或返回明确错误，而非静默返回假数据
  - 预估: 小

- [ ] **`utils/vision.py:112-117`** — `crop_by_bbox` mock 占位文件
  - 当前: GDAL 不可用时写 `.mock` 后缀文本文件
  - 目标: GDAL 不可用时抛异常
  - 预估: 小

- [ ] **`utils/vision.py:184`** — `raster_to_prompt_asset` 返回真实图像
  - 当前: 只返回结构化元信息
  - 目标: 返回 base64 缩略图或对象存储 URL，供多模态 LLM 使用
  - 预估: 中

---

## 四、模块层

### 4.1 DKI 知识注入

- [ ] **`modules/DKI.py:59-60`** — 知识过滤
  - 当前: `api.select(question, knowledge)` 全量返回
  - 目标: 实现真正的相关性筛选（依赖 1.1 `select()` 实现）
  - 预估: 中

### 4.2 PEDA 专家决策装配

- [ ] **`modules/PEDA.py:67-76`** — 智能分区选择
  - 当前: `_ = task_type`，永远返回全部分区
  - 目标: 按 task_type 和问题语义选择相关分区
  - 预估: 中

- [ ] **`modules/PEDA.py:41`** — baseline 路径 mock
  - 当前: `api.chat(messages, model="mock-baseline")`
  - 目标: 接入真实 LLM
  - 预估: 小（依赖 1.1）

### 4.3 Moderator 合议

- [ ] **`agents/moderator.py`** — 二轮争辩机制
  - 当前: 骨架版，L1→L2→L3 单轮顺序执行
  - 目标: devil_advocate 拿 L1 结果反驳，Moderator 做二次裁决
  - 预估: 大

---

## 五、硬编码假数据清理

| # | 位置 | 假数据 | 目标 |
|---|------|--------|------|
| 1 | `indicator_calculator.py:62-68` | 5 个指标固定值 | 走真实 CLI |
| 2 | `indicator_calculator.py:53-59` | 4 个固定栅格路径 | 从 region_data 动态解析 |
| 3 | `climate_expert.py:23-28` | 碳峰2030/碳中和2060/SSP | 查真实气候数据 |
| 4 | `data_quality_expert.py:26-28` | coverage=0.94/grade="B" | 从 meta 计算 |
| 5 | `policy_expert.py:22-27` | 三线覆盖率 0.32/0.21/0.18 | 查政策矢量图层 |
| 6 | `simulation_expert.py:46-54` | FoM=0.21/Kappa=0.83/OA=0.88 | 从模拟结果读取 |
| 7 | `vision.py:65-69` | 1024×768/bbox珠三角 | 抛异常而非假数据 |
| 8 | `partition_detector.py:81` | 默认 bbox 珠三角 | 从栅格读取四至 |
| 9 | `un_threshold_db.py:33-70` | 5 个指标阈值硬编码 | 外挂 JSON/YAML |
| 10 | `api.py:137-142` | MockProvider 固定 JSON | 接真实 LLM |
| 11 | `copilot.py:95-101` | Demo City / 珠三角坐标 | 仅作 demo，保留 |

---

## 六、忽略输入参数修复

| # | 文件 | 行号 | 被忽略参数 | 修复方式 |
|---|------|------|-----------|---------|
| 1 | `climate_expert.py` | 22 | `indicators` | 按指标筛选相关气候情景 |
| 2 | `policy_expert.py` | 21 | `bbox` | 按 bbox 查询政策图层 |
| 3 | `simulation_expert.py` | 45 | `indicators` | 按指标筛选相关模拟精度 |
| 4 | `PEDA.py` | 73 | `task_type` | 按 task_type 选择相关分区 |
| 5 | `api.py` | 93 | `question` | 用 LLM 做相关性过滤 |
| 6 | `common.py` | 52 | `question` | 接入内容安全检测 |

---

## 七、建议实施顺序

```
Phase 1 — LLM 可用（让专家"能思考"）
  ├── 1.1 接入真实 LLM（环境变量自动初始化）
  ├── 1.2 专家 assess() 传真实 model
  └── 1.3 baseline 路径接真实 LLM

Phase 2 — 指标真实化（让 HIE "有真数据"）
  ├── 2.1 CLI_SUBCOMMANDS 补全 4 个指标
  ├── 2.2 减少 fallback mock 路径
  └── 2.3 移除硬编码默认 bbox

Phase 3 — 专家知识填充（让 DKI "有真知识"）
  ├── 3.1 6 个纯壳专家补 get_knowledge + 专属 prompt
  ├── 3.2 4 个 mock 知识专家真实化
  └── 3.3 api.select() 实现过滤

Phase 4 — 高级机制
  ├── 4.1 智能分区策略
  ├── 4.2 Moderator 二轮争辩
  ├── 4.3 PEDA 智能分区选择
  └── 4.4 raster_to_prompt_asset 多模态支持

Phase 5 — 工程化
  ├── 5.1 UN 阈值数据外挂 JSON
  ├── 5.2 GDAL 不可用时抛异常而非静默 mock
  ├── 5.3 rai_filter() 实现真实检测
  └── 5.4 硬编码假数据全部清理
```

---

## 八、当前真实可用逻辑清单

以下逻辑已实现且可工作（需对应环境依赖）：

| 逻辑 | 文件 | 依赖 |
|------|------|------|
| GDAL 栅格读取/裁剪/直方图 | `utils/vision.py` | GDAL 库 |
| SDG_11_3_1 CLI 指标计算 | `tool_pool/indicator_calculator.py` | `geosdg-cli` 二进制 + 真实栅格 |
| UN 阈值分类判断 | `tool_pool/un_threshold_db.py:classify()` | 无 |
| Moderator 加权投票合议编排 | `agents/moderator.py` | LLM |
| LegendDB 外部 JSON 加载 | `tool_pool/legend_db.py` | JSON 图例文件 |
| OpenAI 兼容 LLM 调用 | `utils/api.py:OpenAICompatibleProvider` | API Key |
| HIE 三段编排（分区→指标→标签） | `modules/HIE.py` | 上述工具链 |
