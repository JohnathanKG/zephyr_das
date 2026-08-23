# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(das_zephyr)

set(DAS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/main.das")
set(DAS_BINDINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../bindings")
set(DAS_BIND_GEN "${CMAKE_CURRENT_SOURCE_DIR}/../../bind_gen")
set(DAS_AOT "${CMAKE_BINARY_DIR}/das_aot")
set(DAS_STUBS "${CMAKE_BINARY_DIR}/das_stubs")

find_program(DASLANG daslang REQUIRED)

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
		"${CMAKE_CURRENT_SOURCE_DIR}/zephyr_hooks.das"
		"${DAS_BINDS}/zephyr_das.shared_module"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/aot_driver.das"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/das_aot.cmake"
		"${DAS_BIND}/extras.h"
		"${DAS_BIND}/headers.txt"
	WORKING_DIRECTORY "${DAS_BINDS}"
	COMMENT "AOT test.das"
	VERBATIM
)

target_sources(app PRIVATE src/main.cpp "${DAS_AOT}/main.das.cpp")
target_include_directories(app PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${DAS_STUBS}"
	"${DAS_AOT}"
	"${DAS_BINDS}"
	"${DAS_BIND}"
)
