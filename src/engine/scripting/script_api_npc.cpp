#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "game/ai/pathfinding.h"
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

// =============== NPC Runtime API ===============

static Value native_npc_get_x(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(0);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_number(npc ? npc->position.x : 0);
}

static Value native_npc_get_y(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(0);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_number(npc ? npc->position.y : 0);
}

static Value native_npc_set_pos(int argc, Value* args) {
    if (argc < 3 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc) {
        if (args[1].type == VAL_NUMBER) npc->position.x = (float)args[1].as.number;
        if (args[2].type == VAL_NUMBER) npc->position.y = (float)args[2].as.number;
    }
    return val_nil();
}

static Value native_npc_get_speed(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(0);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_number(npc ? npc->move_speed : 0);
}

static Value native_npc_set_speed(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc && args[1].type == VAL_NUMBER) npc->move_speed = (float)args[1].as.number;
    return val_nil();
}

static Value native_npc_get_dir(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(0);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_number(npc ? npc->dir : 0);
}

static Value native_npc_set_dir(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc && args[1].type == VAL_NUMBER) npc->dir = (int)args[1].as.number;
    return val_nil();
}

static Value native_npc_set_hostile(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc) npc->hostile = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                            (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    return val_nil();
}

static Value native_npc_is_hostile(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_bool(0);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_bool(npc ? npc->hostile : false);
}

static Value native_npc_count(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number((int)s_active_engine->game_state_->npcs.size());
}

static Value native_npc_exists(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_bool(0);
    return val_bool(find_npc_by_name(args[0].as.string) != nullptr);
}

static Value native_npc_remove(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1 || args[0].type != VAL_STRING) return val_nil();
    std::string name = args[0].as.string;
    auto& npcs = s_active_engine->game_state_->npcs;
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
        [&](auto& n) { return n.name == name; }), npcs.end());
    return val_nil();
}

// =============== Sprite Manipulation API ===============

// npc_set_scale(name, scale)
static Value native_npc_set_scale(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc && args[1].type == VAL_NUMBER) npc->sprite_scale = std::max(0.1f, std::min(10.0f, (float)args[1].as.number));
    return val_nil();
}

// npc_get_scale(name) -> number
static Value native_npc_get_scale(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(1);
    NPC* npc = find_npc_by_name(args[0].as.string);
    return val_number(npc ? npc->sprite_scale : 1.0f);
}

// npc_set_tint(name, r, g, b, a)
static Value native_npc_set_tint(int argc, Value* args) {
    if (argc < 5 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc) {
        npc->sprite_tint = {
            (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1,
            (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1,
            (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1,
            (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 1
        };
    }
    return val_nil();
}

// npc_set_flip(name, flip_h)
static Value native_npc_set_flip(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (npc) npc->sprite_flip_h = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                                   (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    return val_nil();
}

// npc_set_grid(name, grid_w, grid_h) — change sprite grid size at runtime
static Value native_npc_set_grid(int argc, Value* args) {
    if (argc < 3 || args[0].type != VAL_STRING) return val_nil();
    NPC* npc = find_npc_by_name(args[0].as.string);
    if (!npc) return val_nil();
    npc->sprite_grid_w = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    npc->sprite_grid_h = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    // Re-cache with new grid size
    if (s_active_engine && s_active_engine->game_state_ && !npc->sprite_atlas_key.empty()) {
        auto* gs = s_active_engine->game_state_;
        // Build cache key
        std::string cache_key = npc->sprite_atlas_key;
        if (npc->sprite_grid_w > 0 && npc->sprite_grid_h > 0) {
            char buf[32]; std::snprintf(buf, sizeof(buf), "@%dx%d", npc->sprite_grid_w, npc->sprite_grid_h);
            cache_key += buf;
        }
        if (!gs->atlas_cache.count(cache_key) && gs->resource_manager && gs->renderer) {
            try {
                auto* tex = gs->resource_manager->load_texture(npc->sprite_atlas_key);
                if (tex) {
                    int cw = (npc->sprite_grid_w > 0) ? npc->sprite_grid_w : tex->width() / 3;
                    int ch = (npc->sprite_grid_h > 0) ? npc->sprite_grid_h : tex->height() / 3;
                    auto atlas = std::make_shared<eb::TextureAtlas>(tex);
                    define_npc_atlas_regions(*atlas, cw, ch);
                    gs->atlas_cache[cache_key] = atlas;
                    gs->atlas_descs[cache_key] = gs->renderer->get_texture_descriptor(*tex);
                }
            } catch (...) {
                std::fprintf(stderr, "[Script] Failed to load sprite: %s\n", npc->sprite_atlas_key.c_str());
            }
        }
    }
    return val_nil();
}

// =============== Pathfinding API ===============

// npc_move_to(name, tile_x, tile_y)
static Value native_npc_move_to(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    NPC* npc = find_npc_by_name(name);
    if (!npc) return val_nil();
    auto& map = s_active_engine->game_state_->tile_map;
    int ts = map.tile_size();
    int sx = (int)(npc->position.x / ts);
    int sy = (int)(npc->position.y / ts);
    npc->current_path = eb::find_path(map, sx, sy, tx, ty);
    npc->path_index = 0;
    npc->path_active = !npc->current_path.empty();
    return val_bool(npc->path_active);
}

// =============== Route API ===============

// npc_add_waypoint(name, x, y)
static Value native_npc_add_waypoint(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    npc->route.waypoints.push_back({x, y});
    return val_nil();
}

// npc_set_route(name, mode) — "patrol", "once", "pingpong"
static Value native_npc_set_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    const char* mode = (args[1].type == VAL_STRING) ? args[1].as.string : "patrol";
    if (std::strcmp(mode, "once") == 0) npc->route.mode = RouteMode::Once;
    else if (std::strcmp(mode, "pingpong") == 0) npc->route.mode = RouteMode::PingPong;
    else npc->route.mode = RouteMode::Patrol;
    return val_nil();
}

// npc_start_route(name)
static Value native_npc_start_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc && !npc->route.waypoints.empty()) { npc->route.active = true; npc->route.current_waypoint = 0; }
    return val_nil();
}

// npc_stop_route(name)
static Value native_npc_stop_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->route.active = false;
    return val_nil();
}

// npc_clear_route(name)
static Value native_npc_clear_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) { npc->route.waypoints.clear(); npc->route.active = false; npc->route.current_waypoint = 0; }
    return val_nil();
}

