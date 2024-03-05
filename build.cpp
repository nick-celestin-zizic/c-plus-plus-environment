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


cstr copy_cstr(cstr src, Allocator allocator = context.allocator);

template <typename T>
struct Result {
    T value; 
    bool ok;
    Result(T v) : value(v), ok(true) {};
    Result()    : ok(false) {};
};

bool write_file(cstr path, String data);
bool read_file(String_Builder *stream, cstr path);
Result<String> read_file(cstr path);

enum class File_Type { FILE, FOLDER, LINK, OTHER };
Result<File_Type> get_file_type(cstr path);
Result<Array<cstr>> read_folder(cstr parent);

// F is a function/closure of type (String path, File_Type type) -> bool
// While path is a String, path.data is a null terminated cstr for convenience.
// This function allocates all paths with temporary storage so if you want to keep
// a path you are visiting you need to use copy_string or copy_cstr
template <typename F>
bool traverse_folder(cstr path, F visit_proc) {
    Array<cstr> files;
    String_Builder full_path {};
    { NCZ_PUSH_STATE(context.allocator, NCZ_TEMP);
        auto result = read_folder(path);
        if (!result.ok) return false;
        files = result.value;
        format(&full_path, path);
    }
    
    u64 base_path_len = full_path.count;
    for (cstr f: files) if (!_visit(f, visit_proc, &full_path, &base_path_len)) return false;
    
    return true;
}

template <typename F>
static bool _visit(cstr file, F visit_proc, String_Builder *full_path, u64 *base_path_len) {
    if ((strcmp(".", file) == 0) || (strcmp("..", file) == 0)) return true;
    full_path->count = *base_path_len;
    print(full_path, file, "\0"_str);
    
    auto [type, ok] = get_file_type(full_path->data);
    if (!ok) return false;
    ok = visit_proc(String{full_path->data, full_path->count-1}, type);
    if (!ok) return false;
    
    if (type == File_Type::FOLDER) {
        auto [children, ok] = read_folder(full_path->data);
        if (!ok) return false;
        (*full_path)[full_path->count-1] = '/'; // replace null terminator to make it a proper folder
        NCZ_PUSH_STATE(*base_path_len, full_path->count); // hell yeah.....
        for (cstr c: children) if (!_visit(c, visit_proc, full_path, base_path_len)) return false;
    }
    
    return true;
}

// you can bootstrap the build system with this command:
// WINDOWS: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -nostdinc++ -fno-rtti -fno-exceptions -ldbghelp -Xlinker /INCREMENTAL:NO -Xlinker /NOLOGO -Xlinker /NOIMPLIB -Xlinker /NODEFAULTLIB:msvcrt.lib -o build.exe build.cpp
// POSIX: clang -std=c++17 -Wall -Wextra -Wpedantic -Werror -g -nostdinc++ -fno-rtti -fno-exceptions -o build.out build.cpp
// after that you just have to run build.exe

bool build_dependencies();
bool build_application();

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
    context.logger.labels.push("build");
    context.allocator = NCZ_TEMP;
    
    #ifdef  BUILD_NATIVE
    assert(build_dependencies());
    #endif//BUILD_NATIVE
    assert(build_application());
    
    // run_cmd(DEBUGGER, NATIVE_EXE);
    // run_cmd("chromium", WEB_EXE);
    return 0;
}

NCZ_STATIC_ARRAY_LITERAL(cstr, raylib_units,
    "raudio", "rcore", "rglfw", "rmodels", "rshapes", "rtext", "rtextures", "utils"
);

