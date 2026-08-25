# Patch standalone AOT (call __init_script) and emit daScript header stubs
# from whatever the generated file actually includes.
if(NOT DAS_CPP)
	message(FATAL_ERROR "das_aot.cmake requires -DDAS_CPP=")
endif()
if(NOT DAS_STUBS)
	message(FATAL_ERROR "das_aot.cmake requires -DDAS_STUBS=")
endif()

file(READ "${DAS_CPP}" src)
if(NOT src MATCHES "__init_script\\(&context")
	if(src MATCHES "context\\.runInitScript\\(\\);")
		string(REPLACE
			"context.runInitScript();"
			"context.runInitScript(); __init_script(&context, true);"
			src "${src}")
	else()
		# aot_standalone.das only emits the ctor closer when the program has
		# AOT functions. Empty scripts (no runInitScript) leave Standalone()
		# unclosed, so later namespace reopenings fail to compile.
		string(REPLACE
			"#ifdef STANDALONE_CONTEXT_TESTS"
			"    context.runInitScript(); __init_script(&context, true);\n}\n#ifdef STANDALONE_CONTEXT_TESTS"
			src "${src}")
	endif()
	file(WRITE "${DAS_CPP}" "${src}")
endif()

set(_inc_src "${src}")
string(REGEX REPLACE "\\.cpp$" ".h" _das_h "${DAS_CPP}")
if(EXISTS "${_das_h}")
	file(READ "${_das_h}" _hsrc)
	string(APPEND _inc_src "\n${_hsrc}")
endif()

string(REGEX MATCHALL "#include[ \t]*\"daScript/[^\"]+\"" inc_lines "${_inc_src}")
foreach(line IN LISTS inc_lines)
	string(REGEX REPLACE "#include[ \t]*\"(daScript/[^\"]+)\"" "\\1" rel "${line}")
	set(out "${DAS_STUBS}/${rel}")
	get_filename_component(dir "${out}" DIRECTORY)
	file(MAKE_DIRECTORY "${dir}")
	file(WRITE "${out}" "#pragma once\n#include \"das_mcu.h\"\n")
endforeach()
