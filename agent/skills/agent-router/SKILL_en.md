---
name: agent-router
description: GeoSDG Agent intent router. Recognizes user natural-language intent and routes it to the appropriate Skill and Tool system. Covers 6 intent types: SDG indicator calculation, CA accuracy assessment, priority-area identification, knowledge query, composite assessment, and report generation. Trigger keywords: SDG assessment, GeoSDG analysis, spatial SDG analysis, accuracy assessment, priority area, sustainable development analysis, agent-router
---

# Agent Router Skill

## Purpose

The **first entry point** of the GeoSDG Agent. Analyzes user natural-language input, identifies the intent category, and routes it to the correct Skill and Tool system. It only performs **intent recognition and routing** — not task planning or tool execution.

**RAG retrieval**: After routing, it automatically triggers knowledge-base semantic retrieval (`agent/shared/kb_retriever.py`) to inject the Top-K relevant knowledge snippets into the downstream Skill/Planner context. Retrieval is driven by the `rag_query_hint` field in `intent-map_en.yaml`.

## When to Use

- When the user describes an SDG assessment need in natural language
- When you need to determine which category the user's intent belongs to
- When the user's input is ambiguous and needs guided clarification

## Intent Classification Table

| Intent category | Priority | Routing target | Typical scenario |
|-----------------|----------|----------------|------------------|
| `sdg-calc` | 1 | sdg-indicator-knowledge → planner | "Calculate SDG 15.3.1" |
| `ca-precision` | 2 | planner → executor | "Is this simulation accurate?" |
| `priority` | 3 | planner → executor | "Find priority protection areas" |
| `knowledge` | 4 | sdg-indicator-knowledge / paper-revision-workflow | "What is SDG 6.6.1?" |
| `composite` | 10 (fallback) | planner (composite analysis) | "Fully analyze Beijing for me" |
| `report` | 5 | planner → generate-report | "Generate an assessment report" |

## References (loaded on demand, not auto-loaded)

| File | When to read |
|------|--------------|
| `references/intent-map_en.yaml` | When loading intent-mapping rules and matching patterns |

## Shared (cross-Skill, loaded on demand)

| File | When to read |
|------|--------------|
| `agent/shared/sdg-tool-map.md` | When the SDG → Tool mapping is needed |

## Detail

For the full execution flow (matching rules, routing flow, test cases), see `SKILL-detail_en.md`; load it on demand via `read_file`.