bool build_dependencies() {
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
    for (cstr unit: raylib_units) {
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

// struct Base16 { u64 i; };
// void format(String_Builder *sb, Base16 h) {
//     char buf[256];
//     auto len = static_cast<u64>(snprintf(buf, sizeof(buf), "%02X", static_cast<u8>(h.c)));
//     assert(len > 0);
//     while (sb->capacity < sb->count + len) sb->expand();
//     assert(sb->capacity >= sb->count + len);
//     memcpy(sb->data + sb->count, buf, len);
//     sb->count += len;
// }

bool build_application() {
    List<cstr> sources {};
    bool ok = traverse_folder(SRC_DIR, [&](String path, File_Type type) {
        if (type == File_Type::FILE) sources.push(copy_cstr(path.data));
        return true;
    });
    if (!ok) return false;
    
    List<cstr> cmd {};
#ifdef  BUILD_NATIVE
    if (needs_update(NATIVE_EXE, sources)) {
        cmd.count = 0;
        cmd.append("clang", NCZ_CFLAGS, "-fsanitize=address",
                   ENTRY_POINT, "-o", NATIVE_EXE, "-DPLATFORM_DESKTOP",
                   "-I./source/raylib/external/glfw/include",
                   "-L" TMP_DIR, "-lraylib", NCZ_LDFLAGS);
    #ifdef _WIN32
        cmd.append("-lwinmm", "-lgdi32", "-luser32", "-lshell32");
    #endif
    
        if (!run_command_sync(cmd)) return false;
    }
#endif//BUILD_NATIVE

#ifdef  BUILD_WEB
    if (needs_update(WEB_EXE, sources)) {
        cmd.count = 0;
        cmd.append("clang", NCZ_RCFLAGS, "-Os",
                    ENTRY_POINT, "-o", TMP_DIR PROJECT_NAME ".wasm",
                    "--target=wasm32-wasi", "--sysroot=temporary/wasi-sysroot",
                    "-nodefaultlibs", "-lc", "-lwasi-emulated-mman",
                    "-DPLATFORM_WEB", "-DNCZ_NO_MULTIPROCESSING", "-D_WASI_EMULATED_MMAN", 
                    "-Wl,--allow-undefined", "-Wl,--export-all");
        if (!run_command_sync(cmd)) return false;
        
        String_Builder out {};
        auto [wasm_bin, ok] = read_file(TMP_DIR PROJECT_NAME ".wasm");
        if (!ok) return false;
        
        // embed raylib and wasm application into html
        format(&out, "<script>\n"_str);
            format(&out, "const wasm_binary = new Uint8Array([\n"_str);
            constexpr const u64 CHUNK = 32;
            for (u64 i = 0, c = 0; i < wasm_bin.count; ++i, ++c) {
                print(&out, static_cast<u8>(wasm_bin[i]), ","_str);
                if (c == CHUNK) { print(&out, "\n"_str); c = 0; }
            }
            format(&out, "\n]);\n"_str);
            if (!read_file(&out, "source/raylib/raylib.js")) return false;
        format(&out, "</script>\n"_str);
        
        if (!read_file(&out, "source/index.html")) return false;
        // log(String{out.data, out.count});
        return write_file(WEB_EXE, String { out.data, out.count });
    }
#endif//BUILD_WEB

    return true;
}

Result<String> read_file(cstr path) {
    String_Builder sb {};
    if (!read_file(&sb, path)) return {};
    return String{sb.data, sb.count};
}

bool read_file(String_Builder *stream, cstr path) {
    #define CHECK(x, e) if (x) {                                  \
    log_error("Could not read file ", path, ": ", strerror((e))); \
    return false;                                                 \
    }
    
    FILE *f = fopen(path, "rb");
    CHECK(f == NULL, errno);
    NCZ_DEFER(fclose(f));
    CHECK(fseek(f, 0, SEEK_END) < 0, errno);
    
    long m = ftell(f);
    CHECK(m < 0, errno);
    CHECK(fseek(f, 0, SEEK_SET) < 0, errno);

    size_t new_count = stream->count + m;
    if (new_count > stream->capacity) {
        stream->data     = static_cast<char*>(resize(stream->data, new_count, stream->count));
        stream->capacity = new_count;
    }

    fread(stream->data + stream->count, m, 1, f);
    int err = ferror(f);
    CHECK(err, err);
    stream->count = new_count;
    
    return true;
    #undef CHECK
}

bool write_file(cstr path, String data) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        log_error("Could not open ", path, ": ", strerror(errno));
        return false;
    }
    NCZ_DEFER(fclose(f));
    
    char *buf = data.data;
    int  size = data.count;
    int error = 0;
    while (size > 0) {
        usize n = fwrite(buf, 1, size, f);
        error   = ferror(f);
        if (error) {
            log_error("Could not write data to ", path, ": ", strerror(error));
            return false;
        }
        size -= n;
        buf  += n;
    }
    
    return true;
}

cstr copy_cstr(cstr src, Allocator allocator) {
    auto len = static_cast<u64>(strlen(src));
    auto dst = static_cast<char*>(allocate(len+1, allocator));
    memcpy(dst, src, len);
    dst[len] = 0;
    return dst;
}

#include <dirent.h>
Result<Array<cstr>> read_folder(cstr parent) {
    List<cstr> children {};
    
    DIR *dir = opendir(parent);
    if (dir == nullptr) {
        log_error("Could not open folder ", parent, ": ", strerror(errno));
        return {};
    }
    NCZ_DEFER(closedir(dir));
    
    errno = 0;
    
    struct dirent *ent = readdir(dir);
    while (ent != nullptr) {
        children.push(copy_cstr(ent->d_name, NCZ_TEMP));
        ent = readdir(dir);
    }
    
    if (errno != 0) {
        log_error("Could not read folder ", parent, ": ", strerror(errno));
        return {};
    }
    
    return children;
}

Result<File_Type> get_file_type(cstr path) {
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        log_error("Could not get stat of ", path, ": ", strerror(errno));
        return {};
    }

    switch (statbuf.st_mode & S_IFMT) {
        case S_IFDIR:  return File_Type::FOLDER;
        case S_IFREG:  return File_Type::FILE;
        case S_IFLNK:  return File_Type::LINK;
        default:       return File_Type::OTHER;
    }
}

void format(String_Builder *sb, File_Type ft) {
    format(sb, "File_Type::"_str);
    switch (ft) {
    case File_Type::FILE:   format(sb, "FILE"_str);   break;
    case File_Type::FOLDER: format(sb, "FOLDER"_str); break;
    case File_Type::LINK:   format(sb, "LINK"_str);   break;
    case File_Type::OTHER:  format(sb, "OTHER"_str);  break;
    }
}