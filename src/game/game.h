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
#include "game/ui/merchant_ui.h"
#include "game/systems/tween.h"
#include "game/systems/particles.h"
#include "game/systems/save_system.h"
#include "game/systems/sprite_anim.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

// Forward declare
namespace eb { class ScriptEngine; class AudioEngine; class Renderer; }
#include "game/systems/level_manager.h"

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
    float scale = 1.0f;            // Render scale multiplier
    eb::Vec4 tint = {1,1,1,1};     // Color tint/alpha
};

struct ObjectDef {
    eb::Vec2 src_pos, src_size, render_size;
};

// ─── Pathfinding ───
struct PathNode { int x, y; };

// ─── NPC Route ───
enum class RouteMode { Patrol, Once, PingPong };

struct NPCRoute {
    std::vector<eb::Vec2> waypoints;
    RouteMode mode = RouteMode::Patrol;
    int current_waypoint = 0;
    bool active = false;
    bool forward = true;
    float stuck_timer = 0.0f;       // Time spent trying to reach current waypoint
    float stuck_timeout = 8.0f;     // Skip waypoint after this many seconds
    int pathfind_failures = 0;      // Consecutive failed A* attempts
};

// ─── NPC Schedule ───
struct NPCSchedule {
    float start_hour = 0.0f;
    float end_hour = 24.0f;
    eb::Vec2 spawn_point = {0, 0};
    bool has_schedule = false;
    bool currently_visible = true;
};

// ─── NPC Meet Trigger ───
struct NPCMeetTrigger {
    std::string npc1_name, npc2_name;
    std::string callback_func;
    float trigger_radius = 40.0f;
    bool fired = false;
    bool repeatable = false;
};

// ─── Day-Night Cycle ───
struct DayNightCycle {
    float day_speed = 1.0f;           // 1.0 = 1 real second per game minute
    float game_hours = 8.0f;          // 0.0 - 24.0 (starts at 8 AM)
    eb::Vec4 current_tint = {1,1,1,1};
};

// ─── Spawn Loop ───
struct SpawnLoop {
    std::string npc_template_name;
    float interval = 60.0f;
    int max_count = 5, current_count = 0;
    float timer = 0.0f;
    bool active = false;
    eb::Vec2 area_min = {0,0}, area_max = {0,0};
    bool has_area = false;
    std::string on_spawn_func;
    // Time-of-day gating (only spawn during these hours)
    float spawn_start_hour = 0.0f;
    float spawn_end_hour = 24.0f;
    bool has_time_gate = false;
};

// ─── Survival Stats ───
struct SurvivalStats {
    float hunger = 100.0f, thirst = 100.0f, energy = 100.0f; // 0-100
    float hunger_rate = 1.0f, thirst_rate = 1.5f, energy_rate = 0.8f; // per game-minute
    bool enabled = false;
};

// ─── HUD Configuration (script-controllable) ───
struct HUDConfig {
    // Player panel
    float player_x = 8, player_y = 8;
    float player_w = 280, player_h = 72;
    float hp_bar_w = 170, hp_bar_h = 14;
    float text_scale = 0.9f;

    // Time panel
    float time_x_offset = 148;  // from right edge
    float time_w = 140, time_h = 64;
    float time_text_scale = 0.9f;

    // Inventory bar
    float inv_slot_size = 46;
    float inv_padding = 4;
    int inv_max_slots = 8;
    float inv_y_offset = 54;  // from bottom edge

    // Survival bars
    float surv_bar_w = 80, surv_bar_h = 8;

    // Minimap
    float minimap_size = 120;  // Base size (before scale)

    // Screen dimensions (updated each frame by render_game_ui)
    float screen_w = 960, screen_h = 720;
    // Native screen pixels (for touch coordinate conversion)
    float native_w = 960, native_h = 720;

    // Global scale
    float scale = 1.5f;

    // Visibility (default off — HUD defined in default.sage via script UI)
    bool show_player = false;
    bool show_time = false;
    bool show_inventory = true;  // Inventory bar still C++ (interactive)
    bool show_survival = false;
    bool show_minimap = true;    // Minimap still C++ (complex rendering)

