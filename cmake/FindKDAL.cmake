# FindKDAL.cmake
# Locate an installed KDAL package.
#
# This module is used when KDAL is installed system-wide (not via
# add_subdirectory). For in-tree builds, use KDALConfig.cmake instead.
#
# Input variables:
#   KDAL_ROOT              - override search root
#
# Output variables (set on success):
#   KDAL_FOUND             - TRUE when all required components are found
#   KDAL_VERSION           - package version string
#   KDAL_INCLUDE_DIRS      - header search paths
#   KDAL_LIBRARIES         - link libraries
#   KDAL_COMPILER          - path to kdalc binary
#   KDAL_TOOL              - path to kdality binary
#   KDAL_STDLIB_DIR        - path to .kdh standard library files
#
# Imported targets:
#   KDAL::libkdal          - KDAL runtime static library
#   KDAL::kdality          - kdality binary target

cmake_minimum_required(VERSION 3.16)

# ── Search paths ──────────────────────────────────────────────────────

set(_KDAL_SEARCH_PATHS
    "${KDAL_ROOT}"
    "$ENV{KDAL_ROOT}"
    "/usr/local"
    "/usr"
    "/opt/kdal"
)

# ── Find headers ──────────────────────────────────────────────────────

find_path(KDAL_INCLUDE_DIR
    NAMES kdal.h
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES include include/kdal
    NO_DEFAULT_PATH)

if(NOT KDAL_INCLUDE_DIR)
    find_path(KDAL_INCLUDE_DIR NAMES kdal.h)
endif()

# ── Find libraries ────────────────────────────────────────────────────

find_library(KDAL_LIBRARY
    NAMES kdal
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH)

if(NOT KDAL_LIBRARY)
    find_library(KDAL_LIBRARY NAMES kdal)
endif()

find_library(KDAL_COMPILER_LIB
    NAMES kdalc
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH)

# ── Find binaries ─────────────────────────────────────────────────────

find_program(KDAL_COMPILER
    NAMES kdalc
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES bin
    NO_DEFAULT_PATH)

if(NOT KDAL_COMPILER)
    find_program(KDAL_COMPILER NAMES kdalc)
endif()

find_program(KDAL_TOOL
    NAMES kdality
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES bin
    NO_DEFAULT_PATH)

if(NOT KDAL_TOOL)
    find_program(KDAL_TOOL NAMES kdality)
endif()

# ── Standard library directory ────────────────────────────────────────

find_path(KDAL_STDLIB_DIR
    NAMES common.kdh uart.kdh
    PATHS ${_KDAL_SEARCH_PATHS}
    PATH_SUFFIXES share/kdal/stdlib
    NO_DEFAULT_PATH)

# ── Version detection ─────────────────────────────────────────────────

if(KDAL_TOOL)
    execute_process(
        COMMAND "${KDAL_TOOL}" version
        OUTPUT_VARIABLE _KDAL_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" KDAL_VERSION
           "${_KDAL_VERSION_OUTPUT}")
endif()

if(NOT KDAL_VERSION)
    set(KDAL_VERSION "0.1.0")
endif()

# ── Set aggregate variables ───────────────────────────────────────────

set(KDAL_INCLUDE_DIRS "${KDAL_INCLUDE_DIR}" CACHE PATH "KDAL include dirs")
set(KDAL_LIBRARIES    "${KDAL_LIBRARY}"     CACHE STRING "KDAL libraries")

# ── Create imported targets ───────────────────────────────────────────

if(KDAL_LIBRARY AND NOT TARGET KDAL::libkdal)
    add_library(KDAL::libkdal STATIC IMPORTED)
    set_target_properties(KDAL::libkdal PROPERTIES
        IMPORTED_LOCATION           "${KDAL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${KDAL_INCLUDE_DIRS}")
endif()

if(KDAL_TOOL AND NOT TARGET KDAL::kdality)
    add_executable(KDAL::kdality IMPORTED)
    set_target_properties(KDAL::kdality PROPERTIES
        IMPORTED_LOCATION "${KDAL_TOOL}")
endif()

# ── Handle standard find_package arguments ────────────────────────────

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KDAL
    REQUIRED_VARS KDAL_INCLUDE_DIR KDAL_LIBRARY
    VERSION_VAR   KDAL_VERSION
    HANDLE_COMPONENTS)

mark_as_advanced(KDAL_INCLUDE_DIR KDAL_LIBRARY KDAL_COMPILER_LIB
                 KDAL_COMPILER KDAL_TOOL KDAL_STDLIB_DIR)