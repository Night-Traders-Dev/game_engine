#pragma once

#include "engine/core/types.h"
#include "engine/graphics/renderer.h"
#include "engine/graphics/texture.h"
#include "engine/graphics/texture_atlas.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/text_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/platform/platform.h"
#include "engine/platform/input.h"
#include "game/overworld/camera.h"
#include "game/overworld/tile_map.h"
#include "game/dialogue/dialogue_box.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

// Forward declare
namespace eb { class ScriptEngine; }

// ─── Tile IDs (1-indexed, 0 = empty) ───
enum Tile : int {
    TILE_EMPTY = 0,
    // Grass (1-4)
    TILE_GRASS_PURE = 1, TILE_GRASS_LIGHT, TILE_GRASS_FLOWERS, TILE_GRASS_DARK,
    // Dirt (5-8)
    TILE_DIRT_BROWN, TILE_DIRT_DARK, TILE_DIRT_MUD, TILE_DIRT_GRAVEL,
    // Grass edges & hedges (9-12)
    TILE_GRASS_EDGE1, TILE_GRASS_EDGE2, TILE_HEDGE1, TILE_HEDGE2,
    // Dirt paths (13-18)
    TILE_DIRT_PATH1, TILE_DIRT_PATH2, TILE_DIRT_MIXED1, TILE_DIRT_MIXED2,
    TILE_STONE_PATH1, TILE_STONE_PATH2,
    // Special ground (19-24)
    TILE_PENTAGRAM, TILE_DARK_GROUND1, TILE_DARK_GROUND2,
    TILE_BLOOD_DIRT, TILE_DARK_STONE1, TILE_DARK_STONE2,
    // Roads (25-36)
    TILE_ROAD_H, TILE_ROAD_V, TILE_ROAD_CROSS,
    TILE_ROAD_TL, TILE_ROAD_TR, TILE_ROAD_BL, TILE_ROAD_BR,
    TILE_SIDEWALK, TILE_SIDEWALK2, TILE_ASPHALT,
    TILE_ROAD_MARKING, TILE_CURB,
    // Road extras (37-41)
    TILE_ROAD_DIRT, TILE_ROAD_BLOOD1, TILE_ROAD_PATCH1, TILE_ROAD_PATCH2, TILE_ROAD_BLOOD2,
    // Water & shore (42-50)
    TILE_WATER_DEEP, TILE_WATER_MID, TILE_WATER_SHORE_L, TILE_WATER_SHORE_R,
    TILE_SAND, TILE_SAND_WET, TILE_WATER_SHALLOW, TILE_WATER_BLOOD, TILE_SAND_DARK,
    // Water objects as tiles (51-54)
    TILE_BENCH_WATER, TILE_ROCK_WATER1, TILE_ROCK_WATER2, TILE_ROCK_WATER3,
    TILE_COUNT
};

// ─── Object definition (multi-tile stamps from tileset) ───
struct ObjectStamp {
    std::string name;
    int region_id;       // Index into tileset atlas
    float src_w, src_h;  // Size in tileset pixels
    float place_w, place_h; // Size when placed in world
    std::string category; // "building", "vehicle", "tree", "misc"
};

// ─── World object (trees, buildings) ───
struct WorldObject {
    int sprite_id;
    eb::Vec2 position;
};

struct ObjectDef {
    eb::Vec2 src_pos, src_size, render_size;
};

// ─── NPC ───
struct NPC {
    std::string name;
    eb::Vec2 position;
    eb::Vec2 home_pos;
    int dir = 0, frame = 0;
    float anim_timer = 0.0f;
    bool moving = false;
    float interact_radius = 40.0f;
    std::vector<eb::DialogueLine> dialogue;
    bool has_battle = false;
    std::string battle_enemy_name;
    int battle_enemy_hp = 0, battle_enemy_atk = 0;
    int sprite_atlas_id = -1;
    bool hostile = false;
    float aggro_range = 150.0f, attack_range = 32.0f;
    float wander_timer = 0.0f, wander_interval = 3.0f;
    eb::Vec2 wander_target;
    float move_speed = 50.0f;
    bool aggro_active = false, has_triggered = false;
};

// ─── Battle state ───
enum class BattlePhase { None, Intro, PlayerTurn, PlayerAttack, EnemyTurn, EnemyAttack, Victory, Defeat };

