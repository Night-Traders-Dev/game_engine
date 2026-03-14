#include "engine/platform/touch_controls.h"
#include "engine/graphics/sprite_batch.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eb {

void TouchControls::begin_frame(int screen_w, int screen_h) {
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    // D-pad: bottom-left
    dpad_center_ = {MARGIN + DPAD_RADIUS + 20.0f,
                    screen_h - MARGIN - DPAD_RADIUS - 20.0f};

    // Buttons: bottom-right, A lower-right, B upper-left of A
    float btn_base_x = screen_w - MARGIN - BUTTON_RADIUS - 20.0f;
    float btn_base_y = screen_h - MARGIN - BUTTON_RADIUS - 30.0f;
    btn_a_center_ = {btn_base_x, btn_base_y};
    btn_b_center_ = {btn_base_x - BUTTON_RADIUS * 2.5f, btn_base_y - BUTTON_RADIUS * 1.2f};

    // Menu button: top-right corner
    btn_menu_center_ = {static_cast<float>(screen_w) - MARGIN - 25.0f, MARGIN + 25.0f};

    // Reset per-frame flags
    a_pressed_ = false;
    b_pressed_ = false;
    menu_pressed_ = false;
}

TouchZone TouchControls::zone_at(float x, float y) const {
    // Check d-pad (circular region, generous)
    float dx = x - dpad_center_.x;
    float dy = y - dpad_center_.y;
    if (dx * dx + dy * dy < (DPAD_RADIUS + 40.0f) * (DPAD_RADIUS + 40.0f))
        return TouchZone::DPad;

    // Check button A
    dx = x - btn_a_center_.x;
    dy = y - btn_a_center_.y;
    if (dx * dx + dy * dy < (BUTTON_RADIUS + 20.0f) * (BUTTON_RADIUS + 20.0f))
        return TouchZone::ButtonA;

    // Check button B
    dx = x - btn_b_center_.x;
    dy = y - btn_b_center_.y;
    if (dx * dx + dy * dy < (BUTTON_RADIUS + 20.0f) * (BUTTON_RADIUS + 20.0f))
        return TouchZone::ButtonB;

    // Check menu button
    dx = x - btn_menu_center_.x;
    dy = y - btn_menu_center_.y;
    if (dx * dx + dy * dy < 50.0f * 50.0f)
        return TouchZone::ButtonMenu;

    return TouchZone::None;
}

void TouchControls::touch_down(int pointer_id, float x, float y) {
    if (pointer_id < 0 || pointer_id >= MAX_FINGERS) return;

    auto& f = fingers_[pointer_id];
    f.active = true;
    f.x = x;
    f.y = y;
    f.start_x = x;
    f.start_y = y;
    f.zone = zone_at(x, y);

    switch (f.zone) {
        case TouchZone::ButtonA:
            if (!a_held_) a_pressed_ = true;
            a_held_ = true;
            break;
        case TouchZone::ButtonB:
            if (!b_held_) b_pressed_ = true;
            b_held_ = true;
            break;
        case TouchZone::ButtonMenu:
            if (!menu_held_) menu_pressed_ = true;
            menu_held_ = true;
            break;
        case TouchZone::DPad:
            update_dpad(pointer_id);
            break;
        default:
            break;
    }
}

void TouchControls::touch_move(int pointer_id, float x, float y) {
    if (pointer_id < 0 || pointer_id >= MAX_FINGERS) return;

    auto& f = fingers_[pointer_id];
    if (!f.active) return;
    f.x = x;
    f.y = y;

    if (f.zone == TouchZone::DPad) {
        update_dpad(pointer_id);
    }
}

void TouchControls::touch_up(int pointer_id) {
    if (pointer_id < 0 || pointer_id >= MAX_FINGERS) return;

    auto& f = fingers_[pointer_id];
    TouchZone was_zone = f.zone;
    f.active = false;
    f.zone = TouchZone::None;

    switch (was_zone) {
        case TouchZone::DPad:
            dpad_dir_ = {0.0f, 0.0f};
            break;
        case TouchZone::ButtonA:
            a_held_ = false;
            break;
        case TouchZone::ButtonB:
            b_held_ = false;
            break;
        case TouchZone::ButtonMenu:
            menu_held_ = false;
            break;
        default:
            break;
    }
}

