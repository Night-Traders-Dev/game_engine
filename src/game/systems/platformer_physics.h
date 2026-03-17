#pragma once
#include "engine/core/types.h"
#include <string>

namespace eb {

enum class PlayerPlatformState : uint8_t {
    Idle, Run, Jump, Fall, WallSlide, Climb, Crouch, Dash
};

struct PlatformerState {
    eb::Vec2 velocity = {0, 0};
    float gravity = 980.0f;
    float max_fall_speed = 600.0f;
    float move_speed = 200.0f;
    float run_speed = 320.0f;
    bool on_ground = false;
    bool on_wall = false;
    bool on_ladder = false;
    int wall_dir = 0;
    int facing_dir = 1; // 1=right, -1=left

    // Jumping
    float jump_force = -380.0f;
    float jump_hold_gravity = 400.0f;
    float coyote_timer = 0.0f;
    float coyote_time = 0.08f;
    float jump_buffer_timer = 0.0f;
    float jump_buffer_time = 0.12f;
    bool jump_held = false;
    bool can_double_jump = false;
    bool used_double_jump = false;

    // Wall slide/jump
    float wall_slide_speed = 60.0f;
    float wall_jump_force_x = 280.0f;
    float wall_jump_force_y = -350.0f;
    bool wall_slide_enabled = false;

    // Dash
    float dash_speed = 500.0f;
    float dash_duration = 0.15f;
    float dash_timer = 0.0f;
    float dash_cooldown = 0.0f;
    int dash_dir = 1;
    bool can_dash = true;
    bool dash_enabled = false;

    // State
    PlayerPlatformState state = PlayerPlatformState::Idle;
    PlayerPlatformState prev_state = PlayerPlatformState::Idle;

    // Animation bindings
    std::string anim_idle = "idle";
    std::string anim_run = "run";
    std::string anim_jump = "jump";
    std::string anim_fall = "fall";
    std::string anim_wall_slide = "wall_slide";
    std::string anim_climb = "climb";
    std::string anim_crouch = "crouch";
    std::string anim_dash = "dash";

    // Player hitbox for platformer (relative to player_pos center-bottom)
    float hitbox_w = 14.0f;
    float hitbox_h = 24.0f;
};

} // namespace eb
