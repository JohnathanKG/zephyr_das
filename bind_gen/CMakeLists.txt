# Host daslang plugin (macOS/Linux dylib). Invoked from projects/app/CMakeLists.txt
# after Zephyr has generated syscall_list.h. Nested cmake uses the host compiler,
# not the Zephyr ARM toolchain.
#
# Standalone (only after a west build has emitted generated headers):
#   daslang bind_zephyr.das && cmake -S bind_gen -B bind_gen/build && cmake --build bind_gen/build
cmake_minimum_required(VERSION 3.16)
project(zephyr_das)

find_package(DAS REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/zephyr_host.cmake")

# DAS_BUILD_ID is DAS_VERSION*100+sizeof(void*) in NDEBUG, +10 without.
# A Release plugin against Debug daslang fails with
# "build-id mismatch (host 60418); rebuild the module".
if(NOT CMAKE_CONFIGURATION_TYPES)
	get_property(_das_cfgs TARGET DAS::libDaScriptDyn PROPERTY IMPORTED_CONFIGURATIONS)
	set(_das_bt "")
	if("DEBUG" IN_LIST _das_cfgs)
		set(_das_bt Debug)
	elseif("RELEASE" IN_LIST _das_cfgs)
		set(_das_bt Release)
	elseif("RELWITHDEBINFO" IN_LIST _das_cfgs)
		set(_das_bt RelWithDebInfo)
	elseif("MINSIZEREL" IN_LIST _das_cfgs)
		set(_das_bt MinSizeRel)
	endif()
	string(TOUPPER "${CMAKE_BUILD_TYPE}" _cur)
	string(TOUPPER "${_das_bt}" _need)
	if(_das_bt AND NOT _cur STREQUAL _need)
		if(CMAKE_BUILD_TYPE)
			message(WARNING "DAS is ${_das_cfgs}; CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} mismatches DAS_BUILD_ID. Using ${_das_bt}.")
		else()
			message(STATUS "Matching host plugin CMAKE_BUILD_TYPE=${_das_bt} to DAS (${_das_cfgs})")
		endif()
		set(CMAKE_BUILD_TYPE "${_das_bt}" CACHE STRING "Must match DAS/daslang DAS_BUILD_ID" FORCE)
	endif()
	unset(_das_cfgs)
	unset(_das_bt)
	unset(_cur)
	unset(_need)
endif()

set(STMRENODE_BINDS "${STMRENODE_ROOT}/bindings")
file(MAKE_DIRECTORY "${STMRENODE_BINDS}")
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/das_module.das"
	"${STMRENODE_BINDS}/.das_module"
	COPYONLY
)

file(GLOB DAS_ZEPHYR_FUNC_SRCS CONFIGURE_DEPENDS
	"${STMRENODE_BINDS}/zephyr_das.func_*.cpp"
)

add_library(zephyr_das SHARED
	"${STMRENODE_BINDS}/zephyr_das.main.cpp"
	"${STMRENODE_BINDS}/zephyr_das.cpp"
	"${STMRENODE_BINDS}/zephyr_host_stubs.gen.cpp"
	${DAS_ZEPHYR_FUNC_SRCS}
)
target_link_libraries(zephyr_das PRIVATE DAS::libDaScriptDyn)
target_compile_features(zephyr_das PRIVATE cxx_std_17)
# DAS is built with -fno-rtti. A plugin compiled with RTTI references
# typeinfo for das::Annotation, which liblibDaScriptDyn_runtime.dylib
# does not export — dyld then fails to dlopen the .shared_module.
if(MSVC)
	target_compile_options(zephyr_das PRIVATE /GR-)
else()
	target_compile_options(zephyr_das PRIVATE -fno-rtti)
endif()
stmrenode_zephyr_host_setup(zephyr_das)

find_program(DASLANG daslang REQUIRED)
add_custom_target(regen-binds
	COMMAND ${CMAKE_COMMAND} -E env
		"ZEPHYR_SDK_INSTALL_DIR=${STMRENODE_ZEPHYR_SDK}"
		${DASLANG} "${STMRENODE_BIND}/bind_zephyr.das"
	WORKING_DIRECTORY "${STMRENODE_BIND}"
	COMMENT "Regenerate zephyr_das bindings from bind/headers.txt"
	VERBATIM
)

# Output as .shared_module in the module directory.
# Debug daslang dlopens <name>_debug.shared_module from the path registered
# in .das_module (which stays <name>.shared_module).
set_target_properties(zephyr_das PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${STMRENODE_BINDS}"
	RUNTIME_OUTPUT_DIRECTORY "${STMRENODE_BINDS}"
	SUFFIX ".shared_module"
	PREFIX ""
)
add_custom_command(TARGET zephyr_das POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"$<TARGET_FILE:zephyr_das>"
		"${STMRENODE_BINDS}/zephyr_das_debug.shared_module"
	COMMENT "Copy debug daslang plugin zephyr_das_debug.shared_module"
)
# Zephyr C symbols live in firmware, not in this host plugin.
if(APPLE)
	target_link_options(zephyr_das PRIVATE "LINKER:-undefined,dynamic_lookup")
endif()
