#include "engine/core/engine.h"
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
#include "editor/tile_editor.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#include <cmath>
#include <cstring>
#include <sys/stat.h>
#include <random>

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
    int dir = 0; // 0=down, 1=up, 2=left, 3=right
    float interact_radius = 40.0f;
    std::vector<eb::DialogueLine> dialogue;
    bool has_battle = false;
    std::string battle_enemy_name;
    int battle_enemy_hp = 0;
    int battle_enemy_atk = 0;
};

// ─── Battle state ───
enum class BattlePhase { None, Intro, PlayerTurn, PlayerAttack, EnemyTurn, EnemyAttack, Victory, Defeat };

struct BattleState {
    BattlePhase phase = BattlePhase::None;
    std::string enemy_name;
    int enemy_hp_actual = 0;
    int enemy_hp_max = 0;
    int enemy_atk = 0;

    int player_hp_actual = 0;
    int player_hp_max = 0;
    float player_hp_display = 0.0f; // Rolling HP odometer
    int player_atk = 0;
    int player_def = 0;

    int menu_selection = 0;
    float phase_timer = 0.0f;
    std::string message;
    int last_damage = 0;
    bool random_encounter = false;
};

// ─── Party follower (EarthBound-style breadcrumb trail) ───
struct PartyMember {
    std::string name;
    eb::Vec2 position;
    int dir = 0;
    int frame = 0;
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
    eb::TileEditor editor;

    std::vector<eb::AtlasRegion> object_regions;
    std::vector<ObjectDef> object_defs;
    std::vector<WorldObject> world_objects;

    // Player
    eb::Vec2 player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
    float player_speed = 120.0f;
    int player_dir = 0;
    int player_frame = 0;
    float anim_timer = 0.0f;
    bool player_moving = false;

    // Player stats
    int player_hp = 100;
    int player_hp_max = 100;
    int player_atk = 18;
    int player_def = 5;
    int player_level = 1;
    int player_xp = 0;

    // Party members
    std::vector<PartyMember> party;
    // Breadcrumb trail: records of leader positions, followers pick from this
    static constexpr int TRAIL_SIZE = 256;
    static constexpr int FOLLOW_DISTANCE = 8; // How many trail entries behind leader
    std::vector<PositionRecord> trail;
    int trail_head = 0;
    int trail_count = 0;
    float trail_step_accum = 0.0f;

    // NPCs
    std::vector<NPC> npcs;

    // Dialogue
    eb::DialogueBox dialogue;
    int pending_battle_npc = -1;

    // Battle
    BattleState battle;

    // Random encounter tracking
    float steps_since_encounter = 0.0f;
    std::mt19937 rng{std::random_device{}()};
};

// ─── Map generation ───
static void define_tileset_regions(eb::TextureAtlas& atlas) {
    atlas.add_region(126, 96,  63, 53);  atlas.add_region(189, 96,  64, 53);
    atlas.add_region(126, 149, 63, 53);  atlas.add_region(189, 149, 64, 53);
    atlas.add_region(275, 129, 50, 35);  atlas.add_region(325, 129, 48, 35);
    atlas.add_region(275, 164, 50, 38);  atlas.add_region(325, 164, 48, 38);
    atlas.add_region(570, 175, 50, 50);  atlas.add_region(720, 100, 50, 50);
    atlas.add_region(705, 175, 60, 60);  atlas.add_region(570, 100, 55, 55);
    atlas.add_region(830, 100, 55, 55);  atlas.add_region(570, 270, 55, 55);
    atlas.add_region(830, 270, 55, 55);  atlas.add_region(660, 155, 40, 40);
    atlas.add_region(140, 560, 50, 50);  atlas.add_region(200, 530, 50, 50);
    atlas.add_region(250, 510, 60, 50);  atlas.add_region(200, 575, 50, 40);
    atlas.add_region(392, 152, 60, 25);  atlas.add_region(392, 177, 60, 25);
    atlas.add_region(126, 303, 74, 43);  atlas.add_region(200, 303, 74, 43);
    atlas.add_region(126, 346, 74, 43);  atlas.add_region(200, 346, 74, 43);
}

static std::vector<int> generate_town_map(int width, int height) {
    std::vector<int> data(width * height, TILE_GRASS1);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            data[idx] = TILE_GRASS1 + ((x * 7 + y * 13) % 4);
            if (y == 9 || y == 10) data[idx] = TILE_ROAD_H;
            if (y == 8 || y == 11) data[idx] = TILE_SIDEWALK;
            if (x == 15 && y >= 3 && y < 9) data[idx] = TILE_ROAD_V;
            if (x == 15 && (y == 9 || y == 10)) data[idx] = TILE_ROAD_CROSS;
            if ((x == 14 || x == 16) && y >= 3 && y < 9) data[idx] = TILE_SIDEWALK;
            if (x >= 18 && x <= 22 && y >= 5 && y <= 7)
                data[idx] = TILE_DIRT1 + ((x + y) % 4);
            if (x >= 13 && x <= 17 && y >= 5 && y <= 7)
                data[idx] = TILE_STONE1 + ((x + y) % 4);
            if (x >= 22 && x <= 28 && y >= 14 && y <= 18) {
                float dx = x - 25.0f, dy = y - 16.0f;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < 2.5f) data[idx] = TILE_WATER_DEEP;
                else if (d < 3.5f) data[idx] = TILE_WATER_MID;
                else if (d < 4.0f) data[idx] = TILE_SAND;
            }
            if (y == 4 && x >= 5 && x <= 11) data[idx] = TILE_HEDGE1;
            if (y == 4 && x >= 19 && x <= 24) data[idx] = TILE_HEDGE2;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                data[idx] = TILE_WATER_MID;
        }
    }
    return data;
}

