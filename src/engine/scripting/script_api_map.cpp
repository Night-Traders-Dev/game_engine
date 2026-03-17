#include "engine/scripting/script_engine.h"
#include "game/game.h"
#include "game/systems/day_night.h"
#include "game/systems/level_manager.h"
#include "engine/audio/audio_engine.h"
#include "engine/graphics/renderer.h"
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
#include <unordered_map>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== Day-Night API ===============

// get_hour() -> number (0-23)
static Value native_get_hour(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number((int)s_active_engine->game_state_->day_night.game_hours);
}

// get_minute() -> number (0-59)
static Value native_get_minute(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_number((int)((h - (int)h) * 60.0f));
}

// set_time(hour, minute)
static Value native_set_time(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    float h = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float m = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    s_active_engine->game_state_->day_night.game_hours = h + m / 60.0f;
    return val_nil();
}

// set_day_speed(multiplier)
static Value native_set_day_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->day_night.day_speed = std::max(0.0f, std::min(100.0f, (float)args[0].as.number));
    return val_nil();
}

// is_day() -> bool (6-18)
static Value native_is_day(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_bool(h >= 6.0f && h < 18.0f);
}

// is_night() -> bool
static Value native_is_night(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_bool(h >= 18.0f || h < 6.0f);
}

// =============== Map API ===============

// Helper: load a sprite into atlas cache with specific grid size
static void ensure_sprite_cached(GameState* gs, const std::string& path, int grid_w, int grid_h) {
    if (path.empty() || !gs->resource_manager || !gs->renderer) return;
    // Build cache key: path + grid dimensions (same texture with different grids = different atlases)
    std::string cache_key = path;
    if (grid_w > 0 && grid_h > 0) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "@%dx%d", grid_w, grid_h);
        cache_key += buf;
    }
    if (gs->atlas_cache.count(cache_key)) return; // Already cached
    try {
        auto* tex = gs->resource_manager->load_texture(path);
        if (!tex) return;
        int cw = (grid_w > 0) ? grid_w : tex->width() / 3;
        int ch = (grid_h > 0) ? grid_h : tex->height() / 3;
        auto atlas = std::make_shared<eb::TextureAtlas>(tex);
        define_npc_atlas_regions(*atlas, cw, ch);
        gs->atlas_cache[cache_key] = atlas;
        gs->atlas_descs[cache_key] = gs->renderer->get_texture_descriptor(*tex);
    } catch (...) {
        std::fprintf(stderr, "[Script] Failed to load sprite: %s\n", path.c_str());
    }
}

// spawn_npc(name, x, y, dir, hostile, sprite, hp, atk, speed, aggro, grid_w, grid_h)
static Value native_spawn_npc_map(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    NPC npc;
    npc.name = (args[0].type == VAL_STRING) ? args[0].as.string : "NPC";
    npc.position.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc.position.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    npc.home_pos = npc.position;
    npc.wander_target = npc.position;
    if (argc > 3 && args[3].type == VAL_NUMBER) npc.dir = (int)args[3].as.number;
    if (argc > 4) npc.hostile = (args[4].type == VAL_BOOL) ? args[4].as.boolean : (args[4].type == VAL_NUMBER && args[4].as.number != 0);
    // Optional grid size (args 10-11)
    if (argc > 10 && args[10].type == VAL_NUMBER) npc.sprite_grid_w = (int)args[10].as.number;
    if (argc > 11 && args[11].type == VAL_NUMBER) npc.sprite_grid_h = (int)args[11].as.number;
    if (argc > 5) {
        if (args[5].type == VAL_STRING) {
            npc.sprite_atlas_key = args[5].as.string;
            ensure_sprite_cached(gs, npc.sprite_atlas_key, npc.sprite_grid_w, npc.sprite_grid_h);
        }
        else if (args[5].type == VAL_NUMBER) npc.sprite_atlas_id = (int)args[5].as.number;
    }
    if (argc > 6 && args[6].type == VAL_NUMBER) { npc.battle_enemy_hp = (int)args[6].as.number; npc.has_battle = npc.battle_enemy_hp > 0; }
    if (argc > 7 && args[7].type == VAL_NUMBER) npc.battle_enemy_atk = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_NUMBER) npc.move_speed = (float)args[8].as.number;
    if (argc > 9 && args[9].type == VAL_NUMBER) npc.aggro_range = (float)args[9].as.number;
    npc.battle_enemy_name = npc.name;
    npc.dialogue = {{npc.name, "..."}};
    gs->npcs.push_back(npc);
    return val_nil();
}

