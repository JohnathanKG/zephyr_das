options gen2
require cbind/cbind_boost
require daslib/fio
require daslib/defer
require daslib/strings_boost
require daslib/regex_boost

def trim_line(s : string) {
    var a = 0
    var b = length(s)
    while (a < b) {
        let c = character_at(s, a)
        if (c != ' ' && c != '\t' && c != '\r') {
            break
        }
        a++
    }
    while (b > a) {
        let c = character_at(s, b - 1)
        if (c != ' ' && c != '\t' && c != '\r') {
            break
        }
        b--
    }
    return s |> slice(a, b)
}

def read_lines(path : string) : array<string> {
    var out : array<string>
    let data = fread(path)
    if (empty(data)) {
        panic("missing or empty {path}")
    }
    for (line in data |> split("\n")) {
        let s = trim_line(line)
        if (empty(s) || s |> starts_with("#")) {
            continue
        }
        out |> push(s)
    }
    return <- out
}

def find_repo_root : string {
    var here = get_full_file_name("bind_zephyr.das")
    if (!stat(here).is_valid) {
        here = get_full_file_name("bind_gen/bind_zephyr.das")
    }
    if (!stat(here).is_valid) {
        panic("cannot locate bind_zephyr.das")
    }
    return dir_name(dir_name(here))
}

def require_env(name, fallback : string) {
    let v = get_env_variable(name)
    return empty(v) ? fallback : v
}

def find_clang_resource_dir : string {
    let env = get_env_variable("CLANG_RESOURCE_DIR")
    if (!empty(env)) {
        return env
    }
    let base = "/opt/homebrew/opt/llvm/lib/clang"
    if (!stat(base).is_valid) {
        panic("set CLANG_RESOURCE_DIR or install Homebrew LLVM at {base}")
    }
    var ver = ""
    dir(base) $(name) {
        ver = name
    }
    if (empty(ver)) {
        panic("no clang resource dir under {base}")
    }
    return "{base}/{ver}"
}

class ZephyrBind : CppGenBind {
    override func_to_stdout = false
    override include_local_parse_file = true
    bound_func_names : table<string; bool>
    stub_needed : table<string; bool>
    repo_root : string
    skip_func_pfx : array<string>
    skip_struct_names : table<string; bool>

    def ZephyrBind(root, bind_root_, pfn, pfp : string; args : array<string>) {
        repo_root = root
        bind_root = bind_root_
        bind_module = "zephyr_das"
        bind_das_module = "zephyr"
        skip_func_pfx <- array<string>("ARM_MPU_", "arm_m_")
        skip_struct_names["arm_m_cs_ptrs"] = true
        skip_struct_names["_isr_list_sname"] = true
        func_per_chunk = 50
        super(pfn, pfp, args)
        openAllFiles()
        desktop_stub_file = openFile("{bind_root}/zephyr_host_stubs.gen.cpp")
        fprint(desktop_stub_file, "#include \"zephyr_host_compat.h\"\n")
        fprint(desktop_stub_file, "#include \"zephyr_api.h\"\n")
        fprint(desktop_stub_file, "#include <zephyr/kernel_structs.h>\n")
        fprint(desktop_stub_file, "#include <zephyr/sw_isr_table.h>\n")
        fprint(desktop_stub_file, "#include <cstdlib>\n")
        fprint(desktop_stub_file, "#pragma clang diagnostic ignored \"-Winvalid-noreturn\"\n")
        fprint(desktop_stub_file, "#pragma clang diagnostic ignored \"-Wunused-parameter\"\n\n")
    }

    def override generateModuleNeedHPrefix {
        module_need_h_file |> fwrite("#include \"zephyr_host_compat.h\"\n")
    }

    def override isArgByValue(name : string) {
        return true
    }

    def override skip_struct(name : string) {
        if (skip_struct_names |> key_exists(name)) {
            return true
        }
        return inClass > 0 && !(name |> starts_with("__"))
    }

    def override skip_function(var c : CXCursor) : bool {
        let function_name = string(clang_getCursorSpelling(c))
        for (pfx in skip_func_pfx) {
            if (function_name |> starts_with(pfx)) {
                return true
            }
        }
        if (bound_func_names |> key_exists(function_name)) {
            return true
        }
        if (skip_anyFunction(c, false)) {
            return true
        }
        // SkipFunctionBodies makes every fn look like a prototype; only stub
        // non-static functions from Zephyr (not bind/ extras).
        let loc = clang_getCursorLocationFileName(c)
        let sc = clang_Cursor_getStorageClass(c)
        if (sc != CX_StorageClass.Static && !(loc |> starts_with("{repo_root}/bind/"))) {
            stub_needed[function_name] = true
        }
        bound_func_names[function_name] = true
        return false
    }

    def override emit_desktop_stub(function_name : string) : bool {
        return stub_needed?[function_name] ?? false
    }

    def override skip_file(fname : string) {
        if (!(fname |> ends_with(".h"))) {
            return true
        }
        if (fname |> starts_with("{repo_root}/zephyr/")) {
            return false
        }
        if (fname |> starts_with("{repo_root}/build/zephyr/")) {
            return false
        }
        if (fname |> starts_with("{repo_root}/bind/")) {
            return false
        }
        if (fname |> starts_with(bind_root)) {
            return false
        }
        return true
    }

