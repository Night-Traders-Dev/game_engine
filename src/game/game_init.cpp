#include "game/game.h"
#include "engine/resource/file_io.h"
#include "engine/scripting/script_engine.h"
#include "game/ai/pathfinding.h"
#include "game/systems/day_night.h"
#include "game/systems/survival.h"
#include "game/systems/spawn_system.h"
#include "game/systems/level_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <queue>

#if !defined(__ANDROID__) && !defined(EB_ANDROID)
#include <filesystem>
#endif

// ─── Tile hash for deterministic pseudo-random placement ───
static float tile_hash(int x, int y, int seed) {
    int h = (x * 374761393 + y * 668265263 + seed * 1274126177) ^ 0x5bf03635;
    h = ((h >> 13) ^ h) * 1274126177;
    return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// ─── Atlas region helpers ───

void define_tileset_regions(eb::TextureAtlas& atlas) {
    // All tile regions from New_Tileset.png (1536x1024)
    // ORDER MUST match the Tile enum exactly (1-indexed)

    // Grass (1-4)
    atlas.add_region(125, 95, 62, 53);    // 1: TILE_GRASS_PURE
    atlas.add_region(192, 95, 63, 53);    // 2: TILE_GRASS_LIGHT
    atlas.add_region(125, 152, 62, 53);   // 3: TILE_GRASS_FLOWERS
    atlas.add_region(192, 152, 63, 53);   // 4: TILE_GRASS_DARK
    // Dirt (5-8)
    atlas.add_region(278, 95, 45, 53);    // 5: TILE_DIRT_BROWN
    atlas.add_region(328, 95, 45, 53);    // 6: TILE_DIRT_DARK
    atlas.add_region(278, 152, 45, 53);   // 7: TILE_DIRT_MUD
    atlas.add_region(328, 152, 45, 53);   // 8: TILE_DIRT_GRAVEL
    // Grass edges & hedges (9-12)
    atlas.add_region(390, 95, 60, 53);    // 9: TILE_GRASS_EDGE1
    atlas.add_region(390, 152, 60, 53);   // 10: TILE_GRASS_EDGE2
    atlas.add_region(465, 95, 55, 53);    // 11: TILE_HEDGE1
    atlas.add_region(465, 152, 55, 53);   // 12: TILE_HEDGE2
    // Dirt paths (13-18)
    atlas.add_region(128, 222, 48, 65);   // 13: TILE_DIRT_PATH1
    atlas.add_region(180, 222, 48, 65);   // 14: TILE_DIRT_PATH2
    atlas.add_region(248, 222, 60, 65);   // 15: TILE_DIRT_MIXED1
    atlas.add_region(312, 222, 60, 65);   // 16: TILE_DIRT_MIXED2
    atlas.add_region(395, 222, 55, 65);   // 17: TILE_STONE_PATH11
    atlas.add_region(470, 222, 52, 65);   // 18: TILE_STONE_PATH12
    // Special ground (19-24)
    atlas.add_region(155, 320, 70, 70);   // 19: TILE_PENTAGRAM
    atlas.add_region(130, 310, 60, 40);   // 20: TILE_DARK_GROUND1
    atlas.add_region(300, 310, 60, 40);   // 21: TILE_DARK_GROUND2
    atlas.add_region(360, 310, 60, 40);   // 22: TILE_BLOOD_DIRT
    atlas.add_region(445, 310, 35, 35);   // 23: TILE_DARK_STONE1
    atlas.add_region(490, 310, 35, 35);   // 24: TILE_DARK_STONE2
    // Roads (25-36)
    atlas.add_region(620, 160, 60, 55);   // 25: TILE_ROAD_H
    atlas.add_region(700, 100, 55, 60);   // 26: TILE_ROAD_V
    atlas.add_region(690, 165, 70, 65);   // 27: TILE_ROAD_CROSS
    atlas.add_region(620, 100, 60, 55);   // 28: TILE_ROAD_TL
    atlas.add_region(780, 100, 60, 55);   // 29: TILE_ROAD_TR
    atlas.add_region(620, 230, 60, 55);   // 30: TILE_ROAD_BL
    atlas.add_region(780, 230, 60, 55);   // 31: TILE_ROAD_BR
    atlas.add_region(565, 290, 55, 50);   // 32: TILE_SIDEWALK
    atlas.add_region(625, 290, 55, 50);   // 33: TILE_SIDEWALK2
    atlas.add_region(750, 160, 55, 55);   // 34: TILE_ASPHALT
    atlas.add_region(670, 100, 55, 55);   // 35: TILE_ROAD_MARKING
    atlas.add_region(565, 235, 55, 50);   // 36: TILE_CURB
    // Road extras (37-41)
    atlas.add_region(560, 341, 100, 50);  // 37: TILE_ROAD_DIRT
    atlas.add_region(671, 341, 50, 50);   // 38: TILE_ROAD_BLOOD1
    atlas.add_region(730, 341, 65, 50);   // 39: TILE_ROAD_PATCH1
    atlas.add_region(806, 341, 65, 50);   // 40: TILE_ROAD_PATCH2
    atlas.add_region(882, 341, 90, 50);   // 41: TILE_ROAD_BLOOD2
    // Water & shore (42-50)
    atlas.add_region(130, 510, 55, 50);   // 42: TILE_WATER_DEEP
    atlas.add_region(195, 510, 55, 50);   // 43: TILE_WATER_MID
    atlas.add_region(260, 510, 50, 50);   // 44: TILE_WATER_SHORE_L_L
    atlas.add_region(320, 510, 50, 50);   // 45: TILE_WATER_SHORE_L_R
    atlas.add_region(380, 510, 55, 50);   // 46: TILE_SAND
    atlas.add_region(440, 510, 50, 50);   // 47: TILE_SAND_WET
    atlas.add_region(130, 565, 55, 50);   // 48: TILE_WATER_SHALLOW
    atlas.add_region(195, 565, 55, 50);   // 49: TILE_WATER_BLOOD
    atlas.add_region(380, 565, 55, 50);   // 50: TILE_SAND_DARK
    // Water objects as tiles (51-54)
    atlas.add_region(130, 640, 60, 30);   // 51: TILE_BENCH_WATER
    atlas.add_region(290, 680, 35, 30);   // 52: TILE_ROCK_WATER1
    atlas.add_region(340, 680, 40, 30);   // 53: TILE_ROCK_WATER2
    atlas.add_region(400, 680, 50, 40);   // 54: TILE_ROCK_WATER3

    // ── Object stamps (precise bounding boxes from sprite detection) ──
    // Buildings
    atlas.add_region(1007, 92, 157, 97);  // 55: Gas Mart
    atlas.add_region(1171, 97, 135, 92);  // 56: Salvage Repair
    atlas.add_region(1312, 86, 105, 103); // 57: Tall Building
    atlas.add_region(1107, 200, 142, 90); // 58: Motel
    atlas.add_region(1251, 201, 162, 89); // 59: House
    // Vehicles
    atlas.add_region(1109, 250, 140, 40); // 60: Dark Car / Impala
    atlas.add_region(1253, 250, 160, 40); // 61: Blue Car
    atlas.add_region(1086, 806, 202, 96); // 62: Impala (full side)
    // Trees (precise bboxes)
    atlas.add_region(590, 491, 85, 105);  // 63: Large Tree 1
    atlas.add_region(695, 497, 98, 107);  // 64: Large Tree 2
    atlas.add_region(819, 497, 86, 108);  // 65: Dead Tree 1
    atlas.add_region(916, 511, 63, 97);   // 66: Dead Tree 2
    atlas.add_region(990, 531, 56, 91);   // 67: Dead Tree 3
    atlas.add_region(582, 623, 69, 83);   // 68: Gnarly Tree 1
    atlas.add_region(673, 623, 68, 82);   // 69: Gnarly Tree 2
    atlas.add_region(750, 621, 75, 76);   // 70: Spooky Tree
    atlas.add_region(831, 630, 48, 45);   // 71: Bush 1
    atlas.add_region(892, 640, 43, 38);   // 72: Bush 2
    atlas.add_region(955, 642, 44, 40);   // 73: Bush 3
    atlas.add_region(1005, 652, 45, 55);  // 74: Dark Bush
    atlas.add_region(749, 718, 66, 66);   // 75: Night Tree
    atlas.add_region(575, 724, 158, 61);  // 76: Dark Hedge Row
    atlas.add_region(907, 734, 49, 45);   // 77: Rock/Moss 1
    atlas.add_region(842, 698, 33, 33);   // 78: Stump 1
    atlas.add_region(865, 743, 26, 25);   // 79: Stump 2
    // Misc Objects (precise bboxes)
    atlas.add_region(1083, 508, 26, 41);  // 80: Tombstone 1
    atlas.add_region(1122, 508, 26, 41);  // 81: Tombstone 2
    atlas.add_region(1251, 515, 40, 41);  // 82: Statue
    atlas.add_region(1167, 522, 25, 34);  // 83: Urn
    atlas.add_region(1208, 525, 27, 31);  // 84: Dark Urn
    atlas.add_region(1317, 509, 26, 48);  // 85: Lamp Post Small
    atlas.add_region(1362, 495, 28, 136); // 86: Lamp Post Tall
    atlas.add_region(1077, 569, 74, 46);  // 87: Chest/Crate
    atlas.add_region(1311, 577, 32, 54);  // 88: Pipe/Hydrant
    atlas.add_region(1248, 581, 40, 35);  // 89: Stone Block
    atlas.add_region(1169, 584, 60, 31);  // 90: Bench
    atlas.add_region(1080, 640, 104, 66); // 91: Brick Wall 1
    atlas.add_region(1202, 655, 89, 44);  // 92: Brick Wall 2
    atlas.add_region(1316, 653, 87, 57);  // 93: Stone Wall
    atlas.add_region(1081, 724, 82, 61);  // 94: Pentagram Circle
    atlas.add_region(1193, 726, 122, 62); // 95: Campfire
    atlas.add_region(1339, 724, 73, 66);  // 96: Well/Barrel Set
    atlas.add_region(1272, 809, 53, 26);  // 97: Barrel 1
    atlas.add_region(1347, 816, 59, 33);  // 98: Barrel 2
    atlas.add_region(1314, 867, 41, 43);  // 99: Can 1
    atlas.add_region(1369, 868, 48, 42);  // 100: Can 2
}

void define_object_stamps(GameState& game) {
    auto add = [&](const char* name, int region, float pw, float ph, const char* cat) {
        game.object_stamps.push_back({name, region, 0, 0, pw, ph, cat});
    };
    // Buildings (regions 54-58)
    add("Gas Mart",       54, 140, 86, "building");
    add("Salvage Repair", 55, 120, 82, "building");
    add("Tall Building",  56, 94,  92, "building");
    add("Motel",          57, 128, 80, "building");
    add("House",          58, 144, 80, "building");
    // Vehicles (regions 59-61)
    add("Impala",         59, 126, 36, "vehicle");
    add("Blue Car",       60, 144, 36, "vehicle");
    add("Impala (side)",  61, 180, 86, "vehicle");
    // Trees (regions 62-78)
    add("Large Tree 1",   62, 76,  94, "tree");
    add("Large Tree 2",   63, 88,  96, "tree");
    add("Dead Tree 1",    64, 76,  96, "tree");
    add("Dead Tree 2",    65, 56,  86, "tree");
    add("Dead Tree 3",    66, 50,  82, "tree");
    add("Gnarly Tree 1",  67, 62,  74, "tree");
    add("Gnarly Tree 2",  68, 60,  74, "tree");
    add("Spooky Tree",    69, 68,  68, "tree");
    add("Bush 1",         70, 43,  40, "tree");
    add("Bush 2",         71, 38,  34, "tree");
    add("Bush 3",         72, 40,  36, "tree");
    add("Dark Bush",      73, 40,  50, "tree");
    add("Night Tree",     74, 60,  60, "tree");
    add("Dark Hedge Row", 75, 142, 55, "tree");
    add("Rock/Moss",      76, 44,  40, "tree");
    add("Stump 1",        77, 30,  30, "tree");
    add("Stump 2",        78, 23,  22, "tree");
    // Misc (regions 79-99)
    add("Tombstone 1",    79, 23,  37, "misc");
    add("Tombstone 2",    80, 23,  37, "misc");
    add("Statue",         81, 36,  37, "misc");
    add("Urn",            82, 22,  30, "misc");
    add("Dark Urn",       83, 24,  28, "misc");
    add("Lamp Post Small",84, 23,  43, "misc");
    add("Lamp Post Tall", 85, 25,  122,"misc");
    add("Chest/Crate",    86, 66,  41, "misc");
    add("Pipe/Hydrant",   87, 28,  48, "misc");
    add("Stone Block",    88, 36,  31, "misc");
    add("Bench",          89, 54,  28, "misc");
    add("Brick Wall 1",   90, 94,  60, "misc");
    add("Brick Wall 2",   91, 80,  40, "misc");
    add("Stone Wall",     92, 78,  51, "misc");
    add("Pentagram",      93, 74,  55, "misc");
    add("Campfire",       94, 110, 56, "misc");
    add("Well/Barrels",   95, 66,  60, "misc");
    add("Barrel 1",       96, 48,  23, "misc");
    add("Barrel 2",       97, 53,  30, "misc");
    add("Can 1",          98, 37,  39, "misc");
    add("Can 2",          99, 43,  38, "misc");
}

void define_npc_atlas_regions(eb::TextureAtlas& atlas, int cw, int ch) {
    atlas.define_region("idle_down",     0,      0, cw, ch);
    atlas.define_region("walk_down_0",   cw,     0, cw, ch);
    atlas.define_region("walk_down_1",   cw * 2, 0, cw, ch);
    atlas.define_region("idle_up",       0,      ch, cw, ch);
    atlas.define_region("walk_up_0",     cw,     ch, cw, ch);
    atlas.define_region("walk_up_1",     cw * 2, ch, cw, ch);
    atlas.define_region("idle_right",    0,      ch * 2, cw, ch);
    atlas.define_region("walk_right_0",  cw,     ch * 2, cw, ch);
    atlas.define_region("walk_right_1",  cw * 2, ch * 2, cw, ch);
}

// ─── Map generation ───

std::vector<int> generate_town_map(int width, int height) {
    std::vector<int> data(width * height, TILE_GRASS_PURE);

    // Varied grass field
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            int v = ((x*3 + y*7) % 10);
            if (v < 5) data[y*width+x] = TILE_GRASS_PURE;
            else if (v < 8) data[y*width+x] = TILE_GRASS_LIGHT;
            else data[y*width+x] = TILE_GRASS_FLOWERS;
        }

    // Water border
    auto set = [&](int x, int y, int t) {
        if (x >= 0 && x < width && y >= 0 && y < height) data[y*width+x] = t;
    };
    for (int x = 0; x < width; x++) {
        set(x, 0, TILE_WATER_DEEP); set(x, height-1, TILE_WATER_DEEP);
        set(x, 1, TILE_WATER_MID);  set(x, height-2, TILE_SAND);
    }
    for (int y = 0; y < height; y++) {
        set(0, y, TILE_WATER_DEEP); set(width-1, y, TILE_WATER_DEEP);
        set(1, y, TILE_WATER_MID);  set(width-2, y, TILE_SAND);
    }

    return data;
}

