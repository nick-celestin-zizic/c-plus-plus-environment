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
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "dbghelp")
#pragma warning(disable : 4996)
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#include <dbghelp.h>
#else // POSIX
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#endif
#endif//NCZ_NO_OS

namespace ncz {
// macros
#define NCZ_HERE ::ncz::Source_Location {__FILE__, __LINE__-1}
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
void LogStackTrace(usize skip = 0);

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
template <typename F> bool TraverseFolder(cstr path, F visitProc);


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
    
    // #define ANSI_COLOR_RED     "\x1b[31m"
    // #define ANSI_COLOR_YELLOW  "\x1b[33m"
    // #define ANSI_COLOR_RESET   "\x1b[0m"

    auto sink = type == Log_Type::ERRO ? stderr : stdout;
    switch (type) {
    case Log_Type::INFO:
        fwrite(message.data, 1, message.count, sink);
        break;
    case Log_Type::ERRO:
        fprintf(sink, /*ANSI_COLOR_RED*/ "%s" /*ANSI_COLOR_RESET*/, message.data);
        break;
    case Log_Type::WARN:
        fprintf(sink, /*ANSI_COLOR_YELLOW*/ "%s" /*ANSI_COLOR_RESET*/, message.data);
        break;
    }
    
    // #undef ANSI_COLOR_RED    
    // #undef ANSI_COLOR_YELLOW 
    // #undef ANSI_COLOR_RESET 
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
    LogStackTrace(2);
    // TODO: stack trace
    exit(1);
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

bool NeedsUpdate(cstr outputPath, cstr inputPath) {
    return NeedsUpdate(outputPath, {1, &inputPath});
}

Result<String> ReadFile(cstr path) {
    String_Builder sb {};
    if (!ReadFile(&sb, path)) return {};
    return {sb};
}

#ifndef NCZ_NO_OS

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

    usize newCount = stream->count + m;
    if (newCount > stream->capacity) {
        stream->data     = static_cast<char*>(Resize(stream->data, newCount, stream->count));
        stream->capacity = newCount;
    }

    fread(stream->data + stream->count, m, 1, f);
    int err = ferror(f);
    CHECK(err, err);
    stream->count = newCount;
    
    return true;
    #undef CHECK
}

#ifdef _WIN32
// Stack Trace
void LogStackTrace(usize skip) {
    // Initialize symbols
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    // Get the current context
    CONTEXT context;
    RtlCaptureContext(&context);

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

    DWORD machineType;
#ifdef _M_IX86
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rsp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif
    
    // TODO: Pool constructor
    // auto mark = ::ncz::context.temporary_storage.mark;
    // NCZ_DEFER(::ncz::context.temporary_storage.mark = mark);
    
    
    String_Builder sb{};//(NCZ_TEMP);
    sb.allocator = NCZ_TEMP;
    Write(&sb, "Stack Trace:\n"_str);
    
    // Walk the stack
    while (StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        if (skip--) continue;
        
        DWORD64 displacement = 0;
        const int bufferSize = 1024;
        char buffer[sizeof(SYMBOL_INFO) + bufferSize] = { 0 };
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = bufferSize - sizeof(SYMBOL_INFO);

        // Get symbol information
        if (SymFromAddr(GetCurrentProcess(), stackFrame.AddrPC.Offset, &displacement, symbol)) {
            IMAGEHLP_LINE64 line;
            DWORD lineDisplacement;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            // Get line information
            if (SymGetLineFromAddr64(GetCurrentProcess(), stackFrame.AddrPC.Offset, &lineDisplacement, &line)) {
                Print(&sb, line.FileName, ":", (u64) line.LineNumber-1, ": ");
            } else {
                Print(&sb, "unknown: ");
            }
            Print(&sb, symbol->Name, "\n");
        } else {
            Print(&sb, "unknown:\n");
        }
        
        if (strcmp(symbol->Name, "main") == 0) break;
    }
    
    LogEx(Log_Level::TRACE, Log_Type::INFO, sb);
    
    // Cleanup
    SymCleanup(GetCurrentProcess());
}

