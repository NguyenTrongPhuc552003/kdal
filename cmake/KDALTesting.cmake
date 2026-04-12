# KDALTesting.cmake
# Testing helpers for KDAL projects.
#
# Provides functions for registering KUnit tests and userspace tests
# with CMake's CTest framework.
#
# Usage:
#
#   include(KDALTesting)
#   enable_testing()
#
#   # Register a KUnit test suite (kernel-space)
#   add_kdal_kunit_test(test_chardev
#       SOURCE  tests/kunit/test_chardev.c
#       MODULE  kdal_test_chardev
#       DEPENDS kdal_core
#   )
#
#   # Register a userspace test binary
#   add_kdal_userspace_test(test_ioctl
#       SOURCE  tests/userspace/test_ioctl.c
#       DEPENDS kdal_core kdal_test_helper
#   )
#
# KUnit tests are run by loading the generated .ko module inside
# the QEMU environment via the qemu_run_tests helper. Userspace
# tests are compiled and run natively (host).

cmake_minimum_required(VERSION 3.16)

# ── Configuration ─────────────────────────────────────────────────────

# QEMU command for running kernel tests (override via -DKDAL_QEMU_CMD=...)
if(NOT KDAL_QEMU_CMD)
    set(KDAL_QEMU_CMD "qemu-system-aarch64"
        CACHE STRING "QEMU binary for KUnit test execution")
endif()

# Timeout for individual KUnit tests (seconds)
if(NOT KDAL_KUNIT_TIMEOUT)
    set(KDAL_KUNIT_TIMEOUT 120
        CACHE STRING "KUnit test timeout in seconds")
endif()

# ── add_kdal_kunit_test ───────────────────────────────────────────────
#
# Registers a KUnit test suite as a kernel module that is compiled and
# loaded inside QEMU. The module name is used as the CTest test name.
#
function(add_kdal_kunit_test TEST_NAME)
    cmake_parse_arguments(KUNIT
        ""                              # flags
        "SOURCE;MODULE;KERNEL_DIR"      # one-value
        "DEPENDS;EXTRA_CFLAGS"          # multi-value
        ${ARGN})

    if(NOT KUNIT_SOURCE)
        message(FATAL_ERROR "add_kdal_kunit_test(${TEST_NAME}): SOURCE required")
    endif()

    if(NOT KUNIT_MODULE)
        set(KUNIT_MODULE "${TEST_NAME}")
    endif()

    if(NOT KUNIT_KERNEL_DIR AND KDAL_KERNEL_DIR)
        set(KUNIT_KERNEL_DIR "${KDAL_KERNEL_DIR}")
    endif()

    # Output directory for the .ko
    set(_KUNIT_OUT "${CMAKE_CURRENT_BINARY_DIR}/kunit_${TEST_NAME}")
    file(MAKE_DIRECTORY "${_KUNIT_OUT}")

    # Generate a minimal Kbuild stub
    set(_KUNIT_KBUILD "${_KUNIT_OUT}/Makefile")
    file(WRITE "${_KUNIT_KBUILD}"
         "obj-m := ${KUNIT_MODULE}.o\n"
         "ccflags-y := -I${CMAKE_SOURCE_DIR}/include ${KUNIT_EXTRA_CFLAGS}\n")

    # Copy the source into the output directory (kbuild needs it there)
    configure_file("${KUNIT_SOURCE}" "${_KUNIT_OUT}/${KUNIT_MODULE}.c" COPYONLY)

    # Custom target to build the module
    if(KUNIT_KERNEL_DIR)
        add_custom_target("kunit_build_${TEST_NAME}"
            COMMAND make -C "${KUNIT_KERNEL_DIR}"
                         M="${_KUNIT_OUT}"
                         modules
            DEPENDS "${KUNIT_SOURCE}" ${KUNIT_DEPENDS}
            COMMENT "Building KUnit module: ${KUNIT_MODULE}.ko"
            VERBATIM)

        # CTest entry: loads the module inside QEMU and checks dmesg
        add_test(
            NAME    "${TEST_NAME}"
            COMMAND "${CMAKE_SOURCE_DIR}/scripts/ci/run_kunit.sh"
                    "${_KUNIT_OUT}/${KUNIT_MODULE}.ko"
                    "${KUNIT_MODULE}")
        set_tests_properties("${TEST_NAME}" PROPERTIES
            TIMEOUT   "${KDAL_KUNIT_TIMEOUT}"
            LABELS    "kunit;kernel")
    else()
        message(STATUS
            "KUnit test '${TEST_NAME}': KERNEL_DIR not set - skipping module build")
    endif()
endfunction()

# ── add_kdal_userspace_test ───────────────────────────────────────────
#
# Compiles and registers a native (host) userspace test executable.
#
function(add_kdal_userspace_test TEST_NAME)
    cmake_parse_arguments(UTEST
        "NO_RUN"                   # flags
        "SOURCE"                   # one-value
        "DEPENDS;DEFINES;ARGS"     # multi-value
        ${ARGN})

    if(NOT UTEST_SOURCE)
        message(FATAL_ERROR "add_kdal_userspace_test(${TEST_NAME}): SOURCE required")
    endif()

    add_executable("${TEST_NAME}" "${UTEST_SOURCE}")

    target_include_directories("${TEST_NAME}" PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/tools/kdality")

    if(UTEST_DEPENDS)
        target_link_libraries("${TEST_NAME}" PRIVATE ${UTEST_DEPENDS})
    endif()

    if(UTEST_DEFINES)
        target_compile_definitions("${TEST_NAME}" PRIVATE ${UTEST_DEFINES})
    endif()

    if(NOT UTEST_NO_RUN)
        add_test(
            NAME    "${TEST_NAME}"
            COMMAND "${TEST_NAME}" ${UTEST_ARGS})
        set_tests_properties("${TEST_NAME}" PROPERTIES
            LABELS "userspace;unit")
    endif()
endfunction()

# ── add_kdal_benchmark ────────────────────────────────────────────────
#
# Registers a benchmark binary (userspace). Benchmarks are not run
# automatically by `ctest`; use `ctest -L benchmark` to run them.
#
function(add_kdal_benchmark BENCH_NAME)
    cmake_parse_arguments(BENCH "" "SOURCE" "DEPENDS;ARGS" ${ARGN})

    if(NOT BENCH_SOURCE)
        message(FATAL_ERROR "add_kdal_benchmark(${BENCH_NAME}): SOURCE required")
    endif()

    add_executable("${BENCH_NAME}" "${BENCH_SOURCE}")
    target_include_directories("${BENCH_NAME}" PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

    if(BENCH_DEPENDS)
        target_link_libraries("${BENCH_NAME}" PRIVATE ${BENCH_DEPENDS})
    endif()

    add_test(
        NAME    "${BENCH_NAME}"
        COMMAND "${BENCH_NAME}" ${BENCH_ARGS})
    set_tests_properties("${BENCH_NAME}" PROPERTIES
        LABELS "benchmark")
endfunction()