// place_object(x, y, stamp_name)
static Value native_place_object(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    const char* stamp_name = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    // Find stamp by name
    for (int i = 0; i < (int)gs->object_stamps.size(); i++) {
        if (gs->object_stamps[i].name == stamp_name) {
            WorldObject obj;
            obj.sprite_id = i;
            obj.position = {x, y};
            gs->world_objects.push_back(obj);
            return val_nil();
        }
    }
    return val_nil();
}

// remove_object(x, y)
static Value native_remove_object(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float best_dist = 48.0f;
    int best_idx = -1;
    for (int i = 0; i < (int)gs->world_objects.size(); i++) {
        float dx = gs->world_objects[i].position.x - x;
        float dy = gs->world_objects[i].position.y - y;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < best_dist) { best_dist = d; best_idx = i; }
    }
    if (best_idx >= 0) gs->world_objects.erase(gs->world_objects.begin() + best_idx);
    return val_nil();
}

// set_portal(tx, ty, target_map, target_x, target_y, label)
static Value native_set_portal(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    gs->tile_map.set_collision_at(tx, ty, eb::CollisionType::Portal);
    eb::Portal p;
    p.tile_x = tx; p.tile_y = ty;
    p.target_map = (argc > 2 && args[2].type == VAL_STRING) ? args[2].as.string : "";
    p.target_x = (argc > 3 && args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    p.target_y = (argc > 4 && args[4].type == VAL_NUMBER) ? (int)args[4].as.number : 0;
    p.label = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "portal";
    // Remove existing portal at same tile
    auto& portals = gs->tile_map.portals();
    for (int i = (int)portals.size()-1; i >= 0; i--)
        if (portals[i].tile_x == tx && portals[i].tile_y == ty)
            gs->tile_map.remove_portal(i);
    portals.push_back(p);
    return val_nil();
}

// remove_portal(tx, ty)
static Value native_remove_portal(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    gs->tile_map.set_collision_at(tx, ty, eb::CollisionType::None);
    auto& portals = gs->tile_map.portals();
    for (int i = (int)portals.size()-1; i >= 0; i--)
        if (portals[i].tile_x == tx && portals[i].tile_y == ty)
            gs->tile_map.remove_portal(i);
    return val_nil();
}

// set_collision(tx, ty, type) — type: 0=None, 1=Solid, 2=Portal
static Value native_set_collision(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int type = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    s_active_engine->game_state_->tile_map.set_collision_at(tx, ty, static_cast<eb::CollisionType>(type));
    return val_nil();
}

// set_reflective(tx, ty, bool) — mark tile as reflective (water, ice, etc.)
static Value native_set_reflective(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    bool val = (args[2].type == VAL_BOOL) ? args[2].as.boolean :
               (args[2].type == VAL_NUMBER) ? (args[2].as.number != 0) : false;
    s_active_engine->game_state_->tile_map.set_reflective_at(tx, ty, val);
    return val_nil();
}

// is_reflective(tx, ty) -> bool
static Value native_is_reflective(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_bool(false);
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    return val_bool(s_active_engine->game_state_->tile_map.is_reflective(tx, ty));
}

// set_tile(layer, tx, ty, tile_id)
static Value native_set_tile(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    int tile_id = (args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    s_active_engine->game_state_->tile_map.set_tile(layer, tx, ty, tile_id);
    return val_nil();
}

// add_loot(enemy_name, item_id, item_name, drop_chance, type, desc, heal, dmg, element, sage_func)
static Value native_add_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string enemy = (args[0].type == VAL_STRING) ? args[0].as.string : "*";
    LootEntry entry;
    entry.item_id = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    entry.item_name = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    entry.drop_chance = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0.5f;
    if (argc > 4 && args[4].type == VAL_STRING) {
        const char* t = args[4].as.string;
        if (std::strcmp(t, "weapon") == 0) entry.type = ItemType::Weapon;
        else if (std::strcmp(t, "key") == 0) entry.type = ItemType::KeyItem;
    }
    if (argc > 5 && args[5].type == VAL_STRING) entry.description = args[5].as.string;
    if (argc > 6 && args[6].type == VAL_NUMBER) entry.heal_hp = (int)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) entry.damage = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_STRING) entry.element = args[8].as.string;
    if (argc > 9 && args[9].type == VAL_STRING) entry.sage_func = args[9].as.string;

    // Find or create table for this enemy
    for (auto& table : gs->loot_tables) {
        if (table.enemy_name == enemy) { table.entries.push_back(entry); return val_nil(); }
    }
    gs->loot_tables.push_back({enemy, {entry}});
    return val_nil();
}

