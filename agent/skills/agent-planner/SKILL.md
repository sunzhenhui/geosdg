---
name: agent-planner
description: GeoSDG 任务规划器。将用户 SDG 评估意图拆解为多步工具调用链，采用 ReAct 模式自主规划与执行。支持线性/并行/条件/循环四种规划模式。触发关键词：评估规划、自动分析、综合评估、帮我做、帮我算、帮我分析、完整评估、agent-planner
---

# Agent Planner Skill

## Purpose

基于 **ReAct（Reasoning + Acting）** 模式，将用户意图转化为有序的多步执行计划并逐步执行。

## When to Use

- agent-router 识别为操作类意图（sdg-calc / ca-precision / priority / composite）后自动加载
- 用户直接表达多步骤需求（如"帮我完整评估"）
- 需要根据中间结果决定后续步骤时

## 规划模式表

| 意图 | 默认模式 | 说明 |
|------|---------|------|
| sdg-calc（单一指标） | linear | 知识查询 → 数据检查 → 计算 → 解读 |
| sdg-calc（多区域对比） | parallel | 各区域独立计算 → 汇总对比 |
| ca-precision | linear | 数据检查 → ca-precision → 可选统计检验 → 解读 |
| priority | linear + loop | 规则 1-6 逐个执行 → merge → 解读 |
| composite | 递归 + 报告生成 | 拆解为子意图 → 自动追加报告生成 |
| knowledge | 无规划 | 纯检索，直接返回 |

## References（按需读取，不自动加载）

| 文件 | 何时读取 |
|------|---------|
| `references/plan-strategies.md` | 选择规划模式、Linear/Parallel/Conditional/Loop 模板时 |

## Shared（跨 Skill 共享，按需读取）

| 文件 | 何时读取 |
|------|---------|
| `agent/shared/data-semantics.md` | data_check 步骤需要追问 region/year 时 |

## Detail

完整执行流程（ReAct 循环、参数引用、降级策略、示例）见 `SKILL-detail.md`，按需 `read_file` 加载。
