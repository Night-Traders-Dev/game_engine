#include "game/game.h"
#include "engine/resource/file_io.h"
#include <fstream>
#include <sstream>

// ─── Map file save/load ───

static std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

bool save_map_file(const GameState& game, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    const auto& map = game.tile_map;
    int w = map.width(), h = map.height();

    f << "{\n";
    f << "  \"format\": \"twilight_map\",\n";
    f << "  \"version\": 2,\n";

    // ── Metadata ──
    f << "  \"metadata\": {\n";
    f << "    \"name\": \"untitled\",\n";
    f << "    \"width\": " << w << ",\n";
    f << "    \"height\": " << h << ",\n";
    f << "    \"tile_size\": " << map.tile_size() << ",\n";
    f << "    \"tileset\": \"" << esc(map.tileset_path()) << "\",\n";
    f << "    \"player_start_x\": " << game.player_pos.x << ",\n";
    f << "    \"player_start_y\": " << game.player_pos.y << "\n";
    f << "  },\n";

    // ── Tile layers ──
    f << "  \"layers\": [\n";
    for (int li = 0; li < map.layer_count(); li++) {
        const auto& layer = map.layers()[li];
        f << "    {\"name\": \"" << esc(layer.name) << "\", \"data\": [";
        for (int i = 0; i < (int)layer.data.size(); i++) {
            if (i > 0) f << ",";
            if (i % w == 0) f << "\n      ";
            f << layer.data[i];
        }
        f << "\n    ]}";
        if (li + 1 < map.layer_count()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // ── Collision ──
    f << "  \"collision\": [";
    const auto& col = map.collision_types();
    for (int i = 0; i < (int)col.size(); i++) {
        if (i > 0) f << ",";
        if (i % w == 0) f << "\n    ";
        f << (int)col[i];
    }
    f << "\n  ],\n";

    // ── Portals ──
    f << "  \"portals\": [";
    for (int pi = 0; pi < (int)map.portals().size(); pi++) {
        auto& p = map.portals()[pi];
        if (pi > 0) f << ",";
        f << "\n    {\"x\":" << p.tile_x << ",\"y\":" << p.tile_y
          << ",\"target_map\":\"" << esc(p.target_map) << "\""
          << ",\"target_x\":" << p.target_x << ",\"target_y\":" << p.target_y
          << ",\"label\":\"" << esc(p.label) << "\"}";
    }
    f << "\n  ],\n";

    // ── World objects ──
    f << "  \"objects\": [\n";
    for (int oi = 0; oi < (int)game.world_objects.size(); oi++) {
        auto& obj = game.world_objects[oi];
        if (oi > 0) f << ",\n";
        // Store the atlas region info so we can reconstruct
        if (obj.sprite_id < (int)game.object_regions.size()) {
            auto& r = game.object_regions[obj.sprite_id];
            auto& d = game.object_defs[obj.sprite_id];
            f << "    {\"x\":" << obj.position.x << ",\"y\":" << obj.position.y
              << ",\"src_x\":" << r.pixel_x << ",\"src_y\":" << r.pixel_y
              << ",\"src_w\":" << r.pixel_w << ",\"src_h\":" << r.pixel_h
              << ",\"render_w\":" << d.render_size.x << ",\"render_h\":" << d.render_size.y
              << "}";
        }
    }
    f << "\n  ],\n";

    // ── NPCs ──
    f << "  \"npcs\": [\n";
    for (int ni = 0; ni < (int)game.npcs.size(); ni++) {
        auto& npc = game.npcs[ni];
        if (ni > 0) f << ",\n";
        f << "    {\"name\":\"" << esc(npc.name) << "\""
          << ",\"x\":" << npc.position.x << ",\"y\":" << npc.position.y
          << ",\"dir\":" << npc.dir
          << ",\"sprite_atlas_id\":" << npc.sprite_atlas_id
          << ",\"interact_radius\":" << npc.interact_radius
          << ",\"hostile\":" << (npc.hostile ? "true" : "false")
          << ",\"aggro_range\":" << npc.aggro_range
          << ",\"attack_range\":" << npc.attack_range
          << ",\"move_speed\":" << npc.move_speed
          << ",\"wander_interval\":" << npc.wander_interval
          << ",\"has_battle\":" << (npc.has_battle ? "true" : "false");
        if (npc.has_battle) {
            f << ",\"battle_enemy\":\"" << esc(npc.battle_enemy_name) << "\""
              << ",\"battle_hp\":" << npc.battle_enemy_hp
              << ",\"battle_atk\":" << npc.battle_enemy_atk;
        }
        // Dialogue lines
        f << ",\"dialogue\":[";
        for (int di = 0; di < (int)npc.dialogue.size(); di++) {
            if (di > 0) f << ",";
            f << "{\"speaker\":\"" << esc(npc.dialogue[di].speaker) << "\""
              << ",\"text\":\"" << esc(npc.dialogue[di].text) << "\"}";
        }
        f << "]}";
    }
    f << "\n  ]\n";

    f << "}\n";

    std::printf("[Map] Saved: %s (%dx%d, %d objects, %d npcs)\n",
                path.c_str(), w, h, (int)game.world_objects.size(), (int)game.npcs.size());
    return true;
}

bool load_map_file(GameState& game, eb::Renderer& renderer, const std::string& path) {
    // For now, use the existing TileMap loader for tile data,
    // then parse the extended fields manually.
    // Full JSON parsing is complex — use the existing minimal parser approach.

    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[Map] Failed to load: %s\n", path.c_str());
        return false;
    }

    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Check format version
    if (json.find("\"twilight_map\"") != std::string::npos) {
        // New format v2 — load via TileMap first for tiles/collision/portals
        game.tile_map.load_json(path);

        // Parse objects and NPCs from the extended fields
        // TODO: Full v2 parser for objects/NPCs
        // For now, the tile data is the most important part
        std::printf("[Map] Loaded v2 map: %s\n", path.c_str());
        return true;
    } else {
        // Legacy format v1
        return game.tile_map.load_json(path);
    }
}

// ─── Dialogue file I/O ───

const DialogueFunction* DialogueScript::get(const std::string& name) const {
    for (auto& f : functions) if (f.name == name) return &f;
    return nullptr;
}

std::vector<eb::DialogueLine> DialogueScript::get_lines(const std::string& name) const {
    auto* f = get(name);
    return f ? f->lines : std::vector<eb::DialogueLine>{};
}

bool load_dialogue_file(DialogueScript& script, const std::string& path) {
    // Use FileIO for cross-platform reading (works with Android AAssetManager)
    auto data = eb::FileIO::read_file(path);
    if (data.empty()) {
        std::fprintf(stderr, "[Dialogue] Failed to load: %s\n", path.c_str());
        return false;
    }

    script.filename = path;
    script.functions.clear();
    DialogueFunction* current = nullptr;

    // Parse line by line from the byte buffer
    std::string content(data.begin(), data.end());
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#') continue;

        // Function declaration
        if (line[0] == '@') {
            script.functions.push_back({});
            current = &script.functions.back();
            current->name = line.substr(1);
            // Trim trailing whitespace from name
            size_t end = current->name.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) current->name = current->name.substr(0, end + 1);
            continue;
        }

        // Dialogue line: Speaker: Text
        if (current) {
            size_t colon = line.find(':');
            if (colon != std::string::npos && colon > 0) {
                std::string speaker = line.substr(0, colon);
                std::string text = line.substr(colon + 1);
                // Trim
                size_t ts = text.find_first_not_of(" \t");
                if (ts != std::string::npos) text = text.substr(ts);
                size_t se = speaker.find_last_not_of(" \t");
                if (se != std::string::npos) speaker = speaker.substr(0, se + 1);
                current->lines.push_back({speaker, text});
            }
        }
    }

    std::printf("[Dialogue] Loaded %s: %d functions\n", path.c_str(), (int)script.functions.size());
    return true;
}

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

    // ── Object stamps (not tiles, but registered for palette display) ──
    // Buildings
    atlas.add_region(1007, 86, 157, 103); // 55: Gas Mart
    atlas.add_region(1171, 86, 135, 103); // 56: Salvage Repair
    atlas.add_region(1312, 86, 105, 103); // 57: Tall Building
    atlas.add_region(1003, 200, 92, 100); // 58: Motel/House
    atlas.add_region(1003, 310, 92, 40);  // 59: Dead End Sign
    // Vehicles
    atlas.add_region(1110, 250, 90, 50);  // 60: Impala (parked)
    atlas.add_region(1210, 250, 80, 50);  // 61: Red Car
    atlas.add_region(1300, 250, 80, 50);  // 62: Blue Car
    atlas.add_region(1090, 810, 200, 70); // 63: Impala (full side)
    // Trees
    atlas.add_region(580, 495, 72, 90);   // 64: Large Tree 1
    atlas.add_region(660, 495, 65, 90);   // 65: Large Tree 2
    atlas.add_region(735, 500, 50, 85);   // 66: Medium Tree
    atlas.add_region(800, 495, 55, 95);   // 67: Dead Tree 1
    atlas.add_region(860, 495, 60, 95);   // 68: Dead Tree 2
    atlas.add_region(930, 498, 45, 92);   // 69: Dead Tree 3
    atlas.add_region(580, 600, 60, 45);   // 70: Bush Large
    atlas.add_region(650, 610, 35, 35);   // 71: Bush Small
    atlas.add_region(700, 610, 35, 35);   // 72: Bush Dark
    atlas.add_region(580, 660, 70, 70);   // 73: Night Tree
    atlas.add_region(660, 670, 55, 50);   // 74: Night Bush
    atlas.add_region(580, 740, 80, 50);   // 75: Dark Hedge
    atlas.add_region(730, 660, 40, 35);   // 76: Rock Moss
    atlas.add_region(780, 670, 30, 25);   // 77: Stump
    // Misc Objects
    atlas.add_region(1080, 498, 30, 45);  // 78: Tombstone 1
    atlas.add_region(1115, 498, 30, 45);  // 79: Tombstone 2
    atlas.add_region(1080, 548, 30, 40);  // 80: Tombstone 3
    atlas.add_region(1170, 498, 35, 55);  // 81: Statue
    atlas.add_region(1210, 498, 30, 40);  // 82: Urn
    atlas.add_region(1250, 498, 40, 55);  // 83: Dark Statue
    atlas.add_region(1320, 498, 25, 80);  // 84: Lamp Post
    atlas.add_region(1365, 498, 25, 40);  // 85: Fire Hydrant
    atlas.add_region(1085, 645, 95, 35);  // 86: Brick Wall 1
    atlas.add_region(1085, 685, 95, 30);  // 87: Brick Wall 2
    atlas.add_region(1205, 645, 85, 35);  // 88: Brick Wall 3
    atlas.add_region(1320, 645, 80, 35);  // 89: Stone Wall
    atlas.add_region(1085, 730, 75, 60);  // 90: Pentagram Circle
    atlas.add_region(1195, 730, 55, 55);  // 91: Campfire
    atlas.add_region(1320, 810, 30, 35);  // 92: Barrel 1
    atlas.add_region(1360, 810, 30, 35);  // 93: Barrel 2
}

