# GeoSDG 空间化 SDG 指标详细规范

> 数据来源：GeoSDG 论文 Table 1（New Pathways for UN SDGs Spatialization）
> 共 10 个 SDG · 17 个 Target · 27 个具体指标

---

## 计算类型速查

| 符号 | 类型名称 | CLI 命令 | 核心公式 |
|------|---------|---------|---------|
| `*` | Land Proportion | `sdg-land-proportion` | `score = norm(target_pixels / total_pixels, min, max)` × 100 |
| `**` | Land Conversion | `sdg-land-conversion` | `score = norm(converted_pixels / initial_pixels, min, max)` × 100 |
| `***` | Buffer Zone | `sdg-buffer-zone` | `score = norm(covered_target / total_target, min, max)` × 100 |
| `****` | Total Statistics | `sdg-1131` / `sdg-1322` | 针对特定指标的聚合统计 |

### 归一化公式

$$X_{norm} = \begin{cases} \frac{X - X_{min}}{X_{max} - X_{min}}, & \text{Positive indicator} \\ 1 - \frac{|X - X_{opt}|}{\max(|X - X_{min}|, |X - X_{max}|)}, & \text{Neutral indicator} \\ \frac{X_{max} - X}{X_{max} - X_{min}}, & \text{Negative indicator} \end{cases}$$

最终得分: $Score = X_{norm} \times 100$

---

## SDG 2: Zero Hunger（零饥饿）

### Target 2.1.2

| 属性 | 值 |
|------|-----|
| 指标编号 | 1 |
| 指标名称 | Proportion of land for agriculture use |
| 计算类型 | `*` Land Proportion |
| CLI 命令 | `sdg-land-proportion` |
| 输入 | LUCC GeoTIFF (GDT_Byte), cropland code (e.g., `{1}`) |
| 最大阈值 | 0.8 (≥80% cropland = 100) |
| 最小阈值 | 0.1 (≤10% cropland = 0) |
| 归一化方向 | 正向 (higher proportion = higher score) |
| 得分范围 | [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 2 |
| 指标名称 | Grain yield |
| 计算类型 | `****` Total Statistics |
| 输入 | LUCC + 人口数据 + 粮食单产系数 |
| 输出 | 人均粮食产量，归一化到 [0, 100] |
| 状态 | 🔄 规划中 |

### Target 2.4.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 3 |
| 指标名称 | Proportion of land for easy accessibility |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | LUCC GeoTIFF (GDT_Byte, cropland code) + 道路缓冲区 (GDT_Byte) |
| 输出 | 道路覆盖范围内耕地占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 4 |
| 指标名称 | Proportion of land for efficient irrigation practices |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | LUCC GeoTIFF (GDT_Byte, cropland code) + 灌溉设施缓冲区 (GDT_Byte) |
| 输出 | 灌溉设施覆盖范围内耕地占比，归一化到 [0, 100] |

---

## SDG 3: Good Health and Well-being（良好健康与福祉）

### Target 3.8.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 5 |
| 指标名称 | Proportion of health facility coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 医疗设施缓冲区 (GDT_Byte) |
| 输出 | 医疗设施服务人口占比，归一化到 [0, 100] |
| 归一化方向 | 正向 (higher coverage = higher score) |

| 属性 | 值 |
|------|-----|
| 指标编号 | 6 |
| 指标名称 | Proportion of elderly care facility coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 养老设施缓冲区 (GDT_Byte) |
| 输出 | 养老设施服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 7 |
| 指标名称 | Proportion of sport and cultural facility coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 文体设施缓冲区 (GDT_Byte) |
| 输出 | 文体设施服务人口占比，归一化到 [0, 100] |

### Target 3.c.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 8 |
| 指标名称 | Proportion of community healthcare institution coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 社区医疗机构缓冲区 (GDT_Byte) |
| 输出 | 社区医疗机构服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 9 |
| 指标名称 | Proportion of city-level hospital coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 市级医院缓冲区 (GDT_Byte) |
| 输出 | 市级医院服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 10 |
| 指标名称 | Proportion of tiered hospital coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 分级医院缓冲区 (GDT_Byte) |
| 输出 | 分级医院服务人口占比，归一化到 [0, 100] |

---

## SDG 4: Quality Education（优质教育）

### Target 4.1.2

| 属性 | 值 |
|------|-----|
| 指标编号 | 11 |
| 指标名称 | Proportion of preschool coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 幼儿园缓冲区 (GDT_Byte) |
| 输出 | 幼儿园服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 12 |
| 指标名称 | Proportion of primary school coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 小学缓冲区 (GDT_Byte) |
| 输出 | 小学服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 13 |
| 指标名称 | Proportion of secondary school coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 中学缓冲区 (GDT_Byte) |
| 输出 | 中学服务人口占比，归一化到 [0, 100] |

---

## SDG 6: Clean Water and Sanitation（清洁饮水和卫生设施）

### Target 6.6.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 14 |
| 指标名称 | Trends in water-related ecosystems over time |
| 计算类型 | `*` Land Proportion |
| CLI 命令 | `sdg-land-proportion` |
| 输入 | LUCC GeoTIFF (GDT_Byte), water body code (e.g., `{4}`) |
| 最大阈值 | 0.3 (≤30% water = 100) |
| 最小阈值 | 0.05 (≤5% water = 0) |
| 归一化方向 | 正向 (higher proportion = higher score) |
| 得分范围 | [0, 100] |

---

## SDG 7: Affordable and Clean Energy（经济适用的清洁能源）

### Target 7.2.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 15 |
| 指标名称 | Proportion of community clean energy facility coverage |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 清洁能源设施缓冲区 (GDT_Byte) |
| 输出 | 清洁能源设施服务人口占比，归一化到 [0, 100] |

---

## SDG 9: Industry, Innovation and Infrastructure（产业、创新和基础设施）

### Target 9.1.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 16 |
| 指标名称 | Percentage of population residing within 2km of a road |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 道路缓冲区 (GDT_Byte, radius=2km) |
| 输出 | 2km 道路缓冲区内人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 17 |
| 指标名称 | Proportion of population served by subway stations |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 地铁站缓冲区 (GDT_Byte) |
| 输出 | 地铁站服务人口占比，归一化到 [0, 100] |

### Target 9.c.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 18 |
| 指标名称 | Proportion of population served by a mobile network |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 移动基站缓冲区 (GDT_Byte) |
| 输出 | 移动网络服务人口占比，归一化到 [0, 100] |

---

## SDG 11: Sustainable Cities and Communities（可持续城市和社区）

### Target 11.2.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 19 |
| 指标名称 | Proportion of population served by subway stations |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 地铁站缓冲区 (GDT_Byte) |
| 输出 | 地铁站服务人口占比，归一化到 [0, 100] |

| 属性 | 值 |
|------|-----|
| 指标编号 | 20 |
| 指标名称 | Proportion of population served by bus stations |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | 人口 GeoTIFF (Float32) + 公交站缓冲区 (GDT_Byte) |
| 输出 | 公交站服务人口占比，归一化到 [0, 100] |

### Target 11.3.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 21 |
| 指标名称 | Ratio of land consumption rate to population growth rate |
| 计算类型 | `****` Total Statistics |
| CLI 命令 | `sdg-1131` |
| 输入 | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) + Initial Population (Float32) + Current Population (Float32) |
| 城市地类编码 | e.g., `{5}` (urban land) |
| 最优阈值 | 1.12 (基于联合国人居署推荐值) |
| 归一化方向 | 中性（偏离最优值扣分） |
| 得分范围 | [0, 100]，城市增长过快或人口增长过快都会扣分 |

