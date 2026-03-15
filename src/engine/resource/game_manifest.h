#pragma once

#include <string>
#include <vector>

namespace eb {

struct SkillsDef {
    int hardiness = 5, unholiness = 3, nerve = 5;
    int tactics = 5, exorcism = 4, riflery = 5;
};

struct SpriteRegionDef {
    std::string name;
    int x, y, w, h;
};

struct CharacterDef {
    std::string name;
    std::string sprite_path;
    int sprite_grid_w = 0, sprite_grid_h = 0;  // 0 = use custom regions
    std::vector<SpriteRegionDef> custom_regions;
    std::string portrait_path;
    int hp = 100, hp_max = 100, atk = 10, def = 5;
    int level = 1, xp = 0;
    float start_x = 480, start_y = 320;
    SkillsDef skills;
};

struct NPCDef {
    std::string name;
    float x = 0, y = 0;
    int dir = 0;
    std::string sprite_path;
    int sprite_grid_w = 0, sprite_grid_h = 0;
    std::string portrait_path;
    std::string dialogue_file;
    float interact_radius = 40;
    bool hostile = false;
    float aggro_range = 150, attack_range = 32;
    float move_speed = 30, wander_interval = 4;
    bool has_battle = false;
    std::string battle_enemy;
    int battle_hp = 0, battle_atk = 0;
};

struct AudioDef {
    std::string overworld;
    std::string battle;
};

struct GameManifest {
    // Game info
    std::string title;
    std::string version;
    int window_width = 960, window_height = 720;

    // Player (character 0)
    CharacterDef player;

    // Party members (characters 1+)
    std::vector<CharacterDef> party;

    // Assets
    std::string tileset_path;
    std::string dialog_bg_path;
    std::string default_font;

    // NPCs
    std::vector<NPCDef> npcs;

    // Scripts to load (in order)
    std::vector<std::string> scripts;

    // Init functions to call after loading
    std::vector<std::string> init_scripts;

    // Audio
    AudioDef audio;

    // Default map
    std::string default_map;

    // Base path (directory containing game.json)
    std::string base_path;

    // Resolve a relative asset path to full path
    std::string resolve(const std::string& relative) const {
        if (base_path.empty()) return relative;
        return base_path + "/" + relative;
    }

    bool loaded = false;
};

// Load a game manifest from a JSON file
bool load_game_manifest(GameManifest& manifest, const std::string& path);

} // namespace eb
