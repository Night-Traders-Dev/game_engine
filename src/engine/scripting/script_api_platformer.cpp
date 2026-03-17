#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "engine/core/debug_log.h"

extern "C" {
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== Physics Control ===============

// set_gravity(float)
static Value native_set_gravity(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.gravity = (float)args[0].as.number;
    return val_nil();
}

// set_jump_force(float) — ensure negative (upward)
static Value native_set_jump_force(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.jump_force = -std::abs((float)args[0].as.number);
    return val_nil();
}

// set_max_fall_speed(float)
static Value native_set_max_fall_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.max_fall_speed = (float)args[0].as.number;
    return val_nil();
}

// set_coyote_time(float)
static Value native_set_coyote_time(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.coyote_time = (float)args[0].as.number;
    return val_nil();
}

// set_wall_slide(bool)
static Value native_set_wall_slide(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_BOOL)
        s_active_engine->game_state_->platformer.wall_slide_enabled = args[0].as.boolean;
    else if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.wall_slide_enabled = (args[0].as.number != 0);
    return val_nil();
}

// set_double_jump(bool)
static Value native_set_double_jump(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_BOOL)
        s_active_engine->game_state_->platformer.can_double_jump = args[0].as.boolean;
    else if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->platformer.can_double_jump = (args[0].as.number != 0);
    return val_nil();
}

// set_dash(bool enabled, float speed, float duration)
static Value native_set_dash(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* ps = &s_active_engine->game_state_->platformer;
    if (args[0].type == VAL_BOOL)
        ps->dash_enabled = args[0].as.boolean;
    else if (args[0].type == VAL_NUMBER)
        ps->dash_enabled = (args[0].as.number != 0);
    if (argc >= 2 && args[1].type == VAL_NUMBER)
        ps->dash_speed = (float)args[1].as.number;
    if (argc >= 3 && args[2].type == VAL_NUMBER)
        ps->dash_duration = (float)args[2].as.number;
    return val_nil();
}

// get_velocity_x() -> number
static Value native_get_velocity_x(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->platformer.velocity.x);
}

// get_velocity_y() -> number
static Value native_get_velocity_y(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->platformer.velocity.y);
}

// set_velocity(x, y)
static Value native_set_velocity(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* ps = &s_active_engine->game_state_->platformer;
    if (args[0].type == VAL_NUMBER) ps->velocity.x = (float)args[0].as.number;
    if (args[1].type == VAL_NUMBER) ps->velocity.y = (float)args[1].as.number;
    return val_nil();
}

// is_on_ground() -> bool
static Value native_is_on_ground(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->platformer.on_ground ? 1 : 0);
}

// is_on_wall() -> bool
static Value native_is_on_wall(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->platformer.on_wall ? 1 : 0);
}

// is_on_ladder() -> bool
static Value native_is_on_ladder(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->platformer.on_ladder ? 1 : 0);
}

// get_player_state() -> string ("idle", "run", "jump", etc.)
static Value native_get_player_state(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_string("idle");
    auto st = s_active_engine->game_state_->platformer.state;
    switch (st) {
        case PlayerPlatformState::Idle:      return val_string("idle");
        case PlayerPlatformState::Run:       return val_string("run");
        case PlayerPlatformState::Jump:      return val_string("jump");
        case PlayerPlatformState::Fall:      return val_string("fall");
        case PlayerPlatformState::WallSlide: return val_string("wall_slide");
        case PlayerPlatformState::Climb:     return val_string("climb");
        case PlayerPlatformState::Crouch:    return val_string("crouch");
        case PlayerPlatformState::Dash:      return val_string("dash");
    }
    return val_string("idle");
}

// =============== Enemy AI ===============

