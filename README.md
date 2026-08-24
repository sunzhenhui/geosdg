# GeoSDG

**AI 驱动的空间化可持续发展目标（SDG）评估工具**

GeoSDG 将人工智能与地理信息系统（GIS）相结合，为联合国可持续发展目标（SDGs）提供空间化、智能化的评估解决方案。它能够将抽象的全球 SDG 指标落实到具体的地理空间上，帮助你回答"某个地方在可持续发展上做得如何"这类问题。

核心能力包括：

- **SDG 指标空间化计算**：将 SDG 15.3.1（土地退化）、SDG 11.3.1（城市扩张）等指标从统计口径转化为可量化的空间分布。
- **土地利用变化模拟（CA）**：基于元胞自动机与随机森林、Markov 需求预测，模拟未来土地利用格局。
- **优先区域识别**：定位人地关系失衡、需要优先保护或干预的区域。
- **自然语言驱动**：内置 AI Agent 体系，用对话即可完成指标计算、模拟与结果解读。

## 关于本版本

GeoSDG 最初作为研究工具随论文发布（Sun et al., 2026）。受论文阶段的技术栈与设计目标所限，随论文发布的原始源码与图形界面在今天的 AI 时代已较难直接应用。为了方便大家改造和更好地使用 GeoSDG，我们将核心算法重构为独立、可编程的命令行工具（CLI），并配套提供了一套 AI Agent Skill 体系，使评估流程既能被自然语言驱动，也能灵活集成到各类工作流中。

> **论文引用**：
> Sun, Zhenhui, Minyi Gao, Mengya Li, Ziheng Xu, and Xia Li. 2026. “New Pathways for UN SDGs Spatialization: GeoSDG Toolkit Empowering the Sustainable Future under a Spatial Context.” *International Journal of Geographical Information Science*, August, 1–24. doi:10.1080/13658816.2026.2717629.

一个更加智能的 GeoSDG 版本正在开发中，敬请期待。

## 目录结构

```
geosdg/
├── cli/                        # 核心命令行工具（C++17 / CMake）
│   ├── src/                    # 源码（14 个 .cpp + 13 个 .h）
│   ├── include/                # 插件 SDK 头文件
│   ├── examples/               # 插件骨架示例
│   ├── CMakeLists.txt          # 构建配置
│   └── README.md               # CLI 详细文档
│
├── third_party/                # 三方库
│   ├── alglib/                 # ALGLIB 4.08 数值分析库
│   ├── eigen3/                 # Eigen3 线性代数库（header-only）
│   └── gdal/                   # GDAL 预编译（Windows）
│
├── agent/                      # Agent Skill 体系
│   ├── skills/                 # 6 个核心 Skill
│   │   ├── agent-router/       # 意图路由器
│   │   ├── agent-executor/     # 工具执行器
│   │   ├── agent-planner/      # 任务规划器
│   │   ├── geosdg-assistant/   # 项目助手（源码理解）
│   │   ├── sdg-indicator-knowledge/  # SDG 指标知识库
│   │   └── data-standardizer/  # 数据标准化
│   ├── tools/                  # Tool Schema（25 个 JSON）
│   ├── shared/                 # 共享知识（CLI 映射、工具地图、数据语义）
│   └── validation/             # 校验规则
│
├── data/                       # 示例数据（最小集）
│   ├── rasters/                # 23 个核心 GeoTIFF
│   ├── configs/                # 指标参数、图例、优先区域配置
│   ├── _template/              # 新区域数据搭建模板
│   ├── manifest.json           # 数据清单
│   └── DATA_FORMAT_SPEC.md     # 数据格式规范
│
├── scripts/                    # 工具脚本
│   ├── env-init.sh             # 环境初始化
│   ├── build.sh                # 编译脚本
│   ├── release.sh              # 发布脚本
│   └── *.py                    # 知识库/报告脚本
│
└── wiki/                       # 项目文档（按需扩展）
    └── features/
```

## 快速开始

