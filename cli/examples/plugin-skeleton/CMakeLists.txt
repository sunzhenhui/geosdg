# ============================================================================
# CMakeLists.txt — Example Plugin Skeleton
# ============================================================================
#
# This CMake file builds the example plugin as a shared library.
# The resulting library should be placed in the same directory as plugin.json.
#
# Usage:
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/gdal
#   cmake --build build
#   cp build/libsdg_custom_forest.* .
#
# Then copy the entire directory to the geosdg-cli plugins folder.

cmake_minimum_required(VERSION 3.16)

project(sdg_custom_forest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# Find GDAL
# ----------------------------------------------------------------------------
find_package(GDAL REQUIRED)

# ----------------------------------------------------------------------------
# Include directories
# ----------------------------------------------------------------------------
# Point this to the geosdg-cli/include directory where IIndicator.h lives
set(SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
    CACHE PATH "Path to GeoSDG plugin SDK headers")

include_directories(
    ${SDK_INCLUDE_DIR}
    ${GDAL_INCLUDE_DIRS}
)

# ----------------------------------------------------------------------------
# Build shared library
# ----------------------------------------------------------------------------
add_library(sdg_custom_forest SHARED
    MyIndicator.cpp
)

target_link_libraries(sdg_custom_forest
    ${GDAL_LIBRARIES}
)

# ----------------------------------------------------------------------------
# Set output directory to the plugin directory
# ----------------------------------------------------------------------------
set_target_properties(sdg_custom_forest PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# On macOS, allow undefined symbols (the host provides IIndicator vtable)
if(APPLE)
    set_target_properties(sdg_custom_forest PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif()
