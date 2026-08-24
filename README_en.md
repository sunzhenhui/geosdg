# GeoSDG

**AI-Powered Spatial Sustainable Development Goals (SDG) Assessment Tool**

GeoSDG combines artificial intelligence with geographic information systems (GIS) to deliver spatialized, intelligent assessment solutions for the United Nations Sustainable Development Goals.

## Directory Structure

```
geosdg/
├── cli/                        # Core command-line tool (C++17 / CMake)
│   ├── src/                    # Source code (14 .cpp + 13 .h)
│   ├── include/                # Plugin SDK headers
│   ├── examples/               # Plugin skeleton examples
│   ├── CMakeLists.txt          # Build configuration
│   └── README.md               # Detailed CLI documentation
│
├── third_party/                # Third-party libraries
│   ├── alglib/                 # ALGLIB 4.08 numerical analysis library
│   ├── eigen3/                 # Eigen3 linear algebra library (header-only)
│   └── gdal/                   # Precompiled GDAL (Windows)
│
├── agent/                      # Agent Skill system
│   ├── skills/                 # 6 core Skills
│   │   ├── agent-router/       # Intent router
│   │   ├── agent-executor/     # Tool executor
│   │   ├── agent-planner/      # Task planner
│   │   ├── geosdg-assistant/   # Project assistant (source-code understanding)
│   │   ├── sdg-indicator-knowledge/  # SDG indicator knowledge base
│   │   └── data-standardizer/  # Data standardization
│   ├── tools/                  # Tool schemas (25 JSON files)
│   ├── shared/                 # Shared knowledge (CLI mapping, tool map, data semantics)
│   └── validation/             # Validation rules
│
├── data/                       # Sample data (minimal set)
│   ├── rasters/                # 23 core GeoTIFF files
│   ├── configs/                # Indicator parameters, legends, priority-area configs
│   ├── _template/              # Template for setting up a new region's data
│   ├── manifest.json           # Data manifest
│   └── DATA_FORMAT_SPEC.md     # Data format specification
│
├── scripts/                    # Utility scripts
│   ├── env-init.sh             # Environment initialization
│   ├── build.sh                # Build script
│   ├── release.sh              # Release script
│   └── *.py                    # Knowledge-base / report scripts
│
└── wiki/                       # Project documentation (extended on demand)
    └── features/
```

## Quick Start

> **Prerequisite**: This project uses [Git LFS](https://git-lfs.com/) to manage TIFF data files. Install it before cloning:
> ```bash
> git lfs install
> git clone <repo-url>
> ```

GeoSDG Assistant ships with a complete AI Agent system that supports **natural-language-driven workflows** — you just tell the AI what you want, and it automatically routes intent, plans steps, invokes the CLI to execute, and produces results and reports.

### AI Smart Mode

Open this project in the CodeBuddy IDE and the 6 core Skills load automatically. Simply describe your task in natural language:

| What you want to do | What to say to the AI |
|---------------------|-----------------------|
| Initialize environment & build | "Install dependencies and build the project for me" |
| Run a demo (quick start) | "Run the full workflow with the demo data" |
| Compute an SDG indicator | "Calculate SDG 15.3.1 for me; the data is at data/rasters/lucc_demo_2020.tif" |
| Assess CA simulation accuracy | "Assess the accuracy of this simulation: original is ori.tif, simulated is sim.tif, real is real.tif" |
| Run CA simulation (full workflow) | "Run a land-use simulation: training period 2010, current period 2020, predict to 2050" |
| CA simulation — probability estimation (Pg) | "Estimate land-use transition probabilities with random forest; driving factors are DEM and slope" |
| CA simulation — Markov demand prediction | "Predict land demand from 2030 to 2050 based on 2010 and 2020 land-use data" |
| CA simulation — iterative simulation | "Simulate land use for 2050 with the FLUS model; set the convergence threshold to 0.1%" |
| Infrastructure CA simulation | "Simulate infrastructure expansion for 2030, using ecological red-line to constrain new areas" |
| Identify priority areas | "Find which areas need priority protection and analyze human-environment imbalance zones" |
| Query SDG knowledge | "How is SDG 11.3.1 defined? How is it spatialized and computed?" |
| Understand source code | "What is the logic of CalculateSDG.cpp?" |

### Manual Mode (for reference)

#### 1. Environment initialization

```bash
source scripts/env-init.sh
```

The script automatically checks CMake, a C++17 compiler, GDAL, and other dependencies, and sets environment variables.

#### 2. Build

```bash
# Default Release build
scripts/build.sh

# Debug build with 4 parallel jobs
scripts/build.sh -t Debug -j 4

# Clean and rebuild
scripts/build.sh -c
```

#### 3. Run

```bash
# Version check
./cli/build/bin/geosdg-cli version

# Demo mode
./cli/build/bin/geosdg-cli demo

# Compute an SDG indicator
./cli/build/bin/geosdg-cli sdg-land-proportion \
    --input data/rasters/lucc_demo_2020.tif \
    --target-type 1 \
    --output data/outputs/result.tif

# CA accuracy assessment
./cli/build/bin/geosdg-cli ca-precision \
    --original data/rasters/lucc_demo_2020.tif \
    --simulated data/rasters/lucc_demo_2020_sim.tif

# Priority-area identification
./cli/build/bin/geosdg-cli priority-loss \
    --input data/rasters/lucc_demo_2020.tif \
    --output data/outputs/priority.tif
```

> See [cli/README.md](./cli/README.md) for detailed CLI usage.

## Dependencies

| Dependency | Version | macOS install | Linux install |
|------------|---------|---------------|---------------|
| CMake | 3.10+ | `brew install cmake` | `sudo apt install cmake` |
| C++17 compiler | Clang 14+ / GCC 9+ | Xcode Command Line Tools | `sudo apt install g++` |
| GDAL | 3.0+ | `brew install gdal` | `sudo apt install libgdal-dev` |
| OpenMP | Optional | `brew install libomp` | `sudo apt install libomp-dev` |

Windows users: GDAL is bundled in `third_party/gdal/`.

## Agent Skill System

The 6 core Skills form a minimal closed loop:

```
User question → agent-router (intent routing)
              → agent-planner (task planning)
              → agent-executor (CLI execution)
              → sdg-indicator-knowledge (indicator lookup)
              → geosdg-assistant (source-code understanding)
              → data-standardizer (data validation)
```

## License

GPL-3.0