void define_object_stamps(GameState& game) {
    // Object stamps reference atlas regions by index (0-based, but region 55+ are objects)
    auto add = [&](const char* name, int region, float pw, float ph, const char* cat) {
        game.object_stamps.push_back({name, region, 0, 0, pw, ph, cat});
    };
    // Buildings (regions 55-59)
    add("Gas Mart",       54, 140, 96, "building");
    add("Salvage Repair", 55, 128, 96, "building");
    add("Tall Building",  56, 96,  96, "building");
    add("Motel/House",    57, 80,  90, "building");
    add("Dead End Sign",  58, 80,  36, "building");
    // Vehicles (regions 60-63)
    add("Impala",         59, 80,  44, "vehicle");
    add("Red Car",        60, 72,  44, "vehicle");
    add("Blue Car",       61, 72,  44, "vehicle");
    add("Impala (side)",  62, 180, 64, "vehicle");
    // Trees (regions 64-77)
    add("Large Tree 1",   63, 64,  80, "tree");
    add("Large Tree 2",   64, 58,  80, "tree");
    add("Medium Tree",    65, 44,  76, "tree");
    add("Dead Tree 1",    66, 48,  86, "tree");
    add("Dead Tree 2",    67, 52,  86, "tree");
    add("Dead Tree 3",    68, 40,  82, "tree");
    add("Bush Large",     69, 52,  40, "tree");
    add("Bush Small",     70, 30,  30, "tree");
    add("Bush Dark",      71, 30,  30, "tree");
    add("Night Tree",     72, 60,  64, "tree");
    add("Night Bush",     73, 48,  44, "tree");
    add("Dark Hedge",     74, 72,  44, "tree");
    add("Rock Moss",      75, 36,  30, "tree");
    add("Stump",          76, 26,  22, "tree");
    // Misc (regions 78-93)
    add("Tombstone 1",    77, 26,  40, "misc");
    add("Tombstone 2",    78, 26,  40, "misc");
    add("Tombstone 3",    79, 26,  36, "misc");
    add("Statue",         80, 30,  48, "misc");
    add("Urn",            81, 26,  36, "misc");
    add("Dark Statue",    82, 36,  48, "misc");
    add("Lamp Post",      83, 22,  72, "misc");
    add("Fire Hydrant",   84, 22,  36, "misc");
    add("Brick Wall 1",   85, 86,  32, "misc");
    add("Brick Wall 2",   86, 86,  26, "misc");
    add("Brick Wall 3",   87, 76,  32, "misc");
    add("Stone Wall",     88, 72,  32, "misc");
    add("Pentagram",      89, 68,  54, "misc");
    add("Campfire",       90, 48,  48, "misc");
    add("Barrel 1",       91, 26,  30, "misc");
    add("Barrel 2",       92, 26,  30, "misc");
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
    bobby.dir = 0; bobby.sprite_atlas_id = 0;
    bobby.move_speed = 30.0f; bobby.wander_interval = 4.0f;
    bobby.dialogue = bobby_dlg.get_lines("greeting");
    if (bobby.dialogue.empty()) bobby.dialogue = {{"Bobby", "Hey there."}};
    game.npcs.push_back(bobby);

    NPC stranger;
    stranger.name = "???"; stranger.position = {20.0f * 32.0f, 7.0f * 32.0f};
    stranger.home_pos = stranger.position; stranger.wander_target = stranger.position;
    stranger.dir = 1; stranger.sprite_atlas_id = 1;
    stranger.move_speed = 25.0f; stranger.wander_interval = 5.0f;
    stranger.dialogue = stranger_dlg.get_lines("greeting");
    if (stranger.dialogue.empty()) stranger.dialogue = {{"???", "..."}};
    game.npcs.push_back(stranger);

    NPC vampire;
    vampire.name = "Vampire"; vampire.position = {19.0f * 32.0f, 14.0f * 32.0f};
    vampire.home_pos = vampire.position; vampire.wander_target = vampire.position;
    vampire.dir = 2; vampire.sprite_atlas_id = 2;
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
    azazel.dir = 0; azazel.sprite_atlas_id = 3;
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

        // Load tileset
        auto* tileset_tex = resources.load_texture("assets/textures/new_tileset.png");
        game.tileset_atlas = std::make_unique<eb::TextureAtlas>(tileset_tex);
        define_tileset_regions(*game.tileset_atlas);
        game.tileset_desc = renderer.get_texture_descriptor(*tileset_tex);

        // Load Dean
        auto* dean_tex = resources.load_texture("assets/textures/dean_sprites.png");
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

        // Load Sam
        auto* sam_tex = resources.load_texture("assets/textures/sam_sprites.png");
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

        // Sam as party member
        PartyMember sam;
        sam.name = "Sam";
        sam.position = {game.player_pos.x, game.player_pos.y + 32.0f};
        sam.dir = 0;
        game.party.push_back(sam);

        // Breadcrumb trail
        game.trail.resize(GameState::TRAIL_SIZE);
        for (auto& r : game.trail) { r.pos = game.player_pos; r.dir = 0; }
        game.trail_head = 0; game.trail_count = 0;

        // Create map
        const int MAP_W = 30, MAP_H = 20, TILE_SZ = 32;
        game.tile_map.create(MAP_W, MAP_H, TILE_SZ);
        game.tile_map.set_tileset(game.tileset_atlas.get());
        game.tile_map.set_tileset_path("assets/textures/new_tileset.png");
        game.tile_map.add_layer("ground", generate_town_map(MAP_W, MAP_H));
        game.tile_map.set_collision(generate_town_collision(MAP_W, MAP_H));
        game.tile_map.set_animated_tiles(TILE_WATER_DEEP, TILE_WATER_SHORE_L);

        // Objects & NPCs
        setup_objects(game, tileset_tex);
        define_object_stamps(game);
        setup_npcs(game);

        // Load NPC sprite sheets
        auto load_npc = [&](const char* path, int cw, int ch) {
            auto* tex = resources.load_texture(path);
            auto atlas = std::make_unique<eb::TextureAtlas>(tex);
            define_npc_atlas_regions(*atlas, cw, ch);
            game.npc_descs.push_back(renderer.get_texture_descriptor(*tex));
            game.npc_atlases.push_back(std::move(atlas));
        };
        load_npc("assets/textures/bobby_sprites.png",      123, 174);
        load_npc("assets/textures/stranger_sprites.png",     70, 140);
        load_npc("assets/textures/vampire_sprites.png",     136, 190);
        load_npc("assets/textures/yelloweyes_sprites.png",  134, 187);

        // Dialogue box: background texture and character portraits
        auto* dialog_tex = resources.load_texture("assets/textures/dialog_bg.png");
        if (dialog_tex) {
            game.dialogue.set_background(renderer.get_texture_descriptor(*dialog_tex));
        }
        auto load_portrait = [&](const char* path, const char* speaker) {
            auto* tex = resources.load_texture(path);
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

// ─── Battle logic ───

void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id) {
    auto& b = game.battle;
    b.phase = BattlePhase::Intro;
    b.enemy_name = enemy;
    b.enemy_hp_actual = hp; b.enemy_hp_max = hp; b.enemy_atk = atk;
    b.enemy_sprite_id = sprite_id;
    b.sam_hp_actual = game.sam_hp; b.sam_hp_max = game.sam_hp_max;
    b.sam_hp_display = static_cast<float>(game.sam_hp);
    b.sam_atk = game.sam_atk;
    b.active_fighter = 0; // Dean goes first
    b.attack_anim_timer = 0.0f;
    b.player_hp_actual = game.player_hp;
    b.player_hp_max = game.player_hp_max;
    b.player_hp_display = static_cast<float>(game.player_hp);
    b.player_atk = game.player_atk; b.player_def = game.player_def;
    b.menu_selection = 0; b.phase_timer = 0.0f;
    b.message = "A " + enemy + " appeared!";
    b.last_damage = 0; b.random_encounter = random;
}

void update_battle(GameState& game, float dt, bool confirm, bool up, bool down) {
    auto& b = game.battle;
    b.phase_timer += dt;
    b.attack_anim_timer += dt;

    // Roll HP displays toward actual
    auto roll_hp = [&](float& display, int actual) {
        if (display > actual) {
            display -= 40.0f * dt;
            if (display < actual) display = static_cast<float>(actual);
        }
    };
    roll_hp(b.player_hp_display, b.player_hp_actual);
    roll_hp(b.sam_hp_display, b.sam_hp_actual);

    switch (b.phase) {
    case BattlePhase::Intro:
        if (b.phase_timer > 1.5f || confirm) {
            b.phase = BattlePhase::PlayerTurn; b.phase_timer = 0.0f;
            b.active_fighter = 0; b.menu_selection = 0; b.message = "";
        }
        break;

    case BattlePhase::PlayerTurn:
        if (up && b.menu_selection > 0) b.menu_selection--;
        if (down && b.menu_selection < 2) b.menu_selection++;
        if (confirm) {
            b.phase_timer = 0.0f; b.attack_anim_timer = 0.0f;
            const char* fighter = (b.active_fighter == 0) ? "Dean" : "Sam";
            int atk = (b.active_fighter == 0) ? b.player_atk : b.sam_atk;

            if (b.menu_selection == 0) {
                int damage = atk + (game.rng() % 5) - 2;
                if (damage < 1) damage = 1;
                b.enemy_hp_actual -= damage; b.last_damage = damage;
                b.message = std::string(fighter) + " attacks! " + std::to_string(damage) + " damage!";
                b.phase = BattlePhase::PlayerAttack;
            } else if (b.menu_selection == 1) {
                int heal = 8 + game.rng() % 8;
                if (b.active_fighter == 0) {
                    b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                } else {
                    b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
                }
                b.message = std::string(fighter) + " braces! Recovered " + std::to_string(heal) + " HP.";
                b.phase = BattlePhase::PlayerAttack;
            } else {
                if (b.random_encounter && (game.rng() % 3) != 0) {
                    b.message = "Got away safely!";
                    b.phase = BattlePhase::Victory; b.phase_timer = 0.0f;
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
            } else if (b.active_fighter == 0 && b.sam_hp_actual > 0) {
                // Sam's turn next
                b.active_fighter = 1; b.menu_selection = 0;
                b.phase = BattlePhase::PlayerTurn; b.message = "";
            } else {
                // Enemy turn
                b.phase = BattlePhase::EnemyTurn;
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::EnemyTurn: {
        // Enemy attacks a random party member
        int target = (game.rng() % 2 == 0 && b.sam_hp_actual > 0) ? 1 : 0;
        if (b.player_hp_actual <= 0) target = 1;
        if (b.sam_hp_actual <= 0) target = 0;

        int def = (target == 0) ? b.player_def : 2;
        int damage = b.enemy_atk + (game.rng() % 5) - 2 - def / 3;
        if (damage < 1) damage = 1;

        if (target == 0) {
            b.player_hp_actual -= damage;
            b.message = b.enemy_name + " attacks Dean! " + std::to_string(damage) + " damage!";
        } else {
            b.sam_hp_actual -= damage;
            b.message = b.enemy_name + " attacks Sam! " + std::to_string(damage) + " damage!";
        }
        b.last_damage = damage;
        b.phase = BattlePhase::EnemyAttack; b.phase_timer = 0.0f;
        b.attack_anim_timer = 0.0f;
        break;
    }

    case BattlePhase::EnemyAttack:
        if (b.phase_timer > 1.2f || confirm) {
            bool dean_down = b.player_hp_actual <= 0;
            bool sam_down = b.sam_hp_actual <= 0;
            if (dean_down && sam_down) {
                b.player_hp_actual = 0; b.sam_hp_actual = 0;
                b.phase = BattlePhase::Defeat; b.message = "The Winchesters are down!";
            } else {
                b.active_fighter = 0; b.menu_selection = 0;
                // Skip Dean if he's down
                if (b.player_hp_actual <= 0) b.active_fighter = 1;
                b.phase = BattlePhase::PlayerTurn; b.message = "";
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::Victory:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = std::max(b.player_hp_actual, 1);
            game.sam_hp = std::max(b.sam_hp_actual, 1);
            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::Defeat:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = game.player_hp_max / 2;
            game.sam_hp = game.sam_hp_max / 2;
            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::None: break;
    }
}

// ─── Core game update ───

void update_game(GameState& game, const eb::InputState& input, float dt) {
    game.game_time += dt;

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
                         npc.battle_enemy_hp, npc.battle_enemy_atk, false,
                         npc.sprite_atlas_id);
            game.pending_battle_npc = -1;
        }
        return;
    }

    // Player movement
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

        // Breadcrumb trail for party followers
        game.trail_step_accum += speed * dt;
        const float TRAIL_STEP = 4.0f;
        while (game.trail_step_accum >= TRAIL_STEP) {
            game.trail_step_accum -= TRAIL_STEP;
            game.trail[game.trail_head] = {game.player_pos, game.player_dir};
            game.trail_head = (game.trail_head + 1) % GameState::TRAIL_SIZE;
            if (game.trail_count < GameState::TRAIL_SIZE) game.trail_count++;
        }

        // Update party followers
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
                    float lerp_speed = speed * 1.1f;
                    if (d <= lerp_speed * dt) { pm.position = target.pos; }
                    else { pm.position.x += (dx/d)*lerp_speed*dt; pm.position.y += (dy/d)*lerp_speed*dt; }
                    if (std::abs(dx) > std::abs(dy)) pm.dir = (dx < 0) ? 2 : 3;
                    else pm.dir = (dy < 0) ? 1 : 0;
                    pm.anim_timer += dt;
                    if (pm.anim_timer >= 0.2f) { pm.anim_timer -= 0.2f; pm.frame = 1 - pm.frame; }
                } else {
                    pm.dir = target.dir; pm.frame = 0; pm.anim_timer = 0.0f;
                }
            }
        }

        // (Random encounters disabled — battles only through NPC interaction)
    } else {
        game.player_frame = 0; game.anim_timer = 0.0f;
        for (auto& pm : game.party) { pm.moving = false; pm.frame = 0; pm.anim_timer = 0.0f; }
    }

    // ── NPC AI update ──
    for (int i = 0; i < (int)game.npcs.size(); i++) {
        auto& npc = game.npcs[i];
        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (npc.hostile && !npc.has_triggered && dist < npc.aggro_range) {
            // Hostile NPC: chase player
            npc.aggro_active = true;
            float nx = dx / dist, ny = dy / dist;
            npc.position.x += nx * npc.move_speed * dt;
            npc.position.y += ny * npc.move_speed * dt;
            npc.moving = true;
            if (std::abs(dx) > std::abs(dy))
                npc.dir = (dx > 0) ? 3 : 2;
            else
                npc.dir = (dy > 0) ? 0 : 1;

            // Reached attack range — auto trigger dialogue then battle
            if (dist < npc.attack_range) {
                npc.has_triggered = true;
                npc.aggro_active = false;
                npc.moving = false;
                game.dialogue.start(npc.dialogue);
                game.pending_battle_npc = npc.has_battle ? i : -1;
            }
        } else if (!npc.hostile || npc.has_triggered) {
            // Friendly/passive NPC: idle wander near home
            npc.wander_timer += dt;
            if (npc.wander_timer >= npc.wander_interval) {
                npc.wander_timer = 0.0f;
                float angle = (game.rng() % 628) / 100.0f;
                float r = 16.0f + (game.rng() % 32);
                npc.wander_target = eb::Vec2(
                    npc.home_pos.x + std::cos(angle) * r,
                    npc.home_pos.y + std::sin(angle) * r);
            }
            float wx = npc.wander_target.x - npc.position.x;
            float wy = npc.wander_target.y - npc.position.y;
            float wd = std::sqrt(wx*wx + wy*wy);
            if (wd > 3.0f) {
                npc.position.x += (wx/wd) * npc.move_speed * dt;
                npc.position.y += (wy/wd) * npc.move_speed * dt;
                npc.moving = true;
                if (std::abs(wx) > std::abs(wy))
                    npc.dir = (wx > 0) ? 3 : 2;
                else
                    npc.dir = (wy > 0) ? 0 : 1;
            } else {
                npc.moving = false;
            }
        }
        // NPC walk animation
        if (npc.moving) {
            npc.anim_timer += dt;
            if (npc.anim_timer >= 0.2f) { npc.anim_timer -= 0.2f; npc.frame = 1 - npc.frame; }
        } else {
            npc.frame = 0; npc.anim_timer = 0.0f;
        }
    }

    // Manual NPC interaction (Z/A button for friendly NPCs)
    if (input.is_pressed(eb::InputAction::Confirm)) {
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            auto& npc = game.npcs[i];
            if (npc.hostile && !npc.has_triggered) continue; // Hostile auto-triggers
            float dx = game.player_pos.x - npc.position.x;
            float dy = game.player_pos.y - npc.position.y;
            float dist = std::sqrt(dx*dx + dy*dy);
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

// ─── Grass overlay ───

static void render_grass_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    eb::Rect view = game.camera.visible_area();
    float ts = (float)game.tile_map.tile_size();
    int sx = std::max(0, (int)std::floor(view.x / ts));
    int sy = std::max(0, (int)std::floor(view.y / ts));
    int ex = std::min(game.tile_map.width(), (int)std::ceil((view.x + view.w) / ts) + 1);
    int ey = std::min(game.tile_map.height(), (int)std::ceil((view.y + view.h) / ts) + 1);
    batch.set_texture(game.white_desc);
    const float margin = 6.0f;
    for (int ty = sy; ty < ey; ty++) {
        for (int tx = sx; tx < ex; tx++) {
            int tile = game.tile_map.tile_at(0, tx, ty);
            if (tile < TILE_GRASS_PURE || tile > TILE_GRASS_DARK) continue;
            auto is_grass = [&](int x, int y) {
                if (x < 0 || x >= game.tile_map.width() || y < 0 || y >= game.tile_map.height()) return false;
                int t = game.tile_map.tile_at(0, x, y);
                return t >= TILE_GRASS_PURE && t <= TILE_GRASS_DARK;
            };
            if (!is_grass(tx-1,ty)||!is_grass(tx+1,ty)||!is_grass(tx,ty-1)||!is_grass(tx,ty+1)) continue;
            float x_min = margin, x_max = ts - margin, y_min = margin, y_max = ts - margin;
            int tuft_count = 2 + (int)(tile_hash(tx, ty, 0) * 2.0f);
            for (int t = 0; t < tuft_count; t++) {
                float fx = tile_hash(tx,ty,t*5+1), fy = tile_hash(tx,ty,t*5+2);
                float base_x = tx*ts + x_min + fx*(x_max-x_min);
                float base_y = ty*ts + y_min + fy*(y_max-y_min);
                float wind = std::sin(time*2.0f + tx*0.7f + ty*0.5f + t*1.3f) * 1.5f;
                int blades = 2 + (int)(tile_hash(tx,ty,t*5+3)*1.5f);
                for (int b = 0; b < blades; b++) {
                    float blade_h = 2.0f + tile_hash(tx,ty,t*5+b+10)*2.0f;
                    float spread = (b-(blades-1)*0.5f)*1.5f;
                    float blade_sway = wind * (0.7f + tile_hash(tx,ty,t*5+b+20)*0.6f);
                    float shade = 0.7f + tile_hash(tx,ty,t*5+b+30)*0.3f;
                    batch.draw_quad({base_x+spread+blade_sway, base_y-blade_h},
                                    {1.0f, blade_h}, {0,0},{1,1},
                                    {0.2f*shade, 0.55f*shade, 0.12f*shade, 0.8f});
                }
            }
        }
    }
}

