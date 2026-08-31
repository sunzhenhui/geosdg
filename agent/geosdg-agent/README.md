# GeoSDG-Agent

> **定位**：GeoSDG 工具箱之上的**决策 Copilot**。
>
> 给定一个研究区，Agent 把它切成若干分区，沿用 GeoSDG 的**指标空间化 + CA 模拟 + 优先区域识别**方法链读懂现状与未来，
> 并召集一支虚拟专家团**分层合议**，产出"哪里出了问题、该怎么办、有多可信"。

---

## 与 GeoSDG 工具箱的关系

GeoSDG-Agent **不重造轮子**，它是把 `cli/` 里已有的 SDG 指标计算与 CA 模拟能力，包装成对话式决策流：

| 层 | GeoSDG 已有能力 | GeoSDG-Agent 的新增职责 |
|----|----------------|------------------------|
| 数据 | `data/rasters/` — LUCC / 人口 / 基础设施 / 未来情景（SSP1-5） | 按分区裁剪、按任务召回 |
| 计算 | `cli/build/geosdg-cli` — `sdg-*` / `ca-*` / `priority-*` 十余子命令 | 由 `tool_pool` 统一调度 |
| 分析框架 | 论文 Sun et al. (2026) 的四维矩阵：过去 / 现在 / 未来 / 目标间 | 用**认知层专家**把四个维度串成决策 |
| 交付 | 栅格与统计结果 | 结构化 JSON（答案 + 证据 + 置信度 + 挑战） |

**一句话**：CLI 负责算，Agent 负责"看懂 + 权衡 + 交付"。

---

## GeoSDG 采用的四维分析矩阵

论文与 CLI 已经定义好了分析范式，Agent 严格沿用：

```mermaid
flowchart LR
    subgraph M["GeoSDG 四维评估矩阵"]
        direction LR
        P["<b>过去</b><br/>纵向趋势<br/>双时相对比"]
        N["<b>现在</b><br/>基线现状<br/>指标空间化"]
        F["<b>未来</b><br/>SSP1-5 情景<br/>CA 模拟"]
        C["<b>目标间</b><br/>协同 / 权衡<br/>多 SDG 交叉"]
        P --> N --> F --> C
    end
```

每个格子对应到 CLI 的一组子命令；Agent 根据决策任务自动选择走哪些格子。

---

## 决策主流程：HIE → DKI → PEDA

这不是"出题-解题"，而是"决策会诊"。Agent 的每次决策，都要沿三段式主干走一遍——
**读现状（HIE）→ 拉约束（DKI）→ 专家会诊出建议（PEDA）**：

```mermaid
flowchart TB
    U(["👤 决策请求<br/><i>region_data + task + task_type</i>"])

    subgraph HIE["🗺️ HIE — 读现状：分区提取"]
        H1["① 分区检测<br/><code>partition_detector</code><br/><small>按 LUCC/行政区切成 N 个治理分区</small>"]
        H2["② 指标计算<br/><code>indicator_calculator</code><br/><small>每分区调 geosdg-cli 算 SDG 指标</small>"]
        H3["③ UN 标签<br/><code>un_threshold_db</code><br/><small>贴 达标/预警/严重 三色标签</small>"]
        H1 --> H2 --> H3
    end

    subgraph DKI["📚 DKI — 拉约束：知识注入"]
        D1["按决策任务并行召回"]
        D2["UN 阈值"]
        D3["政策红线"]
        D4["气候情景"]
        D5["模拟精度"]
        D6["数据质量"]
        D1 --> D2 & D3 & D4 & D5 & D6
    end

    subgraph PEDA["🧠 PEDA — 出建议：专家会诊装配"]
        P1["按 task_type 取输出契约模板"]
        P2["填入 sdg_meta + 召回知识"]
        P3["交给 Moderator 组织多专家合议"]
        P1 --> P2 --> P3
    end

    A(["📦 决策会诊结果<br/><i>answer + evidence + confidence + concerns</i>"])

    U --> HIE
    HIE -- "sdg_meta.json" --> DKI
    DKI -- "knowledge" --> PEDA
    PEDA --> A
```

- **HIE** 的产出物 `sdg_meta.json` 结构固定为 `{region, partitions, legend, information}`，也是 CLI 与 Agent 之间的契约。
- **DKI** 只召回和当前决策任务相关的约束知识，避免上下文膨胀。
- **PEDA**（Partition Expert Decision Assembly）不直接调 LLM，而是把决策权交给 Moderator，由它组织多专家合议，产出**带证据链的决策建议**。