struct BattleState {
    BattlePhase phase = BattlePhase::None;
    std::string enemy_name;
    int enemy_hp_actual = 0, enemy_hp_max = 0, enemy_atk = 0;
    int enemy_sprite_id = -1;
    int player_hp_actual = 0, player_hp_max = 0;
    float player_hp_display = 0.0f;
    int player_atk = 0, player_def = 0;
    int sam_hp_actual = 0, sam_hp_max = 0;
    float sam_hp_display = 0.0f;
    int sam_atk = 0;
    int menu_selection = 0, active_fighter = 0;
    float phase_timer = 0.0f, attack_anim_timer = 0.0f;
    std::string message;
    int last_damage = 0;
    bool random_encounter = false;
};

// ─── Party follower ───
struct PartyMember {
    std::string name;
    eb::Vec2 position;
    int dir = 0, frame = 0;
    float anim_timer = 0.0f;
    bool moving = false;
};

struct PositionRecord { eb::Vec2 pos; int dir; };

// ─── Game state ───
struct GameState {
    eb::Camera camera;
    eb::TileMap tile_map;
    std::unique_ptr<eb::TextureAtlas> tileset_atlas;
    std::unique_ptr<eb::TextureAtlas> dean_atlas;
    std::unique_ptr<eb::TextureAtlas> sam_atlas;

    std::vector<eb::AtlasRegion> object_regions;
    std::vector<ObjectDef> object_defs;
    std::vector<WorldObject> world_objects;
    std::vector<ObjectStamp> object_stamps; // All available stamps

    eb::Vec2 player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
    float player_speed = 120.0f;
    int player_dir = 0, player_frame = 0;
    float anim_timer = 0.0f;
    bool player_moving = false;
    int player_hp = 100, player_hp_max = 100;
    int player_atk = 18, player_def = 5;
    int player_level = 1, player_xp = 0;
    int sam_hp = 90, sam_hp_max = 90, sam_atk = 15;

    std::vector<PartyMember> party;
    static constexpr int TRAIL_SIZE = 256, FOLLOW_DISTANCE = 8;
    std::vector<PositionRecord> trail;
    int trail_head = 0, trail_count = 0;
    float trail_step_accum = 0.0f;

    std::vector<NPC> npcs;
    std::vector<std::unique_ptr<eb::TextureAtlas>> npc_atlases;
    std::vector<VkDescriptorSet> npc_descs;

    eb::DialogueBox dialogue;
    int pending_battle_npc = -1;
    BattleState battle;
    float game_time = 0.0f, steps_since_encounter = 0.0f;
    std::mt19937 rng{std::random_device{}()};

    eb::ScriptEngine* script_engine = nullptr;

    VkDescriptorSet tileset_desc = VK_NULL_HANDLE;
    VkDescriptorSet dean_desc = VK_NULL_HANDLE;
    VkDescriptorSet sam_desc = VK_NULL_HANDLE;
    VkDescriptorSet font_desc = VK_NULL_HANDLE;
    VkDescriptorSet white_desc = VK_NULL_HANDLE;
    bool initialized = false;
};

// ─── Map file I/O ───
bool save_map_file(const GameState& game, const std::string& path);
bool load_map_file(GameState& game, eb::Renderer& renderer, const std::string& path);

// ─── Dialogue file I/O ───
// Dialogue files use @function_name sections with Speaker: Text lines
struct DialogueFunction {
    std::string name;
    std::vector<eb::DialogueLine> lines;
};

struct DialogueScript {
    std::string filename;
    std::vector<DialogueFunction> functions;

    // Get a dialogue function by name (e.g. "greeting", "after_battle")
    const DialogueFunction* get(const std::string& name) const;
    // Get lines for a function (convenience)
    std::vector<eb::DialogueLine> get_lines(const std::string& name) const;
};

bool load_dialogue_file(DialogueScript& script, const std::string& path);


// ─── Shared functions ───
void define_tileset_regions(eb::TextureAtlas& atlas);
void define_object_stamps(GameState& game);
void define_npc_atlas_regions(eb::TextureAtlas& atlas, int cw, int ch);
std::vector<int> generate_town_map(int width, int height);
std::vector<int> generate_town_collision(int width, int height);
void setup_objects(GameState& game, eb::Texture* tileset_tex);
void setup_npcs(GameState& game);
bool init_game(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
               float viewport_w, float viewport_h);
void update_game(GameState& game, const eb::InputState& input, float dt);
void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id = -1);
void update_battle(GameState& game, float dt, bool confirm, bool up, bool down);
void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text);
void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh);
void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh);
eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas, int dir, bool moving, int frame);