// ─── Leaf overlay ───

static void render_leaf_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    batch.set_texture(game.white_desc);
    for (int oi = 0; oi < (int)game.world_objects.size(); oi++) {
        const auto& obj = game.world_objects[oi];
        if (obj.sprite_id > 4) continue;
        const auto& def = game.object_defs[obj.sprite_id];
        float canopy_w = def.render_size.x * 0.8f, canopy_h = def.render_size.y * 0.5f;
        float canopy_cx = obj.position.x, canopy_cy = obj.position.y - def.render_size.y * 0.7f;
        int leaf_count = 6 + (int)(tile_hash(oi, 0, 99) * 6.0f);
        for (int l = 0; l < leaf_count; l++) {
            float lx_f = tile_hash(oi,l,10)-0.5f, ly_f = tile_hash(oi,l,20)-0.5f;
            float leaf_x = canopy_cx + lx_f*canopy_w, leaf_y = canopy_cy + ly_f*canopy_h;
            float leaf_sz = 2.0f + tile_hash(oi,l,30)*2.5f;
            float wind_phase = time*1.8f + oi*2.1f + l*0.9f;
            float sway_x = std::sin(wind_phase)*2.0f, sway_y = std::cos(wind_phase*0.7f)*1.0f;
            float flutter = std::sin(time*4.0f + l*3.7f);
            if (flutter > 0.7f) { sway_x *= 2.0f; sway_y *= 1.5f; }
            float shade = 0.6f + tile_hash(oi,l,40)*0.4f;
            float r = 0.15f + tile_hash(oi,l,50)*0.15f;
            batch.draw_sorted({leaf_x+sway_x-leaf_sz*0.5f, leaf_y+sway_y-leaf_sz*0.5f},
                              {leaf_sz, leaf_sz}, {0,0},{1,1},
                              obj.position.y - 0.05f, game.white_desc,
                              {r*shade, 0.55f*shade, 0.1f*shade, 0.75f});
        }
    }
}

