#define NCZ_DEFAULT_LIST_CAPACITY 32
#define NCZ_IMPLEMENTATION
#include "source/ncz.hpp"
using namespace ncz;

// template <typename F> void visit_files(String dir, F visit_proc)1
// List<String> srcs {}; visit_files("source"str, [&](String path){ if (!path.contains("raylib"str)) src.push(path.copy()); })
// if (needs_update(String::from_cstr(OUTPUT), srcs)) { ... }
#define BUILD_SCRIPT_MEMORY_LIMIT 4 * 1024
#define OUTPUT_DIR  "build/"
#define OUTPUT_NAME "breakout"
#define BUILD_NATIVE
// #define BUILD_WEB
// #define DEVELOPER
// #define OPTIMIZED
// #define DISABLE_ASSERT
// #define POST_BUILD() if (!run_cmd(OUTPUT_DIR OUTPUT_NAME)) return 1
// #define POST_BUILD() if (!run_cmd("remedybg.exe", OUTPUT_DIR OUTPUT_NAME)) exit(1)

bool build_dependencies();
bool build_application();

int main(int argc, cstr *argv) {
    context.logger.labels.push("config");
    NCZ_CPP_FILE_IS_SCRIPT(argc, argv);
    context.logger.labels.pop();
    context.logger.labels.push("build");
    
    auto global_arena = use_global_arena<BUILD_SCRIPT_MEMORY_LIMIT>();
    NCZ_DEFER(log("Total memory used: ", global_arena->data.count, " bytes."));
    
    if (!build_dependencies()) return 1;
    if (!build_application())  return 1;
    
    #ifdef POST_BUILD
    POST_BUILD();
    #endif
    
    return 0;
}

#define ENTRY_POINT "source/main.cpp"
#define RAYLIB_DIR  "source/raylib/"

static const cstr raylib_compilation_unit_names[] = { "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils" };
static const Array<cstr> raylib_units = { (cstr*)raylib_compilation_unit_names, sizeof(raylib_compilation_unit_names)/sizeof(raylib_compilation_unit_names[0]) };

#define EMCC "clang", "-target", "wasm32-unknown-emscripten", "-fignore-exceptions", "-fvisibility=default", "-mllvm", "-combiner-global-alias-analysis=false", "-mllvm", "-enable-emscripten-sjlj", "-mllvm", "-disable-lsr", "-DEMSCRIPTEN", "-mllvm", "-lc", "-Xclang", "--sysroot=build/temp/sysroot", "-Xclang", "-iwithsysroot/include/fakesdl", "-Xclang", "-iwithsysroot/include/compat"

#ifdef _WIN32
#define MAYBE_WSL(cmd) (cmd).push("wsl")
#define OUTPUT OUTPUT_DIR OUTPUT_NAME ".exe"
#else
#define MAYBE_WSL(cmd)
#define OUTPUT OUTPUT_DIR OUTPUT_NAME
#endif

void bar() {
    log_stack_trace();
}
void foo() {
    bar();
}

