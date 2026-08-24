# CLI 参数映射规则（跨 Skill 共享）

> 本文件从 agent-executor Step 3 和 tool-registry.md §3 抽取的 CLI 参数映射规则。
> agent-executor 引用本文件而非在 SKILL.md 中重复维护。

---

## 转换规则

agent-executor 将 JSON Schema 参数名转换为 CLI 参数名，采用 **snake_case → kebab-case** 转换。

### 标准转换

| JSON 属性名 | CLI 参数 | 说明 |
|------------|---------|------|
| `ori` | `--ori` | 原始 LUCC |
| `sim` | `--sim` | 模拟 LUCC |
| `real` | `--real` | 真实 LUCC |
| `data1` | `--data1` | 第一组数据 |
| `data2` | `--data2` | 第二组数据 |
| `init_lucc` | `--init-lucc` | 初始 LUCC |
| `curr_lucc` | `--curr-lucc` | 变化期 LUCC |
| `init_popu` | `--init-popu` | 初始人口 |
| `curr_popu` | `--curr-popu` | 变化期人口 |
| `buffer` | `--buffer` | 缓冲区数据 |
| `types` | `--types` | 地类编码集合 |
| `transitions` | `--transitions` | 转换规则 |
| `emission` | `--emission` | 排放系数 |
| `max` | `--max` | 上限阈值 |
| `min` | `--min` | 下限阈值 |
| `best` | `--best` | 最优值阈值 |
| `ratio` | `--ratio` | 比例参数 |
| `radius` | `--radius` | 邻域半径 |
| `pop_threshold` | `--pop-threshold` | 人口阈值 |
| `files` | `--files` | 文件列表 |
| `resume` | `--resume` | 断点续跑（布尔） |

### 特殊映射

| JSON 属性名 | CLI 参数 | 说明 |
|------------|---------|------|
| `output` | `-o` | 输出路径（使用短选项，CLI 同时支持 `-o` 和 `--output`） |

---

## 布尔参数处理

布尔型参数（`type: "boolean"`）不接收值，仅作为 flag 出现：

| JSON 属性 | 值 | CLI 生成 |
|-----------|-----|---------|
| `positive` | `true` | `--positive` |
| `positive` | `false` | `--negative` |
| `resume` | `true` | `--resume` |
| `resume` | `false` | （省略） |

> 注意：`positive`/`negative` 互斥，CLI 中 `--positive` 为默认。当 JSON 中 `positive=false` 时生成 `--negative`。

---

## 特殊 format 值

| format | 说明 | CLI 映射示例 |
|--------|------|-------------|
| `filepath` | 文件路径，执行前校验文件是否存在 | `--init-lucc /data/a.tif` |
| `comma-list` | 逗号分隔列表 | `--types 1,2,3` |
| `transition-map` | 转换规则映射 `src:t1:t2,...` | `--transitions 2:5:6,4:5` |
| `emission-map` | 排放系数映射 `type:factor,...` | `--emission 1:-1,2:-20` |
| `file-list` | 逗号分隔文件路径列表 | `--files a.tif,b.tif` |

---

## 路径处理建议

- **数据安全优先**：`agent-executor` 在构建完 CLI 命令后、执行前（Step 3.5），会自动将所有输入文件拷贝到 `tmp/` 目录，CLI 对副本执行计算，完成后清理。用户原始数据不会被修改
- 如果用户提供的路径是相对路径，基于用户工作目录解析
- `geosdg-cli` 的路径默认相对于可执行文件所在目录，建议使用绝对路径
- 路径含空格时用引号包裹

---

## 命令构建示例

**输入 JSON**：
```json
{
  "name": "sdg-land-conversion",
  "parameters": {
    "init_lucc": "/data/lucc_2010.tif",
    "curr_lucc": "/data/lucc_2030.tif",
    "transitions": "2:5:6,4:5",
    "max": 100,
    "min": 0,
    "positive": true
  }
}
```

**生成 CLI 命令**：
```bash
geosdg-cli sdg-land-conversion \
  --init-lucc /data/lucc_2010.tif \
  --curr-lucc /data/lucc_2030.tif \
  --transitions 2:5:6,4:5 \
  --max 100 \
  --min 0 \
  --positive
```