    // Inventory selection (overworld item use)
    bool inv_open = false;      // Item bar is in selection mode
    int inv_selected = 0;       // Currently highlighted slot
    float inv_use_cooldown = 0; // Prevent rapid-fire use
};

// ─── Script UI ───
struct ScriptUILabel {
    std::string id, text;
    eb::Vec2 position;
    eb::Vec4 color = {1,1,1,1};
    float scale = 0.7f;
    float rotation = 0.0f;         // Rotation in degrees
    float opacity = 1.0f;          // Independent opacity (multiplied with color.w)
    int layer = 0;                 // Z-order layer (higher = on top)
    std::string on_click;          // SageLang callback function name
    bool visible = true;
};
struct ScriptUIBar {
    std::string id;
    float value = 0, max_value = 100;
    eb::Vec2 position;
    float width = 100, height = 12;
    eb::Vec4 color = {0.2f, 0.8f, 0.2f, 1.0f};
    eb::Vec4 bg_color = {0.15f, 0.15f, 0.15f, 0.8f};
    float border_radius = 0.0f;    // Rounded corners (visual hint)
    float opacity = 1.0f;
    int layer = 0;
    bool show_text = false;        // Show "value/max" text overlay
    bool visible = true;
};
struct ScriptUIPanel {
    std::string id;
    eb::Vec2 position;
    float width = 100, height = 60;
    std::string sprite_region;  // UI atlas region name (e.g. "panel_hud_wide")
    eb::Vec4 color = {1,1,1,1}; // Tint / fallback color
    float opacity = 1.0f;
    float scale = 1.0f;           // Panel scale (for animations)
    int layer = 0;
    std::string on_click;
    bool visible = true;
};
struct ScriptUIImage {
    std::string id;
    eb::Vec2 position;
    float width = 32, height = 32;
    std::string icon_name;  // UI atlas region name (e.g. "icon_sword")
    eb::Vec4 tint = {1,1,1,1};
    float rotation = 0.0f;
    float opacity = 1.0f;
    float scale = 1.0f;
    bool flip_h = false;
    bool flip_v = false;
    int layer = 0;
    std::string on_click;
    bool visible = true;
};
struct ScriptUINotification {
    std::string text;
    float duration = 3.0f, timer = 0.0f;
    eb::Vec4 color = {1, 1, 1, 1};
};
struct ScriptUI {
    std::vector<ScriptUILabel> labels;
    std::vector<ScriptUIBar> bars;
    std::vector<ScriptUIPanel> panels;
    std::vector<ScriptUIImage> images;
    std::vector<ScriptUINotification> notifications;
};

// ─── Weather System ───

struct RainDrop {
    float x, y;        // Position (screen space)
    float speed;        // Fall speed
    float length;       // Visual length
    float opacity;
};

struct SnowFlake {
    float x, y;
    float speed;
    float drift;        // Horizontal sine offset
    float size;
    float phase;        // Sine phase for drift
    float opacity;
};

struct CloudShadow {
    float x, y;         // Center position (world space)
    float radius;       // Shadow radius
    float opacity;
    float speed_x, speed_y; // Movement direction
};

struct LightningBolt {
    float x1, y1, x2, y2;  // Start and end
    float timer;            // Time remaining
    float brightness;
    std::vector<std::pair<float,float>> segments; // Fork segments
};

struct WeatherState {
    // Rain
    bool rain_active = false;
    float rain_intensity = 0.5f;    // 0.0 to 1.0 (controls density)
    float rain_speed = 800.0f;      // Pixels per second
    float rain_angle = 5.0f;        // Degrees from vertical (wind)
    float rain_opacity = 0.4f;
    eb::Vec4 rain_color = {0.7f, 0.8f, 1.0f, 0.4f};
    std::vector<RainDrop> rain_drops;
    int rain_max_drops = 200;

