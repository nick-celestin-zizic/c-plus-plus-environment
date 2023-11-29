using namespace ncz;
using Rect = rl::Rectangle;
using Vec2 = rl::Vector2;

struct Target {
    bool broken;
    Rect rect;
};

struct Ball {
    Vec2 pos; // this is stored in cartesian form
    Vec2 vel; // this is stored in polar form
    bool dead;
    float radius;
};

Vec2 from_polar(float mag, float angle) { return { cosf(angle) * mag, sinf(angle) * mag }; }

void handle_collision(Ball* b, Rect rect) {
    if (rl::CheckCollisionCircleRec(b->pos, b->radius, rect)) {
        if (b->pos.x < rect.x && b->pos.x > rect.x - b->radius) {
            b->pos.x = rect.x - b->radius;
            b->vel.x *= -1;
        } else if (b->pos.y < rect.y && b->pos.y > rect.y - b->radius) {
            b->pos.y = rect.y - b->radius;
            b->vel.y *= -1;
        }
        if (b->pos.x > rect.x + rect.width && b->pos.x < rect.x + rect.width + b->radius) {
            b->pos.x = rect.x + rect.width + b->radius;
            b->vel.x *= -1;
        } else if (b->pos.y > rect.y + rect.height && b->pos.y < rect.y + rect.height + b->radius) {
            b->pos.y = rect.y + rect.height + b->radius;
            b->vel.y *= -1;
        }
    }
}

