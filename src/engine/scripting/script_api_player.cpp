#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "engine/core/debug_log.h"
#include "engine/resource/file_io.h"

extern "C" {
#include "interpreter.h"
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <string>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== Skills API ===============

static CharacterStats* resolve_skills(const char* character) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    auto* gs = s_active_engine->game_state_;
    if (std::strcmp(character, "player") == 0 || std::strcmp(character, "Player") == 0)
        return &gs->player_stats;
    if (std::strcmp(character, "ally") == 0 || std::strcmp(character, "Ally") == 0)
        return &gs->ally_stats;
    return nullptr;
}

static int* resolve_skill_field(CharacterStats* skills, const char* name) {
    if (!skills) return nullptr;
    if (std::strcmp(name, "vitality") == 0)   return &skills->vitality;
    if (std::strcmp(name, "arcana") == 0)     return &skills->arcana;
    if (std::strcmp(name, "agility") == 0)    return &skills->agility;
    if (std::strcmp(name, "tactics") == 0)    return &skills->tactics;
    if (std::strcmp(name, "spirit") == 0)     return &skills->spirit;
    if (std::strcmp(name, "strength") == 0)   return &skills->strength;
    return nullptr;
}

// get_skill(character, skill_name) -> number
static Value native_get_skill(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_number(0);
    auto* skills = resolve_skills(args[0].as.string);
    auto* field = resolve_skill_field(skills, args[1].as.string);
    return val_number(field ? *field : 0);
}

// set_skill(character, skill_name, value)
static Value native_set_skill(int argc, Value* args) {
    if (argc < 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_nil();
    auto* skills = resolve_skills(args[0].as.string);
    auto* field = resolve_skill_field(skills, args[1].as.string);
    if (field) {
        int val = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 5;
        *field = std::max(CharacterStats::MIN_STAT, std::min(CharacterStats::MAX_STAT, val));
    }
    return val_nil();
}

// get_skill_bonus(character, bonus_type) -> number
static Value native_get_skill_bonus(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_number(0);
    auto* skills = resolve_skills(args[0].as.string);
    if (!skills) return val_number(0);
    const char* bonus = args[1].as.string;
    if (std::strcmp(bonus, "hp") == 0)         return val_number(skills->hp_bonus());
    if (std::strcmp(bonus, "crit") == 0)       return val_number(skills->crit_chance() * 100);
    if (std::strcmp(bonus, "defense") == 0)    return val_number(skills->defense_bonus());
    if (std::strcmp(bonus, "magic_mult") == 0)  return val_number(skills->magic_damage_mult() * 100);
    if (std::strcmp(bonus, "weapon_dmg") == 0)  return val_number(skills->weapon_damage_bonus());
    if (std::strcmp(bonus, "dodge") == 0)       return val_number(skills->dodge_chance() * 100);
    if (std::strcmp(bonus, "spell_mult") == 0)  return val_number(skills->spell_power_mult() * 100);
    return val_number(0);
}

// =============== Player API ===============

static Value native_get_player_x(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_pos.x);
}

static Value native_get_player_y(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_pos.y);
}

static Value native_set_player_pos(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (args[0].type == VAL_NUMBER) gs->player_pos.x = (float)args[0].as.number;
    if (args[1].type == VAL_NUMBER) gs->player_pos.y = (float)args[1].as.number;
    gs->camera.center_on(gs->player_pos);
    return val_nil();
}

static Value native_get_player_speed(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_speed);
}

static Value native_set_player_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_speed = (float)args[0].as.number;
    return val_nil();
}

static Value native_get_player_hp(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_hp);
}

static Value native_get_player_hp_max(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_hp_max);
}

static Value native_set_player_hp(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_hp = (int)args[0].as.number;
    return val_nil();
}

static Value native_get_player_atk(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_atk);
}

static Value native_set_player_atk(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_atk = (int)args[0].as.number;
    return val_nil();
}

static Value native_get_player_def(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_def);
}

