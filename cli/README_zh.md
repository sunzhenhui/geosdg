# geosdg-cli - 地理空间可持续发展目标指标计算工具包

[English](./README_en.md)

Geospatial SDG Indicator Calculation Toolkit — 基于 CA 模拟精度评估与 SDG 指标计算，面向土地利用/覆被变化 (LUCC) 的量化分析框架。

## 项目结构

```
geosdg-cli/
├── CMakeLists.txt          # 构建配置（可执行程序 + 静态/动态库）
├── README.md               # 本文件
└── src/
    ├── main.cpp            # CLI 入口（子命令式，支持各模块独立调用）
    ├── Logger.h / .cpp     # 日志模块（控制台 + 文件，断点续传）
    ├── CalculateCAPrecision.h / .cpp   # CA 模拟精度评估
    ├── CalculateSDG.h / .cpp           # SDG 指标计算
    ├── ExtractPriorityAreas.h / .cpp   # 优先区域识别
    └── RasterPreprocessor.h / .cpp     # 栅格数据预处理（重采样/归一化/重分类/变化检测/压缩）
```

## 构建方式

### 前置条件

- CMake ≥ 3.10
- C++17 编译器 (MSVC / GCC / Clang)
- **Windows**：GDAL 已包含于 `gdal2.0.2/` 目录
- **macOS**：`brew install gdal cmake`
- **Linux**：`apt install libgdal-dev cmake` 或等效命令

### 编译

```bash
cd geosdg-cli
mkdir build && cd build

# 默认构建（静态库 + demo 可执行程序）
cmake ..
cmake --build . --config Release

# 构建动态库版本
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build . --config Release

# 仅构建库，不构建 demo
cmake .. -DBUILD_DEMO=OFF
cmake --build . --config Release
```

### 构建产物

| 目标 | 类型 | Windows | macOS / Linux |
|------|------|---------|---------------|
| `geosdg_static` | 静态库 | `build/lib/geosdg.lib` | `build/lib/libgeosdg.a` |
| `geosdg_shared` | 动态库 | `build/bin/geosdg.dll` | `build/bin/libgeosdg.dylib` / `.so` |
| `geosdg-cli` | 可执行程序 | `build/bin/geosdg-cli.exe` | `build/bin/geosdg-cli` |

## 命令行使用

```
./geosdg-cli <command> [options]
```

> macOS / Linux 从 `build/bin/` 目录运行时需加 `./` 前缀。

### 子命令一览

| 命令 | 说明 |
|------|------|
| `demo` | 运行完整 demo（8 步全流程） |
| `ca-precision` | CA 模拟精度评估（FoM, PA, UA, Kappa, OA） |
| `correlation` | Pearson 相关系数 |
| `t-test` | 独立样本 t 检验 |
| `sdg-land-proportion` | SDG 土地比例指标 |
| `sdg-land-conversion` | SDG 土地转换指标 |
| `sdg-buffer-zone` | SDG 缓冲区指标 |
| `sdg-1131` | SDG 11.3.1 指标（城市/人口增长率比） |
| `sdg-1322` | SDG 13.2.2 指标（碳排放达峰） |
| `priority-loss` | 优先区域 Rule 1：土地被侵占 |
| `priority-transition` | 优先区域 Rule 2：特定转换 |
| `priority-buffer` | 优先区域 Rule 3/4：基础设施覆盖外 |
| `priority-emission` | 优先区域 Rule 5：排放未达峰 |
| `priority-human-land` | 优先区域 Rule 6：人地关系失衡 |
| `priority-merge` | 合并优先区域为排名图 |
| `priority-stats` | 优先区域面积统计（各等级面积） |
| `check` | 检查 GeoTIFF 元数据与数据质量 |
| `resample` | 栅格重采样对齐（到基准栅格网格） |
| `normalize` | 栅格归一化（Min-Max 到目标范围） |
| `reclass` | 栅格重分类与 NoData 标记 |
| `detect-change` | 土地利用变化检测（两期对比） |
| `compress` | 栅格压缩（DEFLATE/LZW/ZSTD/LERC） |
| `version` | 显示版本信息 |
| `help` | 显示帮助 |

### 全局选项

| 选项 | 说明 |
|------|------|
| `--log <path>` | 日志文件路径（默认 `logs/geosdg.log`） |

### 示例

