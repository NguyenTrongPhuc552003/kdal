# KDALConfig.cmake
# CMake package configuration file for KDAL.
#
# Usage in a downstream project:
#
#   find_package(KDAL REQUIRED)
#   target_link_libraries(myapp KDAL::kdality)
#
# This file is installed by `cmake --install` to:
#   ${CMAKE_INSTALL_PREFIX}/lib/cmake/KDAL/KDALConfig.cmake
#
# It exposes the following CMake targets:
#   KDAL::libkdal      — KDAL runtime static library (libkdal.a)
#   KDAL::kdality      — kdality unified toolchain binary
#   KDAL::kdalc        — standalone KDAL compiler binary
#
# And the following variables:
#   KDAL_VERSION          — e.g. "0.1.0"
#   KDAL_INCLUDE_DIRS     — public header directories
#   KDAL_LIBRARIES        — link targets (equal to KDAL::libkdal)
#   KDAL_COMPILER         — path to kdalc binary
#   KDAL_TOOL             — path to kdality binary
#   KDAL_STDLIB_DIR       — path to lang/stdlib/ .kdh files
#   KDAL_FOUND            — TRUE when package is found

cmake_minimum_required(VERSION 3.16)

# Guard against multiple inclusion
if(KDAL_CONFIG_INCLUDED)
    return()
endif()
set(KDAL_CONFIG_INCLUDED TRUE)

# ── Package root ──────────────────────────────────────────────────────
# When installed, this file sits at <prefix>/lib/cmake/KDAL/KDALConfig.cmake
# Compute the install prefix relative to this file.
get_filename_component(_KDAL_SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(KDAL_ROOT "${_KDAL_SELF_DIR}/../../.." ABSOLUTE)

# ── Version ───────────────────────────────────────────────────────────
include("${_KDAL_SELF_DIR}/KDALConfigVersion.cmake" OPTIONAL)

# ── Imported targets (from build or install) ──────────────────────────
if(EXISTS "${_KDAL_SELF_DIR}/KDALTargets.cmake")
    include("${_KDAL_SELF_DIR}/KDALTargets.cmake")
endif()

# ── Helper modules ───────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${_KDAL_SELF_DIR}")

# ── Exported variables ────────────────────────────────────────────────

# Public headers
set(KDAL_INCLUDE_DIRS
    "${KDAL_ROOT}/include"
    CACHE PATH "KDAL include directories")

# Runtime library
if(TARGET KDAL::libkdal)
    set(KDAL_LIBRARIES KDAL::libkdal)
else()
    find_library(KDAL_LIBRARY
        NAMES kdal
        PATHS "${KDAL_ROOT}/lib" "${KDAL_ROOT}/build"
        NO_DEFAULT_PATH)
    if(KDAL_LIBRARY)
        add_library(KDAL::libkdal STATIC IMPORTED)
        set_target_properties(KDAL::libkdal PROPERTIES
            IMPORTED_LOCATION "${KDAL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${KDAL_INCLUDE_DIRS}")
        set(KDAL_LIBRARIES KDAL::libkdal)
    endif()
endif()

# Compiler binary
find_program(KDAL_COMPILER
    NAMES kdalc
    PATHS
        "${KDAL_ROOT}/bin"
        "${KDAL_ROOT}/compiler"
    NO_DEFAULT_PATH)

# Unified tool binary
find_program(KDAL_TOOL
    NAMES kdality
    PATHS
        "${KDAL_ROOT}/bin"
        "${KDAL_ROOT}/tools/kdality"
    NO_DEFAULT_PATH)

# Standard library .kdh path
set(KDAL_STDLIB_DIR "${KDAL_ROOT}/lang/stdlib"
    CACHE PATH "KDAL standard library .kdh directory")

# ── CompileKDC helper function ─────────────────────────────────────────
# Included only if the compiler was found.
if(KDAL_COMPILER)
    include("${_KDAL_SELF_DIR}/CompileKDC.cmake" OPTIONAL)
endif()

# ── KDAL_FOUND ────────────────────────────────────────────────────────
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KDAL
    REQUIRED_VARS KDAL_INCLUDE_DIRS KDAL_LIBRARIES
    VERSION_VAR   KDAL_VERSION)

mark_as_advanced(KDAL_LIBRARY KDAL_COMPILER KDAL_TOOL)
