# Host daslang plugin. Generated C++ lives in ../binds (gitignored).
#   cmake -S bind -B build/host && cmake --build build/host
# Regen bindings first if binds/ is empty:
#   cmake --build build/host --target regen-binds
cmake_minimum_required(VERSION 3.16)
project(zephyr_das)

find_package(DAS REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/zephyr_host.cmake")

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
	"${CMAKE_CURRENT_SOURCE_DIR}/zephyr_das.main.cpp"
	"${STMRENODE_BINDS}/zephyr_das.cpp"
	"${STMRENODE_BINDS}/zephyr_host_stubs.gen.cpp"
	${DAS_ZEPHYR_FUNC_SRCS}
)
target_link_libraries(zephyr_das PRIVATE DAS::libDaScriptDyn)
target_compile_features(zephyr_das PRIVATE cxx_std_17)
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