void TouchControls::update_dpad(int pointer_id) {
    auto& f = fingers_[pointer_id];
    float dx = f.x - dpad_center_.x;
    float dy = f.y - dpad_center_.y;
    float len = std::sqrt(dx * dx + dy * dy);

    if (len < DPAD_DEAD_ZONE) {
        dpad_dir_ = {0.0f, 0.0f};
    } else {
        // Normalize and clamp to unit circle
        float nx = dx / len;
        float ny = dy / len;

        // Snap to 8 directions for cleaner movement
        float angle = std::atan2(ny, nx);
        // Round to nearest 45 degrees
        float snap = std::round(angle / (3.14159265f / 4.0f)) * (3.14159265f / 4.0f);
        dpad_dir_ = {std::cos(snap), std::sin(snap)};
    }
}

void TouchControls::apply_to(InputState& input) const {
    // D-pad directions
    int up = static_cast<int>(InputAction::MoveUp);
    int down = static_cast<int>(InputAction::MoveDown);
    int left = static_cast<int>(InputAction::MoveLeft);
    int right = static_cast<int>(InputAction::MoveRight);

    input.actions[up] = dpad_dir_.y < -0.3f;
    input.actions[down] = dpad_dir_.y > 0.3f;
    input.actions[left] = dpad_dir_.x < -0.3f;
    input.actions[right] = dpad_dir_.x > 0.3f;

    // Buttons
    int confirm = static_cast<int>(InputAction::Confirm);
    int cancel = static_cast<int>(InputAction::Cancel);
    int menu = static_cast<int>(InputAction::Menu);

    input.actions[confirm] = a_held_;
    input.actions_pressed[confirm] = a_pressed_;
    input.actions[cancel] = b_held_;
    input.actions_pressed[cancel] = b_pressed_;
    input.actions[menu] = menu_held_;
    input.actions_pressed[menu] = menu_pressed_;
}

// ── Rendering ──

static void draw_circle(SpriteBatch& batch, Vec2 center, float radius,
                        Vec4 color, int segments = 24) {
    // Approximate circle with small quads along the perimeter, plus fill
    // For simplicity, draw a filled diamond/rounded rect approximation
    float r = radius;
    // Center fill
    batch.draw_quad({center.x - r * 0.7f, center.y - r * 0.7f},
                    {r * 1.4f, r * 1.4f}, color);
    // Top/bottom caps
    batch.draw_quad({center.x - r * 0.5f, center.y - r},
                    {r, r * 0.3f}, color);
    batch.draw_quad({center.x - r * 0.5f, center.y + r * 0.7f},
                    {r, r * 0.3f}, color);
    // Left/right caps
    batch.draw_quad({center.x - r, center.y - r * 0.5f},
                    {r * 0.3f, r}, color);
    batch.draw_quad({center.x + r * 0.7f, center.y - r * 0.5f},
                    {r * 0.3f, r}, color);
}

static void draw_arrow(SpriteBatch& batch, Vec2 center, Vec2 dir,
                       float size, Vec4 color) {
    // Draw a small arrow pointing in dir
    float hs = size * 0.5f;
    Vec2 tip = {center.x + dir.x * hs, center.y + dir.y * hs};
    // Arrow as a small triangle approximation (quad rotated)
    float w = size * 0.35f;
    if (std::abs(dir.x) > std::abs(dir.y)) {
        // Horizontal arrow
        batch.draw_quad({tip.x - w * 0.5f, tip.y - w * 0.5f}, {w, w}, color);
    } else {
        // Vertical arrow
        batch.draw_quad({tip.x - w * 0.5f, tip.y - w * 0.5f}, {w, w}, color);
    }
}