> **为什么是"会诊"不是"解题"**：GeoSDG-Agent 的产物不是一个"标准答案"，而是一份决策建议。因此任务不按题型（选择题/问答题）分，而按**决策产物形态**分：`point_estimate`（现状诊断）/ `boolean_judgment`（是否判定）/ `ranking`（优先级排序）/ `advisory_report`（会诊报告）。

---

## 认知层：11 位专家 × 3 层 × 路由表

### 专家池分层

```mermaid
flowchart TB
    subgraph L1["🎯 L1 · 核心视角（提出主答案）"]
        direction LR
        L1a["🏙️ planner<br/><small>SDG 11 国土空间规划</small>"]
        L1b["🌿 ecologist<br/><small>SDG 15/6/14 生态保护</small>"]
        L1c["💰 economist<br/><small>SDG 8/9/1 经济效益</small>"]
        L1d["👥 sociologist<br/><small>SDG 1/5/10 社会公平</small>"]
    end

    subgraph L2["📏 L2 · 领域约束（校准是否踩线）"]
        direction LR
        L2a["🌐 un<br/><small>UN 官方阈值口径</small>"]
        L2b["📜 policy<br/><small>三线一单 红线</small>"]
        L2c["🌡️ climate<br/><small>SDG 13 情景/达峰</small>"]
        L2d["🔮 simulation<br/><small>CA 模拟可信度</small>"]
        L2e["🔍 data_quality<br/><small>元数据不确定性</small>"]
    end

    subgraph L3["⚖️ L3 · 挑战校验（压低虚高信心）"]
        direction LR
        L3a["😈 devil_advocate<br/><small>找证据漏洞</small>"]
        L3b["📊 statistician<br/><small>统计显著性</small>"]
    end

    L1 --> L2 --> L3
    L3 --> M["🎙️ Moderator · 合议裁决"]
```

- **L1** 关注"该怎么办"——四种视角互为镜像
- **L2** 关注"能不能这么办"——把 L1 的方案往 UN 口径、政策红线、气候情景、模拟可信度、数据质量上一一贴
- **L3** 关注"这答案靠不靠谱"——反方 + 统计校准

> **climate vs simulation 分工**：`climate` 判断方案是否符合 2030 达峰 / 2060 中和的气候目标（结论对不对）；`simulation` 判断支撑"未来维度"的 CA+RF+Markov 模拟本身可不可信（依据牢不牢）——FoM / Kappa 是否达标、SSP 情景是否匹配、外推年限是否过长。凡涉及未来预测的决策任务，二者一起到场。

### 按任务类型精准召唤

不是所有专家每次都到场——每种 `task_type` 对应一份专家清单，由 `agents/routing.py::ROUTING_TABLE` 定义：

| 决策任务 | 走哪些维度 | L1 到场 | L2 到场 | L3 到场 | 合计 |
|------|-----------|--------|--------|--------|:----:|
| `diagnosing-indicator_value` | 现在 | planner | un, data_quality | — | 3 |
| `comparing-partition_comparison` | 现在 | planner | un | statistician | 3 |
| `reasoning-longitudinal_trend` | 过去 → 现在 | planner | un, climate | statistician | 4 |
| `reasoning-scenario_forecast` | 现在 → 未来 | planner, ecologist | climate, simulation, data_quality | devil_advocate | 6 |
| `reasoning-tradeoff_detection` | **目标间** | **全部 L1** | un, policy | devil_advocate | 7 |
| `analyzing-priority_area` | 现在 + 目标间 | **全部 L1** | un, policy, simulation, data_quality | devil_advocate, statistician | **10** |
| `analyzing-intervention_plan` | 现在 → 未来 + 目标间 | **全部 L1** | policy, climate, simulation | devil_advocate | 8 |
| `analyzing-policy_conflict` | 现在 + 目标间 | planner, ecologist | policy | devil_advocate | 4 |

**简单任务少召人（读一个现状值只需 3 位），决策任务召全班（分区优先级要 10 位过一遍）**——省 token，降噪，可解释。凡涉及"未来维度"的任务（`scenario_forecast` / `priority_area` / `intervention_plan`），`simulation` 必到场为模拟结果背书或亮红灯。