**计算公式**：
- 城市用地增长率 = (Current_Urban - Initial_Urban) / Initial_Urban
- 人口增长率 = (Current_Pop - Initial_Pop) / Initial_Pop
- Ratio = 城市用地增长率 / 人口增长率
- Score = triangular_norm(Ratio, min, max, optimal)

### Target 11.7.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 22 |
| 指标名称 | Coverage rate of urban open public spaces |
| 计算类型 | `***` Buffer Zone |
| CLI 命令 | `sdg-buffer-zone` |
| 输入 | LUCC GeoTIFF (GDT_Byte, green space / square codes) + 城市区域缓冲区 (GDT_Byte) |
| 输出 | 城市区域内开放公共空间占比，归一化到 [0, 100] |

---

## SDG 13: Climate Action（气候行动）

### Target 13.2.2

| 属性 | 值 |
|------|-----|
| 指标编号 | 23 |
| 指标名称 | Status of achieving "Carbon Peak" |
| 计算类型 | `****` Total Statistics |
| CLI 命令 | `sdg-1322` |
| 输入 | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) + 各地类碳排放系数 |
| 排放系数 | `unordered_map<int, double>` — key=地类编码, value=排放系数 (正=排放, 负=吸收) |
| 减排强度比例 | `dRatio`, range (0, 1) |
| 得分规则 | 排放减少 = 100 分（已达峰）, 排放增加 = 按比例线性归一化到 [0, 100) |

**排放系数参考**（来自论文 Yao et al., 2023）：

| 地类 | 编码 | 排放系数 (tC/ha/yr) |
|------|------|-------------------|
| Cropland | 1 | ~0.5 |
| Forest | 2 | ~-1.2 (carbon sink) |
| Grassland | 3 | ~0.1 |
| Water | 4 | ~0 |
| Urban/Built-up | 5 | ~50.0 |
| Barren | 6 | ~0 |

> 实际系数需根据研究区域校准。

---

## SDG 14: Life below Water（水下生物）

### Target 14.5.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 24 |
| 指标名称 | Changes in water ecosystem within protected areas |
| 计算类型 | `**` Land Conversion |
| CLI 命令 | `sdg-land-conversion` |
| 输入 | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| 转换类型 | Water body → non-water (within protected area boundaries) |
| 归一化方向 | 负向 (more water loss = lower score) |
| 得分规则 | 水域面积减少比例越大，得分越低；水域稳定或增加 = 100 |
| 最大阈值 | 0.3 (≥30% water loss = 0) |
| 最小阈值 | 0 (no loss = 100) |

