#define NCZ_IMPLEMENTATION
#include "source/nczlib/ncz.hpp"
using namespace ncz;

#define BUILD_NATIVE
#define BUILD_WEB

#define PROJECT_NAME "test-application"

#define OUT_DIR     "./output/"
#define TMP_DIR     "./temporary/"
#define SRC_DIR     "./source/"
#define ENTRY_POINT "./source/main.cpp"

// you can bootstrap the build system with this command:
// WINDOWS: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -nostdinc++ -fno-rtti -fno-exceptions -ldbghelp -Xlinker /INCREMENTAL:NO -Xlinker /NOLOGO -Xlinker /NOIMPLIB -Xlinker /NODEFAULTLIB:msvcrt.lib -o build.exe build.cpp
// POSIX: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -nostdinc++ -fno-rtti -fno-exceptions -o build.out build.cpp
// after that you just have to run build.exe

bool BuildDependencies();
bool BuildApplication();

#ifdef _WIN32
#define NATIVE_EXE OUT_DIR PROJECT_NAME ".exe"
#define DEBUGGER "remedybg"
#else
#define NATIVE_EXE OUT_DIR PROJECT_NAME
#define DEBUGGER "gf"
#endif // _WIN32
#define WEB_EXE OUT_DIR PROJECT_NAME ".html"

int main(int argc, cstr *argv) {
    NCZ_CPP_FILE_IS_SCRIPT(argc, argv);
    context.logger.label = "build";
    context.allocator    = NCZ_TEMP;
    
    #ifdef  BUILD_NATIVE
    NCZ_ASSERT(BuildDependencies());
    #endif//BUILD_NATIVE
    NCZ_ASSERT(BuildApplication());
    
    RunCmd(/*DEBUGGER,*/NATIVE_EXE);
    // RunCmd("chromium", WEB_EXE);
    return 0;
}

NCZ_STATIC_ARRAY_LITERAL(cstr, raylib_units,
    "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils"
);

bool BuildDependencies() {
    NCZ_PUSH_STATE(context.logger.label, "build raylib");
    List<cstr> cc {};
    List<cstr> ar {};
    
    Append(&cc, "clang", "-std=c11", "-nostdlib", "-Wno-everything",
              "-I./source/raylib/external/glfw/include",
              "-DPLATFORM_DESKTOP", "-g", "-c");
    
    Append(&ar, "llvm-ar", "crs",
        #ifndef _WIN32
        TMP_DIR "libraylib.a"
        #else
        TMP_DIR "raylib.lib"
        #endif//_WIN32
    );
    
    List<Process>  procs     {};
    String_Builder inputPath {};
    for (cstr unit: raylib_units) {
        inputPath.count = 0;
        Print(&inputPath, "./source/raylib/"_str, unit, ".c\0"_str);
        cstr objectFile = SPrint(TMP_DIR, unit, ".o"_str).data;
        Push(&ar, objectFile);
        
        if (!NeedsUpdate(objectFile, inputPath.data)) continue;
        
        Append(&cc, AsCstr(inputPath), "-o", objectFile);
        Append(&cc, static_cast<cstr>(inputPath.data), "-o", objectFile);
            auto [proc, ok] = RunCommandAsync(cc);
            if (!ok) return false;
            Push(&procs, proc);
        cc.count -= 3;
    }
    
    if (!Wait(procs)) return false;
    if (procs.count)  return RunCommandSync(ar);
    return true;
}

bool BuildApplication() {
    NCZ_PUSH_STATE(context.logger.label, "build application");
    
    List<cstr> sources {};
    bool ok = TraverseFolder(SRC_DIR, [&](String path, File_Type type) {
        if (type == File_Type::FILE) Push(&sources, CopyCstr(path.data));
        return true;
    });
    if (!ok) return false;
    
    List<cstr> cmd {};
#ifdef  BUILD_NATIVE
    if (NeedsUpdate(NATIVE_EXE, sources)) {
        Log("Building native");
        cmd.count = 0;
        Append(&cmd,
            "clang", NCZ_CFLAGS, "-fsanitize=address",
            ENTRY_POINT, "-o", NATIVE_EXE, "-DPLATFORM_DESKTOP",
            "-I./source/raylib/external/glfw/include",
            "-L" TMP_DIR, "-lraylib"
        );
    #ifdef _WIN32
        Append(&cmd,
            "-D_CRT_SECURE_NO_WARNINGS",
            "-Xlinker", "/INCREMENTAL:NO",
            "-Xlinker", "/NOLOGO",
            "-Xlinker", "/NOIMPLIB",
            "-Xlinker", "/NODEFAULTLIB:msvcrt.lib",
            "-ldbghelp", "-lwinmm", "-lgdi32", "-luser32", "-lshell32"
        );
    #endif
    
        if (!RunCommandSync(cmd)) return false;
    }
#endif//BUILD_NATIVE

#ifdef  BUILD_WEB
    if (NeedsUpdate(WEB_EXE, sources)) {
        Log("Building web");
        cmd.count = 0;
        Append(&cmd, "clang", NCZ_CSTD, "-Os",
                    ENTRY_POINT, "-o", TMP_DIR PROJECT_NAME ".wasm",
                    "--target=wasm32-wasi", "--sysroot=temporary/wasi-sysroot",
                    "-nodefaultlibs", "-lc", "-lwasi-emulated-mman",
                    "-DPLATFORM_WEB", "-DNCZ_NO_OS", "-D_WASI_EMULATED_MMAN",
                    "-Wl,--allow-undefined", "-Wl,--export-all");
        if (!RunCommandSync(cmd)) return false;
        
        String_Builder out {};
        auto [wasm_bin, ok] = ReadFile(TMP_DIR PROJECT_NAME ".wasm");
        if (!ok) return false;
        
        // embed raylib and wasm application into html
        Write(&out, "<script>\n"_str);
            Write(&out, "const wasm_binary = new Uint8Array([\n"_str);
            constexpr const u64 CHUNK = 32;
            for (u64 i = 0, c = 0; i < wasm_bin.count; ++i, ++c) {
                Print(&out, static_cast<u8>(wasm_bin[i]), ","_str);
                if (c == CHUNK) { Print(&out, "\n"_str); c = 0; }
            }
            Write(&out, "\n]);\n"_str);
            if (!ReadFile(&out, "source/raylib/raylib.js")) return false;
        Write(&out, "</script>\n"_str);
        
        if (!ReadFile(&out, "source/index.html")) return false;
        // Log(String{out.data, out.count});
        return WriteFile(WEB_EXE, { out.count, out.data });
    }
#endif//BUILD_WEB

    return true;
}