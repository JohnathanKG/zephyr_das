options gen2
options stack = 1_048_576

require daslib/aot_cpp
require daslib/aot_standalone
require daslib/fio
require daslib/strings_boost
require daslib/rtti
require daslib/ast
require daslib/ast_boost

let INPUT_CALLBACKS_MARK = "// stmrenode:input_callbacks"

def updateCOP(var cop : CodeOfPolicies; gen2 : bool; gen2_make : bool) {
    cop.threadlock_context = false
    cop.aot = false
    cop.aot_module = true
    cop.aot_lib = false
    cop.fail_on_lack_of_aot_export = true
    cop.ignore_shared_modules = false
    cop.version_2_syntax = gen2
    cop.gen2_make_syntax = gen2_make
    cop.tune_frozen = true
}

def get_list_of_files(args : array<string>; key : string) {
    let len = length(args)
    var result : array<tuple<string; string>>
    for (i in range(len)) {
        if (args[i] == key) {
            if (i + 2 >= len) {
                panic("Arguments expected in format `-ctx <in> <out>`.")
            }
            result |> push((args[i + 1], args[i + 2]))
        }
    }
    return <- result
}

def find_bool(args : array<string>; key : string) {
    let idx = find_index(args, key)
    return idx >= 0 && idx + 1 < length(args)
}

def ann_str(args : AnnotationArgumentList; name, defv : string) {
    let v = find_arg(args, name)
    if (v is tString) {
        return v as tString
    }
    return defv
}

def aot_fn_name(fn : FunctionPtr) {
    let n = string(fn.origin != null ? fn.origin.name : fn.name)
    return replace(n, "`", "__")
}

def collect_input_callbacks(program : ProgramPtr) {
    var out : array<tuple<string; string; string>>
    program.getThisModule |> for_each_function("") $(fn) {
        for (ann in fn.annotations) {
            if (ann == null || ann.annotation == null) {
                continue
            }
            if (string(ann.annotation.name) != "on_input") {
                continue
            }
            out |> push((
                aot_fn_name(fn),
                ann_str(ann.arguments, "dev", "NULL"),
                ann_str(ann.arguments, "user_data", "NULL")
            ))
        }
    }
    return <- out
}

def emit_input_callbacks(output_dir, ctx_name : string; hooks : array<tuple<string; string; string>>) {
    let hpp = "{output_dir}/{ctx_name}.das.h"
    let cpp = "{output_dir}/{ctx_name}.das.cpp"
    var header = fread(hpp)
    var source = fread(cpp)
    if (empty(header) || empty(source)) {
        panic("standalone AOT did not write {hpp} / {cpp}")
    }
    // Standalone AOT prunes cbind modules (their fns are builtIn), so
    // aotRequire never emits zephyr_api.h. Inject it for the firmware TU.
    if (find(header, "zephyr_api.h") < 0) {
        header = "#include \"zephyr_api.h\"\n" + header
    }
    if (find(source, "zephyr_api.h") < 0) {
        source = "#include \"zephyr_api.h\"\n" + source
    }
    if (find(header, INPUT_CALLBACKS_MARK) < 0) {
        if (!empty(hooks) && find(header, "zephyr/input/input.h") < 0) {
            header = "#include <zephyr/input/input.h>\n" + header
        }
        header += build_string() $(tw) {
            write(tw, "\n{INPUT_CALLBACKS_MARK}\n")
            write(tw, "namespace das \{\nnamespace {ctx_name} \{\n")
            write(tw, "extern Standalone *g_ctx;\n")
            write(tw, "\}\n\}\n")
        }
        if (!fwrite(hpp, header)) {
            panic("failed to write {hpp}")
        }
    }
    if (find(source, INPUT_CALLBACKS_MARK) < 0) {
        source += build_string() $(tw) {
            write(tw, "\n{INPUT_CALLBACKS_MARK}\n")
            write(tw, "namespace das \{\nnamespace {ctx_name} \{\n")
            write(tw, "Standalone *g_ctx = nullptr;\n")
            write(tw, "\}\n\}\n")
            if (!empty(hooks)) {
                write(tw, "\n#include <zephyr/input/input.h>\n\n")
                for ((name, dev, user_data) in hooks) {
                    write(tw, "static void das_{name}(struct input_event *evt, void *arg)\n")
                    write(tw, "\{\n")
                    write(tw, "	if (das::{ctx_name}::g_ctx == nullptr || evt == nullptr) \{\n")
                    write(tw, "		return;\n")
                    write(tw, "	\}\n")
                    write(tw, "	das::{ctx_name}::g_ctx->{name}(*evt, arg);\n")
                    write(tw, "\}\n\n")
                    write(tw, "INPUT_CALLBACK_DEFINE({dev}, das_{name}, {user_data});\n\n")
                }
            }
        }
        if (!fwrite(cpp, source)) {
            panic("failed to write {cpp}")
        }
    }
}

def standalone_aot_zephyr(input, output_dir : string; cross_platform : bool; cop : CodeOfPolicies) {
    let file_name = input |> split_by_chars("/\\") |> back()
    let ctx_name = (file_name |> split("."))[0]
    var hooks : array<tuple<string; string; string>>
    var compiled = false
    using() $(var mg : ModuleGroup) {
        var inscope access <- make_file_access("")
        compile_and_simulate(input, access, unsafe(addr(mg)), cop) $(var program : ProgramPtr; var pctx : smart_ptr<Context>) {
            let (_, noAotModule) = getRequiredModulesFor(program)
            if (program._options |> find_arg("no_aot") ?as tBool ?? false || noAotModule) {
                panic("Standalone context called on non aot module {input}")
            }
            hooks <- collect_input_callbacks(program)
            compiled = true
        }
    }
    if (!compiled) {
        return false
    }
    if (!standalone_aot(input, output_dir, cross_platform, false, cop)) {
        return false
    }
    emit_input_callbacks(output_dir, ctx_name, hooks)
    return true
}

[export]
def main() {
    let args <- get_command_line_arguments()
    let cross_platform = find_bool(args, "-cross_platform")
    let gen1 = find_bool(args, "-gen1")
    let gen2_make = find_bool(args, "-gen2-make")
    var ctx_files <- get_list_of_files(args, "-ctx")
    if (empty(ctx_files)) {
        panic("expected `-ctx <in.das> <out_dir>`")
    }
    var result = true
    using() $(var cop : CodeOfPolicies) {
        updateCOP(cop, !gen1, gen2_make)
        for ((ctx_in, ctx_out) in ctx_files) {
            ast_gc_guard() {
                if (!standalone_aot_zephyr(ctx_in, ctx_out, cross_platform, cop)) {
                    to_log(LOG_ERROR, "Failed to compile `{ctx_in}` in standalone.\n")
                    result = false
                }
            }
        }
    }
    return result ? 0 : -1
}