```bash
# 运行完整 demo
./geosdg-cli demo

# 从断点续传
./geosdg-cli demo --resume

# CA 模拟精度评估
./geosdg-cli ca-precision --ori data/2010.tif --sim data/Simulation.tif --real data/2020.tif

# Pearson 相关系数
./geosdg-cli correlation --data1 34.7,34.4,51.8,51.1 --data2 45.2,45.5,47.6,52.7

# SDG 土地比例指标
./geosdg-cli sdg-land-proportion --init-lucc data/2025.tif --types 2,3

# SDG 土地转换指标（正向）
./geosdg-cli sdg-land-conversion --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 --positive

# SDG 土地转换指标（负向）
./geosdg-cli sdg-land-conversion --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 --negative

# SDG 缓冲区指标
./geosdg-cli sdg-buffer-zone --init-lucc data/2025.tif --buffer data/roads.tif --types 2,3

# SDG 11.3.1
./geosdg-cli sdg-1131 --init-lucc data/2025.tif --curr-lucc data/2050.tif --init-popu data/pop2025.tif --curr-popu data/pop2050.tif --types 2,3

# SDG 13.2.2
./geosdg-cli sdg-1322 --init-lucc data/2025.tif --curr-lucc data/2050.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3

# 优先区域 Rule 1：土地被侵占
./geosdg-cli priority-loss --init-lucc data/2025.tif --curr-lucc data/2050.tif --types 2,3 -o output/rule1.tif

# 优先区域 Rule 2：特定转换
./geosdg-cli priority-transition --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 -o output/rule2.tif

# 优先区域 Rule 3：基础设施覆盖外（土地类型）
./geosdg-cli priority-buffer --init-lucc data/2025.tif --buffer data/roads.tif --types 5 -o output/rule3.tif

# 优先区域 Rule 4：基础设施覆盖外（人口）
./geosdg-cli priority-buffer --init-lucc data/pop2025.tif --buffer data/roads.tif --pop-threshold 1000 -o output/rule4.tif

# 优先区域 Rule 5：排放未达峰
./geosdg-cli priority-emission --init-lucc data/2025.tif --curr-lucc data/2050.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3 --radius 5 -o output/rule5.tif

# 优先区域 Rule 6：人地关系失衡
./geosdg-cli priority-human-land --init-lucc data/2025.tif --curr-lucc data/2050.tif --init-popu data/pop2025.tif --curr-popu data/pop2050.tif --types 5 --radius 3 -o output/rule6.tif

# 合并所有优先区域
./geosdg-cli priority-merge --files output/rule1.tif,output/rule2.tif,output/rule3.tif,output/rule4.tif,output/rule5.tif,output/rule6.tif -o output/ranking.tif
```

### 各命令参数详情

#### `demo`

| 选项 | 说明 |
|------|------|
| `--resume` | 从日志中的断点续传，跳过已完成步骤 |

#### `ca-precision`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--ori <path>` | 是 | 原始 LUCC（如 2010.tif） |
| `--sim <path>` | 是 | 模拟 LUCC（如 Simulation.tif） |
| `--real <path>` | 是 | 真实 LUCC（如 2020.tif） |

#### `correlation` / `t-test`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--data1 <v1,v2,...>` | 是 | 第一组样本（逗号分隔） |
| `--data2 <v1,v2,...>` | 是 | 第二组样本（逗号分隔） |

#### `sdg-land-proportion`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | LUCC 数据路径 |
| `--types <1,2,...>` | 是 | 关注的地类编码 |
| `--max <value>` | 否 | 上限阈值（默认 100） |
| `--min <value>` | 否 | 下限阈值（默认 0） |

#### `sdg-land-conversion`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 变化后 LUCC |
| `--transitions <s:t1:t2,...>` | 是 | 转换规则，如 `2:5:6,4:5` |
| `--max <value>` | 否 | 上限阈值（默认 100） |
| `--min <value>` | 否 | 下限阈值（默认 0） |
| `--positive` | 否 | 正向指标（默认） |
| `--negative` | 否 | 负向指标 |

#### `sdg-buffer-zone`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | LUCC 或人口数据路径 |
| `--buffer <path>` | 是 | 缓冲区数据路径 |
| `--types <1,2,...>` | 是 | 地类编码 |
| `--max <value>` | 否 | 上限阈值（默认 100） |
| `--min <value>` | 否 | 下限阈值（默认 0） |

#### `sdg-1131`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 当前 LUCC |
| `--init-popu <path>` | 是 | 初始人口 |
| `--curr-popu <path>` | 是 | 当前人口 |
| `--types <1,2,...>` | 是 | 城市用地编码 |
| `--max <value>` | 否 | 上限阈值（默认 3.0） |
| `--min <value>` | 否 | 下限阈值（默认 0） |
| `--best <value>` | 否 | 最优阈值（默认 1.12） |

#### `sdg-1322`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 变化后 LUCC |
| `--emission <type:factor,...>` | 是 | 排放系数，如 `1:-1,2:-20,5:5` |
| `--ratio <value>` | 否 | 减排比例（默认 0.012） |

#### `priority-loss`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 变化后 LUCC |
| `--types <1,2,...>` | 是 | 被侵占的地类编码 |
| `-o <path>` | 是 | 输出路径 |

