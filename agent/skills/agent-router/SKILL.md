---
name: agent-router
description: GeoSDG Agent 意图路由器。识别用户自然语言意图并路由到对应 Skill 和 Tool 体系。覆盖 6 类意图：SDG指标计算、CA精度评估、优先区域识别、知识查询、综合评估、报告生成。触发关键词：SDG评估、GeoSDG分析、空间SDG分析、精度评估、优先区域、可持续发展分析、帮我算、帮我分析、帮我评估、agent-router
---

# Agent Router Skill

## Purpose

GeoSDG Agent 的**第一入口**，分析用户自然语言输入，识别意图类别并路由到正确的 Skill 和 Tool 体系。只做**意图识别和路由**，不做任务规划和工具执行。

**RAG 检索**：路由完成后，自动触发知识库语义检索（`agent/shared/kb_retriever.py`），将 Top-K 相关知识片段注入下游 Skill/Planner 上下文。检索由 `intent-map.yaml` 中的 `rag_query_hint` 字段驱动。

## When to Use

- 用户用自然语言描述 SDG 评估需求时
- 需要判断用户意图属于哪个类别时
- 用户输入模糊，需要引导澄清时

## 意图分类表

| 意图类别 | 优先级 | 路由目标 | 典型场景 |
|---------|--------|---------|---------|
| `sdg-calc` | 1 | sdg-indicator-knowledge → planner | "算一下 SDG 15.3.1" |
| `ca-precision` | 2 | planner → executor | "这个模拟准不准" |
| `priority` | 3 | planner → executor | "找出优先保护区域" |
| `knowledge` | 4 | sdg-indicator-knowledge / paper-revision-workflow | "SDG 6.6.1 是什么" |
| `composite` | 10（兜底） | planner（综合分析） | "帮我完整分析一下北京" |
| `report` | 5 | planner → generate-report | "生成评估报告" |

## References（按需读取，不自动加载）

| 文件 | 何时读取 |
|------|---------|
| `references/intent-map.yaml` | 加载意图映射规则和匹配模式时 |

## Shared（跨 Skill 共享，按需读取）

| 文件 | 何时读取 |
|------|---------|
| `agent/shared/sdg-tool-map.md` | 需要 SDG → Tool 映射时 |

## Detail

完整执行流程（匹配规则、路由流程、测试用例）见 `SKILL-detail.md`，按需 `read_file` 加载。