// clear_loot(enemy_name) — remove all loot entries for an enemy
static Value native_clear_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string enemy = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& tables = s_active_engine->game_state_->loot_tables;
    tables.erase(std::remove_if(tables.begin(), tables.end(),
        [&](auto& t) { return t.enemy_name == enemy; }), tables.end());
    return val_nil();
}

// drop_item(x, y, id, name, type, desc, heal, dmg, element, sage_func)
static Value native_drop_item(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    WorldDrop drop;
    drop.position.x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    drop.position.y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    drop.item_id = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    drop.item_name = (args[3].type == VAL_STRING) ? args[3].as.string : "";
    if (argc > 4 && args[4].type == VAL_STRING) {
        const char* t = args[4].as.string;
        if (std::strcmp(t, "weapon") == 0) drop.type = ItemType::Weapon;
        else if (std::strcmp(t, "key") == 0) drop.type = ItemType::KeyItem;
    }
    if (argc > 5 && args[5].type == VAL_STRING) drop.description = args[5].as.string;
    if (argc > 6 && args[6].type == VAL_NUMBER) drop.heal_hp = (int)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) drop.damage = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_STRING) drop.element = args[8].as.string;
    if (argc > 9 && args[9].type == VAL_STRING) drop.sage_func = args[9].as.string;
    gs->world_drops.push_back(drop);
    return val_nil();
}

// npc_set_despawn_day(name, enabled) — hostile NPCs disappear at dawn
static Value native_npc_set_despawn_day(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->despawn_at_day = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                                    (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    return val_nil();
}

// npc_set_loot(name, loot_func) — SageLang function called when NPC dies/despawns
static Value native_npc_set_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->loot_func = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    return val_nil();
}

// set_object_scale(x, y, scale) — set scale of nearest object to (x,y)
static Value native_set_object_scale(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float scale = (args[2].type == VAL_NUMBER) ? std::max(0.1f, std::min(10.0f, (float)args[2].as.number)) : 1.0f;
    float best_dist = 48.0f;
    int best_idx = -1;
    for (int i = 0; i < (int)gs->world_objects.size(); i++) {
        float dx = gs->world_objects[i].position.x - x;
        float dy = gs->world_objects[i].position.y - y;
        float d = dx*dx + dy*dy;
        if (d < best_dist*best_dist) { best_dist = std::sqrt(d); best_idx = i; }
    }
    if (best_idx >= 0) gs->world_objects[best_idx].scale = scale;
    return val_nil();
}