    def override generate {
        CppGenBind`generate(self)
        fprint(desktop_stub_file, "\nextern \"C\" \{\n")
        fprint(desktop_stub_file, "struct z_kernel _kernel \{\};\n")
        fprint(desktop_stub_file, "bool z_sys_post_kernel = true;\n")
        fprint(desktop_stub_file, "struct device _device_list_start[1] \{\};\n")
        fprint(desktop_stub_file, "struct device _device_list_end[1] \{\};\n")
        fprint(desktop_stub_file, "\}\n")
        closeFile(desktop_stub_file)
        let stub_path = "{bind_root}/zephyr_host_stubs.gen.cpp"
        var src = fread(stub_path)
        src = src |> replace("\{ return (void)0; \}", "\{ return; \}")
        var re <- %regex~\{\s*return \([^)]+\)0;\s*\}%%
        src = regex_replace(re, src, "\{ return \{\}; \}")
        // Parentheses around the name stop Zephyr function-like macros
        // (#define arch_irq_enable(irq) arm_irq_enable(irq)) from rewriting stubs.
        var re_name <- %regex~([A-Za-z_][A-Za-z0-9_]*)\(%%
        src = regex_replace(re_name, src, "($1)(")
        fwrite(stub_path, src)
    }
}

def write_umbrella(bind_root : string; headers : array<string>) {
    var text = "#pragma once\n\n"
    text += "// generated from bind_gen/headers.txt — do not edit\n"
    for (h in headers) {
        text += "#include <{h}>\n"
    }
    text += "#include \"extras.h\"\n"
    fwrite("{bind_root}/zephyr_api.h", text)
}

def write_macro_constants(root, bind_root : string; headers : array<string>) {
    var seen : table<string; bool>
    var fnlike : table<string; bool>
    var text = "// generated from bind_gen/headers.txt — do not edit\n"
    var re_fn <- %regex~#define\s+([A-Z][A-Z0-9_]+)\(%%
    var re_obj <- %regex~#define\s+([A-Z][A-Z0-9_]+)[ \t]+([^\n]*)%%
    for (h in headers) {
        let data = fread("{root}/zephyr/include/{h}")
        regex_foreach(re_fn, data) $(r) {
            fnlike[regex_group(re_fn, 1, data)] = true
            return true
        }
        regex_foreach(re_obj, data) $(r) {
            let name = regex_group(re_obj, 1, data)
            var repl = trim_line(regex_group(re_obj, 2, data))
            if (fnlike |> key_exists(name) || seen |> key_exists(name)) {
                return true
            }
            if (name |> starts_with("CONFIG_") || name |> starts_with("DT_") || name |> starts_with("_")) {
                return true
            }
            if (name |> ends_with("_H_") || name |> ends_with("_H") || find(name, "_DT_") >= 0) {
                return true
            }
            if (empty(repl) || find(repl, ";") >= 0 || find(repl, "\{") >= 0 || find(repl, "\"") >= 0) {
                return true
            }
            // Bare identifier aliases (K_FOREVER -> Z_FOREVER) are often structs.
            if (find(repl, " ") < 0 && find(repl, "(") < 0 && find(repl, "|") < 0 && find(repl, "+") < 0 && find(repl, "-") < 0) {
                return true
            }
            seen[name] = true
            text += "#ifdef {name}\n"
            text += "addConstant<uint32_t>(*this, \"{name}\", (uint32_t)({name}));\n"
            text += "#endif\n"
            return true
        }
    }
    fwrite("{bind_root}/zephyr_das.const.inc", text)
}

def clang_args(root, sdk, clang_res : string; inc_lines : array<string>) : array<string> {
    var args <- array<string>(
        "-xc++-header",
        "-std=c++17",
        "-nostdinc++",
        "--target=armv7m-none-eabi",
        "-mcpu=cortex-m4",
        "-mthumb",
        "-mabi=aapcs",
        "-D__ZEPHYR__=1",
        "-DCORE_CM4",
        "-DSTM32F446xx",
        "-D__PROGRAM_START",
        "--sysroot={sdk}/gnu/arm-zephyr-eabi/arm-zephyr-eabi",
        "-resource-dir", clang_res,
        "-isystem", "{clang_res}/include",
        "-I{root}/bind_gen"
    )
    for (line in inc_lines) {
        if (line |> starts_with("sys:")) {
            args |> push("-isystem")
            args |> push("{root}/{line |> slice(4)}")
        } elif (line |> starts_with("imacros:")) {
            args |> push("-imacros")
            args |> push("{root}/{line |> slice(8)}")
        } else {
            args |> push("-I{root}/{line}")
        }
    }
    return <- args
}

[export]
def main {
    let root = find_repo_root()
    let bind_gen_dir = "{root}/bind_gen"
    let bindings = "{root}/bindings"
    let sdk = require_env("ZEPHYR_SDK_INSTALL_DIR", "")
    if (empty(sdk)) {
        panic("ZEPHYR_SDK_INSTALL_DIR is not set")
    }
    let headers <- read_lines("{bind_gen_dir}/headers.txt")
    let inc_lines <- read_lines("{bind_gen_dir}/include_dirs.txt")
    write_umbrella(bind_gen_dir, headers)
    write_macro_constants(root, bind_gen_dir, headers)
    dir(bindings) $(name) {
        if (name |> starts_with("zephyr_das.func_") && name |> ends_with(".cpp")) {
            remove("{bind_gen_dir}/{name}")
        }
    }
    var args <- clang_args(root, sdk, find_clang_resource_dir(), inc_lines)
    var cgb = new ZephyrBind(root, bindings, "zephyr_api.h", "{bindings}/", args)
    defer() {
        unsafe {
            delete cgb
        }
    }
    cgb->generate()
}