### Moderator 合议流程

```mermaid
sequenceDiagram
    autonumber
    participant P as PEDA
    participant M as Moderator
    participant L1 as L1 核心专家
    participant L2 as L2 领域专家
    participant L3 as L3 挑战专家

    P->>M: deliberate(task, task_type, sdg_meta, knowledge)
    M->>M: 查 ROUTING_TABLE，锁定本任务的专家名单

    Note over M,L3: 分层调度，按 L1 → L2 → L3 顺序执行
    M->>L1: assess(...) × N
    L1-->>M: ExpertOpinion（answer / evidence / confidence）
    M->>L2: assess(...) × N
    L2-->>M: ExpertOpinion（constraints / concerns）
    M->>L3: assess(...) × N
    L3-->>M: ExpertOpinion（challenges）

    M->>M: 合议裁决<br/>· L1 置信度加权投票 → 主答案<br/>· L2 汇总为 constraints<br/>· L3 汇总为 challenges<br/>· L3 concerns 惩罚整体 confidence
    M-->>P: {answer, confidence, reasoning, evidence, concerns, experts_invoked}
```

### 每位专家的统一契约

所有专家继承 `agents/base.py::ExpertBase`：

```python
class ExpertBase:
    role: str                        # "planner" / "ecologist" / ...
    layer: str                       # "L1" / "L2" / "L3"
    focus_indicators: tuple[str,...] # 该专家只看的 SDG 指标
    system_prompt: str               # 独立人设

    def assess(task, task_type, sdg_meta, knowledge) -> ExpertOpinion
    def get_knowledge(indicators) -> dict           # 兼作 DKI 的知识源
```

`focus_indicators` 是一个精简技巧：Moderator 送进各专家的 legend 会被裁到只保留该专家关心的指标（例如 `EcologistExpert` 只看 `SDG_15_3_1` / `SDG_6_6_1`），避免专家越权或被无关信息干扰。

---

## 目录结构

```
agent/geosdg-agent/
├── copilot.py                     # 入口 sdg_copilot(region_data, task, task_type)
│
├── modules/                       # 决策主流程
│   ├── HIE.py                     # 分区提取 + 指标计算 + UN 标签
│   ├── DKI.py                     # 按决策任务召回约束知识
│   └── PEDA.py                    # 拼输出契约 → 交给 Moderator 合议
│
├── agents/                        # 认知层（11 专家 + 编排）
│   ├── base.py                    # ExpertBase + ExpertOpinion
│   ├── routing.py                 # task_type → 专家召唤矩阵
│   ├── moderator.py               # 合议编排 + 加权投票 + 证据合并
│   │
│   ├── planner_expert.py          # L1  SDG 11 国土空间规划
│   ├── ecologist_expert.py        # L1  SDG 15/6/14 生态保护
│   ├── economist_expert.py        # L1  SDG 8/9/1 经济效益
│   ├── sociologist_expert.py     # L1  SDG 1/5/10 社会公平
│   │
│   ├── un_expert.py               # L2  UN 官方阈值口径
│   ├── policy_expert.py           # L2  中国"三线一单"红线
│   ├── climate_expert.py          # L2  SDG 13 情景/达峰/碳汇
│   ├── simulation_expert.py       # L2  CA+RF+Markov 模拟可信度
│   ├── data_quality_expert.py     # L2  数据质量与不确定性
│   │
│   ├── devil_advocate_expert.py   # L3  找证据链漏洞
│   └── statistician_expert.py     # L3  统计显著性与置信度
│
├── tool_pool/                     # 对接 GeoSDG CLI
│   ├── partition_detector.py      # 把研究区切成 N 个分区
│   ├── indicator_calculator.py    # 每分区调 geosdg-cli 算 SDG 指标
│   └── un_threshold_db.py         # UN 阈值/达标口径知识库
│
├── benchmark/                     # 决策质量评测
│   ├── cases/*.json               # 评测样例（研究区 + 决策任务 + 标准答案）
│   ├── scorer.py                  # 三维打分器（命中/证据/一致性）
│   ├── run_benchmark.py           # 跑批器
│   └── README.md
│
└── utils/                         # 共享工具
    ├── prompt.py                  # 12 种决策任务 × 4 种产物形态的输出契约
    ├── common.py                  # 路径/日志/RAI 过滤
    ├── api.py                     # LLM chat/select 抽象层
    └── vision.py                  # 栅格 IO 与裁剪
```