    // Snow
    bool snow_active = false;
    float snow_intensity = 0.3f;
    float snow_speed = 60.0f;
    float snow_drift = 30.0f;       // Horizontal drift amount
    eb::Vec4 snow_color = {1.0f, 1.0f, 1.0f, 0.8f};
    std::vector<SnowFlake> snow_flakes;
    int snow_max_flakes = 150;

    // Lightning
    bool lightning_active = false;
    float lightning_interval = 8.0f;   // Seconds between strikes
    float lightning_timer = 0.0f;
    float lightning_chance = 0.3f;     // Probability per interval
    float lightning_flash_duration = 0.15f;
    std::vector<LightningBolt> bolts;

    // Cloud shadows
    bool clouds_active = false;
    float cloud_density = 0.4f;        // 0.0 to 1.0
    float cloud_speed = 20.0f;         // Pixels per second
    float cloud_direction = 45.0f;     // Degrees (0=right, 90=down)
    float cloud_shadow_opacity = 0.25f;
    std::vector<CloudShadow> cloud_shadows;
    int cloud_max = 8;

    // God rays
    bool god_rays_active = false;
    float god_ray_intensity = 0.3f;
    float god_ray_angle = 30.0f;       // Degrees from vertical
    float god_ray_sway = 0.0f;         // Current sway offset (animated)
    int god_ray_count = 5;
    eb::Vec4 god_ray_color = {1.0f, 0.95f, 0.7f, 0.15f};

    // Wind (affects rain angle, snow drift, cloud speed)
    float wind_strength = 0.0f;        // 0.0 to 1.0
    float wind_direction = 0.0f;       // Degrees

    // Fog
    bool fog_active = false;
    float fog_density = 0.3f;
    eb::Vec4 fog_color = {0.8f, 0.85f, 0.9f, 0.3f};
};

// ─── Item type (forward for loot/drops) ───
enum class ItemType { Consumable, Weapon, KeyItem };

// ─── Loot Table ───
struct LootEntry {
    std::string item_id, item_name, description;
    ItemType type = ItemType::Consumable;
    int heal_hp = 0, damage = 0;
    std::string element, sage_func;
    float drop_chance = 0.5f;  // 0.0 to 1.0
};

struct LootTable {
    std::string enemy_name;  // Which enemy type this applies to (or "*" for all)
    std::vector<LootEntry> entries;
};

// ─── World Item Drop ───
struct WorldDrop {
    std::string item_id;
    std::string item_name;
    std::string description;
    eb::Vec2 position;
    float anim_timer = 0.0f;
    int heal_hp = 0, damage = 0;
    std::string element, sage_func;
    ItemType type = ItemType::Consumable;
    float pickup_radius = 24.0f;
    float lifetime = 60.0f;  // Despawn after this many seconds
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
    int sprite_atlas_id = -1;       // Legacy (kept for JSON compat)
    std::string sprite_atlas_key;   // Texture path key into GameState::atlas_cache
    int sprite_grid_w = 0;          // Cell width (0 = auto-detect from texture)
    int sprite_grid_h = 0;          // Cell height (0 = auto-detect from texture)
    bool hostile = false;
    float aggro_range = 150.0f, attack_range = 32.0f;
    float wander_timer = 0.0f, wander_interval = 3.0f;
    eb::Vec2 wander_target;
    float move_speed = 50.0f;
    bool aggro_active = false, has_triggered = false;
    bool despawn_at_day = false;  // Remove when day starts
    std::string loot_func;       // SageLang function called on death to spawn drops

    // Per-sprite visual properties (script-controllable)
    float sprite_scale = 1.0f;   // Render scale (1.0 = default, 2.0 = double size)
    eb::Vec4 sprite_tint = {1,1,1,1}; // Color tint/alpha
    bool sprite_flip_h = false;  // Horizontal flip

    // Pathfinding
    std::vector<PathNode> current_path;
    int path_index = 0;
    bool path_active = false;

    // Route / patrol
    NPCRoute route;

    // Time-based schedule
    NPCSchedule schedule;

    // Sprite animation player (multi-frame)
    eb::AnimPlayer anim_player;
};

// ─── Battle state ───
enum class BattlePhase { None, Intro, PlayerTurn, PlayerAttack, EnemyTurn, EnemyAttack, Victory, Defeat };