bool build_dependencies() {
    List<Process>  procs      {};
    String_Builder input_path {};
    
    List<cstr> cc {};
    List<cstr> ar {};
    
    foo();
    return false;
    
    bool ok = true;
    
    // we can compile raylib the same way on pretty much every c compile except msvc....
    auto build_with_cc = [&](bool web) {
        #ifdef OPTIMIZED
        cc.push("-Os");
        #endif
        cc.append("-Wno-everything", "-std=gnu11", "-I./" RAYLIB_DIR "external/glfw/include", "-c");
        
        ar.append("crs", OUTPUT_DIR "libraylib.a");
        
        for (cstr unit : raylib_units) {
            if (web && strcmp(unit, "rglfw") == 0) continue;
            input_path.count = 0;
            write(&input_path, RAYLIB_DIR, unit, ".c\0"_str);
            String object_file = cprint(OUTPUT_DIR, unit, ".o");
            ar.push(object_file.data);
            
            if (true != needs_update(object_file, String { input_path.data, input_path.count })) continue;
            
            cc.append(input_path.data, "-o", object_file.data);
                procs.push(run_command_async(cc));
            cc.count -= 3;
        }
        
        ok = wait(procs);
        if (ok && procs.count) ok = run_command_sync(ar);
        return ok;
    };
    
#ifdef BUILD_WEB
    log("Doing the web build");
    MAYBE_WSL(cc);
    cc.append(EMCC, "-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES2");
    
    MAYBE_WSL(ar);
    ar.append("llvm-emar");
    ok = build_with_cc(true);
#endif // BUILD_WEB

#ifdef BUILD_NATIVE
    log("Doing the native build");
    procs.count = 0; input_path.count = 0; cc.count = 0; ar.count = 0;
    #if defined(_MSC_VER)
        cc.append("cl", "/std:c11", "/MDd",
                   "/nologo", "/W0", "/Zi", "/FS",
                   "/DPLATFORM_DESKTOP", "/I" RAYLIB_DIR "external/glfw/include",
                   "/Fo" OUTPUT_DIR, "/c");
        
        for (cstr unit : raylib_units) {
            write(&input_path, RAYLIB_DIR, unit, ".c\0"_str);
            cc.push(input_path.data);
                procs.push(run_command_async(cc));
            cc.pop();
            input_path.count = 0;
        }
        
        ok = wait(procs);
        if (ok && procs.count) {
            ar.append("lib", "/nologo", "/OUT:" OUTPUT_DIR "raylib.lib");
            for (cstr unit : raylib_units) {
                String path = cprint(OUTPUT_DIR, unit, ".obj");
                ar.push(path.data);
            }
        }
        ok = run_command_sync(cc) && ok;
    #else
        #if defined(__GNUC__)
            cc.push("gcc");
        #elif defined(__clang__)
            cc.push("clang");
        #else
            #error "unsupported c compiler"
        #endif // defined(__GNUC__)
        cc.push("-DPLATFORM_DESKTOP");
        ar.push("ar");
        ok = build_with_cc(false);
    #endif // defined(_MSC_VER)
    
#endif // BUILD_NATIVE
    return ok;
}

bool build_application() {
    List<cstr> cmd {};
    bool ok = true;
    
    // List<String> sources {};
    // visit_files("src"_str, [&] (String path) {
    //     if (!path.includes(to_string(RAYLIB_DIR))) sources.push(path);
    // });
    
#ifdef BUILD_WEB
    cmd.count = 0;
    if (true == needs_update(cprint(OUTPUT_DIR, "index.html"_str), "source/main.cpp"_str)) {
        MAYBE_WSL(cmd);
        cmd.append(EMCC, "-std=c++17", "-o", OUTPUT_DIR "index.html");
        cmd.append("-DDEVELOPER", "-O3");
        cmd.append("-sTOTAL_MEMORY=67108864","-sUSE_GLFW=3", "-sASSERTIONS", "-sASYNCIFY", "-sFORCE_FILESYSTEM", "--profiling");
        cmd.append(ENTRY_POINT, "-L" OUTPUT_DIR, "-lglfw", "-lraylib", "--shell-file", "source/shell.html");
        ok = run_command_sync(cmd);
    }
#endif
    
#ifdef BUILD_NATIVE
    if (true == needs_update(to_string(OUTPUT), "source/main.cpp"_str)) {
        cmd.count = 0;
#if defined(_MSC_VER)
        cmd.append(
            "cl", "/std:c++17", "/MDd", "/DDEVELOPER", "/Od",
            "/W4", "/WX", "/FC", "/Zi", "/J", "/nologo",
            "/Fe" OUTPUT, "/Fd" OUTPUT ".pdb",
            ENTRY_POINT, OUTPUT_DIR "raylib.lib",
            "/link", "/INCREMENTAL:NO", "/ignore:4099"
        );
#else
#if defined(__GNUC__)
        cmd.push("gcc");
#elif defined(__clang__)
        cmd.push("clang");
#else
        #error "unsupported c compiler"
#endif // defined(__GNUC__)
        cmd.append(ENTRY_POINT, "-std=c++17", //"-Wall", "-Wextra",
                   "-Wpedantic", "-o", OUTPUT, "-DPLATFORM_DESKTOP",
                   "-I" RAYLIB_DIR "/src/external/glfw/include",
                   "-L" OUTPUT_DIR, "-l:libraylib.a", "-lstdc++");
        
    #ifdef _WIN32
        cmd.append("-lwinmm", "-lgdi32");
    #endif
        
        ok = run_command_sync(cmd);
#endif // defined(_MSC_VER)
    }
#endif
    
    return ok;
}
