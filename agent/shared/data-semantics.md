# 数据语义元信息约束（跨 Skill 共享）

> 本文件从 agent-executor、agent-planner、agent-memory 三个 Skill 中抽取的数据语义元信息约束。
> 各 Skill 引用本文件而非各自维护副本。

---

## 核心规则

GeoTIFF 文件不含"杭州 2010 年 LUCC"之类的语义标签。以下元信息**必须由用户提供，Agent 不可自动推断**：

| 元信息 | 说明 | 来源 | 示例 |
|--------|------|------|------|
| `region` | 地理区域名 | 用户提供 | "杭州"、"长三角" |
| `year` | 数据对应年份 | 用户提供 | 2010、2030 |
| `category` | 数据类型 | 用户提供（可从路径推断但需确认） | LUCC / POP / INFRA |

技术元信息（file_size_mb / dimensions / crs / data_type）可由 GDAL 自动提取，无需用户关心。

---

## 各 Skill 使用场景

| Skill | 使用场景 | 行为 |
|-------|---------|------|
| agent-planner | `data_check` 步骤检测到文件存在但缺少 region/year | 转为 `ask_user` 动作，追问用户 |
| agent-executor | Step 6 L3 注册数据文件前 | 检查 region/year 是否已提供，未提供则提示 Planner 询问 |
| agent-memory | L3 `registerFile` API | region/year 为必填参数，不接受空值 |

---

## 交互流程

```
用户提供数据路径后:
  ├── 检测到缺少 region/year 语义元信息
  ├── 主动询问用户：
  │     → "请确认以下信息：
  │        1. 该文件覆盖哪个地理区域？如杭州、长三角...
  │        2. 是哪一年的数据？
  │        3. 数据类型是什么？（LUCC 土地利用 / POP 人口 / INFRA 基础设施）"
  └── 用户确认后 → 调用 L3 registerFile(path, region, year, {category}) 注册
```

---

## category 推导规则

如用户未明确提供 category，可从路径名推断（但仍需用户确认）：

| 路径含关键词 | 推断 category |
|-------------|--------------|
| `lucc` / `land` | LUCC |
| `pop` / `population` | POP |
| `infra` / `facility` | INFRA |

alias 生成规则：`{region}_{category}_{year}`（如"杭州_LUCC_2010"）

---

## 远期增强：基于国家 SHP 的自动区域判定

**当前限制**：region 必须由用户手动提供，Agent 无法从 GeoTIFF 像素推断语义区域。

**远期目标**：当用户提供大区域范围数据 + 国家行政边界 SHP 矢量时，可通过空间叠置分析自动判定所属国家：

```
用户提供: 大区域 GeoTIFF + 国家边界 SHP (如 China.shp)
  │
  ├── [远期] RegionResolver 子模块
  │     ├── 读取 GeoTIFF 的 GeoTransform → 获取经纬度范围
  │     ├── 加载 SHP 矢量 → 解析国家多边形边界
  │     ├── 空间叠置分析（contains/intersects）
  │     │     ├── 完全包含 → 判定为国家（如"中国"）
  │     │     ├── 部分交集 → 标记为跨国（列出涉及国家）
  │     │     └── 无匹配 → 回退到手动确认
  │     └── 输出候选 region → 仍需用户最终确认（防误判）
  │
  └── year 仍需用户提供（无法从像素推断）
```

| 阶段 | 内容 | 优先级 |
|------|------|--------|
| **当前** | 用户手动提供 region + year，Agent 主动追问确认 | P0 |
| **远期** | 支持用户提供 SHP → 自动判定 country；支持嵌套行政区划 | P2 |
