#define NCZ_IMPLEMENTATION
#include "math.h" // for fmodf
#include "nczlib/ncz.hpp"
#include "raylib/raylib.cpp"

static const int FACTOR        = 300;
static const int SCREEN_WIDTH  = 4 * FACTOR;
static const int SCREEN_HEIGHT = 3 * FACTOR;

static const int   PADDLE_WIDTH  = SCREEN_WIDTH / 10.f;
static const int   PADDLE_HEIGHT = PADDLE_WIDTH / 5.0f;
static const float PADDLE_SPEED  = SCREEN_WIDTH * .75f;

bool paused     = false;
ncz::u64 frame  = 0;
float hue_angle = 0;

float       circle_radius = PADDLE_HEIGHT * 0.75f;
rl::Vector2 circle_position { 100, 300 };
rl::Vector2 circle_velocity { PADDLE_SPEED, PADDLE_SPEED };

float paddle_velocity = 0;
rl::Rectangle paddle {
    (SCREEN_WIDTH / 2) - (PADDLE_WIDTH / 2),
    SCREEN_HEIGHT - 2  * PADDLE_HEIGHT,
    PADDLE_WIDTH, PADDLE_HEIGHT
};

template <typename T>
void Clamp(T* t, T min, T max) {
    if (*t < min) *t = min;
    else if (*t > max) *t = max;
}

void RunFrame() {
    Reset(&ncz::context.temporaryStorage);
    
    frame += 1;
    float dt = rl::GetFrameTime(), w = SCREEN_WIDTH, h = SCREEN_HEIGHT;
    if (rl::IsKeyPressed(rl::KEY_SPACE)) paused = !paused;

    if (!paused) {
        // update paddle
        paddle_velocity = 0;
        if (rl::IsKeyDown(rl::KEY_RIGHT) || rl::IsKeyDown(rl::KEY_D)) paddle_velocity += PADDLE_SPEED;
        if (rl::IsKeyDown(rl::KEY_LEFT)  || rl::IsKeyDown(rl::KEY_A)) paddle_velocity -= PADDLE_SPEED;
        paddle.x += paddle_velocity * dt;
        Clamp(&paddle.x, 0.0f, w - paddle.width);

        // update ball
        circle_position.x += circle_velocity.x * dt;
        circle_position.y += circle_velocity.y * dt;
        if (circle_position.x < circle_radius || circle_position.x > w - circle_radius) circle_velocity.x *= -1;
        if (circle_position.y < circle_radius || circle_position.y > h - circle_radius) circle_velocity.y *= -1;
        Clamp(&circle_position.x, circle_radius, w - circle_radius);
        Clamp(&circle_position.y, circle_radius, h - circle_radius);

        if (rl::CheckCollisionCircleRec(circle_position, circle_radius, paddle)) {
            ncz::Log("DING! ", paddle);
            if (circle_position.y < paddle.y
            &&  circle_position.y > paddle.y - circle_radius) {
                circle_position.y = paddle.y - circle_radius;
                circle_velocity.y *= -1;
            }
            if (circle_position.x < paddle.x
            &&  circle_position.x > paddle.x - circle_radius) {
                circle_position.x = paddle.x - circle_radius;
                circle_velocity.x *= -1;
            }
            if (circle_position.y > paddle.y + paddle.height
            &&  circle_position.y < paddle.y + paddle.height + circle_radius) {
                circle_position.y = paddle.y + paddle.height + circle_radius;
                circle_velocity.y *= -1;
            }
            if (circle_position.x > paddle.x + paddle.width
            &&  circle_position.x < paddle.x + paddle.width + circle_radius) {
                circle_position.x = paddle.x + paddle.width + circle_radius;
                circle_velocity.x *= -1;
            }
        }

        hue_angle = fmodf(hue_angle + 10.0f * dt, 360.0f);
    }

    // draw the scene
    auto color = rl::ColorFromHSV(hue_angle, 1.0f, 1.0f);
    rl::BeginDrawing();
        rl::ClearBackground(rl::DARKGRAY);

        auto stats = ncz::TPrint("ball:   ", circle_position, "\n\n\n",
                                 "paddle: ", paddle,          "\n\n\n",
                                 "frame:  ", frame,           "\n\n\n");
        rl::DrawSomeText(stats.data, SCREEN_WIDTH*0.025f, SCREEN_HEIGHT*0.05f, 30, color);

        if (paused) {
            const int font_size = 100;
            ncz::cstr text      = "PAUSED.";
            int  text_width     = rl::MeasureText(text, font_size);
            rl::DrawSomeText(text,
                SCREEN_WIDTH  / 2 - text_width / 2,
                SCREEN_HEIGHT / 2 - font_size  / 2,
                font_size, color
            );
        }

        rl::DrawCircleV(circle_position, circle_radius, color);
        rl::DrawRectangleRec(paddle, color);
    rl::EndDrawing();
}

int main(void) {
    #ifndef PLATFORM_WEB // TODO: just add this functionality to raylib.js
    rl::SetTraceLogCallback(ncz::RaylibTraceLogAdapter);
    #endif//PLATFORM_WEB
    rl::InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "wow wasm with raylib so cool");
    #ifdef  PLATFORM_WEB
    raylib_js_set_entry(RunFrame);
    #else
    while (!rl::WindowShouldClose()) RunFrame();
    rl::CloseTheWindow();
    #endif//PLATFORM_WEB
    
    // ncz_context.hpp
    // Memory Primitives:
        // Array       <type>            ();
        // Fixed_Array <type, len>       ();
        // Fixed_List  <type, capacity>  (); 
    // Allocator:
        // allocate
        // dispose
        // resize
    
    // ncz_os.hpp
    // filesystem
    // multiprocessing
    // memory page stuff + unmapping allocator
    // actually building c++ code
    
    // ncz_ds.hpp
    // List        <type>             (allocator);
    // Array_List  <type, array_len>  (allocator);
    // Set         <type>             (allocator);
    // Map         <ktype, vtype>     (allocator);
    // Cache       <type>             (); // uses os page allocator
    
    // ncz_arena.hpp
    // Arena       <>         (align, block_size, allocator);
    // Flat_Arena  <>         (align, capacity);
    // Fixed_Arena <capacity> (align);
    
}