static Value native_set_player_def(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_def = (int)args[0].as.number;
    return val_nil();
}

static Value native_get_player_level(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_level);
}

static Value native_get_player_xp(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_xp);
}

static Value native_add_player_xp(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_xp += (int)args[0].as.number;
    return val_nil();
}

static Value native_get_player_dir(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->player_dir);
}

static Value native_set_player_dir(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->player_dir = (int)args[0].as.number;
    return val_nil();
}

static Value native_get_ally_hp(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->sam_hp);
}

static Value native_set_ally_hp(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER) s_active_engine->game_state_->sam_hp = (int)args[0].as.number;
    return val_nil();
}

static Value native_get_ally_atk(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->sam_atk);
}

// set_player_scale(scale)
static Value native_set_player_scale(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->player_sprite_scale = std::max(0.1f, std::min(10.0f, (float)args[0].as.number));
    return val_nil();
}

// get_player_scale() -> number
static Value native_get_player_scale(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(1);
    return val_number(s_active_engine->game_state_->player_sprite_scale);
}

// set_ally_scale(scale)
static Value native_set_ally_scale(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->ally_sprite_scale = std::max(0.1f, std::min(10.0f, (float)args[0].as.number));
    return val_nil();
}

// =============== Input API ===============

static Value native_is_key_held(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(0);
    auto* input = s_active_engine->game_state_->current_input;
    if (!input || args[0].type != VAL_STRING) return val_bool(0);
    const char* a = args[0].as.string;
    if (std::strcmp(a, "up") == 0)       return val_bool(input->is_held(eb::InputAction::MoveUp));
    if (std::strcmp(a, "down") == 0)     return val_bool(input->is_held(eb::InputAction::MoveDown));
    if (std::strcmp(a, "left") == 0)     return val_bool(input->is_held(eb::InputAction::MoveLeft));
    if (std::strcmp(a, "right") == 0)    return val_bool(input->is_held(eb::InputAction::MoveRight));
    if (std::strcmp(a, "confirm") == 0)  return val_bool(input->is_held(eb::InputAction::Confirm));
    if (std::strcmp(a, "cancel") == 0)   return val_bool(input->is_held(eb::InputAction::Cancel));
    if (std::strcmp(a, "run") == 0)      return val_bool(input->is_held(eb::InputAction::Run));
    return val_bool(0);
}

static Value native_is_key_pressed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(0);
    auto* input = s_active_engine->game_state_->current_input;
    if (!input || args[0].type != VAL_STRING) return val_bool(0);
    const char* a = args[0].as.string;
    if (std::strcmp(a, "up") == 0)       return val_bool(input->is_pressed(eb::InputAction::MoveUp));
    if (std::strcmp(a, "down") == 0)     return val_bool(input->is_pressed(eb::InputAction::MoveDown));
    if (std::strcmp(a, "left") == 0)     return val_bool(input->is_pressed(eb::InputAction::MoveLeft));
    if (std::strcmp(a, "right") == 0)    return val_bool(input->is_pressed(eb::InputAction::MoveRight));
    if (std::strcmp(a, "confirm") == 0)  return val_bool(input->is_pressed(eb::InputAction::Confirm));
    if (std::strcmp(a, "cancel") == 0)   return val_bool(input->is_pressed(eb::InputAction::Cancel));
    if (std::strcmp(a, "run") == 0)      return val_bool(input->is_pressed(eb::InputAction::Run));
    return val_bool(0);
}

static Value native_get_mouse_x(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    auto* input = s_active_engine->game_state_->current_input;
    return val_number(input ? input->mouse.x : 0);
}

static Value native_get_mouse_y(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    auto* input = s_active_engine->game_state_->current_input;
    return val_number(input ? input->mouse.y : 0);
}

// =============== Platform API ===============

static Value native_get_screen_w(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->hud.native_w);
}

static Value native_get_screen_h(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->hud.native_h);
}