static std::vector<int> generate_town_collision(int width, int height) {
    std::vector<int> col(width * height, 0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                col[y * width + x] = 1;
            if (x >= 22 && x <= 28 && y >= 14 && y <= 18) {
                float dx = x - 25.0f, dy = y - 16.0f;
                if (std::sqrt(dx * dx + dy * dy) < 3.5f) col[y * width + x] = 1;
            }
            if (y == 4 && ((x >= 5 && x <= 11) || (x >= 19 && x <= 24)))
                col[y * width + x] = 1;
        }
    }
    return col;
}

static void setup_objects(GameState& game, eb::Texture* tileset_tex) {
    struct ObjSrc { eb::Vec2 sp, ss, rs; };
    ObjSrc srcs[] = {
        {{978, 496},  {103, 111}, {80, 86}},
        {{1099, 499}, {102, 114}, {80, 90}},
        {{1331, 501}, {82, 123},  {48, 72}},
        {{979, 622},  {61, 63},   {48, 50}},
        {{1056, 619}, {57, 64},   {44, 50}},
        {{1271, 617}, {44, 44},   {32, 32}},
        {{1019, 97},  {137, 100}, {128, 96}},
        {{1167, 97},  {137, 103}, {128, 96}},
    };
    float tw = static_cast<float>(tileset_tex->width());
    float th = static_cast<float>(tileset_tex->height());
    for (const auto& s : srcs) {
        game.object_defs.push_back({s.sp, s.ss, s.rs});
        eb::AtlasRegion r;
        r.pixel_x = (int)s.sp.x; r.pixel_y = (int)s.sp.y;
        r.pixel_w = (int)s.ss.x; r.pixel_h = (int)s.ss.y;
        r.uv_min = {s.sp.x / tw, s.sp.y / th};
        r.uv_max = {(s.sp.x + s.ss.x) / tw, (s.sp.y + s.ss.y) / th};
        game.object_regions.push_back(r);
    }
    auto place = [&](int id, float x, float y) {
        game.world_objects.push_back({id, {x * 32.0f, y * 32.0f}});
    };
    place(0, 3, 3); place(1, 8, 2); place(2, 27, 3);
    place(3, 5, 14); place(4, 3, 16); place(0, 10, 15);
    place(5, 7, 13); place(5, 12, 17); place(2, 2, 8);
    place(1, 28, 13);
    place(6, 6, 7.5f); place(7, 20, 7.5f);
}

static void setup_npcs(GameState& game) {
    // Sam is now a party member, not an NPC

    // Bobby — near the shop
    NPC bobby;
    bobby.name = "Bobby";
    bobby.position = {6.0f * 32.0f, 12.0f * 32.0f};
    bobby.dir = 0; // facing down
    bobby.dialogue = {
        {"Bobby", "You idjits better be prepared before heading out."},
        {"Bobby", "I've got some supplies if you need 'em."},
        {"Bobby", "Also, watch your back. Something ain't right in this town."},
    };
    game.npcs.push_back(bobby);

    // Suspicious stranger — has a battle
    NPC stranger;
    stranger.name = "???";
    stranger.position = {15.0f * 32.0f, 16.0f * 32.0f};
    stranger.dir = 1; // facing up
    stranger.dialogue = {
        {"???", "You shouldn't be poking around here, hunter."},
        {"???", "I can smell it on you... the stench of righteousness."},
    };
    stranger.has_battle = true;
    stranger.battle_enemy_name = "Shapeshifter";
    stranger.battle_enemy_hp = 45;
    stranger.battle_enemy_atk = 12;
    game.npcs.push_back(stranger);
}

// ─── Battle logic ───
static void start_battle(GameState& game, const std::string& enemy,
                          int hp, int atk, bool random) {
    auto& b = game.battle;
    b.phase = BattlePhase::Intro;
    b.enemy_name = enemy;
    b.enemy_hp_actual = hp;
    b.enemy_hp_max = hp;
    b.enemy_atk = atk;
    b.player_hp_actual = game.player_hp;
    b.player_hp_max = game.player_hp_max;
    b.player_hp_display = static_cast<float>(game.player_hp);
    b.player_atk = game.player_atk;
    b.player_def = game.player_def;
    b.menu_selection = 0;
    b.phase_timer = 0.0f;
    b.message = "A " + enemy + " appeared!";
    b.last_damage = 0;
    b.random_encounter = random;
}

