#include "engine/platform/touch_controls.h"
#include "engine/graphics/sprite_batch.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace eb {

void TouchControls::begin_frame(int screen_w, int screen_h) {
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    // Scale controls based on the shorter screen dimension (height in landscape)
    float short_side = static_cast<float>(std::min(screen_w, screen_h));
    scale_ = std::max(1.0f, short_side / 720.0f);

    dpad_radius_ = BASE_DPAD_RADIUS * scale_;
    dpad_dead_zone_ = BASE_DPAD_DEAD_ZONE * scale_;
    button_radius_ = BASE_BUTTON_RADIUS * scale_;
    margin_ = BASE_MARGIN * scale_;

    // D-pad: bottom-left, generous margin from edges
    dpad_center_ = {margin_ + dpad_radius_ + 20.0f * scale_,
                    screen_h - margin_ - dpad_radius_ - 20.0f * scale_};

    // Action buttons: bottom-right — A lower-right, B to its left
    float btn_base_x = screen_w - margin_ - button_radius_ - 20.0f * scale_;
    float btn_base_y = screen_h - margin_ - button_radius_ - 30.0f * scale_;
    btn_a_center_ = {btn_base_x, btn_base_y};
    btn_b_center_ = {btn_base_x - button_radius_ * 2.5f, btn_base_y - button_radius_ * 1.2f};

    // Menu button: top-right corner, bigger touch target
    float menu_r = 30.0f * scale_;
    btn_menu_center_ = {static_cast<float>(screen_w) - margin_ - menu_r,
                        margin_ + menu_r + 8.0f * scale_};

    // Reset per-frame flags
    a_pressed_ = false;
    b_pressed_ = false;
    menu_pressed_ = false;
}

TouchZone TouchControls::zone_at(float x, float y) const {
    float touch_pad = 20.0f * scale_;

    float dx = x - dpad_center_.x;
    float dy = y - dpad_center_.y;
    float dpad_touch = dpad_radius_ + touch_pad;
    if (dx * dx + dy * dy < dpad_touch * dpad_touch)
        return TouchZone::DPad;

    dx = x - btn_a_center_.x;
    dy = y - btn_a_center_.y;
    float btn_touch = button_radius_ + touch_pad;
    if (dx * dx + dy * dy < btn_touch * btn_touch)
        return TouchZone::ButtonA;

    dx = x - btn_b_center_.x;
    dy = y - btn_b_center_.y;
    if (dx * dx + dy * dy < btn_touch * btn_touch)
        return TouchZone::ButtonB;

    dx = x - btn_menu_center_.x;
    dy = y - btn_menu_center_.y;
    float menu_touch = 35.0f * scale_;
    if (dx * dx + dy * dy < menu_touch * menu_touch)
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

    if (len < dpad_dead_zone_) {
        dpad_dir_ = {0.0f, 0.0f};
    } else {
        float nx = dx / len;
        float ny = dy / len;
        float angle = std::atan2(ny, nx);
        float snap = std::round(angle / (3.14159265f / 4.0f)) * (3.14159265f / 4.0f);
        dpad_dir_ = {std::cos(snap), std::sin(snap)};
    }
}

void TouchControls::apply_to(InputState& input) const {
    int up = static_cast<int>(InputAction::MoveUp);
    int down = static_cast<int>(InputAction::MoveDown);
    int left = static_cast<int>(InputAction::MoveLeft);
    int right = static_cast<int>(InputAction::MoveRight);

    input.actions[up] = dpad_dir_.y < -0.3f;
    input.actions[down] = dpad_dir_.y > 0.3f;
    input.actions[left] = dpad_dir_.x < -0.3f;
    input.actions[right] = dpad_dir_.x > 0.3f;

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

// ═══════════════════════════════════════════════════════════════════
// Modern Mobile UI Rendering
// ═══════════════════════════════════════════════════════════════════
//
// Design principles:
// - Soft circles via many-segment approximation (16+ quads per ring)
// - Drop shadows for depth separation from game world
// - High-contrast colors following mobile game UI conventions
// - Consistent rounded shapes, clear visual hierarchy
// - Generous opacity for visibility without obstructing gameplay

// Draw a filled circle using radial segments (much smoother than 5-quad cross)
static void draw_filled_circle(SpriteBatch& batch, Vec2 center, float radius, Vec4 color, int segments = 20) {
    // Approximate circle with triangular fan using narrow pie-slice quads
    float step = 2.0f * 3.14159265f / (float)segments;
    for (int i = 0; i < segments; i++) {
        float a0 = i * step;
        float a1 = (i + 1) * step;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);
        // Draw as a quad from center to two edge points
        Vec2 p0 = center;
        Vec2 p1 = {center.x + c0 * radius, center.y + s0 * radius};
        Vec2 p2 = {center.x + c1 * radius, center.y + s1 * radius};
        // Bounding box of triangle, draw as tinted quad
        float min_x = std::min({p0.x, p1.x, p2.x});
        float min_y = std::min({p0.y, p1.y, p2.y});
        float max_x = std::max({p0.x, p1.x, p2.x});
        float max_y = std::max({p0.y, p1.y, p2.y});
        // Use a small centered quad to approximate each segment
        Vec2 mid = {(p0.x + p1.x + p2.x) / 3.0f, (p0.y + p1.y + p2.y) / 3.0f};
        float seg_r = radius * 0.58f; // Covers the segment area
        batch.draw_quad({mid.x - seg_r, mid.y - seg_r}, {seg_r * 2, seg_r * 2}, color);
    }
}

