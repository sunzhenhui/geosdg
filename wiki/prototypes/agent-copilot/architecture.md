# GeoSDG-Agent Copilot 架构原型

> 对标 PEACE `copilot.py::copilot()` 的三段式（HIE → DKI → PEQA），落到 SDG 场景。

---

## 一、整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                        用户（规划师 / 研究者）                          │
│  "帮我评估 A 区 2050 年 SSP2 情景下的 SDG 11 表现"                      │
└──────────────────────────┬───────────────────────────────────────────┘
                           │ 自然语言 + 研究区路径
                           ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      sdg_copilot()  单一入口                          │
│                (scripts/sdg_copilot.py，对标 PEACE copilot.py)         │
└──────────────────────────┬───────────────────────────────────────────┘
                           │
     ┌─────────────────────┼─────────────────────┐
     │                     │                     │
     ▼                     ▼                     ▼
┌─────────┐          ┌──────────┐          ┌──────────┐
│  HIE    │          │   DKI    │          │  PEQA    │
│ 感知层  │          │ 领域检索  │          │ 问答推理  │
└────┬────┘          └─────┬────┘          └─────┬────┘
     │                     │                     │
     │  sdg_meta.json      │  knowledge          │  answer + evidence
     │  (研究区数字化)      │  (三层证据)          │  (决策级输出)
     │                     │                     │
     ▼                     ▼                     ▼