#define DEG_TO_RAD (PI/180.0f)
void breakout() {
    static Fixed_List<Ball, 1000000> balls   {};
    List<Target>       targets {};
    // Slab_Array<Target> targets {};
    
    const float BAR_HEIGHT = SCREEN_HEIGHT / 5.0f;
    const float BAR_WIDTH  = BAR_HEIGHT    / 10.0f;
    const float START_Y    = 0.5f * (SCREEN_HEIGHT - BAR_HEIGHT);
    const int   BALL_SPEED = 250;
    
    Rect left_player  { BAR_WIDTH, START_Y, BAR_WIDTH, BAR_HEIGHT };
    Rect right_player { SCREEN_WIDTH-BAR_WIDTH*2.0f, START_Y, BAR_WIDTH, BAR_HEIGHT };
    
    for (usize i = 0; i < 1000; ++i) {
        auto angle  = static_cast<float>(rl::GetRandomValue(0, 360) * DEG_TO_RAD);
        auto offset = from_polar(angle, static_cast<float>(rl::GetRandomValue(0, 100)));
        Ball* b     = balls.push({}); // balls[balls.get()];
        b->radius   = 25;
        b->pos      = { SCREEN_WIDTH/2 + offset.x, SCREEN_HEIGHT/2 + offset.y };
        b->vel      = from_polar(static_cast<float>(rl::GetRandomValue(BALL_SPEED/2, BALL_SPEED*2)), angle);
    }
    
    float hue_angle = 0;
    while (!rl::WindowShouldClose()) {
        context.temporary_storage.reset();
        
        float dt = rl::GetFrameTime(), w = SCREEN_WIDTH, h = SCREEN_HEIGHT;
        
        for (auto it = balls.begin(); it != balls.end(); ++it) {
            auto& b = *it;
            if (b.dead) continue;
            
            b.pos.x += b.vel.x * dt;
            b.pos.y += b.vel.y * dt;
            
            handle_collision(&b, left_player);
            handle_collision(&b, right_player);
            
            if (b.pos.y <= b.radius || b.pos.y >= h - b.radius) b.vel.y *= -1;
            if (b.pos.x <= b.radius || b.pos.x >= w - b.radius) b.dead   = true;
        }
        
        hue_angle  = fmodf(hue_angle + 60.0f * dt, 360.0f);
        auto color = rl::ColorFromHSV(hue_angle, 1, .75);
        rl::BeginDrawing();
            rl::ClearBackground(rl::BLACK);
            for (auto b : balls)   if (!b.dead)   rl::DrawCircleV(b.pos, b.radius, color);
            for (auto t : targets) if (!t.broken) rl::DrawRectangleRec(t.rect, color);
            rl::DrawRectangleRec(left_player, color);
            rl::DrawRectangleRec(right_player, color);
        rl::EndDrawing();
    }
}

    
void old_breakout() {
    static const int   PADDLE_WIDTH  = SCREEN_WIDTH / 10;
    static const int   PADDLE_HEIGHT = PADDLE_WIDTH / 5;
    static const float PADDLE_SPEED  = SCREEN_WIDTH * .75f;

    float circle_radius = PADDLE_HEIGHT * 0.75f;
    float hue_angle = 0;
    Vec2 circle_position { 100, 300 };
    Vec2 circle_velocity { PADDLE_SPEED, PADDLE_SPEED };

    Rect paddle {
        (SCREEN_WIDTH / 2) - (PADDLE_WIDTH / 2),
        SCREEN_HEIGHT - 2  * PADDLE_HEIGHT,
        PADDLE_WIDTH, PADDLE_HEIGHT
     };
    float paddle_velocity = 0;

    static ncz::Flat_Pool global_pool(32 * 1024);
    ncz::List<int> ints(global_pool.allocator());
    for (int it = 0; it <= 256; ++it) ints.push(it * 2);
    ncz::Array<int> first  { ints.data,  ints.count / 2 };
    ncz::Array<int> second { ints.data + ints.count / 2, ints.count / 2 };

    trace("Hello ", ints);
    // ncz::dbg(ints, first, second);
    bool paused = false;

    while (!rl::WindowShouldClose()) {
        ncz::context.temporary_storage.reset();

        float dt = rl::GetFrameTime(), w = SCREEN_WIDTH, h = SCREEN_HEIGHT;

        if (rl::IsKeyPressed(rl::KEY_SPACE)) paused = !paused;

        if (!paused) {
            // update paddle
            paddle_velocity = 0;
            if (rl::IsKeyDown(rl::KEY_RIGHT) || rl::IsKeyDown(rl::KEY_D)) paddle_velocity += PADDLE_SPEED;
            if (rl::IsKeyDown(rl::KEY_LEFT) || rl::IsKeyDown(rl::KEY_A)) paddle_velocity -= PADDLE_SPEED;
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
                if (circle_position.y < paddle.y && circle_position.y > paddle.y - circle_radius) {
                    circle_position.y = paddle.y - circle_radius;
                    circle_velocity.y *= -1;
                }
                if (circle_position.x < paddle.x && circle_position.x > paddle.x - circle_radius) {
                    circle_position.x = paddle.x - circle_radius;
                    circle_velocity.x *= -1;
                }
                if (circle_position.y > paddle.y + paddle.height && circle_position.y < paddle.y + paddle.height + circle_radius) {
                    circle_position.y = paddle.y + paddle.height + circle_radius;
                    circle_velocity.y *= -1;
                }
                if (circle_position.x > paddle.x + paddle.width && circle_position.x < paddle.x + paddle.width + circle_radius) {
                    circle_position.x = paddle.x + paddle.width + circle_radius;
                    circle_velocity.x *= -1;
                }
            }

            hue_angle = fmodf(hue_angle + 60.0f * dt, 360.0f);
        }

        // draw the scene
        auto color = rl::ColorFromHSV(hue_angle, 1, .75);
        rl::BeginDrawing();
            rl::ClearBackground(rl::LIGHTGRAY);

            auto stats = ncz::tprint("ball:   ", circle_position, "\n\n\n",
                                     "paddle: ", paddle,          "\n\n\n",
                                     "first:  ", first,           "\n\n\n",
                                     "second: ", second,          "\n\n\n");
            rl::DrawSomeText(stats.data, 100, 100, 36, color);

            if (paused) {
                const int font_size = 96;
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
}