#define NCZ_IMPLEMENTATION
#include "nczlib/ncz.hpp"
#include "raylib/raylib.cpp"

static const int FACTOR        = 300;
static const int SCREEN_WIDTH  = 4 * FACTOR;
static const int SCREEN_HEIGHT = 3 * FACTOR;

static const int   PADDLE_WIDTH  = SCREEN_WIDTH / 10.f;
static const int   PADDLE_HEIGHT = PADDLE_WIDTH / 5.0f;
static const float PADDLE_SPEED  = SCREEN_WIDTH * .75f;

bool paused     = false;
u64  frame      = 0;
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

void run_frame() {
    ncz::context.temporary_storage.reset();
    
    frame += 1;
    float dt = rl::GetFrameTime(), w = SCREEN_WIDTH, h = SCREEN_HEIGHT;
    if (rl::IsKeyPressed(rl::KEY_SPACE)) paused = !paused;

    if (!paused) {
        // update paddle
        paddle_velocity = 0;
        if (rl::IsKeyDown(rl::KEY_RIGHT) || rl::IsKeyDown(rl::KEY_D)) paddle_velocity += PADDLE_SPEED;
        if (rl::IsKeyDown(rl::KEY_LEFT)  || rl::IsKeyDown(rl::KEY_A)) paddle_velocity -= PADDLE_SPEED;
        paddle.x += paddle_velocity * dt;
        ncz::clamp(&paddle.x, 0.0f, w - paddle.width);

        // update ball
        circle_position.x += circle_velocity.x * dt;
        circle_position.y += circle_velocity.y * dt;
        if (circle_position.x < circle_radius || circle_position.x > w - circle_radius) circle_velocity.x *= -1;
        if (circle_position.y < circle_radius || circle_position.y > h - circle_radius) circle_velocity.y *= -1;
        ncz::clamp(&circle_position.x, circle_radius, w - circle_radius);
        ncz::clamp(&circle_position.y, circle_radius, h - circle_radius);

        if (rl::CheckCollisionCircleRec(circle_position, circle_radius, paddle)) {
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

        auto stats = ncz::tprint("ball:   ", circle_position, "\n\n\n",
                                 "paddle: ", paddle,          "\n\n\n",
                                 "frame:  ", frame,           "\n\n\n");
        rl::DrawSomeText(stats.data, SCREEN_WIDTH*0.025f, SCREEN_HEIGHT*0.05f, 30, color);

        if (paused) {
            const int font_size = 100;
            cstr text           = "PAUSED.";
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
    rl::InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "wow wasm with raylib so cool");
    #ifdef  WEB_BUILD
    raylib_js_set_entry(run_frame);
    #else
    rl::SetTraceLogCallback(ncz::raylib_trace_log_adapter);
    while (!rl::WindowShouldClose()) run_frame();
    rl::CloseTheWindow();
    #endif//WEB_BUILD
    
    // Array       <type>            ();
    // Fixed_Array <type, len>       ();
    // List        <type>            (allocator);
    // Fixed_List  <type, capacity>  ();
    // Array_List  <type, array_len> (allocator);
    // Cache       <type>            (allocator);
    // Set         <type>            (allocator);
    // Map         <ktype, vtype>    (allocator);
    
    // Arena       <>         (align, block_size, allocator);
    // Flat_Arena  <>         (align, reserve);
    // Fixed_Arena <capacity> (align);
    
    // allocate
    // dispose
    // resize
    // create_heap
    // destroy_heap
}
