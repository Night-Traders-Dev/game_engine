#include "game/systems/level_manager.h"
#include "game/game.h"
#include "game/systems/day_night.h"
#include "engine/resource/file_io.h"
#include "engine/scripting/script_engine.h"

#include <cstdio>
#include <algorithm>

namespace eb {

bool LevelManager::load_level(const std::string& id, const std::string& map_path, GameState& game) {
    // Don't reload if already loaded
    if (levels.count(id) && levels[id].loaded) return true;

    Level level;
    level.id = id;
    level.map_path = map_path;

    // Derive script path
    std::string name = map_path;
    auto slash = name.rfind('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    level.script_path = "assets/scripts/maps/" + name + ".sage";

    // Load tile map
    if (!level.tile_map.load_json(map_path)) {
        std::fprintf(stderr, "[LevelManager] Failed to load map: %s\n", map_path.c_str());
        return false;
    }

    // Parse NPCs and objects from the map JSON (reuse load_map_file logic)
    auto data = FileIO::read_file(map_path);
    if (!data.empty()) {
        // Extract player start from metadata
        std::string json(data.begin(), data.end());
        // Simple extraction of player_start_x/y from JSON
        auto find_num = [&](const char* key) -> float {
            auto pos = json.find(key);
            if (pos == std::string::npos) return 0;
            pos = json.find(':', pos);
            if (pos == std::string::npos) return 0;
            pos++;
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            return std::stof(json.substr(pos));
        };
        level.player_start.x = find_num("\"player_start_x\"");
        level.player_start.y = find_num("\"player_start_y\"");
    }

    level.loaded = true;
    levels[id] = std::move(level);

    std::printf("[LevelManager] Loaded level '%s' from %s\n", id.c_str(), map_path.c_str());
    return true;
}

void LevelManager::save_active_to_level(GameState& game) {
    if (active_level.empty()) return;
    auto it = levels.find(active_level);
    if (it == levels.end()) return;

    auto& lvl = it->second;
    lvl.tile_map = game.tile_map;
    lvl.npcs = game.npcs;
    lvl.objects = game.world_objects;
    lvl.object_defs = game.object_defs;
    lvl.object_regions = game.object_regions;
    lvl.spawn_loops = game.spawn_loops;
    lvl.meet_triggers = game.npc_meet_triggers;
    lvl.drops = game.world_drops;
}

void LevelManager::restore_level_to_game(const std::string& id, GameState& game) {
    auto it = levels.find(id);
    if (it == levels.end()) return;

    auto& lvl = it->second;
    game.tile_map = lvl.tile_map;
    game.npcs = lvl.npcs;
    game.world_objects = lvl.objects;
    game.object_defs = lvl.object_defs;
    game.object_regions = lvl.object_regions;
    game.spawn_loops = lvl.spawn_loops;
    game.npc_meet_triggers = lvl.meet_triggers;
    game.world_drops = lvl.drops;

    // Update camera bounds
    game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
}

bool LevelManager::switch_level(const std::string& id, GameState& game) {
    if (!levels.count(id) || !levels[id].loaded) {
        std::fprintf(stderr, "[LevelManager] Level '%s' not loaded\n", id.c_str());
        return false;
    }

    // Save current level state
    save_active_to_level(game);

    // Restore new level
    restore_level_to_game(id, game);
    active_level = id;

    // Set player to level's spawn point
    auto& lvl = levels[id];
    if (lvl.player_start.x > 0 || lvl.player_start.y > 0) {
        game.player_pos = lvl.player_start;
    }

    // Execute level script if not yet done
    if (!lvl.script_executed && game.script_engine) {
        auto script_data = FileIO::read_file(lvl.script_path);
        if (!script_data.empty()) {
            std::string src(script_data.begin(), script_data.end());
            game.script_engine->execute(src);
        }
        // Run level-specific map_init
        std::string init_func = id + "_init";
        if (game.script_engine->has_function(init_func)) {
            game.script_engine->call_function(init_func);
        } else if (game.script_engine->has_function("map_init")) {
            game.script_engine->call_function("map_init");
        }
        lvl.script_executed = true;
    }

    // Center camera on player
    game.camera.center_on(game.player_pos);

    std::printf("[LevelManager] Switched to level '%s'\n", id.c_str());
    return true;
}

bool LevelManager::switch_level_at(const std::string& id, float x, float y, GameState& game) {
    if (!switch_level(id, game)) return false;
    game.player_pos = {x, y};
    game.camera.center_on(game.player_pos);
    return true;
}

void LevelManager::unload_level(const std::string& id) {
    if (id == active_level) return; // Can't unload active level
    levels.erase(id);
    std::printf("[LevelManager] Unloaded level '%s'\n", id.c_str());
}

bool LevelManager::is_loaded(const std::string& id) const {
    auto it = levels.find(id);
    return it != levels.end() && it->second.loaded;
}

void LevelManager::tick_background(GameState& game, float dt) {
    for (auto& [id, lvl] : levels) {
        if (id == active_level || !lvl.loaded) continue;

        // Tick NPC schedules on background levels
        for (auto& npc : lvl.npcs) {
            if (npc.schedule.has_schedule) {
                bool in_range = is_hour_in_range(game.day_night.game_hours,
                                                  npc.schedule.start_hour, npc.schedule.end_hour);
                npc.schedule.currently_visible = in_range;
            }
        }

        // Advance spawn loop timers (don't actually spawn — just track time)
        for (auto& loop : lvl.spawn_loops) {
            if (!loop.active) continue;
            if (loop.has_time_gate) {
                if (!is_hour_in_range(game.day_night.game_hours,
                                       loop.spawn_start_hour, loop.spawn_end_hour)) {
                    loop.timer = 0;
                    continue;
                }
            }
            loop.timer += dt;
        }
    }
}

} // namespace eb