// ─── Render battle ───

void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh) {
    auto& b = game.battle;
    float sprite_w = 72.0f, sprite_h = 96.0f;

    // Background
    batch.set_texture(game.white_desc);
    batch.draw_quad({0,0},{sw,sh},{0,0},{1,1},{0.02f,0.02f,0.08f,1.0f});

    // ── Enemy sprite (top center, facing down toward player) ──
    float enemy_cx = sw * 0.5f;
    float enemy_y = 20.0f;
    float ew = sprite_w * 1.5f, eh = sprite_h * 1.5f;

    // Enemy hit flash during player attack
    bool enemy_flash = (b.phase == BattlePhase::PlayerAttack &&
                        b.attack_anim_timer < 0.8f &&
                        std::fmod(b.attack_anim_timer, 0.15f) < 0.075f);

    if (b.enemy_sprite_id >= 0 && b.enemy_sprite_id < (int)game.npc_atlases.size() && !enemy_flash) {
        auto& atlas = *game.npc_atlases[b.enemy_sprite_id];
        auto desc = game.npc_descs[b.enemy_sprite_id];
        auto sr = get_character_sprite(atlas, 0, false, 0);
        batch.set_texture(desc);
        batch.draw_quad({enemy_cx - ew*0.5f, enemy_y}, {ew, eh}, sr.uv_min, sr.uv_max);
    }

    // Enemy name + HP bar
    float ebx = sw*0.5f - 120, eby = enemy_y + eh + 6;
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx,eby},{240,45},{0,0},{1,1},{0.1f,0.08f,0.15f,0.8f});
    text.draw_text(batch, game.font_desc, b.enemy_name, {ebx+8,eby+4}, {1,0.4f,0.4f,1}, 0.9f);
    float hp_pct = std::max(0.0f,(float)b.enemy_hp_actual/b.enemy_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx+8,eby+24},{170,12},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 ehc = hp_pct>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:hp_pct>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({ebx+8,eby+24},{170*hp_pct,12},{0,0},{1,1},ehc);
    text.draw_text(batch,game.font_desc,std::to_string(std::max(0,b.enemy_hp_actual))+"/"+std::to_string(b.enemy_hp_max),
                   {ebx+185,eby+22},{1,1,1,1},0.6f);

    // ── Dean + Sam sprites (lower area, backs to us) ──
    float party_y = sh * 0.42f;
    float party_cx = sw * 0.5f;
    float dean_x = party_cx - sprite_w - 10.0f;
    float sam_x = party_cx + 10.0f;

    // Attack animation: lunge forward during PlayerAttack phase
    float dean_offset_y = 0, sam_offset_y = 0;
    if (b.phase == BattlePhase::PlayerAttack && b.attack_anim_timer < 0.5f) {
        float t = b.attack_anim_timer / 0.5f;
        float lunge = std::sin(t * 3.14159f) * 30.0f; // Lunge forward and back
        if (b.active_fighter == 0) dean_offset_y = -lunge;
        else sam_offset_y = -lunge;
    }

    // Dean (left) — flash red if hit
    bool dean_hit = (b.phase == BattlePhase::EnemyAttack &&
                     b.message.find("Dean") != std::string::npos &&
                     b.attack_anim_timer < 0.6f &&
                     std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!dean_hit && b.player_hp_actual > 0) {
        bool dean_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 0);
        int dean_frame = dean_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.dean_atlas, 1, dean_attacking, dean_frame);
        batch.set_texture(game.dean_desc);
        batch.draw_quad({dean_x, party_y + dean_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // Sam (right)
    bool sam_hit = (b.phase == BattlePhase::EnemyAttack &&
                    b.message.find("Sam") != std::string::npos &&
                    b.attack_anim_timer < 0.6f &&
                    std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!sam_hit && b.sam_hp_actual > 0) {
        bool sam_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 1);
        int sam_frame = sam_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.sam_atlas, 1, sam_attacking, sam_frame);
        batch.set_texture(game.sam_desc);
        batch.draw_quad({sam_x, party_y + sam_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // ── Party stats (bottom right) ──
    float pbx = sw - 260, pby = sh - 100;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx,pby},{240,92},{0,0},{1,1},{0.08f,0.08f,0.18f,0.9f});
    batch.draw_quad({pbx,pby},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});
    batch.draw_quad({pbx,pby+90},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});

    // Dean HP
    eb::Vec4 dean_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 0)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Dean",{pbx+8,pby+4},dean_name_col,0.7f);
    float dhp = b.player_hp_display;
    float dp = std::max(0.0f,dhp/b.player_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+60,pby+8},{120,10},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 dc=dp>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:dp>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+60,pby+8},{120*dp,10},{0,0},{1,1},dc);
    char dhs[32]; std::snprintf(dhs,sizeof(dhs),"%d/%d",(int)std::ceil(dhp),b.player_hp_max);
    text.draw_text(batch,game.font_desc,dhs,{pbx+186,pby+5},{1,1,1,1},0.5f);

    // Sam HP
    eb::Vec4 sam_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 1)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Sam",{pbx+8,pby+24},sam_name_col,0.7f);
    float shp = b.sam_hp_display;
    float sp = std::max(0.0f,shp/b.sam_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+60,pby+28},{120,10},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 sc2=sp>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:sp>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+60,pby+28},{120*sp,10},{0,0},{1,1},sc2);
    char shs[32]; std::snprintf(shs,sizeof(shs),"%d/%d",(int)std::ceil(shp),b.sam_hp_max);
    text.draw_text(batch,game.font_desc,shs,{pbx+186,pby+25},{1,1,1,1},0.5f);

    // Level
    text.draw_text(batch,game.font_desc,"Lv."+std::to_string(game.player_level),{pbx+8,pby+46},{0.7f,0.7f,0.7f,1},0.5f);

    // ── Battle menu (player turn) ──
    if (b.phase == BattlePhase::PlayerTurn) {
        const char* fighter = (b.active_fighter == 0) ? "Dean" : "Sam";
        float mx = 16, my = sh - 130;
        batch.set_texture(game.white_desc);
        batch.draw_quad({mx,my},{170,122},{0,0},{1,1},{0.08f,0.05f,0.15f,0.95f});
        batch.draw_quad({mx,my},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx,my+120},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx,my},{2,122},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        batch.draw_quad({mx+168,my},{2,122},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
        // Fighter name header
        text.draw_text(batch,game.font_desc,fighter,{mx+8,my+4},{0.4f,0.8f,1,1},0.7f);
        const char* opts[]={"Attack","Defend","Run"};
        for(int i=0;i<3;i++){
            eb::Vec4 c=(i==b.menu_selection)?eb::Vec4{1,1,0.3f,1}:eb::Vec4{0.8f,0.8f,0.8f,1};
            std::string pfx=(i==b.menu_selection)?"> ":"  ";
            text.draw_text(batch,game.font_desc,pfx+opts[i],{mx+8,my+26+i*30.0f},c,0.9f);
        }
    }

    // ── Message box ──
    if (!b.message.empty()) {
        float mw = sw*0.55f, mh = 36;
        float mx2 = (sw-mw)*0.5f, my2 = sh*0.40f;
        batch.set_texture(game.white_desc);
        batch.draw_quad({mx2,my2},{mw,mh},{0,0},{1,1},{0.05f,0.05f,0.12f,0.92f});
        batch.draw_quad({mx2,my2},{mw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,1});
        text.draw_text(batch,game.font_desc,b.message,{mx2+10,my2+8},{1,1,1,1},0.8f);
    }
}