// npc_set_patrol(name, min_x, max_x, speed)
static Value native_npc_set_patrol(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    if (args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    if (args[1].type == VAL_NUMBER) npc->patrol_min_x = (float)args[1].as.number;
    if (args[2].type == VAL_NUMBER) npc->patrol_max_x = (float)args[2].as.number;
    if (args[3].type == VAL_NUMBER) npc->move_speed = (float)args[3].as.number;
    npc->platform_ai = NPC::PlatformAI::Patrol;
    return val_nil();
}

// npc_set_platform_ai(name, type_string)
static Value native_npc_set_platform_ai(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    const char* t = args[1].as.string;
    if (std::strcmp(t, "patrol") == 0)      npc->platform_ai = NPC::PlatformAI::Patrol;
    else if (std::strcmp(t, "jump") == 0)   npc->platform_ai = NPC::PlatformAI::Jump;
    else if (std::strcmp(t, "fly") == 0)    npc->platform_ai = NPC::PlatformAI::Fly;
    else if (std::strcmp(t, "none") == 0)   npc->platform_ai = NPC::PlatformAI::None;
    return val_nil();
}

// npc_set_stompable(name, bool)
static Value native_npc_set_stompable(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    if (args[1].type == VAL_BOOL) npc->stompable = args[1].as.boolean;
    else if (args[1].type == VAL_NUMBER) npc->stompable = (args[1].as.number != 0);
    return val_nil();
}

// npc_set_contact_damage(name, int)
static Value native_npc_set_contact_damage(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    if (args[1].type == VAL_NUMBER) npc->contact_damage = (int)args[1].as.number;
    return val_nil();
}

// npc_on_stomp(name, callback)
static Value native_npc_on_stomp(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    npc->on_stomp_func = args[1].as.string;
    return val_nil();
}

// npc_on_contact(name, callback)
static Value native_npc_on_contact(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    npc->on_contact_func = args[1].as.string;
    return val_nil();
}

// =============== Collectibles ===============

// spawn_collectible(x, y, id, name, callback)
static Value native_spawn_collectible(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    WorldDrop drop;
    if (args[0].type == VAL_NUMBER) drop.position.x = (float)args[0].as.number;
    if (args[1].type == VAL_NUMBER) drop.position.y = (float)args[1].as.number;
    if (args[2].type == VAL_STRING) drop.item_id = args[2].as.string;
    if (args[3].type == VAL_STRING) drop.item_name = args[3].as.string;
    if (args[4].type == VAL_STRING) drop.sage_func = args[4].as.string;
    drop.pickup_radius = 20.0f;
    drop.lifetime = 9999.0f;
    gs->world_drops.push_back(drop);
    return val_nil();
}

// spawn_coin(x, y) — convenience: spawn_collectible with id="coin", icon="fi_86"
static Value native_spawn_coin(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    WorldDrop drop;
    if (args[0].type == VAL_NUMBER) drop.position.x = (float)args[0].as.number;
    if (args[1].type == VAL_NUMBER) drop.position.y = (float)args[1].as.number;
    drop.item_id = "coin";
    drop.item_name = "Coin";
    drop.pickup_radius = 20.0f;
    drop.lifetime = 9999.0f;
    gs->world_drops.push_back(drop);
    return val_nil();
}

// =============== Moving Platforms ===============

// add_platform(x, y, w, h, tile_id) -> index
static Value native_add_platform(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_number(-1);
    auto* gs = s_active_engine->game_state_;
    MovingPlatform plat;
    if (args[0].type == VAL_NUMBER) plat.position.x = (float)args[0].as.number;
    if (args[1].type == VAL_NUMBER) plat.position.y = (float)args[1].as.number;
    if (args[2].type == VAL_NUMBER) plat.width = (float)args[2].as.number;
    if (args[3].type == VAL_NUMBER) plat.height = (float)args[3].as.number;
    if (args[4].type == VAL_NUMBER) plat.tile_id = (int)args[4].as.number;
    plat.active = false;
    int idx = (int)gs->moving_platforms.size();
    gs->moving_platforms.push_back(plat);
    return val_number(idx);
}

// platform_add_waypoint(idx, x, y)
static Value native_platform_add_waypoint(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (args[0].type != VAL_NUMBER) return val_nil();
    int idx = (int)args[0].as.number;
    if (idx < 0 || idx >= (int)gs->moving_platforms.size()) return val_nil();
    eb::Vec2 wp = {0, 0};
    if (args[1].type == VAL_NUMBER) wp.x = (float)args[1].as.number;
    if (args[2].type == VAL_NUMBER) wp.y = (float)args[2].as.number;
    gs->moving_platforms[idx].path.push_back(wp);
    return val_nil();
}

// platform_set_speed(idx, float)
static Value native_platform_set_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (args[0].type != VAL_NUMBER) return val_nil();
    int idx = (int)args[0].as.number;
    if (idx < 0 || idx >= (int)gs->moving_platforms.size()) return val_nil();
    if (args[1].type == VAL_NUMBER)
        gs->moving_platforms[idx].speed = (float)args[1].as.number;
    return val_nil();
}

// platform_start(idx)
static Value native_platform_start(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (args[0].type != VAL_NUMBER) return val_nil();
    int idx = (int)args[0].as.number;
    if (idx < 0 || idx >= (int)gs->moving_platforms.size()) return val_nil();
    gs->moving_platforms[idx].active = true;
    return val_nil();
}

// platform_stop(idx)
static Value native_platform_stop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (args[0].type != VAL_NUMBER) return val_nil();
    int idx = (int)args[0].as.number;
    if (idx < 0 || idx >= (int)gs->moving_platforms.size()) return val_nil();
    gs->moving_platforms[idx].active = false;
    return val_nil();
}

