#ifndef NCZ_HPP_
#define NCZ_HPP_

// c standard library
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef NCZ_NO_OS
#ifdef _WIN32

#else // POSIX
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#endif
#endif//NCZ_NO_OS

namespace ncz {
// macros
#define NCZ_HERE ::ncz::Source_Location {__FILE__, __LINE__}
#define NCZ_TEMP ::ncz::Allocator { ::ncz::PoolAllocatorProc, &::ncz::context.temporaryStorage }

// TODO: document these funky macros
#define NCZ_CONCAT(x, y)  NCZ__CONCAT_(x, y)
#define NCZ_GENSYM(x)     NCZ_CONCAT(x, __COUNTER__)
#define NCZ_DEFER(code) ::ncz::Defer NCZ_GENSYM(_defer_) {[&](){code;}}
#define NCZ_PUSH_STATE(variable, value) NCZ__PUSH_STATE_((variable), (value), NCZ_GENSYM(_pushedVariable_))
#define NCZ_SAVE_STATE(variable) NCZ__SAVE_STATE_((variable), NCZ_GENSYM(_savedVariable_))
#define NCZ_STATIC_ARRAY_LITERAL(type, name, ...)            \
static constexpr const type _name_##data[] = { __VA_ARGS__ }; \
static constexpr const Array<type> name = { sizeof(_name_##data)/sizeof(_name_##data[0]), (type*)_name_##data }

// These are helper macros and should not be used
#define NCZ__CONCAT_(x, y) x##y
#define NCZ__PUSH_STATE_(var, val, old) auto old = (var); (var) = (val); NCZ_DEFER((var) = old)
#define NCZ__SAVE_STATE_(var, old) auto old = (var); NCZ_DEFER((var) = old)

// Basic Types
typedef const char* cstr;
typedef float       f32;
typedef double      f64;
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;
typedef uintptr_t   usize;

template <typename T>
struct Array {
    usize count = 0;
    T    *data  = nullptr;
    T& operator[](usize index);
    constexpr T *begin() const;// { return data; };
    constexpr T *end()   const;// { return data + count; };
};

using String = Array<char>;
String operator ""_str(cstr data, usize count);
bool StringIsCstr(String str);
cstr AsCstr(String str);
cstr CopyCstr(cstr src);

struct Source_Location { cstr file; s64 line; };

template <typename T>
struct Result {
    T value = T();
    bool ok = false;
    Result(T v) : value(v), ok(true) {};
    Result()    : ok(false) {};
};

template <typename F>
struct Defer {
    F f;
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
};

// Allocator Interface
enum class Allocator_Mode {
    ALLOCATE = 0,
    RESIZE   = 1,
    DISPOSE  = 2,
};
using Allocator_Proc = void* (*)(Allocator_Mode mode, usize size, usize oldSize, void* oldMemory, void* allocatorData);
struct Allocator {
    Allocator_Proc proc = nullptr;
    void *data          = nullptr;
};


// Pooling Allocators
#ifndef NCZ_POOL_ALIGNMENT
#define NCZ_POOL_ALIGNMENT 16
#endif//NCZ_POOL_ALIGNMENT

#ifndef NCZ_POOL_DEFAULT_BLOCK_SIZE
#define NCZ_POOL_DEFAULT_BLOCK_SIZE 4096
#endif//NCZ_POOL_DEFAULT_BLOCK_SIZE

// TODO: void *highWaterMark;
struct Pool {
    struct Marker {
        void **block  = nullptr;
        usize  offset = 0;
    };
    
    usize     blockSize      = 0;
    Allocator blockAllocator = {};
    Marker    mark           = {};
    
    // linked lists of memory blocks where we embed the next pointer at the beginning of the block
    void **blocks    = nullptr;
    void **oversized = nullptr;
};

void *Get(Pool *p, usize numBytes);
void  Reset(Pool *p);
void *PoolAllocatorProc(Allocator_Mode mode, usize size, usize oldSize, void* oldMemory, void* allocator_data);

// Logger
enum class Log_Level {
    NORMAL  = 0,
    VERBOSE = 1,
    TRACE   = 2,
};
enum class Log_Type {
    INFO = 0,
    ERRO = 1,
    WARN = 2,
};
using Logger_Proc = void (*)(String message, Log_Level level, Log_Type type, void* loggerData);
struct Logger {
    Logger_Proc proc = nullptr;
    void *data       = nullptr;
    cstr label       = nullptr;
};

template <typename ...Args>
void LogEx(Log_Level level, Log_Type type, Args... args);

template <typename ...Args>
void Log(Args... args);

template <typename ...Args>
void LogInfo(Args... args);

template <typename ...Args>
void LogError(Args... args);

// Context
extern Allocator crtAllocator;
void *CrtAllocatorProc(Allocator_Mode mode, usize size, usize oldSize, void* oldMemory, void* userData);

extern Logger crtLogger;
void CrtLoggerProc(String message, Log_Level level, Log_Type type, void* userData);

struct Context {
    Allocator allocator        = crtAllocator;
    Logger    logger           = crtLogger;
    bool      handlingAssert   = false;
    Pool      temporaryStorage = {32 * 1024, crtAllocator};
};

extern thread_local Context context;

void *Allocate(usize size, Allocator allocator = context.allocator);
void *Resize(void *memory, usize size, usize oldSize, Allocator allocator = context.allocator);
void  Dispose(void *memory, Allocator allocator = context.allocator);

// Container Types
template <typename T>
struct List : Array<T> {
    usize capacity      = 0;
    Allocator allocator = {};
};

template<typename T>
void Push(List<T> *xs, T x);
template <typename T, typename ... Args>
void Append(List<T> *xs, Args ... args);
template<typename T>
void Extend(List<T> *xs, Array<T> ys);

using String_Builder = List<char>;
void Write(String_Builder *sb, s64 i);
void Write(String_Builder *sb, void *p);
void Write(String_Builder *sb, String str);
void Write(String_Builder *sb, cstr str);
void Write(String_Builder *sb, Source_Location loc);

template <typename T>
void Write(String_Builder *sb, Array<T> list);

// Printing
template <typename ... Args>
void Print(String_Builder *sb, Args ... args);
template <typename ... Args>
String SPrint(Args ... args);
template <typename ... Args>
String TPrint(Args ... args);

// Assertions and Stack Traces
#define NCZ_ASSERT(cond) if (!(cond)) ::ncz::HandleFailedAssertion(#cond, NCZ_HERE)
void HandleFailedAssertion(cstr repr, Source_Location loc);

// C++17 Compiler
#ifndef NCZ_NO_CC

#define NCZ_CSTD "-std=c++17", "-nostdinc++", "-fno-rtti", "-fno-exceptions"
#define NCZ_CFLAGS NCZ_CSTD, "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-g"
#define NCZ_CC(binary_path, source_path) "clang", NCZ_CFLAGS, "-o", binary_path, source_path

// stolen nob Go Rebuild Urself™ Technology
// from: https://github.com/tsoding/musializer/blob/master/src/nob.h#L260
#define NCZ_CPP_FILE_IS_SCRIPT(argc, argv) ::ncz::ReloadCppScript({static_cast<usize>((argc)), (argv)}, __FILE__);
void ReloadCppScript(Array<cstr> args, cstr src);

#endif//NCZ_NO_CC


// Multiprocessing
using Process = u64;

bool Wait(Process proc);
bool Wait(Array<Process> proc);
Result<Process> RunCommandAsync(Array<cstr> args, bool trace = true);
bool RunCommandSync(Array<cstr> args, bool trace = true);

template <typename ... Args>
bool RunCmd(Args ... args);

// Working with files
bool NeedsUpdate(cstr outputPath, Array<cstr> inputPaths);
bool NeedsUpdate(cstr outputPath, cstr inputPaths);
bool RenameFile(cstr oldPath, cstr newPath);

bool WriteFile(cstr path, String data);
bool ReadFile(String_Builder *stream, cstr path);
Result<String> ReadFile(cstr path);

enum class File_Type { FILE, FOLDER, LINK, OTHER };
Result<Array<cstr>> ReadFolder(cstr parent);
Result<File_Type> GetFileType(cstr path);
// template <typename F> // F :: (String path, File_Type type) -> bool 
template <typename F> bool TraverseFolder(cstr path, F visit_proc);


}// namespace ncz
#endif//NCZ_HPP_

#ifdef NCZ_IMPLEMENTATION
namespace ncz {
thread_local Context context {};
Logger    crtLogger    { CrtLoggerProc,    nullptr, nullptr };
Allocator crtAllocator { CrtAllocatorProc, nullptr };

template <typename T>
T& Array<T>::operator[](usize index) { NCZ_ASSERT(index < this->count); return this->data[index]; }
template <typename T>
constexpr T *Array<T>::begin() const { return data; };
template <typename T>
constexpr T *Array<T>::end()   const { return data + count; };

void *Get(Pool *p, usize numBytes) {
    if (!p->blockAllocator.proc) p->blockAllocator = context.allocator;
    if (!p->blockSize) p->blockSize = NCZ_POOL_DEFAULT_BLOCK_SIZE;
    if (!p->blocks) {
        p->blocks  = static_cast<void**>(Allocate(sizeof(void*) + p->blockSize, p->blockAllocator));
        *p->blocks = nullptr;
        p->mark    = { p->blocks, 0 };
    }

    if (numBytes < p->blockSize) {
        u8* currentBlock     = ((u8*)p->mark.block)+sizeof(void*);
        u8* memory           = (u8*)(((u64)((currentBlock+p->mark.offset) + NCZ_POOL_ALIGNMENT-1)) & ((u64)~(NCZ_POOL_ALIGNMENT - 1)));
        u8* currentBlockEnd  = currentBlock + p->blockSize;

        if (memory + numBytes > currentBlockEnd) {
            // we need to cycle in a new block
            if (!*(p->mark.block)) {
                *p->mark.block = Allocate(sizeof(void*) + p->blockSize + NCZ_POOL_ALIGNMENT - 1, p->blockAllocator);
                *((void**)*p->mark.block) = nullptr;
            }
            p->mark = { (void**)*p->mark.block, 0 };
            currentBlock = ((u8*)p->mark.block) + sizeof(void*);
            memory = (u8*)(((u64)((currentBlock) + NCZ_POOL_ALIGNMENT - 1)) & ((u64)~(NCZ_POOL_ALIGNMENT - 1)));
            currentBlockEnd = currentBlock + p->blockSize;
        }

        NCZ_ASSERT(memory < currentBlockEnd);
        NCZ_ASSERT(memory + numBytes <= currentBlockEnd);
        NCZ_ASSERT(memory >= currentBlock);

        p->mark.offset = (memory + numBytes) - currentBlock;
        return memory;
    } else {
        auto newBlock = (void**)Allocate(sizeof(void*)+numBytes+NCZ_POOL_ALIGNMENT-1, p->blockAllocator);
        *newBlock = p->oversized;
        p->oversized = newBlock;
        return &newBlock[1];
    }
}

void Reset(Pool *p) {
    p->mark = {p->blocks, 0};
    for (void** block = p->oversized; block != nullptr; block = (void**)*block) {
        Dispose(block, p->blockAllocator);
    }
    
    // TODO: parameter for this?
    for (void** block = p->blocks; block != nullptr; block = (void**)*block) {
        memset(&block[1], 0xcd, p->blockSize);
    }
}

void *PoolAllocatorProc(Allocator_Mode mode, usize size, usize oldSize, void* oldMemory, void* allocatorData) {
    auto pool = static_cast<Pool*>(allocatorData);
    NCZ_ASSERT(pool != nullptr);
    switch (mode) {
    case Allocator_Mode::ALLOCATE: return Get(pool, size);
    case Allocator_Mode::DISPOSE:  return nullptr;
    case Allocator_Mode::RESIZE: {
        void *newMemory = Get(pool, size);
        memcpy(newMemory, oldMemory, oldSize);
        return newMemory;
    }
    }
    return nullptr;
}

String operator ""_str(cstr data, usize count) { return { count, (char*) data }; }
// NOTE: this is kinda dangerous, if your string ends on a page boundary or is right
// next to memory you don't have access to you will get a segmentation fault
bool StringIsCstr(String str) { return *(str.data+str.count) == '\0'; }
cstr AsCstr(String str) {
    NCZ_ASSERT(StringIsCstr(str));
    return static_cast<cstr>(str.data);
}
cstr CopyCstr(cstr src) {
    auto len = static_cast<u64>(strlen(src));
    auto dst = static_cast<char*>(Allocate(len+1));
    memcpy(dst, src, len);
    dst[len] = 0;
    return dst;
}

template <typename ...Args>
void LogEx(Log_Level level, Log_Type type, Args... args) {
    // TODO: this could work if Pool had a constructor
    // NCZ_SAVE_STATE(context.temporaryStorage.mark);
    String_Builder sb {}; memset(&sb, 0, sizeof(String_Builder));
    sb.allocator = NCZ_TEMP;
    if (context.logger.label) {
        Push(&sb, '[');
        Write(&sb, context.logger.label);
        Extend(&sb, "] "_str);
    }
    (Write(&sb, args), ...);
    Extend(&sb, "\n\0"_str); // when in Rome...
    context.logger.proc({sb.count - 1, sb.data}, level, type, context.logger.data);
}
template <typename ...Args>
void Log(Args... args) { LogEx(Log_Level::NORMAL,  Log_Type::INFO, args...); }
template <typename ...Args>
void LogInfo(Args... args) { LogEx(Log_Level::VERBOSE, Log_Type::INFO, args...); }
template <typename ...Args>
void LogError(Args... args) { LogEx(Log_Level::NORMAL, Log_Type::ERRO, args...); }

void CrtLoggerProc(String message, Log_Level level, Log_Type type, void* userData) {
    (void) userData;
    (void) level;
    
    #define ANSI_COLOR_RED     "\x1b[31m"
    #define ANSI_COLOR_YELLOW  "\x1b[33m"
    #define ANSI_COLOR_RESET   "\x1b[0m"

    auto sink = type == Log_Type::ERRO ? stderr : stdout;
    switch (type) {
    case Log_Type::INFO:
        fwrite(message.data, 1, message.count, sink);
        break;
    case Log_Type::ERRO:
        fprintf(sink, ANSI_COLOR_RED "%s" ANSI_COLOR_RESET, message.data);
        break;
    case Log_Type::WARN:
        fprintf(sink, ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET, message.data);
        break;
    }
    
    #undef ANSI_COLOR_RED    
    #undef ANSI_COLOR_YELLOW 
    #undef ANSI_COLOR_RESET 
}

void *Allocate(usize size, Allocator allocator) {
    if (!size) return nullptr;
    NCZ_ASSERT(allocator.proc != nullptr);
    void *mem = allocator.proc(Allocator_Mode::ALLOCATE, size, 0, nullptr, allocator.data);
    NCZ_ASSERT(mem);
    // You can do custom checks and logging here!!
    return mem;
}

void *Resize(void *memory, usize size, usize oldSize, Allocator allocator) {
    NCZ_ASSERT(allocator.proc != nullptr);
    void *mem = allocator.proc(Allocator_Mode::RESIZE, size, oldSize, memory, allocator.data);
    NCZ_ASSERT(mem);
    // You can do custom checks and logging here!!
    return mem;
}

void Dispose(void *memory, Allocator allocator) {
    NCZ_ASSERT(allocator.proc != nullptr);
    allocator.proc(Allocator_Mode::DISPOSE, 0, 0, memory, allocator.data);
    // You can do custom checks and logging here!!
}

void *CrtAllocatorProc(Allocator_Mode mode, usize size, usize oldSize, void* oldMemory, void* userData) {
    (void) userData;
    (void) oldSize;
    switch (mode) {
    case Allocator_Mode::ALLOCATE: return malloc(size);
    case Allocator_Mode::RESIZE:   return realloc(oldMemory, size);
    case Allocator_Mode::DISPOSE:         free(oldMemory);
    }
    return nullptr;
}

template<typename T> static
void Grow(List<T> *xs) {
    if (!xs->allocator.proc) xs->allocator = context.allocator;
    usize new_capacity = xs->data ? 2 * xs->capacity : 256;
    xs->data = static_cast<T*>(
        Resize(xs->data, new_capacity*sizeof(T), xs->capacity*sizeof(T), xs->allocator)
    );
    NCZ_ASSERT(xs->data);
    xs->capacity = new_capacity;
}

template<typename T>
void Push(List<T> *xs, T x) {
    if (xs->count >= xs->capacity) Grow(xs);
    NCZ_ASSERT(xs->data);
    xs->data[xs->count++] = x;
}

template <typename T, typename ... Args>
void Append(List<T> *xs, Args ... args) { (Push(xs, args), ...); }

template<typename T>
void Extend(List<T> *xs, Array<T> ys) {
    while (xs->count+ys.count >= xs->capacity) Grow(xs);
    memcpy(xs->data+xs->count, ys.data, ys.count*sizeof(T));
    xs->count += ys.count;
}

void Write(String_Builder *sb, s64 i) {
    constexpr const auto MAX_LEN = 32;
    char buf[MAX_LEN];
    auto len = static_cast<usize>(snprintf(buf, MAX_LEN, "%" PRId64, i));
    Extend(sb, {len, buf});
}
void Write(String_Builder *sb, void *p) {
    constexpr const auto MAX_LEN = 64;
    char buf[MAX_LEN];
    auto len = static_cast<usize>(snprintf(buf, MAX_LEN, "%p", p));
    Extend(sb, {len, buf});
}
void Write(String_Builder *sb, String str) { Extend(sb, str); }
void Write(String_Builder *sb, cstr data)  { Extend(sb, { strlen(data), const_cast<char*>(data) }); }
void Write(String_Builder *sb, Source_Location loc) {
    Write(sb, loc.file);
    Push(sb, ':');
    Write(sb, loc.line);
    Push(sb, ':');
}
template <typename T>
void Write(String_Builder *sb, Array<T> xs) {
    Push(sb, '[');
    for (usize i = 0; i < xs.count; ++i) {
        Write(sb, xs[i]);
        if (i != xs.count-1) Extend(sb, ", "_str);
    }
    Push(sb, ']');
}

// Printing
template <typename ... Args>
void Print(String_Builder *sb, Args ... args) { (Write(sb, args), ...); }
template <typename ... Args>
String SPrint(Args ... args) {
    String_Builder sb {};
    (Write(&sb, args), ...);
    Push(&sb, '\0'); // when in Rome...
    return {sb.count-1, sb.data};
}
template <typename ... Args>
String TPrint(Args ... args) {
    String_Builder sb {};
    sb.allocator = NCZ_TEMP;
    (Write(&sb, args), ...);
    Push(&sb, '\0'); // when in Rome...
    return {sb.count-1, sb.data};
}

void HandleFailedAssertion(cstr repr, Source_Location loc) {
    if (context.handlingAssert) return;
    context.handlingAssert = true;
    // int z = 0;
    // *(*(int**)&z) = 12;.
    LogError(loc, " Assertion `"_str, repr, "` Failed!"_str);
    // TODO: stack trace
    // exit(1);
}

#ifndef NCZ_NO_CC
void ReloadCppScript(Array<cstr> args, cstr src) {
    if (NeedsUpdate(args[0], src)) {
        cstr old = TPrint(args[0], ".old").data;
        if (!RenameFile(args[0], old)) exit(1);
        if (!RunCmd(NCZ_CC(args[0], src))) {
            RenameFile(old, args[0]);
            exit(1);
        }
        if (!RunCommandSync(args)) exit(1);
        exit(0);
    }
}
#endif//NCZ_NO_CC

// This library supports Windows and Posix systems. If you want to use this library on a different platform
// #define NCZ_NO_OS and implement any procedures that you use
template <typename... Args>
bool RunCmd(Args ...args) {
    NCZ_SAVE_STATE(context.temporaryStorage.mark);
    List<cstr> cmd {}; cmd.allocator = NCZ_TEMP;
    (Push(&cmd, args), ...);
    return RunCommandSync(cmd);
}

bool RunCommandSync(Array<cstr> args, bool trace) {
    auto [proc, ok] = RunCommandAsync(args, trace);
    if (!ok) return false;
    return Wait(proc);
}

bool Wait(Array<Process> procs) {
    bool ok = true;
    for (auto proc : procs) {
        ok = Wait(proc) && ok;
    }
    return ok;
}

bool NeedsUpdate(cstr outputPath, cstr inputPath) { return NeedsUpdate(outputPath, {1, &inputPath}); }

Result<String> ReadFile(cstr path) {
    String_Builder sb {};
    if (!ReadFile(&sb, path)) return {};
    return {sb};
}

#ifndef NCZ_NO_OS
#ifdef _WIN32

#else // POSIX
// Multiprocessing
bool Wait(Process proc) {
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc, &wstatus, 0) < 0) {
            LogError("could not wait on command "_str, proc, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                LogError("command exited with exit code ", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            LogError("command process was terminated by ", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }
    return true;
}

Result<Process> RunCommandAsync(Array<cstr> args, bool trace) {
    pid_t cpid = fork();
    if (cpid < 0) {
        LogError("Could not fork child process: ", strerror(errno));
        return {};
    }
    
    if (cpid == 0) {
        List<cstr> cmd {};
        String_Builder sb {};
        
        for (auto it : args) {
            Push(&cmd, it);
            if (!strchr(it, ' ')) {
                Write(&sb, it);
            } else {
                Push(&sb, '\"');
                Write(&sb, it);
                Push(&sb, '\"');
            }
            Push(&sb, ' ');
        }
        Push(&cmd, (cstr) nullptr);
        
        if (trace) Log(sb.data);
    
        if (execvp(cmd.data[0], (char * const*) cmd.data) < 0) {
            LogError("Could not exec child process: ", strerror(errno));
            exit(1);
        }
        NCZ_ASSERT(0 && "unreachable");
    }
    
    return {static_cast<unsigned long>(cpid)};
}

// Working with files
bool NeedsUpdate(cstr outputPath, Array<cstr> inputPaths) {
     struct stat statbuf {};
    
    if (stat(outputPath, &statbuf) < 0) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (errno == ENOENT) return true;
        LogError("Could not stat ", outputPath, ": ", strerror(errno));
        return false;
    }
    
    int ouptutPathTime = statbuf.st_mtime;

    for (size_t i = 0; i < inputPaths.count; ++i) {
        cstr input_path = inputPaths[i];
        if (stat(input_path, &statbuf) < 0) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            LogError("Could not stat ", input_path, ": ", strerror(errno));
            return false;
        }
        int inputPathTime = statbuf.st_mtime;
        // NOTE: if even a single input_path is fresher than outputPath that's 100% rebuild
        if (inputPathTime > ouptutPathTime) return true;
    }

    return false;
}

bool RenameFile(cstr oldPath, cstr newPath) {
    if (rename(oldPath, newPath) < 0) {
        LogError("Could not rename ", oldPath, " to ", newPath, ": ", strerror(errno));
        return false;
    }
    return true;
}

bool WriteFile(cstr path, String data) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        LogError("Could not open ", path, ": ", strerror(errno));
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
            LogError("Could not write data to ", path, ": ", strerror(error));
            return false;
        }
        size -= n;
        buf  += n;
    }
    
    return true;
}

