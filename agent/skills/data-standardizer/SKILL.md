# data-standardizer

GeoSDG 数据标准化 Skill — 将散乱 GeoTIFF 文件整理为 `data/` 标准目录结构，生成 `manifest.json`。

**触发关键词**：数据标准化、整理数据、data standardization、organize tiffs、生成 manifest、数据目录结构、scan tiffs

**When to use**：用户有一批 GeoTIFF 文件需要整理为 GeoSDG 可消费的标准结构时触发。

---

## 6 阶段流程速查

| Stage | 名称 | 核心动作 |
|-------|------|---------|
| 1 | Scan & Inventory | `scan_tiffs.py` 扫描目录，按类别分组 |
| 2 | Classify & Rename | 生成标准命名方案 `<cat>_<src>_<year>[_<scenario>].tif` |
| 3 | Metadata Check | 检查 CRS/NoData/分辨率一致性 |
| 4 | Generate Manifest | `generate_manifest.py` 生成 `manifest.json` |
| 5 | Validate Structure | `validate_structure.py` 验证目录与 manifest 一致性 |
| 6 | Finalize | 确认完成，输出 CLI 使用指引 |

## 脚本速查

| 脚本 | 用途 |
|------|------|
| `scripts/scan_tiffs.py <dir> --recursive` | 扫描 GeoTIFF 元数据 |
| `scripts/generate_manifest.py <scan.json>` | 生成 manifest.json |
| `scripts/validate_structure.py <dir> --strict` | 验证结构一致性 |
| `scripts/progress_manager.py <cmd> <dir>` | 跨会话进度管理 |

## Reference 索引

- `references/category_keywords.json` — 16 类关键词映射
- `references/ssp_scenarios.json` — SSP 场景映射
- `references/manifest_template.json` — manifest 模板
- `@see ../../data/manifest-schema.json` — JSON Schema
- `@see ../../data/DATA_FORMAT_SPEC.md` — 数据格式规范

→ 完整执行流程：[SKILL-detail.md](./SKILL-detail.md)
