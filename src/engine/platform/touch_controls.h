#pragma once

#include "engine/core/types.h"
#include "engine/platform/input.h"

namespace eb {

class SpriteBatch;

// On-screen touch control zones
enum class TouchZone : uint8_t {
    None,
    DPad,
    ButtonA,    // Confirm
    ButtonB,    // Cancel
    ButtonMenu, // Pause/menu
};

// Tracks a single finger
struct TouchFinger {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float start_x = 0.0f;
    float start_y = 0.0f;
    TouchZone zone = TouchZone::None;
};

class TouchControls {
public:
    static constexpr int MAX_FINGERS = 5;
    static constexpr float DPAD_RADIUS = 80.0f;
    static constexpr float DPAD_DEAD_ZONE = 15.0f;
    static constexpr float BUTTON_RADIUS = 36.0f;
    static constexpr float MARGIN = 30.0f;

    TouchControls() = default;

    // Call each frame before processing touches
    void begin_frame(int screen_w, int screen_h);

    // Process touch events — call for each pointer
    void touch_down(int pointer_id, float x, float y);
    void touch_move(int pointer_id, float x, float y);
    void touch_up(int pointer_id);

    // Apply current touch state to InputState
    void apply_to(InputState& input) const;

    // Render on-screen controls
    void render(SpriteBatch& batch, int screen_w, int screen_h) const;

    // Get d-pad direction for rendering feedback
    Vec2 dpad_direction() const { return dpad_dir_; }

private:
    TouchZone zone_at(float x, float y) const;
    void update_dpad(int pointer_id);

    int screen_w_ = 0;
    int screen_h_ = 0;

    // Layout positions (computed in begin_frame)
    Vec2 dpad_center_ = {};
    Vec2 btn_a_center_ = {};
    Vec2 btn_b_center_ = {};
    Vec2 btn_menu_center_ = {};

    TouchFinger fingers_[MAX_FINGERS] = {};
    Vec2 dpad_dir_ = {0.0f, 0.0f};

    // Track which buttons were just pressed/released this frame
    bool a_held_ = false;
    bool b_held_ = false;
    bool menu_held_ = false;
    bool a_pressed_ = false;
    bool b_pressed_ = false;
    bool menu_pressed_ = false;
};

} // namespace eb
