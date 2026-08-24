# ALGLIB

> Numerical analysis library (RF, ANN, Markov chain, optimization, etc.) for geosdg-cli.

## Version

ALGLIB 4.08.0 (source code generated 2026-06-08)

## Directory Structure

```
alglib/
├── src/            ← 20 .h + 19 .cpp (compile into project)
│   ├── ap.h/cpp                ← core types (real_2d_array, etc.)
│   ├── dataanalysis.h/cpp     ← RF, ANN, clustering
│   ├── alglibmisc.h/cpp       ← Markov chain (mcpd*)
│   ├── optimization.h/cpp     ← optimization solvers
│   ├── linalg.h/cpp           ← linear algebra
│   ├── statistics.h/cpp       ← statistical functions
│   ├── kernels_sse2.h/cpp     ← SIMD kernel (SSE2)
│   ├── kernels_avx2.h/cpp     ← SIMD kernel (AVX2)
│   ├── kernels_fma.h/cpp      ← SIMD kernel (FMA)
│   ├── kernels_neon.h/cpp     ← SIMD kernel (ARM NEON)
│   ├── kernels_rvv10.h/cpp    ← SIMD kernel (RISC-V V)
│   ├── minlp.h/cpp            ← mixed-integer LP
│   └── ...
├── gpl2.txt
├── gpl3.txt
└── manual.cpp.html
```

## Usage (CMake)

```cmake
# geosdg-cli/CMakeLists.txt
set(ALGLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../alglib/src")
set(ALGLIB_SOURCES
    ${CMAKE_SOURCE_DIR}/../alglib/src/ap.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/dataanalysis.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/alglibmisc.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/optimization.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/linalg.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/statistics.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/specialfunctions.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/solvers.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/interpolation.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/integration.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/diffequations.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/fasttransforms.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/alglibinternal.cpp
    ${CMAKE_SOURCE_DIR}/../alglib/src/minlp.cpp
    # SIMD kernels (select based on target platform)
    ${CMAKE_SOURCE_DIR}/../alglib/src/kernels_sse2.cpp
    # ${CMAKE_SOURCE_DIR}/../alglib/src/kernels_avx2.cpp
    # ${CMAKE_SOURCE_DIR}/../alglib/src/kernels_fma.cpp
    # ${CMAKE_SOURCE_DIR}/../alglib/src/kernels_neon.cpp
)
target_include_directories(geosdg_static PUBLIC ${ALGLIB_INCLUDE_DIR})
```

## Usage (manual)

```cpp
#include "dataanalysis.h"   // RF, ANN
#include "alglibmisc.h"     // Markov chain (mcpd*)
```

## License

GPL-2.0+ (see gpl2.txt, gpl3.txt)
