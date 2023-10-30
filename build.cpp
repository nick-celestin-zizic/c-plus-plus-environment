#define NCZ_IMPLEMENTATION
#include "source/ncz.hpp"
using namespace ncz;

// Here are a few snippets I use to set up a nice development flow quickly:
// cls && clang-cl.exe /std:c++17 build.cpp && cls && build.exe dev native
// clear && ./tools/emsdk/upstream/bin/clang++ -std=c++17 build.cpp -o build.out && ./build.out dev web && python3 -m http.server --directory build/build-web

// This is a template for c++ workshops, the intended development environment is a Windows system with Visual Studio and Windows Subsystem for Linux installed. 
// If you are using something different I trust you are smart enough to edit this script slightly to make it work for your environment (its pretty straightforward)

// To get started, you will have to compile this file! I recommend you do something like this:
// clang-cl.exe /std:c++17 build.cpp
// Then 

// This build script has two targets:
// native - makes a native executable for development computer architecture
// web    - a folder that can be served or uploaded directly to websites like Itch.io as a web page

// 

enum class Build_Tool   { MSVC, CLANG };
enum class Build_Target { NATIVE, WEB, DISTRIBUTION };
cstr toolchain_names[] = {
    "Microsoft Visual C Compiler and Linker",
    "C Language Frontend for LLVM",
    "Minimalist GNU C Compiler for Windows"
};

#define PLATFORM_WINDOWS 0
#define PLATFORM_LINUX   1
#define PLATFORM_MAC     2
#define PLATFORM_WEB     3
#define ALL_PLATFORMS    4
cstr target_names[] = {
    "Windows",
    "Linux",
    "MacOS",
    "WebAssembly",
    "All Targets",
};

#if   defined(__clang__)
Build_Tool toolchain = Build_Tool::CLANG;
#elif defined(_MSC_BUILD)
Build_Tool toolchain = Build_Tool::MSVC;
#elif defined(__MINGW32__)
Build_Tool toolchain = Build_Tool::MINGW;
#else
#error "Unsupported c/c++ toolchain!"
#endif

#if   defined(_WIN32)
#define DEVELOPMENT_PLATFORM PLATFORM_WINDOWS
#elif defined(__linux__)
#define DEVELOPMENT_PLATFORM PLATFORM_LINUX
#elif defined(__apple__)
#define DEVELOPMENT_PLATFORM PLATFORM_MAC
#else
#error "Unsupported development platform!"
#endif

cstr output_name     = "breakout";
cstr entry_point     = "source/main.cpp";
cstr profile         = "dev";
cstr executable; // path to the built application executable
struct Build_Options {
    bool developer       = true;  // debug runtime checks
    bool optimized       = false; // run the optimizing compiler
    bool run_after_build = true;  // run the application after a succesful build
    Build_Target target  = Build_Target::NATIVE;
};

template <typename T>
struct Maybe {
    T value;
    bool ok;
    Maybe(T t) : value(t), ok(true) {}
    Maybe() {}
};

using Command = List<cstr>;
Maybe<Build_Options> parse_options(Array<cstr> args);
bool build_raylib(Build_Options opts);
bool build_breakout(Build_Options opts);

static Fixed_Pool<8 * 1024> global_pool {};
int main(int argc, cstr* argv) {
    context.allocator = global_pool.allocator();
    context.logger.labels.push("build script");
    
    auto opts = parse_options({argv, (usize) argc});
    
    // TODO: configure the toolchain and ensure all necesarry components are installed (emsdk env shit, mingw, and find cl.exe oh my!!)
    
    bool ok = opts.ok
        && build_raylib(opts.value)
        && build_breakout(opts.value);
    if (!ok) return -1;
    
    if (executable && opts.value.run_after_build) {
        Command cmd;
        // cmd.push("remedybg.exe");
        cmd.append(executable);
        cmd.append("Some", "Funny Arguments", "69");
        return run_command_sync(cmd) ? 0 : -1;
    }
    
    if (executable) log("Built ", executable);
}

Maybe<Build_Options> parse_options(Array<cstr> args) {
    Build_Options opts {};
    
    log("Default configuration is \"", toolchain_names[(int) toolchain], "\" targeting \"", target_names[(int) opts.target], "\"");
    
    cstr program = *args.advance();
    if (!args.count) return opts; // return the default build profile if no args are provided
    
    profile = *args.advance();
    if (strcmp("dev", profile) == 0) {
        opts.developer            = true;
        opts.optimized            = false;
        opts.run_after_build      = true;
    } else if (strcmp("release", profile) == 0) {
        opts.developer            = false;
        opts.optimized            = true;
        opts.run_after_build      = false;
    } else if (strcmp("debug_release", profile) == 0) { // debug the optimized build
        opts.developer            = true;
        opts.optimized            = true;
        opts.run_after_build      = true;
    } else if (strcmp("dist", profile) == 0) {
        assert(false && "TODO: build all targets and pack assets into build/dist");
    } else {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Unrecognized profile ", profile);
        return {};
    }
    
    log("selected build profile: ", profile);
    
    cstr* target = args.advance();
    if (!target) return opts; // fall back to default target
    
    if (strcmp("native", *target) == 0) {
        opts.target = Build_Target::NATIVE;
    } else if (strcmp("web", *target) == 0) {
        opts.target = Build_Target::WEB;
    } else if (strcmp("dist", *target) == 0) {
        opts.target = Build_Target::DISTRIBUTION;
    } else {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Unrecognized target ", *target);
        return {};
    }
    
    return opts;
}

static const cstr raylib_compilation_unit_names[] = { "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils" };
static const Array<cstr> raylib_units = { (cstr*)raylib_compilation_unit_names, sizeof(raylib_compilation_unit_names)/sizeof(raylib_compilation_unit_names[0]) };

