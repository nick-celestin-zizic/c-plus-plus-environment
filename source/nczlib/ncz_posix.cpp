#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/mman.h>

Page _os_get_page(u64 size) {
    u64 s   = align(size, NCZ_PAGE_SIZE);
    void* p = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    assert(p);
    return {p, s};
}

void _os_del_page(Page page) {
    if (!page.memory) return;
    munmap(page.memory, page.size);
}

u8* os_reserve_pages(u64 reserve) {
    assert(reserve == align(reserve, NCZ_PAGE_SIZE));
    void* p = mmap(nullptr, reserve, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    assert(p);
    return (u8*) p;
}

u8* os_extend_commited_pages(u8* first_uncommitted_page, u8* end) {
    u64 delta = (u64) (end - first_uncommitted_page);
    assert(delta >= 0);
    u64 size = align(delta, NCZ_PAGE_SIZE);
    return first_uncommitted_page + size;
}

void crt_logger_proc(String message, Log_Level level, Log_Type type, void* logger_data) {
    (void) logger_data;
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
}

#ifndef NCZ_NO_MULTIPROCESSING
#include <unistd.h>
#include <sys/wait.h>
bool wait(Process proc) {
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc.proc, &wstatus, 0) < 0) {
            log_ex(Log_Level::NORMAL, Log_Type::ERRO, "could not wait on command "_str, proc.proc, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                log_ex(Log_Level::NORMAL, Log_Type::ERRO, "command exited with exit code ", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            log_ex(Log_Level::NORMAL, Log_Type::ERRO, "command process was terminated by ", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }
    return true;
}

Process run_command_async(Array<cstr> args, bool trace) {
    pid_t cpid = fork();
    if (cpid < 0) {
        log_ex(Log_Level::NORMAL, Log_Type::ERRO,  "Could not fork child process: ", strerror(errno));
        return {};
    }
    
    if (cpid == 0) {
        List<cstr> cmd {};
        String_Builder sb {};
        
        for (auto it : args) {
            cmd.push(it);
            if (!strchr(it, ' ')) {
                format(&sb, it);
            } else {
                sb.push('\"');
                format(&sb, it);
                sb.push('\"');
            }
            sb.push(' ');
        }
        cmd.push(nullptr);
        
        if (trace) log_info(sb.data);
    
        if (execvp(cmd.data[0], (char * const*) cmd.data) < 0) {
            log_ex(Log_Level::NORMAL, Log_Type::ERRO,  "Could not exec child process: ", strerror(errno));
            exit(1);
        }
        assert(0 && "unreachable");
    }
    return {cpid};
}
#else
Process run_command_async(Array<cstr> args, bool trace) {
    assert(false && "wait: MULTIPROCESSING IS DISABLED");
    return {};
}
bool wait(Process proc) {
    assert(false && "wait: MULTIPROCESSING IS DISABLED");
    return false;
}
#endif//NCZ_NO_MULTIPROCESSING

void log_stack_trace(int skip) {
    // void* callstack[128];
    // int frames = backtrace(callstack, 128);
    // char** strs = backtrace_symbols(callstack, frames);

    // for (int i = 0; i < frames; ++i) {
        // printf("Stack frame #%d: %s\n", i, strs[i]);

        // Prepare the addr2line command
        // run_cmd("addr2line", "-f", "-p", "-C", "-e", "build.exe", tprint((void*)callstack[i]).data);
        // char command[512];
        // snprintf(command, sizeof(command), "addr2line -f -p -C -e ./build.out %p", callstack[i]);

        // // Open the command for reading
        // FILE* fp = popen(command, "r");
        // if (fp) {
        //     // Read the output a line at a time
        //     char line[512];
        //     while (fgets(line, sizeof(line), fp)) {
        //         printf("    %s", line);
        //     }
        //     pclose(fp);
        // } else {
        //     printf("Failed to run addr2line command.\n");
        // }
    // }
    // free(strs);
}

bool needs_update(String output_path, Array<String> input_paths) {
    struct stat statbuf {};
    
    if (stat(output_path.data, &statbuf) < 0) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (errno == ENOENT) return true;
        // nob_log(NOB_ERROR, "could not stat %s: %s", output_path, strerror(errno));
        return false;
    }
    
    int output_path_time = statbuf.st_mtime;

    for (size_t i = 0; i < input_paths.count; ++i) {
        String input_path = input_paths[i];
        assert(*(input_path.data+input_path.count) == 0);
        if (stat(input_path.data, &statbuf) < 0) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            // nob_log(NOB_ERROR, "could not stat %s: %s", input_path, strerror(errno));
            return false;
        }
        int input_path_time = statbuf.st_mtime;
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (input_path_time > output_path_time) return true;
    }

    return false;
}

bool rename_file(String old_path, String new_path) {
    if (rename(old_path.data, new_path.data) < 0) {
        // nob_log(NOB_ERROR, "could not rename %s to %s: %s", old_path, new_path, strerror(errno));
        return false;
    }
    return true;
}

