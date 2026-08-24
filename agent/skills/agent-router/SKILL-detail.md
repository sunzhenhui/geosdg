# Agent Router Skill — 完整执行流程

> 本文件为 agent-router Skill 的完整层，由 SKILL.md 摘要层按需加载。
> 摘要层仅包含意图分类表和路由目标，本文件包含完整匹配规则、路由流程和测试用例。

---

## 执行流程

### Step 1：加载配置

1. 读取 `agent/tools/manifest.json`，获取可用 Tool 列表和分类
2. 读取 `references/intent-map.yaml`，加载意图映射规则
3. 初始化匹配上下文（当前会话 Memory 中的历史意图）

### Step 2：意图识别

对用户输入按以下顺序匹配：

```
for intent in sorted_by_priority(intents):
    if match_regex(intent.patterns.regex, user_input):
        return intent
    if match_keywords(intent.patterns.keywords, user_input):
        return intent
    if match_composite(intent.patterns.composite_keywords, user_input, context):
        return intent

# 所有意图均未命中
return composite  # 兜底，由规划器引导用户澄清
```

**匹配规则**：
- **regex 匹配**：大小写不敏感，支持 SDG 编号的多种写法（`sdg 11`、`SDG11.3`、`目标11`、`sgd 11.3.1`）
- **keywords 匹配**：关键词在用户输入中出现即命中
- **composite_keywords**：需结合上下文（区域名、数据描述）才触发，单独出现不触发

### Step 3：RAG 知识检索

意图识别完成后，**自动触发**知识库语义检索：

```
intent = matched_intent
if intent.rag_query_hint:
    query = format_rag_hint(intent.rag_query_hint, user_input)
    results = KBRetriever.retrieve(query, top_k=5)
    inject results into downstream context
```

- `rag_query_hint`：定义在 `intent-map.yaml` 中，每个意图类别可定制检索提示词
- 检索结果作为 `system prompt` 补充注入下游 Skill/Planner 上下文
- 知识库为空时**静默降级**，不影响正常路由流程
- 检索延迟 < 500ms（本地 FAISS），不阻塞路由

### Step 4：路由分发

根据识别的意图类别，执行对应路由：

#### 路由到 sdg-calc

```
用户输入 → 识别为 sdg-calc 意图
  ↓
0. [RAG] 检索知识库获取指标定义、公式、参数推荐
1. 加载 sdg-indicator-knowledge Skill，查询指标定义和计算公式
2. 加载 agent-planner Skill，规划计算链
3. agent-planner 调用 agent-executor 执行对应 Tool
```

**输出**：
```json
{
  "intent": "sdg-calc",
  "confidence": "high",
  "matched_pattern": "regex: sdg\\s*\\d",
  "target_skills": ["sdg-indicator-knowledge"],
  "target_tools": ["sdg-land-proportion", "sdg-land-conversion", ...],
  "routing_flow": ["加载 sdg-indicator-knowledge", "加载 agent-planner", "调用 agent-executor"],
  "original_query": "算一下 SDG 15.3.1"
}
```

#### 路由到 ca-precision

```
用户输入 → 识别为 ca-precision 意图
  ↓
0. [RAG] 检索知识库获取精度评估方法和参数
1. 加载 agent-planner Skill
2. agent-planner 调用 agent-executor 执行 ca-precision / correlation / t-test
```

#### 路由到 priority

```
用户输入 → 识别为 priority 意图
  ↓
0. [RAG] 检索知识库获取优先区域规则和案例
1. 加载 agent-planner Skill
2. agent-planner 调用 agent-executor 依次执行 Rule 1-6
3. 调用 agent-executor 执行 priority-merge 生成排名图
```

#### 路由到 knowledge（纯检索，不调用 CLI）

```
用户输入 → 识别为 knowledge 意图
  ↓
0. [RAG] 检索知识库获取指标定义和方法论
1. 加载 sdg-indicator-knowledge 或 paper-revision-workflow Skill
2. 纯知识检索对话，返回文字说明
3. 不调用 agent-executor，不执行任何 CLI 命令
```