// Multiprocessing
bool Wait(Process proc) {
    if (!proc) return false;
    
    // String_Builder output(NCZ_TEMP);
    // DWORD bytesRead;
    // CHAR buffer[4096];
    // while (ReadFile(proc.output, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
    //     printf("read %lu\n", bytesRead);
    //     buffer[bytesRead] = '\0';
    //     output.append(Array<char> { buffer, bytesRead });
    // }
    // log(Array<char>{output.data, output.count});
    
    DWORD result = WaitForSingleObject((HANDLE)proc, INFINITE);
    
    if (result == WAIT_FAILED) {
        LogError("could not wait on child process: ", (u64) GetLastError());
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess((HANDLE)proc, &exit_status)) {
        LogError("could not get process exit code: ", (u64) GetLastError());
        return false;
    }

    
    if (exit_status != 0) {
        
        LogError("command exited with exit code ", (u64) exit_status);
        return false;
    }
    
    

    CloseHandle((HANDLE)proc);

    return true;
}

Result<Process> RunCommandAsync(Array<cstr> args, bool trace) {

    // // Create a pipe to capture the process's output
    // HANDLE hRead, hWrite;
    // SECURITY_ATTRIBUTES sa;
    // sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    // sa.lpSecurityDescriptor = NULL;
    // sa.bInheritHandle = TRUE;
    // if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
    //     LogError("CreatePipe failed: ", (u64) GetLastError());
    //     return {};
    // }
    
    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
    
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    // NOTE: theoretically setting NULL to std handles should not be a problem
    // https://docs.microsoft.com/en-us/windows/console/getstdhandle?redirectedfrom=MSDN#attachdetach-behavior
    // TODO: check for errors in GetStdHandle
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    
    // TODO: Pool constructor
    // auto mark = context.temporaryStorage.mark;
        String_Builder sb{};//(NCZ_TEMP);
        sb.allocator = NCZ_TEMP;
        for (auto* it = args.data; it != args.data + args.count; it++) {
            if (it != args.data) Push(&sb, ' ');
            if (!strchr(*it, ' ')) {
                Write(&sb, *it);
            } else {
                Push(&sb, '\"');
                Write(&sb, *it);
                Push(&sb, '\"');
            }
        }
        Push(&sb, '\0');
        if (trace) LogInfo(sb.data);
        BOOL bSuccess = CreateProcessA(NULL, sb.data, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    // context.temporaryStorage.mark = mark;
    
    if (!bSuccess) {
        LogError("Could not create child process: ", (u64) GetLastError());
        return {};
    }
    
    CloseHandle(piProcInfo.hThread);
    return (u64)piProcInfo.hProcess;
}


// Working With Files
bool RenameFile(cstr old_path, cstr new_path) {
    // TODO: make these logs trace or verbose
    LogEx(Log_Level::TRACE, Log_Type::INFO, "[rename] ", old_path, " -> ", new_path);
     if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
        // LogError("Could not rename "_str, old_path, ": "_str, os_get_error());
        // return false;
        
        char buf[256];
        memset(buf, 0, sizeof(buf));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       buf, (sizeof(buf) / sizeof(char)), NULL);
        
        String str { strlen(buf), buf };

        if (str[str.count-1] == '\n') str.count -= 1;
        LogEx(Log_Level::NORMAL, Log_Type::ERRO, "Could not rename ", old_path, ": ", str);
        return false;
    }
    return true;
}

bool NeedsUpdate(cstr output_path, Array<cstr> input_paths) {
    String error_string {};
    u32 error_id = 0;
    #define CHECK(x, ...) if (!(x)) { \
        error_id = GetLastError();    \
        error_string.count = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_string.data, 0, NULL) - 1; \
        LogError(/*NCZ_HERE, " ",*/ __VA_ARGS__, ": (", error_id, ") ", error_string); \
        LocalFree(error_string.data); \
        return false; \
    }
    
    BOOL bSuccess;
    
    HANDLE output_path_fd = CreateFile(output_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE) {
        error_id = GetLastError();
        if (error_id == 2) { // output does not yet exist, means we have to update
            return true;
        } else {
            error_string.count = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_string.data, 0, NULL) - 1;
            LogError(NCZ_HERE, " ", "Could not open file ", output_path, ": (", error_id, ") ", error_string);
            LocalFree(error_string.data);
            return false;
        }
    }
    
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    CHECK(bSuccess, "Could not get time information for ", output_path);
    
    for (auto input_path : input_paths) {
        HANDLE input_path_fd = CreateFile(input_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        CHECK(input_path_fd != INVALID_HANDLE_VALUE, "Could not open file ", input_path);
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        CHECK(bSuccess, "Could not get time information for ", input_path);
    
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1) return true;
    };
    
    return false;
    #undef CHECK
}

