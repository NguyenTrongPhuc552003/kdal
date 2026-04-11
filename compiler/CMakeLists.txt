# SPDX-License-Identifier: GPL-2.0-only
#
# KDAL compiler (kdalc) — CMake build
#
# Mirrors compiler/Makefile for CMake-based builds.
# Produces:
#   kdalc_lib  — static library (lexer + parser + sema + codegen)
#   kdalc      — standalone compiler binary

cmake_minimum_required(VERSION 3.20)

# Read version from the repo-root VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" _KDAL_VER)
string(STRIP "${_KDAL_VER}" _KDAL_VER)

# ── Static library: libkdalc ─────────────────────────────────────────

set(KDALC_LIB_SOURCES
    lexer.c
    parser.c
    sema.c
    codegen.c
)

add_library(kdalc_lib STATIC ${KDALC_LIB_SOURCES})

target_include_directories(kdalc_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(kdalc_lib PRIVATE
    KDAL_VERSION_STRING="${_KDAL_VER}"
)

target_compile_options(kdalc_lib PRIVATE
    -Wall -Wextra -Wpedantic
)

set_target_properties(kdalc_lib PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    OUTPUT_NAME kdalc
)

# ── Standalone compiler binary ───────────────────────────────────────

add_executable(kdalc main.c)

target_link_libraries(kdalc PRIVATE kdalc_lib)

target_compile_definitions(kdalc PRIVATE
    KDAL_VERSION_STRING="${_KDAL_VER}"
)

target_compile_options(kdalc PRIVATE
    -Wall -Wextra -Wpedantic
)

set_target_properties(kdalc PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
