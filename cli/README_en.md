# geosdg-cli - Geospatial SDG Indicator Calculation Toolkit

[中文](./README_zh.md)

A quantitative analysis framework for Land Use / Cover Change (LUCC), supporting CA simulation accuracy assessment and SDG indicator computation.

## Project Structure

```
geosdg-cli/
├── CMakeLists.txt          # Build config (executable + static/shared library)
├── README.md               # This file
└── src/
    ├── main.cpp            # CLI entry point (sub-command dispatch)
    ├── Logger.h / .cpp     # Logging module (console + file, checkpoint resume)
    ├── CalculateCAPrecision.h / .cpp   # CA simulation accuracy assessment
    ├── CalculateSDG.h / .cpp           # SDG indicator computation
    ├── ExtractPriorityAreas.h / .cpp   # Priority area identification
    └── RasterPreprocessor.h / .cpp     # Raster preprocessing (resample / normalize / reclassify / change detection / compress)
```

## Build

### Prerequisites

- CMake ≥ 3.10
- C++17 compiler (MSVC / GCC / Clang)
- **Windows**: GDAL included in `gdal2.0.2/` directory
- **macOS**: `brew install gdal cmake`
- **Linux**: `apt install libgdal-dev cmake` or equivalent

### Compilation

```bash
cd geosdg-cli
mkdir build && cd build

# Default build (static library + demo executable)
cmake ..
cmake --build . --config Release

# Build shared library version
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build . --config Release

# Build library only, skip demo
cmake .. -DBUILD_DEMO=OFF
cmake --build . --config Release
```

### Build Artifacts

| Target | Type | Windows | macOS / Linux |
|--------|------|---------|---------------|
| `geosdg_static` | Static lib | `build/lib/geosdg.lib` | `build/lib/libgeosdg.a` |
| `geosdg_shared` | Shared lib | `build/bin/geosdg.dll` | `build/bin/libgeosdg.dylib` / `.so` |
| `geosdg-cli` | Executable | `build/bin/geosdg-cli.exe` | `build/bin/geosdg-cli` |

## Usage

```
./geosdg-cli <command> [options]
```

> On macOS / Linux, run from `build/bin/` with `./` prefix.

### Sub-commands

| Command | Description |
|---------|-------------|
| `demo` | Run full demo (8-step pipeline) |
| `ca-precision` | CA accuracy (FoM, PA, UA, Kappa, OA) |
| `correlation` | Pearson correlation |
| `t-test` | Independent samples t-test |
| `sdg-land-proportion` | SDG land proportion indicator |
| `sdg-land-conversion` | SDG land conversion indicator |
| `sdg-buffer-zone` | SDG buffer zone indicator |
| `sdg-1131` | SDG 11.3.1 (urban/population growth ratio) |
| `sdg-1322` | SDG 13.2.2 (carbon emission peaking) |
| `priority-loss` | Priority Rule 1: land encroachment |
| `priority-transition` | Priority Rule 2: specific transitions |
| `priority-buffer` | Priority Rule 3/4: outside infra coverage |
| `priority-emission` | Priority Rule 5: emission not peaked |
| `priority-human-land` | Priority Rule 6: human-land imbalance |
| `priority-merge` | Merge priority areas into ranking map |
| `priority-stats` | Priority area area statistics |
| `check` | Check GeoTIFF metadata and data quality |
| `resample` | Resample rasters to base grid alignment |
| `normalize` | Normalize raster values to target range |
| `reclass` | Reclassify raster values and mark NoData |
| `detect-change` | Detect land use change between two periods |
| `compress` | Compress raster file (DEFLATE/LZW/ZSTD/LERC) |
| `version` | Show version |
| `help` | Show help |

### Global Options

| Option | Description |
|--------|-------------|
| `--log <path>` | Log file path (default: `logs/geosdg.log`) |

### Examples