struct BattleState {
    BattlePhase phase = BattlePhase::None;
    std::string enemy_name;
    int enemy_hp_actual = 0, enemy_hp_max = 0, enemy_atk = 0;
    int enemy_sprite_id = -1;       // Legacy
    std::string enemy_sprite_key;   // Atlas cache key for battle enemy sprite
    int player_hp_actual = 0, player_hp_max = 0;
    float player_hp_display = 0.0f;
    int player_atk = 0, player_def = 0;
    int sam_hp_actual = 0, sam_hp_max = 0;
    float sam_hp_display = 0.0f;
    int sam_atk = 0;
    int menu_selection = 0, active_fighter = 0;
    bool item_menu_open = false;
    int item_menu_selection = 0;
    float phase_timer = 0.0f, attack_anim_timer = 0.0f;
    std::string message;
    int last_damage = 0;
    bool random_encounter = false;
};

// ─── Item / Inventory ───

struct Item {
    std::string id;         // e.g. "first_aid_kit"
    std::string name;       // e.g. "First Aid Kit"
    std::string description;
    ItemType type = ItemType::Consumable;
    int quantity = 0;
    int max_stack = 99;
    int buy_price = 0;      // Cost to buy from merchant (0 = not for sale)
    int sell_price = 0;     // Value when selling to merchant
    // Battle effects (read by SageLang)
    int heal_hp = 0;
    int damage = 0;
    std::string element;    // "holy", "silver", "fire", etc.
    std::string sage_func;  // SageLang function to call on use, e.g. "use_first_aid"
};

struct Inventory {
    std::vector<Item> items;
    static constexpr int MAX_SLOTS = 20;

    Item* find(const std::string& id) {
        for (auto& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    const Item* find(const std::string& id) const {
        for (auto& it : items) if (it.id == id) return &it;
        return nullptr;
    }
    bool add(const std::string& id, const std::string& name, int count = 1,
             ItemType type = ItemType::Consumable, const std::string& desc = "",
             int heal = 0, int dmg = 0, const std::string& elem = "",
             const std::string& sage = "") {
        if (auto* it = find(id)) {
            it->quantity = std::min(it->quantity + count, it->max_stack);
            return true;
        }
        if ((int)items.size() >= MAX_SLOTS) return false;
        items.push_back({id, name, desc, type, count, 99, 0, 0, heal, dmg, elem, sage});
        return true;
    }
    bool remove(const std::string& id, int count = 1) {
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (it->id == id) {
                it->quantity -= count;
                if (it->quantity <= 0) items.erase(it);
                return true;
            }
        }
        return false;
    }
    int count(const std::string& id) const {
        auto* it = find(id);
        return it ? it->quantity : 0;
    }
    // Get only usable items (consumable/weapon, not key items)
    std::vector<const Item*> get_battle_items() const {
        std::vector<const Item*> result;
        for (auto& it : items)
            if (it.type != ItemType::KeyItem && it.quantity > 0) result.push_back(&it);
        return result;
    }
};

// ─── Character Stats (Fallout S.P.E.C.I.A.L. style) ───
struct CharacterStats {
    int vitality  = 5;  // HP bonus, damage resistance
    int arcana    = 3;  // Magic power, spell damage
    int agility   = 5;  // Speed, crit chance, dodge
    int tactics   = 5;  // Combat strategy, defense bonus
    int spirit    = 4;  // Healing power, magic resistance
    int strength  = 5;  // Physical damage, weapon scaling

    static constexpr int MIN_STAT = 1;
    static constexpr int MAX_STAT = 10;
    static constexpr int STARTING_POINTS = 27;

    int total() const { return vitality + arcana + agility + tactics + spirit + strength; }