// Draw a filled circle using concentric overlapping quads (better fill)
static void draw_disc(SpriteBatch& batch, Vec2 c, float r, Vec4 color) {
    // Core (largest inscribed square)
    float s = r * 0.707f; // r / sqrt(2)
    batch.draw_quad({c.x - s, c.y - s}, {s * 2, s * 2}, color);
    // Horizontal bar (wider but shorter)
    batch.draw_quad({c.x - r, c.y - s * 0.72f}, {r * 2, s * 1.44f}, color);
    // Vertical bar (taller but narrower)
    batch.draw_quad({c.x - s * 0.72f, c.y - r}, {s * 1.44f, r * 2}, color);
    // Diagonal fills for rounder shape
    float d = r * 0.92f;
    float ds = r * 0.52f;
    batch.draw_quad({c.x - d, c.y - ds}, {d * 2, ds * 2}, color);
    batch.draw_quad({c.x - ds, c.y - d}, {ds * 2, d * 2}, color);
    // 45-degree corner fills
    float cr = r * 0.38f;
    float co = r * 0.62f;
    batch.draw_quad({c.x - co - cr * 0.5f, c.y - co - cr * 0.5f}, {cr * 1.5f, cr * 1.5f}, color);
    batch.draw_quad({c.x + co - cr, c.y - co - cr * 0.5f}, {cr * 1.5f, cr * 1.5f}, color);
    batch.draw_quad({c.x - co - cr * 0.5f, c.y + co - cr}, {cr * 1.5f, cr * 1.5f}, color);
    batch.draw_quad({c.x + co - cr, c.y + co - cr}, {cr * 1.5f, cr * 1.5f}, color);
}

// Draw a ring (circle outline) using quads along the circumference
static void draw_ring(SpriteBatch& batch, Vec2 center, float radius, float thickness, Vec4 color) {
    int segments = 24;
    float step = 2.0f * 3.14159265f / (float)segments;
    float half_t = thickness * 0.5f;
    for (int i = 0; i < segments; i++) {
        float a = (i + 0.5f) * step;
        float cx = center.x + std::cos(a) * radius;
        float cy = center.y + std::sin(a) * radius;
        // Each segment is a small square along the ring
        float seg_w = radius * step * 0.55f + half_t;
        batch.draw_quad({cx - seg_w, cy - half_t}, {seg_w * 2, thickness}, color);
    }
}

// Draw an arrow triangle pointing in a direction
static void draw_arrow(SpriteBatch& batch, Vec2 center, float size, Vec2 dir, Vec4 color) {
    // Arrow as 3 overlapping quads forming a triangle shape
    float s = size;
    float hs = s * 0.5f;
    if (std::abs(dir.x) > std::abs(dir.y)) {
        // Horizontal arrow
        float sx = dir.x > 0 ? 1.0f : -1.0f;
        // Tip
        batch.draw_quad({center.x + sx * hs * 0.3f, center.y - s * 0.12f}, {hs * 0.4f, s * 0.24f}, color);
        // Middle
        batch.draw_quad({center.x - sx * hs * 0.1f, center.y - s * 0.25f}, {hs * 0.5f, s * 0.5f}, color);
        // Base
        batch.draw_quad({center.x - sx * hs * 0.5f, center.y - s * 0.38f}, {hs * 0.4f, s * 0.76f}, color);
    } else {
        // Vertical arrow
        float sy = dir.y > 0 ? 1.0f : -1.0f;
        batch.draw_quad({center.x - s * 0.12f, center.y + sy * hs * 0.3f}, {s * 0.24f, hs * 0.4f}, color);
        batch.draw_quad({center.x - s * 0.25f, center.y - sy * hs * 0.1f}, {s * 0.5f, hs * 0.5f}, color);
        batch.draw_quad({center.x - s * 0.38f, center.y - sy * hs * 0.5f}, {s * 0.76f, hs * 0.4f}, color);
    }
}