```bash
# Run full demo
./geosdg-cli demo

# Resume from checkpoint
./geosdg-cli demo --resume

# CA simulation accuracy assessment
./geosdg-cli ca-precision --ori data/2010.tif --sim data/Simulation.tif --real data/2020.tif

# Pearson correlation
./geosdg-cli correlation --data1 34.7,34.4,51.8,51.1 --data2 45.2,45.5,47.6,52.7

# SDG land proportion indicator
./geosdg-cli sdg-land-proportion --init-lucc data/2025.tif --types 2,3

# SDG land conversion indicator (positive)
./geosdg-cli sdg-land-conversion --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 --positive

# SDG land conversion indicator (negative)
./geosdg-cli sdg-land-conversion --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 --negative

# SDG buffer zone indicator
./geosdg-cli sdg-buffer-zone --init-lucc data/2025.tif --buffer data/roads.tif --types 2,3

# SDG 11.3.1
./geosdg-cli sdg-1131 --init-lucc data/2025.tif --curr-lucc data/2050.tif --init-popu data/pop2025.tif --curr-popu data/pop2050.tif --types 2,3

# SDG 13.2.2
./geosdg-cli sdg-1322 --init-lucc data/2025.tif --curr-lucc data/2050.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3

# Priority Rule 1: land encroachment
./geosdg-cli priority-loss --init-lucc data/2025.tif --curr-lucc data/2050.tif --types 2,3 -o output/rule1.tif

# Priority Rule 2: specific transitions
./geosdg-cli priority-transition --init-lucc data/2025.tif --curr-lucc data/2050.tif --transitions 2:5:6,4:5 -o output/rule2.tif

# Priority Rule 3: outside infra coverage (land type)
./geosdg-cli priority-buffer --init-lucc data/2025.tif --buffer data/roads.tif --types 5 -o output/rule3.tif

# Priority Rule 4: outside infra coverage (population)
./geosdg-cli priority-buffer --init-lucc data/pop2025.tif --buffer data/roads.tif --pop-threshold 1000 -o output/rule4.tif

# Priority Rule 5: emission not peaked
./geosdg-cli priority-emission --init-lucc data/2025.tif --curr-lucc data/2050.tif --emission 1:-1,2:-20,3:-5,4:-0.5,5:5,6:0.3 --radius 5 -o output/rule5.tif

# Priority Rule 6: human-land imbalance
./geosdg-cli priority-human-land --init-lucc data/2025.tif --curr-lucc data/2050.tif --init-popu data/pop2025.tif --curr-popu data/pop2050.tif --types 5 --radius 3 -o output/rule6.tif

# Merge all priority areas
./geosdg-cli priority-merge --files output/rule1.tif,output/rule2.tif,output/rule3.tif,output/rule4.tif,output/rule5.tif,output/rule6.tif -o output/ranking.tif
```

### Command Parameters

#### `demo`

| Option | Description |
|--------|-------------|
| `--resume` | Resume from the last checkpoint in the log, skipping completed steps |

#### `ca-precision`

| Option | Required | Description |
|--------|----------|-------------|
| `--ori <path>` | Yes | Original LUCC (e.g., 2010.tif) |
| `--sim <path>` | Yes | Simulated LUCC |
| `--real <path>` | Yes | Actual LUCC (e.g., 2020.tif) |

#### `correlation` / `t-test`

| Option | Required | Description |
|--------|----------|-------------|
| `--data1 <v1,v2,...>` | Yes | First sample group |
| `--data2 <v1,v2,...>` | Yes | Second sample group |

#### `sdg-land-proportion`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | LUCC data path |
| `--types <1,2,...>` | Yes | Land type codes |
| `--max <value>` | No | Upper threshold (default: 100) |
| `--min <value>` | No | Lower threshold (default: 0) |

#### `sdg-land-conversion`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Changed LUCC |
| `--transitions <s:t1:t2,...>` | Yes | Transition rules (e.g., `2:5:6,4:5`) |
| `--max <value>` | No | Upper threshold (default: 100) |
| `--min <value>` | No | Lower threshold (default: 0) |
| `--positive` / `--negative` | No | Indicator direction (positive by default) |

#### `sdg-buffer-zone`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | LUCC or population data |
| `--buffer <path>` | Yes | Buffer zone data |
| `--types <1,2,...>` | Yes | Land type codes |
| `--max <value>` | No | Upper threshold (default: 100) |
| `--min <value>` | No | Lower threshold (default: 0) |

#### `sdg-1131`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Current LUCC |
| `--init-popu <path>` | Yes | Initial population |
| `--curr-popu <path>` | Yes | Current population |
| `--types <1,2,...>` | Yes | Urban land type codes |
| `--max <value>` | No | Upper threshold (default: 3.0) |
| `--min <value>` | No | Lower threshold (default: 0) |
| `--best <value>` | No | Optimal threshold (default: 1.12) |

#### `sdg-1322`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Changed LUCC |
| `--emission <type:factor,...>` | Yes | Emission coefficients (e.g., `1:-1,2:-20,5:5`) |
| `--ratio <value>` | No | Reduction ratio (default: 0.012) |

#### `priority-loss`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Changed LUCC |
| `--types <1,2,...>` | Yes | Encroached land type codes |
| `-o <path>` | Yes | Output path |