static void update_battle(GameState& game, float dt,
                           bool confirm, bool up, bool down) {
    auto& b = game.battle;
    b.phase_timer += dt;

    // Roll HP display toward actual
    if (b.player_hp_display > b.player_hp_actual) {
        b.player_hp_display -= 40.0f * dt;
        if (b.player_hp_display < b.player_hp_actual)
            b.player_hp_display = static_cast<float>(b.player_hp_actual);
    }

    switch (b.phase) {
    case BattlePhase::Intro:
        if (b.phase_timer > 1.5f || confirm) {
            b.phase = BattlePhase::PlayerTurn;
            b.phase_timer = 0.0f;
            b.message = "";
        }
        break;

    case BattlePhase::PlayerTurn:
        if (up && b.menu_selection > 0) b.menu_selection--;
        if (down && b.menu_selection < 2) b.menu_selection++;
        if (confirm) {
            b.phase_timer = 0.0f;
            if (b.menu_selection == 0) {
                // Attack
                int damage = b.player_atk + (game.rng() % 5) - 2;
                if (damage < 1) damage = 1;
                b.enemy_hp_actual -= damage;
                b.last_damage = damage;
                b.message = "Dean attacks! " + std::to_string(damage) + " damage!";
                b.phase = BattlePhase::PlayerAttack;
            } else if (b.menu_selection == 1) {
                // Defend (heal a bit)
                int heal = 10 + game.rng() % 8;
                b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                b.message = "Dean braces! Recovered " + std::to_string(heal) + " HP.";
                b.phase = BattlePhase::PlayerAttack;
            } else {
                // Run (only from random encounters)
                if (b.random_encounter && (game.rng() % 3) != 0) {
                    b.message = "Got away safely!";
                    b.phase = BattlePhase::Victory;
                    b.phase_timer = 0.0f;
                } else {
                    b.message = "Can't escape!";
                    b.phase = BattlePhase::PlayerAttack;
                }
            }
        }
        break;

    case BattlePhase::PlayerAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.enemy_hp_actual <= 0) {
                b.phase = BattlePhase::Victory;
                int xp = b.enemy_hp_max / 2 + b.enemy_atk;
                b.message = "Victory! Gained " + std::to_string(xp) + " XP!";
                game.player_xp += xp;
            } else {
                b.phase = BattlePhase::EnemyTurn;
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::EnemyTurn: {
        int damage = b.enemy_atk + (game.rng() % 5) - 2 - b.player_def / 3;
        if (damage < 1) damage = 1;
        b.player_hp_actual -= damage;
        b.last_damage = damage;
        b.message = b.enemy_name + " attacks! " + std::to_string(damage) + " damage!";
        b.phase = BattlePhase::EnemyAttack;
        b.phase_timer = 0.0f;
        break;
    }

    case BattlePhase::EnemyAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.player_hp_actual <= 0) {
                b.player_hp_actual = 0;
                b.phase = BattlePhase::Defeat;
                b.message = "Dean is down!";
            } else {
                b.phase = BattlePhase::PlayerTurn;
                b.menu_selection = 0;
                b.message = "";
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::Victory:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = std::max(b.player_hp_actual, 1);
            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::Defeat:
        if (b.phase_timer > 2.0f || confirm) {
            // Revive with half HP
            game.player_hp = game.player_hp_max / 2;
            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::None:
        break;
    }
}

static void render_battle(GameState& game, eb::SpriteBatch& batch,
                           eb::TextRenderer& text,
                           VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                           float sw, float sh) {
    auto& b = game.battle;

    // Full-screen battle background
    batch.set_texture(white_desc);
    batch.draw_quad({0, 0}, {sw, sh}, {0, 0}, {1, 1}, {0.02f, 0.02f, 0.08f, 1.0f});

    // Enemy area (top half)
    float enemy_box_x = sw * 0.5f - 140.0f;
    float enemy_box_y = 40.0f;
    batch.draw_quad({enemy_box_x, enemy_box_y}, {280.0f, 140.0f},
                    {0, 0}, {1, 1}, {0.1f, 0.08f, 0.15f, 1.0f});

    // Enemy name and HP
    text.draw_text(batch, font_desc, b.enemy_name,
                   {enemy_box_x + 10, enemy_box_y + 10},
                   {1.0f, 0.4f, 0.4f, 1.0f}, 1.2f);

    // Enemy HP bar
    float hp_pct = std::max(0.0f, (float)b.enemy_hp_actual / b.enemy_hp_max);
    float bar_x = enemy_box_x + 10, bar_y = enemy_box_y + 45;
    float bar_w = 260.0f, bar_h = 16.0f;
    batch.set_texture(white_desc);
    batch.draw_quad({bar_x, bar_y}, {bar_w, bar_h}, {0,0}, {1,1}, {0.2f, 0.2f, 0.2f, 1.0f});
    eb::Vec4 hp_color = hp_pct > 0.5f ? eb::Vec4{0.2f, 0.8f, 0.2f, 1.0f}
                      : hp_pct > 0.25f ? eb::Vec4{0.9f, 0.7f, 0.1f, 1.0f}
                      : eb::Vec4{0.9f, 0.2f, 0.2f, 1.0f};
    batch.draw_quad({bar_x, bar_y}, {bar_w * hp_pct, bar_h}, {0,0}, {1,1}, hp_color);
    text.draw_text(batch, font_desc,
                   std::to_string(std::max(0, b.enemy_hp_actual)) + "/" + std::to_string(b.enemy_hp_max),
                   {bar_x + bar_w + 8, bar_y - 2}, {1,1,1,1}, 0.8f);

    // Player stats area (bottom right)
    float pbox_x = sw - 300.0f, pbox_y = sh - 200.0f;
    batch.set_texture(white_desc);
    batch.draw_quad({pbox_x, pbox_y}, {280.0f, 80.0f}, {0,0}, {1,1}, {0.08f, 0.08f, 0.18f, 0.9f});
    // Border
    batch.draw_quad({pbox_x, pbox_y}, {280.0f, 2.0f}, {0,0}, {1,1}, {0.5f, 0.5f, 0.8f, 1.0f});
    batch.draw_quad({pbox_x, pbox_y + 78.0f}, {280.0f, 2.0f}, {0,0}, {1,1}, {0.5f, 0.5f, 0.8f, 1.0f});

    text.draw_text(batch, font_desc, "Dean  Lv." + std::to_string(game.player_level),
                   {pbox_x + 10, pbox_y + 8}, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);

    // Rolling HP display
    float display_hp = b.player_hp_display;
    float php_pct = std::max(0.0f, display_hp / b.player_hp_max);
    float pb_x = pbox_x + 10, pb_y = pbox_y + 38;
    float pb_w = 200.0f;
    batch.set_texture(white_desc);
    batch.draw_quad({pb_x, pb_y}, {pb_w, 14.0f}, {0,0}, {1,1}, {0.2f, 0.2f, 0.2f, 1.0f});
    eb::Vec4 php_col = php_pct > 0.5f ? eb::Vec4{0.2f, 0.8f, 0.2f, 1.0f}
                     : php_pct > 0.25f ? eb::Vec4{0.9f, 0.7f, 0.1f, 1.0f}
                     : eb::Vec4{0.9f, 0.2f, 0.2f, 1.0f};
    batch.draw_quad({pb_x, pb_y}, {pb_w * php_pct, 14.0f}, {0,0}, {1,1}, php_col);

    // HP number (shows rolling display value)
    char hp_str[32];
    std::snprintf(hp_str, sizeof(hp_str), "HP %d/%d", (int)std::ceil(display_hp), b.player_hp_max);
    text.draw_text(batch, font_desc, hp_str,
                   {pb_x + pb_w + 8, pb_y - 2}, {1,1,1,1}, 0.8f);

    // Battle menu (player turn)
    if (b.phase == BattlePhase::PlayerTurn) {
        float menu_x = 30.0f, menu_y = sh - 200.0f;
        batch.set_texture(white_desc);
        batch.draw_quad({menu_x, menu_y}, {200.0f, 110.0f}, {0,0}, {1,1}, {0.08f, 0.05f, 0.15f, 0.95f});
        batch.draw_quad({menu_x, menu_y}, {200.0f, 2.0f}, {0,0}, {1,1}, {0.6f, 0.6f, 0.8f, 1.0f});
        batch.draw_quad({menu_x, menu_y + 108.0f}, {200.0f, 2.0f}, {0,0}, {1,1}, {0.6f, 0.6f, 0.8f, 1.0f});
        batch.draw_quad({menu_x, menu_y}, {2.0f, 110.0f}, {0,0}, {1,1}, {0.6f, 0.6f, 0.8f, 1.0f});
        batch.draw_quad({menu_x + 198.0f, menu_y}, {2.0f, 110.0f}, {0,0}, {1,1}, {0.6f, 0.6f, 0.8f, 1.0f});

        const char* options[] = {"Attack", "Defend", "Run"};
        for (int i = 0; i < 3; i++) {
            eb::Vec4 col = (i == b.menu_selection)
                ? eb::Vec4{1.0f, 1.0f, 0.3f, 1.0f}
                : eb::Vec4{0.8f, 0.8f, 0.8f, 1.0f};
            std::string prefix = (i == b.menu_selection) ? "> " : "  ";
            text.draw_text(batch, font_desc, prefix + options[i],
                           {menu_x + 12, menu_y + 12 + i * 32.0f}, col, 1.1f);
        }
    }

    // Message box (center bottom)
    if (!b.message.empty()) {
        float msg_w = 500.0f, msg_h = 50.0f;
        float msg_x = (sw - msg_w) * 0.5f;
        float msg_y = sh * 0.5f + 20.0f;
        batch.set_texture(white_desc);
        batch.draw_quad({msg_x, msg_y}, {msg_w, msg_h}, {0,0}, {1,1}, {0.05f, 0.05f, 0.12f, 0.9f});
        batch.draw_quad({msg_x, msg_y}, {msg_w, 2.0f}, {0,0}, {1,1}, {0.5f, 0.5f, 0.7f, 1.0f});
        text.draw_text(batch, font_desc, b.message,
                       {msg_x + 16, msg_y + 12}, {1,1,1,1}, 1.0f);
    }
}

// ─── Render HUD ───
static void render_hud(GameState& game, eb::SpriteBatch& batch,
                        eb::TextRenderer& text,
                        VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                        float sw, float /*sh*/) {
    // HUD panel in top-left
    float hud_x = 10.0f, hud_y = 10.0f;
    float hud_w = 220.0f, hud_h = 56.0f;
    batch.set_texture(white_desc);
    // Background
    batch.draw_quad({hud_x, hud_y}, {hud_w, hud_h}, {0,0}, {1,1}, {0.03f, 0.03f, 0.10f, 0.85f});
    // Border
    batch.draw_quad({hud_x, hud_y}, {hud_w, 2.0f}, {0,0}, {1,1}, {0.5f, 0.5f, 0.7f, 0.9f});
    batch.draw_quad({hud_x, hud_y + hud_h - 2}, {hud_w, 2.0f}, {0,0}, {1,1}, {0.5f, 0.5f, 0.7f, 0.9f});
    batch.draw_quad({hud_x, hud_y}, {2.0f, hud_h}, {0,0}, {1,1}, {0.5f, 0.5f, 0.7f, 0.9f});
    batch.draw_quad({hud_x + hud_w - 2, hud_y}, {2.0f, hud_h}, {0,0}, {1,1}, {0.5f, 0.5f, 0.7f, 0.9f});

    // Name and level
    char name_str[64];
    std::snprintf(name_str, sizeof(name_str), "Dean  Lv.%d", game.player_level);
    text.draw_text(batch, font_desc, name_str,
                   {hud_x + 10, hud_y + 8}, {1,1,1,1}, 0.9f);

    // HP bar
    float hp_pct = std::max(0.0f, (float)game.player_hp / game.player_hp_max);
    float bar_x = hud_x + 10, bar_y = hud_y + 34;
    float bar_w = 140.0f, bar_h = 12.0f;
    batch.set_texture(white_desc);
    batch.draw_quad({bar_x, bar_y}, {bar_w, bar_h}, {0,0}, {1,1}, {0.2f, 0.2f, 0.2f, 1.0f});
    eb::Vec4 hcol = hp_pct > 0.5f ? eb::Vec4{0.2f, 0.8f, 0.2f, 1.0f}
                  : hp_pct > 0.25f ? eb::Vec4{0.9f, 0.7f, 0.1f, 1.0f}
                  : eb::Vec4{0.9f, 0.2f, 0.2f, 1.0f};
    batch.draw_quad({bar_x, bar_y}, {bar_w * hp_pct, bar_h}, {0,0}, {1,1}, hcol);

    char hp_str[32];
    std::snprintf(hp_str, sizeof(hp_str), "%d/%d", game.player_hp, game.player_hp_max);
    text.draw_text(batch, font_desc, hp_str,
                   {bar_x + bar_w + 8, bar_y - 1}, {1,1,1,1}, 0.7f);
}

// ─── Draw an NPC (simple colored rectangle for now) ───
static void render_npc(const NPC& npc, eb::SpriteBatch& batch,
                        VkDescriptorSet white_desc) {
    batch.set_texture(white_desc);
    float npc_w = 28.0f, npc_h = 40.0f;
    eb::Vec2 dp = {npc.position.x - npc_w * 0.5f, npc.position.y - npc_h};

    // Body
    eb::Vec4 body_color = {0.3f, 0.5f, 0.8f, 1.0f};
    if (npc.has_battle) body_color = {0.7f, 0.2f, 0.2f, 1.0f};
    batch.draw_quad(dp, {npc_w, npc_h}, {0,0}, {1,1}, body_color);

    // Head
    float head_sz = 16.0f;
    eb::Vec2 head_pos = {npc.position.x - head_sz * 0.5f, npc.position.y - npc_h - head_sz * 0.6f};
    batch.draw_quad(head_pos, {head_sz, head_sz}, {0,0}, {1,1}, {0.85f, 0.7f, 0.55f, 1.0f});
}

// ─── Sprite lookup helper ───
static eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas,
                                             int dir, bool moving, int frame) {
    bool flip_h = (dir == 2); // left = flipped right
    const char* lookup_dir = flip_h ? "right" :
        (dir == 0 ? "down" : dir == 1 ? "up" : "right");

    const eb::AtlasRegion* sr_ptr = nullptr;
    char name[32];
    if (moving) {
        std::snprintf(name, sizeof(name), "walk_%s_%d", lookup_dir, frame);
        sr_ptr = atlas.find_region(name);
    } else {
        std::snprintf(name, sizeof(name), "idle_%s", lookup_dir);
        sr_ptr = atlas.find_region(name);
    }
    auto sr = sr_ptr ? *sr_ptr : atlas.region(0, 0);
    if (flip_h) std::swap(sr.uv_min.x, sr.uv_max.x);
    return sr;
}

