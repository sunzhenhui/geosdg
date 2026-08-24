#!/usr/bin/env bash
# ============================================================================
# GeoSDG Build Script
# ============================================================================
# Usage: scripts/build.sh [options]
#
# Options:
#   -t, --type TYPE     Build type: Release (default), Debug, MinSizeRel, RelWithDebInfo
#   -j, --jobs N        Number of parallel jobs (default: nproc)
#   -s, --shared        Build shared library instead of static
#   --no-demo           Skip building demo executable
#   -c, --clean         Clean build directory before building
#   -h, --help          Show this help message
# ============================================================================

set -euo pipefail

# ── Project root detection ─────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_DIR="${PROJECT_ROOT}/cli"
BUILD_DIR="${CLI_DIR}/build"

# ── Defaults ────────────────────────────────────────────────────────────────
BUILD_TYPE="Release"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
BUILD_SHARED=OFF
BUILD_DEMO=ON
CLEAN=0

# ── Parse arguments ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--type)    BUILD_TYPE="$2"; shift 2 ;;
        -j|--jobs)    JOBS="$2"; shift 2 ;;
        -s|--shared)  BUILD_SHARED=ON; shift ;;
        --no-demo)    BUILD_DEMO=OFF; shift ;;
        -c|--clean)   CLEAN=1; shift ;;
        -h|--help)
            echo "Usage: scripts/build.sh [options]"
            echo ""
            echo "Options:"
            echo "  -t, --type TYPE     Build type (Release|Debug|MinSizeRel|RelWithDebInfo)"
            echo "  -j, --jobs N        Parallel jobs (default: ${JOBS})"
            echo "  -s, --shared        Build shared library"
            echo "  --no-demo           Skip demo executable"
            echo "  -c, --clean         Clean before building"
            echo "  -h, --help          Show help"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Clean if requested ──────────────────────────────────────────────────────
if [[ ${CLEAN} -eq 1 ]] && [[ -d "${BUILD_DIR}" ]]; then
    echo "Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

# ── Configure ───────────────────────────────────────────────────────────────
echo "========================================="
echo "  GeoSDG Build Configuration"
echo "========================================="
echo "  Build type:  ${BUILD_TYPE}"
echo "  Jobs:        ${JOBS}"
echo "  Shared lib:  ${BUILD_SHARED}"
echo "  Demo:        ${BUILD_DEMO}"
echo "  Source dir:  ${CLI_DIR}"
echo "  Build dir:   ${BUILD_DIR}"
echo "========================================="
echo ""

mkdir -p "${BUILD_DIR}"

cmake -S "${CLI_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS="${BUILD_SHARED}" \
    -DBUILD_DEMO="${BUILD_DEMO}"

# ── Build ───────────────────────────────────────────────────────────────────
cmake --build "${BUILD_DIR}" -j"${JOBS}"

# ── Report ──────────────────────────────────────────────────────────────────
echo ""
echo "========================================="
echo "  Build Complete"
echo "========================================="
if [[ -f "${BUILD_DIR}/bin/geosdg-cli" ]]; then
    echo "  Executable: ${BUILD_DIR}/bin/geosdg-cli"
fi
if [[ -f "${BUILD_DIR}/lib/libgeosdg.a" ]]; then
    echo "  Static lib: ${BUILD_DIR}/lib/libgeosdg.a"
fi
if [[ -f "${BUILD_DIR}/bin/libgeosdg.dylib" ]] || [[ -f "${BUILD_DIR}/bin/libgeosdg.so" ]]; then
    echo "  Shared lib: ${BUILD_DIR}/bin/libgeosdg.*"
fi
echo "========================================="
