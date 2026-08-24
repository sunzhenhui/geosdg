# GeoSDG 操作问题速查表（操作 → 输入/输出/预期结果/问题）

> 基于 CLI 源代码 (`cli/src/`) 和 PDF 文档 `Introduction for GeoSDG.pdf` 分析生成

---

## 操作1：CA模拟精度评估

| 项目 | 详情 |
|------|------|
| **操作名称** | 评估CA模拟结果的精度 |
| **CLI 调用示例** | `geosdg-cli ca-precision --ori ../data/LUCC/2010.tif --sim ../data/LUCC/Simulation.tif --real ../data/LUCC/2020.tif` |
| **论文对应** | Section 3.2: Implementation results (Land use and infrastructure coverage) |
| **输入** | ① 原始土地利用数据 (`2010.tif`, GDT_Byte) ② 模拟结果数据 (`Simulation.tif`, GDT_Byte) ③ 真实土地利用数据 (`2020.tif`, GDT_Byte) |
| **输出** | FoM（Figure of Merit）、PA（Producer's Accuracy）、UA（User's Accuracy）、Kappa系数、OA（Overall Accuracy） |
| **预期结果** | FoM和OA值用于衡量CA模型（如FLUS/PLUS）模拟精度 |
| **代码位置** | `CalculateCAPrecision.cpp:45` (`calculatePrecision`) |
| **调用示例** | `main.cpp:26-29` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 文件无法打开，返回全零 | `GDALOpen返回NULL` | `CalculateCAPrecision.cpp:33-36` — 输入路径错误或文件不存在 |
| 数据类型不匹配，函数静默返回 | `GDT_Byte检查失败` | `CalculateCAPrecision.cpp:41-44` — 输入必须为Byte类型且单波段 |
| 栅格尺寸不一致，函数静默返回 | `尺寸不匹配` | `CalculateCAPrecision.cpp:58-65` — 三幅影像行列数必须完全一致 |
| FoM值为NaN或异常 | `分母为零` | `CalculateCAPrecision.cpp:140` — 若A+B+C+D=0（无变化区域），FoM除零 |
| Kappa值异常 | `_pe接近1` | `CalculateCAPrecision.cpp:199` — 若偶然一致性极高，Kappa分母接近0 |

---

## 操作2：Pearson相关系数计算

| 项目 | 详情 |
|------|------|
| **操作名称** | 计算两组SDG得分的Pearson相关系数 |
| **CLI 调用示例** | `geosdg-cli correlation --data1 34.7,34.4,51.8,51.1,44.1 --data2 45.2,45.5,47.6,52.7,48.7` |
| **论文对应** | Section 3.2: Implementation results for SDG scores (Table 3) |
| **输入** | ① 传统统计方法SDG得分数组 `data1` ② GeoSDG空间化SDG得分数组 `data2` |
| **输出** | Pearson相关系数（-1 ~ 1） |
| **预期结果** | 验证空间化SDG指数与传统统计方法的一致性 |
| **代码位置** | `CalculateCAPrecision.cpp:338` (`calculateCorrelationCoefficient`) |
| **调用示例** | `main.cpp:30-33` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 抛出invalid_argument异常 | `向量长度不等/为空` | `CalculateCAPrecision.cpp:212-214` — 两组数据长度必须相同且非空 |
| 返回0 | `分母为零` | `CalculateCAPrecision.cpp:226-228` — 数据方差为0（常数序列），无法计算 |

---

## 操作3：独立样本t检验

| 项目 | 详情 |
|------|------|
| **操作名称** | 检验两组SDG得分是否存在显著差异 |
| **CLI 调用示例** | `geosdg-cli t-test --data1 34.7,34.4,51.8,51.1,44.1 --data2 45.2,45.5,47.6,52.7,48.7` |
| **论文对应** | Section 3.2: Table 3 |
| **输入** | ① 样本1 SDG得分 `data1` ② 样本2 SDG得分 `data2` |
| **输出** | t统计量 |
| **预期结果** | 结合自由度和显著性水平判断两组得分差异是否显著 |
| **代码位置** | `CalculateCAPrecision.cpp:415` (`tTestIndependent`) |
| **调用示例** | `main.cpp:34-35` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 方差计算除零 | `样本量=1` | `CalculateCAPrecision.cpp:244` — 方差除以(n-1)，n=1时除零；但函数未做保护 |
| t值异常大 | `方差极小` | `CalculateCAPrecision.cpp:258` — 当两组方差极小时，t统计量可能溢出 |