// =============== Schedule API ===============

// npc_set_schedule(name, start_hour, end_hour)
static Value native_npc_set_schedule(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    npc->schedule.start_hour = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc->schedule.end_hour = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 24;
    npc->schedule.has_schedule = true;
    return val_nil();
}

// npc_set_spawn_point(name, x, y)
static Value native_npc_set_spawn_point(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    npc->schedule.spawn_point.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc->schedule.spawn_point.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    return val_nil();
}

// npc_clear_schedule(name)
static Value native_npc_clear_schedule(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) { npc->schedule.has_schedule = false; npc->schedule.currently_visible = true; }
    return val_nil();
}

// =============== NPC Interact API ===============

// npc_on_meet(npc1, npc2, callback_func)
static Value native_npc_on_meet(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPCMeetTrigger t;
    t.npc1_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    t.npc2_name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    t.callback_func = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    s_active_engine->game_state_->npc_meet_triggers.push_back(t);
    return val_nil();
}

// npc_set_meet_radius(npc1, npc2, radius)
static Value native_npc_set_meet_radius(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    std::string n1 = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string n2 = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float r = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 40;
    for (auto& t : s_active_engine->game_state_->npc_meet_triggers)
        if (t.npc1_name == n1 && t.npc2_name == n2) { t.trigger_radius = r; break; }
    return val_nil();
}

// npc_face_each_other(npc1, npc2)
static Value native_npc_face_each_other(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    NPC* a = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    NPC* b = find_npc_by_name((args[1].type == VAL_STRING) ? args[1].as.string : "");
    if (!a || !b) return val_nil();
    float dx = b->position.x - a->position.x;
    float dy = b->position.y - a->position.y;
    if (std::abs(dx) > std::abs(dy)) { a->dir = (dx > 0) ? 3 : 2; b->dir = (dx > 0) ? 2 : 3; }
    else { a->dir = (dy > 0) ? 0 : 1; b->dir = (dy > 0) ? 1 : 0; }
    return val_nil();
}

// =============== Spawn API ===============

// spawn_loop(npc_name, interval, max_count)
static Value native_spawn_loop(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    SpawnLoop loop;
    loop.npc_template_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    loop.interval = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 60;
    loop.max_count = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 5;
    loop.active = true;
    s_active_engine->game_state_->spawn_loops.push_back(loop);
    return val_nil();
}

// stop_spawn_loop(npc_name)
static Value native_stop_spawn_loop(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops)
        if (l.npc_template_name == name) l.active = false;
    return val_nil();
}

// set_spawn_area(name, x1, y1, x2, y2)
static Value native_set_spawn_area(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<5) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops) {
        if (l.npc_template_name == name) {
            l.area_min.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
            l.area_min.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
            l.area_max.x = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
            l.area_max.y = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
            l.has_area = true;
            break;
        }
    }
    return val_nil();
}