> **前置条件**：本项目使用 [Git LFS](https://git-lfs.com/) 管理 TIFF 数据文件。克隆前请先安装：
> ```bash
> git lfs install
> git clone <repo-url>
> ```

GeoSDG Assistant 内置了完整的 AI Agent 系统，支持**自然语言驱动** —— 你只需要告诉 AI 想做什么，它会自动路由意图、规划步骤、调用 CLI 执行，并生成结果与报告。

### AI 智能模式

使用 CodeBuddy IDE 打开本项目，6 个核心 Skill 会自动加载。直接用自然语言描述任务即可：

| 你想做什么 | 对 AI 这样说 |
|-----------|-------------|
| 初始化环境 & 编译 | "帮我安装依赖并编译项目" |
| 运行演示（快速体验） | "用演示数据跑一遍完整流程" |
| 计算 SDG 指标 | "帮我算一下 SDG 15.3.1，数据在 data/rasters/lucc_demo_2020.tif" |
| 评估 CA 模拟精度 | "评估一下这次模拟的精度，原始数据是 ori.tif，模拟结果是 sim.tif，真实数据是 real.tif" |
| 运行 CA 模拟（完整流程） | "帮我跑一次土地利用模拟，训练期2010年，当前期2020年，预测到2050年" |
| CA 模拟 — 概率估计（Pg） | "用随机森林估算土地利用转换概率，驱动因子是 DEM 和坡度" |
| CA 模拟 — Markov 需求预测 | "基于2010和2020年土地利用数据，预测2030到2050的土地需求" |
| CA 模拟 — 迭代模拟 | "用 FLUS 模型模拟2050年的土地利用，收敛阈值设为0.1%" |
| 基础设施 CA 模拟 | "模拟2030年基础设施扩张，用生态红线约束新建区域" |
| 识别优先区域 | "帮我找出哪些地方需要优先保护，分析人地关系失衡的区域" |
| 查询 SDG 知识 | "SDG 11.3.1 指标怎么定义的？空间化怎么计算？" |
| 理解源码 | "CalculateSDG.cpp 的逻辑是什么？" |

### 古法（手动模式备查）

#### 1. 环境初始化

```bash
source scripts/env-init.sh
```

脚本会自动检查 CMake、C++17 编译器、GDAL 等依赖，并设置环境变量。

#### 2. 编译

```bash
# 默认 Release 构建
scripts/build.sh

# Debug 构建 + 4 并行
scripts/build.sh -t Debug -j 4

# 清理后重新构建
scripts/build.sh -c
```

#### 3. 运行

```bash
# 版本检查
./cli/build/bin/geosdg-cli version

# 演示模式
./cli/build/bin/geosdg-cli demo

# 计算 SDG 指标
./cli/build/bin/geosdg-cli sdg-land-proportion \
    --input data/rasters/lucc_demo_2020.tif \
    --target-type 1 \
    --output data/outputs/result.tif

# CA 精度评估
./cli/build/bin/geosdg-cli ca-precision \
    --original data/rasters/lucc_demo_2020.tif \
    --simulated data/rasters/lucc_demo_2020_sim.tif

# 优先区域识别
./cli/build/bin/geosdg-cli priority-loss \
    --input data/rasters/lucc_demo_2020.tif \
    --output data/outputs/priority.tif
```

> 详细 CLI 用法参见 [cli/README.md](./cli/README.md)。

## 依赖

| 依赖 | 版本 | macOS 安装 | Linux 安装 |
|------|------|-----------|-----------|
| CMake | 3.10+ | `brew install cmake` | `sudo apt install cmake` |
| C++17 编译器 | Clang 14+ / GCC 9+ | Xcode Command Line Tools | `sudo apt install g++` |
| GDAL | 3.0+ | `brew install gdal` | `sudo apt install libgdal-dev` |
| OpenMP | 可选 | `brew install libomp` | `sudo apt install libomp-dev` |

Windows 用户：GDAL 已包含在 `third_party/gdal/` 中。

## Agent Skill 体系

6 个核心 Skill 构成最小闭环：

```
用户提问 → agent-router（意图路由）
         → agent-planner（任务规划）
         → agent-executor（CLI 执行）
         → sdg-indicator-knowledge（指标查询）
         → geosdg-assistant（源码理解）
         → data-standardizer（数据校验）
```

## 开源许可

GPL-3.0
