#pragma once

// Shared game logic used by both desktop and Android builds.
// Contains all game state, initialization, update, and rendering code
// that is platform-independent.

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

// ─── Tile IDs ───
enum Tile : int {
    TILE_EMPTY = 0,
    TILE_GRASS1 = 1, TILE_GRASS2, TILE_GRASS3, TILE_GRASS4,
    TILE_DIRT1, TILE_DIRT2, TILE_DIRT3, TILE_DIRT4,
    TILE_ROAD_H, TILE_ROAD_V, TILE_ROAD_CROSS,
    TILE_ROAD_TL, TILE_ROAD_TR, TILE_ROAD_BL, TILE_ROAD_BR,
    TILE_SIDEWALK,
    TILE_WATER_DEEP, TILE_WATER_MID, TILE_WATER_SHORE, TILE_SAND,
    TILE_HEDGE1, TILE_HEDGE2,
    TILE_STONE1, TILE_STONE2, TILE_STONE3, TILE_STONE4,
    TILE_COUNT
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
    int dir = 0;
    float interact_radius = 40.0f;
    std::vector<eb::DialogueLine> dialogue;
    bool has_battle = false;
    std::string battle_enemy_name;
    int battle_enemy_hp = 0;
    int battle_enemy_atk = 0;
    int sprite_atlas_id = -1;
};

// ─── Battle state ───
enum class BattlePhase { None, Intro, PlayerTurn, PlayerAttack, EnemyTurn, EnemyAttack, Victory, Defeat };

struct BattleState {
    BattlePhase phase = BattlePhase::None;
    std::string enemy_name;
    int enemy_hp_actual = 0, enemy_hp_max = 0, enemy_atk = 0;
    int player_hp_actual = 0, player_hp_max = 0;
    float player_hp_display = 0.0f;
    int player_atk = 0, player_def = 0;
    int menu_selection = 0;
    float phase_timer = 0.0f;
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

struct PositionRecord {
    eb::Vec2 pos;
    int dir;
};

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

    // Player
    eb::Vec2 player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
    float player_speed = 120.0f;
    int player_dir = 0, player_frame = 0;
    float anim_timer = 0.0f;
    bool player_moving = false;

    // Player stats
    int player_hp = 100, player_hp_max = 100;
    int player_atk = 18, player_def = 5;
    int player_level = 1, player_xp = 0;

    // Party
    std::vector<PartyMember> party;
    static constexpr int TRAIL_SIZE = 256;
    static constexpr int FOLLOW_DISTANCE = 8;
    std::vector<PositionRecord> trail;
    int trail_head = 0, trail_count = 0;
    float trail_step_accum = 0.0f;

    // NPCs
    std::vector<NPC> npcs;
    std::vector<std::unique_ptr<eb::TextureAtlas>> npc_atlases;
    std::vector<VkDescriptorSet> npc_descs;

    // Dialogue
    eb::DialogueBox dialogue;
    int pending_battle_npc = -1;

    // Battle
    BattleState battle;

    // Timing
    float game_time = 0.0f;
    float steps_since_encounter = 0.0f;
    std::mt19937 rng{std::random_device{}()};

    // Descriptor sets (cached)
    VkDescriptorSet tileset_desc = VK_NULL_HANDLE;
    VkDescriptorSet dean_desc = VK_NULL_HANDLE;
    VkDescriptorSet sam_desc = VK_NULL_HANDLE;
    VkDescriptorSet font_desc = VK_NULL_HANDLE;
    VkDescriptorSet white_desc = VK_NULL_HANDLE;

    bool initialized = false;
};

// ─── Shared functions (implemented in game.cpp) ───

void define_tileset_regions(eb::TextureAtlas& atlas);
void define_npc_atlas_regions(eb::TextureAtlas& atlas, int cw, int ch);
std::vector<int> generate_town_map(int width, int height);
std::vector<int> generate_town_collision(int width, int height);
void setup_objects(GameState& game, eb::Texture* tileset_tex);
void setup_npcs(GameState& game);

// Initialize all game assets and state
bool init_game(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
               float viewport_w, float viewport_h);

// Core game update (platform-independent)
void update_game(GameState& game, const eb::InputState& input, float dt);

// Battle logic
void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random);
void update_battle(GameState& game, float dt, bool confirm, bool up, bool down);

// Rendering
void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text);
void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh);
void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh);

eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas, int dir, bool moving, int frame);