#### `priority-transition`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 变化后 LUCC |
| `--transitions <s:t1:t2,...>` | 是 | 转换规则 |
| `-o <path>` | 是 | 输出路径 |

#### `priority-buffer`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | LUCC 或人口数据 |
| `--buffer <path>` | 是 | 基础设施覆盖数据 |
| `--types <1,2,...>` | 否* | 地类编码（Rule 3） |
| `--pop-threshold <v>` | 否 | 人口阈值（Rule 4，默认 1000） |
| `-o <path>` | 是 | 输出路径 |

> *Rule 3 需 `--types`，Rule 4 需 `--pop-threshold`，程序根据输入数据类型自动选择。

#### `priority-emission`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 变化后 LUCC |
| `--emission <type:factor,...>` | 是 | 排放系数 |
| `--ratio <value>` | 否 | 减排比例（默认 0.012） |
| `--radius <value>` | 否 | 邻域半径像素数（默认 5） |
| `-o <path>` | 是 | 输出路径 |

#### `priority-human-land`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--init-lucc <path>` | 是 | 初始 LUCC |
| `--curr-lucc <path>` | 是 | 当前 LUCC |
| `--init-popu <path>` | 是 | 初始人口 |
| `--curr-popu <path>` | 是 | 当前人口 |
| `--types <1,2,...>` | 是 | 城市用地编码 |
| `--radius <value>` | 否 | 邻域半径（默认 3） |
| `-o <path>` | 是 | 输出路径 |

#### `priority-merge`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--files <p1,p2,...>` | 是 | 优先区域文件路径（逗号分隔） |
| `-o <path>` | 是 | 输出排名图路径 |

#### `priority-stats`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--ranking <path>` | 是 | 排名图 GeoTIFF 路径（值 0-6） |

输出格式为 stdout key=value，包含各等级像元数、面积（km²）、总面积、优先区域面积及占比。

#### `resample`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--base <path>` | 是 | 基准栅格路径（目标网格参考） |
| `--inputs <p1,p2,...>` | 是 | 待重采样栅格路径列表（逗号分隔） |
| `--method <nearest\|bilinear>` | 否 | 重采样方法（默认 `nearest`） |
| `--output-dir <path>` | 否 | 输出目录（自动命名 `<原名>_resampled.tif`） |
| `-o <p1,p2,...>` | 否 | 指定输出路径（逗号分隔，与 `--inputs` 一一对应） |

#### `normalize`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--inputs <p1,p2,...>` | 是 | 待归一化栅格路径列表（逗号分隔） |
| `--range <min:max>` | 否 | 目标范围（默认 `0:1`） |
| `--min-val <v>` | 否 | 手动指定最小值（跳过自动扫描） |
| `--max-val <v>` | 否 | 手动指定最大值 |
| `--output-dir <path>` | 否 | 输出目录（自动命名 `<原名>_normalized.tif`） |
| `-o <p1,p2,...>` | 否 | 指定输出路径（逗号分隔） |

#### `reclass`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--input <path>` | 是 | 输入单波段栅格 |
| `--rules <path>` | 否* | 重分类规则 JSON 文件 |
| `--remap <k1:v1,...>` | 否* | 简化版重映射规则（如 `1:10,2:10,3:20`） |
| `--set-nodata <v1,v2,...>` | 否* | 标记为 NoData 的像素值列表 |
| `--nodata-value <v>` | 否 | 输出 NoData 值（默认 `-9999`） |
| `-o <path>` | 是 | 输出栅格路径 |

> *`--rules` 与 `--remap`/`--set-nodata` 二选一，优先使用 `--rules`。

#### `detect-change`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--before <path>` | 是 | 变化前栅格路径 |
| `--after <path>` | 是 | 变化后栅格路径 |
| `--encode` | 否 | 输出编码变化图（`before*1000+after`） |
| `-o <path>` | 是 | 输出变化栅格路径 |

#### `compress`

| 选项 | 必填 | 说明 |
|------|------|------|
| `--input <path>` | 是 | 输入栅格路径 |
| `--method <deflate\|lzw\|zstd\|lerc\|lerc_zstd>` | 否 | 压缩算法（默认 `deflate`） |
| `--level <1-9>` | 否 | 压缩级别（默认 `6`） |
| `--predictor <0\|2\|3>` | 否 | 水平差分预测器（默认 `2`） |
| `--max-error <v>` | 否 | LERC 有损精度（默认 `0.001`） |
| `--tiled` | 否 | 启用分块存储 |
| `--block-size <n>` | 否 | 分块大小（默认 `256`，仅 `--tiled` 有效） |
| `--bigtiff <yes\|no\|if_needed>` | 否 | BigTIFF 模式（默认 `if_needed`） |
| `--overview` | 否 | 生成内嵌金字塔概览图 |
| `-o <path>` | 是 | 输出压缩栅格路径 |

