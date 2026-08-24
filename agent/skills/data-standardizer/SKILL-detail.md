# data-standardizer — 完整执行流程

> GeoSDG 数据标准化 Skill，引导用户将散乱 GeoTIFF 文件整理为 `data/` 标准目录结构，生成 `manifest.json`，使数据可被 GeoSDG CLI 直接消费。

---

## 🔄 中断恢复机制

每次会话开始时，检查 `<data_dir>/.standardize-progress.json`：

- **存在** → 读取 `current_stage`，向用户展示进度摘要，询问"从 Stage N 继续？"
- **不存在** → 从 Stage 1 开始

进度文件由 `scripts/progress_manager.py` 管理，6 阶段状态持久化。

---

## 📋 6 阶段完整对话流

### Stage 1: Scan & Inventory（扫描与清单）

**目标**：扫描用户数据目录，建立完整文件清单。

**执行步骤**：

1. 询问用户数据目录路径（默认 `data/`）
2. 运行 `scripts/scan_tiffs.py <data_dir> --recursive --output json`
3. 解析输出，按 `category_hint` 分组统计
4. 向用户展示扫描摘要：

```
📊 扫描结果：
- 总文件数：42 个 GeoTIFF
- 总大小：1.2 GB
- 类别分布：
  · lucc: 8 files (2020-2050, 5yr step)
  · pop: 7 files (2020-2050, 5yr step)
  · carbon: 6 files (2020-2050, 5yr step)
  · elevation: 1 file (static)
  · slope: 1 file (static)
  · unknown: 19 files ← 需要人工确认
```

5. 对 `unknown` 类别的文件，逐个询问用户确认类别
6. 更新进度：`progress_manager.py update <data_dir> --stage 1 --status done`

**中断恢复**：重新扫描即可，无副作用。

---

### Stage 2: Classify & Rename（分类与重命名）

**目标**：将文件重命名为标准命名规范 `<category>_<source>_<year>[_<scenario>].tif`。

**标准命名规范**：

| 组成 | 说明 | 示例 |
|------|------|------|
| category | 数据类别缩写 | `lucc`, `pop`, `carbon` |
| source | 数据来源/项目名 | `globe30`, `worldpop`, `edgar` |
| year | 4 位年份 | `2020`, `2050` |
| scenario | SSP 场景（可选） | `ssp1_26`, `ssp2_45` |

**示例**：
- `LUCC_China_2020.tif` → `lucc_globe30_2020.tif`
- `pop_ssp2_2050.tif` → `pop_worldpop_2050_ssp2_45.tif`

**执行步骤**：

1. 基于 Stage 1 扫描结果，为每个文件生成重命名建议
2. 向用户展示重命名方案（表格：原文件名 → 新文件名）
3. 用户可逐条修改或批量确认
4. **不自动执行重命名**，仅生成方案。用户确认后，生成 shell 脚本 `rename.sh` 供用户执行
5. 更新进度

**中断恢复**：重命名方案可重新生成，已执行的 `rename.sh` 不可逆（提醒用户备份）。

---

### Stage 3: Metadata Check（元数据检查）

**目标**：检查 CRS、NoData、分辨率一致性，标记问题文件。

**检查项**：

| 检查项 | 合格标准 | 问题处理 |
|--------|---------|---------|
| CRS 一致性 | 同一 dataset 内所有文件 CRS 相同 | 标记不一致文件，建议统一到项目 CRS |
| NoData 一致性 | 同一 dataset 内 NoData 值相同 | 标记不一致文件，建议统一 |
| 分辨率合理性 | 分辨率 > 0 且在合理范围 | 标记异常值 |
| 波段数 | 单波段（bands=1） | 多波段文件标记需确认 |

**执行步骤**：

1. 读取 Stage 1 扫描结果中的元数据
2. 按类别分组，逐项检查
3. 生成检查报告：

```
🔍 元数据检查报告：

✅ lucc: CRS=EPSG:4326, NoData=-9999, Resolution=0.000278° — OK
⚠️  pop: CRS 不一致 — 5 files EPSG:4326, 2 files EPSG:32650
❌ carbon: NoData 不一致 — 4 files -9999, 2 files -3.4e38
```

4. 对问题项，建议修复方案（如 `gdalwarp` 重投影）
5. 更新进度

**中断恢复**：检查是只读操作，可重复执行。

---

### Stage 4: Generate Manifest（生成清单文件）

**目标**：生成 `manifest.json`，描述数据目录的完整结构。