---

## 操作4：土地比例指标计算（SDG 2.1.2 / 6.6.1 / 15.1.1）

| 项目 | 详情 |
|------|------|
| **操作名称** | 计算特定土地利用类型占比的归一化得分 |
| **CLI 调用示例** | `geosdg-cli sdg-land-proportion --init-lucc ../data/LUCC/2025.tif --types 2,3 --max 100 --min 0` |
| **论文对应** | Figure 9: Land Proportion Indicators |
| **输入** | ① 土地利用数据 (GeoTIFF, GDT_Byte) ② 最大阈值 `dMaxThreshold` ③ 最小阈值 `dMinThreshold` ④ 观测地类集合 `setLuccTypeSDG` |
| **输出** | 归一化得分（0~100） |
| **预期结果** | 指定地类占比越高（正向指标），得分越高 |
| **代码位置** | `CalculateSDG.cpp:34` (`calculateLandProportionIndicator`) |
| **调用示例** | `main.cpp:41-48` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 返回0 | `GDALOpen失败` | `CalculateSDG.cpp:58-61` — 文件路径错误或格式不支持 |
| 以GA_Update方式打开 | `只读数据被修改` | `CalculateSDG.cpp:58` — 使用`GA_Update`打开，可能意外修改输入文件 |
| 得分恒为0或100 | `阈值设置不当` | `CalculateSDG.cpp:517-518` — 若实际比例<=dMinThreshold返回0，>=dMaxThreshold返回100 |
| NoData值判断异常 | `NoData为非整数` | `CalculateSDG.cpp:71` — Byte类型与double NoData比较，若NoData非0~255整数可能失败 |

---

## 操作5：土地转换指标计算（SDG 14.5.1 / 15.2.1 / 15.3.1）

| 项目 | 详情 |
|------|------|
| **操作名称** | 计算特定土地利用转换比例的归一化得分 |
| **CLI 调用示例** | `geosdg-cli sdg-land-conversion --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --transitions 2:5:6,4:5 --positive` |
| **论文对应** | Figure 9: Land Conversion Indicators |
| **输入** | ① 初始期土地利用数据 ② 变化期土地利用数据 ③ 转换类型映射 ④ 最大阈值 ⑤ 最小阈值 ⑥ 正/负向标识 `bState` |
| **输出** | 归一化得分（0~100）；正向指标越高越好，负向指标越低越好 |
| **预期结果** | 根据`bState`决定归一化方向 |
| **代码位置** | `CalculateSDG.cpp:108` (`calculateLandConversionIndicator`) |
| **调用示例** | `main.cpp:52-60` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 返回0 | `维度不匹配` | `CalculateSDG.cpp:154-158` — 两期数据行列数必须一致 |
| 转换比例分母异常 | `nAllCount为0` | `CalculateSDG.cpp:195` — 若无匹配的转换类型，除零导致NaN |
| 归一化方向错误 | `bState设置反` | `CalculateSDG.cpp:200-201` — 正向用`normalization`，负向用`normalizationNegative` |
| 转换映射逻辑复杂 | `targetLuccTypesSet含first` | `CalculateSDG.cpp:187` — 当targetTypes包含源类型时，nAllCount也计入"保持不变" |

---

## 操作6：缓冲区指标计算（SDG 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1）

| 项目 | 详情 |
|------|------|
| **操作名称** | 计算缓冲区（基础设施覆盖区）内外特定地类/人口的覆盖率得分 |
| **CLI 调用示例** | `geosdg-cli sdg-buffer-zone --init-lucc ../data/LUCC/2025.tif --buffer ../data/INFRA/roads.tif --types 2,3 --max 100 --min 0` |
| **论文对应** | Figure 9: Buffer Zone Indicators |
| **输入** | ① 土地利用或人口数据（自动识别类型） ② 缓冲区（基础设施）覆盖数据 ③ 观测地类集合 ④ 最大阈值 ⑤ 最小阈值 |
| **输出** | 归一化得分（0~100） |
| **预期结果** | 基础设施覆盖区内指定类型占比越高，得分越高 |
| **代码位置** | `CalculateSDG.cpp:233` (`calculateBufferZoneIndicator`) |
| **调用示例** | `main.cpp:64-76` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 返回0 | `数据类型不支持` | `CalculateSDG.cpp:249-252` — 仅支持GDT_Byte/Float32/Float64 |
| 人口数据未用LUCC类型过滤 | `Float类型跳过类型过滤` | `CalculateSDG.cpp:317-318` — 人口数据(Float)直接累加值，不检查LUCC类型集合 |
| 除零 | `dOriSum为0` | `CalculateSDG.cpp:368` — 若无有效数据，dOriSum=0导致除零 |
| 缓冲区数据无有效层 | `nLayers<=0` | `CalculateSDG.cpp:270-274` — 缓冲区文件无栅格波段 |

