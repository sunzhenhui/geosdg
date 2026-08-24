# GeoSDG 项目结构速查表（功能 → 代码文件映射）

## 项目概述

GeoSDG 是一个基于 C++17 / CMake / GDAL 的跨平台 CLI 空间化可持续发展目标（SDG）评估工具，主要功能包括：CA模拟精度评估、空间SDG指标计算、优先区域识别与排名。代码位于 `cli/src/`。**不再依赖 Qt**。

---

## 模块总览

| 模块 | 头文件 | 源文件 | 核心功能 |
|------|--------|--------|----------|
| CA精度评估 | `CalculateCAPrecision.h` | `CalculateCAPrecision.cpp` | CA模拟精度指标计算、相关性分析、t检验 |
| SDG指标计算 | `CalculateSDG.h` | `CalculateSDG.cpp` | 5类空间SDG指标计算及归一化 |
| 优先区域提取 | `ExtractPriorityAreas.h` | `ExtractPriorityAreas.cpp` | 6种规则识别优先区域+排名图生成 |
| 日志系统 | `Logger.h` | `Logger.cpp` | 文件+控制台双通道日志、四级分类、断点续传 |
| 程序入口 | — | `main.cpp` | CLI 工具，16 个子命令 + help，主入口 `main.cpp:674` |

---

## 功能 → 文件 详细映射

### 一、CA模拟精度评估模块 (`CalculateCAPrecision`)

| 功能 | 函数名 | 所在文件 | 说明 |
|------|--------|----------|------|
| CA模拟精度评估 | `calculatePrecision()` | `CalculateCAPrecision.cpp:45` | 计算FoM、PA、UA、Kappa、OA五项精度指标 |
| Pearson相关系数 | `calculateCorrelationCoefficient()` | `CalculateCAPrecision.cpp:338` | 计算两组SDG得分数据的线性相关性 |
| 独立样本t检验 | `tTestIndependent()` | `CalculateCAPrecision.cpp:415` | 检验两组SDG得分是否存在显著差异 |
| 均值计算 | `mean()` | `CalculateCAPrecision.cpp:393` | 私有辅助函数，计算样本均值 |
| 方差计算 | `variance()` | `CalculateCAPrecision.cpp:399` | 私有辅助函数，计算样本方差 |

### 二、SDG指标计算模块 (`CalculateSDG`)

| 功能 | 函数名 | 所在文件 | 指标类别 | 对应SDG |
|------|--------|----------|----------|---------|
| 土地比例指标 | `calculateLandProportionIndicator()` | `CalculateSDG.cpp:34` | Land Proportion | SDG 2.1.2 / 6.6.1 / 15.1.1 |
| 土地转换指标 | `calculateLandConversionIndicator()` | `CalculateSDG.cpp:108` | Land Conversion | SDG 14.5.1 / 15.2.1 / 15.3.1 |
| 缓冲区指标 | `calculateBufferZoneIndicator()` | `CalculateSDG.cpp:233` | Buffer Zone | SDG 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1 |
| SDG 11.3.1指标 | `calculateSDG1131Indicator()` | `CalculateSDG.cpp:429` | Total Statistics | SDG 11.3.1（城市用地增长率/人口增长率） |
| SDG 13.2.2指标 | `calculateSDG1322Indicator()` | `CalculateSDG.cpp:549` | Total Statistics | SDG 13.2.2（碳排放达峰评估） |
| 正向归一化 | `normalization()` | `CalculateSDG.cpp:614` | 辅助函数 | 值越大得分越高（0~100） |
| 负向归一化 | `normalizationNegative()` | `CalculateSDG.cpp:628` | 辅助函数 | 值越小得分越高（0~100） |
| 人口总和统计 | `getPopuSum()` | `CalculateSDG.cpp:641` | 辅助函数 | 读取Float32人口栅格并求和 |
| 城市用地统计 | `getUrbanSum()` | `CalculateSDG.cpp:686` | 辅助函数 | 读取Byte用地栅格统计指定类型像元数 |
| 碳排放总量 | `getEmissionSum()` | `CalculateSDG.cpp:732` | 辅助函数 | 基于排放系数计算土地利用碳排放 |

### 三、优先区域提取模块 (`ExtractPriorityAreas`)

