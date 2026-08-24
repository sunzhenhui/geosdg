#!/usr/bin/env bash
# ============================================================================
# GeoSDG Environment Initialization Script
# ============================================================================
# Usage: source scripts/env-init.sh
#
# This script checks and configures the development environment for GeoSDG.
# It verifies required dependencies (CMake, C++17 compiler, GDAL) and sets
# up environment variables needed for building and running the CLI.
# ============================================================================

set -euo pipefail

# ── Colors ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }

# ── Project root detection ─────────────────────────────────────────────────
# Support both bash (BASH_SOURCE) and zsh (%x/$_)
if [[ -n "${BASH_SOURCE+x}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
elif [[ -n "${(%):-%x}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${(%):-%x}")" && pwd)"
else
    # Fallback: assume we're in the project root or scripts/ dir
    if [[ -f "scripts/env-init.sh" ]]; then
        SCRIPT_DIR="$(pwd)/scripts"
    elif [[ -f "../scripts/env-init.sh" ]]; then
        SCRIPT_DIR="$(cd scripts 2>/dev/null && pwd) || $(pwd)"
    else
        SCRIPT_DIR="$(pwd)"
    fi
fi
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

info "GeoSDG project root: ${PROJECT_ROOT}"

# ── Check CMake ─────────────────────────────────────────────────────────────
check_cmake() {
    if command -v cmake &>/dev/null; then
        local version
        version=$(cmake --version 2>/dev/null | head -1 | awk '{print $NF}')
        info "CMake found: ${version}"
        return 0
    else
        error "CMake not found. Install with:"
        error "  macOS:   brew install cmake"
        error "  Ubuntu:  sudo apt install cmake"
        error "  Windows: choco install cmake"
        return 1
    fi
}

# ── Check C++17 compiler ───────────────────────────────────────────────────
check_compiler() {
    local found=0
    if command -v clang++ &>/dev/null; then
        local version
        version=$(clang++ --version 2>/dev/null | head -1)
        info "Clang found: ${version}"
        found=1
    elif command -v g++ &>/dev/null; then
        local version
        version=$(g++ --version 2>/dev/null | head -1)
        info "GCC found: ${version}"
        found=1
    fi

    if [[ ${found} -eq 0 ]]; then
        error "No C++17-compatible compiler found. Install clang or gcc."
        return 1
    fi
    return 0
}

# ── Check GDAL ──────────────────────────────────────────────────────────────
check_gdal() {
    if [[ "$(uname -s)" == "Darwin" ]] || [[ "$(uname -s)" == "Linux" ]]; then
        if command -v gdal-config &>/dev/null; then
            local version
            version=$(gdal-config --version 2>/dev/null)
            info "GDAL found: ${version}"

            local gdal_data
            gdal_data=$(gdal-config --datadir 2>/dev/null || echo "")
            if [[ -n "${gdal_data}" ]]; then
                export GEOSDG_GDAL_DATA="${gdal_data}"
                info "GDAL_DATA: ${gdal_data}"
            fi
            return 0
        else
            error "GDAL not found. Install with:"
            error "  macOS:  brew install gdal"
            error "  Ubuntu: sudo apt install libgdal-dev"
            return 1
        fi
    else
        # Windows: check bundled GDAL
        local gdal_root="${PROJECT_ROOT}/third_party/gdal"
        if [[ -d "${gdal_root}" ]]; then
            info "Windows bundled GDAL found at: ${gdal_root}"
            return 0
        else
            error "Windows bundled GDAL not found at: ${gdal_root}"
            return 1
        fi
    fi
}

# ── Check Python (optional, for agent scripts) ─────────────────────────────
check_python() {
    if command -v python3 &>/dev/null; then
        local version
        version=$(python3 --version 2>/dev/null)
        info "Python3 found: ${version}"
    else
        warn "Python3 not found. Agent scripts will not work."
    fi
}

# ── Set environment variables ───────────────────────────────────────────────
setup_env() {
    export GEOSDG_ROOT="${PROJECT_ROOT}"
    export GEOSDG_CLI="${PROJECT_ROOT}/cli"
    export GEOSDG_DATA="${PROJECT_ROOT}/data"
    export GEOSDG_AGENT="${PROJECT_ROOT}/agent"

    # Add CLI build output to PATH if it exists
    local build_bin="${PROJECT_ROOT}/cli/build/bin"
    if [[ -d "${build_bin}" ]]; then
        export PATH="${build_bin}:${PATH}"
        info "Added ${build_bin} to PATH"
    fi

    info "Environment variables set:"
    info "  GEOSDG_ROOT  = ${GEOSDG_ROOT}"
    info "  GEOSDG_CLI   = ${GEOSDG_CLI}"
    info "  GEOSDG_DATA  = ${GEOSDG_DATA}"
    info "  GEOSDG_AGENT = ${GEOSDG_AGENT}"
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo "========================================="
    echo "  GeoSDG Environment Initialization"
    echo "========================================="
    echo ""

    local failed=0

    check_cmake   || failed=1
    check_compiler || failed=1
    check_gdal    || failed=1
    check_python

    setup_env

    echo ""
    if [[ ${failed} -eq 0 ]]; then
        info "All required dependencies are satisfied."
    else
        error "Some dependencies are missing. Please install them before building."
        return 1
    fi
    echo ""
}

main
