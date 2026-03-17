#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "engine/physics/collision.h"
#include "engine/physics/raycast.h"

extern "C" {
#include "interpreter.h"
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>
#include <memory>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== Collision API ===============

// collide_rects(x1,y1,w1,h1, x2,y2,w2,h2) -> bool
static Value native_collide_rects(int argc, Value* args) {
    if (argc < 8) return val_bool(0);
    float x1 = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y1 = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float w1 = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float h1 = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float x2 = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float y2 = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 0;
    float w2 = (args[6].type == VAL_NUMBER) ? (float)args[6].as.number : 0;
    float h2 = (args[7].type == VAL_NUMBER) ? (float)args[7].as.number : 0;
    bool hit = (x1 < x2 + w2) && (x1 + w1 > x2) && (y1 < y2 + h2) && (y1 + h1 > y2);
    return val_bool(hit ? 1 : 0);
}

// collide_circles(cx1,cy1,r1, cx2,cy2,r2) -> bool
static Value native_collide_circles(int argc, Value* args) {
    if (argc < 6) return val_bool(0);
    float cx1 = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float cy1 = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float r1  = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float cx2 = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float cy2 = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float r2  = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 0;
    float dx = cx2 - cx1;
    float dy = cy2 - cy1;
    float dist_sq = dx * dx + dy * dy;
    float rad_sum = r1 + r2;
    return val_bool(dist_sq <= rad_sum * rad_sum ? 1 : 0);
}

// collide_rect_circle(rx,ry,rw,rh, cx,cy,cr) -> bool
static Value native_collide_rect_circle(int argc, Value* args) {
    if (argc < 7) return val_bool(0);
    float rx = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float ry = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float rw = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float rh = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float cx = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float cy = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 0;
    float cr = (args[6].type == VAL_NUMBER) ? (float)args[6].as.number : 0;
    // Find closest point on rect to circle center
    float closest_x = cx < rx ? rx : (cx > rx + rw ? rx + rw : cx);
    float closest_y = cy < ry ? ry : (cy > ry + rh ? ry + rh : cy);
    float dx = cx - closest_x;
    float dy = cy - closest_y;
    return val_bool(dx * dx + dy * dy <= cr * cr ? 1 : 0);
}

// point_in_rect(px,py, rx,ry,rw,rh) -> bool
static Value native_point_in_rect(int argc, Value* args) {
    if (argc < 6) return val_bool(0);
    float px = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float py = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float rx = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float ry = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float rw = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float rh = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 0;
    bool inside = (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
    return val_bool(inside ? 1 : 0);
}

// point_in_circle(px,py, cx,cy,cr) -> bool
static Value native_point_in_circle(int argc, Value* args) {
    if (argc < 5) return val_bool(0);
    float px = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float py = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float cx = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float cy = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float cr = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float dx = px - cx;
    float dy = py - cy;
    return val_bool(dx * dx + dy * dy <= cr * cr ? 1 : 0);
}

// Helper: build an AABB collider offset from a world position
static Collider make_aabb_collider(float world_x, float world_y, float xoff, float yoff, float w, float h) {
    Collider c;
    c.type = ColliderType::AABB;
    c.aabb = {world_x + xoff, world_y + yoff, w, h};
    return c;
}

// Helper: build a circle collider offset from a world position
static Collider make_circle_collider(float world_x, float world_y, float xoff, float yoff, float radius) {
    Collider c;
    c.type = ColliderType::CircleShape;
    c.circle = {{world_x + xoff, world_y + yoff}, radius};
    return c;
}

// set_collider_rect(entity_name, x_off, y_off, w, h)
static Value native_set_collider_rect(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float xoff = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float yoff = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w    = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 16;
    float h    = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 16;
    if (std::strcmp(name, "player") == 0) {
        gs->player_collider = make_aabb_collider(gs->player_pos.x, gs->player_pos.y, xoff, yoff, w, h);
    } else {
        NPC* npc = find_npc_by_name(name);
        if (npc) npc->collider = make_aabb_collider(npc->position.x, npc->position.y, xoff, yoff, w, h);
    }
    return val_nil();
}

// set_collider_circle(entity_name, x_off, y_off, radius)
static Value native_set_collider_circle(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float xoff   = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float yoff   = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float radius = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 8;
    if (std::strcmp(name, "player") == 0) {
        gs->player_collider = make_circle_collider(gs->player_pos.x, gs->player_pos.y, xoff, yoff, radius);
    } else {
        NPC* npc = find_npc_by_name(name);
        if (npc) npc->collider = make_circle_collider(npc->position.x, npc->position.y, xoff, yoff, radius);
    }
    return val_nil();
}

// check_collision(entity1, entity2) -> bool
static Value native_check_collision(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    const char* name1 = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* name2 = (args[1].type == VAL_STRING) ? args[1].as.string : "";

    // Resolve entity 1 collider
    Collider c1 = {};
    if (std::strcmp(name1, "player") == 0) {
        c1 = gs->player_collider;
    } else {
        NPC* npc = find_npc_by_name(name1);
        if (!npc) return val_bool(0);
        c1 = npc->collider;
    }

    // Resolve entity 2 collider
    Collider c2 = {};
    if (std::strcmp(name2, "player") == 0) {
        c2 = gs->player_collider;
    } else {
        NPC* npc = find_npc_by_name(name2);
        if (!npc) return val_bool(0);
        c2 = npc->collider;
    }

    CollisionResult result = collider_vs_collider(c1, c2);
    return val_bool(result.hit ? 1 : 0);
}

// =============== Raycast API ===============

static float s_raycast_hit_x = 0;
static float s_raycast_hit_y = 0;

// Helper: build a collision grid from the tilemap for raycasting
// We use a plain bool array because std::vector<bool> is bit-packed and has no data().
static std::unique_ptr<bool[]> build_collision_grid(GameState* gs) {
    auto& tm = gs->tile_map;
    int w = tm.width();
    int h = tm.height();
    auto grid = std::make_unique<bool[]>(w * h);
    auto& ctypes = tm.collision_types();
    for (int i = 0; i < w * h && i < static_cast<int>(ctypes.size()); i++) {
        grid[i] = (ctypes[i] != eb::CollisionType::None);
    }
    return grid;
}

// raycast(x,y,dx,dy,max_dist) -> hit distance (0 if no hit)
static Value native_raycast(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_number(0);
    auto* gs = s_active_engine->game_state_;
    float x    = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y    = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float dx   = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float dy   = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float dist = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 100;

    auto grid = build_collision_grid(gs);
    Ray ray = {{x, y}, {dx, dy}};
    auto& tm = gs->tile_map;
    RayHit result = raycast_tilemap(ray, grid.get(), tm.width(), tm.height(),
                                     static_cast<float>(tm.tile_size()), dist);
    if (result.hit) {
        s_raycast_hit_x = result.point.x;
        s_raycast_hit_y = result.point.y;
        return val_number(result.distance);
    }
    s_raycast_hit_x = 0;
    s_raycast_hit_y = 0;
    return val_number(0);
}

// raycast_hit_x() -> last hit x
static Value native_raycast_hit_x(int, Value*) {
    return val_number(s_raycast_hit_x);
}

// raycast_hit_y() -> last hit y
static Value native_raycast_hit_y(int, Value*) {
    return val_number(s_raycast_hit_y);
}

// line_of_sight(x1,y1,x2,y2) -> bool
static Value native_line_of_sight(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    float x1 = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y1 = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float x2 = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float y2 = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;

    auto grid = build_collision_grid(gs);
    auto& tm = gs->tile_map;
    bool clear = eb::line_of_sight({x1, y1}, {x2, y2}, grid.get(),
                                    tm.width(), tm.height(),
                                    static_cast<float>(tm.tile_size()));
    return val_bool(clear ? 1 : 0);
}

// =============== Trigger API ===============

// add_trigger(id, x, y, w, h, on_enter, on_exit, on_stay)
static Value native_add_trigger(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 32;
    float h = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 32;
    std::string on_enter = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "";
    std::string on_exit  = (argc > 6 && args[6].type == VAL_STRING) ? args[6].as.string : "";
    std::string on_stay  = (argc > 7 && args[7].type == VAL_STRING) ? args[7].as.string : "";

    // Update existing trigger if same id
    for (auto& t : gs->trigger_zones) {
        if (t.id == id) {
            t.area = {x, y, w, h};
            t.is_circle = false;
            t.on_enter = on_enter; t.on_exit = on_exit; t.on_stay = on_stay;
            return val_nil();
        }
    }
    TriggerZone tz;
    tz.id = id;
    tz.area = {x, y, w, h};
    tz.is_circle = false;
    tz.on_enter = on_enter;
    tz.on_exit = on_exit;
    tz.on_stay = on_stay;
    tz.active = true;
    gs->trigger_zones.push_back(std::move(tz));
    return val_nil();
}

// add_trigger_circle(id, cx, cy, radius, on_enter, on_exit, on_stay)
static Value native_add_trigger_circle(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float cx     = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float cy     = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float radius = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 16;
    std::string on_enter = (argc > 4 && args[4].type == VAL_STRING) ? args[4].as.string : "";
    std::string on_exit  = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "";
    std::string on_stay  = (argc > 6 && args[6].type == VAL_STRING) ? args[6].as.string : "";

    for (auto& t : gs->trigger_zones) {
        if (t.id == id) {
            t.center = {cx, cy}; t.radius = radius;
            t.is_circle = true;
            t.on_enter = on_enter; t.on_exit = on_exit; t.on_stay = on_stay;
            return val_nil();
        }
    }
    TriggerZone tz;
    tz.id = id;
    tz.is_circle = true;
    tz.center = {cx, cy};
    tz.radius = radius;
    tz.on_enter = on_enter;
    tz.on_exit = on_exit;
    tz.on_stay = on_stay;
    tz.active = true;
    gs->trigger_zones.push_back(std::move(tz));
    return val_nil();
}

// remove_trigger(id)
static Value native_remove_trigger(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& zones = gs->trigger_zones;
    zones.erase(std::remove_if(zones.begin(), zones.end(),
        [&](auto& t) { return t.id == id; }), zones.end());
    return val_nil();
}

// set_trigger_active(id, active)
static Value native_set_trigger_active(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    bool active = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                  (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    for (auto& t : gs->trigger_zones) {
        if (t.id == id) { t.active = active; return val_nil(); }
    }
    return val_nil();
}

// =============== Register Methods ===============

void ScriptEngine::register_collision_api() {
    if (!env_) return;
    env_define(env_, "collide_rects", 13, val_native(native_collide_rects));
    env_define(env_, "collide_circles", 15, val_native(native_collide_circles));
    env_define(env_, "collide_rect_circle", 19, val_native(native_collide_rect_circle));
    env_define(env_, "point_in_rect", 13, val_native(native_point_in_rect));
    env_define(env_, "point_in_circle", 15, val_native(native_point_in_circle));
    env_define(env_, "set_collider_rect", 17, val_native(native_set_collider_rect));
    env_define(env_, "set_collider_circle", 19, val_native(native_set_collider_circle));
    env_define(env_, "check_collision", 15, val_native(native_check_collision));
    std::printf("[ScriptEngine] Collision API registered\n");
}

void ScriptEngine::register_raycast_api() {
    if (!env_) return;
    env_define(env_, "raycast", 7, val_native(native_raycast));
    env_define(env_, "raycast_hit_x", 13, val_native(native_raycast_hit_x));
    env_define(env_, "raycast_hit_y", 13, val_native(native_raycast_hit_y));
    env_define(env_, "line_of_sight", 13, val_native(native_line_of_sight));
    std::printf("[ScriptEngine] Raycast API registered\n");
}

void ScriptEngine::register_trigger_api() {
    if (!env_) return;
    env_define(env_, "add_trigger", 11, val_native(native_add_trigger));
    env_define(env_, "add_trigger_circle", 18, val_native(native_add_trigger_circle));
    env_define(env_, "remove_trigger", 14, val_native(native_remove_trigger));
    env_define(env_, "set_trigger_active", 18, val_native(native_set_trigger_active));
    std::printf("[ScriptEngine] Trigger API registered\n");
}

} // namespace eb
