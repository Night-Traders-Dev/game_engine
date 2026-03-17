#pragma once

#include "engine/core/types.h"

namespace eb {

class Camera {
public:
    Camera();
    Camera(float viewport_w, float viewport_h);

    void set_viewport(float w, float h);
    void set_position(Vec2 pos);
    void set_bounds(float min_x, float min_y, float max_x, float max_y);
    void clear_bounds();

    // Follow offset: shifts where the player appears on screen
    // e.g., {0, -60} puts the player 60px below center (Twilight style)
    void set_follow_offset(Vec2 offset) { follow_offset_ = offset; }

    void follow(Vec2 target, float speed = 5.0f);
    void follow_platformer(Vec2 target, int facing_dir, bool on_ground, float speed = 5.0f);
    void center_on(Vec2 target);

    void update(float dt);

    Vec2 position() const { return position_; }
    Vec2 viewport() const { return viewport_; }

    // World-to-screen offset: subtract from world coords to get screen coords
    Vec2 offset() const;

    // Visible area in world coordinates
    Rect visible_area() const;

    // Orthographic projection incorporating camera offset
    Mat4 projection_matrix() const;

private:
    void clamp_to_bounds();

    Vec2 position_ = {0.0f, 0.0f};
    Vec2 viewport_ = {960.0f, 720.0f};
    Vec2 follow_offset_ = {0.0f, 0.0f};

    bool has_bounds_ = false;
    float bounds_min_x_ = 0, bounds_min_y_ = 0;
    float bounds_max_x_ = 0, bounds_max_y_ = 0;

    Vec2 target_ = {0.0f, 0.0f};
    bool following_ = false;
    float follow_speed_ = 5.0f;

    // Platformer camera settings
    float deadzone_y_top_ = 0.35f;
    float deadzone_y_bottom_ = 0.35f;
    float lookahead_x_ = 80.0f;
    float lookahead_speed_ = 3.0f;
    float vertical_snap_speed_ = 8.0f;
    float current_lookahead_ = 0.0f;
    bool platformer_mode_ = false;
};

} // namespace eb