bool ReadFile(String_Builder *stream, cstr path) {
    #define CHECK(x, e) if (x) {                                 \
    LogError("Could not read file ", path, ": ", strerror((e))); \
    return false;                                                \
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
        stream->data     = static_cast<char*>(Resize(stream->data, new_count, stream->count));
        stream->capacity = new_count;
    }

    fread(stream->data + stream->count, m, 1, f);
    int err = ferror(f);
    CHECK(err, err);
    stream->count = new_count;
    
    return true;
    #undef CHECK
}

Result<Array<cstr>> ReadFolder(cstr parent) {
    NCZ_PUSH_STATE(context.allocator, NCZ_TEMP);
    List<cstr> children {};
    
    DIR *dir = opendir(parent);
    if (dir == nullptr) {
        LogError("Could not open folder ", parent, ": ", strerror(errno));
        return {};
    }
    NCZ_DEFER(closedir(dir));
    
    errno = 0;
    
    struct dirent *ent = readdir(dir);
    while (ent != nullptr) {
        Push(&children, CopyCstr(ent->d_name));
        ent = readdir(dir);
    }
    
    if (errno != 0) {
        LogError("Could not read folder ", parent, ": ", strerror(errno));
        return {};
    }
    
    return children;
}

Result<File_Type> GetFileType(cstr path) {
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        LogError("Could not get stat of ", path, ": ", strerror(errno));
        return {};
    }

    switch (statbuf.st_mode & S_IFMT) {
    case S_IFDIR:  return File_Type::FOLDER;
    case S_IFREG:  return File_Type::FILE;
    case S_IFLNK:  return File_Type::LINK;
    default:       return File_Type::OTHER;
    }
}