---

## 操作7：SDG 11.3.1指标计算

| 项目 | 详情 |
|------|------|
| **操作名称** | 计算城市用地增长率与人口增长率的比值，评估城市化协调性 |
| **CLI 调用示例** | `geosdg-cli sdg-1131 --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --init-popu ../data/POPU/2025.tif --curr-popu ../data/POPU/2050.tif --types 2,3 --max 3.0 --min 0 --best 1.12` |
| **论文对应** | SDG 11.3.1 |
| **输入** | ① 初始期LUCC ② 当前期LUCC ③ 初始期人口 ④ 当前期人口 ⑤ 城市地类集合 ⑥ 最大阈值 ⑦ 最小阈值 ⑧ 最优阈值 |
| **输出** | 归一化得分（0~100），越接近最优值得分越高 |
| **预期结果** | 比值在[min, max]范围内，越接近best得分越高；超出范围得0分 |
| **代码位置** | `CalculateSDG.cpp:429` (`calculateSDG1131Indicator`) |
| **调用示例** | `main.cpp:81-90` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 人口增长率为0导致除零 | `_dRatioPOPU=0` | `CalculateSDG.cpp:431` — 若初始期人口为0或两期人口相同，比值计算除零 |
| 城市用地增长率为0 | `_dRatioLUCC=0` | `CalculateSDG.cpp:429` — 初始期城市用地为0时除零 |
| 仅计算第一对比值 | `vRatios[0]` | `CalculateSDG.cpp:452` — 多期数据仅使用第一对，后续被忽略 |
| 最优阈值=最小/最大阈值 | `best=min或best=max` | `CalculateSDG.cpp:442,448` — 距离除以0，导致NaN |

---

## 操作8：SDG 13.2.2指标计算（碳排放达峰评估）

| 项目 | 详情 |
|------|------|
| **操作名称** | 评估碳排放是否达峰及得分 |
| **CLI 调用示例** | `geosdg-cli sdg-1322 --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3 --ratio 0.012` |
| **论文对应** | SDG 13.2.2 |
| **输入** | ① 初始期LUCC ② 变化期LUCC ③ 各地类排放系数映射 ④ 减排强度比例 |
| **输出** | 若排放减少返回100；否则返回归一化得分 |
| **预期结果** | 碳排放下降得100分，上升则按比例归一化 |
| **代码位置** | `CalculateSDG.cpp:549` (`calculateSDG1322Indicator`) |
| **调用示例** | `main.cpp:93-105` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 排放系数被原地修改 | `vLUCCEmissionScheme被修改` | `CalculateSDG.cpp:479-486` — 传入的map引用被修改，影响后续使用同一map的计算 |
| 归一化分母为0 | `min(dEmissionOriginal,dEmissionChanged)=0` | `CalculateSDG.cpp:491` — 若原始或变化后排放为0，归一化分母为0 |
| 临时文件冲突 | `../tmp/目录不存在` | `ExtractPriorityAreas.cpp:405-416` — Rule 5使用tmp目录，需预先创建 |

---

## 操作9：优先区域识别 — Rule 1（土地被侵占）

