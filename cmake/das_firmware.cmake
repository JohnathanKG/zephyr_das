# Shared west wiring for Zephyr apps that AOT a daslang script and
# link libDaScriptNano. Include this after find_package(Zephyr) so the
# ARM toolchain is already set. Nested host plugin cmake uses host cc/c++.

# find_package(DAS) imports host SHARED libs. Zephyr's Generic/ARM platform
# rejects add_library(SHARED), so allow imported shared targets only for this
# lookup. Firmware still links libDaScriptNano, never DAS::libDaScript.
get_property(_das_had_shared GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
find_package(DAS REQUIRED)
if(_das_had_shared)
	set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS "${_das_had_shared}")
else()
	set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
endif()
unset(_das_had_shared)
get_filename_component(DAS_SDK_ROOT "${DAS_DIR}/../../.." ABSOLUTE)
message(STATUS "daslang SDK root: ${DAS_SDK_ROOT}")

add_subdirectory("${DAS_SDK_ROOT}/nano" nano_build)

# Do not clear INCLUDE_DIRECTORIES on the Zephyr app directory — that
# would drop Zephyr includes. Nano's PUBLIC include order (nano/include
# then SDK include) is enough as long as firmware never links DAS::libDaScript.
target_link_libraries(libDaScriptNano PRIVATE zephyr_interface)

get_filename_component(DAS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(DAS_BIND_GEN "${DAS_ROOT}/bindings/generator")
set(DAS_BINDINGS "${DAS_ROOT}/bindings/generated")
set(DAS_CMAKE "${DAS_ROOT}/cmake")
set(DAS_HOOKS "${DAS_ROOT}/aot_hooks")

find_program(DASLANG daslang REQUIRED)
find_program(DAS_HOST_C_COMPILER NAMES cc clang gcc REQUIRED)
find_program(DAS_HOST_CXX_COMPILER NAMES c++ clang++ g++ REQUIRED)

function(das_zephyr_app das_script app_cpp)
	set(DAS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${das_script}")
	set(DAS_APP_CPP "${CMAKE_CURRENT_SOURCE_DIR}/${app_cpp}")
	set(DAS_AOT "${CMAKE_BINARY_DIR}/das_aot")
	set(DAS_HOST_PLUGIN_BUILD "${CMAKE_BINARY_DIR}/bind_gen_host")
	set(DAS_PLUGIN "${DAS_BINDINGS}/zephyr_das.shared_module")
	set(DAS_BIND_CPP "${DAS_BINDINGS}/zephyr_das.cpp")
	set(ZEPHYR_GEN "${CMAKE_BINARY_DIR}/zephyr/include/generated/zephyr")

	# Zephyr emits syscall_list.h / heap_constants.h during the firmware
	# build. The binder parses those headers, so it must run after those
	# custom commands — not as a prerequisite of generating them.
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
		COMMAND ${CMAKE_COMMAND} --build "${DAS_HOST_PLUGIN_BUILD}"
		DEPENDS
			"${DAS_BIND_CPP}"
			"${DAS_BINDINGS}/zephyr_host_stubs.gen.cpp"
			"${DAS_BIND_GEN}/CMakeLists.txt"
			"${DAS_BIND_GEN}/zephyr_host.cmake"
			"${DAS_BIND_GEN}/zephyr_das.main.cpp"
		COMMENT "Build host daslang plugin zephyr_das.shared_module"
		VERBATIM
	)

	add_custom_command(
		OUTPUT "${DAS_AOT}/main.das.cpp" "${DAS_AOT}/main.das.h"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${DAS_AOT}"
		COMMAND ${DASLANG}
			--disable-module dasZephyr
			-load_module "${DAS_BINDINGS}"
			"${DAS_CMAKE}/aot_driver.das" --
			-cross_platform 1 -ctx "${DAS_SCRIPT}" "${DAS_AOT}/"
		DEPENDS
			"${DAS_SCRIPT}"
			"${DAS_HOOKS}/zephyr_hooks.das"
			"${DAS_PLUGIN}"
			"${DAS_CMAKE}/aot_driver.das"
			"${DAS_BIND_GEN}/extras.h"
			"${DAS_BIND_GEN}/headers.txt"
		WORKING_DIRECTORY "${DAS_BINDINGS}"
		COMMENT "AOT ${das_script}"
		VERBATIM
	)

	set_source_files_properties(
		"${DAS_AOT}/main.das.cpp" "${DAS_AOT}/main.das.h"
		PROPERTIES GENERATED TRUE
	)
	set_source_files_properties("${DAS_APP_CPP}" PROPERTIES
		OBJECT_DEPENDS "${DAS_AOT}/main.das.h"
	)

	target_sources(app PRIVATE
		"${DAS_APP_CPP}"
		"${DAS_AOT}/main.das.cpp"
	)
	target_include_directories(app PRIVATE
		"${DAS_AOT}"
		"${DAS_BINDINGS}"
		"${DAS_BIND_GEN}"
	)
	target_link_libraries(app PRIVATE libDaScriptNano)
endfunction()