// set_object_tint(x, y, r, g, b, a)
static Value native_set_object_tint(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 6) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float best_dist = 48.0f;
    int best_idx = -1;
    for (int i = 0; i < (int)gs->world_objects.size(); i++) {
        float dx = gs->world_objects[i].position.x - x;
        float dy = gs->world_objects[i].position.y - y;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < best_dist) { best_dist = d; best_idx = i; }
    }
    if (best_idx >= 0) {
        gs->world_objects[best_idx].tint = {
            (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1,
            (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1,
            (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 1,
            (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 1
        };
    }
    return val_nil();
}

// =============== Tile Map Query API ===============

static Value native_get_tile(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_number(0);
    auto& map = s_active_engine->game_state_->tile_map;
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    return val_number(eb::tile_id(map.tile_at(layer, tx, ty)));
}

// get_tile_rotation(layer, tx, ty) -> 0-3 (0=0, 1=90, 2=180, 3=270)
static Value native_get_tile_rotation(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_number(0);
    auto& map = s_active_engine->game_state_->tile_map;
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    return val_number(eb::tile_rotation(map.tile_at(layer, tx, ty)));
}

// set_tile_rotation(layer, tx, ty, rotation) — rotation: 0-3
static Value native_set_tile_rotation(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto& map = s_active_engine->game_state_->tile_map;
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    int rot = (args[3].type == VAL_NUMBER) ? ((int)args[3].as.number & 3) : 0;
    int raw = map.tile_at(layer, tx, ty);
    int id = eb::tile_id(raw);
    bool fh = eb::tile_flip_h(raw), fv = eb::tile_flip_v(raw);
    map.set_tile(layer, tx, ty, eb::make_tile(id, rot, fh, fv));
    return val_nil();
}

// set_tile_flip(layer, tx, ty, flip_h, flip_v)
static Value native_set_tile_flip(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto& map = s_active_engine->game_state_->tile_map;
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    bool fh = (args[3].type == VAL_BOOL) ? args[3].as.boolean : (args[3].type == VAL_NUMBER && args[3].as.number != 0);
    bool fv = (args[4].type == VAL_BOOL) ? args[4].as.boolean : (args[4].type == VAL_NUMBER && args[4].as.number != 0);
    int raw = map.tile_at(layer, tx, ty);
    int id = eb::tile_id(raw);
    int rot = eb::tile_rotation(raw);
    map.set_tile(layer, tx, ty, eb::make_tile(id, rot, fh, fv));
    return val_nil();
}

// set_tile_ex(layer, tx, ty, tile_id, rotation, flip_h, flip_v) — full control
static Value native_set_tile_ex(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto& map = s_active_engine->game_state_->tile_map;
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    int id = (args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    int rot = (argc > 4 && args[4].type == VAL_NUMBER) ? ((int)args[4].as.number & 3) : 0;
    bool fh = (argc > 5) && ((args[5].type == VAL_BOOL) ? args[5].as.boolean : (args[5].type == VAL_NUMBER && args[5].as.number != 0));
    bool fv = (argc > 6) && ((args[6].type == VAL_BOOL) ? args[6].as.boolean : (args[6].type == VAL_NUMBER && args[6].as.number != 0));
    map.set_tile(layer, tx, ty, eb::make_tile(id, rot, fh, fv));
    return val_nil();
}

static Value native_is_solid(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_bool(0);
    auto& map = s_active_engine->game_state_->tile_map;
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    return val_bool(map.is_solid(tx, ty));
}

static Value native_is_solid_world(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_bool(0);
    auto& map = s_active_engine->game_state_->tile_map;
    float wx = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float wy = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    int ts = map.tile_size();
    int tx = (int)(wx / ts);
    int ty = (int)(wy / ts);
    return val_bool(map.is_solid(tx, ty));
}

static Value native_get_map_width(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->tile_map.width());
}

static Value native_get_map_height(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->tile_map.height());
}

static Value native_get_tile_size(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->tile_map.tile_size());
}

static Value native_get_layer_count(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->tile_map.layer_count());
}

// =============== Camera API ===============

static Value native_get_camera_x(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->camera.position().x);
}

static Value native_get_camera_y(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->camera.position().y);
}

static Value native_set_camera_pos(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    gs->camera.set_position({x, y});
    return val_nil();
}

static Value native_camera_follow(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float tx = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float ty = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float speed = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1.0f;
    gs->camera.follow({tx, ty}, speed);
    return val_nil();
}

static Value native_camera_center(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    gs->camera.center_on({x, y});
    return val_nil();
}

static Value native_camera_shake(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    gs->shake_intensity = (args[0].type == VAL_NUMBER) ? std::max(0.0f, std::min(50.0f, (float)args[0].as.number)) : 4.0f;
    gs->shake_timer = (args[1].type == VAL_NUMBER) ? std::max(0.0f, std::min(10.0f, (float)args[1].as.number)) : 0.3f;
    return val_nil();
}

static Value native_camera_set_zoom(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    float zoom = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 1.0f;
    if (zoom < 0.1f) zoom = 0.1f;
    float base_w = s_active_engine->game_state_->hud.screen_w;
    float base_h = s_active_engine->game_state_->hud.screen_h;
    s_active_engine->game_state_->camera.set_viewport(base_w / zoom, base_h / zoom);
    return val_nil();
}