// ─── Main ───
int main(int /*argc*/, char* /*argv*/[]) {
    try {
        eb::EngineConfig config;
        config.title = "Twilight - Supernatural RPG (Tab = Editor)";
        config.width = 960;
        config.height = 720;
        config.vsync = true;

        eb::Engine engine(config);
        engine.renderer().set_shader_dir("shaders/");
        engine.renderer().set_clear_color(0.05f, 0.05f, 0.12f);

        GameState game;

        // Text renderer
        eb::TextRenderer text_renderer(engine.renderer().vulkan_context(),
                                        "assets/fonts/default.ttf", 20.0f);
        VkDescriptorSet font_desc = engine.renderer().get_texture_descriptor(
            *text_renderer.texture());
        VkDescriptorSet white_desc = engine.renderer().default_texture_descriptor();

        // Load tileset
        auto* tileset_tex = engine.resources().load_texture("assets/textures/tileset.png");
        game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
        define_tileset_regions(*game.tileset_atlas);

        // Load Dean sprite sheet
        auto* dean_tex = engine.resources().load_texture("assets/textures/dean_sprites.png");
        game.dean_atlas = std::make_unique<eb::TextureAtlas>(dean_tex, 158, 210);
        game.dean_atlas->define_region("idle_down",  0*158, 0*210, 158, 210);
        game.dean_atlas->define_region("idle_up",    3*158, 2*210, 158, 210);
        game.dean_atlas->define_region("idle_right", 0*158, 3*210, 158, 210);
        game.dean_atlas->define_region("walk_down_0", 0*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_down_1", 2*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_up_0", 0*158, 1*210, 158, 210);
        game.dean_atlas->define_region("walk_up_1", 2*158, 1*210, 158, 210);
        game.dean_atlas->define_region("walk_right_0", 3*158, 0*210, 158, 210);
        game.dean_atlas->define_region("walk_right_1", 0*158, 2*210, 158, 210);

        // Load Sam sprite sheet (1840x1290, non-uniform cells)
        auto* sam_tex = engine.resources().load_texture("assets/textures/sam_sprites.png");
        game.sam_atlas = std::make_unique<eb::TextureAtlas>(sam_tex);
        // Padded regions (140x190) centered on each sprite to match Dean's proportions
        game.sam_atlas->define_region("idle_down",     44,  92, 140, 190);
        game.sam_atlas->define_region("walk_down_0",   44,  92, 140, 190);
        game.sam_atlas->define_region("walk_down_1",  504,  92, 140, 190);
        game.sam_atlas->define_region("idle_up",      734,  92, 140, 190);
        game.sam_atlas->define_region("walk_up_0",    734,  92, 140, 190);
        game.sam_atlas->define_region("walk_up_1",    275, 350, 140, 190);
        game.sam_atlas->define_region("idle_right",   504, 350, 140, 190);
        game.sam_atlas->define_region("walk_right_0", 504, 350, 140, 190);
        game.sam_atlas->define_region("walk_right_1", 965, 350, 140, 190);

        // Setup Sam as party member
        PartyMember sam;
        sam.name = "Sam";
        sam.position = {game.player_pos.x, game.player_pos.y + 32.0f};
        sam.dir = 0;
        game.party.push_back(sam);

        // Initialize breadcrumb trail
        game.trail.resize(GameState::TRAIL_SIZE);
        for (auto& r : game.trail) {
            r.pos = game.player_pos;
            r.dir = 0;
        }
        game.trail_head = 0;
        game.trail_count = 0;

        // Create map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        game.tile_map.set_tileset(game.tileset_atlas.get());
        game.tile_map.set_tileset_path("assets/textures/tileset.png");
        game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));
        game.tile_map.set_animated_tiles(TILE_WATER_DEEP, TILE_WATER_SHORE);

        // Objects & NPCs
        setup_objects(game, tileset_tex);
        setup_npcs(game);

        // Camera
        game.camera.set_viewport((float)config.width, (float)config.height);
        game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
        game.camera.set_follow_offset({0.0f, -config.height * 0.1f});
        game.camera.center_on(game.player_pos);

        // Descriptor sets
        VkDescriptorSet tileset_desc = engine.renderer().get_texture_descriptor(*tileset_tex);
        VkDescriptorSet dean_desc = engine.renderer().get_texture_descriptor(*dean_tex);
        VkDescriptorSet sam_desc = engine.renderer().get_texture_descriptor(*sam_tex);

        // Editor
        game.editor.set_map(&game.tile_map);
        game.editor.set_tileset(game.tileset_atlas.get(), tileset_desc);

