# CompileKDC.cmake
# Provides add_kdc_driver() — compile a .kdc file into a kernel module.
#
# Usage:
#
#   include(CompileKDC)
#
#   add_kdc_driver(my_uart
#       SOURCE      uart_hello.kdc
#       KERNEL_DIR  /lib/modules/6.6.0/build
#       OUTPUT_DIR  ${CMAKE_CURRENT_BINARY_DIR}/kdc_out
#       CROSS       aarch64-linux-gnu-
#       VERBOSE
#   )
#
# After cmake --build, the following files are created in OUTPUT_DIR:
#   my_uart.c            — generated kernel C source
#   Makefile.kbuild      — obj-m fragment
#
# A custom target named `my_uart_kmod` is also added, which runs
# `make -f Makefile.kbuild KDIR=<KERNEL_DIR>` to produce my_uart.ko.
#
# Variables respected:
#   KDAL_COMPILER        — path to kdalc (set by KDALConfig.cmake)
#   KDAL_STDLIB_DIR      — path to .kdh stdlib (set by KDALConfig.cmake)
#   KDAL_KERNEL_DIR      — default KERNEL_DIR if not specified per-call
#   KDAL_CROSS_COMPILE   — default CROSS_COMPILE if not specified per-call

cmake_minimum_required(VERSION 3.16)

# Resolve the compiler binary
if(NOT KDAL_COMPILER)
    find_program(KDAL_COMPILER NAMES kdalc kdality
        PATHS
            "${CMAKE_SOURCE_DIR}/compiler"
            "${CMAKE_SOURCE_DIR}/tools/kdality"
        NO_DEFAULT_PATH)
endif()

if(NOT KDAL_COMPILER)
    message(WARNING "CompileKDC: kdalc not found — add_kdc_driver() will be a no-op")
endif()

# ── add_kdc_driver ────────────────────────────────────────────────────
function(add_kdc_driver TARGET_NAME)
    if(NOT KDAL_COMPILER)
        message(WARNING "add_kdc_driver(${TARGET_NAME}): compiler not found, skipping")
        return()
    endif()

    cmake_parse_arguments(KDC
        "VERBOSE"                                          # options (flags)
        "SOURCE;KERNEL_DIR;OUTPUT_DIR;CROSS"               # one-value keys
        ""                                                 # multi-value keys
        ${ARGN})

    # Validate SOURCE
    if(NOT KDC_SOURCE)
        message(FATAL_ERROR "add_kdc_driver(${TARGET_NAME}): SOURCE is required")
    endif()

    # Resolve absolute source path
    if(NOT IS_ABSOLUTE "${KDC_SOURCE}")
        set(KDC_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${KDC_SOURCE}")
    endif()

    if(NOT EXISTS "${KDC_SOURCE}")
        message(FATAL_ERROR "add_kdc_driver(${TARGET_NAME}): SOURCE '${KDC_SOURCE}' not found")
    endif()

    # Output directory
    if(NOT KDC_OUTPUT_DIR)
        set(KDC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/kdc_${TARGET_NAME}")
    endif()
    file(MAKE_DIRECTORY "${KDC_OUTPUT_DIR}")

    # Deduced output file names
    get_filename_component(_KDC_STEM "${KDC_SOURCE}" NAME_WE)
    set(_KDC_C_OUT   "${KDC_OUTPUT_DIR}/${_KDC_STEM}.c")
    set(_KDC_MK_OUT  "${KDC_OUTPUT_DIR}/Makefile.kbuild")

    # Kernel directory
    if(NOT KDC_KERNEL_DIR AND KDAL_KERNEL_DIR)
        set(KDC_KERNEL_DIR "${KDAL_KERNEL_DIR}")
    endif()

    # Cross compile prefix
    if(NOT KDC_CROSS AND KDAL_CROSS_COMPILE)
        set(KDC_CROSS "${KDAL_CROSS_COMPILE}")
    endif()

    # Build compiler command line
    set(_KDC_CMD "${KDAL_COMPILER}")

    # If kdality is used as the compiler, prefix with "compile" subcommand
    get_filename_component(_KDC_COMPNAME "${KDAL_COMPILER}" NAME)
    if(_KDC_COMPNAME STREQUAL "kdality")
        list(APPEND _KDC_CMD "compile")
    endif()

    list(APPEND _KDC_CMD "-o" "${KDC_OUTPUT_DIR}")
    if(KDC_KERNEL_DIR)
        list(APPEND _KDC_CMD "-K" "${KDC_KERNEL_DIR}")
    endif()
    if(KDC_CROSS)
        list(APPEND _KDC_CMD "-x" "${KDC_CROSS}")
    endif()
    if(KDC_VERBOSE)
        list(APPEND _KDC_CMD "-v")
    endif()
    list(APPEND _KDC_CMD "${KDC_SOURCE}")

    # Custom command: runs kdalc when the .kdc file changes
    add_custom_command(
        OUTPUT  "${_KDC_C_OUT}" "${_KDC_MK_OUT}"
        COMMAND ${_KDC_CMD}
        DEPENDS "${KDC_SOURCE}"
        COMMENT "Compiling KDAL driver: ${KDC_SOURCE}"
        VERBATIM)

    # Custom target: synthesises the .c output
    add_custom_target("${TARGET_NAME}_kdc_compile" ALL
        DEPENDS "${_KDC_C_OUT}" "${_KDC_MK_OUT}")

    # Optional kmod build target (requires KERNEL_DIR)
    if(KDC_KERNEL_DIR)
        set(_KDC_KMOD_TARGET "${TARGET_NAME}_kmod")
        set(_MAKE_CMD make -f "${_KDC_MK_OUT}"
                     KDIR="${KDC_KERNEL_DIR}")
        if(KDC_CROSS)
            list(APPEND _MAKE_CMD CROSS_COMPILE="${KDC_CROSS}")
        endif()

        add_custom_target("${_KDC_KMOD_TARGET}"
            COMMAND           ${_MAKE_CMD}
            WORKING_DIRECTORY "${KDC_OUTPUT_DIR}"
            DEPENDS           "${TARGET_NAME}_kdc_compile"
            COMMENT           "Building kernel module: ${_KDC_STEM}.ko"
            VERBATIM)
    endif()

    # Export output path to parent scope
    set("${TARGET_NAME}_OUTPUT_DIR" "${KDC_OUTPUT_DIR}" PARENT_SCOPE)
    set("${TARGET_NAME}_C_SOURCE"   "${_KDC_C_OUT}"     PARENT_SCOPE)
endfunction()
