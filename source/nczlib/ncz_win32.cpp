#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#include <dbghelp.h>

Page _os_get_page(u64 size) {
    u64 s   = align(size, NCZ_PAGE_SIZE);
    void* p = VirtualAlloc(nullptr, static_cast<size_t>(s), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(p);
    return {p, s};
}

void _os_del_page(Page page) {
    if (!page.memory) return;
    VirtualFree(page.memory, 0, MEM_RELEASE);
}

u8* os_reserve_pages(u64 reserve) {
    assert(reserve == align(reserve, NCZ_PAGE_SIZE));
    auto p = (u8*) VirtualAlloc(nullptr, static_cast<size_t>(reserve), MEM_RESERVE, PAGE_READWRITE);
    assert(p);
    return p;
}

u8* os_extend_commited_pages(u8* first_uncommitted_page, u8* end) {
    u64 delta = (u64) (end - first_uncommitted_page);
    assert(delta >= 0);
    u64 size = align(delta, NCZ_PAGE_SIZE);
    VirtualAlloc(first_uncommitted_page, static_cast<size_t>(size), MEM_COMMIT, PAGE_READWRITE);
    return first_uncommitted_page + size;
}

void crt_logger_proc(String message, Log_Level level, Log_Type type, void* logger_data) {
    (void) logger_data;
    (void) level;
    
    auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (type) {
    case Log_Type::INFO:
        fwrite(message.data, 1, message.count, stdout);
        break;
    case Log_Type::ERRO:
        SetConsoleTextAttribute(handle, 0x0004);
        fwrite(message.data, 1, message.count, stdout);
        SetConsoleTextAttribute(handle, 0x0004 | 0x0002 | 0x0001);
        break;
    case Log_Type::WARN:
        SetConsoleTextAttribute(handle, 0x0004 | 0x0002);
        fwrite(message.data, 1, message.count, stdout);
        SetConsoleTextAttribute(handle, 0x0004 | 0x0002 | 0x0001);
        break;
    }
}

bool wait(Process proc) {
    if (!proc.proc) return false;
    
    // String_Builder output(NCZ_TEMP);
    // DWORD bytesRead;
    // CHAR buffer[4096];
    // while (ReadFile(proc.output, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
    //     printf("read %lu\n", bytesRead);
    //     buffer[bytesRead] = '\0';
    //     output.append(Array<char> { buffer, bytesRead });
    // }
    // log(Array<char>{output.data, output.count});
    
    DWORD result = WaitForSingleObject(proc.proc, INFINITE);
    
    if (result == WAIT_FAILED) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "could not wait on child process: ", (u64) GetLastError());
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc.proc, &exit_status)) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "could not get process exit code: ", (u64) GetLastError());
        return false;
    }

    
    if (exit_status != 0) {
        
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "command exited with exit code ", (u64) exit_status);
        return false;
    }
    
    

    CloseHandle(proc.proc);

    return true;
}

Process run_command_async(Array<cstr> args, bool trace) {

    // Create a pipe to capture the process's output
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        log_error("CreatePipe failed: ", (u64) GetLastError());
        return {};
    }
    
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
    
    auto mark = context.temporary_storage.mark;
        String_Builder sb(NCZ_TEMP);
        for (auto* it = args.data; it != args.data + args.count; it++) {
            if (it != args.data) sb.push(' ');
            if (!strchr(*it, ' ')) {
                format(&sb, *it);
            } else {
                sb.push('\"');
                format(&sb, *it);
                sb.push('\"');
            }
        }
        sb.push(0);
        if (trace) log_info(sb.data);
        BOOL bSuccess = CreateProcessA(NULL, sb.data, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    context.temporary_storage.mark = mark;
    
    if (!bSuccess) {
        // TODO: log_error
        log_error("Could not create child process: ", (u64) GetLastError());
        return {};
    }
    
    CloseHandle(piProcInfo.hThread);
    return {piProcInfo.hProcess, hRead};
}

// TODO: once we can capture output from procs we should make the mingw version not suck
void log_stack_trace(int skip) {
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

    auto mark = ::ncz::context.temporary_storage.mark;
    NCZ_DEFER(::ncz::context.temporary_storage.mark = mark);
    
    
    String_Builder sb(NCZ_TEMP);
    format(&sb, "Stack Trace:\n"_str);
    
    // Walk the stack
    while (StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        if (skip) { --skip; continue; }
        
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
                print(&sb, line.FileName, ":", (u64) line.LineNumber-1, ": ");
            } else {
                print(&sb, "unknown: ");
            }
            print(&sb, symbol->Name, "\n");
        } else {
            print(&sb, "unknown:\n");
        }
        
        if (strcmp(symbol->Name, "main") == 0) break;
    }
    
    ncz::context.logger.proc({sb.data, sb.count}, Log_Level::TRACE, Log_Type::INFO, ncz::context.logger.data);
    
    // Cleanup
    SymCleanup(GetCurrentProcess());
}

bool needs_update(cstr output_path, Array<cstr> input_paths) {
    String error_string {};
    u32 error_id = 0;
    #define CHECK(x, ...) if (!(x)) { \
        error_id = GetLastError();    \
        error_string.count = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_string.data, 0, NULL) - 1; \
        log_error(/*NCZ_HERE, " ",*/ __VA_ARGS__, ": (", error_id, ") ", error_string); \
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
            log_error(NCZ_HERE, " ", "Could not open file ", output_path, ": (", error_id, ") ", error_string);
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

bool rename_file(cstr old_path, cstr new_path) {
    // TODO: make these logs trace or verbose
    log_ex(Log_Level::TRACE, Log_Type::INFO, "[rename] ", old_path, " -> ", new_path);
     if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
        // log_error("Could not rename "_str, old_path, ": "_str, os_get_error());
        // return false;
        
        char buf[256];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       buf, (sizeof(buf) / sizeof(char)), NULL);
        auto str = to_string(buf);
        if (str[str.count-1] == '\n') str.count -= 1;
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Could not rename ", old_path, ": ", str);
        return false;
    }
    return true;
}
