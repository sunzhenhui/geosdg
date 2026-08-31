# GeoSDG-Agent Copilot 交互原型

> **原型定位**：把 PEACE / GeoMap-Agent 的"读图 → 结构化 → 领域检索 → CoT 回答"闭环，改造为面向可持续发展评估的"四维决策引擎"。
>
> **对标关系**：PEACE 服务"读懂一张地质图"，本原型服务"评估一个还在演变的研究区"。
>
> **本目录不包含可执行代码**，只包含**交互原型（对话样例 + JSON schema + 架构图）**，用于对齐产品愿景和 MVP 边界。

---

## 📋 基本信息

| 项目 | 内容 |
|------|------|
| 类型 | 产品交互原型（Design Prototype） |
| 关联需求 | 把 GeoSDG 从"CLI 工具箱"升级为"决策 Copilot" |
| 分支 | `feature/agent-copilot-prototype` |
| 创建日期 | 2026-08-31 |
| 状态 | 📋 原型阶段（未编码） |

---

## 🎯 核心命题：PEACE vs GeoSDG-Agent

|维度|PEACE / GeoMap-Agent|GeoSDG-Agent（本原型）|
|---|---|---|
|**评估对象**|一张**静态地质图**|一个**动态研究区**（可干预、有多个未来）|
|**评估时间轴**|过去时（图已画好）|**过去 + 现在 + 未来 + 目标间**|
|**评估性质**|**认知型**（读懂 = 成功）|**决策型**（读懂 = 起点，行动建议 = 终点）|
|**服务对象**|地质学家、出题人|规划师、决策者、SDG 报告撰写人|
|**成功标准**|答对多选题|**改变一个真实决策**|

**一句话**：PEACE 让 AI 会读图，GeoSDG-Agent 让 AI 会评估一个地方值不值得、往哪里走。

---

## 🧭 四维评估矩阵（本原型的核心创新）

```
                  ┌─────────────────────────────────────────────────┐
                  │           GeoSDG-Agent 的评估矩阵                 │
                  ├──────────┬──────────┬──────────┬─────────────────┤
                  │ 过去     │ 现在     │ 未来      │ 目标间          │
                  │ (纵向)   │ (基线)   │ (情景)   │ (协同/权衡)     │
┌─────────────────┼──────────┼──────────┼──────────┼─────────────────┤
│ SDG 11.3.1      │ 趋势线   │ 现状值    │ SSP1-5   │ vs 15.3 / 13.2 │
│ SDG 11.7.1      │ 趋势线   │ 现状值    │ SSP1-5   │ vs 15.3        │
│ SDG 13.2.2      │ 趋势线   │ 现状值    │ SSP1-5   │ vs 11.2 / 11.3 │
│ SDG 15.3.1      │ 趋势线   │ 现状值    │ SSP1-5   │ vs 11.3        │
│ SDG 3.8/4.a/... │ 趋势线   │ 现状值    │ (可拓展) │ ...            │
└─────────────────┴──────────┴──────────┴──────────┴─────────────────┘
                        ↓
              每个格子 = 一个评估动作 = 一段可回溯对话 + 一份证据链

              PEACE 只能填第 2 列（现状），且只填 1 行（地质）
```

---

## 📁 原型文件清单

| 文件 | 对应评估维度 | PEACE 能力对标 | 说明 |
|------|------------|---------------|------|
| [`conversation-01-extracting.md`](./conversation-01-extracting.md) | 现在（基线） | Extracting | 现状提取，最基础，热身用 |
| [`conversation-02-longitudinal.md`](./conversation-02-longitudinal.md) | 过去（纵向） | — | **PEACE 做不了**：双时相趋势对比 |
| [`conversation-03-scenario.md`](./conversation-03-scenario.md) | 未来（情景） | Reasoning | **PEACE 做不了**：SSP1-5 情景推演 |
| [`conversation-04-tradeoff.md`](./conversation-04-tradeoff.md) | 目标间（协同/权衡） | — | **PEACE 做不了**：多 SDG 交叉分析 |
| [`conversation-05-expert-panel.md`](./conversation-05-expert-panel.md) | 综合决策 | Analyzing | 五专家会诊 + 冲突暴露 |
| [`evidence-schema.json`](./evidence-schema.json) | 全部 | few-shot JSON | 证据可回溯的强 schema |
| [`architecture.md`](./architecture.md) | 全部 | copilot.py 对标 | 单一入口 `sdg_copilot()` 的执行流 |

---

## 🎬 阅读顺序建议

1. **先读架构** → [`architecture.md`](./architecture.md) 建立整体印象
2. **再读 schema** → [`evidence-schema.json`](./evidence-schema.json) 理解输出契约
3. **按对话样例走一遍** → 从 01 到 05，每份都是一个真实场景
4. **回到本 README** → 对照四维矩阵检查覆盖度

---

## 👥 三类用户 × 三种价值

| 用户 | 过去的痛 | 有 Copilot 后 | 意义 |
|------|---------|--------------|------|
| 规划科员小李 | 拿到新研究区 → 跑一周 GIS → 不知道图意味着什么 → 汇报被打回 | 一句话四维评估 → 决策一页纸 → 领导拍板 | 从"数据搬运工"变"决策辅助者" |
| 规划评审专家老张 | 凭经验判断方案好坏 → 说不清就通过 | 输入方案 → 输出 SSP2 情景下 SDG 11↑ 但 15.3↓8% → 有据可依 | 从"经验评审"变"证据评审" |
| SDG 研究者小王 | 手工算 5 指标 × 3 情景 × 10 研究区 = 2 个月 | 批量评估 + 自动写 method 段 → 2 天 | SDG 空间化研究"工业化生产" |

---

## 🔗 与现有资产的映射

| 原型概念 | 复用现有资产 |
|---------|------------|
| 单一入口 `sdg_copilot()` | 新增，包裹现有 `agent-router` / `agent-planner` / `agent-executor` |
| 感知层 SDG-Meta | 扩展 `cli/src/GeoTiffInspector.cpp` + `data/manifest.json` |
| 领域检索 | 复用 `agent/skills/sdg-indicator-knowledge/` + `scripts/kb_retrieve.py` |
| 五专家 | 新建 `agent/skills/experts/*.md`，工具全部复用现有 16 个 CLI 子命令 |
| 证据链 | 新增 schema，绑定现有 CLI 输出（`data/outputs/*.tif`） |

**关键原则**：原型阶段**只加层，不改层**——现有 CLI、tool schema、skill 一行不动。

---

## 📋 变更记录

| 日期 | 版本 | 变更内容 | 变更人 |
|------|------|---------|--------|
| 2026-08-31 | v0.1 | 原型初稿：四维评估矩阵 + 5 段对话样例 + evidence schema + 架构图 | — |