#define RL_SRC "source/raylib/"

bool wait(Array<Process> procs) {
    bool ok = true;
    for (auto proc : procs) ok = wait(proc) && ok;
    return ok;
}

#if DEVELOPMENT_PLATFORM == PLATFORM_WINDOWS

// #define MINGW_ROOT "./tools/mingw/"

bool build_raylib(Build_Options opts) {
    if (toolchain == Build_Tool::MINGW) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: include mingw in the distribution");
        return false;
    }
    
    if (opts.target != Build_Target::NATIVE) {
       log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Only native compilation is supported on windows");
       return false;
    }
    
    bool clang = toolchain == Build_Tool::CLANG;
    List<Process> procs {};
    Command       cmd   {};
    
    cmd.append(clang ? "clang-cl.exe" : "cl.exe", "/std:c11", opts.developer ? "/MDd" : "/MD",
               "/nologo", "/W0", "/Zi", "/FS",
               "/DPLATFORM_DESKTOP", "/I" RL_SRC "external/glfw/include",
               "/Fobuild/temp/", "/c");
    
    String_Builder input_path {};
    for (cstr unit : raylib_units) {
        write(&input_path, RL_SRC, unit, ".c");
        input_path.push(0);
            cmd.push(input_path.data);
                procs.push(run_command_async(cmd));
            cmd.pop();
        input_path.count = 0;
    }
    
    if (!wait(procs)) return false;
    
    cmd.count = 0;
    cmd.append("lib", "/nologo", "/OUT:build/temp/raylib.lib");
    for (cstr unit : raylib_units) {
        String path = cprint("build/temp/", unit, ".obj");
        cmd.push(path.data);
    }
    return run_command_sync(cmd);
}

bool build_breakout(Build_Options opts) {
    if (toolchain == Build_Tool::MINGW) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: include mingw in the distribution");
        return false;
    }
    
    if (opts.target != Build_Target::NATIVE) {
       log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Only native compilation is supported on windows");
       return false;
    }
    
    bool clang = toolchain == Build_Tool::CLANG;
    Command cmd {};
    cmd.append(clang ? "clang-cl.exe" : "cl.exe", "/std:c++17");
    
    if (opts.developer) cmd.append("/MDd", "/DDEVELOPER");
    else                cmd.push("/MD");
    cmd.push(opts.optimized ? "/O2" : "/Od");
    cmd.append("/W4", "/WX", "/FC", "/Zi", "/J", "/nologo");
    
    cstr out   = cprint("build/build-windows/", output_name, "_", profile).data;
    executable = cprint(out, ".exe").data;
    
    cmd.push(cprint("/Fe", executable).data);
    cmd.push(cprint("/Fd", out, ".pdb").data);
    if (!clang) cmd.push(cprint("/Fo", out, ".obj").data);
    
    cmd.append(entry_point, "build/temp/raylib.lib");
    
    cmd.append("/link", "/INCREMENTAL:NO", "/ignore:4099");
    if (!opts.developer) cmd.append("/SUBSYSTEM:windows", "/ENTRY:mainCRTStartup");
    return run_command_sync(cmd);
}

#elif DEVELOPMENT_PLATFORM == PLATFORM_LINUX

#define EMSDK_ROOT "./tools/emsdk/upstream/emscripten/"
#define CLANG_ROOT "./tools/emsdk/upstream/bin/"

bool build_raylib(Build_Options opts) {
    if (toolchain != Build_Tool::CLANG) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: include mingw in the distribution");
        return false;
    }
    
    if (opts.target == Build_Target::NATIVE) {
       log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: native linux build");
       return false;
    }
    
    Command compile {};
    compile.append(EMSDK_ROOT "emcc", "-Wno-everything", "-Os", "-std=gnu11");
    compile.append("-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES2");
    compile.append("-I./" RL_SRC "external/glfw/include", "-c");
    
    Command archive {};
    archive.append(EMSDK_ROOT "emar", "crs", "build/temp/libraylib.a");
    
    Dynamic_Array<Process> compilations {};
    String_Builder input_path {};
    for (cstr unit : raylib_units) {
        if (strcmp(unit, "rglfw") == 0) continue;
        write(&input_path, RL_SRC, unit, ".c\0"_str);
        compile.append(input_path.data, "-o");
            String_Builder object_file {};
            write(&object_file, "build/temp/", unit, ".o\0"_str);
            archive.push(object_file.data);
            compile.push(object_file.data);
                compilations.push(run_command_async(compile));
            compile.pop();
        compile.pop(); compile.pop();
        input_path.count = 0;
    }
    
    if (wait(compilations)) return run_command_sync(archive);
    return false;
}

bool build_breakout(Build_Options opts) {
    if (toolchain != Build_Tool::CLANG) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: include mingw in the distribution");
        return false;
    }
    
    if (opts.target == Build_Target::NATIVE) {
       log_ex(Log_Level::NORMAL, Log_Type::ERRO, "TODO: native linux build");
       return false;
    }
    
    Command compile {};
    compile.append(EMSDK_ROOT "em++", "-std=c++17", "-o", "build/build-web/index.html");
    if (opts.developer) compile.push("-DDEVELOPER");
    if (opts.optimized) compile.push("-O3");
    compile.append("-s", "TOTAL_MEMORY=67108864","-s", "USE_GLFW=3", "-sASSERTIONS", "-sASYNCIFY", "-sFORCE_FILESYSTEM", "--profiling");
    compile.append("source/main.cpp", "-L./build/temp", "-lglfw", "-lraylib", "--shell-file", "build/static/shell.html");
    return run_command_sync(compile);
}

#else
#error "TODO: unsupported dev env"
#endif
