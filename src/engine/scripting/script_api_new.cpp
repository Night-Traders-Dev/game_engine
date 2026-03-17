// New Phase 1-4 API registrations
// Tween, Particle, Save, Transition, Quest, Equipment, Dialogue History,
// Event, Locale, Achievement, Lighting, Animation, Visual FX

#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "engine/core/debug_log.h"
#include "game/systems/tween.h"
#include "game/systems/particles.h"
#include "game/systems/save_system.h"
#include "game/systems/sprite_anim.h"

extern "C" {
#include "interpreter.h"
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include <unordered_map>

namespace eb {
extern ScriptEngine* s_active_engine;
extern std::unordered_map<std::string, Value> s_flags;

// ═══════════════════════════════════════════════════════════════

static Value native_tween(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_number(-1);
    auto* gs = s_active_engine->game_state_;
    const char* target = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* prop = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float end_val = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float dur = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1;
    const char* ease_name = (argc > 4 && args[4].type == VAL_STRING) ? args[4].as.string : "linear";
    const char* on_complete = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "";
    auto ease_type = eb::ease_from_string(ease_name);
    int id = gs->tween_system.add(target, prop, end_val, dur, ease_type, on_complete);
    return val_number(id);
}

static Value native_tween_stop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    int id = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : -1;
    s_active_engine->game_state_->tween_system.stop(id);
    return val_nil();
}

static Value native_tween_stop_all(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    const char* target = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->tween_system.stop_all(target);
    return val_nil();
}

static Value native_tween_delay(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    float dur = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 1;
    const char* cb = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    s_active_engine->game_state_->tween_system.add_delay(dur, cb);
    return val_nil();
}

static Value native_set_input_locked(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    s_active_engine->game_state_->input_locked = (args[0].type == VAL_BOOL) ? args[0].as.boolean : false;
    return val_nil();
}

void ScriptEngine::register_tween_api() {
    if (!env_) return;
    env_define(env_, "tween", 5, val_native(native_tween));
    env_define(env_, "tween_stop", 10, val_native(native_tween_stop));
    env_define(env_, "tween_stop_all", 14, val_native(native_tween_stop_all));
    env_define(env_, "tween_delay", 11, val_native(native_tween_delay));
    env_define(env_, "wait", 4, val_native(native_tween_delay)); // alias
    env_define(env_, "set_input_locked", 16, val_native(native_set_input_locked));
    std::printf("[ScriptEngine] Tween API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Particle API
// ═══════════════════════════════════════════════════════════════

static Value native_emit_preset(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "dust";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    auto emitter = eb::make_preset(name, x, y);
    s_active_engine->game_state_->emitters.push_back(std::move(emitter));
    return val_nil();
}

static Value native_emit_burst(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    int count = (argc > 2 && args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 10;
    eb::ParticleEmitter e;
    e.pos = {x, y};
    e.burst_count = count;
    e.max_particles = count;
    e.vel_min = {-40, -40}; e.vel_max = {40, 40};
    e.color_start = {1, 1, 1, 1}; e.color_end = {1, 1, 1, 0};
    e.size_min = 2; e.size_max = 5;
    e.life_min = 0.3f; e.life_max = 0.6f;
    e.gravity = 50;
    s_active_engine->game_state_->emitters.push_back(std::move(e));
    return val_nil();
}

static Value native_emit_clear(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    s_active_engine->game_state_->emitters.clear();
    return val_nil();
}

void ScriptEngine::register_particle_api() {
    if (!env_) return;
    env_define(env_, "emit_preset", 11, val_native(native_emit_preset));
    env_define(env_, "emit_burst", 10, val_native(native_emit_burst));
    env_define(env_, "emit_clear", 10, val_native(native_emit_clear));
    std::printf("[ScriptEngine] Particle API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Save/Load API
// ═══════════════════════════════════════════════════════════════

static Value native_save_game(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(false);
    auto* gs = s_active_engine->game_state_;
    int slot = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 1;

    eb::SaveData data;
    data.player_x = gs->player_pos.x;
    data.player_y = gs->player_pos.y;
    data.player_dir = gs->player_dir;
    data.player_hp = gs->player_hp;
    data.player_hp_max = gs->player_hp_max;
    data.player_atk = gs->player_atk;
    data.player_def = gs->player_def;
    data.player_level = gs->player_level;
    data.player_xp = gs->player_xp;
    data.gold = gs->gold;
    data.game_hour = gs->day_night.game_hours;
    data.playtime_seconds = gs->playtime_seconds;
    data.flags = gs->flags;
    // TODO: serialize inventory, party, equipment, current_map

    return val_bool(eb::SaveSystem::save(slot, data));
}

static Value native_load_game(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(false);
    auto* gs = s_active_engine->game_state_;
    int slot = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 1;

    eb::SaveData data;
    if (!eb::SaveSystem::load(slot, data)) return val_bool(false);

    gs->player_pos = {data.player_x, data.player_y};
    gs->player_dir = data.player_dir;
    gs->player_hp = data.player_hp;
    gs->player_hp_max = data.player_hp_max;
    gs->player_atk = data.player_atk;
    gs->player_def = data.player_def;
    gs->player_level = data.player_level;
    gs->player_xp = data.player_xp;
    gs->gold = data.gold;
    gs->day_night.game_hours = data.game_hour;
    gs->playtime_seconds = data.playtime_seconds;
    gs->flags = data.flags;

    return val_bool(true);
}

static Value native_has_save(int argc, Value* args) {
    int slot = (argc > 0 && args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 1;
    return val_bool(eb::SaveSystem::has_save(slot));
}

static Value native_delete_save(int argc, Value* args) {
    int slot = (argc > 0 && args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 1;
    return val_bool(eb::SaveSystem::delete_save(slot));
}

static Value native_has_flag(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_bool(false);
    const char* key = args[0].as.string;
    // Check both the script flags store and the GameState flags
    if (s_flags.find(key) != s_flags.end()) return val_bool(true);
    if (s_active_engine && s_active_engine->game_state_)
        return val_bool(s_active_engine->game_state_->flags.has(key));
    return val_bool(false);
}

static Value native_get_playtime(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->playtime_seconds);
}

void ScriptEngine::register_save_api() {
    if (!env_) return;
    env_define(env_, "save_game", 9, val_native(native_save_game));
    env_define(env_, "load_game", 9, val_native(native_load_game));
    env_define(env_, "has_save", 8, val_native(native_has_save));
    env_define(env_, "delete_save", 11, val_native(native_delete_save));
    env_define(env_, "has_flag", 8, val_native(native_has_flag));
    env_define(env_, "get_playtime", 12, val_native(native_get_playtime));
    std::printf("[ScriptEngine] Save API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Screen Transition API
// ═══════════════════════════════════════════════════════════════

static Value native_transition(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    const char* type_str = (args[0].type == VAL_STRING) ? args[0].as.string : "fade";
    float dur = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0.5;
    const char* cb = (argc > 2 && args[2].type == VAL_STRING) ? args[2].as.string : "";
    int dir = (argc > 3 && args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;

    eb::TransitionType tt = eb::TransitionType::Fade;
    std::string ts(type_str);
    if (ts == "iris") tt = eb::TransitionType::Iris;
    else if (ts == "wipe") tt = eb::TransitionType::Wipe;
    else if (ts == "pixelate") tt = eb::TransitionType::Pixelate;
    else if (ts == "slide") tt = eb::TransitionType::Slide;

    s_active_engine->game_state_->transition.start(tt, dur, false, dir, cb);
    return val_nil();
}

static Value native_transition_out(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    const char* type_str = (args[0].type == VAL_STRING) ? args[0].as.string : "fade";
    float dur = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0.5;
    const char* cb = (argc > 2 && args[2].type == VAL_STRING) ? args[2].as.string : "";

    eb::TransitionType tt = eb::TransitionType::Fade;
    std::string ts(type_str);
    if (ts == "iris") tt = eb::TransitionType::Iris;
    else if (ts == "wipe") tt = eb::TransitionType::Wipe;
    else if (ts == "pixelate") tt = eb::TransitionType::Pixelate;

    s_active_engine->game_state_->transition.start(tt, dur, true, 0, cb);
    return val_nil();
}

void ScriptEngine::register_transition_api() {
    if (!env_) return;
    env_define(env_, "transition", 10, val_native(native_transition));
    env_define(env_, "transition_out", 14, val_native(native_transition_out));
    std::printf("[ScriptEngine] Transition API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Quest API
// ═══════════════════════════════════════════════════════════════

static GameState::Quest* find_quest(const std::string& id) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    for (auto& q : s_active_engine->game_state_->quests)
        if (q.id == id) return &q;
    return nullptr;
}

static Value native_quest_start(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string title = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string desc = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    const char* on_complete = (argc > 3 && args[3].type == VAL_STRING) ? args[3].as.string : "";
    auto* existing = find_quest(id);
    if (existing) { existing->state = GameState::Quest::State::Active; return val_nil(); }
    GameState::Quest q; q.id = id; q.title = title; q.description = desc;
    q.state = GameState::Quest::State::Active; q.on_complete = on_complete;
    gs->quests.push_back(q);
    std::printf("[Quest] Started: %s\n", title.c_str());
    return val_nil();
}

static Value native_quest_complete(int argc, Value* args) {
    if (argc < 1) return val_nil();
    auto* q = find_quest((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (q) { q->state = GameState::Quest::State::Complete; std::printf("[Quest] Complete: %s\n", q->title.c_str()); }
    return val_nil();
}

static Value native_quest_add_objective(int argc, Value* args) {
    if (argc < 2) return val_nil();
    auto* q = find_quest((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (q) q->objectives.push_back({(args[1].type == VAL_STRING) ? args[1].as.string : "", false});
    return val_nil();
}

static Value native_quest_complete_objective(int argc, Value* args) {
    if (argc < 2) return val_nil();
    auto* q = find_quest((args[0].type == VAL_STRING) ? args[0].as.string : "");
    int idx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    if (q && idx >= 0 && idx < (int)q->objectives.size()) q->objectives[idx].complete = true;
    return val_nil();
}

static Value native_quest_is_active(int argc, Value* args) {
    if (argc < 1) return val_bool(false);
    auto* q = find_quest((args[0].type == VAL_STRING) ? args[0].as.string : "");
    return val_bool(q && q->state == GameState::Quest::State::Active);
}

static Value native_quest_is_complete(int argc, Value* args) {
    if (argc < 1) return val_bool(false);
    auto* q = find_quest((args[0].type == VAL_STRING) ? args[0].as.string : "");
    return val_bool(q && q->state == GameState::Quest::State::Complete);
}

static Value native_quest_set_tracker(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    s_active_engine->game_state_->active_quest_tracker = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_nil();
}

void ScriptEngine::register_quest_api() {
    if (!env_) return;
    env_define(env_, "quest_start", 11, val_native(native_quest_start));
    env_define(env_, "quest_complete", 14, val_native(native_quest_complete));
    env_define(env_, "quest_add_objective", 19, val_native(native_quest_add_objective));
    env_define(env_, "quest_complete_objective", 24, val_native(native_quest_complete_objective));
    env_define(env_, "quest_is_active", 15, val_native(native_quest_is_active));
    env_define(env_, "quest_is_complete", 17, val_native(native_quest_is_complete));
    env_define(env_, "quest_set_tracker", 17, val_native(native_quest_set_tracker));
    std::printf("[ScriptEngine] Quest API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Equipment API
// ═══════════════════════════════════════════════════════════════

static Value native_equip(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* slot = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* item = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string s(slot);
    if (s == "weapon") gs->equipment.weapon = item;
    else if (s == "armor") gs->equipment.armor = item;
    else if (s == "accessory") gs->equipment.accessory = item;
    else if (s == "shield") gs->equipment.shield = item;
    gs->equipment.recalc(gs->inventory);
    return val_nil();
}

static Value native_unequip(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string s = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (s == "weapon") gs->equipment.weapon.clear();
    else if (s == "armor") gs->equipment.armor.clear();
    else if (s == "accessory") gs->equipment.accessory.clear();
    else if (s == "shield") gs->equipment.shield.clear();
    gs->equipment.recalc(gs->inventory);
    return val_nil();
}

static Value native_get_equipped(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_string("");
    auto* gs = s_active_engine->game_state_;
    std::string s = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (s == "weapon") return val_string(gs->equipment.weapon.c_str());
    if (s == "armor") return val_string(gs->equipment.armor.c_str());
    if (s == "accessory") return val_string(gs->equipment.accessory.c_str());
    if (s == "shield") return val_string(gs->equipment.shield.c_str());
    return val_string("");
}

void ScriptEngine::register_equipment_api() {
    if (!env_) return;
    env_define(env_, "equip", 5, val_native(native_equip));
    env_define(env_, "unequip", 7, val_native(native_unequip));
    env_define(env_, "get_equipped", 12, val_native(native_get_equipped));
    std::printf("[ScriptEngine] Equipment API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Dialogue History API
// ═══════════════════════════════════════════════════════════════

static Value native_has_talked_to(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(false);
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_bool(s_active_engine->game_state_->talked_to.count(name) > 0);
}

void ScriptEngine::register_dialogue_history_api() {
    if (!env_) return;
    env_define(env_, "has_talked_to", 13, val_native(native_has_talked_to));
    std::printf("[ScriptEngine] Dialogue History API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Event System API
// ═══════════════════════════════════════════════════════════════

static Value native_on_event(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* event_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* callback = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    gs->event_listeners.push_back({event_name, callback});
    return val_nil();
}

static Value native_emit_event(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* event_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    // Find and queue callbacks for this event
    for (auto& listener : gs->event_listeners) {
        if (listener.event_name == event_name && !listener.callback.empty()) {
            if (s_active_engine->has_function(listener.callback)) {
                s_active_engine->call_function(listener.callback);
            }
        }
    }
    return val_nil();
}

void ScriptEngine::register_event_api() {
    if (!env_) return;
    env_define(env_, "on_event", 8, val_native(native_on_event));
    env_define(env_, "emit_event", 10, val_native(native_emit_event));
    std::printf("[ScriptEngine] Event API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Localization API
// ═══════════════════════════════════════════════════════════════

static Value native_loc(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_string("");
    auto* gs = s_active_engine->game_state_;
    const char* key = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto locale_it = gs->locale_strings.find(gs->locale);
    if (locale_it != gs->locale_strings.end()) {
        auto str_it = locale_it->second.find(key);
        if (str_it != locale_it->second.end()) return val_string(str_it->second.c_str());
    }
    // Fallback to English
    auto en_it = gs->locale_strings.find("en");
    if (en_it != gs->locale_strings.end()) {
        auto str_it = en_it->second.find(key);
        if (str_it != en_it->second.end()) return val_string(str_it->second.c_str());
    }
    return val_string(key); // Return key itself as fallback
}

static Value native_set_locale(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    s_active_engine->game_state_->locale = (args[0].type == VAL_STRING) ? args[0].as.string : "en";
    return val_nil();
}

void ScriptEngine::register_locale_api() {
    if (!env_) return;
    env_define(env_, "loc", 3, val_native(native_loc));
    env_define(env_, "set_locale", 10, val_native(native_set_locale));
    std::printf("[ScriptEngine] Locale API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Achievement API
// ═══════════════════════════════════════════════════════════════

static Value native_unlock_achievement(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string title = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string desc = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    // Check if already unlocked
    for (auto& a : gs->achievements) {
        if (a.id == id) { if (!a.unlocked) { a.unlocked = true; a.unlock_time = gs->playtime_seconds; } return val_nil(); }
    }
    gs->achievements.push_back({id, title, desc, true, gs->playtime_seconds});
    std::printf("[Achievement] Unlocked: %s\n", title.c_str());
    // Use flag system for persistence
    gs->flags.set("achievement_" + id, "1");
    return val_nil();
}

static Value native_has_achievement(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(false);
    auto* gs = s_active_engine->game_state_;
    const char* id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& a : gs->achievements) if (a.id == id && a.unlocked) return val_bool(true);
    return val_bool(gs->flags.has("achievement_" + std::string(id)));
}

void ScriptEngine::register_achievement_api() {
    if (!env_) return;
    env_define(env_, "unlock_achievement", 18, val_native(native_unlock_achievement));
    env_define(env_, "has_achievement", 15, val_native(native_has_achievement));
    std::printf("[ScriptEngine] Achievement API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// 2D Lighting API
// ═══════════════════════════════════════════════════════════════

static Value native_add_light(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_number(-1);
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float radius = (argc > 2 && args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 128;
    float intensity = (argc > 3 && args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1.0;
    gs->lights.push_back({x, y, radius, intensity, {1, 0.9f, 0.7f, 1}, true});
    return val_number((double)(gs->lights.size() - 1));
}

static Value native_remove_light(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int idx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : -1;
    if (idx >= 0 && idx < (int)gs->lights.size()) gs->lights[idx].active = false;
    return val_nil();
}

static Value native_set_ambient_light(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    float v = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 1.0;
    s_active_engine->game_state_->ambient_light = std::max(0.0f, std::min(1.0f, v));
    return val_nil();
}

static Value native_enable_lighting(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    s_active_engine->game_state_->lighting_enabled = (args[0].type == VAL_BOOL) ? args[0].as.boolean : true;
    return val_nil();
}

void ScriptEngine::register_lighting_api() {
    if (!env_) return;
    env_define(env_, "add_light", 9, val_native(native_add_light));
    env_define(env_, "remove_light", 12, val_native(native_remove_light));
    env_define(env_, "set_ambient_light", 17, val_native(native_set_ambient_light));
    env_define(env_, "enable_lighting", 15, val_native(native_enable_lighting));
    std::printf("[ScriptEngine] Lighting API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Sprite Animation API
// ═══════════════════════════════════════════════════════════════

static Value native_anim_play(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* npc_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* anim_name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    for (auto& npc : gs->npcs) {
        if (npc.name == npc_name) {
            npc.anim_player.play(anim_name);
            break;
        }
    }
    return val_nil();
}

static Value native_anim_stop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* npc_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& npc : gs->npcs) {
        if (npc.name == npc_name) { npc.anim_player.stop(); break; }
    }
    return val_nil();
}

static Value native_anim_define(int argc, Value* args) {
    // anim_define(npc_name, anim_name, frame_count, duration, loop)
    // Simplified: defines sequential frames starting from current sprite index
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* npc_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* anim_name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    int frame_count = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 2;
    float frame_dur = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0.15;
    bool loop = (argc > 4 && args[4].type == VAL_BOOL) ? args[4].as.boolean : true;

    for (auto& npc : gs->npcs) {
        if (npc.name == npc_name) {
            std::vector<eb::AnimFrame> frames;
            for (int i = 0; i < frame_count; i++) {
                frames.push_back({i, frame_dur});
            }
            npc.anim_player.define(anim_name, frames, loop);
            break;
        }
    }
    return val_nil();
}

void ScriptEngine::register_anim_api() {
    if (!env_) return;
    env_define(env_, "anim_play", 9, val_native(native_anim_play));
    env_define(env_, "anim_stop", 9, val_native(native_anim_stop));
    env_define(env_, "anim_define", 11, val_native(native_anim_define));
    std::printf("[ScriptEngine] Animation API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Visual Effects API (water reflections, bloom)
// ═══════════════════════════════════════════════════════════════

static Value native_set_water_reflections(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    s_active_engine->game_state_->water_reflections = (args[0].type == VAL_BOOL) ? args[0].as.boolean : true;
    return val_nil();
}

static Value native_set_bloom(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->bloom_enabled = (args[0].type == VAL_BOOL) ? args[0].as.boolean : true;
    if (argc > 1 && args[1].type == VAL_NUMBER) gs->bloom_intensity = (float)args[1].as.number;
    if (argc > 2 && args[2].type == VAL_NUMBER) gs->bloom_threshold = (float)args[2].as.number;
    return val_nil();
}

void ScriptEngine::register_visual_fx_api() {
    if (!env_) return;
    env_define(env_, "set_water_reflections", 21, val_native(native_set_water_reflections));
    env_define(env_, "set_bloom", 9, val_native(native_set_bloom));
    std::printf("[ScriptEngine] Visual FX API registered\n");
}

// ═══════════════════════════════════════════════════════════════
// Parallax Background API
// ═══════════════════════════════════════════════════════════════

// add_parallax(texture_path, scroll_x, scroll_y) -> index
static Value native_add_parallax(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_number(-1);
    auto* gs = s_active_engine->game_state_;
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float sx = (argc > 1 && args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0.5f;
    float sy = (argc > 2 && args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0.5f;

    // Load texture at runtime
    if (gs->resource_manager && gs->renderer) {
        try {
            auto* tex = gs->resource_manager->load_texture(path);
            if (tex) {
                eb::ParallaxLayer layer;
                layer.texture_path = path;
                layer.scroll_x = sx;
                layer.scroll_y = sy;
                layer.texture_desc = (void*)gs->renderer->get_texture_descriptor(*tex);
                layer.tex_width = tex->width();
                layer.tex_height = tex->height();
                gs->parallax_layers.push_back(layer);
                return val_number((double)(gs->parallax_layers.size() - 1));
            }
        } catch (...) {
            std::fprintf(stderr, "[Parallax] Failed to load texture: %s\n", path);
        }
    }
    return val_number(-1);
}

// remove_parallax(index)
static Value native_remove_parallax(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int idx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : -1;
    if (idx >= 0 && idx < (int)gs->parallax_layers.size()) {
        gs->parallax_layers.erase(gs->parallax_layers.begin() + idx);
    }
    return val_nil();
}

// set_parallax(index, property, value)
static Value native_set_parallax(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int idx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : -1;
    const char* prop = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float nv = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    bool bv = (args[2].type == VAL_BOOL) ? args[2].as.boolean : (nv != 0);
    if (idx < 0 || idx >= (int)gs->parallax_layers.size()) return val_nil();
    auto& l = gs->parallax_layers[idx];
    if (std::strcmp(prop, "scroll_x") == 0) l.scroll_x = nv;
    else if (std::strcmp(prop, "scroll_y") == 0) l.scroll_y = nv;
    else if (std::strcmp(prop, "offset_x") == 0) l.offset_x = nv;
    else if (std::strcmp(prop, "offset_y") == 0) l.offset_y = nv;
    else if (std::strcmp(prop, "repeat_x") == 0) l.repeat_x = bv;
    else if (std::strcmp(prop, "repeat_y") == 0) l.repeat_y = bv;
    else if (std::strcmp(prop, "active") == 0) l.active = bv;
    else if (std::strcmp(prop, "tint_r") == 0) l.tint.x = nv;
    else if (std::strcmp(prop, "tint_g") == 0) l.tint.y = nv;
    else if (std::strcmp(prop, "tint_b") == 0) l.tint.z = nv;
    else if (std::strcmp(prop, "tint_a") == 0) l.tint.w = nv;
    else if (std::strcmp(prop, "auto_scroll_x") == 0) l.auto_scroll_x = nv;
    else if (std::strcmp(prop, "auto_scroll_y") == 0) l.auto_scroll_y = nv;
    else if (std::strcmp(prop, "scale") == 0) l.scale = nv;
    else if (std::strcmp(prop, "pin_bottom") == 0) l.pin_bottom = bv;
    else if (std::strcmp(prop, "fill_viewport") == 0) l.fill_viewport = bv;
    else if (std::strcmp(prop, "z_order") == 0) l.z_order = (int)nv;
    return val_nil();
}

// clear_parallax() — remove all parallax layers
static Value native_clear_parallax(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    s_active_engine->game_state_->parallax_layers.clear();
    return val_nil();
}

// parallax_count() → number of layers
static Value native_parallax_count(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number((double)s_active_engine->game_state_->parallax_layers.size());
}

// load_parallax_preset(biome) — loads all layers for a biome from assets/textures/parallax/<biome>/
// Biome presets: "forest", "cave", "night", "sunset", "snow", "desert", "forest_sunset", "forest_trees"
static Value native_load_parallax_preset(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* biome = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!biome[0]) return val_nil();

    // Clear existing layers
    gs->parallax_layers.clear();

    // Scroll speeds: layer 0 = slowest (sky), increasing toward foreground
    // For a typical 5-layer setup: sky(0.0), far_mtns(0.1), near_mtns(0.2), trees(0.35), fg(0.5)
    struct LayerPreset { const char* suffix; float scroll_x; float scroll_y; bool pin_bottom; bool fill_vp; };
    // Try to load layers 0-9 sequentially (different biomes have different counts)
    const char* layer_names[] = {
        "sky", "stalactites", "mountains", "dunes", "rocks", "trees",
        "snow_trees", "foreground", "gradient", "sky2", "bg2", "bg1",
        "bg0", "middleground", "middleplus"
    };

    std::string base = "assets/textures/parallax/" + std::string(biome) + "/";
    int loaded = 0;

    // Scan layer_0 through layer_9
    for (int i = 0; i < 10; i++) {
        // Try each possible suffix for this layer index
        bool found = false;
        for (auto& name : layer_names) {
            std::string path = base + "layer_" + std::to_string(i) + "_" + name + ".png";
            if (gs->resource_manager && gs->renderer) {
                try {
                    auto* tex = gs->resource_manager->load_texture(path);
                    if (tex) {
                        eb::ParallaxLayer layer;
                        layer.texture_path = path;
                        layer.texture_desc = (void*)gs->renderer->get_texture_descriptor(*tex);
                        layer.tex_width = tex->width();
                        layer.tex_height = tex->height();
                        layer.z_order = i;
                        layer.repeat_x = true;

                        // Configure scroll speed based on layer depth
                        float depth_frac = (float)i / std::max(1.0f, 4.0f); // 0..1 from back to front
                        layer.scroll_x = depth_frac * 0.5f;  // 0.0 for sky → 0.5 for foreground
                        layer.scroll_y = depth_frac * 0.15f;  // Minimal vertical parallax

                        // Sky layers (index 0): fill viewport, no scroll
                        if (i == 0) {
                            layer.fill_viewport = true;
                            layer.scroll_x = 0;
                            layer.scroll_y = 0;
                        } else {
                            // Content layers: pin to bottom
                            layer.pin_bottom = true;
                        }

                        gs->parallax_layers.push_back(layer);
                        loaded++;
                        found = true;
                        break;
                    }
                } catch (...) {}
            }
        }
    }

    std::printf("[Parallax] Loaded preset '%s': %d layers\n", biome, loaded);
    return val_number(loaded);
}

void ScriptEngine::register_parallax_api() {
    if (!env_) return;
    env_define(env_, "add_parallax", 12, val_native(native_add_parallax));
    env_define(env_, "remove_parallax", 15, val_native(native_remove_parallax));
    env_define(env_, "set_parallax", 12, val_native(native_set_parallax));
    env_define(env_, "clear_parallax", 15, val_native(native_clear_parallax));
    env_define(env_, "parallax_count", 14, val_native(native_parallax_count));
    env_define(env_, "load_parallax_preset", 20, val_native(native_load_parallax_preset));
    std::printf("[ScriptEngine] Parallax API registered\n");
}

} // namespace eb