// =============== Register Methods ===============

void ScriptEngine::register_skills_api() {
    if (!env_) return;
    env_define(env_, "get_skill", 9, val_native(native_get_skill));
    env_define(env_, "set_skill", 9, val_native(native_set_skill));
    env_define(env_, "get_skill_bonus", 15, val_native(native_get_skill_bonus));
    std::printf("[ScriptEngine] Skills API registered\n");
}

void ScriptEngine::register_player_api() {
    if (!env_) return;
    env_define(env_, "get_player_x", 12, val_native(native_get_player_x));
    env_define(env_, "get_player_y", 12, val_native(native_get_player_y));
    env_define(env_, "set_player_pos", 14, val_native(native_set_player_pos));
    env_define(env_, "get_player_speed", 16, val_native(native_get_player_speed));
    env_define(env_, "set_player_speed", 16, val_native(native_set_player_speed));
    env_define(env_, "get_player_hp", 13, val_native(native_get_player_hp));
    env_define(env_, "get_player_hp_max", 17, val_native(native_get_player_hp_max));
    env_define(env_, "set_player_hp", 13, val_native(native_set_player_hp));
    env_define(env_, "get_player_atk", 14, val_native(native_get_player_atk));
    env_define(env_, "set_player_atk", 14, val_native(native_set_player_atk));
    env_define(env_, "get_player_def", 14, val_native(native_get_player_def));
    env_define(env_, "set_player_def", 14, val_native(native_set_player_def));
    env_define(env_, "get_player_level", 16, val_native(native_get_player_level));
    env_define(env_, "get_player_xp", 13, val_native(native_get_player_xp));
    env_define(env_, "add_player_xp", 13, val_native(native_add_player_xp));
    env_define(env_, "get_player_dir", 14, val_native(native_get_player_dir));
    env_define(env_, "set_player_dir", 14, val_native(native_set_player_dir));
    env_define(env_, "get_ally_hp", 11, val_native(native_get_ally_hp));
    env_define(env_, "set_ally_hp", 11, val_native(native_set_ally_hp));
    env_define(env_, "get_ally_atk", 12, val_native(native_get_ally_atk));
    // Sprite scale
    env_define(env_, "set_player_scale", 16, val_native(native_set_player_scale));
    env_define(env_, "get_player_scale", 16, val_native(native_get_player_scale));
    env_define(env_, "set_ally_scale", 14, val_native(native_set_ally_scale));
    std::printf("[ScriptEngine] Player API registered\n");
}

void ScriptEngine::register_input_api() {
    if (!env_) return;
    env_define(env_, "is_key_held", 11, val_native(native_is_key_held));
    env_define(env_, "is_key_pressed", 14, val_native(native_is_key_pressed));
    env_define(env_, "get_mouse_x", 11, val_native(native_get_mouse_x));
    env_define(env_, "get_mouse_y", 11, val_native(native_get_mouse_y));
    std::printf("[ScriptEngine] Input API registered\n");
}

void ScriptEngine::register_platform_api() {
    if (!env_) return;
#ifdef __ANDROID__
    set_string("PLATFORM", "android");
    set_bool("IS_ANDROID", true);
    set_bool("IS_DESKTOP", false);
    set_bool("IS_QUEST", false);  // Set to true at runtime if Quest detected
#elif _WIN32
    set_string("PLATFORM", "windows");
    set_bool("IS_ANDROID", false);
    set_bool("IS_DESKTOP", true);
    set_bool("IS_QUEST", false);
#else
    set_string("PLATFORM", "linux");
    set_bool("IS_ANDROID", false);
    set_bool("IS_DESKTOP", true);
    set_bool("IS_QUEST", false);
#endif
    env_define(env_, "get_screen_w", 12, val_native(native_get_screen_w));
    env_define(env_, "get_screen_h", 12, val_native(native_get_screen_h));
    std::printf("[ScriptEngine] Platform API registered\n");
}

} // namespace eb