// set_spawn_callback(npc_name, callback_func)
static Value native_set_spawn_callback(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string func = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops)
        if (l.npc_template_name == name) { l.on_spawn_func = func; break; }
    return val_nil();
}

// set_spawn_time(name, start_hour, end_hour) — only spawn during these hours
static Value native_set_spawn_time(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float start = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float end = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 24;
    for (auto& l : s_active_engine->game_state_->spawn_loops) {
        if (l.npc_template_name == name) {
            l.spawn_start_hour = start;
            l.spawn_end_hour = end;
            l.has_time_gate = true;
            break;
        }
    }
    return val_nil();
}

// =============== Register Methods ===============

void ScriptEngine::register_npc_runtime_api() {
    if (!env_) return;
    env_define(env_, "npc_get_x", 9, val_native(native_npc_get_x));
    env_define(env_, "npc_get_y", 9, val_native(native_npc_get_y));
    env_define(env_, "npc_set_pos", 11, val_native(native_npc_set_pos));
    env_define(env_, "npc_get_speed", 13, val_native(native_npc_get_speed));
    env_define(env_, "npc_set_speed", 13, val_native(native_npc_set_speed));
    env_define(env_, "npc_get_dir", 11, val_native(native_npc_get_dir));
    env_define(env_, "npc_set_dir", 11, val_native(native_npc_set_dir));
    env_define(env_, "npc_set_hostile", 15, val_native(native_npc_set_hostile));
    env_define(env_, "npc_is_hostile", 14, val_native(native_npc_is_hostile));
    env_define(env_, "npc_count", 9, val_native(native_npc_count));
    env_define(env_, "npc_exists", 10, val_native(native_npc_exists));
    env_define(env_, "npc_remove", 10, val_native(native_npc_remove));
    // Sprite manipulation
    env_define(env_, "npc_set_scale", 13, val_native(native_npc_set_scale));
    env_define(env_, "npc_get_scale", 13, val_native(native_npc_get_scale));
    env_define(env_, "npc_set_tint", 12, val_native(native_npc_set_tint));
    env_define(env_, "npc_set_flip", 12, val_native(native_npc_set_flip));
    env_define(env_, "npc_set_grid", 12, val_native(native_npc_set_grid));
    std::printf("[ScriptEngine] NPC Runtime API registered\n");
}

void ScriptEngine::register_pathfinding_api() {
    if (!env_) return;
    env_define(env_, "npc_move_to", 11, val_native(native_npc_move_to));
    std::printf("[ScriptEngine] Pathfinding API registered\n");
}

void ScriptEngine::register_route_api() {
    if (!env_) return;
    env_define(env_, "npc_add_waypoint", 16, val_native(native_npc_add_waypoint));
    env_define(env_, "npc_set_route", 13, val_native(native_npc_set_route));
    env_define(env_, "npc_start_route", 15, val_native(native_npc_start_route));
    env_define(env_, "npc_stop_route", 14, val_native(native_npc_stop_route));
    env_define(env_, "npc_clear_route", 15, val_native(native_npc_clear_route));
    std::printf("[ScriptEngine] Route API registered\n");
}

void ScriptEngine::register_schedule_api() {
    if (!env_) return;
    env_define(env_, "npc_set_schedule", 16, val_native(native_npc_set_schedule));
    env_define(env_, "npc_set_spawn_point", 19, val_native(native_npc_set_spawn_point));
    env_define(env_, "npc_clear_schedule", 18, val_native(native_npc_clear_schedule));
    std::printf("[ScriptEngine] Schedule API registered\n");
}

void ScriptEngine::register_npc_interact_api() {
    if (!env_) return;
    env_define(env_, "npc_on_meet", 11, val_native(native_npc_on_meet));
    env_define(env_, "npc_set_meet_radius", 19, val_native(native_npc_set_meet_radius));
    env_define(env_, "npc_face_each_other", 19, val_native(native_npc_face_each_other));
    std::printf("[ScriptEngine] NPC Interact API registered\n");
}

void ScriptEngine::register_spawn_api() {
    if (!env_) return;
    env_define(env_, "spawn_loop", 10, val_native(native_spawn_loop));
    env_define(env_, "stop_spawn_loop", 15, val_native(native_stop_spawn_loop));
    env_define(env_, "set_spawn_area", 14, val_native(native_set_spawn_area));
    env_define(env_, "set_spawn_callback", 18, val_native(native_set_spawn_callback));
    env_define(env_, "set_spawn_time", 14, val_native(native_set_spawn_time));
    std::printf("[ScriptEngine] Spawn API registered\n");
}

} // namespace eb