---

## SDG 15: Life on Land（陆地生物）

### Target 15.1.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 25 |
| 指标名称 | Percentage of forest area relative to total land area |
| 计算类型 | `*` Land Proportion |
| CLI 命令 | `sdg-land-proportion` |
| 输入 | LUCC GeoTIFF (GDT_Byte), forest code (e.g., `{2}`) |
| 最大阈值 | 0.5 (≥50% forest = 100) |
| 最小阈值 | 0.05 (≤5% forest = 0) |
| 归一化方向 | 正向 (higher proportion = higher score) |
| 得分范围 | [0, 100] |

### Target 15.2.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 26 |
| 指标名称 | Percentage of progress towards sustainable forest management |
| 计算类型 | `**` Land Conversion |
| CLI 命令 | `sdg-land-conversion` |
| 输入 | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| 转换类型 | Forest → non-forest (deforestation) |
| 归一化方向 | 负向 (more deforestation = lower score) |
| 得分规则 | 森林面积减少越多，得分越低；森林增加 = 100 |

### Target 15.3.1

| 属性 | 值 |
|------|-----|
| 指标编号 | 27 |
| 指标名称 | Percentage of degraded land compared to total land area |
| 计算类型 | `**` Land Conversion |
| CLI 命令 | `sdg-land-conversion` |
| 输入 | Initial LUCC (GDT_Byte) + Current LUCC (GDT_Byte) |
| 转换类型 | Vegetation (forest/grassland) → barren/urban (degradation) |
| 归一化方向 | 负向 (more degradation = lower score) |
| 得分规则 | 退化面积比例越大，得分越低；零退化 = 100 |
| 最大阈值 | 0.5 (≥50% degraded = 0) |
| 最小阈值 | 0 (no degradation = 100) |

---

## 优先区域识别规则

基于 SDG 指标计算结果，通过 6 条规则识别需要优先干预的区域：

| 规则 | 类型 | 触发条件 |
|------|------|---------|
| Rule 1 | Land Proportion 下降 | 关键地类（耕地/森林/水域）占比低于阈值 |
| Rule 2 | Land Conversion 负向 | 土地退化/森林砍伐/水域丧失比例超过阈值 |
| Rule 3 | Buffer Zone 不足 | 关键设施服务覆盖人口占比低于阈值 |
| Rule 4 | SDG 11.3.1 失衡 | 城市用地增长 / 人口增长比值偏离最优值 |
| Rule 5 | SDG 13.2.2 未达峰 | 碳排放持续增长，未出现达峰迹象 |
| Rule 6 | 综合排名叠加 | 多个指标同时在同区域触发，等级越高 |

**优先级分级**：
- **Low**（仅监控）：<10% 的 SDG 指标触发
- **Moderate**（中等关注）：20–30% 指标触发
- **High**（重点关注）：40–50% 指标触发
- **Critical**（重大措施）：>50% 指标触发

代码位置：`ExtractPriorityAreas::extractRuleN()` (N=1~6) + `mergePriorityAreas()`

---

## 数据来源参考

| 数据 | 格式 | 分辨率 | 来源 |
|------|------|--------|------|
| 土地利用 | GeoTIFF | 30m | CNLUCC (Resource and Environment Science and Data Center) |
| 人口分布 | GeoTIFF | 1km | WorldPop Hub |
| POI（基础设施） | Shapefile Point | — | OpenStreetMap / Gaode |
| 道路网络 | Shapefile Line | — | OpenStreetMap |
| DEM | GeoTIFF | 30m | ASTER GDEM |
| 碳排放系数 | CSV/Text | — | Yao et al., 2023 |
| 生态保护区 | Shapefile Polygon | — | Protected Planet |

---

## 完整 CLI 示例

### Land Proportion（计算森林面积占比）

```bash
geosdg-cli sdg-land-proportion \
  --lucctype 2 \
  --input lucc_2020.tif \
  --max 0.5 --min 0.05
```

### Land Conversion（计算森林退化）

```bash
geosdg-cli sdg-land-conversion \
  --ori lucc_2010.tif --new lucc_2020.tif \
  --conv "2->5,6" \
  --max 0.3 --min 0 --state false
```

### Buffer Zone（计算医院服务覆盖）

```bash
geosdg-cli sdg-buffer-zone \
  --input population_2020.tif \
  --buffer hospital_buffer.tif \
  --max 0.9 --min 0.1
```

### SDG 11.3.1（城市协调性）

```bash
geosdg-cli sdg-1131 \
  --lucctype 5 \
  --lucc-initial lucc_2010.tif --lucc-current lucc_2020.tif \
  --pop-initial pop_2010.tif --pop-current pop_2020.tif \
  --best 1.12 --max 5.0 --min 0.5
```

### SDG 13.2.2（碳排放达峰）

```bash
geosdg-cli sdg-1322 \
  --ori lucc_2010.tif --new lucc_2020.tif \
  --ratio 0.1 \
  --scheme "1:0.5,2:-1.2,3:0.1,4:0,5:50,6:0"
```
