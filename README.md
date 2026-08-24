# GeoSDG

**AI 驱动的空间化可持续发展目标（SDG）评估工具**

GeoSDG 将人工智能与地理信息系统（GIS）相结合，为联合国可持续发展目标提供空间化、智能化的评估解决方案。

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

### 1. 环境初始化

```bash
source scripts/env-init.sh
```

脚本会自动检查 CMake、C++17 编译器、GDAL 等依赖，并设置环境变量。

### 2. 编译

```bash
# 默认 Release 构建
scripts/build.sh

# Debug 构建 + 4 并行
scripts/build.sh -t Debug -j 4

# 清理后重新构建
scripts/build.sh -c
```

### 3. 运行

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

### 4. AI 智能模式

使用 CodeBuddy IDE 打开本项目，6 个核心 Skill 会自动加载。直接用自然语言描述任务即可：

| 你想做什么 | 对 AI 这样说 |
|-----------|-------------|
| 计算 SDG 指标 | "帮我算一下 SDG 15.3.1" |
| 评估 CA 精度 | "评估一下模拟精度" |
| 识别优先区域 | "找出需要优先保护的区域" |
| 查询指标定义 | "SDG 11.3.1 怎么计算？" |
| 理解源码 | "CalculateSDG.cpp 的逻辑是什么？" |

## 核心功能

| 功能模块 | CLI 子命令 | 说明 |
|---------|-----------|------|
| SDG 指标计算 | `sdg-land-proportion`, `sdg-land-conversion`, `sdg-buffer-zone`, `sdg-1131`, `sdg-1322` | 覆盖 10 个 SDG · 17 个 Target · 27 个指标 |
| CA 精度评估 | `ca-precision`, `correlation`, `t-test` | FoM / Kappa / 混淆矩阵 |
| 优先区域识别 | `priority-loss`, `priority-buffer`, `priority-emission`, `priority-human-land`, `priority-transition`, `priority-merge` | 6 条规则综合判定 |
| CA 模拟 | `ca-pg`, `ca-simulate`, `markov-predict` | Pg 估计 → Markov 预测 → FLUS CA 迭代 |
| 栅格预处理 | `raster-resample`, `raster-normalize`, `raster-reclassify`, `raster-diff`, `raster-compress` | 重采样/归一化/重分类/变化检测/压缩 |
| 数据检查 | `check`, `demo`, `help` | GeoTIFF 元数据查询、演示、帮助 |

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
