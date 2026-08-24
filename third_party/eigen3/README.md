# Eigen3

> Header-only linear algebra library, extracted from upstream for geosdg-cli integration.

## Version

Eigen 3.5.0.1-dev (master, 2026-07-23)

## Usage (CMake)

```cmake
# geosdg-cli/CMakeLists.txt
set(EIGEN3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../eigen3")
target_include_directories(geosdg_static PUBLIC ${EIGEN3_INCLUDE_DIR})
```

## Usage (manual)

```cpp
#include <Eigen/Dense>
#include <Eigen/Sparse>
```

```bash
# compile
clang++ -std=c++17 -I eigen3 your_code.cpp
```

## License

MPL-2.0 (see COPYING.MPL2)