| 功能 | 函数名 | 所在文件 | 规则编号 | 说明 |
|------|--------|----------|----------|------|
| 土地被侵占区域 | `PriorityAreasExtractLUCCLoss()` | `ExtractPriorityAreas.cpp:31` | Rule 1 | 识别特定土地利用类型被其他类型侵占的区域 |
| 土地转换区域 | `PriorityAreasExtractLUCCTransition()` | `ExtractPriorityAreas.cpp:131` | Rule 2 | 识别特定土地利用转换类型的区域 |
| 缓冲区外区域 | `PriorityAreasExtractOutsideBufferArea()` | `ExtractPriorityAreas.cpp:237` | Rule 3/4 | 识别基础设施未覆盖+特定用地/人口阈值区域 |
| 碳排放未达峰区域 | `PriorityAreasExtractEmissionNoPeak()` | `ExtractPriorityAreas.cpp:277` | Rule 5 | 识别邻域内碳排放仍上升的区域 |
| 人地关系失调区域 | `PriorityAreasExtractHumanLandRelationship()` | `ExtractPriorityAreas.cpp:320` | Rule 6 | 识别城市扩张与人口变化不匹配区域 |
| 生成排名图 | `generatePriorityAreas()` | `ExtractPriorityAreas.cpp:415` | 合并 | 叠加6类优先区域文件生成排名图 |
| 提取用地优先区(缓冲区外) | `extractLUCC()` | `ExtractPriorityAreas.cpp:495` | 私有辅助 | LUCC数据在缓冲区外的特定类型 |
| 提取人口优先区(缓冲区外) | `extractPOPU()` | `ExtractPriorityAreas.cpp:573` | 私有辅助 | 人口数据在缓冲区外超阈值区域 |
| 计算排放前缀和 | `calculatePrefixEmisiion()` | `ExtractPriorityAreas.cpp:657` | 私有辅助 | 二维前缀和加速邻域排放计算 |
| 提取排放增长区域 | `extractEmissionIncreaseLand()` | `ExtractPriorityAreas.cpp:740` | 私有辅助 | 利用前缀和判断邻域排放是否增加 |
| 移除NoData | `removeNoDataFromSecondRaster()` | `ExtractPriorityAreas.cpp:820` | 私有辅助 | 用参考栅格NoData掩膜目标栅格 |

### 四、程序入口 (`main.cpp`)

CLI 工具，支持 16 个子命令，通过 `parseArgs()` 解析参数，`dispatch()` 路由到对应计算模块。

| 子命令 | 所属模块 | 功能 |
|--------|----------|------|
| `ca-precision` | CalculateCAPrecision | CA模拟精度评估 |
| `correlation` | CalculateCAPrecision | Pearson相关系数 |
| `t-test` | CalculateCAPrecision | 独立样本t检验 |
| `sdg-land-proportion` | CalculateSDG | 土地比例指标 |
| `sdg-land-conversion` | CalculateSDG | 土地转换指标 |
| `sdg-buffer-zone` | CalculateSDG | 缓冲区指标 |
| `sdg-1131` | CalculateSDG | SDG 11.3.1 |
| `sdg-1322` | CalculateSDG | SDG 13.2.2 |
| `priority-loss` | ExtractPriorityAreas | Rule 1: 土地被侵占 |
| `priority-transition` | ExtractPriorityAreas | Rule 2: 特定转换 |
| `priority-buffer` | ExtractPriorityAreas | Rule 3/4: 缓冲区外 |
| `priority-emission` | ExtractPriorityAreas | Rule 5: 排放未达峰 |
| `priority-human-land` | ExtractPriorityAreas | Rule 6: 人地失调 |
| `priority-merge` | ExtractPriorityAreas | 合并排名图 |
| `demo` | 全部 | 8步全流程演示，支持 `--resume` 断点续传 |
| `help` | — | 显示帮助信息 |

---

## 数据依赖映射

| 数据目录 | 数据类型 | 格式 | 使用的模块 |
|----------|----------|------|-----------|
| `data/LUCC/` | 土地利用覆盖 | GeoTIFF (GDT_Byte) | `CalculateSDG`, `ExtractPriorityAreas`, `CalculateCAPrecision` |
| `data/POPU/` | 人口分布 | GeoTIFF (GDT_Float32/64)（Rule 6 仅 Float32） | `CalculateSDG`, `ExtractPriorityAreas` |
| `data/INFRA/` | 基础设施覆盖 | GeoTIFF (GDT_Byte) | `CalculateSDG`, `ExtractPriorityAreas` |
| `data/Simulation.tif` | CA模拟结果 | GeoTIFF (GDT_Byte) | `CalculateCAPrecision` |