#### 路由到 composite（兜底）

```
用户输入 → 无法明确分类，归入 composite
  ↓
0. [RAG] 检索知识库获取相关指标和案例
1. 加载 agent-planner Skill 进行综合分析
2. 规划器自主决定调用哪些 Skill 和 Tool
3. 如果过于模糊 → 引导用户说明具体需求
```

#### 路由到 report（报告生成）

```
用户输入 → 识别为 report 意图
  ↓
1. 加载 agent-planner Skill
2. 规划器根据上下文决定：
   ├── 已有评估结果 → 直接调用 generate-charts + generate-report
   └── 无评估结果 → 先执行 SDG 计算 → 再生成报告（等同 composite 流程）
3. agent-planner 调用 agent-executor 执行 report 分类下的 Tool
```

### Step 5：写入 Memory

将识别结果写入 L0 工作记忆 `agent/memory/current/working.json`（调用 `updateWorkingMemory`）：
```json
{
  "current_intent": "sdg-calc",
  "status": "planning",
  "active_data_refs": []
}
```

> 注：旧版写入 `context.json` 已 DEPRECATED，统一改用 L0 `working.json`。

---

## 路由准确性保障

### 关键词覆盖策略

| 策略 | 说明 |
|------|------|
| 宽松匹配 | SDG 编号支持多种写法（`sdg 11`/`SDG11.3`/`目标11`/`sgd 11.3.1`） |
| 中英混合 | 同时覆盖中文（"精度"）和英文（"FoM"）关键词 |
| 上下文感知 | composite_keywords 需结合区域名或数据描述才触发 |
| 优先级兜底 | 无法识别的输入归入 composite，由规划器引导 |

### 歧义处理

| 场景 | 处理方式 |
|------|---------|
| 同时匹配 sdg-calc 和 ca-precision | 按 priority 数字选择（sdg-calc=1 优先于 ca-precision=2） |
| 用户语气为询问（"是什么""定义"）但含 SDG 编号 | 归入 knowledge（询问优先于计算） |
| "帮我评估" 无明确对象 | 归入 composite，引导用户说明评估什么 |
| 多个 SDG 指标同时请求 | 归入 sdg-calc，规划器识别为并行任务 |

---

## 路由测试用例

| 用户输入 | 识别意图 | 路由目标 | 匹配依据 |
|---------|---------|---------|---------|
| "算一下 SDG 15.3.1" | sdg-calc | sdg-indicator-knowledge → sdg-land-conversion | regex: `sdg\s*\d` |
| "sgd 11 这个指标怎么算" | sdg-calc | sdg-indicator-knowledge | regex: `sgd\s*\d` |
| "这个模拟准不准" | ca-precision | ca-precision | composite_keyword: 准不准 |
| "验证下 CA 模拟的 Kappa 值" | ca-precision | ca-precision | keyword: Kappa |
| "还有哪些地方需要优先保护" | priority | priority-merge | keyword: 优先保护 |
| "SDG 6.6.1 是什么指标" | knowledge | sdg-indicator-knowledge | regex: 是什么 |
| "帮我完整分析一下北京" | composite | agent-planner | keyword: 完整 |
| "生成评估报告" | report | generate-report | keyword: 报告 |
| "出个雷达图看看" | report | generate-charts | keyword: 雷达图 |
| "帮我看看这个" | composite | agent-planner（引导） | 兜底 |

---

## Important Notes

- 本 Skill 是 Agent 的**第一入口**，但不是唯一入口——用户也可直接触发 agent-planner 或 agent-executor
- 路由结果不是最终决策——agent-planner 可以根据 Memory 上下文调整执行计划
- `knowledge` 意图**不调用任何 CLI Tool**，纯知识检索对话
- 意图识别失败时不要静默放弃，应归入 composite 并引导用户
- 跨会话恢复时，Router 优先读取 Memory 中的历史意图，保持上下文连贯
