# Shared host-plugin include/define setup. Used by bind/CMakeLists.txt.
get_filename_component(STMRENODE_BIND "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(STMRENODE_ROOT "${STMRENODE_BIND}/../../" ABSOLUTE)

if(NOT DEFINED STMRENODE_ZEPHYR_SDK)
	set(STMRENODE_ZEPHYR_SDK "$ENV{ZEPHYR_SDK_INSTALL_DIR}")
endif()

function(stmrenode_read_bind_paths include_dirs system_dirs imacros)
	set(_inc "")
	set(_sys "")
	set(_im "")
	file(STRINGS "${STMRENODE_BIND}/include_dirs.txt" _lines)
	foreach(line IN LISTS _lines)
		string(STRIP "${line}" line)
		if(line STREQUAL "")
		elseif(line MATCHES "^#")
		elseif(line MATCHES "^sys:")
			string(SUBSTRING "${line}" 4 -1 p)
			list(APPEND _sys "${STMRENODE_ROOT}/${p}")
		elseif(line MATCHES "^imacros:")
			string(SUBSTRING "${line}" 8 -1 p)
			list(APPEND _im "${STMRENODE_ROOT}/${p}")
		else()
			list(APPEND _inc "${STMRENODE_ROOT}/${line}")
		endif()
	endforeach()
	set(${include_dirs} "${_inc}" PARENT_SCOPE)
	set(${system_dirs} "${_sys}" PARENT_SCOPE)
	set(${imacros} "${_im}" PARENT_SCOPE)
endfunction()

function(stmrenode_zephyr_host_setup target)
	stmrenode_read_bind_paths(_inc _sys _im)
	target_include_directories(${target} PRIVATE
		"${STMRENODE_BIND}/host_compat"
		"${STMRENODE_ROOT}/bindings"
		"${STMRENODE_BIND}"
	)
	target_include_directories(${target} SYSTEM PRIVATE ${_inc} ${_sys})
	target_compile_definitions(${target} PRIVATE
		DAS_MOD_EXPORTS
		DAS_ZEPHYR_HOST
		__ZEPHYR__=1
		CORE_CM4
		STM32F446xx
		__PROGRAM_START
	)
	target_compile_options(${target} PRIVATE
		"SHELL:-include ${STMRENODE_BIND}/host_compat/zephyr_host_compat.h"
	)
	foreach(m IN LISTS _im)
		target_compile_options(${target} PRIVATE "SHELL:-imacros ${m}")
	endforeach()
endfunction()
