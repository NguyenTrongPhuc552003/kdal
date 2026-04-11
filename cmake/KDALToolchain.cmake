# KDALToolchain.cmake
# CMake toolchain file for cross-compiling KDAL drivers and tools
# targeting aarch64 Linux (QEMU virt / bare-metal ARM boards).
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/KDALToolchain.cmake \
#         -DKDAL_CROSS_PREFIX=aarch64-linux-gnu- \
#         ..
#
# Or, to configure just the target arch without fully replacing the
# host toolchain, include this file from your own CMakeLists.txt:
#   include(cmake/KDALToolchain.cmake)
#
# Variables set by this file:
#   CMAKE_SYSTEM_NAME           Linux
#   CMAKE_SYSTEM_PROCESSOR      aarch64
#   CMAKE_C_COMPILER            <cross-prefix>gcc
#   CMAKE_AR                    <cross-prefix>ar
#   CMAKE_STRIP                 <cross-prefix>strip
#   CMAKE_OBJCOPY               <cross-prefix>objcopy
#   CMAKE_FIND_ROOT_PATH        <sysroot> (if KDAL_SYSROOT is set)
#   KDAL_CROSS_COMPILE          cross compiler prefix string
#   KDAL_ARCH                   "arm64"   (kernel ARCH= value)
#   KDAL_KERNEL_DIR             kernel build dir (if auto-detected)

cmake_minimum_required(VERSION 3.16)

# ── User-configurable options ─────────────────────────────────────────

# Cross-compiler prefix (default: aarch64-linux-gnu-)
if(NOT KDAL_CROSS_PREFIX)
    set(KDAL_CROSS_PREFIX "aarch64-linux-gnu-"
        CACHE STRING "Cross-compiler prefix (e.g. aarch64-linux-gnu-)")
endif()

# Optional sysroot
if(NOT KDAL_SYSROOT)
    set(KDAL_SYSROOT ""
        CACHE PATH "Cross-compilation sysroot (optional)")
endif()

# Kernel build directory for module builds
if(NOT KDAL_KERNEL_DIR)
    # Try to auto-detect from common locations
    if(EXISTS "/lib/modules/6.6.0-arm64/build")
        set(KDAL_KERNEL_DIR "/lib/modules/6.6.0-arm64/build")
    elseif(EXISTS "$ENV{KDAL_KERNEL_DIR}")
        set(KDAL_KERNEL_DIR "$ENV{KDAL_KERNEL_DIR}")
    endif()
    set(KDAL_KERNEL_DIR "${KDAL_KERNEL_DIR}"
        CACHE PATH "Kernel build directory for module compilation")
endif()

# ── Toolchain configuration ───────────────────────────────────────────

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Compiler
set(CMAKE_C_COMPILER   "${KDAL_CROSS_PREFIX}gcc"   CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "${KDAL_CROSS_PREFIX}g++"   CACHE FILEPATH "C++ compiler")
set(CMAKE_AR           "${KDAL_CROSS_PREFIX}ar"     CACHE FILEPATH "Archiver")
set(CMAKE_STRIP        "${KDAL_CROSS_PREFIX}strip"  CACHE FILEPATH "Strip utility")
set(CMAKE_OBJCOPY      "${KDAL_CROSS_PREFIX}objcopy" CACHE FILEPATH "Objcopy")
set(CMAKE_LINKER       "${KDAL_CROSS_PREFIX}ld"     CACHE FILEPATH "Linker")

# CROSS_COMPILE string (without trailing dash the variable is used directly)
set(KDAL_CROSS_COMPILE "${KDAL_CROSS_PREFIX}" CACHE STRING "CROSS_COMPILE prefix")

# Kernel ARCH value
set(KDAL_ARCH "arm64" CACHE STRING "Kernel ARCH= value")

# ── Sysroot configuration ─────────────────────────────────────────────

if(KDAL_SYSROOT)
    set(CMAKE_SYSROOT "${KDAL_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${KDAL_SYSROOT}")
    # Don't search host paths for libraries and headers
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

# ── Compiler flags ────────────────────────────────────────────────────
# -fno-PIE: kernel modules must not be position-independent executables
# -march=armv8-a: baseline ARMv8 (AArch64)

set(KDAL_KERNEL_CFLAGS
    "-march=armv8-a -fno-PIE -fno-pie -fno-stack-protector"
    CACHE STRING "Extra CFLAGS for kernel module compilation")

# ── Summary message ───────────────────────────────────────────────────

message(STATUS "KDAL toolchain: ${KDAL_CROSS_PREFIX}gcc (aarch64)")
if(KDAL_KERNEL_DIR)
    message(STATUS "KDAL kernel dir: ${KDAL_KERNEL_DIR}")
else()
    message(STATUS "KDAL kernel dir: not set (set KDAL_KERNEL_DIR)")
endif()