---

## 依赖库

| 库 | 最低版本 | 用途 |
|----|---------|------|
| CMake | 3.10 | 跨平台构建系统，含平台自动检测（`GEOSDG_PLATFORM_WINDOWS/MACOS/LINUX`） |
| GDAL | — | 栅格数据读写、空间参考处理（不绑定特定版本） |

**注意**：CLI 版本**不再依赖 Qt**。旧 UI 版使用的 Qt 5.10 已被移除。构建方式见项目 `geosdg-cli/CMakeLists.txt`。

---

## 功能 → 常见问题索引

> 详细问题排查请参考 [GeoSDG-Operation-Troubleshooting-Quick-Reference.md](./GeoSDG-Operation-Troubleshooting-Quick-Reference.md)

### CalculateCAPrecision 模块问题

| 功能函数 | 常见问题 | 关键词 |
|----------|----------|--------|
| `calculatePrecision()` | 文件打开失败返回全零 | `GDALOpen返回NULL` |
| `calculatePrecision()` | 数据类型非Byte静默返回 | `GDT_Byte检查失败` |
| `calculatePrecision()` | 三幅影像尺寸不一致 | `尺寸不匹配` |
| `calculatePrecision()` | FoM分母为零 | `A+B+C+D=0` |
| `calculateCorrelationCoefficient()` | 向量长度不等抛异常 | `向量长度不等/为空` |
| `tTestIndependent()` | 样本量=1方差除零 | `样本量=1` |

### CalculateSDG 模块问题

| 功能函数 | 常见问题 | 关键词 |
|----------|----------|--------|
| `calculateLandProportionIndicator()` | 以GA_Update打开可能修改源文件 | `GA_Update` |
| `calculateLandProportionIndicator()` | NoData值与Byte比较异常 | `NoData为非整数` |
| `calculateLandConversionIndicator()` | 两期数据维度不匹配 | `维度不匹配` |
| `calculateLandConversionIndicator()` | nAllCount为0导致除零 | `nAllCount为0` |
| `calculateBufferZoneIndicator()` | 人口数据不检查地类类型 | `Float类型跳过类型过滤` |
| `calculateBufferZoneIndicator()` | dOriSum为0导致除零 | `dOriSum为0` |
| `calculateSDG1131Indicator()` | 人口增长率为0除零 | `_dRatioPOPU=0` |
| `calculateSDG1131Indicator()` | 多期仅用第一对 | `vRatios[0]` |
| `calculateSDG1322Indicator()` | 排放系数map被原地修改 | `vLUCCEmissionScheme被修改` |
| `calculateSDG1322Indicator()` | 归一化分母为0 | `min=0` |

### ExtractPriorityAreas 模块问题

| 功能函数 | 常见问题 | 关键词 |
|----------|----------|--------|
| `PriorityAreasExtractLUCCLoss()` | 输出文件创建失败 | `dst=nullptr` |
| `PriorityAreasExtractLUCCLoss()` | GeoTransform获取失败 | `CE_Failure` |
| `PriorityAreasExtractOutsideBufferArea()` | 数据类型自动识别分发 | `GDT_Byte→extractLUCC` |
| `PriorityAreasExtractEmissionNoPeak()` | tmp目录不存在 | `../tmp/` |
| `PriorityAreasExtractEmissionNoPeak()` | 临时文件清理不完整 | `重复删除ChangedPrefix` |
| `PriorityAreasExtractHumanLandRelationship()` | 人口仅支持Float32 | `GDT_Float32硬编码` |
| `PriorityAreasExtractHumanLandRelationship()` | 边界邻域不完整 | `边界裁剪` |
| `generatePriorityAreas()` | 输入文件尺寸不一致越界 | `行列数不同` |
| `generatePriorityAreas()` | 不同分辨率导致差异 | `分辨率不一致` |

### 通用问题

| 问题描述 | 关键词 |
|----------|--------|
| 跨平台编译失败（CMake/GDAL 未找到） | `CMake/GDAL/brew install gdal` |
| GDAL中文路径异常 | `GDAL_FILENAME_IS_UTF8` |
| 数据坐标系必须一致 | `坐标系/行列数/分辨率` |
| 人口与LUCC分辨率不同 | `重采样/对齐` |
| 行政区划裁剪 | `shapefile裁剪` |
| 日志文件未创建 | `Logger/init/logPath` |
| 断点无法恢复 | `readLastCheckpoint/CHECKPOINT` |
