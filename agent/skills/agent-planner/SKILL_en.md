---
name: agent-planner
description: GeoSDG task planner. Decomposes the user's SDG assessment intent into a multi-step tool-call chain and autonomously plans and executes using the ReAct pattern. Supports four planning modes: linear, parallel, conditional, and loop. Trigger keywords: assessment planning, automatic analysis, composite assessment, help me do, help me calculate, help me analyze, full assessment, agent-planner
---

# Agent Planner Skill

## Purpose

Based on the **ReAct (Reasoning + Acting)** pattern, converts user intent into an ordered multi-step execution plan and executes it step by step.

## When to Use

- Automatically loaded after agent-router recognizes an operational intent (sdg-calc / ca-precision / priority / composite)
- When the user directly expresses a multi-step need (e.g., "fully assess it for me")
- When subsequent steps must be decided based on intermediate results

## Planning Mode Table

| Intent | Default mode | Description |
|--------|--------------|-------------|
| sdg-calc (single indicator) | linear | Knowledge query → data check → compute → interpret |
| sdg-calc (multi-region comparison) | parallel | Each region computes independently → aggregate and compare |
| ca-precision | linear | Data check → ca-precision → optional statistical test → interpret |
| priority | linear + loop | Run rules 1-6 sequentially → merge → interpret |
| composite | recursive + report generation | Decompose into sub-intents → auto-append report generation |
| knowledge | no planning | Pure retrieval, returns directly |

## References (loaded on demand, not auto-loaded)

| File | When to read |
|------|--------------|
| `references/plan-strategies_en.md` | When selecting a planning mode and Linear/Parallel/Conditional/Loop templates |

## Shared (cross-Skill, loaded on demand)

| File | When to read |
|------|--------------|
| `agent/shared/data-semantics.md` | When the data_check step needs to ask for region/year |

## Detail

For the full execution flow (ReAct loop, parameter references, fallback strategies, examples), see `SKILL-detail_en.md`; load it on demand via `read_file`.