| 项目 | 详情 |
|------|------|
| **操作名称** | 识别特定地类被其他类型侵占的区域 |
| **CLI 调用示例** | `geosdg-cli priority-loss --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif -o ../data/PriorityAreas-1.tif --types 2,3` |
| **论文对应** | Figure 11/12: Priority Areas Rule 1 |
| **输入** | ① 初始期LUCC ② 变化期LUCC ③ 输出文件路径 ④ 被侵占地类集合 |
| **输出** | GeoTIFF（1=优先区域，NoData=无效） |
| **预期结果** | 仅标记指定地类转变为其他类型的像元 |
| **代码位置** | `ExtractPriorityAreas.cpp:31` (`PriorityAreasExtractLUCCLoss`) |
| **调用示例** | `main.cpp:112-121` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 输出文件创建失败 | `dst=nullptr` | `ExtractPriorityAreas.cpp:113-118` — 磁盘空间不足或路径无效 |
| GeoTransform获取失败 | `CE_Failure` | `ExtractPriorityAreas.cpp:121-126` — 输入数据缺少空间参考信息 |
| 投影为空时修正 | `ProjectionRef为空` | `ExtractPriorityAreas.cpp:128-131` — 尝试手动设置左上角Y坐标 |

---

## 操作10：优先区域识别 — Rule 2（特定转换类型）

| 项目 | 详情 |
|------|------|
| **操作名称** | 识别特定土地利用转换的区域 |
| **CLI 调用示例** | `geosdg-cli priority-transition --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif -o ../data/PriorityAreas-2.tif --transitions 2:5:6,4:5` |
| **论文对应** | Figure 11/12: Priority Areas Rule 2 |
| **输入** | ① 初始期LUCC ② 变化期LUCC ③ 输出文件路径 ④ 转换类型映射 |
| **输出** | GeoTIFF（1=优先区域，NoData=无效） |
| **预期结果** | 仅标记符合指定转换规则（如2→5, 2→6, 4→5）的像元 |
| **代码位置** | `ExtractPriorityAreas.cpp:131` (`PriorityAreasExtractLUCCTransition`) |
| **调用示例** | `main.cpp:125-136` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 与SDG模块转换逻辑不一致 | `无nAllCount逻辑` | `ExtractPriorityAreas.cpp:284-290` — 优先区域提取只标记匹配转换，不计算比例分母 |

---

## 操作11：优先区域识别 — Rule 3/4（基础设施覆盖外区域）

| 项目 | 详情 |
|------|------|
| **操作名称** | 识别基础设施未覆盖+特定地类/人口阈值区域 |
| **CLI 调用示例** | `geosdg-cli priority-buffer --init-lucc ../data/LUCC/2025.tif --buffer ../data/INFRA/roads.tif -o ../data/PriorityAreas-3.tif --types 5 --pop-threshold 1000` |
| **论文对应** | Figure 11/12: Priority Areas Rule 3/4 |
| **输入** | ① LUCC或人口数据 ② 基础设施覆盖数据 ③ 输出文件路径 ④ 地类集合 ⑤ 人口阈值 |
| **输出** | GeoTIFF（1=优先区域，0=非优先） |
| **预期结果** | Rule 3: 特定地类不在基础设施覆盖区；Rule 4: 人口超阈值且不在覆盖区 |
| **代码位置** | `ExtractPriorityAreas.cpp:237` (`PriorityAreasExtractOutsideBufferArea`) |
| **调用示例** | `main.cpp:139-158` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 数据类型自动识别 | `GDT_Byte→extractLUCC, Float→extractPOPU` | `ExtractPriorityAreas.cpp:361-364` — 根据输入数据类型自动分发 |
| 人口数据不使用地类过滤 | `extractPOPU无LUCC类型参数` | `ExtractPriorityAreas.cpp:769` — 人口模式仅按阈值+覆盖判断 |
| 缓冲区未覆盖判断逻辑 | `BufferNodata=未覆盖` | `ExtractPriorityAreas.cpp:735` — 缓冲区NoData像元视为未覆盖 |

---

## 操作12：优先区域识别 — Rule 5（碳排放未达峰区域）