void TouchControls::render(SpriteBatch& batch, int screen_w, int screen_h) const {
    Mat4 proj = glm::ortho(0.0f, static_cast<float>(screen_w),
                           0.0f, static_cast<float>(screen_h),
                           -1.0f, 1.0f);
    batch.set_projection(proj);

    float ls = scale_;

    // ══════════════════════════════════════════════════════════════
    // Color Palette (Material Design inspired, high-contrast mobile)
    // ══════════════════════════════════════════════════════════════
    // Primary:   #4FC3F7 (light blue) → action, interactive
    // Confirm:   #66BB6A (green 400)  → accept, confirm
    // Cancel:    #EF5350 (red 400)    → back, cancel
    // Menu:      #FFA726 (orange 400) → menu, settings
    // Base:      #263238 (blue-grey 900) → backgrounds
    // Highlight: #FFFFFF at low alpha → pressed state glow

    Vec4 col_base      = {0.15f, 0.19f, 0.22f, 0.50f}; // Blue-grey 900
    Vec4 col_base_lite = {0.22f, 0.28f, 0.32f, 0.40f}; // Lighter base
    Vec4 col_ring      = {0.31f, 0.76f, 0.97f, 0.30f}; // Light blue ring
    Vec4 col_shadow    = {0.0f, 0.0f, 0.0f, 0.25f};    // Drop shadow

    Vec4 col_confirm      = {0.40f, 0.73f, 0.42f, 0.55f}; // Green 400
    Vec4 col_confirm_held = {0.55f, 0.88f, 0.55f, 0.75f}; // Green 300 bright
    Vec4 col_cancel       = {0.94f, 0.33f, 0.31f, 0.55f}; // Red 400
    Vec4 col_cancel_held  = {1.00f, 0.48f, 0.45f, 0.75f}; // Red 300 bright
    Vec4 col_menu         = {1.00f, 0.65f, 0.15f, 0.55f}; // Orange 400
    Vec4 col_menu_held    = {1.00f, 0.80f, 0.30f, 0.75f}; // Orange 300 bright

    Vec4 col_dpad_arrow        = {0.85f, 0.90f, 0.95f, 0.55f}; // Soft white
    Vec4 col_dpad_arrow_active = {0.31f, 0.76f, 0.97f, 0.90f}; // Light blue active
    Vec4 col_knob              = {0.31f, 0.76f, 0.97f, 0.60f}; // Knob highlight
    Vec4 col_label             = {1.0f, 1.0f, 1.0f, 0.85f};    // Button labels

    // ══════════════════════════════════════════════════════════════
    // D-Pad (bottom-left)
    // ══════════════════════════════════════════════════════════════

    // Shadow
    draw_disc(batch, {dpad_center_.x + 2*ls, dpad_center_.y + 3*ls}, dpad_radius_ + 4*ls, col_shadow);
    // Base disc
    draw_disc(batch, dpad_center_, dpad_radius_, col_base);
    // Subtle ring outline
    draw_ring(batch, dpad_center_, dpad_radius_ - 2*ls, 3.0f * ls, col_ring);

    // Directional arrows — triangular indicators
    float arrow_dist = dpad_radius_ * 0.55f;
    float arrow_sz = dpad_radius_ * 0.35f;
    struct DirDef { Vec2 dir; bool active; };
    DirDef dirs[] = {
        {{0, -1}, dpad_dir_.y < -0.3f},  // Up
        {{0,  1}, dpad_dir_.y >  0.3f},  // Down
        {{-1, 0}, dpad_dir_.x < -0.3f},  // Left
        {{ 1, 0}, dpad_dir_.x >  0.3f},  // Right
    };
    for (const auto& d : dirs) {
        Vec2 pos = {dpad_center_.x + d.dir.x * arrow_dist,
                    dpad_center_.y + d.dir.y * arrow_dist};
        Vec4 col = d.active ? col_dpad_arrow_active : col_dpad_arrow;
        draw_arrow(batch, pos, arrow_sz, d.dir, col);
    }

    // Knob indicator when direction is pressed
    if (dpad_dir_.x != 0.0f || dpad_dir_.y != 0.0f) {
        float knob_dist = dpad_radius_ * 0.30f;
        Vec2 knob = {dpad_center_.x + dpad_dir_.x * knob_dist,
                     dpad_center_.y + dpad_dir_.y * knob_dist};
        draw_disc(batch, knob, 16.0f * ls, col_knob);
    }

    // Center dot
    draw_disc(batch, dpad_center_, 6.0f * ls, col_base_lite);

    // ══════════════════════════════════════════════════════════════
    // Button A — Confirm (green, bottom-right)
    // ══════════════════════════════════════════════════════════════
    {
        Vec4 bg = a_held_ ? col_confirm_held : col_confirm;
        // Shadow
        draw_disc(batch, {btn_a_center_.x + 2*ls, btn_a_center_.y + 3*ls}, button_radius_ + 2*ls, col_shadow);
        // Base
        draw_disc(batch, btn_a_center_, button_radius_, bg);
        // Ring
        if (!a_held_)
            draw_ring(batch, btn_a_center_, button_radius_ - 2*ls, 2.5f * ls, {0.5f, 0.85f, 0.5f, 0.25f});

        // "A" letter — drawn with 3 quads: two vertical strokes + horizontal bar
        float lw = 3.0f * ls;  // Line width
        float lh = 16.0f * ls; // Letter height
        float lo = 7.0f * ls;  // Half letter width
        Vec2 bc = btn_a_center_;
        // Left stroke
        batch.draw_quad({bc.x - lo, bc.y - lh*0.5f + 2*ls}, {lw, lh - 2*ls}, col_label);
        // Right stroke
        batch.draw_quad({bc.x + lo - lw, bc.y - lh*0.5f + 2*ls}, {lw, lh - 2*ls}, col_label);
        // Top bar (apex)
        batch.draw_quad({bc.x - lo + lw, bc.y - lh*0.5f}, {(lo - lw) * 2, lw}, col_label);
        // Middle bar
        batch.draw_quad({bc.x - lo + lw*1.5f, bc.y}, {(lo - lw) * 1.4f, lw * 0.8f}, col_label);
    }

    // ══════════════════════════════════════════════════════════════
    // Button B — Cancel (red, upper-left of A)
    // ══════════════════════════════════════════════════════════════
    {
        Vec4 bg = b_held_ ? col_cancel_held : col_cancel;
        draw_disc(batch, {btn_b_center_.x + 2*ls, btn_b_center_.y + 3*ls}, button_radius_ + 2*ls, col_shadow);
        draw_disc(batch, btn_b_center_, button_radius_, bg);
        if (!b_held_)
            draw_ring(batch, btn_b_center_, button_radius_ - 2*ls, 2.5f * ls, {0.9f, 0.4f, 0.4f, 0.25f});

        // "B" letter — vertical stroke + two bumps (3 horizontal bars + left vertical)
        float lw = 3.0f * ls;
        float lh = 16.0f * ls;
        float lo = 6.0f * ls;
        Vec2 bc = btn_b_center_;
        // Left vertical
        batch.draw_quad({bc.x - lo, bc.y - lh*0.5f}, {lw, lh}, col_label);
        // Top horizontal
        batch.draw_quad({bc.x - lo + lw, bc.y - lh*0.5f}, {lo * 1.2f, lw}, col_label);
        // Middle horizontal
        batch.draw_quad({bc.x - lo + lw, bc.y - lw*0.4f}, {lo * 1.3f, lw}, col_label);
        // Bottom horizontal
        batch.draw_quad({bc.x - lo + lw, bc.y + lh*0.5f - lw}, {lo * 1.2f, lw}, col_label);
        // Right upper bump
        batch.draw_quad({bc.x + lo * 0.3f, bc.y - lh*0.5f + lw}, {lw, lh*0.5f - lw * 1.4f}, col_label);
        // Right lower bump
        batch.draw_quad({bc.x + lo * 0.4f, bc.y + lw * 0.6f}, {lw, lh*0.5f - lw * 1.6f}, col_label);
    }

    // ══════════════════════════════════════════════════════════════
    // Menu Button — hamburger (orange, top-right)
    // ══════════════════════════════════════════════════════════════
    {
        float mr = 30.0f * ls;
        Vec4 bg = menu_held_ ? col_menu_held : col_menu;
        // Shadow
        draw_disc(batch, {btn_menu_center_.x + 1.5f*ls, btn_menu_center_.y + 2.5f*ls}, mr + 2*ls, col_shadow);
        // Base
        draw_disc(batch, btn_menu_center_, mr, bg);
        // Ring
        if (!menu_held_)
            draw_ring(batch, btn_menu_center_, mr - 2*ls, 2.0f * ls, {1.0f, 0.75f, 0.3f, 0.25f});

        // Hamburger icon — 3 horizontal lines with rounded ends
        float line_w = 18.0f * ls;
        float line_h = 2.5f * ls;
        float spacing = 6.0f * ls;
        Vec4 lc = col_label;
        for (int i = -1; i <= 1; i++) {
            float ly = btn_menu_center_.y + i * spacing;
            // Main line
            batch.draw_quad({btn_menu_center_.x - line_w * 0.5f, ly - line_h * 0.5f},
                           {line_w, line_h}, lc);
            // Rounded end caps (small squares at ends)
            float cap = line_h * 0.6f;
            batch.draw_quad({btn_menu_center_.x - line_w * 0.5f - cap * 0.3f, ly - cap * 0.5f},
                           {cap, cap}, lc);
            batch.draw_quad({btn_menu_center_.x + line_w * 0.5f - cap * 0.7f, ly - cap * 0.5f},
                           {cap, cap}, lc);
        }
    }
}

} // namespace eb