// ─── Render HUD ───

static void render_hud(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    float hx=10, hy=10, hw=200, hh=50;
    batch.set_texture(game.white_desc);
    batch.draw_quad({hx,hy},{hw,hh},{0,0},{1,1},{0.03f,0.03f,0.10f,0.85f});
    batch.draw_quad({hx,hy},{hw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx,hy+hh-2},{hw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx,hy},{2,hh},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    batch.draw_quad({hx+hw-2,hy},{2,hh},{0,0},{1,1},{0.5f,0.5f,0.7f,0.9f});
    char ns[64]; std::snprintf(ns,sizeof(ns),"Dean  Lv.%d",game.player_level);
    text.draw_text(batch,game.font_desc,ns,{hx+10,hy+6},{1,1,1,1},0.8f);
    float hp_pct=std::max(0.0f,(float)game.player_hp/game.player_hp_max);
    float bx=hx+10, by=hy+28, bw=130, bh=12;
    batch.set_texture(game.white_desc);
    batch.draw_quad({bx,by},{bw,bh},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 hc=hp_pct>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:hp_pct>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({bx,by},{bw*hp_pct,bh},{0,0},{1,1},hc);
    char hs[32]; std::snprintf(hs,sizeof(hs),"%d/%d",game.player_hp,game.player_hp_max);
    text.draw_text(batch,game.font_desc,hs,{bx+bw+6,by-1},{1,1,1,1},0.6f);
}

// ─── Render world ───

void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    batch.set_projection(game.camera.projection_matrix());

    // Tile map with water animation
    batch.set_texture(game.tileset_desc);
    game.tile_map.render(batch, game.camera, game.game_time);

    // Grass overlay
    render_grass_overlay(game, batch, game.game_time);

    // Y-sorted objects
    for (const auto& obj : game.world_objects) {
        const auto& def = game.object_defs[obj.sprite_id];
        const auto& region = game.object_regions[obj.sprite_id];
        eb::Vec2 dp = {obj.position.x - def.render_size.x*0.5f, obj.position.y - def.render_size.y};
        batch.draw_sorted(dp, def.render_size, region.uv_min, region.uv_max,
                         obj.position.y, game.tileset_desc);
    }

    // Leaf overlay
    render_leaf_overlay(game, batch, game.game_time);

    // NPCs
    for (const auto& npc : game.npcs) {
        if (npc.sprite_atlas_id >= 0 && npc.sprite_atlas_id < (int)game.npc_atlases.size()) {
            auto& atlas = *game.npc_atlases[npc.sprite_atlas_id];
            auto desc = game.npc_descs[npc.sprite_atlas_id];
            auto sr = get_character_sprite(atlas, npc.dir, npc.moving, npc.frame);
            float rw=48, rh=64;
            eb::Vec2 dp = {npc.position.x-rw*0.5f, npc.position.y-rh+4};
            batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, npc.position.y, desc);
        }
    }

    // Party (Sam)
    for (int pi = 0; pi < (int)game.party.size(); pi++) {
        auto& pm = game.party[pi];
        auto sr = get_character_sprite(*game.sam_atlas, pm.dir, pm.moving, pm.frame);
        float rw=48, rh=64;
        float bob = pm.moving ? std::sin(pm.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {pm.position.x-rw*0.5f, pm.position.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, pm.position.y, game.sam_desc);
    }

    // Player (Dean)
    {
        auto sr = get_character_sprite(*game.dean_atlas, game.player_dir, game.player_moving, game.player_frame);
        float rw=48, rh=64;
        float bob = game.player_moving ? std::sin(game.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {game.player_pos.x-rw*0.5f, game.player_pos.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, game.player_pos.y, game.dean_desc);
    }

    batch.flush_sorted();
    batch.flush();
}

// ─── Render UI overlay ───

void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh) {
    batch.set_projection(screen_proj);

    // NPC labels
    for (const auto& npc : game.npcs) {
        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < npc.interact_radius * 1.5f) {
            eb::Vec2 cam_off = game.camera.offset();
            float sx = npc.position.x + cam_off.x;
            float sy = npc.position.y + cam_off.y - 100.0f;
            float ns = 0.7f;
            auto name_size = text.measure_text(npc.name, ns);
            float lp = 6.0f;
            batch.set_texture(game.white_desc);
            batch.draw_quad({sx-name_size.x*0.5f-lp, sy-lp*0.5f},
                {name_size.x+lp*2, name_size.y+lp}, {0,0},{1,1},{0,0,0,0.6f});
            text.draw_text(batch, game.font_desc, npc.name,
                {sx-name_size.x*0.5f, sy}, {1,1,0.4f,1}, ns);
            if (dist < npc.interact_radius) {
                const char* hint_text =
#ifdef __ANDROID__
                    "[A] Talk";
#else
                    "[Z] Talk";
#endif
                float hs = 0.5f;
                auto hint_size = text.measure_text(hint_text, hs);
                float hy = sy + name_size.y + 8.0f;
                batch.set_texture(game.white_desc);
                batch.draw_quad({sx-hint_size.x*0.5f-4,hy-2},
                    {hint_size.x+8,hint_size.y+4},{0,0},{1,1},{0,0,0,0.5f});
                text.draw_text(batch, game.font_desc, hint_text,
                    {sx-hint_size.x*0.5f, hy}, {0.8f,0.8f,0.8f,0.9f}, hs);
            }
        }
    }

    // HUD
    render_hud(game, batch, text);

    // Dialogue
    if (game.dialogue.is_active()) {
        game.dialogue.render(batch, text, game.font_desc, game.white_desc, sw, sh);
    }
}