std::vector<int> generate_town_collision(int width, int height) {
    std::vector<int> col(width * height, 0);
    // Water border only
    for (int x = 0; x < width; x++) {
        col[0*width+x] = 1; col[1*width+x] = 1;
        col[(height-1)*width+x] = 1; col[(height-2)*width+x] = 1;
    }
    for (int y = 0; y < height; y++) {
        col[y*width+0] = 1; col[y*width+1] = 1;
        col[y*width+width-1] = 1; col[y*width+width-2] = 1;
    }
    return col;
}

// ─── Object setup ───

void setup_objects(GameState& game, eb::Texture* tileset_tex) {
    struct ObjSrc { eb::Vec2 sp, ss, rs; };
    // Object sprites from New_Tileset.png (1536x1024)
    // Trees & Night section starts at ~(575, 491)
    ObjSrc srcs[] = {
        {{580, 500}, {75, 95},  {64, 80}},    // 0: Large tree 1
        {{660, 500}, {70, 95},  {64, 80}},    // 1: Large tree 2
        {{740, 510}, {50, 85},  {40, 68}},    // 2: Medium tree
        {{795, 530}, {40, 60},  {32, 48}},    // 3: Small tree
        {{845, 540}, {35, 50},  {28, 40}},    // 4: Bush/shrub
        {{900, 560}, {30, 30},  {24, 24}},    // 5: Small bush
        {{1007, 86}, {157, 103},{140, 96}},   // 6: Gas Mart building
        {{1171, 86}, {135, 103},{128, 96}},   // 7: Salvage Repair
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
    // No pre-placed objects — build the map in the editor
    (void)game;
    (void)tileset_tex;
}

// ─── NPC setup ───

void setup_npcs(GameState& game) {
    // Load dialogue from files
    DialogueScript bobby_dlg, stranger_dlg, vampire_dlg, azazel_dlg;
    load_dialogue_file(bobby_dlg, "assets/dialogue/bobby.dialogue");
    load_dialogue_file(stranger_dlg, "assets/dialogue/stranger.dialogue");
    load_dialogue_file(vampire_dlg, "assets/dialogue/vampire.dialogue");
    load_dialogue_file(azazel_dlg, "assets/dialogue/azazel.dialogue");

    NPC bobby;
    bobby.name = "Bobby"; bobby.position = {8.0f * 32.0f, 7.0f * 32.0f};
    bobby.home_pos = bobby.position; bobby.wander_target = bobby.position;
    bobby.dir = 0; bobby.sprite_atlas_id = 0; bobby.sprite_atlas_key = "assets/textures/bobby_sprites.png";
    bobby.move_speed = 30.0f; bobby.wander_interval = 4.0f;
    bobby.dialogue = bobby_dlg.get_lines("greeting");
    if (bobby.dialogue.empty()) bobby.dialogue = {{"Bobby", "Hey there."}};
    game.npcs.push_back(bobby);

    NPC stranger;
    stranger.name = "???"; stranger.position = {20.0f * 32.0f, 7.0f * 32.0f};
    stranger.home_pos = stranger.position; stranger.wander_target = stranger.position;
    stranger.dir = 1; stranger.sprite_atlas_id = 1; stranger.sprite_atlas_key = "assets/textures/stranger_sprites.png";
    stranger.move_speed = 25.0f; stranger.wander_interval = 5.0f;
    stranger.dialogue = stranger_dlg.get_lines("greeting");
    if (stranger.dialogue.empty()) stranger.dialogue = {{"???", "..."}};
    game.npcs.push_back(stranger);

    NPC vampire;
    vampire.name = "Vampire"; vampire.position = {19.0f * 32.0f, 14.0f * 32.0f};
    vampire.home_pos = vampire.position; vampire.wander_target = vampire.position;
    vampire.dir = 2; vampire.sprite_atlas_id = 2; vampire.sprite_atlas_key = "assets/textures/vampire_sprites.png";
    vampire.dialogue = vampire_dlg.get_lines("encounter");
    if (vampire.dialogue.empty()) vampire.dialogue = {{"Vampire", "..."}};

    vampire.has_battle = true;
    vampire.battle_enemy_name = "Vampire";
    vampire.battle_enemy_hp = 60; vampire.battle_enemy_atk = 15;
    vampire.hostile = true; vampire.aggro_range = 160.0f; vampire.attack_range = 36.0f;
    vampire.move_speed = 65.0f;
    game.npcs.push_back(vampire);

    NPC azazel;
    azazel.name = "Azazel"; azazel.position = {7.0f * 32.0f, 15.0f * 32.0f};
    azazel.home_pos = azazel.position; azazel.wander_target = azazel.position;
    azazel.dir = 0; azazel.sprite_atlas_id = 3; azazel.sprite_atlas_key = "assets/textures/yelloweyes_sprites.png";
    azazel.move_speed = 20.0f; azazel.wander_interval = 6.0f;
    azazel.dialogue = azazel_dlg.get_lines("greeting");
    if (azazel.dialogue.empty()) azazel.dialogue = {{"Azazel", "..."}};
    game.npcs.push_back(azazel);
}

// ─── Sprite lookup ───

eb::AtlasRegion get_character_sprite(eb::TextureAtlas& atlas, int dir, bool moving, int frame) {
    bool flip_h = (dir == 2);
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

// ─── Game initialization ───

bool init_game(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
               float viewport_w, float viewport_h) {
    if (game.initialized) return true;

    try {
        game.white_desc = renderer.default_texture_descriptor();

        // Helper: try to load a texture, return nullptr on failure
        auto try_load_tex = [&](const char* path) -> eb::Texture* {
            try { return resources.load_texture(path); }
            catch (...) { std::fprintf(stderr, "[Game] Texture not found: %s (skipping)\n", path); return nullptr; }
        };

        // Load tileset (try game-specific, no crash if missing)
        eb::Texture* tileset_tex = try_load_tex("assets/textures/new_tileset.png");
        if (tileset_tex) {
            game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
            define_tileset_regions(*game.tileset_atlas);
            game.tileset_desc = renderer.get_texture_descriptor(*tileset_tex);
        } else {
            game.tileset_desc = game.white_desc;
        }

        // Load player character sprites (optional)
        auto* dean_tex = try_load_tex("assets/textures/dean_sprites.png");
        if (dean_tex) {
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
            game.dean_desc = renderer.get_texture_descriptor(*dean_tex);
        } else {
            game.dean_desc = game.white_desc;
        }

        // Load party member sprites (optional)
        auto* sam_tex = try_load_tex("assets/textures/sam_sprites.png");
        if (sam_tex) {
            game.sam_atlas = std::make_unique<eb::TextureAtlas>(sam_tex);
            game.sam_atlas->define_region("idle_down",     44,  92, 140, 190);
            game.sam_atlas->define_region("walk_down_0",   44,  92, 140, 190);
            game.sam_atlas->define_region("walk_down_1",  504,  92, 140, 190);
            game.sam_atlas->define_region("idle_up",      734,  92, 140, 190);
            game.sam_atlas->define_region("walk_up_0",    734,  92, 140, 190);
            game.sam_atlas->define_region("walk_up_1",    275, 350, 140, 190);
            game.sam_atlas->define_region("idle_right",   504, 350, 140, 190);
            game.sam_atlas->define_region("walk_right_0", 504, 350, 140, 190);
            game.sam_atlas->define_region("walk_right_1", 965, 350, 140, 190);
            game.sam_desc = renderer.get_texture_descriptor(*sam_tex);
        } else {
            game.sam_desc = game.white_desc;
        }

        // Party member (uses character name from atlas or defaults)
        PartyMember follower;
        follower.name = "Follower";
        follower.position = {game.player_pos.x, game.player_pos.y + 32.0f};
        follower.dir = 0;
        game.party.push_back(follower);

        // Breadcrumb trail
        game.trail.resize(GameState::TRAIL_SIZE);
        for (auto& r : game.trail) { r.pos = game.player_pos; r.dir = 0; }
        game.trail_head = 0; game.trail_count = 0;

        // Create default map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        if (game.tileset_atlas) game.tile_map.set_tileset(game.tileset_atlas.get());
        game.tile_map.set_tileset_path("assets/textures/new_tileset.png");
        game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));
        game.tile_map.set_animated_tiles(TILE_WATER_DEEP, TILE_WATER_SHORE_L);
        auto_mark_reflective_tiles(game);

        // Objects & NPCs (only if tileset loaded)
        if (tileset_tex) {
            setup_objects(game, tileset_tex);
            define_object_stamps(game);
        }
        setup_npcs(game);

        // Load NPC sprite sheets into atlas cache (skip missing ones gracefully)
        auto load_npc_cached = [&](const char* path, int cw, int ch) -> std::string {
            if (game.atlas_cache.count(path)) return path; // Already cached
            auto* tex = try_load_tex(path);
            if (tex) {
                auto atlas = std::make_shared<eb::TextureAtlas>(tex);
                define_npc_atlas_regions(*atlas, cw, ch);
                game.atlas_cache[path] = atlas;
                game.atlas_descs[path] = renderer.get_texture_descriptor(*tex);
                // Also push to legacy vectors for backward compat
                game.npc_descs.push_back(game.atlas_descs[path]);
                game.npc_atlases.push_back(std::make_unique<eb::TextureAtlas>(tex));
                define_npc_atlas_regions(*game.npc_atlases.back(), cw, ch);
                return path;
            }
            return "";
        };
        std::string bobby_key    = load_npc_cached("assets/textures/bobby_sprites.png",      123, 174);
        std::string stranger_key = load_npc_cached("assets/textures/stranger_sprites.png",     70, 140);
        std::string vampire_key  = load_npc_cached("assets/textures/vampire_sprites.png",     136, 190);
        std::string azazel_key   = load_npc_cached("assets/textures/yelloweyes_sprites.png",  134, 187);

        // Dialogue box: background texture and character portraits (optional)
        auto* dialog_tex = try_load_tex("assets/textures/dialog_bg.png");
        if (dialog_tex) {
            game.dialogue.set_background(renderer.get_texture_descriptor(*dialog_tex));
        }
        auto load_portrait = [&](const char* path, const char* speaker) {
            auto* tex = try_load_tex(path);
            if (tex) game.dialogue.set_portrait(speaker, renderer.get_texture_descriptor(*tex));
        };
        load_portrait("assets/textures/portrait_dean.png", "Dean");
        load_portrait("assets/textures/portrait_sam.png", "Sam");
        load_portrait("assets/textures/portrait_bobby.png", "Bobby");

        // Camera
        game.camera.set_viewport(viewport_w, viewport_h);
        game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
        game.camera.set_follow_offset(eb::Vec2(0.0f, -viewport_h * 0.1f));
        game.camera.center_on(game.player_pos);

        game.initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Game] Init failed: %s\n", e.what());
        return false;
    }
}