**执行步骤**：

1. 收集前 3 阶段信息：文件清单、分类结果、元数据
2. 询问项目元信息（如尚未提供）：
   - 项目名称
   - 区域名称与 bbox
   - 基准年 / 目标年 / 时间步长
3. 运行 `scripts/generate_manifest.py <scan_results.json> --project-name <name> --region <name> --bbox <w,s,e,n> --baseline <year> --horizon <year>`
4. 展示生成的 `manifest.json` 摘要，用户确认
5. 写入 `<data_dir>/manifest.json`
6. 更新进度

**manifest.json 关键结构**：

```json
{
  "$schema": "../manifest-schema.json",
  "project": { "name": "...", "region": {...}, "time_range": {...} },
  "scenarios": [...],
  "datasets": [
    { "id": "lucc", "name": "...", "category": "landuse", "files": [...] }
  ],
  "indicators": [],
  "priority_areas": { "rules": [], "output": {...} }
}
```

**中断恢复**：manifest.json 可重新生成覆盖。

---

### Stage 5: Validate Structure（验证结构）

**目标**：验证 `data/` 目录结构与 `manifest.json` 完全一致。

**执行步骤**：

1. 运行 `scripts/validate_structure.py <data_dir> --strict`
2. 检查项：
   - manifest.json 存在且为合法 JSON
   - manifest.json 符合 manifest-schema.json
   - 所有 manifest 引用的文件存在
   - 所有 .tif 文件都在 manifest 中（strict 模式）
   - CRS 一致性
3. 展示验证结果，如有问题回到对应阶段修复
4. 更新进度

**中断恢复**：验证是只读操作，可重复执行。

---

### Stage 6: Finalize（确认完成）

**目标**：确认最终结构，清理临时文件，输出使用指引。

**执行步骤**：

1. 确认 `manifest.json` 最终版本
2. 删除 `.standardize-progress.json`（可选，用户确认）
3. 输出使用指引：

```
✅ 数据标准化完成！

📂 目录结构：
data/
├── manifest.json
├── rasters/
│   ├── lucc_globe30_2020.tif
│   ├── lucc_globe30_2050_ssp2_45.tif
│   ├── pop_worldpop_2020.tif
│   └── ...
└── _template/

🚀 下一步：
  1. 使用 GeoSDG CLI 计算指标：
     geosdg-cli --command calc-sdg1131 --lucc data/rasters/lucc_globe30_2020.tif ...
  2. 或通过 Agent 执行：
     "帮我计算 SDG 11.3.1"
```

4. 更新进度为全部完成

---

## 🔗 脚本索引

| 脚本 | 用途 | 调用方式 |
|------|------|---------|
| `scripts/scan_tiffs.py` | 扫描 GeoTIFF 并提取元数据 | `python scan_tiffs.py <dir> --recursive` |
| `scripts/generate_manifest.py` | 从扫描结果生成 manifest.json | `python generate_manifest.py <scan.json> --project-name <name>` |
| `scripts/validate_structure.py` | 验证目录结构一致性 | `python validate_structure.py <dir> --strict` |
| `scripts/progress_manager.py` | 跨会话进度管理 | `python progress_manager.py <init\|status\|update\|resume> <dir>` |

## 🔗 参考文件索引

| 文件 | 用途 |
|------|------|
| `references/category_keywords.json` | 16 类地物类别关键词映射 |
| `references/ssp_scenarios.json` | SSP 场景标准名称映射 |
| `references/manifest_template.json` | manifest.json 空白模板 |
| `@see ../../data/manifest-schema.json` | manifest JSON Schema 定义 |
| `@see ../../data/DATA_FORMAT_SPEC.md` | 数据目录格式规范 |

## ⚠️ 边界处理

| 情况 | 处理 |
|------|------|
| 数据目录为空 | 提示用户放入 GeoTIFF 文件后重新扫描 |
| 无 GDAL Python 绑定 | 提示安装：`pip install GDAL`，降级为仅文件名分类 |
| 文件名无法提取年份 | 标记为 `year: null`，提示用户手动补充 |
| manifest-schema.json 不存在 | 跳过 schema 验证，仅做文件引用检查 |
| 用户中断后恢复 | 读取 `.standardize-progress.json`，从 `current_stage` 继续 |
| 重命名冲突（目标文件已存在） | 添加数字后缀 `_2`，提示用户确认 |
| 多波段 GeoTIFF | 标记需确认，默认取第 1 波段元数据 |
