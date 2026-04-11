cmake_minimum_required(VERSION 3.20)
project(kdal C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include KDAL CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(KDALToolchain OPTIONAL)

# Compiler library (lexer + parser + sema + codegen)
add_subdirectory(compiler)

# kdality — runtime CLI + language toolchain
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
