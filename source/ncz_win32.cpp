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
    if (!proc) return false;
    DWORD result = WaitForSingleObject(proc, INFINITE);
    
    if (result == WAIT_FAILED) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "could not wait on child process: ", (u64) GetLastError());
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "could not get process exit code: ", (u64) GetLastError());
        return false;
    }

    if (exit_status != 0) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO, "command exited with exit code ", (u64) exit_status);
        return false;
    }

    CloseHandle(proc);

    return true;
}

Process run_command_async(Array<cstr> args, bool trace) {
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
        return 0;
    }
    
    CloseHandle(piProcInfo.hThread);
    return piProcInfo.hProcess;
}

// TODO: once we can capture output from procs we should make the mingw version not suck
void log_stack_trace() {
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
    
    
    #ifdef __GNUC__
    List<cstr> addr2line(NCZ_TEMP);
    addr2line.append("addr2line", "-f", "-p", "-C", "-e", __argv[0]);
    log_ex(Log_Level::TRACE, Log_Type::INFO, "Stack Trace:");
    #else
    String_Builder sb(NCZ_TEMP);
    format(&sb, "Stack Trace:\n"_str);
    #endif // __GNUC__
    
    // Walk the stack
    while (StackWalk64(machineType, GetCurrentProcess(), GetCurrentThread(), &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
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
            #ifdef __GNUC__
            addr2line.push(tprint((void*)stackFrame.AddrPC.Offset).data);
            NCZ_DEFER(addr2line.pop());
            if (!run_command_sync(addr2line, false)) break;
            #else
            if (SymGetLineFromAddr64(GetCurrentProcess(), stackFrame.AddrPC.Offset, &lineDisplacement, &line)) {
                write(&sb, line.FileName, ":", (u64) line.LineNumber-1, ": ");
            } else {
                write(&sb, "unknown: ");
            }
            write(&sb, symbol->Name, "\n");
            #endif // __GNUC__
        } else {
            #ifndef __GNUC__
            write(&sb, "unknown:\n");
            #endif
        }
        
        if (strcmp(symbol->Name, "main") == 0) break;
    }
    
    ncz::context.logger.proc({sb.data, sb.count}, Log_Level::TRACE, Log_Type::INFO, ncz::context.logger.data);
    
    // Cleanup
    SymCleanup(GetCurrentProcess());
}

bool needs_update(String output_path, Array<String> input_paths) {
    BOOL bSuccess;
    
    HANDLE output_path_fd = CreateFile(output_path.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return true;
        // log_ex(Log_Level::NORMAL, Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
        return false;
    }
    
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    if (!bSuccess) {
        // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
        return false;
    }
    
    for (auto input_path : input_paths) {
        HANDLE input_path_fd = CreateFile(input_path.data, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (input_path_fd == INVALID_HANDLE_VALUE) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
            log("BAD");
            return false;
        }
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        if (!bSuccess) {
            // log_ex(Log_Level::NORMAL,  Log_Type::ERRO, "Could not open file "_str, input_path.data, ": "_str, GetLastError());
            log("BAD");
            return false;
        }
    
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1) return true;
    };
    
    return false;
}

bool rename_file(String old_path, String new_path) {
    assert(*(old_path.data + old_path.count) == 0);
    assert(*(new_path.data + new_path.count) == 0);
    // TODO: make these logs trace or verbose
    log_ex(Log_Level::TRACE, Log_Type::INFO, "[rename] ", old_path, " -> ", new_path);
     if (!MoveFileEx(old_path.data, new_path.data, MOVEFILE_REPLACE_EXISTING)) {
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
