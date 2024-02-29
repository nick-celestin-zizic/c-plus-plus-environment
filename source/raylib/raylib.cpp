namespace rl {
    #include "raylib.h"
}

namespace ncz {
    void format(String_Builder *sb, rl::Vector2 v) {
        print(sb, "(", v.x, ", ", v.y, ")");
    }
    void format(String_Builder *sb, rl::Rectangle r) {
        print(sb, "{x: ", r.x, ", y: ", r.y, ", w: ", r.width, ", h: ", r.height, "}");
    }
    void raylib_trace_log_adapter(int log_level, cstr text, void* args) {
        Log_Type type;
        if      (log_level <= rl::LOG_INFO)    type = Log_Type::INFO;
        else if (log_level == rl::LOG_WARNING) type = Log_Type::WARN;
        else                                   type = Log_Type::ERRO;
        char buf[1024];
        // auto argp = *(va_list*)(&args);
        #ifdef _WIN32
        vsnprintf(buf, 1024, text, *(va_list*)&args);
        #else
        vsnprintf(buf, 1024, text, *(va_list*)args);
        #endif
        context.logger.labels.push("raylib");
        log_ex(Log_Level::TRACE, type, buf);
        context.logger.labels.pop();
    }
}

// a lot of the procedures in raylib are actually cross platform
// its just very hard to detect that. If you run into a raylib
// procedure that is not implemented in raylib.js, you can either
// paste the implementation from the c library below, or you will
// have to implement it in javascript if its not possible.
#ifdef  WEB_BUILD
extern "C" {
void raylib_js_set_entry(void (*entry)(void));
bool CheckCollisionCircleRec(rl::Vector2 center, float radius, rl::Rectangle rec)
{
    bool collision = false;

    int recCenterX = (int)(rec.x + rec.width/2.0f);
    int recCenterY = (int)(rec.y + rec.height/2.0f);

    float dx = fabsf(center.x - (float)recCenterX);
    float dy = fabsf(center.y - (float)recCenterY);

    if (dx > (rec.width/2.0f + radius)) { return false; }
    if (dy > (rec.height/2.0f + radius)) { return false; }

    if (dx <= (rec.width/2.0f)) { return true; }
    if (dy <= (rec.height/2.0f)) { return true; }

    float cornerDistanceSq = (dx - rec.width/2.0f)*(dx - rec.width/2.0f) +
                             (dy - rec.height/2.0f)*(dy - rec.height/2.0f);

    collision = (cornerDistanceSq <= (radius*radius));

    return collision;
}
// Get a Color from HSV values
// Implementation reference: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSV_conversion
// NOTE: Color->HSV->Color conversion will not yield exactly the same color due to rounding errors
// Hue is provided in degrees: [0..360]
// Saturation/Value are provided normalized: [0.0f..1.0f]
rl::Color ColorFromHSV(float hue, float saturation, float value)
{
    rl::Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}
}
#endif