    // Derived stat bonuses
    int hp_bonus() const { return vitality * 10; }             // +10 HP per point
    float crit_chance() const { return agility * 0.03f; }      // +3% crit per point
    int defense_bonus() const { return tactics * 2; }          // +2 def per point
    float magic_damage_mult() const { return 1.0f + spirit * 0.1f; }  // +10% magic dmg per point
    int weapon_damage_bonus() const { return strength * 2; }   // +2 weapon dmg per point
    float spell_power_mult() const { return 1.0f + arcana * 0.08f; }  // +8% spell power per point
    float dodge_chance() const { return agility * 0.02f; }     // +2% dodge per point
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

    // Per-sprite visual properties (player + party)
    float player_sprite_scale = 1.0f;
    float ally_sprite_scale = 1.0f;

    // Character stats
    CharacterStats player_stats;
    CharacterStats ally_stats;

    std::vector<PartyMember> party;
    static constexpr int TRAIL_SIZE = 256, FOLLOW_DISTANCE = 8;
    std::vector<PositionRecord> trail;
    int trail_head = 0, trail_count = 0;
    float trail_step_accum = 0.0f;

    std::vector<NPC> npcs;
    // Legacy indexed atlas storage (kept for backward compat with old save files)
    std::vector<std::unique_ptr<eb::TextureAtlas>> npc_atlases;
    std::vector<VkDescriptorSet> npc_descs;
    // String-keyed atlas cache (new system — shared across levels)
    std::unordered_map<std::string, std::shared_ptr<eb::TextureAtlas>> atlas_cache;
    std::unordered_map<std::string, VkDescriptorSet> atlas_descs;

    eb::DialogueBox dialogue;
    int pending_battle_npc = -1;
    BattleState battle;
    float game_time = 0.0f, steps_since_encounter = 0.0f;
    std::mt19937 rng{std::random_device{}()};

    Inventory inventory;
    int gold = 200;
    eb::MerchantUI merchant_ui;
    eb::ScriptEngine* script_engine = nullptr;
    eb::AudioEngine* audio_engine = nullptr;

    // World systems
    DayNightCycle day_night;
    std::vector<NPCMeetTrigger> npc_meet_triggers;
    std::vector<SpawnLoop> spawn_loops;
    SurvivalStats survival;
    HUDConfig hud;
    ScriptUI script_ui;
    std::vector<WorldDrop> world_drops;
    std::vector<LootTable> loot_tables;

    // Weather system
    WeatherState weather;

    // Screen effects (controlled by scripts)
    float shake_intensity = 0, shake_timer = 0;
    float flash_r = 0, flash_g = 0, flash_b = 0, flash_a = 0, flash_timer = 0;
    float fade_r = 0, fade_g = 0, fade_b = 0, fade_a = 0;
    float fade_target = 0, fade_timer = 0, fade_duration = 0;

    // Input state snapshot (set each frame for script access)
    const eb::InputState* current_input = nullptr;

    // Renderer and resource manager (for runtime asset loading)
    eb::Renderer* renderer = nullptr;
    eb::ResourceManager* resource_manager = nullptr;

    // XP multiplier (customizable from scripts)
    float xp_multiplier = 1.0f;

    VkDescriptorSet tileset_desc = VK_NULL_HANDLE;
    VkDescriptorSet dean_desc = VK_NULL_HANDLE;
    VkDescriptorSet sam_desc = VK_NULL_HANDLE;
    VkDescriptorSet font_desc = VK_NULL_HANDLE;
    VkDescriptorSet white_desc = VK_NULL_HANDLE;
    VkDescriptorSet ui_desc = VK_NULL_HANDLE;
    VkDescriptorSet icons_desc = VK_NULL_HANDLE;
    std::unique_ptr<eb::TextureAtlas> ui_atlas;
    std::unique_ptr<eb::TextureAtlas> icons_atlas;
    // Fantasy icons atlas (Shikashi pack — ~300 RPG icons in 32x32 grid)
    VkDescriptorSet fantasy_icons_desc = VK_NULL_HANDLE;
    std::unique_ptr<eb::TextureAtlas> fantasy_icons_atlas;
    // Flat UI pack
    VkDescriptorSet ui_flat_desc = VK_NULL_HANDLE;
    std::unique_ptr<eb::TextureAtlas> ui_flat_atlas;
    bool initialized = false;

