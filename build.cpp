#define NCZ_IMPLEMENTATION
#include "source/ncz.hpp"
using namespace ncz;

namespace ncz {
    bool wait(Array<Process> procs) {
        bool ok = true;
        for (auto proc : procs) ok = wait(proc) && ok;
        return ok;
    }
    
    // encodes False, True, and Error values
    enum class Result_Bool { F = 0, T = 1, E = 2, };
    
    String to_string(cstr str) { return { (char*)str, strlen(str) }; }
}

#ifdef _WIN32
Result_Bool needs_update(String output_path, Array<String> input_paths) {
    BOOL bSuccess;
    
    HANDLE output_path_fd = CreateFile(output_path.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return Result_Bool::T;
        // log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
        return Result_Bool::E;
    }
    
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    if (!bSuccess) {
        // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
        return Result_Bool::E;
    }
    
    for (auto input_path : input_paths) {
        HANDLE input_path_fd = CreateFile(input_path.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (input_path_fd == INVALID_HANDLE_VALUE) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
            log("BAD");
            return Result_Bool::E;
        }
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        if (!bSuccess) {
            // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
            log("BAD");
            return Result_Bool::E;
        }
    
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1) return Result_Bool::T;
    };
    
    return Result_Bool::F;
}

Result_Bool needs_update(String output_path, String input_path) {
    return needs_update(output_path, { &input_path, 1 });
}

bool rename_file(String old_path, String new_path) {
    assert(*(old_path.data + old_path.count) == 0);
    assert(*(new_path.data + new_path.count) == 0);
    // TODO: make these logs trace or verbose
    log("[rename_file] ", old_path, " -> ", new_path);
     if (!MoveFileEx(old_path.data, new_path.data, MOVEFILE_REPLACE_EXISTING)) {
        char buf[256];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
        auto str = to_string(buf);
        if (str[str.count-1] == '\n') str.count -= 1;
        log_ex(Log_Level::NORMAL,Log_Type::ERRO, "Could not rename "_str, old_path, ": "_str, str);
        return false;
    }
    return true;
}

bool file_exists(String path) {
    assert(false);
    return false;
}

bool write_file(String path, String data) {
    assert(false);
    return false;
}
#else
#error "TODO"
#endif

#define NCZ_CPP_FILE_IS_SCRIPT(argc, argv) \
    reload_cpp_script({(argv), static_cast<u64>(argc)}, to_string(__FILE__), {}, {})
#define NCZ_CPP_FILE_IS_SCRIPT_WITH_CONFIG(argc, argv, path, conf) \
    reload_cpp_script({(argv), static_cast<u64>(argc)}, to_string(__FILE__), (path), (conf))

// TODO: if you call an executable without the .exe on windows but you built with .exe, it wont work lol
void reload_cpp_script(Array<cstr> args, String src, String config_path, String default_config) {
    assert(args.count >= 1);
    auto program = to_string(args[0]);
    auto old     = cprint(program, ".old");
    if (needs_update(program, src) == Result_Bool::T) {
        log("Rebuilding script ", src);
        if (!rename_file(program, old)) exit(1);
        if (!run_cmd(NCZ_BUILD_CPP_EXE(program.data, __FILE__))) {
            rename_file(old, program);
            exit(1);
        }
        if (!run_command_sync(args)) exit(1);
        exit(0);
    }
    #ifndef CONFIGURED
    if (config_path.count && (needs_update(program, config_path) == Result_Bool::T)) {
        log("Rebuilding config ", config_path);
        assert(default_config.count);
        if (!file_exists(config_path)) {
            if (!write_file(config_path, default_config)) exit(1);
        }
        if (!rename_file(program, old)) exit(1);
        if (!run_cmd(NCZ_BUILD_CPP_EXE(program.data, __FILE__))) {
            rename_file(old, program);
            exit(1);
        }
        if (!run_command_sync(args)) exit(1);
        exit(0);
    }
    #endif
}

// cls && clang-cl.exe /std:c++17 build.cpp && cls && build.exe dev native
// clear && ./tools/emsdk/upstream/bin/clang++ -std=c++17 build.cpp -o build.out && ./build.out dev web && python3 -m http.server --directory build/build-web