| 项目 | 详情 |
|------|--------|
| **操作名称** | 识别邻域内碳排放仍上升的区域 |
| **CLI 调用示例** | `geosdg-cli priority-emission --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif -o ../data/PriorityAreas-5.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3 --ratio 0.012 --radius 5` |
| **论文对应** | Figure 11/12: Priority Areas Rule 5 |
| **输入** | ① 初始期LUCC ② 变化期LUCC ③ 输出文件路径 ④ 排放系数映射 ⑤ 减排比例 ⑥ 邻域半径 |
| **输出** | GeoTIFF（1=排放增加区域） |
| **预期结果** | 利用二维前缀和快速计算邻域排放，标记增加区域 |
| **代码位置** | `ExtractPriorityAreas.cpp:277` (`PriorityAreasExtractEmissionNoPeak`) |
| **调用示例** | `main.cpp:162-171` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 临时文件目录不存在 | `../tmp/` | `ExtractPriorityAreas.cpp:405-416` — 需要手动创建tmp目录，否则GDAL创建失败 |
| 排放系数map被修改 | `原地修改vLUCCEmissionScheme` | `ExtractPriorityAreas.cpp:406-413` — 正系数乘(1-dRatio)，负系数乘(1+dRatio) |
| 前缀和越界异常 | `vLUCCEmissionScheme键不存在` | `ExtractPriorityAreas.cpp:950-956` — 若地类编码不在排放方案中，`[]`操作插入0 |
| 临时文件清理不完整 | `重复删除ChangedPrefix` | `ExtractPriorityAreas.cpp:419` — 列表中ChangedPrefix出现两次，OriginalPrefix未删除 |

---

## 操作13：优先区域识别 — Rule 6（人地关系失调区域）

| 项目 | 详情 |
|------|------|
| **操作名称** | 识别城市用地扩张/缩减与人口增长/减少不匹配的区域 |
| **CLI 调用示例** | `geosdg-cli priority-human-land --init-lucc ../data/LUCC/2025.tif --curr-lucc ../data/LUCC/2050.tif --init-popu ../data/POPU/2025.tif --curr-popu ../data/POPU/2050.tif -o ../data/PriorityAreas-6.tif --types 5 --radius 3` |
| **论文对应** | Figure 11/12: Priority Areas Rule 6 |
| **输入** | ① 初始期LUCC ② 变化期LUCC ③ 初始期人口 ④ 变化期人口 ⑤ 邻域半径 ⑥ 输出文件路径 ⑦ 城市地类集合 |
| **输出** | GeoTIFF（1=失调区域，0=正常） |
| **预期结果** | 标记"城市增+人口减"或"城市减+人口增"的邻域 |
| **代码位置** | `ExtractPriorityAreas.cpp:320` (`PriorityAreasExtractHumanLandRelationship`) |
| **调用示例** | `main.cpp:174-183` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 邻域统计包含自身 | `offsets排除(0,0)` | `ExtractPriorityAreas.cpp:503` — 已排除中心像元 |
| 人口数据必须Float32 | `GDT_Float32硬编码` | `ExtractPriorityAreas.cpp:497-498` — 仅读取Float32，Float64人口数据会出错 |
| 边界像元邻域不完整 | `边界裁剪` | `ExtractPriorityAreas.cpp:526` — 边界处邻域被截断，统计可能偏低 |

---

## 操作14：生成优先区域排名图

| 项目 | 详情 |
|------|------|
| **操作名称** | 叠加6类优先区域文件生成综合排名图 |
| **CLI 调用示例** | `geosdg-cli priority-merge --files ../data/PriorityAreas-1.tif,../data/PriorityAreas-2.tif,../data/PriorityAreas-3.tif,../data/PriorityAreas-4.tif,../data/PriorityAreas-5.tif,../data/PriorityAreas-6.tif -o ../data/PriorityAreasRankingMap.tif` |
| **论文对应** | Figure 11/12: Priority Areas Ranking Map |
| **输入** | ① PriorityAreas-1.tif ~ PriorityAreas-6.tif 的文件路径列表 |
| **输出** | GeoTIFF（像元值=被标记次数，0~6，值越高越需关注） |
| **预期结果** | 值为n表示该像元被n条规则标记为优先区域 |
| **代码位置** | `ExtractPriorityAreas.cpp:415` (`generatePriorityAreas`) |
| **调用示例** | `main.cpp:186-196` |

### 可能遇到的问题

| 问题 | 关键词 | 对应模块/文件/原理 |
|------|--------|-------------------|
| 输入文件尺寸/坐标系不一致 | `行列数不同` | `ExtractPriorityAreas.cpp:596-634` — 未做尺寸一致性检查，不一致时越界访问 |
| NoData值处理不一致 | `各文件NoData不同` | `ExtractPriorityAreas.cpp:621-630` — 每个文件独立判断NoData，不同NoData值可能造成误判 |
| 不同分辨率导致优先区域差异 | `分辨率不一致` | PDF Page 13-14 — 需重采样到统一分辨率后再合并 |