    // Pause menu
    bool paused = false;
    int pause_selection = 0;  // 0=Resume, 1=Editor, 2=Levels, 3=Reset, 4=Settings, 5=Quit
    bool pause_request_editor = false;
    bool pause_request_reset = false;
    bool pause_request_quit = false;
    // Level selector sub-menu
    bool level_select_open = false;
    int level_select_cursor = 0;
    std::vector<std::string> level_select_ids;  // Cached level IDs for selector

    // Level system
    std::unique_ptr<eb::LevelManager> level_manager;

    // ─── New Systems (Phase 1) ───

    // Tween engine (smooth animations for UI, camera, NPCs)
    eb::TweenSystem tween_system;

    // Particle system
    std::vector<eb::ParticleEmitter> emitters;

    // Save/load flags (persistent key-value store)
    eb::GameFlags flags;
    float playtime_seconds = 0;

    // Input lock (for cutscenes)
    bool input_locked = false;

    // Gamepad
    bool gamepad_connected = false;

    // ─── Phase 2 Systems ───

    // Screen transitions (iris, wipe, pixelate, etc.)
    eb::ScreenTransition transition;

    // Parallax background layers
    std::vector<eb::ParallaxLayer> parallax_layers;

    // Debug overlay
    bool show_debug_overlay = false;
    float debug_fps = 0;
    int debug_draw_calls = 0;
    int debug_particle_count = 0;

    // ─── Phase 3 Systems ───

    // Quest system
    struct Quest {
        std::string id, title, description;
        enum class State { NotStarted, Active, Complete, Failed } state = State::NotStarted;
        struct Objective { std::string desc; bool complete = false; };
        std::vector<Objective> objectives;
        std::string on_complete;
    };
    std::vector<Quest> quests;
    std::string active_quest_tracker;  // Quest ID pinned to HUD

    // Equipment system
    struct Equipment {
        std::string weapon, armor, accessory, shield;
        int atk_bonus = 0, def_bonus = 0, hp_bonus = 0;
        void recalc(const Inventory& inv) {
            atk_bonus = def_bonus = hp_bonus = 0;
            // Look up item stats from inventory for equipped items
            for (auto& item : inv.items) {
                if (item.id == weapon) atk_bonus += item.damage;
                if (item.id == armor) def_bonus += item.heal_hp; // reuse heal_hp as def for armor
            }
        }
    };
    Equipment equipment;

    // Settings
    struct GameSettings {
        float master_volume = 1.0f;
        float music_volume = 0.8f;
        float sfx_volume = 1.0f;
        bool fullscreen = true;
        int text_speed = 2; // 1=slow 2=normal 3=fast
    };
    GameSettings settings;

    // Dialogue history
    struct DialogueEntry { std::string speaker, text; };
    std::vector<DialogueEntry> dialogue_history;
    std::unordered_map<std::string, bool> talked_to;  // NPC name -> has talked

    // Event system
    struct EventListener { std::string event_name, callback; };
    std::vector<EventListener> event_listeners;

    // Localization
    std::string locale = "en";
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> locale_strings;

    // Achievements
    struct Achievement { std::string id, title, desc; bool unlocked = false; float unlock_time = 0; };
    std::vector<Achievement> achievements;

    // 2D Lighting
    struct Light2D {
        float x, y;               // World position
        float radius = 128.0f;    // Light radius in pixels
        float intensity = 1.0f;   // 0-1
        eb::Vec4 color = {1, 0.9f, 0.7f, 1}; // Warm light default
        bool active = true;
    };
    std::vector<Light2D> lights;
    float ambient_light = 1.0f;   // 0 = pitch black, 1 = full bright
    bool lighting_enabled = false;
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

// Forward declare
namespace eb { struct GameManifest; }
bool init_game_from_manifest(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
                              float viewport_w, float viewport_h, const eb::GameManifest& manifest);
void update_game(GameState& game, const eb::InputState& input, float dt);
void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id = -1);
void update_battle(GameState& game, float dt, bool confirm, bool up, bool down);
void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text);
void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh);
void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh);
eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas, int dir, bool moving, int frame);
