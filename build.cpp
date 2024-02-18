#define BUILD_NATIVE
#define BUILD_WEB
#define OUTPUT_NAME "breakout"
#define BUILD_SCRIPT_MEMORY_LIMIT 20 * 1024
// #define DEVELOPER
// #define OPTIMIZED
// #define DISABLE_ASSERT

// #define POST_BUILD() if (!run_cmd(OUTPUT)) return 1
// #define POST_BUILD() if (!run_cmd("remedybg.exe", OUTPUT_DIR OUTPUT_NAME)) exit(1)
// #define POST_BUILD() if (!run_cmd(PY, "-m", "http.server", "-d", OUTPUT_DIR)) return 1

// we are going to get our build tools from the emsdk
// https://emscripten.org/
#define EMSDK "tools/emsdk/"

// The final output of this build system is a website
// with a c++ program embedded in it, but since the webassembly
// compiler is slower than a normal c compiler we'll use clang to
// build a native application that we can use during development.
#define CC EMSDK "upstream/bin/clang"
#define AR EMSDK "upstream/bin/llvm-ar"

// The WebAssembly compiler toolchain is actually a series of
// python scripts that call clang and llvm-ar in funky ways
#define PY EMSDK "python/3.9.2-nuget_64bit/python.exe"
#define EMCC PY, EMSDK "upstream/emscripten/emcc.py"
#define EMAR PY, EMSDK "upstream/emscripten/emar.py"

#define NCZ_DEFAULT_LIST_CAPACITY 32
#define NCZ_CC(bin, src) CC, "-g", "-std=c++17", "-ldbghelp", "-o", bin, src
#define NCZ_IMPLEMENTATION
#include "source/ncz.hpp"
using namespace ncz;

// template <typename F> void visit_files(String dir, F visit_proc)
// List<String> srcs {}; visit_files("source"str, [&](String path){ if (!path.contains("raylib"str)) src.push(path.copy()); })
// if (needs_update(String::from_cstr(OUTPUT), srcs)) { ... }

#define OUTPUT_DIR  "build/"
#define TEMP_DIR    "build/temp/"
#define ENTRY_POINT "source/main.cpp"
#define RAYLIB_DIR  "source/raylib/"

bool build_dependencies();
bool build_application();

#ifdef _WIN32
#define OUTPUT OUTPUT_DIR OUTPUT_NAME ".exe"
#else
#define OUTPUT OUTPUT_DIR OUTPUT_NAME
#endif // _WIN32

int main(int argc, cstr *argv) {
    context.logger.labels.push("config");
    NCZ_CPP_FILE_IS_SCRIPT(argc, argv);
    context.logger.labels.pop();
    context.logger.labels.push("build");
    
    auto global_arena = use_global_arena<BUILD_SCRIPT_MEMORY_LIMIT>();
    NCZ_DEFER(log("script used ", global_arena->data.count, " bytes."));
    
    if (!build_dependencies()) return 1;
    if (!build_application())  return 1;
    
    #ifdef POST_BUILD
    POST_BUILD();
    #endif
    
    return 0;
}

static const cstr raylib_compilation_unit_names[] = {
    "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils"
};
static const Array<cstr> raylib_units = {
    (cstr*)raylib_compilation_unit_names, sizeof(raylib_compilation_unit_names)/sizeof(raylib_compilation_unit_names[0])
};

bool build_dependencies() {
    List<Process>  procs      {};
    String_Builder input_path {};
    
    List<cstr> cc {};
    List<cstr> ar {};
    auto build_with_cc = [&](bool web) {
        #ifdef OPTIMIZED
        cc.push("-Os");
        #endif // OPTIMIZED
        
        auto target = web ? "-web"_str : "-native"_str;
        
        #ifdef _WIN32
        cstr lib_name = cprint(TEMP_DIR "raylib", target, ".lib"_str).data;
        #else
        cstr lib_name = cprint(TEMP_DIR "libraylib", target, ".a"_str).data;
        #endif // _WIN32
        
        cc.append("-Wno-everything", "-std=gnu11", "-I./" RAYLIB_DIR "external/glfw/include", "-c");
        ar.append("crs", lib_name);
        
        for (cstr unit : raylib_units) {
            if (web && strcmp(unit, "rglfw") == 0) continue;
            input_path.count = 0;
            write(&input_path, RAYLIB_DIR, unit, ".c\0"_str);
            String object_file = cprint(TEMP_DIR, unit, target, ".o");
            ar.push(object_file.data);
            
            if (!needs_update(object_file, {input_path.data, input_path.count})) continue;
            
            cc.append(input_path.data, "-o", object_file.data);
                procs.push(run_command_async(cc));
            cc.count -= 3;
        }
        
        if (!wait(procs)) return false;
        if (procs.count)  return run_command_sync(ar);
        return true;
    };
    
    bool ok = true;
    
#ifdef BUILD_WEB
    log("compiling raylib to wasm");
    cc.append(EMCC, "-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES2");
    ar.append(EMAR);
    ok = build_with_cc(true);
#endif // BUILD_WEB

#ifdef BUILD_NATIVE
    log("compiling native raylib");
    procs.count = 0; input_path.count = 0; cc.count = 0; ar.count = 0;
    cc.append(CC, "-g", "-DPLATFORM_DESKTOP");
    ar.append(AR);
    ok = build_with_cc(false);
#endif // BUILD_NATIVE

    return ok;
}

bool build_application() {
    List<cstr> cmd {};
    bool ok = true;
    
    // List<String> sources {};
    // visit_files("./src"_str, [&] (String path) { if (!path.includes(to_string(RAYLIB_DIR))) sources.push(path); });
    
#ifdef BUILD_WEB
    cmd.count = 0;
    if (needs_update(cprint(OUTPUT_DIR, "index.html"), "source/main.cpp"_str)) {
        cmd.append(EMCC, "-std=c++17", "-o", OUTPUT_DIR "index.html");
        cmd.append("-sTOTAL_MEMORY=67108864","-sUSE_GLFW=3", "-sASSERTIONS", "-sASYNCIFY", "-sFORCE_FILESYSTEM", "-sGL_ENABLE_GET_PROC_ADDRESS", "--profiling");
        cmd.append(ENTRY_POINT, "-L" TEMP_DIR, "-lglfw", "-lraylib-web", "--shell-file", "source/shell.html");
        ok = run_command_sync(cmd);
    }
#endif
    
#ifdef BUILD_NATIVE
    if (needs_update(to_string(OUTPUT), "source/main.cpp"_str)) {
        cmd.count = 0;
        cmd.append(CC, ENTRY_POINT, "-g", "-std=c++17", //"-Wall", "-Wextra",
                   "-Wpedantic", "-o", OUTPUT, "-DPLATFORM_DESKTOP",
                   "-I" RAYLIB_DIR "/src/external/glfw/include",
                   "-L" TEMP_DIR, "-lraylib-native");
    #ifdef _WIN32
        cmd.append("-lwinmm", "-lgdi32", "-luser32", "-lshell32");
    #endif
        ok = run_command_sync(cmd);
    }
#endif
    
    return ok;
}