# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(das_zephyr)

set(DAS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/main.das")
set(DAS_BINDINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../bindings")
set(DAS_BIND_GEN "${CMAKE_CURRENT_SOURCE_DIR}/../../bind_gen")
set(DAS_AOT "${CMAKE_BINARY_DIR}/das_aot")
set(DAS_STUBS "${CMAKE_BINARY_DIR}/das_stubs")
set(DAS_HOST_PLUGIN_BUILD "${CMAKE_BINARY_DIR}/bind_gen_host")
set(DAS_PLUGIN "${DAS_BINDINGS}/zephyr_das.shared_module")
set(DAS_BIND_CPP "${DAS_BINDINGS}/zephyr_das.cpp")
set(ZEPHYR_GEN "${CMAKE_BINARY_DIR}/zephyr/include/generated/zephyr")

find_program(DASLANG daslang REQUIRED)
find_program(DAS_HOST_C_COMPILER NAMES cc clang gcc REQUIRED)
find_program(DAS_HOST_CXX_COMPILER NAMES c++ clang++ g++ REQUIRED)

# Zephyr emits syscall_list.h / heap_constants.h / syscalls/*.h during the
# firmware build. The binder parses those headers, so it must run after
# those custom commands — not as a prerequisite of generating them.
add_custom_command(
	OUTPUT
		"${DAS_BIND_CPP}"
		"${DAS_BINDINGS}/zephyr_host_stubs.gen.cpp"
	COMMAND ${DASLANG} "${DAS_BIND_GEN}/bind_zephyr.das"
	DEPENDS
		"${ZEPHYR_GEN}/syscall_list.h"
		"${ZEPHYR_GEN}/heap_constants.h"
		"${DAS_BIND_GEN}/bind_zephyr.das"
		"${DAS_BIND_GEN}/headers.txt"
		"${DAS_BIND_GEN}/include_dirs.txt"
		"${DAS_BIND_GEN}/extras.h"
	WORKING_DIRECTORY "${DAS_BIND_GEN}"
	COMMENT "Generate daScript bindings from Zephyr headers"
	VERBATIM
)

# Host plugin uses the host compiler; the west graph is the Zephyr ARM
# toolchain. Configure a nested host build after cbind has written C++.
add_custom_command(
	OUTPUT "${DAS_PLUGIN}"
	COMMAND ${CMAKE_COMMAND}
		-S "${DAS_BIND_GEN}"
		-B "${DAS_HOST_PLUGIN_BUILD}"
		-G "${CMAKE_GENERATOR}"
		"-DCMAKE_C_COMPILER=${DAS_HOST_C_COMPILER}"
		"-DCMAKE_CXX_COMPILER=${DAS_HOST_CXX_COMPILER}"
		"-DCMAKE_BUILD_TYPE=Release"
	COMMAND ${CMAKE_COMMAND} --build "${DAS_HOST_PLUGIN_BUILD}"
	DEPENDS
		"${DAS_BIND_CPP}"
		"${DAS_BINDINGS}/zephyr_host_stubs.gen.cpp"
		"${DAS_BIND_GEN}/CMakeLists.txt"
		"${DAS_BIND_GEN}/zephyr_host.cmake"
		"${DAS_BINDINGS}/zephyr_das.main.cpp"
	COMMENT "Build host daslang plugin zephyr_das.shared_module"
	VERBATIM
)

add_custom_command(
	OUTPUT "${DAS_AOT}/main.das.cpp" "${DAS_AOT}/main.das.h"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DAS_AOT}" "${DAS_STUBS}"
	COMMAND ${DASLANG} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot_driver.das" --
		-cross_platform 1 -ctx "${DAS_SCRIPT}" "${DAS_AOT}/"
	COMMAND ${CMAKE_COMMAND}
		-DDAS_CPP=${DAS_AOT}/main.das.cpp
		-DDAS_STUBS=${DAS_STUBS}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/das_aot.cmake"
	DEPENDS
		"${DAS_SCRIPT}"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/zephyr_hooks.das"
		"${DAS_PLUGIN}"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot_driver.das"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/das_aot.cmake"
		"${DAS_BIND_GEN}/extras.h"
		"${DAS_BIND_GEN}/headers.txt"
	WORKING_DIRECTORY "${DAS_BINDINGS}"
	COMMENT "AOT main.das"
	VERBATIM
)

set_source_files_properties("${DAS_AOT}/main.das.cpp" PROPERTIES GENERATED TRUE)

target_sources(app PRIVATE src/main.cpp "${DAS_AOT}/main.das.cpp")
target_include_directories(app PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${DAS_STUBS}"
	"${DAS_AOT}"
	"${DAS_BINDINGS}"
	"${DAS_BIND_GEN}"
)