// =============== Mode ===============

// set_game_type(string) — "topdown" or "platformer"
static Value native_set_game_type(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type != VAL_STRING) return val_nil();
    const char* t = args[0].as.string;
    if (std::strcmp(t, "topdown") == 0)
        s_active_engine->game_state_->game_type = GameType::TopDown;
    else if (std::strcmp(t, "platformer") == 0)
        s_active_engine->game_state_->game_type = GameType::Platformer;
    return val_nil();
}

// get_game_type() -> string
static Value native_get_game_type(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_string("topdown");
    if (s_active_engine->game_state_->game_type == GameType::Platformer)
        return val_string("platformer");
    return val_string("topdown");
}

// =============== Animation ===============

// set_platformer_anim(state_name, anim_name)
static Value native_set_platformer_anim(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) return val_nil();
    auto* ps = &s_active_engine->game_state_->platformer;
    const char* state = args[0].as.string;
    const char* anim = args[1].as.string;
    if (std::strcmp(state, "idle") == 0)           ps->anim_idle = anim;
    else if (std::strcmp(state, "run") == 0)        ps->anim_run = anim;
    else if (std::strcmp(state, "jump") == 0)       ps->anim_jump = anim;
    else if (std::strcmp(state, "fall") == 0)       ps->anim_fall = anim;
    else if (std::strcmp(state, "wall_slide") == 0) ps->anim_wall_slide = anim;
    else if (std::strcmp(state, "climb") == 0)      ps->anim_climb = anim;
    else if (std::strcmp(state, "crouch") == 0)     ps->anim_crouch = anim;
    else if (std::strcmp(state, "dash") == 0)       ps->anim_dash = anim;
    return val_nil();
}

// =============== Register ===============

void ScriptEngine::register_platformer_api() {
    if (!env_) return;
    // Physics control
    env_define(env_, "set_gravity", 11, val_native(native_set_gravity));
    env_define(env_, "set_jump_force", 14, val_native(native_set_jump_force));
    env_define(env_, "set_max_fall_speed", 18, val_native(native_set_max_fall_speed));
    env_define(env_, "set_coyote_time", 15, val_native(native_set_coyote_time));
    env_define(env_, "set_wall_slide", 14, val_native(native_set_wall_slide));
    env_define(env_, "set_double_jump", 15, val_native(native_set_double_jump));
    env_define(env_, "set_dash", 8, val_native(native_set_dash));
    env_define(env_, "get_velocity_x", 14, val_native(native_get_velocity_x));
    env_define(env_, "get_velocity_y", 14, val_native(native_get_velocity_y));
    env_define(env_, "set_velocity", 12, val_native(native_set_velocity));
    env_define(env_, "is_on_ground", 12, val_native(native_is_on_ground));
    env_define(env_, "is_on_wall", 10, val_native(native_is_on_wall));
    env_define(env_, "is_on_ladder", 12, val_native(native_is_on_ladder));
    env_define(env_, "get_player_state", 16, val_native(native_get_player_state));
    // Enemy AI
    env_define(env_, "npc_set_patrol", 14, val_native(native_npc_set_patrol));
    env_define(env_, "npc_set_platform_ai", 19, val_native(native_npc_set_platform_ai));
    env_define(env_, "npc_set_stompable", 17, val_native(native_npc_set_stompable));
    env_define(env_, "npc_set_contact_damage", 22, val_native(native_npc_set_contact_damage));
    env_define(env_, "npc_on_stomp", 12, val_native(native_npc_on_stomp));
    env_define(env_, "npc_on_contact", 14, val_native(native_npc_on_contact));
    // Collectibles
    env_define(env_, "spawn_collectible", 17, val_native(native_spawn_collectible));
    env_define(env_, "spawn_coin", 10, val_native(native_spawn_coin));
    // Moving platforms
    env_define(env_, "add_platform", 12, val_native(native_add_platform));
    env_define(env_, "platform_add_waypoint", 21, val_native(native_platform_add_waypoint));
    env_define(env_, "platform_set_speed", 18, val_native(native_platform_set_speed));
    env_define(env_, "platform_start", 14, val_native(native_platform_start));
    env_define(env_, "platform_stop", 13, val_native(native_platform_stop));
    // Mode
    env_define(env_, "set_game_type", 13, val_native(native_set_game_type));
    env_define(env_, "get_game_type", 13, val_native(native_get_game_type));
    // Animation
    env_define(env_, "set_platformer_anim", 19, val_native(native_set_platformer_anim));
    std::printf("[ScriptEngine] Platformer API registered (31 functions)\n");
}

} // namespace eb