static String GetErrorString() {
    auto errorCode = GetLastError();
    
    // Returns a string in temporary storage.
    wchar_t *lpMsgBuf;
    bool success = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
           FORMAT_MESSAGE_FROM_SYSTEM  |
           FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode,
        MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), // Default language
        (wchar_t*)&lpMsgBuf, 0, nullptr
    );
    if (!success) return ""_str;
    NCZ_DEFER(LocalFree(lpMsgBuf));

    
    // result := Utf8.wide_to_utf8_new(lpMsgBuf,, Basic.temp);
    auto queryResult = WideCharToMultiByte(CP_UTF8, 0,
                                            lpMsgBuf, -1,
                                            nullptr, 0,
                                            nullptr, nullptr);
    if (queryResult <= 0) return ""_str;

    String name;
    auto nameBytes = static_cast<char*>(Allocate(queryResult));
    auto result    = WideCharToMultiByte(CP_UTF8, 0, lpMsgBuf, -1, nameBytes, queryResult, nullptr, nullptr);
    if (result <= 0) return ""_str;
    NCZ_ASSERT(result <= queryResult);
    
    name.data  = nameBytes;
    name.count = result-1; // Returned length DOES include null character if we passed in a null-terminated string!
    
    usize trimmedCount = name.count;
    for (usize i = name.count; i >= 0; --i) {
        if (name.data[i] == '\r' || name.data[i] == '\n') trimmedCount -= 1;
        else break;
    }
    
    // Chop off the newline so that we can format this standardly. Argh.
    return {trimmedCount, name.data};
}

Result<Array<cstr>> ReadFolder(cstr parent) {
    NCZ_PUSH_STATE(context.allocator, NCZ_TEMP);
    List<cstr> children {};
    
    // cstr search_path = Concat(parent, "\\*"); // Append wildcard to parent path
    cstr search_path = TPrint(parent, "\\*"_str).data;
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(search_path, &ffd);
    if (INVALID_HANDLE_VALUE == hFind) {
        LogError("Could not open folder ", parent, ": ", GetErrorString());
        return {};
    }
    NCZ_DEFER(FindClose(hFind));

    do Push(&children, CopyCstr(ffd.cFileName));
    while (FindNextFile(hFind, &ffd) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        LogError("Could not read folder ", parent, ": ", GetErrorString());
        return {};
    }

    return children;
}

Result<File_Type> GetFileType(cstr path) {
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &wfad)) {
        LogError("Could not get attributes of ", path, ": ", GetErrorString());
        return {};
    }

    if (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return File_Type::FOLDER;
    else if (wfad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return File_Type::LINK;
    else
        return File_Type::FILE;
}

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


#endif//WIN32/POSIX

// POSIX
// #define NCZ_PATH_SEP '/'

// WIN32
#define NCZ_PATH_SEP "\\"

template <typename F> static
bool Visit(cstr file, F visitProc, String_Builder *fullPath, u64 *basePathLen) {
    if ((strcmp(".", file) == 0) || (strcmp("..", file) == 0)) return true;
    fullPath->count = *basePathLen;
    Print(fullPath, NCZ_PATH_SEP, file, "\0"_str);
    
    auto [type, ok] = GetFileType(fullPath->data);
    if (!ok) return false;
    ok = visitProc(String{fullPath->count-1, fullPath->data}, type);
    if (!ok) return false;
    
    if (type == File_Type::FOLDER) {
        auto [children, ok] = ReadFolder(fullPath->data);
        if (!ok) return false;
        (*fullPath)[fullPath->count-1] = NCZ_PATH_SEP[0]; // replace null terminator to make it a proper folder
        NCZ_PUSH_STATE(*basePathLen, fullPath->count); // hell yeah.....
        for (cstr c: children) if (!Visit(c, visitProc, fullPath, basePathLen)) return false;
    }
    
    return true;
}

// F is a function/closure of type (String path, File_Type type) -> bool
// While path is a String, path.data is a null terminated cstr for convenience.
// This function Allocates all paths with temporary storage so if you want to keep
// a path you are visiting you need to use CopyString or CopyCstr
template <typename F>
bool TraverseFolder(cstr path, F visitProc) {
    Array<cstr> files;
    String_Builder fullPath {};
    {
        NCZ_PUSH_STATE(context.allocator, NCZ_TEMP);
        auto result = ReadFolder(path);
        if (!result.ok) return false;
        files = result.value;
        Write(&fullPath, path);
    }
    
    u64 basePathLen = fullPath.count;
    for (cstr f: files) if (!Visit(f, visitProc, &fullPath, &basePathLen)) return false;
    
    return true;
}

#endif//NCZ_NO_OS

}//namespace ncz
#endif//NCZ_IMPLEMENTATION