```

---

## 二、三层详解

### 2.1 HIE-SDG：感知层（对标 PEACE `HIE.digitalize()`）

**输入**：研究区目录 `data/rasters/`
**输出**：`.codebuddy/sessions/<region>/sdg_meta.json`

```
┌──────────────────────────────────────────────────────────┐
│  geosdg-cli inspect --region data/                       │
│  ├─ 扫描 data/rasters/*.tif                              │
│  ├─ 读取 manifest.json                                    │
│  ├─ 计算 bbox / resolution / area                        │
│  ├─ 统计各图层类别占比、时相覆盖                            │
│  └─ 判断每个 SDG 指标是否 "ready"                          │
└──────────────────────────────────────────────────────────┘
                          ↓
{
  "region": "demo",
  "extent": {"min_lon": ..., "max_lon": ..., ...},
  "layers": {
    "lucc": {"years": [2010, 2020], "class_ratio_2020": {...}},
    "pop":  {"years": [2010, 2020], "total_2020": 1.2e6},
    "infra": {...},
    "future": {"scenarios": ["ssp1"..."ssp5"], "years": [2025...2050]}
  },
  "sdg_readiness": {
    "sdg_11_3_1": {"ready": true},
    "sdg_15_3_1": {"ready": false, "reason": "缺少 NDVI"}
  }
}
```

**PEACE 对应**：把地质图切成 main_map/legend/title 等组件 + OCR + 岩性分区
**GeoSDG 对应**：把研究区栅格集切成 lucc/pop/infra/future 等图层 + 统计 + SDG 就绪度

---

### 2.2 DKI-SDG：领域检索层（对标 PEACE `DKI.consult()`）

**输入**：问题 + sdg_meta
**输出**：三层证据包

```
┌──────────────────────────────────────────────────────────┐
│  question + sdg_meta                                     │
│       │                                                   │
│       ▼                                                   │
│  ┌─────────────────────────────────────────┐             │
│  │ router.classify(question) → {           │             │
│  │   sdg_targets: ["11.3.1", "13.2.2"],   │             │
│  │   time_axis:   "future",                │             │
│  │   scenario:    "ssp2",                  │             │
│  │   ability:     "reasoning"              │             │
│  │ }                                       │             │
│  └─────────────────────────────────────────┘             │
│                                                           │
│  三路并行检索：                                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌───────────┐ │
│  │ UN 指标手册      │  │ 本地空间图层     │  │ 政策文本   │ │
│  │ (RAG)           │  │ (manifest 过滤) │  │ (RAG)     │ │
│  └────────┬────────┘  └────────┬────────┘  └─────┬─────┘ │
│           │                    │                  │        │
│           └────────────┬───────┴──────────────────┘        │
│                        ▼                                   │
│              LLM 二次筛选（避免 prompt 冲爆）                │
│                        │                                   │
│                        ▼                                   │
│                   knowledge                                │
└──────────────────────────────────────────────────────────┘
```

**PEACE 对应**：按 bbox 查断层/地震/土地覆盖/人口，LLM 挑相关的
**GeoSDG 对应**：按 bbox + SDG 目标查 UN 手册 + 本地图层 + 政策文本，LLM 挑相关的

---

### 2.3 PEQA-SDG：问答推理层（对标 PEACE `PEQA.answer()`）

**输入**：sdg_meta + knowledge + question
**输出**：符合 `evidence-schema.json` 的答案对象

```
┌──────────────────────────────────────────────────────────────┐
│                    能力路由（5 类）                             │
│  ┌─────────────┬──────────────────────────────────────────┐  │
│  │ extracting  │ 直接查 meta，无需推理                       │  │
│  │ grounding   │ 调 sdg-* / priority-* CLI 出栅格            │  │
│  │ referring   │ 走 RAG (indicator-knowledge)                │  │
│  │ reasoning   │ 编排 pipeline: CA → SDG → 对比              │  │
│  │ analyzing   │ 五专家会诊（见下）                            │  │
│  └─────────────┴──────────────────────────────────────────┘  │
│                          ↓                                    │
│                   分派给专家团                                  │
│                          ↓                                    │
│  ┌──────────────────────────────────────────────────────┐    │
│  │  land_use_expert    │ SDG 11.3, 15.3                 │    │
│  │  population_expert  │ SDG 1, 10, 11 人地关系          │    │
│  │  infra_expert       │ SDG 3.8/4.a/6.1/9.c/11.2/11.7  │    │
│  │  climate_expert     │ SDG 13.2                        │    │
│  │  policy_expert      │ 综合裁决 + 报告生成              │    │
│  └──────────────────────────────────────────────────────┘    │
│                          ↓                                    │
│              各自输出 {stance, evidence, confidence, caveats}  │
│                          ↓                                    │
│              policy_expert 汇总 → 冲突暴露 → 一致意见           │
│                          ↓                                    │
│                    符合 schema 的最终 answer                    │
└──────────────────────────────────────────────────────────────┘
```

**PEACE 对应**：3 专家 + 5 能力 + few-shot JSON
**GeoSDG 对应**：5 专家 + 5 能力 + evidence-schema.json + **冲突暴露**（GeoSDG 独有）

---

## 三、单入口 `sdg_copilot()` 伪代码

```python
# scripts/sdg_copilot.py（原型阶段，未实现）
def sdg_copilot(region_path: str, question: str,
                copilot_modes=("HIE", "DKI", "PEQA"),
                session_id: str = None) -> dict:
    """
    对标 PEACE copilot.py::copilot()

    :return: 符合 wiki/prototypes/agent-copilot/evidence-schema.json 的 dict
    """
    session = load_or_create_session(region_path, session_id)

    # ── HIE：感知层（首次进入研究区时跑一次，后续复用缓存）
    if "HIE" in copilot_modes:
        if not session.has("sdg_meta"):
            session["sdg_meta"] = run_cli("geosdg-cli inspect", region_path)

    # ── 路由（问题分类）
    intent = router.classify(question, session["sdg_meta"])
    #   intent = {sdg_targets, time_axis, scenario, ability}

    # ── DKI：领域检索
    knowledge = None
    if "DKI" in copilot_modes:
        knowledge = knowledge_injector.inject(question, session["sdg_meta"], intent)

    # ── PEQA：能力路由 + 专家分派 + 证据汇总
    answer = peqa.answer(
        sdg_meta = session["sdg_meta"],
        knowledge = knowledge,
        question = question,
        intent = intent,
    )

    # ── 增量缓存（what-if 追问的加速关键）
    session.append_evidence(answer["evidence"])
    session.save()

    return answer  # 符合 evidence-schema.json
```

---

## 四、增量执行（GeoSDG 独有：让研究区"活"起来）

**PEACE 一图一问一答**；**GeoSDG 一区多问，且问答之间可以增量。**

```
第 1 轮：用户问 SSP2 2050 → 全量跑 → 缓存证据 A
第 2 轮：用户改问 SSP3 2050 → planner 检测 diff = 情景切换
        → 只重跑 sdg-1131(ssp3) + sdg-1322(ssp3)
        → 复用缓存的 UN 手册、政策文本、meta
        → 3 秒出结果（对比：全量 20 分钟）
第 3 轮：用户问"如果生态红线扩大 10%" → planner 检测 diff = 参数
        → 只跑 priority-merge 重算
        → 复用所有指标计算结果
```

这是**决策 Copilot** 相对于 **一次性评估工具** 的核心体验差异。

---

## 五、与 PEACE `copilot.py` 的逐行对照

| PEACE 代码 | GeoSDG 对应 | 差异点 |
|-----------|------------|--------|
| `hie.digitalize(image_path)` | `geosdg-cli inspect` + 缓存 | 输入从图变成"栅格集目录" |
| `dki.consult(question, information)` | `knowledge_injector.inject(...)` | 检索 3 路：UN手册 + 空间图层 + 政策 |
| `peqa.answer(...)` | `peqa.answer(...)` | 增加**专家团 + 冲突暴露** |
| 返回单个 answer 字符串 | 返回 `evidence-schema` 对象 | 强制证据可回溯 |
| — | `session.save()` | **增量执行**（PEACE 无） |

---

## 六、MVP 边界（原型阶段不要越界）

✅ **原型阶段做**：
- 四份对话样例走通 happy path
- evidence-schema.json 强 schema 落地
- 架构图 + 一句话说清每层职责

❌ **原型阶段不做**：
- 真的实现 `sdg_copilot()`（P4 期任务）
- 真的接入 LLM
- 真的做 benchmark

**目的**：先用交互原型对齐产品愿景，再谈实现。

---

## 📋 变更记录

| 日期 | 版本 | 变更内容 | 变更人 |
|------|------|---------|--------|
| 2026-08-31 | v0.1 | 架构原型初稿，对齐 PEACE copilot.py 的三段式 | — |
