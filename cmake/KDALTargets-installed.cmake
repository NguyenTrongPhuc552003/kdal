# KDALTargets-installed.cmake
# Defines exported CMake targets for an *installed* KDAL package.
#
# This file is placed in <prefix>/lib/cmake/KDAL/ by `make install`
# or `cmake --install`. It finds binaries and libraries relative to
# the install prefix.
#
# Targets defined:
#   KDAL::libkdalc  - KDAL compiler library (static)
#   KDAL::kdality   - unified toolchain binary (imported executable)
#   KDAL::kdalc     - standalone compiler binary (imported executable)
#   KDAL::compiler  - alias for KDAL::kdalc

cmake_minimum_required(VERSION 3.16)

# Guard against re-inclusion
if(TARGET KDAL::kdalc)
    return()
endif()

# ── Compute install prefix relative to this file ──────────────────────
# This file sits at: <prefix>/lib/cmake/KDAL/KDALTargets-installed.cmake
get_filename_component(_KDAL_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_KDAL_PREFIX "${_KDAL_CMAKE_DIR}/../../.." ABSOLUTE)

# ── KDAL compiler library (libkdalc.a) ────────────────────────────────
add_library(KDAL::libkdalc STATIC IMPORTED)
set_target_properties(KDAL::libkdalc PROPERTIES
    IMPORTED_LOCATION
        "${_KDAL_PREFIX}/lib/libkdalc.a"
    INTERFACE_INCLUDE_DIRECTORIES
        "${_KDAL_PREFIX}/include/kdalc"
)

# ── kdality binary ────────────────────────────────────────────────────
add_executable(KDAL::kdality IMPORTED)
set_target_properties(KDAL::kdality PROPERTIES
    IMPORTED_LOCATION "${_KDAL_PREFIX}/bin/kdality"
)

# ── kdalc binary ──────────────────────────────────────────────────────
add_executable(KDAL::kdalc IMPORTED)
set_target_properties(KDAL::kdalc PROPERTIES
    IMPORTED_LOCATION "${_KDAL_PREFIX}/bin/kdalc"
)

# ── Convenience alias ─────────────────────────────────────────────────
# Cannot ALIAS imported targets, so just document that KDAL::kdalc is
# the canonical compiler target.

unset(_KDAL_CMAKE_DIR)
unset(_KDAL_PREFIX)