// ─── Manifest-driven init ───

#include "engine/resource/game_manifest.h"

bool init_game_from_manifest(GameState& game, eb::Renderer& renderer, eb::ResourceManager& resources,
                              float viewport_w, float viewport_h, const eb::GameManifest& manifest) {
    if (game.initialized) return true;
    if (!manifest.loaded) return init_game(game, renderer, resources, viewport_w, viewport_h);

    try {
        game.white_desc = renderer.default_texture_descriptor();

        auto try_load = [&](const std::string& path) -> eb::Texture* {
            if (path.empty()) return nullptr;
            try { return resources.load_texture(path); }
            catch (...) { std::fprintf(stderr, "[Game] Texture not found: %s (skipping)\n", path.c_str()); return nullptr; }
        };

        // Load tileset from manifest
        eb::Texture* tileset_tex = try_load(manifest.tileset_path);
        if (tileset_tex) {
            game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
            // For non-standard tilesets, define regions as a simple grid
            int tw = tileset_tex->width(), th = tileset_tex->height();
            int tile_sz = 32;
            int cols = tw / tile_sz, rows = th / tile_sz;
            for (int r = 0; r < rows; r++)
                for (int c = 0; c < cols; c++)
                    game.tileset_atlas->add_region(c * tile_sz, r * tile_sz, tile_sz, tile_sz);
            game.tileset_desc = renderer.get_texture_descriptor(*tileset_tex);
            std::printf("[Game] Tileset: %s (%dx%d, %d tiles)\n", manifest.tileset_path.c_str(), tw, th, cols*rows);

            // Load object stamps from stamps file (if exists alongside tileset)
            std::string stamps_path = manifest.tileset_path;
            auto dot = stamps_path.rfind('.');
            if (dot != std::string::npos) stamps_path = stamps_path.substr(0, dot);
            stamps_path = stamps_path.substr(0, stamps_path.rfind('/') + 1);
            // Try cf_stamps.txt in same directory as tileset
            std::string stamps_dir = manifest.tileset_path.substr(0, manifest.tileset_path.rfind('/') + 1);
            auto stamps_data = eb::FileIO::read_file(stamps_dir + "cf_stamps.txt");
            if (stamps_data.empty()) stamps_data = eb::FileIO::read_file("assets/textures/cf_stamps.txt");
            if (!stamps_data.empty()) {
                std::string stamps_str(stamps_data.begin(), stamps_data.end());
                size_t pos = 0;
                while (pos < stamps_str.size()) {
                    size_t eol = stamps_str.find('\n', pos);
                    if (eol == std::string::npos) eol = stamps_str.size();
                    std::string line = stamps_str.substr(pos, eol - pos);
                    pos = eol + 1;
                    if (line.empty()) continue;
                    // Parse: name|px|py|w|h|category
                    std::vector<std::string> parts;
                    size_t p = 0;
                    while (p < line.size()) {
                        size_t d = line.find('|', p);
                        if (d == std::string::npos) d = line.size();
                        parts.push_back(line.substr(p, d - p));
                        p = d + 1;
                    }
                    if (parts.size() >= 6) {
                        int sx = std::stoi(parts[1]), sy = std::stoi(parts[2]);
                        int sw_s = std::stoi(parts[3]), sh_s = std::stoi(parts[4]);
                        // Add as atlas region
                        int region_idx = game.tileset_atlas->region_count();
                        game.tileset_atlas->add_region(sx, sy, sw_s, sh_s);
                        // Add as object stamp
                        ObjectDef def;
                        def.src_pos = {(float)sx, (float)sy};
                        def.src_size = {(float)sw_s, (float)sh_s};
                        def.render_size = {(float)sw_s, (float)sh_s};
                        game.object_defs.push_back(def);
                        eb::AtlasRegion ar = game.tileset_atlas->region(region_idx);
                        game.object_regions.push_back(ar);
                        game.object_stamps.push_back({parts[0], region_idx,
                            (float)sw_s, (float)sh_s, (float)sw_s, (float)sh_s, parts[5]});
                    }
                }
                std::printf("[Game] Loaded %d object stamps\n", (int)game.object_stamps.size());
            }

            // Auto-discover additional stamps files from the textures directory
            // (biome tilesets generated by generate_tileset.py)
#if !defined(__ANDROID__) && !defined(EB_ANDROID)
            {
                namespace fs = std::filesystem;
                std::string tex_dir = manifest.tileset_path.substr(0, manifest.tileset_path.rfind('/') + 1);
                if (!tex_dir.empty()) {
                    try {
                        for (auto& entry : fs::directory_iterator(tex_dir)) {
                            std::string fname = entry.path().filename().string();
                            // Skip the primary stamps file (already loaded)
                            if (fname == "cf_stamps.txt") continue;
                            // Load any *_stamps.txt files
                            if (fname.size() > 11 && fname.substr(fname.size() - 11) == "_stamps.txt") {
                                auto extra_data = eb::FileIO::read_file(entry.path().string());
                                if (extra_data.empty()) continue;
                                std::string biome_name = fname.substr(0, fname.size() - 11);
                                // Load the corresponding tileset for this stamps file
                                std::string biome_tileset_path = tex_dir + biome_name + "_tileset.png";
                                auto biome_tex_data = eb::FileIO::read_file(biome_tileset_path);
                                if (biome_tex_data.empty()) continue;
                                // We can't load separate tilesets into the same atlas, but we can register the stamps
                                // for discovery. They'll be usable when the player is in that biome's map.
                                int stamp_count = 0;
                                std::string s(extra_data.begin(), extra_data.end());
                                size_t sp = 0;
                                while (sp < s.size()) {
                                    size_t eol = s.find('\n', sp);
                                    if (eol == std::string::npos) eol = s.size();
                                    std::string line = s.substr(sp, eol - sp);
                                    sp = eol + 1;
                                    if (line.empty()) continue;
                                    // Just count for now — stamps are biome-specific
                                    stamp_count++;
                                }
                                std::printf("[Game] Discovered %s: %d stamps (biome: %s)\n",
                                    fname.c_str(), stamp_count, biome_name.c_str());
                            }
                        }
                    } catch (...) {
                        // Directory iteration may fail on some platforms
                    }
                }
            }
#endif // !__ANDROID__
        } else {
            game.tileset_desc = game.white_desc;
        }

        // Player sprite from manifest
        auto* player_tex = try_load(manifest.player.sprite_path);
        if (player_tex) {
            if (manifest.player.sprite_grid_w > 0 && manifest.player.sprite_grid_h > 0) {
                game.dean_atlas = std::make_unique<eb::TextureAtlas>(player_tex,
                    manifest.player.sprite_grid_w, manifest.player.sprite_grid_h);
            } else {
                game.dean_atlas = std::make_unique<eb::TextureAtlas>(player_tex);
            }
            // Define regions from manifest custom regions
            for (auto& reg : manifest.player.custom_regions)
                game.dean_atlas->define_region(reg.name, reg.x, reg.y, reg.w, reg.h);
            // If no custom regions and grid-based, define standard walk cycle
            if (manifest.player.custom_regions.empty() && manifest.player.sprite_grid_w > 0) {
                int cw = manifest.player.sprite_grid_w, ch = manifest.player.sprite_grid_h;
                // 3x3 grid: row 0=down, row 1=up, row 2=right
                game.dean_atlas->define_region("idle_down",     0,      0, cw, ch);
                game.dean_atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                game.dean_atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                game.dean_atlas->define_region("idle_up",       0,     ch, cw, ch);
                game.dean_atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                game.dean_atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                game.dean_atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                game.dean_atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                game.dean_atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
            }
            game.dean_desc = renderer.get_texture_descriptor(*player_tex);
        } else {
            game.dean_desc = game.white_desc;
        }

        // Player stats from manifest
        game.player_hp = manifest.player.hp; game.player_hp_max = manifest.player.hp_max;
        game.player_atk = manifest.player.atk; game.player_def = manifest.player.def;
        game.player_level = manifest.player.level; game.player_xp = manifest.player.xp;
        game.player_pos = {manifest.player.start_x, manifest.player.start_y};

        // Party members from manifest
        if (!manifest.party.empty()) {
            auto& pm_def = manifest.party[0];
            auto* pm_tex = try_load(pm_def.sprite_path);
            if (pm_tex) {
                if (pm_def.sprite_grid_w > 0 && pm_def.sprite_grid_h > 0) {
                    game.sam_atlas = std::make_unique<eb::TextureAtlas>(pm_tex,
                        pm_def.sprite_grid_w, pm_def.sprite_grid_h);
                } else {
                    game.sam_atlas = std::make_unique<eb::TextureAtlas>(pm_tex);
                }
                for (auto& reg : pm_def.custom_regions)
                    game.sam_atlas->define_region(reg.name, reg.x, reg.y, reg.w, reg.h);
                if (pm_def.custom_regions.empty() && pm_def.sprite_grid_w > 0) {
                    int cw = pm_def.sprite_grid_w, ch = pm_def.sprite_grid_h;
                    game.sam_atlas->define_region("idle_down",     0,      0, cw, ch);
                    game.sam_atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                    game.sam_atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                    game.sam_atlas->define_region("idle_up",       0,     ch, cw, ch);
                    game.sam_atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                    game.sam_atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                    game.sam_atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                    game.sam_atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                    game.sam_atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                }
                game.sam_desc = renderer.get_texture_descriptor(*pm_tex);
            } else {
                game.sam_desc = game.white_desc;
            }
            game.sam_hp = pm_def.hp; game.sam_hp_max = pm_def.hp_max; game.sam_atk = pm_def.atk;

            PartyMember follower;
            follower.name = pm_def.name;
            follower.position = {game.player_pos.x, game.player_pos.y + 32.0f};
            follower.dir = 0;
            game.party.push_back(follower);
        }

        // Breadcrumb trail
        game.trail.resize(GameState::TRAIL_SIZE);
        for (auto& r : game.trail) { r.pos = game.player_pos; r.dir = 0; }
        game.trail_head = 0; game.trail_count = 0;

        // Create default map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        if (game.tileset_atlas) {
            game.tile_map.set_tileset(game.tileset_atlas.get());
            game.tile_map.set_tileset_path(manifest.tileset_path);
        }
        // Generate a simple grass field map using first tile
        std::vector<int> ground(MAP_W * MAP_H, 1);
        // Water border
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++)
                if (x == 0 || x == MAP_W-1 || y == 0 || y == MAP_H-1) ground[y*MAP_W+x] = 0;
        game.tile_map.add_layer("ground", ground);
        // Collision: border is solid
        std::vector<int> col(MAP_W * MAP_H, 0);
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++)
                if (x == 0 || x == MAP_W-1 || y == 0 || y == MAP_H-1) col[y*MAP_W+x] = 1;
        game.tile_map.set_collision(col);
        auto_mark_reflective_tiles(game);

        // NPCs from manifest
        for (auto& npc_def : manifest.npcs) {
            NPC npc;
            npc.name = npc_def.name;
            npc.position = {npc_def.x, npc_def.y};
            npc.home_pos = npc.position;
            npc.wander_target = npc.position;
            npc.dir = npc_def.dir;
            npc.interact_radius = npc_def.interact_radius;
            npc.hostile = npc_def.hostile;
            npc.aggro_range = npc_def.aggro_range;
            npc.attack_range = npc_def.attack_range;
            npc.move_speed = npc_def.move_speed;
            npc.wander_interval = npc_def.wander_interval;
            npc.has_battle = npc_def.has_battle;
            npc.battle_enemy_name = npc_def.battle_enemy;
            npc.battle_enemy_hp = npc_def.battle_hp;
            npc.battle_enemy_atk = npc_def.battle_atk;

            // Load NPC sprite into atlas cache with grid-size-aware key
            if (!npc_def.sprite_path.empty()) {
                npc.sprite_atlas_key = npc_def.sprite_path;
                int cw = npc_def.sprite_grid_w > 0 ? npc_def.sprite_grid_w : 32;
                int ch = npc_def.sprite_grid_h > 0 ? npc_def.sprite_grid_h : 32;
                npc.sprite_grid_w = cw;
                npc.sprite_grid_h = ch;
                // Build grid-aware cache key
                std::string cache_key = npc_def.sprite_path;
                char kbuf[32]; std::snprintf(kbuf, sizeof(kbuf), "@%dx%d", cw, ch);
                cache_key += kbuf;
                if (!game.atlas_cache.count(cache_key)) {
                    auto* npc_tex = try_load(npc_def.sprite_path);
                    if (npc_tex) {
                        auto atlas = std::make_shared<eb::TextureAtlas>(npc_tex);
                        define_npc_atlas_regions(*atlas, cw, ch);
                        game.atlas_cache[cache_key] = atlas;
                        game.atlas_descs[cache_key] = renderer.get_texture_descriptor(*npc_tex);
                        // Also push to legacy vectors
                        npc.sprite_atlas_id = (int)game.npc_atlases.size();
                        game.npc_descs.push_back(game.atlas_descs[cache_key]);
                        game.npc_atlases.push_back(std::make_unique<eb::TextureAtlas>(npc_tex));
                        define_npc_atlas_regions(*game.npc_atlases.back(), cw, ch);
                    }
                } else {
                    // Already cached — just set the legacy ID for backward compat
                    npc.sprite_atlas_id = -1; // Will use key-based lookup
                }
            }

            // Load dialogue
            if (!npc_def.dialogue_file.empty()) {
                DialogueScript dlg;
                if (load_dialogue_file(dlg, npc_def.dialogue_file)) {
                    auto lines = dlg.get_lines("greeting");
                    if (!lines.empty()) npc.dialogue = lines;
                }
            }
            if (npc.dialogue.empty()) npc.dialogue = {{npc.name, "..."}};

            game.npcs.push_back(npc);
        }

        // Load dialogue background and portraits from manifest
        if (!manifest.dialog_bg_path.empty()) {
            auto* dtex = try_load(manifest.dialog_bg_path);
            if (dtex) game.dialogue.set_background(renderer.get_texture_descriptor(*dtex));
        }
        // Player portrait
        if (!manifest.player.portrait_path.empty()) {
            auto* ptex = try_load(manifest.player.portrait_path);
            if (ptex) game.dialogue.set_portrait(manifest.player.name, renderer.get_texture_descriptor(*ptex));
        }
        // Party portraits
        for (auto& pm : manifest.party) {
            if (!pm.portrait_path.empty()) {
                auto* ptex = try_load(pm.portrait_path);
                if (ptex) game.dialogue.set_portrait(pm.name, renderer.get_texture_descriptor(*ptex));
            }
        }
        // NPC portraits
        for (auto& npc : manifest.npcs) {
            if (!npc.portrait_path.empty()) {
                auto* ptex = try_load(npc.portrait_path);
                if (ptex) game.dialogue.set_portrait(npc.name, renderer.get_texture_descriptor(*ptex));
            }
        }

        // UI sprite sheet (for merchant store, menus, etc.)
        auto* ui_tex = try_load("assets/textures/ui_spritesheet.png");
        if (ui_tex) {
            game.ui_atlas = std::make_unique<eb::TextureAtlas>(ui_tex);
            eb::define_ui_atlas_regions(*game.ui_atlas);
            game.ui_desc = renderer.get_texture_descriptor(*ui_tex);
            std::printf("[Game] UI sprite sheet loaded (%dx%d)\n", ui_tex->width(), ui_tex->height());
        }

        // Icons sprite sheet
        auto* icons_tex = try_load("assets/textures/demo_srw_free_icons1.png");
        if (icons_tex) {
            game.icons_atlas = std::make_unique<eb::TextureAtlas>(icons_tex);
            eb::define_icons_atlas_regions(*game.icons_atlas);
            game.icons_desc = renderer.get_texture_descriptor(*icons_tex);
            std::printf("[Game] Icons sprite sheet loaded\n");
        }

        // Fantasy icons atlas (32x32 grid of ~300 RPG icons)
        auto* fantasy_tex = try_load("assets/textures/fantasy_icons.png");
        if (fantasy_tex) {
            game.fantasy_icons_atlas = std::make_unique<eb::TextureAtlas>(fantasy_tex);
            // Define as 32x32 grid
            int fi_cols = fantasy_tex->width() / 32;
            int fi_rows = fantasy_tex->height() / 32;
            for (int r = 0; r < fi_rows; r++)
                for (int c = 0; c < fi_cols; c++)
                    game.fantasy_icons_atlas->add_region(c * 32, r * 32, 32, 32);
            game.fantasy_icons_desc = renderer.get_texture_descriptor(*fantasy_tex);
            std::printf("[Game] Fantasy icons loaded (%dx%d, %d icons)\n",
                       fantasy_tex->width(), fantasy_tex->height(), fi_cols * fi_rows);
        }

        // Flat UI pack — define named regions for panels/bars/buttons
        auto* ui_flat_tex = try_load("assets/textures/ui_flat_pack.png");
        if (ui_flat_tex) {
            game.ui_flat_atlas = std::make_unique<eb::TextureAtlas>(ui_flat_tex);
            // Large panels (96x64 each, starting at y=32)
            game.ui_flat_atlas->define_region("flat_grey",    32, 32, 96, 64);
            game.ui_flat_atlas->define_region("flat_blue",   128, 32, 96, 64);
            game.ui_flat_atlas->define_region("flat_orange", 224, 32, 96, 64);
            game.ui_flat_atlas->define_region("flat_cream",  320, 32, 64, 64);
            // Smaller panels (various sizes)
            game.ui_flat_atlas->define_region("flat_grey_sm",    32, 96, 64, 32);
            game.ui_flat_atlas->define_region("flat_blue_sm",   128, 96, 64, 32);
            game.ui_flat_atlas->define_region("flat_orange_sm", 224, 96, 64, 32);
            // Dark panels (at y=96-192, right side)
            game.ui_flat_atlas->define_region("flat_dark",  288, 96, 64, 64);
            game.ui_flat_atlas->define_region("flat_dark_tall", 288, 96, 64, 128);
            game.ui_flat_desc = renderer.get_texture_descriptor(*ui_flat_tex);
            std::printf("[Game] Flat UI pack loaded (%dx%d)\n", ui_flat_tex->width(), ui_flat_tex->height());
        }

        // Camera
        game.camera.set_viewport(viewport_w, viewport_h);
        game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
        game.camera.set_follow_offset(eb::Vec2(0.0f, -viewport_h * 0.1f));
        game.camera.center_on(game.player_pos);

        // Initialize level manager
        game.level_manager = std::make_unique<eb::LevelManager>();

        game.initialized = true;
        std::printf("[Game] Initialized from manifest: %s\n", manifest.title.c_str());
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Game] Manifest init failed: %s\n", e.what());
        return false;
    }
}

