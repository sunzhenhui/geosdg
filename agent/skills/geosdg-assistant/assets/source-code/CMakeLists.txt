cmake_minimum_required(VERSION 3.10)
project(geosdg-cli VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Default build type ──────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
message(STATUS "BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# ── Release-specific flags ──────────────────────────────────────────────
# MSVC: /O2 /Ob2 /DNDEBUG are already set by default in CMAKE_CXX_FLAGS_RELEASE
# Add Whole Program Optimization + Link-Time Code Generation
add_compile_options(
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/GL>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3>
)
add_link_options(
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/LTCG>
)

# ── Build options ────────────────────────────────────────────────────────
option(BUILD_SHARED_LIBS "Build shared library instead of static" OFF)
option(BUILD_DEMO "Build demo executable" ON)

# ── Platform detection macro ────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(GEOSDG_PLATFORM_WINDOWS=1)
elseif(APPLE)
    add_compile_definitions(GEOSDG_PLATFORM_MACOS=1)
else()
    add_compile_definitions(GEOSDG_PLATFORM_LINUX=1)
endif()

# ── GDAL ──────────────────────────────────────────────────────────────────
if(WIN32)
    # Windows: use bundled gdal2.0.2
    set(GDAL_ROOT "${CMAKE_SOURCE_DIR}/../gdal2.0.2")
    set(GDAL_INCLUDE_DIR "${GDAL_ROOT}/include")
    set(GDAL_LIBRARY     "${GDAL_ROOT}/lib/gdal_i.lib")
    set(GDAL_DATA_DIR    "${GDAL_ROOT}/data")
else()
    # macOS / Linux: use system GDAL
    find_package(GDAL REQUIRED)
    set(GDAL_INCLUDE_DIR ${GDAL_INCLUDE_DIRS})
    set(GDAL_LIBRARY     ${GDAL_LIBRARIES})
    # Detect GDAL_DATA path via gdal-config
    execute_process(
        COMMAND gdal-config --datadir
        OUTPUT_VARIABLE GDAL_DATA_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GDAL_DATA_DIR)
        set(GDAL_DATA_DIR "$ENV{GDAL_DATA}")
    endif()
endif()

message(STATUS "GDAL_INCLUDE_DIR= ${GDAL_INCLUDE_DIR}")
message(STATUS "GDAL_LIBRARY    = ${GDAL_LIBRARY}")
message(STATUS "GDAL_DATA_DIR   = ${GDAL_DATA_DIR}")

# ── Library sources (exclude main.cpp) ──────────────────────────────────
set(LIB_SOURCES
    src/CalculateCAPrecision.cpp
    src/CalculateSDG.cpp
    src/ExtractPriorityAreas.cpp
    src/Logger.cpp
)

set(LIB_HEADERS
    src/CalculateCAPrecision.h
    src/CalculateSDG.h
    src/ExtractPriorityAreas.h
    src/Logger.h
)

# ── Static library ──────────────────────────────────────────────────────
add_library(geosdg_static STATIC ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(geosdg_static PUBLIC
    ${GDAL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(geosdg_static PUBLIC ${GDAL_LIBRARY})

set_target_properties(geosdg_static PROPERTIES
    OUTPUT_NAME "geosdg"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ── Shared library ──────────────────────────────────────────────────────
add_library(geosdg_shared SHARED ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(geosdg_shared PUBLIC
    ${GDAL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(geosdg_shared PUBLIC ${GDAL_LIBRARY})

set_target_properties(geosdg_shared PROPERTIES
    OUTPUT_NAME "geosdg"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── Demo executable ─────────────────────────────────────────────────────
if(BUILD_DEMO)
    add_executable(${PROJECT_NAME} src/main.cpp)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GDAL_INCLUDE_DIR}
        src
    )

    # Link against the static library by default; switch to shared if desired
    if(BUILD_SHARED_LIBS)
        target_link_libraries(${PROJECT_NAME} PRIVATE geosdg_shared)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE geosdg_static)
    endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # ── GDAL runtime setup ──────────────────────────────────────────
    if(WIN32)
        # Windows: copy DLLs and data to output directory
        set(GDAL_BIN_DIR "${GDAL_ROOT}/bin")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GDAL_BIN_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GDAL_DATA_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/gdal_data"
            COMMENT "Copying GDAL runtime files..."
        )
        set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "GDAL_DATA=$<TARGET_FILE_DIR:${PROJECT_NAME}>/gdal_data"
        )
    else()
        # macOS / Linux: GDAL data dir baked in as compile definition
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            GEOSDG_GDAL_DATA_DIR="${GDAL_DATA_DIR}"
        )
        message(STATUS "GDAL data baked: ${GDAL_DATA_DIR}")
    endif()
endif()

# ── Install rules ───────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS geosdg_static geosdg_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${LIB_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/geosdg
)

if(BUILD_DEMO)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