#### `priority-transition`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Changed LUCC |
| `--transitions <s:t1:t2,...>` | Yes | Transition rules |
| `-o <path>` | Yes | Output path |

#### `priority-buffer`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | LUCC or population data |
| `--buffer <path>` | Yes | Infrastructure coverage data |
| `--types <1,2,...>` | No* | Land type codes (Rule 3) |
| `--pop-threshold <v>` | No | Population threshold (Rule 4, default: 1000) |
| `-o <path>` | Yes | Output path |

> *Rule 3 requires `--types`, Rule 4 requires `--pop-threshold`. Auto-selects based on input type.

#### `priority-emission`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Changed LUCC |
| `--emission <type:factor,...>` | Yes | Emission coefficients |
| `--ratio <value>` | No | Reduction ratio (default: 0.012) |
| `--radius <value>` | No | Neighborhood radius in pixels (default: 5) |
| `-o <path>` | Yes | Output path |

#### `priority-human-land`

| Option | Required | Description |
|--------|----------|-------------|
| `--init-lucc <path>` | Yes | Initial LUCC |
| `--curr-lucc <path>` | Yes | Current LUCC |
| `--init-popu <path>` | Yes | Initial population |
| `--curr-popu <path>` | Yes | Current population |
| `--types <1,2,...>` | Yes | Urban land type codes |
| `--radius <value>` | No | Neighborhood radius (default: 3) |
| `-o <path>` | Yes | Output path |

#### `priority-merge`

| Option | Required | Description |
|--------|----------|-------------|
| `--files <p1,p2,...>` | Yes | Priority area file paths (comma-separated) |
| `-o <path>` | Yes | Output ranking map path |

#### `priority-stats`

| Option | Required | Description |
|--------|----------|-------------|
| `--ranking <path>` | Yes | Ranking map GeoTIFF path (values 0-6) |

Outputs key=value pairs: per-level pixel counts, area (km²), total area, priority area with percentage.

#### `resample`

| Option | Required | Description |
|--------|----------|-------------|
| `--base <path>` | Yes | Base raster (target grid reference) |
| `--inputs <p1,p2,...>` | Yes | Input rasters to resample (comma-separated) |
| `--method <nearest\|bilinear>` | No | Resample method (default: `nearest`) |
| `--output-dir <path>` | No | Output directory (auto-named `<name>_resampled.tif`) |
| `-o <p1,p2,...>` | No | Output paths (comma-separated, must match --inputs count) |

#### `normalize`

| Option | Required | Description |
|--------|----------|-------------|
| `--inputs <p1,p2,...>` | Yes | Input rasters to normalize (comma-separated) |
| `--range <min:max>` | No | Target range (default: `0:1`) |
| `--min-val <v>` | No | Manual minimum value (skip auto-scan) |
| `--max-val <v>` | No | Manual maximum value |
| `--output-dir <path>` | No | Output directory (auto-named `<name>_normalized.tif`) |
| `-o <p1,p2,...>` | No | Output paths (comma-separated) |

#### `reclass`

| Option | Required | Description |
|--------|----------|-------------|
| `--input <path>` | Yes | Input single-band raster |
| `--rules <path>` | No* | Reclassify rule JSON file |
| `--remap <k1:v1,...>` | No* | Inline remap rules (e.g., `1:10,2:10,3:20`) |
| `--set-nodata <v1,v2,...>` | No* | Values to mark as NoData |
| `--nodata-value <v>` | No | Output NoData value (default: `-9999`) |
| `-o <path>` | Yes | Output raster path |

> *Use `--rules` or `--remap`/`--set-nodata`. `--rules` takes priority.

#### `detect-change`

| Option | Required | Description |
|--------|----------|-------------|
| `--before <path>` | Yes | Before-period raster |
| `--after <path>` | Yes | After-period raster |
| `--encode` | No | Output encoded change map (`before*1000+after`) |
| `-o <path>` | Yes | Output change raster path |

#### `compress`

| Option | Required | Description |
|--------|----------|-------------|
| `--input <path>` | Yes | Input raster |
| `--method <deflate\|lzw\|zstd\|lerc\|lerc_zstd>` | No | Compress algorithm (default: `deflate`) |
| `--level <1-9>` | No | Compression level (default: `6`) |
| `--predictor <0\|2\|3>` | No | Predictor for float data (default: `2`) |
| `--max-error <v>` | No | LERC max error (default: `0.001`) |
| `--tiled` | No | Enable tiled storage |
| `--block-size <n>` | No | Tile block size in pixels (default: `256`) |
| `--bigtiff <yes\|no\|if_needed>` | No | BigTIFF mode (default: `if_needed`) |
| `--overview` | No | Generate internal overviews |
| `-o <path>` | Yes | Output compressed raster path |