#ifdef _WIN32
        mkdir("assets/maps");
#else
        mkdir("assets/maps", 0755);
#endif

        float game_time = 0.0f;

        // ─── Update ───
        engine.on_update = [&](float dt) {
            game_time += dt;
            auto& input = engine.platform().input();
            float sw = (float)engine.platform().get_width();
            float sh = (float)engine.platform().get_height();

            // Battle mode
            if (game.battle.phase != BattlePhase::None) {
                update_battle(game, dt,
                              input.is_pressed(eb::InputAction::Confirm),
                              input.is_pressed(eb::InputAction::MoveUp),
                              input.is_pressed(eb::InputAction::MoveDown));
                return;
            }

            // Dialogue mode
            if (game.dialogue.is_active()) {
                int result = game.dialogue.update(dt,
                    input.is_pressed(eb::InputAction::Confirm),
                    input.is_pressed(eb::InputAction::MoveUp),
                    input.is_pressed(eb::InputAction::MoveDown));
                if (result >= 0 && game.pending_battle_npc >= 0) {
                    auto& npc = game.npcs[game.pending_battle_npc];
                    start_battle(game, npc.battle_enemy_name,
                                 npc.battle_enemy_hp, npc.battle_enemy_atk, false);
                    game.pending_battle_npc = -1;
                }
                return;
            }

            // Tab toggles editor
            if (input.key_pressed(GLFW_KEY_TAB)) {
                game.editor.toggle();
                if (game.editor.is_active()) {
                    game.camera.clear_bounds();
                } else {
                    game.camera.set_bounds(0, 0,
                        game.tile_map.world_width(), game.tile_map.world_height());
                    game.camera.set_follow_offset({0.0f, -config.height * 0.1f});
                    game.camera.follow(game.player_pos, 100.0f);
                }
            }

            if (input.is_pressed(eb::InputAction::Menu) && !game.editor.is_active()) {
                engine.quit();
                return;
            }

            if (game.editor.is_active()) {
                game.editor.update(input, game.camera, dt, (int)sw, (int)sh);
                game.camera.update(dt);
            } else {
                // ─── Game mode ───
                eb::Vec2 move = {0.0f, 0.0f};
                if (input.is_held(eb::InputAction::MoveUp))    move.y -= 1.0f;
                if (input.is_held(eb::InputAction::MoveDown))  move.y += 1.0f;
                if (input.is_held(eb::InputAction::MoveLeft))  move.x -= 1.0f;
                if (input.is_held(eb::InputAction::MoveRight)) move.x += 1.0f;

                game.player_moving = (move.x != 0.0f || move.y != 0.0f);

                if (game.player_moving) {
                    float len = std::sqrt(move.x * move.x + move.y * move.y);
                    if (len > 0.0f) { move.x /= len; move.y /= len; }

                    float speed = game.player_speed;
                    if (input.is_held(eb::InputAction::Run)) speed *= 1.8f;

                    float pw = 20.0f, ph = 12.0f;
                    float ox = -pw * 0.5f, oy = -ph;

                    float new_x = game.player_pos.x + move.x * speed * dt;
                    bool bx = game.tile_map.is_solid_world(new_x + ox, game.player_pos.y + oy)
                           || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y + oy)
                           || game.tile_map.is_solid_world(new_x + ox, game.player_pos.y)
                           || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y);
                    if (!bx) game.player_pos.x = new_x;

                    float new_y = game.player_pos.y + move.y * speed * dt;
                    bool by = game.tile_map.is_solid_world(game.player_pos.x + ox, new_y + oy)
                           || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y + oy)
                           || game.tile_map.is_solid_world(game.player_pos.x + ox, new_y)
                           || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y);
                    if (!by) game.player_pos.y = new_y;

                    if (std::abs(move.x) > std::abs(move.y))
                        game.player_dir = (move.x < 0) ? 2 : 3;
                    else
                        game.player_dir = (move.y < 0) ? 1 : 0;

                    game.anim_timer += dt;
                    if (game.anim_timer >= 0.2f) {
                        game.anim_timer -= 0.2f;
                        game.player_frame = 1 - game.player_frame;
                    }

                    // Record breadcrumb trail for party followers
                    game.trail_step_accum += speed * dt;
                    const float TRAIL_STEP = 4.0f; // Record every 4 pixels of movement
                    while (game.trail_step_accum >= TRAIL_STEP) {
                        game.trail_step_accum -= TRAIL_STEP;
                        game.trail[game.trail_head] = {game.player_pos, game.player_dir};
                        game.trail_head = (game.trail_head + 1) % GameState::TRAIL_SIZE;
                        if (game.trail_count < GameState::TRAIL_SIZE)
                            game.trail_count++;
                    }

                    // Update party followers from trail (smooth lerp)
                    for (int pi = 0; pi < (int)game.party.size(); pi++) {
                        int delay = GameState::FOLLOW_DISTANCE * (pi + 1);
                        if (game.trail_count >= delay) {
                            int idx = (game.trail_head - delay + GameState::TRAIL_SIZE) % GameState::TRAIL_SIZE;
                            auto& pm = game.party[pi];
                            auto& target = game.trail[idx];
                            float dx = target.pos.x - pm.position.x;
                            float dy = target.pos.y - pm.position.y;
                            float d = std::sqrt(dx*dx + dy*dy);

                            pm.moving = (d > 2.0f);
                            if (pm.moving) {
                                // Lerp toward target at player speed
                                float lerp_speed = speed * 1.1f;
                                if (d <= lerp_speed * dt) {
                                    pm.position = target.pos;
                                } else {
                                    pm.position.x += (dx / d) * lerp_speed * dt;
                                    pm.position.y += (dy / d) * lerp_speed * dt;
                                }
                                // Direction from movement
                                if (std::abs(dx) > std::abs(dy))
                                    pm.dir = (dx < 0) ? 2 : 3;
                                else
                                    pm.dir = (dy < 0) ? 1 : 0;

                                pm.anim_timer += dt;
                                if (pm.anim_timer >= 0.2f) {
                                    pm.anim_timer -= 0.2f;
                                    pm.frame = 1 - pm.frame;
                                }
                            } else {
                                pm.dir = target.dir;
                                pm.frame = 0;
                                pm.anim_timer = 0.0f;
                            }
                        }
                    }

                    // Random encounter chance (grass tiles only)
                    game.steps_since_encounter += speed * dt;
                    if (game.steps_since_encounter > 200.0f) {
                        game.steps_since_encounter = 0.0f;
                        int tx = (int)(game.player_pos.x / 32.0f);
                        int ty = (int)(game.player_pos.y / 32.0f);
                        int tile = game.tile_map.tile_at(0, tx, ty);
                        if (tile >= TILE_GRASS1 && tile <= TILE_GRASS4) {
                            if ((game.rng() % 100) < 12) {
                                const char* enemies[] = {"Ghost", "Ghoul", "Demon", "Vengeful Spirit"};
                                int idx = game.rng() % 4;
                                int hp = 25 + game.rng() % 30;
                                int atk = 8 + game.rng() % 8;
                                start_battle(game, enemies[idx], hp, atk, true);
                            }
                        }
                    }
                } else {
                    game.player_frame = 0;
                    game.anim_timer = 0.0f;
                    // Stop party followers when player stops
                    for (auto& pm : game.party) {
                        pm.moving = false;
                        pm.frame = 0;
                        pm.anim_timer = 0.0f;
                    }
                }

                // NPC interaction (Z/Enter)
                if (input.is_pressed(eb::InputAction::Confirm)) {
                    for (int i = 0; i < (int)game.npcs.size(); i++) {
                        auto& npc = game.npcs[i];
                        float dx = game.player_pos.x - npc.position.x;
                        float dy = game.player_pos.y - npc.position.y;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist < npc.interact_radius) {
                            game.dialogue.start(npc.dialogue);
                            game.pending_battle_npc = npc.has_battle ? i : -1;
                            break;
                        }
                    }
                }

                game.camera.follow(game.player_pos, 4.0f);
                game.camera.update(dt);
            }
        };

        // ─── Render ───
        engine.on_render = [&]() {
            auto& batch = engine.renderer().sprite_batch();
            float sw = (float)engine.platform().get_width();
            float sh = (float)engine.platform().get_height();

            // Battle screen (full takeover)
            if (game.battle.phase != BattlePhase::None) {
                batch.set_projection(engine.renderer().screen_projection());
                render_battle(game, batch, text_renderer, font_desc, white_desc, sw, sh);
                return;
            }

            // ─── World rendering ───
            batch.set_projection(game.camera.projection_matrix());

            // Tile map
            batch.set_texture(tileset_desc);
            game.tile_map.render(batch, game.camera, game_time);

            // Y-sorted objects
            for (const auto& obj : game.world_objects) {
                const auto& def = game.object_defs[obj.sprite_id];
                const auto& region = game.object_regions[obj.sprite_id];
                eb::Vec2 draw_pos = {
                    obj.position.x - def.render_size.x * 0.5f,
                    obj.position.y - def.render_size.y
                };
                batch.draw_sorted(draw_pos, def.render_size,
                                 region.uv_min, region.uv_max,
                                 obj.position.y, tileset_desc);
            }

            // NPCs (Y-sorted with everything else)
            for (const auto& npc : game.npcs) {
                // Use draw_sorted with white_desc for NPC placeholder sprites
                float npc_w = 28.0f, npc_h = 40.0f;
                eb::Vec2 dp = {npc.position.x - npc_w * 0.5f, npc.position.y - npc_h};
                eb::Vec4 body_color = npc.has_battle
                    ? eb::Vec4{0.7f, 0.2f, 0.2f, 1.0f}
                    : eb::Vec4{0.3f, 0.5f, 0.8f, 1.0f};
                batch.draw_sorted(dp, {npc_w, npc_h}, {0,0}, {1,1},
                                 npc.position.y, white_desc, body_color);
                // Head
                float head_sz = 16.0f;
                eb::Vec2 hp = {npc.position.x - head_sz * 0.5f,
                               npc.position.y - npc_h - head_sz * 0.6f};
                batch.draw_sorted(hp, {head_sz, head_sz}, {0,0}, {1,1},
                                 npc.position.y - 0.1f, white_desc,
                                 {0.85f, 0.7f, 0.55f, 1.0f});
            }

            // Party members (Sam follows Dean)
            if (!game.editor.is_active()) {
                for (int pi = 0; pi < (int)game.party.size(); pi++) {
                    auto& pm = game.party[pi];
                    auto sr = get_character_sprite(*game.sam_atlas,
                                                   pm.dir, pm.moving, pm.frame);
                    float rw = 48.0f, rh = 64.0f;
                    float bob = pm.moving
                        ? std::sin(pm.anim_timer * 15.0f) * 2.0f : 0.0f;
                    eb::Vec2 dp = {pm.position.x - rw * 0.5f,
                                   pm.position.y - rh + 4.0f + bob};
                    batch.draw_sorted(dp, {rw, rh}, sr.uv_min, sr.uv_max,
                                     pm.position.y, sam_desc);
                }
            }

            // Player (Dean)
            if (!game.editor.is_active()) {
                auto sr = get_character_sprite(*game.dean_atlas,
                                               game.player_dir, game.player_moving,
                                               game.player_frame);
                float rw = 48.0f, rh = 64.0f;
                float bob = game.player_moving
                    ? std::sin(game.anim_timer * 15.0f) * 2.0f : 0.0f;
                eb::Vec2 dp = {game.player_pos.x - rw * 0.5f,
                               game.player_pos.y - rh + 4.0f + bob};
                batch.draw_sorted(dp, {rw, rh}, sr.uv_min, sr.uv_max,
                                 game.player_pos.y, dean_desc);
            }

            batch.flush_sorted();
            batch.flush();

            // ─── UI overlay (screen space) ───
            batch.set_projection(engine.renderer().screen_projection());

            // NPC name labels (screen space, drawn near NPCs)
            for (const auto& npc : game.npcs) {
                float dx = game.player_pos.x - npc.position.x;
                float dy = game.player_pos.y - npc.position.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < npc.interact_radius * 1.5f) {
                    // Convert world pos to screen pos
                    eb::Vec2 cam_off = game.camera.offset();
                    float sx = npc.position.x + cam_off.x;
                    float sy = npc.position.y + cam_off.y - 100.0f;

                    // Name label with dark background for readability
                    float name_scale = 1.0f;
                    auto name_size = text_renderer.measure_text(npc.name, name_scale);
                    float label_pad = 6.0f;
                    float label_x = sx - name_size.x * 0.5f - label_pad;
                    float label_y = sy - label_pad * 0.5f;
                    batch.set_texture(white_desc);
                    batch.draw_quad({label_x, label_y},
                        {name_size.x + label_pad * 2, name_size.y + label_pad},
                        {0,0}, {1,1}, {0.0f, 0.0f, 0.0f, 0.6f});
                    text_renderer.draw_text(batch, font_desc, npc.name,
                        {sx - name_size.x * 0.5f, sy},
                        {1.0f, 1.0f, 0.4f, 1.0f}, name_scale);

                    // Interaction hint
                    if (dist < npc.interact_radius) {
                        float hint_scale = 0.7f;
                        auto hint_size = text_renderer.measure_text("[Z] Talk", hint_scale);
                        float hint_y = sy + name_size.y + 8.0f;
                        batch.set_texture(white_desc);
                        batch.draw_quad(
                            {sx - hint_size.x * 0.5f - 4.0f, hint_y - 2.0f},
                            {hint_size.x + 8.0f, hint_size.y + 4.0f},
                            {0,0}, {1,1}, {0.0f, 0.0f, 0.0f, 0.5f});
                        text_renderer.draw_text(batch, font_desc, "[Z] Talk",
                            {sx - hint_size.x * 0.5f, hint_y},
                            {0.8f, 0.8f, 0.8f, 0.9f}, hint_scale);
                    }
                }
            }

            // HUD
            if (!game.editor.is_active()) {
                render_hud(game, batch, text_renderer, font_desc, white_desc, sw, sh);
            }

            // Dialogue box (on top of everything)
            if (game.dialogue.is_active()) {
                game.dialogue.render(batch, text_renderer, font_desc, white_desc, sw, sh);
            }

            // Editor overlay
            if (game.editor.is_active()) {
                batch.set_projection(game.camera.projection_matrix());
                batch.set_texture(engine.renderer().default_texture_descriptor());
                game.editor.render(batch, game.camera, tileset_desc, (int)sw, (int)sh);
            }
        };

        std::printf("[Main] Starting Twilight RPG Demo\n");
        std::printf("  WASD/Arrows - Move\n");
        std::printf("  Shift       - Run\n");
        std::printf("  Z/Enter     - Talk / Confirm\n");
        std::printf("  Tab         - Toggle editor\n");
        std::printf("  ESC         - Quit\n");
        engine.run();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Fatal] %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
