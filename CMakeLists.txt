cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" _KDAL_VER_RAW)
string(STRIP "${_KDAL_VER_RAW}" _KDAL_VER)
project(kdal VERSION ${_KDAL_VER} LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include KDAL CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(KDALToolchain OPTIONAL)
include(GNUInstallDirs)

# Compiler library (lexer + parser + sema + codegen)
add_subdirectory(compiler)

# kdality - runtime CLI + language toolchain
add_executable(kdality
	tools/kdality/main.c
	tools/kdality/kdalctl.c
	tools/kdality/compile.c
	tools/kdality/init.c
	tools/kdality/dtgen.c
	tools/kdality/simulate.c
	tools/kdality/testgen.c
	tools/kdality/lint.c)
target_compile_options(kdality PRIVATE -Wall -Wextra -pedantic -D_DEFAULT_SOURCE)
target_include_directories(kdality PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/compiler/include)
target_link_libraries(kdality PRIVATE kdalc_lib)

# kdc language examples (optional, requires kdality compile)
option(KDAL_ENABLE_KDC_EXAMPLES "Build .kdc language examples" OFF)
if(KDAL_ENABLE_KDC_EXAMPLES)
  include(CompileKDC)
  add_kdc_driver(uart_hello examples/kdc_hello/uart_hello.kdc)
  add_kdc_driver(i2c_sensor examples/kdc_hello/i2c_sensor.kdc)
endif()

# ── Install rules ─────────────────────────────────────────────────────

# Binaries
install(TARGETS kdality RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Public kernel-space headers
install(DIRECTORY include/kdal
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")

# Compiler headers (for downstream libkdalc users)
install(DIRECTORY compiler/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kdalc
        FILES_MATCHING PATTERN "*.h")

# Standard library .kdh files
install(DIRECTORY lang/stdlib/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/kdal/stdlib
        FILES_MATCHING PATTERN "*.kdh")

# CMake package files
install(FILES
        cmake/KDALConfig.cmake
        cmake/KDALConfigVersion.cmake
        cmake/KDALTargets-installed.cmake
        cmake/KDALToolchain.cmake
        cmake/CompileKDC.cmake
        cmake/FindKDAL.cmake
        cmake/KDALTesting.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/KDAL
        OPTIONAL)

# Vim/Neovim plugin files
install(DIRECTORY editor/vim/vim-kdal/syntax
                  editor/vim/vim-kdal/ftdetect
                  editor/vim/vim-kdal/ftplugin
                  editor/vim/vim-kdal/plugin
                  editor/vim/vim-kdal/compiler
        DESTINATION ${CMAKE_INSTALL_DATADIR}/kdal/vim)

install(FILES editor/vim/vim-kdal/filetype.lua
	DESTINATION ${CMAKE_INSTALL_DATADIR}/kdal/vim)

# ── Uninstall target ─────────────────────────────────────────────────
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")
endif()

# ── Convenience targets ───────────────────────────────────────────────

# Kernel module (kbuild wrapper - requires kernel headers)
set(KDIR "/lib/modules/${CMAKE_HOST_SYSTEM_VERSION}/build" CACHE PATH
    "Path to kernel build directory")
add_custom_target(module
	COMMAND ${CMAKE_MAKE_PROGRAM} -C ${KDIR} M=${CMAKE_SOURCE_DIR} modules
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Building KDAL kernel module via kbuild"
	VERBATIM)

# Website (Astro Starlight documentation site)
find_program(NPM npm)
if(NPM)
  add_custom_target(website
	COMMAND ${NPM} install --no-audit --no-fund
	COMMAND ${NPM} run build
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/website
	COMMENT "Building KDAL documentation website"
	VERBATIM)

  # VS Code extension
  add_custom_target(vscode
	COMMAND ${NPM} install --no-audit --no-fund
	COMMAND ${NPM} exec -- @vscode/vsce package --no-dependencies
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/editor/vscode/kdal-lang
	COMMENT "Packaging KDAL VS Code extension"
	VERBATIM)
endif()

# End-to-end tests
add_custom_target(e2e
	COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/e2e/run_all.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Running KDAL E2E test suites"
	VERBATIM)

# Lint (checkpatch + static analysis)
add_custom_target(lint
	COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/ci/checkpatch.sh
	COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/ci/static_analysis.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Running lint checks"
	VERBATIM)

# CI smoke tests
add_custom_target(ci
	COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/ci/test.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Running CI smoke tests"
	VERBATIM)

# ── Aggregate 'all' target (every buildable target) ──────────────────
# CMake's built-in 'all' only builds compiled targets (kdalc, kdality).
# This meta-target also builds website and vscode extension.
# Test/check targets (e2e, lint, ci) are excluded - run them separately.
set(_KDAL_ALL_DEPS kdalc kdalc_lib kdality)
if(NPM)
  list(APPEND _KDAL_ALL_DEPS website vscode)
endif()
add_custom_target(kdal-all DEPENDS ${_KDAL_ALL_DEPS}
	COMMENT "Building all KDAL targets")

# CPack integration
include(KDALPackaging OPTIONAL)