---

## 通用问题速查

| 问题 | 关键词 | 涉及模块/文件/原理 |
|------|--------|-------------------|
| 跨平台编译失败 | `CMake/GDAL/brew install gdal` | CMakeLists.txt 已支持三平台，GDAL 需通过包管理器安装 |
| GDAL中文路径问题 | `GDAL_FILENAME_IS_UTF8` | 所有函数均设置`GDAL_FILENAME_IS_UTF8=NO`，中文路径可能异常 |
| 输入数据坐标系必须一致 | `坐标系/行列数/分辨率` | 所有函数均不检查坐标系一致性，需用户自行保证 |
| 人口数据与LUCC数据分辨率不同 | `重采样/对齐` | PDF Page 9 — 需确保行列维度和坐标系统一对齐 |
| 行政区划裁剪 | `shapefile裁剪` | PDF Page 10 — 需使用外部工具（QGIS/ArcGIS）按行政区裁剪后计算 |
| 日志文件未创建 | `Logger/init/logPath` | `Logger::instance().init(logPath)` 自动创建目录，检查写入权限 |
| 断点无法恢复 | `readLastCheckpoint/CHECKPOINT` | 日志中无有效 `[CHECKPOINT]` 行，或 `--log` 路径与写入时不一致 |
| 构建时找不到GDAL | `find_package(GDAL)/brew install gdal` | macOS 需 `brew install gdal`，Linux 需系统包管理器安装 GDAL 开发包 |

---

## 反向索引：问题关键词 → 模块/文件/操作

> 用于快速根据问题关键词定位到具体模块和操作