static Value native_camera_get_zoom(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(1);
    float base_w = s_active_engine->game_state_->hud.screen_w;
    float vp_w = s_active_engine->game_state_->camera.viewport().x;
    return val_number(vp_w > 0 ? base_w / vp_w : 1.0f);
}

// =============== Audio API ===============

static Value native_play_music(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!is_safe_path(path)) { DLOG_WARN("play_music: rejected unsafe path: %s", path); return val_nil(); }
    bool loop = (argc > 1 && args[1].type == VAL_BOOL) ? args[1].as.boolean : true;
    audio->play_music(path, loop);
    return val_nil();
}

static Value native_stop_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->stop_music();
    return val_nil();
}

static Value native_pause_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->pause_music();
    return val_nil();
}

static Value native_resume_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->resume_music();
    return val_nil();
}

static Value native_set_music_volume(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio && args[0].type == VAL_NUMBER) audio->set_music_volume(std::max(0.0f, std::min(2.0f, (float)args[0].as.number)));
    return val_nil();
}

static Value native_set_master_volume(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio && args[0].type == VAL_NUMBER) audio->set_master_volume(std::max(0.0f, std::min(2.0f, (float)args[0].as.number)));
    return val_nil();
}

static Value native_play_sfx(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!is_safe_path(path)) { DLOG_WARN("play_sfx: rejected unsafe path: %s", path); return val_nil(); }
    float vol = (argc > 1 && args[1].type == VAL_NUMBER) ? std::max(0.0f, std::min(2.0f, (float)args[1].as.number)) : 1.0f;
    audio->play_sfx(path, vol);
    return val_nil();
}

static Value native_crossfade_music(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!is_safe_path(path)) { DLOG_WARN("crossfade_music: rejected unsafe path: %s", path); return val_nil(); }
    float dur = (argc > 1 && args[1].type == VAL_NUMBER) ? std::max(0.01f, std::min(30.0f, (float)args[1].as.number)) : 1.0f;
    bool loop = (argc > 2 && args[2].type == VAL_BOOL) ? args[2].as.boolean : true;
    audio->crossfade_music(path, dur, loop);
    return val_nil();
}

static Value native_is_music_playing(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    auto* audio = s_active_engine->game_state_->audio_engine;
    return val_bool(audio && audio->is_music_playing());
}

// =============== Weather API ===============

static Value native_set_weather(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    const char* type = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float intensity = (argc > 1 && args[1].type == VAL_NUMBER) ? std::max(0.0f, std::min(1.0f, (float)args[1].as.number)) : 0.5f;

    // Reset all
    w.rain_active = w.snow_active = w.lightning_active = false;
    w.clouds_active = w.god_rays_active = w.fog_active = false;

    if (std::strcmp(type, "rain") == 0) {
        w.rain_active = true; w.rain_intensity = intensity;
        w.clouds_active = true; w.cloud_density = intensity * 0.6f;
    } else if (std::strcmp(type, "storm") == 0) {
        w.rain_active = true; w.rain_intensity = std::max(0.7f, intensity);
        w.lightning_active = true; w.lightning_chance = intensity;
        w.clouds_active = true; w.cloud_density = 0.7f;
        w.wind_strength = 0.6f;
    } else if (std::strcmp(type, "snow") == 0) {
        w.snow_active = true; w.snow_intensity = intensity;
        w.clouds_active = true; w.cloud_density = intensity * 0.4f;
        w.fog_active = true; w.fog_density = intensity * 0.2f;
    } else if (std::strcmp(type, "fog") == 0) {
        w.fog_active = true; w.fog_density = intensity;
        w.clouds_active = true; w.cloud_density = intensity * 0.3f;
    } else if (std::strcmp(type, "cloudy") == 0) {
        w.clouds_active = true; w.cloud_density = intensity;
        w.god_rays_active = true; w.god_ray_intensity = intensity * 0.3f;
    } else if (std::strcmp(type, "clear") == 0) {
        // Everything already reset
    }
    return val_nil();
}

static Value native_set_rain(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.rain_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.rain_intensity = std::max(0.0f, std::min(1.0f, (float)args[1].as.number));
    return val_nil();
}

static Value native_set_snow(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.snow_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.snow_intensity = std::max(0.0f, std::min(1.0f, (float)args[1].as.number));
    return val_nil();
}