// #ifdef CONFIGURED
// #include "build/config.h"
// #else
// #define DEFAULT_CONFIG R"(
#define OUTPUT_DIR  "build/"
#define OUTPUT_NAME "breakout"
#define BUILD_NATIVE
// #define BUILD_WEB
// #define DEVELOPER
// #define OPTIMIZED
// #define DISABLE_ASSERT
#define POST_BUILD() if (!run_cmd(OUTPUT_DIR OUTPUT_NAME)) exit(1)
// #define POST_BUILD() log("Done!")
// #define POST_BUILD() if (!run_cmd("remedybg.exe", OUTPUT)) exit(1)
// #define POST_BUILD(out) if (!run_cmd("python3", "-m", "http.server", "-dir", OUTPUT_DIR)) exit(1)
// )"_str
// #endif // CONFIGURED

bool build_dependencies();
bool build_application();

static Fixed_Pool<16 * 1024> global_pool {};
int main(int argc, cstr *argv) {
    context.allocator = global_pool.allocator();
    context.logger.labels.push("config");
    NCZ_CPP_FILE_IS_SCRIPT(argc, argv);
    // NCZ_CPP_FILE_IS_SCRIPT_WITH_CONFIG(argc, argv, "build/config.h", DEFAULT_CONFIG);
    context.logger.labels.pop();
    context.logger.labels.push("build");
    
    
    if (!build_dependencies()) return 1;
    if (!build_application())  return 1;
    
    #ifdef POST_BUILD
    POST_BUILD();
    #endif
    
    return 0;
}

#define OUTPUT OUTPUT_DIR OUTPUT_NAME
#define ENTRY_POINT "source/main.cpp"
#define RAYLIB_DIR  "source/raylib/"

static const cstr raylib_compilation_unit_names[] = { "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils" };
static const Array<cstr> raylib_units = { (cstr*)raylib_compilation_unit_names, sizeof(raylib_compilation_unit_names)/sizeof(raylib_compilation_unit_names[0]) };

#define EMCC "clang", "-target", "wasm32-unknown-emscripten", "-fignore-exceptions", "-fvisibility=default", "-mllvm", "-combiner-global-alias-analysis=false", "-mllvm", "-enable-emscripten-sjlj", "-mllvm", "-disable-lsr", "-DEMSCRIPTEN", "-mllvm", "-lc", "-Xclang", "--sysroot=build/temp/sysroot", "-Xclang", "-iwithsysroot/include/fakesdl", "-Xclang", "-iwithsysroot/include/compat"

#ifdef _WIN32
#define MAYBE_WSL(cmd) (cmd).push("wsl")
#define MAYBE_EXE ".exe"
#else
#define MAYBE_WSL(cmd)
#define MAYBE_EXE
#endif

bool build_dependencies() {
    List<Process>  procs      {};
    String_Builder input_path {};
    
    List<cstr> cc {};
    List<cstr> ar {};
    
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
            
            if (Result_Bool::T != needs_update(object_file, String { input_path.data, input_path.count })) continue;
            
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
    
#ifdef BUILD_WEB
    cmd.count = 0;
    // TODO: get all loose files in source/ and pass that instead of just main
    if (Result_Bool::T == needs_update(cprint(OUTPUT_DIR, "index.html"_str), "source/main.cpp"_str)) {
        MAYBE_WSL(cmd);
        cmd.append(EMCC, "-std=c++17", "-o", OUTPUT_DIR "index.html");
        cmd.append("-DDEVELOPER", "-O3");
        cmd.append("-s", "TOTAL_MEMORY=67108864","-s", "USE_GLFW=3", "-sASSERTIONS", "-sASYNCIFY", "-sFORCE_FILESYSTEM", "--profiling");
        cmd.append(ENTRY_POINT, "-L" OUTPUT_DIR, "-lglfw", "-lraylib", "--shell-file", "source/shell.html");
        ok = run_command_sync(cmd);
    }
#endif
    
#ifdef BUILD_NATIVE
    cmd.count = 0;
#if defined(_MSC_VER)
    cmd.append(
        "cl", "/std:c++17", "/MDd", "/DDEVELOPER", "/Od",
        "/W4", "/WX", "/FC", "/Zi", "/J", "/nologo",
        "/Fe" OUTPUT MAYBE_EXE, "/Fd" OUTPUT ".pdb",
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
               "-Wpedantic", "-o", OUTPUT MAYBE_EXE, "-DPLATFORM_DESKTOP",
               "-I" RAYLIB_DIR "/src/external/glfw/include",
               "-L" OUTPUT_DIR, "-l:libraylib.a", "-lstdc++");
    
    #ifdef _WIN32
    cmd.append("-lwinmm", "-lgdi32");
    #endif
    
    ok = run_command_sync(cmd);
#endif // defined(_MSC_VER)
#endif
    
    return ok;
}