#### `version`

Show version information. Supports three invocation methods:

```bash
./geosdg-cli version
./geosdg-cli --version
./geosdg-cli -v
```

## Logging & Checkpoint

Logs are written to a local file at runtime, including:

- **INFO** — General runtime information
- **PROGRESS** — Step progress (e.g., `3/8 Land Proportion Indicator`)
- **RESULT** — Computation results (e.g., `FoM = 0.2345`)
- **CHECKPOINT** — Resume marker for `--resume`
- **WARN / ERROR** — Warnings and errors

When using `demo --resume`, the program reads the last CHECKPOINT from the log and automatically skips completed steps to continue execution.

## Library Integration

geosdg-cli can be integrated into third-party projects as a static or shared library:

```cmake
# CMakeLists.txt example
find_package(geosdg-cli REQUIRED)

target_link_libraries(YourApp PRIVATE geosdg_static)
# or
target_link_libraries(YourApp PRIVATE geosdg_shared)
```

### Core Classes

| Class | Header | Function |
|-------|--------|----------|
| `CalculateCAPrecision` | `CalculateCAPrecision.h` | CA simulation accuracy (FoM, Kappa, OA, etc.), Pearson correlation, t-test |
| `CalculateSDG` | `CalculateSDG.h` | SDG indicator computation (land proportion, conversion, buffer, 11.3.1, 13.2.2) |
| `ExtractPriorityAreas` | `ExtractPriorityAreas.h` | Priority area identification (6 rules + ranking merge) |
| `RasterPreprocessor` | `RasterPreprocessor.h` | Raster preprocessing (resample, normalize, reclassify, change detection, compress) |
| `Logger` | `Logger.h` | Logging (singleton, file output, checkpoint resume) |

## Data Directory Convention

Demo mode reads data from the following relative paths:

```
../data/
├── LUCC/           # Land use / cover data (GeoTIFF)
│   ├── 2010.tif
│   ├── 2020.tif
│   ├── 2025.tif
│   ├── 2050.tif
│   └── Simulation.tif
├── POPU/           # Population data (GeoTIFF)
│   ├── 2025.tif
│   └── 2050.tif
└── INFRA/          # Infrastructure data
    └── roads.tif
```

Output files are written to the `../data/` directory (e.g., `PriorityAreas-*.tif`, `PriorityAreasRankingMap.tif`).

## Feature Roadmap

> Status is synced with requirements documents under `wiki/features/` and current code implementation.

### ✅ Completed

| Feature | Date | Description |
|---------|------|-------------|
| macOS Platform Support | 2026-07-21 | CMake platform detection, adaptive GDAL integration, GDAL_DATA runtime injection |
| Skill Update: UI → CLI Migration | 2026-07-21 | CLI sub-command system aligned with UI version's full computation capabilities |
| Skill Context Optimization | 2026-07-22 | Layered loading and on-demand reads to reduce Agent context usage |
| CA Simulation CLI Module | 2026-07-23 | `ca-pg` (Pg estimation), `ca-markov` (Markov demand prediction), `ca-simulate` (CA iterative simulation) |
| Data Preprocessing CLI Module | 2026-07-23 | 5 sub-commands: `resample`, `normalize`, `reclass`, `detect-change`, `compress` |
| GeoTIFF Quality Check CLI Module | 2026-07-23 | `check` (metadata inspection, ref comparison, type coverage, category count, integer validation) |
| README Format Fix | 2026-07-23 | Fixed Feature Roadmap table separator `! \|` formatting bug |

### 🔄 In Progress

(None)

### 📋 Planned

| Feature | Priority | Description |
|---------|----------|-------------|
| Agent Core Architecture | — | Make the Agent "think": tool layer + Agent dispatch capabilities |
| Agent Hierarchical Memory | — | Make the Agent "remember, find, and use" |
| Auto-Assessment & Reflection | — | Agent meta-capability: result validation and self-correction |
| External Data Connector Layer | — | Agent-layer external data access |
| Project Engineering Improvements | — | Cross-project engineering infrastructure |
| ML Model Integration Framework | — | ML-driven land use simulation and SDG prediction |
| Compute Pipeline Orchestration | — | Training/inference pipeline orchestration |
| RAG Knowledge Base & Vector Memory | — | Knowledge and memory layer modernization |
| SDG Comprehensive Assessment Reports | — | Report generation + visualization toolchain |

## License

See the license file in the project root directory.