static Value native_set_lightning(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.lightning_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.lightning_interval = std::max(1.0f, (float)args[1].as.number);
    if (argc > 2 && args[2].type == VAL_NUMBER) w.lightning_chance = std::max(0.0f, std::min(1.0f, (float)args[2].as.number));
    return val_nil();
}

static Value native_set_clouds(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.clouds_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.cloud_density = std::max(0.0f, std::min(1.0f, (float)args[1].as.number));
    if (argc > 2 && args[2].type == VAL_NUMBER) w.cloud_speed = (float)args[2].as.number;
    if (argc > 3 && args[3].type == VAL_NUMBER) w.cloud_direction = (float)args[3].as.number;
    return val_nil();
}

static Value native_set_god_rays(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.god_rays_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.god_ray_intensity = std::max(0.0f, std::min(1.0f, (float)args[1].as.number));
    if (argc > 2 && args[2].type == VAL_NUMBER) w.god_ray_count = std::max(1, std::min(20, (int)args[2].as.number));
    return val_nil();
}

static Value native_set_fog(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.fog_active = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    if (argc > 1 && args[1].type == VAL_NUMBER) w.fog_density = std::max(0.0f, std::min(1.0f, (float)args[1].as.number));
    if (argc > 2 && args[2].type == VAL_NUMBER) w.fog_color.x = (float)args[2].as.number;
    if (argc > 3 && args[3].type == VAL_NUMBER) w.fog_color.y = (float)args[3].as.number;
    if (argc > 4 && args[4].type == VAL_NUMBER) w.fog_color.z = (float)args[4].as.number;
    return val_nil();
}

static Value native_set_wind(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.wind_strength = (args[0].type == VAL_NUMBER) ? std::max(0.0f, std::min(1.0f, (float)args[0].as.number)) : 0;
    if (argc > 1 && args[1].type == VAL_NUMBER) w.wind_direction = (float)args[1].as.number;
    return val_nil();
}

static Value native_set_rain_color(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto& w = s_active_engine->game_state_->weather;
    w.rain_color.x = (float)args[0].as.number;
    w.rain_color.y = (float)args[1].as.number;
    w.rain_color.z = (float)args[2].as.number;
    if (argc > 3) w.rain_color.w = (float)args[3].as.number;
    return val_nil();
}

static Value native_get_weather(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_string("clear");
    auto& w = s_active_engine->game_state_->weather;
    if (w.rain_active && w.lightning_active) return val_string("storm");
    if (w.rain_active) return val_string("rain");
    if (w.snow_active) return val_string("snow");
    if (w.fog_active) return val_string("fog");
    if (w.clouds_active) return val_string("cloudy");
    return val_string("clear");
}

static Value native_is_raining(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->weather.rain_active);
}

static Value native_is_snowing(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    return val_bool(s_active_engine->game_state_->weather.snow_active);
}

// =============== Level API ===============

// load_level(id, map_path)
static Value native_load_level(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_bool(0);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string path = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    if (!is_safe_path(path.c_str())) { DLOG_WARN("load_level: rejected unsafe path: %s", path.c_str()); return val_bool(0); }
    return val_bool(gs->level_manager->load_level(id, path, *gs));
}

// switch_level(id)
static Value native_switch_level(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_bool(0);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_bool(gs->level_manager->switch_level(id, *gs));
}

// switch_level_at(id, x, y)
static Value native_switch_level_at(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_bool(0);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    return val_bool(gs->level_manager->switch_level_at(id, x, y, *gs));
}

// preload_level(id, map_path) — same as load_level, just a semantic alias
static Value native_preload_level(int argc, Value* args) {
    return native_load_level(argc, args); // Same implementation
}

// unload_level(id)
static Value native_unload_level(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    gs->level_manager->unload_level(id);
    return val_nil();
}

// get_active_level() -> string
static Value native_get_active_level(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_number(0);
    if (s_active_engine) s_active_engine->set_string("_active_level", gs->level_manager->active_level);
    return val_number(0);
}

// is_level_loaded(id) -> bool
static Value native_is_level_loaded(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_bool(0);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_bool(gs->level_manager->is_loaded(id));
}

// get_level_count() -> number
static Value native_get_level_count(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_number(0);
    return val_number(gs->level_manager->count());
}

