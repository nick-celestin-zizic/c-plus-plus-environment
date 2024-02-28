#define NCZ_IMPLEMENTATION
#include "source/nczlib/ncz.hpp"
using namespace ncz;

#define BUILD_NATIVE
#define BUILD_WEB

#define PROJECT_NAME "test-application"

#define OUT_DIR     "./output/"
#define TMP_DIR     "./temporary/"
#define ENTRY_POINT "./source/main.cpp"

// you can bootstrap the build system with this command:
// WINDOWS: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsanitize=address -g -nostdinc++ -fno-rtti -fno-exceptions -ldbghelp -Xlinker /INCREMENTAL:NO -Xlinker /NOLOGO -Xlinker /NOIMPLIB -Xlinker /NODEFAULTLIB:msvcrt.lib -o build.exe build.cpp
// POSIX: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsanitize=address -g -nostdinc++ -fno-rtti -fno-exceptions -o build.out build.cpp
// after that you just have to run build.exe

bool build_dependencies();
bool build_application();

int main(int argc, cstr *argv) {
    NCZ_CPP_FILE_IS_SCRIPT(argc, argv);
    context.logger.labels.push("build");
    
    #ifdef  BUILD_NATIVE
    assert(build_dependencies());
    #endif//BUILD_NATIVE
    assert(build_application());
    
    return 0;
}

#ifdef _WIN32
#define EXE OUT_DIR PROJECT_NAME ".exe"
#else
#define EXE OUT_DIR PROJECT_NAME
#endif // _WIN32

static const cstr raylib_compilation_unit_names[] = {
    "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils"
};
static const Array<cstr> raylib_units = {
    (cstr*)raylib_compilation_unit_names,
    sizeof(raylib_compilation_unit_names)/sizeof(raylib_compilation_unit_names[0])
};

bool build_dependencies() {
    // auto old = context.allocator;
    // context.allocator = NCZ_TEMP;
    // NCZ_DEFER(context.allocator = old);
    // NCZ_DEFER(context.temporary_storage.reset());
    
    List<cstr> cc {};
    List<cstr> ar {};
    
    cc.append("clang", "-std=c11", "-nostdlib", "-Wno-everything",
              "-I./source/raylib/external/glfw/include",
              "-DPLATFORM_DESKTOP", "-g", "-c");
    
    ar.append("llvm-ar", "crs",
        #ifndef _WIN32
        TMP_DIR "libraylib.a"
        #else
        TMP_DIR "raylib.lib"
        #endif//_WIN32
    );
    
    List<Process>  procs      {};
    String_Builder input_path {};
    for (cstr unit : raylib_units) {
        input_path.count = 0;
        print(&input_path, "./source/raylib/"_str, unit, ".c\0"_str);
        cstr object_file = cprint(TMP_DIR, unit, ".o"_str).data;
        ar.push(object_file);
        
        if (!needs_update(object_file, input_path.data)) continue;
        
        cc.append(input_path.data, "-o", object_file);
            procs.push(run_command_async(cc));
        cc.count -= 3;
    }
    
    if (!wait(procs)) return false;
    if (procs.count)  return run_command_sync(ar);
    return true;
}

bool read_file(String_Builder *out, cstr path);
String read_file(cstr path);
bool write_file(cstr path, String data);

bool build_application() {
    List<cstr> cmd {};
#ifdef  BUILD_NATIVE
    cmd.count = 0;
    cmd.append("clang", NCZ_CFLAGS, ENTRY_POINT, "-o", EXE, "-DPLATFORM_DESKTOP",
               "-I./source/raylib/external/glfw/include",
               "-L" TMP_DIR, "-lraylib", NCZ_LDFLAGS);
#ifdef _WIN32
    cmd.append("-lwinmm", "-lgdi32", "-luser32", "-lshell32");
#else
    cmd.append("-lm");
#endif

    if (!run_command_sync(cmd)) return false;
#endif//BUILD_NATIVE

#ifdef  BUILD_WEB
    cmd.count = 0;
    cmd.append("clang", "-std=c++17", NCZ_RCFLAGS, ENTRY_POINT, "-Os",
        "--target=wasm32-wasi", "--sysroot=temporary/wasi-sysroot",
        "-nodefaultlibs", "-L./temporary/wasi-sysroot/lib/",
        "-DNCZ_NO_MULTIPROCESSING",
        "-DWEB_BUILD",
        "-D_WASI_EMULATED_MMAN", "-lwasi-emulated-mman",
        "-lc", "-Wl,--allow-undefined", "-Wl,--export-all",
        "-o", TMP_DIR PROJECT_NAME ".wasm"
    );
    
    if (!run_command_sync(cmd)) return false;
    
    String_Builder out {};
    String wasm_bin = read_file(TMP_DIR PROJECT_NAME ".wasm");
    if (!wasm_bin.count) return false;
    
    // embed raylib and wasm application into html
    // NOTE: I am not splittting the binary into lines because my latop's and my phone's
    //       text editor could handle a 300k character line with no problem so the
    //       additional complexity is not worth it just to support bad text editors.
    format(&out, "<script>\n"_str);
        format(&out, "const wasm_bin = new Uint8Array(["_str);
        for (char c : wasm_bin) print(&out, static_cast<u8>(c), ","_str);
        format(&out, "]);\n"_str);
        if (!read_file(&out, "source/raylib/raylib.js")) return false;
    format(&out, "</script>\n"_str);
    
    if (!read_file(&out, "source/index.html")) return false;
    // log(String { out.data, out.count });
    return write_file(OUT_DIR PROJECT_NAME ".html", String { out.data, out.count });
#endif//BUILD_WEB

    return true;
}

String read_file(cstr path) {
    String_Builder sb {};
    read_file(&sb, path);
    return {sb.data, sb.count};
}

bool read_file(String_Builder *out, cstr path) {
    // char buffer[256];
    // strerror_s(buffer, sizeof(buffer), (e));
    #define CHECK(x, e) if (x) {                                \
    log_error("Could not read file ", path, ": ", strerror(e)); \
    return false;                                               \
    }
    
    FILE *f = fopen(path, "rb");
    // errno_t err = fopen_s(&f, path, "rb");
    CHECK(f == NULL, errno);
    NCZ_DEFER(fclose(f));
    CHECK(fseek(f, 0, SEEK_END) < 0, errno);
    
    long m = ftell(f);
    CHECK(m < 0, errno);
    CHECK(fseek(f, 0, SEEK_SET) < 0, errno);

    size_t new_count = out->count + m;
    if (new_count > out->capacity) {
        out->data     = static_cast<char*>(resize(out->data, new_count, out->count));
        out->capacity = new_count;
    }

    fread(out->data + out->count, m, 1, f);
    int err = ferror(f);
    CHECK(err, err);
    out->count = new_count;
    
    return true;
    #undef CHECK
}

bool write_file(cstr path, String data) {
    // char buffer[256];
    // strerror_s(buffer, sizeof(buffer), (e));
    #define CHECK(x, e) if (x) {                                    \
    log_error("Could not write data to ", path, ": ", strerror(e)); \
    return false;                                                   \
    }
    
    FILE *f = fopen(path, "wb");
    CHECK(f == NULL, errno);
    NCZ_DEFER(fclose(f));
    
    char *buf = data.data;
    int  size = data.count;
    int error = 0;
    while (size > 0) {
        usize n = fwrite(buf, 1, size, f);
        error   = ferror(f);
        CHECK(error, error);
        size -= n;
        buf  += n;
    }
    
    return true;
    #undef CHECK
}