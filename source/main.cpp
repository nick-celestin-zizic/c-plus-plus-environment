#define NCZ_IMPLEMENTATION
#include "ncz.hpp"

namespace rl {
    #include "raylib/raylib.h"
}

namespace ncz {
    void format(ncz::String_Builder* sb, rl::Vector2 v) {
        ncz::write(sb, "(", v.x, ", ", v.y, ")");
    }
    void format(ncz::String_Builder* sb, rl::Rectangle r) {
        ncz::write(sb, "{x: ", r.x, ", y: ", r.y, ", w: ", r.width, ", h: ", r.height, "}");
    }
    void raylib_trace_log_adapter(int log_level, cstr text, void* args) {
        Log_Type type;
        if      (log_level <= rl::LOG_INFO)    type = Log_Type::INFO;
        else if (log_level == rl::LOG_WARNING) type = Log_Type::WARN;
        else                                   type = Log_Type::ERRO;
        static char buf[1024];
        vsnprintf(buf, 1024, text, (va_list) args);
        context.logger.labels.push("raylib");
        log_ex(Log_Level::TRACE, type, buf);
        context.logger.labels.pop();
    }
}

// TODO: figure out how to do backtraces sighhhh
// Array<void*> backtrace {};
// backtrace.count = CaptureStackBackTrace(0, 65535, backtrace.data, nullptr);
// log(backtrace);
// char buf[256];
// FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//                NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
//                buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
// auto str = String { buf, strlen(buf) };
// if (str[str.count-1] == '\n') str.count -= 1;
// log(str);
// exit(0);

static const int FACTOR        = 400;
static const int SCREEN_WIDTH  = 4 * FACTOR;
static const int SCREEN_HEIGHT = 3 * FACTOR;

#include "pongout.cpp"

void hello_world() {
    using namespace rl;
    u64 frame = 0;
    while (!WindowShouldClose()) {
        ncz::context.temporary_storage.reset();
        frame += 1;
        
        BeginDrawing();
        ClearBackground(rl::WHITE);

        const int height = 50;
        const int x      = 10;
        int y            = 10;
        
        DrawSomeText("Hello, World!", x, y, height, rl::BLACK);
        y += height;
        
        DrawSomeText("Hello, Again!", x, y, height, rl::BLACK);
        y += height;
        
        auto text = ncz::tprint("Hello on frame ", frame, "!!!!");
        DrawSomeText(text.data, x, y, height, rl::BLACK);
        y += height;
        EndDrawing();
    }
}

void command_line(Array<cstr> args) {
    using namespace ncz;
    context.logger.labels.push("command line");
    NCZ_DEFER(context.logger.labels.pop());
    
    auto& ls = context.logger.labels;
    ls.push("loops");
        log("args are ", args);
        for (cstr arg : args) log("ARG: ", arg);
    ls.pop();
    
    cstr exe = *args.advance();
    log("WOW THE EXE IS ", exe);
    if (!args.count) return;
    
    ls.push("string manipulation");
        args.advance();
        String str = from_cstr(*args.advance());
        log("BEFORE: ", str);
        String lhs = str.chop_by(' ');
        log("L \"", lhs, "\" and R \"", str, "\"");
    ls.pop();
    
    cstr f = *args.advance();
    log("heuheuheuhue ", f);
}

int main(int argc, cstr* argv) {
    command_line({ argv, (usize) argc });
    rl::SetTraceLogCallback(ncz::raylib_trace_log_adapter);
    rl::SetTargetFPS(144);
    rl::InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "BREAKOUT :D");    
    // hello_world();
    // return 69;
    breakout();
    
    // Array         <type> ();
    // Dynamic_Array <type> (allocator);
    
    // Arena       <>         (align, block_size, allocator);
    // Flat_Arena  <>         (align, reserve);
    // Fixed_Arena <capacity> (align);
    
    // Bucket_Array <type, items_per_bucket> (allocator);
    // Slab_Array   <type, pages_per_slab>   ();
    // Fixed_Array  <type, capacity>         ();
    
    // malloc;
    // Slab_Heap  <>         (align);
    // Fixed_Heap <capacity> (align);
    
    // ncz::Bucket_Array <int> funny {};
    // // ncz::Slab_Array   <int> epic  {};
    // // ncz::Bucket_Array <int> funny {};
    // for (int i = 0; i <= 256; ++i) {
    //     auto& epic = funny[funny.get()];
    //     epic = i;
    //     log("lol ", epic);
    // }
    
    // log(funny.count);
    // log(funny.all_buckets.count);
    
    // return 69;
    // for (auto it : funny) { 
    //     log("WOWOWOWO A ", it);
    // }
    
    // rl::CloseTheWindow();
    // return 0;
}