// set_level_spawn(id, x, y)
static Value native_set_level_spawn(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto it = gs->level_manager->levels.find(id);
    if (it != gs->level_manager->levels.end()) {
        it->second.player_start.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
        it->second.player_start.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    }
    return val_nil();
}

// level_get_npc_count(id) -> number
static Value native_level_get_npc_count(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_number(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_number(0);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto it = gs->level_manager->levels.find(id);
    if (it != gs->level_manager->levels.end()) return val_number((int)it->second.npcs.size());
    return val_number(0);
}

// set_level_zoom(id, zoom) — set per-level camera zoom (applied on switch)
static Value native_set_level_zoom(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float zoom = (args[1].type == VAL_NUMBER) ? std::max(0.25f, std::min(8.0f, (float)args[1].as.number)) : 1.0f;
    auto it = gs->level_manager->levels.find(id);
    if (it != gs->level_manager->levels.end()) {
        it->second.zoom = zoom;
    }
    // If setting zoom for active level, apply immediately
    if (id == gs->level_manager->active_level) {
        float base_w = gs->hud.screen_w, base_h = gs->hud.screen_h;
        gs->camera.set_viewport(base_w / zoom, base_h / zoom);
    }
    return val_nil();
}

// get_level_zoom(id) -> number
static Value native_get_level_zoom(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_number(1);
    auto* gs = s_active_engine->game_state_;
    if (!gs->level_manager) return val_number(1);
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto it = gs->level_manager->levels.find(id);
    if (it != gs->level_manager->levels.end()) return val_number(it->second.zoom);
    return val_number(1);
}

// =============== Register Methods ===============

void ScriptEngine::register_daynight_api() {
    if (!env_) return;
    env_define(env_, "get_hour", 8, val_native(native_get_hour));
    env_define(env_, "get_minute", 10, val_native(native_get_minute));
    env_define(env_, "set_time", 8, val_native(native_set_time));
    env_define(env_, "set_day_speed", 13, val_native(native_set_day_speed));
    env_define(env_, "is_day", 6, val_native(native_is_day));
    env_define(env_, "is_night", 8, val_native(native_is_night));
    std::printf("[ScriptEngine] Day-Night API registered\n");
}

void ScriptEngine::register_map_api() {
    if (!env_) return;
    env_define(env_, "spawn_npc", 9, val_native(native_spawn_npc_map));
    env_define(env_, "place_object", 12, val_native(native_place_object));
    env_define(env_, "remove_object", 13, val_native(native_remove_object));
    env_define(env_, "set_portal", 10, val_native(native_set_portal));
    env_define(env_, "remove_portal", 13, val_native(native_remove_portal));
    env_define(env_, "set_collision", 13, val_native(native_set_collision));
    env_define(env_, "set_reflective", 14, val_native(native_set_reflective));
    env_define(env_, "is_reflective", 13, val_native(native_is_reflective));
    env_define(env_, "set_tile", 8, val_native(native_set_tile));
    env_define(env_, "drop_item", 9, val_native(native_drop_item));
    env_define(env_, "add_loot", 8, val_native(native_add_loot));
    env_define(env_, "clear_loot", 10, val_native(native_clear_loot));
    env_define(env_, "npc_set_despawn_day", 19, val_native(native_npc_set_despawn_day));
    env_define(env_, "npc_set_loot", 12, val_native(native_npc_set_loot));
    env_define(env_, "set_object_scale", 16, val_native(native_set_object_scale));
    env_define(env_, "set_object_tint", 15, val_native(native_set_object_tint));
    std::printf("[ScriptEngine] Map API registered\n");
}

void ScriptEngine::register_tilemap_api() {
    if (!env_) return;
    env_define(env_, "get_tile", 8, val_native(native_get_tile));
    env_define(env_, "get_tile_rotation", 17, val_native(native_get_tile_rotation));
    env_define(env_, "set_tile_rotation", 17, val_native(native_set_tile_rotation));
    env_define(env_, "set_tile_flip", 13, val_native(native_set_tile_flip));
    env_define(env_, "set_tile_ex", 11, val_native(native_set_tile_ex));
    env_define(env_, "is_solid", 8, val_native(native_is_solid));
    env_define(env_, "is_solid_world", 14, val_native(native_is_solid_world));
    env_define(env_, "get_map_width", 13, val_native(native_get_map_width));
    env_define(env_, "get_map_height", 14, val_native(native_get_map_height));
    env_define(env_, "get_tile_size", 13, val_native(native_get_tile_size));
    env_define(env_, "get_layer_count", 15, val_native(native_get_layer_count));
    std::printf("[ScriptEngine] Tile Map API registered\n");
}

void ScriptEngine::register_camera_api() {
    if (!env_) return;
    env_define(env_, "get_camera_x", 12, val_native(native_get_camera_x));
    env_define(env_, "get_camera_y", 12, val_native(native_get_camera_y));
    env_define(env_, "set_camera_pos", 14, val_native(native_set_camera_pos));
    env_define(env_, "camera_follow", 13, val_native(native_camera_follow));
    env_define(env_, "camera_center", 13, val_native(native_camera_center));
    env_define(env_, "camera_shake", 12, val_native(native_camera_shake));
    env_define(env_, "camera_set_zoom", 15, val_native(native_camera_set_zoom));
    env_define(env_, "camera_get_zoom", 15, val_native(native_camera_get_zoom));
    std::printf("[ScriptEngine] Camera API registered\n");
}

void ScriptEngine::register_audio_api() {
    if (!env_) return;
    env_define(env_, "play_music", 10, val_native(native_play_music));
    env_define(env_, "stop_music", 10, val_native(native_stop_music));
    env_define(env_, "pause_music", 11, val_native(native_pause_music));
    env_define(env_, "resume_music", 12, val_native(native_resume_music));
    env_define(env_, "set_music_volume", 16, val_native(native_set_music_volume));
    env_define(env_, "set_master_volume", 17, val_native(native_set_master_volume));
    env_define(env_, "play_sfx", 8, val_native(native_play_sfx));
    env_define(env_, "crossfade_music", 15, val_native(native_crossfade_music));
    env_define(env_, "is_music_playing", 16, val_native(native_is_music_playing));
    std::printf("[ScriptEngine] Audio API registered\n");
}

void ScriptEngine::register_weather_api() {
    if (!env_) return;
    env_define(env_, "set_weather", 11, val_native(native_set_weather));
    env_define(env_, "get_weather", 11, val_native(native_get_weather));
    env_define(env_, "set_rain", 8, val_native(native_set_rain));
    env_define(env_, "set_snow", 8, val_native(native_set_snow));
    env_define(env_, "set_lightning", 13, val_native(native_set_lightning));
    env_define(env_, "set_clouds", 10, val_native(native_set_clouds));
    env_define(env_, "set_god_rays", 12, val_native(native_set_god_rays));
    env_define(env_, "set_fog", 7, val_native(native_set_fog));
    env_define(env_, "set_wind", 8, val_native(native_set_wind));
    env_define(env_, "set_rain_color", 14, val_native(native_set_rain_color));
    env_define(env_, "is_raining", 10, val_native(native_is_raining));
    env_define(env_, "is_snowing", 10, val_native(native_is_snowing));
    std::printf("[ScriptEngine] Weather API registered\n");
}

void ScriptEngine::register_level_api() {
    if (!env_) return;
    env_define(env_, "load_level", 10, val_native(native_load_level));
    env_define(env_, "switch_level", 12, val_native(native_switch_level));
    env_define(env_, "switch_level_at", 15, val_native(native_switch_level_at));
    env_define(env_, "preload_level", 13, val_native(native_preload_level));
    env_define(env_, "unload_level", 12, val_native(native_unload_level));
    env_define(env_, "get_active_level", 16, val_native(native_get_active_level));
    env_define(env_, "is_level_loaded", 15, val_native(native_is_level_loaded));
    env_define(env_, "get_level_count", 15, val_native(native_get_level_count));
    env_define(env_, "set_level_spawn", 15, val_native(native_set_level_spawn));
    env_define(env_, "level_get_npc_count", 19, val_native(native_level_get_npc_count));
    env_define(env_, "set_level_zoom", 14, val_native(native_set_level_zoom));
    env_define(env_, "get_level_zoom", 14, val_native(native_get_level_zoom));
    std::printf("[ScriptEngine] Level API registered\n");
}

} // namespace eb