#### `version`

显示版本信息，支持三种调用方式：

```bash
./geosdg-cli version
./geosdg-cli --version
./geosdg-cli -v
```

## 日志与断点续传

程序运行时自动将日志写入本地文件，包含：

- **INFO** — 一般运行信息
- **PROGRESS** — 步骤进度（如 `3/8 Land Proportion Indicator`）
- **RESULT** — 计算结果（如 `FoM = 0.2345`）
- **CHECKPOINT** — 断点标记，用于 `--resume` 续传
- **WARN / ERROR** — 警告与错误

使用 `demo --resume` 时，程序读取日志中最后一条 CHECKPOINT，自动跳过已完成步骤继续执行。

## 库的集成

geosdg-cli 可作为静态库或动态库集成到第三方项目中：

```cmake
# CMakeLists.txt 示例
find_package(geosdg-cli REQUIRED)

target_link_libraries(YourApp PRIVATE geosdg_static)
# 或
target_link_libraries(YourApp PRIVATE geosdg_shared)
```

### 核心类

| 类 | 头文件 | 功能 |
|----|--------|------|
| `CalculateCAPrecision` | `CalculateCAPrecision.h` | CA 模拟精度评估（FoM, Kappa, OA 等）、Pearson 相关系数、t 检验 |
| `CalculateSDG` | `CalculateSDG.h` | SDG 指标计算（土地比例、转换、缓冲区、11.3.1、13.2.2） |
| `ExtractPriorityAreas` | `ExtractPriorityAreas.h` | 优先区域识别（6 条规则 + 排名合并） |
| `RasterPreprocessor` | `RasterPreprocessor.h` | 栅格数据预处理（重采样、归一化、重分类、变化检测、压缩） |
| `Logger` | `Logger.h` | 日志记录（单例，支持文件输出与断点续传） |

## 数据目录约定

Demo 模式默认从以下相对路径读取数据：

```
../data/
├── LUCC/           # 土地利用/覆被数据 (GeoTIFF)
│   ├── 2010.tif
│   ├── 2020.tif
│   ├── 2025.tif
│   ├── 2050.tif
│   └── Simulation.tif
├── POPU/           # 人口数据 (GeoTIFF)
│   ├── 2025.tif
│   └── 2050.tif
└── INFRA/          # 基础设施数据
    └── roads.tif
```

输出文件将写入 `../data/` 目录下（PriorityAreas-*.tif, PriorityAreasRankingMap.tif）。

## 功能路线图

> 状态依据 `wiki/features/` 下的需求文档与代码实现情况同步更新。

### ✅ 已完成

| 需求 | 日期 | 说明 |
|------|------|------|
| macOS 平台支持 | 2026-07-21 | CMake 平台检测、GDAL 自适应引入、GDAL_DATA 运行时注入 |
| Skill 更新：从 UI 版迁移到 CLI 版 | 2026-07-21 | CLI 子命令体系对齐 UI 版本全部计算能力 |
| Skill 上下文占用优化 | 2026-07-22 | 分层加载与按需读取，降低 Agent 上下文占用 |
| CA 模拟 CLI 模块 | 2026-07-23 | `ca-pg`（Pg 估计）、`ca-markov`（Markov 需求预测）、`ca-simulate`（CA 迭代模拟） |
| 数据预处理 CLI 模块 | 2026-07-23 | `resample`、`normalize`、`reclass`、`detect-change`、`compress` 5 个子命令 |
| GeoTIFF 质量检查 CLI 模块 | 2026-07-23 | `check`（元数据查询、参考对比、类型覆盖、地类数量、整型校验） |
| README 格式修复 | 2026-07-23 | 修复 Feature Roadmap 表格分隔符 `! \|` 格式错误 |

### 🔄 进行中

（暂无）

### 📋 规划中

| 需求 | 优先级 | 说明 |
|------|--------|------|
| Agent 核心搭建 | — | 让 Agent "会想"：工具层 + Agent 调度能力 |
| Agent 分层记忆架构 | — | 让 Agent "记得住、找得到、用得上" |
| 自动评估与反思机制 | — | Agent 元能力：结果验证与自我纠错 |
| 外部数据连接层 | — | Agent 层外部数据接入能力 |
| 项目工程化建设 | — | 全项目工程化基础设施补全 |
| ML 模型集成框架 | — | ML 驱动的土地利用模拟与 SDG 预测能力 |
| 计算管道编排系统 | — | 训练/推理 Pipeline 编排 |
| RAG 知识库与向量记忆系统 | — | 知识层与记忆层现代化 |
| SDG 综合评估报告生成 | — | 报告生成 + 可视化工具链 |

## 许可证

请参阅项目根目录的许可证文件。
