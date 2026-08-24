# Agent Router Skill — Full Execution Flow

> This file is the complete layer of the agent-router Skill, loaded on demand by the SKILL.md summary layer.
> The summary layer only contains the intent classification table and routing targets; this file contains the full matching rules, routing flow, and test cases.

---

## Execution Flow

### Step 1: Load configuration

1. Read `agent/tools/manifest.json` to obtain the available Tool list and categories
2. Read `references/intent-map_en.yaml` to load the intent-mapping rules
3. Initialize the matching context (historical intents from the current session Memory)

### Step 2: Intent recognition

Match the user input in the following order:

```
for intent in sorted_by_priority(intents):
    if match_regex(intent.patterns.regex, user_input):
        return intent
    if match_keywords(intent.patterns.keywords, user_input):
        return intent
    if match_composite(intent.patterns.composite_keywords, user_input, context):
        return intent

# No intent matched
return composite  # fallback; the planner guides the user to clarify
```

**Matching rules**:
- **regex matching**: case-insensitive, supports multiple ways of writing an SDG number (`sdg 11`, `SDG11.3`, `target 11`, `sgd 11.3.1`)
- **keyword matching**: matches if the keyword appears in the user input
- **composite_keywords**: only triggered together with context (region name, data description); never triggered in isolation

### Step 3: RAG knowledge retrieval

Once the intent is recognized, it **automatically triggers** knowledge-base semantic retrieval:

```
intent = matched_intent
if intent.rag_query_hint:
    query = format_rag_hint(intent.rag_query_hint, user_input)
    results = KBRetriever.retrieve(query, top_k=5)
    inject results into downstream context
```

- `rag_query_hint`: defined in `intent-map_en.yaml`; each intent category can customize its retrieval prompt
- Retrieval results are injected into the downstream Skill/Planner context as a `system prompt` supplement
- When the knowledge base is empty, it **silently degrades** without affecting the normal routing flow
- Retrieval latency < 500ms (local FAISS), does not block routing

### Step 4: Route dispatch

Based on the recognized intent category, execute the corresponding route:

#### Route to sdg-calc

```
User input → recognized as sdg-calc intent
  ↓
0. [RAG] Retrieve the knowledge base for indicator definition, formula, and parameter recommendations
1. Load the sdg-indicator-knowledge Skill to query indicator definition and formula
2. Load the agent-planner Skill to plan the computation chain
3. agent-planner calls agent-executor to execute the corresponding Tool
```

**Output**:
```json
{
  "intent": "sdg-calc",
  "confidence": "high",
  "matched_pattern": "regex: sdg\\s*\\d",
  "target_skills": ["sdg-indicator-knowledge"],
  "target_tools": ["sdg-land-proportion", "sdg-land-conversion", ...],
  "routing_flow": ["load sdg-indicator-knowledge", "load agent-planner", "call agent-executor"],
  "original_query": "calculate SDG 15.3.1"
}
```

#### Route to ca-precision

```
User input → recognized as ca-precision intent
  ↓
0. [RAG] Retrieve the knowledge base for accuracy assessment methods and parameters
1. Load the agent-planner Skill
2. agent-planner calls agent-executor to run ca-precision / correlation / t-test
```

#### Route to priority

```
User input → recognized as priority intent
  ↓
0. [RAG] Retrieve the knowledge base for priority-area rules and cases
1. Load the agent-planner Skill
2. agent-planner calls agent-executor to run Rules 1-6 in sequence
3. Call agent-executor to run priority-merge and generate the ranking map
```

#### Route to knowledge (pure retrieval, no CLI call)

```
User input → recognized as knowledge intent
  ↓
0. [RAG] Retrieve the knowledge base for indicator definitions and methodology
1. Load the sdg-indicator-knowledge or paper-revision-workflow Skill
2. Pure knowledge-retrieval conversation; returns text explanations
3. Does not call agent-executor, does not run any CLI command
```

#### Route to composite (fallback)

```
User input → cannot be clearly classified; falls into composite
  ↓
0. [RAG] Retrieve the knowledge base for relevant indicators and cases
1. Load the agent-planner Skill for composite analysis
2. The planner autonomously decides which Skills and Tools to call
3. If too ambiguous → guide the user to clarify the specific need
```

#### Route to report (report generation)

```
User input → recognized as report intent
  ↓
1. Load the agent-planner Skill
2. The planner decides based on context:
   ├── Existing assessment results → directly call generate-charts + generate-report
   └── No assessment results → first run SDG computation → then generate the report (same as the composite flow)
3. agent-planner calls agent-executor to run the Tools under the report category
```

### Step 5: Write to Memory

Write the recognition result to the L0 working memory `agent/memory/current/working.json` (call `updateWorkingMemory`):
```json
{
  "current_intent": "sdg-calc",
  "status": "planning",
  "active_data_refs": []
}
```

> Note: the legacy write to `context.json` is DEPRECATED; uniformly use L0 `working.json`.

---

## Routing Accuracy Assurance

### Keyword coverage strategy

| Strategy | Description |
|----------|-------------|
| Loose matching | SDG numbers support multiple spellings (`sdg 11` / `SDG11.3` / `target 11` / `sgd 11.3.1`) |
| Mixed Chinese-English | Cover both Chinese ("accuracy") and English ("FoM") keywords |
| Context awareness | composite_keywords only trigger together with a region name or data description |
| Priority fallback | Unrecognized input falls into composite; the planner guides the user |

### Ambiguity handling

| Scenario | Handling |
|----------|----------|
| Matches both sdg-calc and ca-precision | Select by priority number (sdg-calc=1 takes precedence over ca-precision=2) |
| User tone is a question ("what is" / "definition") but contains an SDG number | Classify as knowledge (question takes precedence over computation) |
| "Assess it for me" with no clear object | Classify as composite; guide the user to clarify what to assess |
| Multiple SDG indicators requested at once | Classify as sdg-calc; the planner recognizes it as a parallel task |

---

## Routing Test Cases

| User input | Recognized intent | Routing target | Matching basis |
|------------|-------------------|----------------|----------------|
| "Calculate SDG 15.3.1" | sdg-calc | sdg-indicator-knowledge → sdg-land-conversion | regex: `sdg\s*\d` |
| "How to calculate sgd 11" | sdg-calc | sdg-indicator-knowledge | regex: `sgd\s*\d` |
| "Is this simulation accurate?" | ca-precision | ca-precision | composite_keyword: accurate |
| "Verify the Kappa value of the CA simulation" | ca-precision | ca-precision | keyword: Kappa |
| "Which areas still need priority protection?" | priority | priority-merge | keyword: priority protection |
| "What indicator is SDG 6.6.1?" | knowledge | sdg-indicator-knowledge | regex: what is |
| "Fully analyze Beijing for me" | composite | agent-planner | keyword: fully |
| "Generate an assessment report" | report | generate-report | keyword: report |
| "Produce a radar chart" | report | generate-charts | keyword: radar chart |
| "Take a look at this for me" | composite | agent-planner (guide) | fallback |

---

## Important Notes

- This Skill is the **first entry point** of the Agent, but not the only one — users can also directly trigger agent-planner or agent-executor
- The routing result is not the final decision — agent-planner can adjust the execution plan based on Memory context
- The `knowledge` intent **does not call any CLI Tool**; it is a pure knowledge-retrieval conversation
- Do not silently give up when intent recognition fails; classify it as composite and guide the user
- On cross-session recovery, the Router first reads the historical intent from Memory to maintain contextual continuity
