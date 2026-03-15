#pragma once

#include "engine/core/types.h"
#include "engine/graphics/texture_atlas.h"
#include "game/overworld/tile_map.h"
#include <string>
#include <vector>
#include <unordered_map>

struct NPC;
struct WorldObject;
struct ObjectDef;
struct SpawnLoop;
struct NPCMeetTrigger;
struct WorldDrop;
struct GameState;

namespace eb {
class Renderer;
class ResourceManager;
class ScriptEngine;

struct Level {
    std::string id;
    std::string map_path;
    std::string script_path;
    TileMap tile_map;
    std::vector<NPC> npcs;
    std::vector<WorldObject> objects;
    std::vector<ObjectDef> object_defs;
    std::vector<AtlasRegion> object_regions;
    std::vector<SpawnLoop> spawn_loops;
    std::vector<NPCMeetTrigger> meet_triggers;
    std::vector<WorldDrop> drops;
    Vec2 player_start = {0, 0};
    bool loaded = false;
    bool script_executed = false;
};

struct LevelManager {
    std::unordered_map<std::string, Level> levels;
    std::string active_level;

    // Load a map file into a level slot (does not switch)
    bool load_level(const std::string& id, const std::string& map_path, GameState& game);

    // Switch the active level — swaps per-map state in/out of GameState
    bool switch_level(const std::string& id, GameState& game);

    // Switch and set player position
    bool switch_level_at(const std::string& id, float x, float y, GameState& game);

    // Unload a cached level
    void unload_level(const std::string& id);

    // Save current GameState map data into the active level slot
    void save_active_to_level(GameState& game);

    // Restore level data into GameState
    void restore_level_to_game(const std::string& id, GameState& game);

    // Tick background levels (spawn timers, NPC schedules)
    void tick_background(GameState& game, float dt);

    // Check if level is loaded
    bool is_loaded(const std::string& id) const;

    // Get level count
    int count() const { return (int)levels.size(); }
};

} // namespace eb