template <typename F> static
bool Visit(cstr file, F visit_proc, String_Builder *full_path, u64 *base_path_len) {
    if ((strcmp(".", file) == 0) || (strcmp("..", file) == 0)) return true;
    full_path->count = *base_path_len;
    Print(full_path, file, "\0"_str);
    
    auto [type, ok] = GetFileType(full_path->data);
    if (!ok) return false;
    ok = visit_proc(String{full_path->count-1, full_path->data}, type);
    if (!ok) return false;
    
    if (type == File_Type::FOLDER) {
        auto [children, ok] = ReadFolder(full_path->data);
        if (!ok) return false;
        (*full_path)[full_path->count-1] = '/'; // replace null terminator to make it a proper folder
        NCZ_PUSH_STATE(*base_path_len, full_path->count); // hell yeah.....
        for (cstr c: children) if (!Visit(c, visit_proc, full_path, base_path_len)) return false;
    }
    
    return true;
}

// F is a function/closure of type (String path, File_Type type) -> bool
// While path is a String, path.data is a null terminated cstr for convenience.
// This function Allocates all paths with temporary storage so if you want to keep
// a path you are visiting you need to use CopyString or CopyCstr
template <typename F>
bool TraverseFolder(cstr path, F visit_proc) {
    Array<cstr> files;
    String_Builder full_path {};
    { NCZ_PUSH_STATE(context.allocator, NCZ_TEMP);
        auto result = ReadFolder(path);
        if (!result.ok) return false;
        files = result.value;
        Write(&full_path, path);
    }
    
    u64 base_path_len = full_path.count;
    for (cstr f: files) if (!Visit(f, visit_proc, &full_path, &base_path_len)) return false;
    
    return true;
}

#endif//WIN32/POSIX
#endif//NCZ_NO_OS

}//namespace ncz
#endif//NCZ_IMPLEMENTATION