| 问题关键词 | 所属模块 | 对应文件 | 相关操作编号 |
|-----------|----------|----------|-------------|
| `GDALOpen返回NULL` | CalculateCAPrecision | `CalculateCAPrecision.cpp:33` | 操作1 |
| `GDT_Byte检查失败` | CalculateCAPrecision | `CalculateCAPrecision.cpp:41` | 操作1 |
| `尺寸不匹配` | CalculateCAPrecision | `CalculateCAPrecision.cpp:58` | 操作1, 操作5 |
| `A+B+C+D=0` | CalculateCAPrecision | `CalculateCAPrecision.cpp:140` | 操作1 |
| `_pe接近1` | CalculateCAPrecision | `CalculateCAPrecision.cpp:199` | 操作1 |
| `向量长度不等/为空` | CalculateCAPrecision | `CalculateCAPrecision.cpp:212` | 操作2 |
| `分母为零` | CalculateCAPrecision | `CalculateCAPrecision.cpp:226` | 操作2 |
| `样本量=1` | CalculateCAPrecision | `CalculateCAPrecision.cpp:244` | 操作3 |
| `方差极小` | CalculateCAPrecision | `CalculateCAPrecision.cpp:258` | 操作3 |
| `GA_Update` | CalculateSDG | `CalculateSDG.cpp:58` | 操作4 |
| `NoData为非整数` | CalculateSDG | `CalculateSDG.cpp:71` | 操作4 |
| `阈值设置不当` | CalculateSDG | `CalculateSDG.cpp:517` | 操作4, 操作5, 操作6 |
| `维度不匹配` | CalculateSDG | `CalculateSDG.cpp:154` | 操作5 |
| `nAllCount为0` | CalculateSDG | `CalculateSDG.cpp:195` | 操作5 |
| `bState设置反` | CalculateSDG | `CalculateSDG.cpp:200` | 操作5 |
| `数据类型不支持` | CalculateSDG | `CalculateSDG.cpp:249` | 操作6 |
| `Float类型跳过类型过滤` | CalculateSDG | `CalculateSDG.cpp:317` | 操作6 |
| `dOriSum为0` | CalculateSDG | `CalculateSDG.cpp:368` | 操作6 |
| `nLayers<=0` | CalculateSDG | `CalculateSDG.cpp:270` | 操作6 |
| `_dRatioPOPU=0` | CalculateSDG | `CalculateSDG.cpp:431` | 操作7 |
| `_dRatioLUCC=0` | CalculateSDG | `CalculateSDG.cpp:429` | 操作7 |
| `vRatios[0]` | CalculateSDG | `CalculateSDG.cpp:452` | 操作7 |
| `best=min或best=max` | CalculateSDG | `CalculateSDG.cpp:442` | 操作7 |
| `vLUCCEmissionScheme被修改` | CalculateSDG | `CalculateSDG.cpp:479` | 操作8 |
| `min(dEmissionOriginal,dEmissionChanged)=0` | CalculateSDG | `CalculateSDG.cpp:491` | 操作8 |
| `dst=nullptr` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:113` | 操作9, 操作10 |
| `CE_Failure` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:121` | 操作9, 操作10 |
| `ProjectionRef为空` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:128` | 操作9, 操作10 |
| `GDT_Byte→extractLUCC` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:361` | 操作11 |
| `BufferNodata=未覆盖` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:735` | 操作11 |
| `../tmp/` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:405` | 操作12 |
| `原地修改vLUCCEmissionScheme` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:406` | 操作12 |
| `vLUCCEmissionScheme键不存在` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:950` | 操作12 |
| `重复删除ChangedPrefix` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:419` | 操作12 |
| `GDT_Float32硬编码` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:497` | 操作13 |
| `边界裁剪` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:526` | 操作13 |
| `行列数不同` | ExtractPriorityAreas | `ExtractPriorityAreas.cpp:596` | 操作14 |
| `分辨率不一致` | ExtractPriorityAreas | — (需外部重采样) | 操作14 |
| `Linux/MacOS/CMakeLists` | 跨平台 | `CMakeLists.txt` | 通用 |
| `GDAL_FILENAME_IS_UTF8` | 编码 | 所有cpp文件 | 通用 |
| `坐标系/行列数/分辨率` | 数据一致性 | 所有函数 | 通用 |
| `重采样/对齐` | 数据预处理 | — (需外部工具) | 通用 |
| `shapefile裁剪` | 数据预处理 | — (需QGIS/ArcGIS) | 通用 |
| `Logger/init/logPath` | 日志 | `Logger.cpp` | 通用 |
| `readLastCheckpoint` | 断点续传 | `Logger.cpp` | 通用 |
| `CMake/GDAL/find_package` | 构建 | `CMakeLists.txt` | 通用 |

---

## 操作流程总图

```
┌─────────────────────────────────────────────────────┐
│                  GeoSDG 完整操作流程                    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ① 数据准备                                         │
│  ├─ LUCC数据 (GeoTIFF, GDT_Byte, 像素值1-6)         │
│  ├─ POPU数据 (GeoTIFF, GDT_Float32/64)              │
│  ├─ INFRA数据 (GeoTIFF, GDT_Byte)                   │
│  └─ 行政边界 (Shapefile, 用于裁剪)                    │
│                                                     │
│  ② CA模拟与精度验证 (操作1)                           │
│  ├─ 训练RF模型 → 概率图                              │
│  ├─ CA模拟分配 → 模拟结果                             │
│  └─ geosdg-cli ca-precision --ori ... --sim ... --real ...│
│                                                     │
│  ③ SDG指标计算 (操作4~8)                             │
│  ├─ Land Proportion → geosdg-cli sdg-land-proportion │
│  ├─ Land Conversion → geosdg-cli sdg-land-conversion │
│  ├─ Buffer Zone → geosdg-cli sdg-buffer-zone         │
│  ├─ Total Statistics → geosdg-cli sdg-1131           │
│  └─ Total Statistics → geosdg-cli sdg-1322           │
│                                                     │
│  ④ 一致性验证 (操作2~3)                              │
│  ├─ geosdg-cli correlation --data1 ... --data2 ...   │
│  └─ geosdg-cli t-test --data1 ... --data2 ...        │
│                                                     │
│  ⑤ 优先区域识别 (操作9~14)                            │
│  ├─ Rule 1: geosdg-cli priority-loss                 │
│  ├─ Rule 2: geosdg-cli priority-transition           │
│  ├─ Rule 3/4: geosdg-cli priority-buffer             │
│  ├─ Rule 5: geosdg-cli priority-emission             │
│  ├─ Rule 6: geosdg-cli priority-human-land           │
│  └─ 合并: geosdg-cli priority-merge → 排名图          │
│                                                     │
│  ⑥ 结果可视化                                        │
│  └─ 在QGIS/ArcGIS中展示SDG分数分布和优先区域排名图      │
│                                                     │
└─────────────────────────────────────────────────────┘
```