void TouchControls::render(SpriteBatch& batch, int screen_w, int screen_h) const {
    // Set screen-space projection
    Mat4 proj = glm::ortho(0.0f, static_cast<float>(screen_w),
                           0.0f, static_cast<float>(screen_h),
                           -1.0f, 1.0f);
    batch.set_projection(proj);

    Vec4 bg_color = {0.2f, 0.2f, 0.2f, 0.3f};
    Vec4 active_color = {0.5f, 0.5f, 0.5f, 0.5f};
    Vec4 arrow_color = {0.9f, 0.9f, 0.9f, 0.5f};
    Vec4 arrow_active = {1.0f, 1.0f, 1.0f, 0.8f};

    // ── D-Pad base ──
    draw_circle(batch, dpad_center_, DPAD_RADIUS, bg_color);

    // D-pad directional arrows
    float arrow_dist = DPAD_RADIUS * 0.55f;
    float arrow_sz = DPAD_RADIUS * 0.45f;

    struct DirDef { Vec2 dir; bool active; };
    DirDef dirs[] = {
        {{0, -1}, dpad_dir_.y < -0.3f},  // Up
        {{0,  1}, dpad_dir_.y > 0.3f},   // Down
        {{-1, 0}, dpad_dir_.x < -0.3f},  // Left
        {{ 1, 0}, dpad_dir_.x > 0.3f},   // Right
    };
    for (const auto& d : dirs) {
        Vec2 pos = {dpad_center_.x + d.dir.x * arrow_dist - arrow_sz * 0.35f,
                    dpad_center_.y + d.dir.y * arrow_dist - arrow_sz * 0.35f};
        Vec2 sz = {arrow_sz * 0.7f, arrow_sz * 0.7f};
        batch.draw_quad(pos, sz, d.active ? arrow_active : arrow_color);
    }

    // D-pad knob showing current direction
    if (dpad_dir_.x != 0.0f || dpad_dir_.y != 0.0f) {
        float knob_dist = DPAD_RADIUS * 0.35f;
        Vec2 knob = {dpad_center_.x + dpad_dir_.x * knob_dist,
                     dpad_center_.y + dpad_dir_.y * knob_dist};
        draw_circle(batch, knob, 14.0f, {0.8f, 0.8f, 0.9f, 0.6f});
    }

    // ── Button A (Confirm) — green-ish ──
    Vec4 a_bg = a_held_ ? Vec4(0.2f, 0.7f, 0.3f, 0.6f) : Vec4(0.15f, 0.5f, 0.2f, 0.35f);
    draw_circle(batch, btn_a_center_, BUTTON_RADIUS, a_bg);
    // "A" label (small centered quad as placeholder)
    batch.draw_quad({btn_a_center_.x - 6, btn_a_center_.y - 8}, {12, 3},
                    {1.0f, 1.0f, 1.0f, 0.7f});
    batch.draw_quad({btn_a_center_.x - 8, btn_a_center_.y - 5}, {3, 13},
                    {1.0f, 1.0f, 1.0f, 0.7f});
    batch.draw_quad({btn_a_center_.x + 5, btn_a_center_.y - 5}, {3, 13},
                    {1.0f, 1.0f, 1.0f, 0.7f});

    // ── Button B (Cancel) — red-ish ──
    Vec4 b_bg = b_held_ ? Vec4(0.7f, 0.2f, 0.2f, 0.6f) : Vec4(0.5f, 0.15f, 0.15f, 0.35f);
    draw_circle(batch, btn_b_center_, BUTTON_RADIUS, b_bg);
    // "B" label
    batch.draw_quad({btn_b_center_.x - 6, btn_b_center_.y - 8}, {10, 3},
                    {1.0f, 1.0f, 1.0f, 0.7f});
    batch.draw_quad({btn_b_center_.x - 6, btn_b_center_.y - 1}, {10, 3},
                    {1.0f, 1.0f, 1.0f, 0.7f});
    batch.draw_quad({btn_b_center_.x - 6, btn_b_center_.y + 5}, {10, 3},
                    {1.0f, 1.0f, 1.0f, 0.7f});
    batch.draw_quad({btn_b_center_.x - 8, btn_b_center_.y - 8}, {3, 16},
                    {1.0f, 1.0f, 1.0f, 0.7f});

    // ── Menu button — small circle top-right ──
    Vec4 menu_bg = menu_held_ ? Vec4(0.6f, 0.6f, 0.3f, 0.6f) : Vec4(0.4f, 0.4f, 0.2f, 0.3f);
    draw_circle(batch, btn_menu_center_, 22.0f, menu_bg);
    // Three horizontal lines (hamburger)
    for (int i = 0; i < 3; i++) {
        float ly = btn_menu_center_.y - 8 + i * 8;
        batch.draw_quad({btn_menu_center_.x - 10, ly}, {20, 2},
                        {1.0f, 1.0f, 1.0f, 0.6f});
    }
}

} // namespace eb