---

## 支持的 SDG 指标

`tool_pool/indicator_calculator.py` 的白名单与 CLI 现有能力对齐：

| 指标 | 语义 | 走哪个 CLI 子命令 |
|------|------|------------------|
| `SDG_2_4_1`  | 农业可持续性 | `sdg-agriculture` |
| `SDG_6_6_1`  | 水体范围变化 | `sdg-water-extent` |
| `SDG_11_3_1` | 土地消耗率 / 人口增长率 | `sdg-land-proportion` |
| `SDG_13_2_2` | 温室气体排放 | `sdg-emission` |
| `SDG_15_3_1` | 土地退化 | `sdg-land-degradation` |

新增指标 = 往这张表加一行 + 把对应专家的 `focus_indicators` 打开，其它不用动。

---

## 快速上手

```python
from agent.geosdg_agent.copilot import sdg_copilot

region_data = {
    "name": "Demo City",
    "bbox": (113.0, 22.0, 114.5, 23.5),
    "lucc_path": "data/rasters/lucc/2020.tif",
    "year": 2020,
}

result = sdg_copilot(
    region_data,
    task="在当前分区中，哪几个应被列为优先干预区？",
    task_type="analyzing-priority_area",
)
```

对于 `analyzing-priority_area` 任务，返回结构：

```json
{
  "task_type": "analyzing-priority_area",
  "answer": ["A1", "A3"],
  "confidence": 0.72,
  "reasoning": {
    "core_views":  [ {"role":"planner",...}, {"role":"ecologist",...}, ... ],
    "constraints": [ {"role":"un",...}, {"role":"policy",...}, ... ],
    "challenges":  [ {"role":"devil_advocate",...}, {"role":"statistician",...} ]
  },
  "evidence": [ ... ],
  "concerns": [ ... ],
  "experts_invoked": ["planner","ecologist","economist","sociologist",
                      "un","policy","simulation","data_quality",
                      "devil_advocate","statistician"]
}
```

直接跑 smoke test（作为包运行，因内部使用相对导入）：

```bash
cd agent && python -m geosdg-agent.copilot     # 若目录名带连字符，见 benchmark/run_benchmark.py 的 importlib 加载法
```

---

## 决策质量评测（Benchmark）

配套一套可运行的评测集，用来量化"Agent 给的决策建议好不好"——而不是"做题对不对"。

```bash
python agent/geosdg-agent/benchmark/run_benchmark.py            # 跑全部 case
python agent/geosdg-agent/benchmark/run_benchmark.py --runs 3   # 每个 case 跑 3 次测一致性
```

评三个维度（详见 `benchmark/README.md`）：

| 维度 | 权重 | 含义 |
|------|:----:|------|
| **accuracy** 决策命中 | 0.4 | `answer` 是否覆盖标准答案的关键结论 |
| **evidence** 证据覆盖 | 0.4 | `evidence` 是否引用 gold 要求的分区/指标，置信度是否达标 |
| **consistency** 结论一致 | 0.2 | 同一任务多次会诊结论是否稳定 |

评测样例放在 `benchmark/cases/*.json`，每条含 `{region, task, task_type, gold}`；
加新 case 只需新增一个 JSON、`task_type` 用合法 key，无需改代码。

---

## 当前状态

**骨架阶段**：所有 `.py` 可 import，`copilot.py` 可跑通空流程，
底层工具与 LLM 调用为 mock（假数据 + echo 桩），接口与数据结构已按最终形态定型。

后续填肉建议顺序：

1. `tool_pool/partition_detector.py` — 用 GDAL 读 `data/rasters/lucc/*.tif`，按 LUCC 类型/形态学聚类分区
2. `tool_pool/indicator_calculator.py` — `subprocess` 调 `cli/build/bin/geosdg-cli` 算真实 SDG 值
3. `tool_pool/un_threshold_db.py` — 灌入论文 Table 里的 UN 官方阈值
4. `utils/api.py` — 接真实 LLM（OpenAI / 混元 / 文心等任选）
5. **可选升级：二轮争辩** — `devil_advocate` 拿 L1 结果反驳，Moderator 做二次裁决
6. `benchmark/cases/` — 补充带真实 gold 的评测样例，用 `run_benchmark.py` 持续跟踪决策质量随迭代的变化
