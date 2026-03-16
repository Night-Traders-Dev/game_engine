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
#include <filesystem>

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
          << ",\"sprite_atlas_key\":\"" << esc(npc.sprite_atlas_key) << "\""
          << ",\"sprite_grid_w\":" << npc.sprite_grid_w
          << ",\"sprite_grid_h\":" << npc.sprite_grid_h
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

// Minimal JSON helpers for map loading
static void mskip(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
}
static std::string mstr(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '"') return "";
    i++; std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i]=='\\' && i+1<s.size()) { i++; out+=s[i]; } else out+=s[i]; i++;
    }
    if (i < s.size()) i++;
    return out;
}
static double mnum(const std::string& s, size_t& i) {
    size_t start = i;
    if (i<s.size() && s[i]=='-') i++;
    while (i<s.size() && ((s[i]>='0'&&s[i]<='9')||s[i]=='.')) i++;
    return std::stod(s.substr(start, i-start));
}
static bool mbool(const std::string& s, size_t& i) {
    if (s.substr(i, 4) == "true") { i+=4; return true; }
    if (s.substr(i, 5) == "false") { i+=5; return false; }
    return false;
}
static void mskipval(const std::string& s, size_t& i);
static void mskipobj(const std::string& s, size_t& i) {
    if (s[i]!='{') return; i++;
    while (i<s.size()&&s[i]!='}') { mskip(s,i); if(s[i]=='}') break; mstr(s,i); mskip(s,i); if(s[i]==':')i++; mskip(s,i); mskipval(s,i); mskip(s,i); if(s[i]==',')i++; }
    if (i<s.size()) i++;
}
static void mskiparr(const std::string& s, size_t& i) {
    if (s[i]!='[') return; i++;
    while (i<s.size()&&s[i]!=']') { mskip(s,i); if(s[i]==']') break; mskipval(s,i); mskip(s,i); if(s[i]==',')i++; }
    if (i<s.size()) i++;
}
static void mskipval(const std::string& s, size_t& i) {
    mskip(s,i); if(i>=s.size()) return;
    if(s[i]=='"') mstr(s,i);
    else if(s[i]=='{') mskipobj(s,i);
    else if(s[i]=='[') mskiparr(s,i);
    else while(i<s.size()&&s[i]!=','&&s[i]!='}'&&s[i]!=']'&&s[i]!=' '&&s[i]!='\n') i++;
}

bool load_map_file(GameState& game, eb::Renderer& renderer, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[Map] Failed to load: %s\n", path.c_str());
        return false;
    }
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Load tiles/collision/portals via existing TileMap loader
    game.tile_map.load_json(path);

    // Clear existing objects for reload
    game.world_objects.clear();
    game.object_defs.clear();
    game.object_regions.clear();

    // Re-populate object defs/regions from stamps (they persist across map loads)
    if (game.tileset_atlas) {
        for (auto& stamp : game.object_stamps) {
            if (stamp.region_id >= 0 && stamp.region_id < game.tileset_atlas->region_count()) {
                ObjectDef def;
                auto ar = game.tileset_atlas->region(stamp.region_id);
                def.src_pos = {(float)ar.pixel_x, (float)ar.pixel_y};
                def.src_size = {(float)ar.pixel_w, (float)ar.pixel_h};
                def.render_size = {stamp.place_w, stamp.place_h};
                game.object_defs.push_back(def);
                game.object_regions.push_back(ar);
            }
        }
    }

    // Parse objects array
    size_t obj_pos = json.find("\"objects\"");
    if (obj_pos != std::string::npos) {
        size_t i = json.find('[', obj_pos);
        if (i != std::string::npos) {
            i++; // skip [
            while (i < json.size() && json[i] != ']') {
                mskip(json, i);
                if (json[i] == ']') break;
                if (json[i] == '{') {
                    i++; // skip {
                    float ox=0,oy=0; int sx=0,sy=0,sw=0,sh=0; float rw=0,rh=0;
                    while (i < json.size() && json[i] != '}') {
                        mskip(json, i);
                        if (json[i] == '}') break;
                        std::string key = mstr(json, i);
                        mskip(json, i); if (json[i]==':') i++; mskip(json, i);
                        if (key=="x") ox=(float)mnum(json,i);
                        else if (key=="y") oy=(float)mnum(json,i);
                        else if (key=="src_x") sx=(int)mnum(json,i);
                        else if (key=="src_y") sy=(int)mnum(json,i);
                        else if (key=="src_w") sw=(int)mnum(json,i);
                        else if (key=="src_h") sh=(int)mnum(json,i);
                        else if (key=="render_w") rw=(float)mnum(json,i);
                        else if (key=="render_h") rh=(float)mnum(json,i);
                        else mskipval(json, i);
                        mskip(json, i); if (json[i]==',') i++;
                    }
                    if (json[i]=='}') i++;

                    if (sw > 0 && sh > 0 && game.tileset_atlas) {
                        // Find or create object def matching this source region (match by full bounds)
                        int obj_id = -1;
                        for (int oi = 0; oi < (int)game.object_regions.size(); oi++) {
                            auto& r = game.object_regions[oi];
                            if (r.pixel_x == sx && r.pixel_y == sy &&
                                r.pixel_w == sw && r.pixel_h == sh) { obj_id = oi; break; }
                        }
                        if (obj_id < 0) {
                            float tw = (float)game.tileset_atlas->texture()->width();
                            float th = (float)game.tileset_atlas->texture()->height();
                            ObjectDef def;
                            def.src_pos = {(float)sx, (float)sy};
                            def.src_size = {(float)sw, (float)sh};
                            def.render_size = {rw, rh};
                            game.object_defs.push_back(def);
                            eb::AtlasRegion ar;
                            ar.pixel_x = sx; ar.pixel_y = sy;
                            ar.pixel_w = sw; ar.pixel_h = sh;
                            ar.uv_min = {sx/tw, sy/th};
                            ar.uv_max = {(sx+sw)/tw, (sy+sh)/th};
                            game.object_regions.push_back(ar);
                            obj_id = (int)game.object_defs.size() - 1;
                        }
                        game.world_objects.push_back({obj_id, {ox, oy}});
                    }
                }
                mskip(json, i); if (json[i]==',') i++;
            }
        }
    }

    // Parse NPCs array
    size_t npc_pos = json.find("\"npcs\"");
    if (npc_pos != std::string::npos) {
        game.npcs.clear();
        size_t i = json.find('[', npc_pos);
        if (i != std::string::npos) {
            i++;
            while (i < json.size() && json[i] != ']') {
                mskip(json, i);
                if (json[i] == ']') break;
                if (json[i] == '{') {
                    i++;
                    NPC npc;
                    npc.home_pos = npc.position;
                    npc.wander_target = npc.position;
                    std::vector<eb::DialogueLine> dlg;

                    while (i < json.size() && json[i] != '}') {
                        mskip(json, i);
                        if (json[i] == '}') break;
                        std::string key = mstr(json, i);
                        mskip(json, i); if (json[i]==':') i++; mskip(json, i);

                        if (key=="name") npc.name = mstr(json, i);
                        else if (key=="x") npc.position.x = (float)mnum(json, i);
                        else if (key=="y") npc.position.y = (float)mnum(json, i);
                        else if (key=="dir") npc.dir = (int)mnum(json, i);
                        else if (key=="sprite_atlas_id") npc.sprite_atlas_id = (int)mnum(json, i);
                        else if (key=="sprite_atlas_key") npc.sprite_atlas_key = mstr(json, i);
                        else if (key=="sprite_grid_w") npc.sprite_grid_w = (int)mnum(json, i);
                        else if (key=="sprite_grid_h") npc.sprite_grid_h = (int)mnum(json, i);
                        else if (key=="interact_radius") npc.interact_radius = (float)mnum(json, i);
                        else if (key=="hostile") npc.hostile = mbool(json, i);
                        else if (key=="aggro_range") npc.aggro_range = (float)mnum(json, i);
                        else if (key=="attack_range") npc.attack_range = (float)mnum(json, i);
                        else if (key=="move_speed") npc.move_speed = (float)mnum(json, i);
                        else if (key=="wander_interval") npc.wander_interval = (float)mnum(json, i);
                        else if (key=="has_battle") npc.has_battle = mbool(json, i);
                        else if (key=="battle_enemy") npc.battle_enemy_name = mstr(json, i);
                        else if (key=="battle_hp") npc.battle_enemy_hp = (int)mnum(json, i);
                        else if (key=="battle_atk") npc.battle_enemy_atk = (int)mnum(json, i);
                        else if (key=="dialogue") {
                            // Parse dialogue array
                            if (json[i] == '[') {
                                i++;
                                while (i < json.size() && json[i] != ']') {
                                    mskip(json, i);
                                    if (json[i] == ']') break;
                                    if (json[i] == '{') {
                                        i++;
                                        std::string speaker, text;
                                        while (i < json.size() && json[i] != '}') {
                                            mskip(json, i);
                                            if (json[i]=='}') break;
                                            std::string dk = mstr(json, i);
                                            mskip(json, i); if (json[i]==':') i++; mskip(json, i);
                                            if (dk=="speaker") speaker = mstr(json, i);
                                            else if (dk=="text") text = mstr(json, i);
                                            else mskipval(json, i);
                                            mskip(json, i); if (json[i]==',') i++;
                                        }
                                        if (json[i]=='}') i++;
                                        dlg.push_back({speaker, text});
                                    }
                                    mskip(json, i); if (json[i]==',') i++;
                                }
                                if (json[i]==']') i++;
                            }
                        }
                        else mskipval(json, i);
                        mskip(json, i); if (json[i]==',') i++;
                    }
                    if (json[i]=='}') i++;

                    npc.home_pos = npc.position;
                    npc.wander_target = npc.position;
                    npc.dialogue = dlg;
                    if (npc.dialogue.empty()) npc.dialogue = {{npc.name, "..."}};
                    // Validate sprite atlas ID (legacy indexed access)
                    if (npc.sprite_atlas_id >= (int)game.npc_atlases.size()) {
                        npc.sprite_atlas_id = -1;
                    }
                    game.npcs.push_back(npc);
                }
                mskip(json, i); if (json[i]==',') i++;
            }
        }
    }

    // Parse player start position from metadata (fallback to map center)
    float default_x = game.tile_map.width() * game.tile_map.tile_size() * 0.5f;
    float default_y = game.tile_map.height() * game.tile_map.tile_size() * 0.5f;
    game.player_pos = {default_x, default_y};

    size_t meta_pos = json.find("\"player_start_x\"");
    if (meta_pos != std::string::npos) {
        size_t i = json.find(':', meta_pos) + 1;
        mskip(json, i);
        game.player_pos.x = (float)mnum(json, i);
    }
    meta_pos = json.find("\"player_start_y\"");
    if (meta_pos != std::string::npos) {
        size_t i = json.find(':', meta_pos) + 1;
        mskip(json, i);
        game.player_pos.y = (float)mnum(json, i);
    }

    // Ensure player isn't stuck in a solid tile — find nearest walkable position
    if (game.tile_map.is_solid_world(game.player_pos.x, game.player_pos.y)) {
        int ts = game.tile_map.tile_size();
        int mw = game.tile_map.width(), mh = game.tile_map.height();
        bool found = false;
        // Spiral search outward from player's tile
        int px = (int)(game.player_pos.x / ts), py = (int)(game.player_pos.y / ts);
        for (int radius = 1; radius < std::max(mw, mh) && !found; radius++) {
            for (int dx = -radius; dx <= radius && !found; dx++) {
                for (int dy = -radius; dy <= radius && !found; dy++) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                    int tx = px + dx, ty = py + dy;
                    if (tx >= 0 && tx < mw && ty >= 0 && ty < mh) {
                        float wx = tx * ts + ts * 0.5f, wy = ty * ts + ts * 0.5f;
                        if (!game.tile_map.is_solid_world(wx, wy)) {
                            game.player_pos = {wx, wy};
                            found = true;
                        }
                    }
                }
            }
        }
        if (found) {
            std::printf("[Map] Player repositioned to walkable tile (%.0f, %.0f)\n",
                        game.player_pos.x, game.player_pos.y);
        }
    }

    // Update camera bounds for new map size
    game.camera.set_bounds(0, 0, game.tile_map.world_width(), game.tile_map.world_height());
    game.camera.center_on(game.player_pos);

    // Reset party followers to player position
    for (auto& pm : game.party) {
        pm.position = game.player_pos;
        pm.dir = 0; pm.frame = 0; pm.moving = false;
    }
    game.trail_head = 0; game.trail_count = 0;
    for (auto& r : game.trail) { r.pos = game.player_pos; r.dir = 0; }

    std::printf("[Map] Loaded: %s (%dx%d, %d objects, %d npcs)\n",
                path.c_str(), game.tile_map.width(), game.tile_map.height(),
                (int)game.world_objects.size(), (int)game.npcs.size());

    // Execute companion map script if it exists
    if (game.script_engine) {
        // Derive script path: assets/maps/foo.json → assets/scripts/maps/foo.sage
        std::string name = path;
        auto slash = name.rfind('/');
        if (slash != std::string::npos) name = name.substr(slash + 1);
        auto dot = name.rfind('.');
        if (dot != std::string::npos) name = name.substr(0, dot);
        std::string script_path = "assets/scripts/maps/" + name + ".sage";
        auto script_data = eb::FileIO::read_file(script_path);
        if (!script_data.empty()) {
            std::string src(script_data.begin(), script_data.end());
            game.script_engine->execute(src);
            // Try level-specific init first (e.g. forest_init), then generic map_init
            std::string level_init = name + "_init";
            if (game.script_engine->has_function(level_init)) {
                game.script_engine->call_function(level_init);
                std::printf("[Map] Executed %s() from %s\n", level_init.c_str(), script_path.c_str());
            } else if (game.script_engine->has_function("map_init")) {
                game.script_engine->call_function("map_init");
                std::printf("[Map] Executed map_init() from %s\n", script_path.c_str());
            }
        }
    }

    return true;
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

// ─── Battle logic ───

void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id) {
    auto& b = game.battle;
    b.phase = BattlePhase::Intro;
    b.enemy_name = enemy;
    b.enemy_hp_actual = hp; b.enemy_hp_max = hp; b.enemy_atk = atk;
    b.enemy_sprite_id = sprite_id;
    b.sam_hp_actual = game.sam_hp; b.sam_hp_max = game.sam_hp_max;
    b.sam_hp_display = static_cast<float>(game.sam_hp);
    b.sam_atk = game.sam_atk + game.ally_stats.weapon_damage_bonus();
    b.active_fighter = 0; // Player goes first
    b.attack_anim_timer = 0.0f;
    b.player_hp_actual = game.player_hp;
    b.player_hp_max = game.player_hp_max;
    b.player_hp_display = static_cast<float>(game.player_hp);
    // Apply character stat bonuses
    b.player_atk = game.player_atk + game.player_stats.weapon_damage_bonus();
    b.player_def = game.player_def + game.player_stats.defense_bonus();
    b.menu_selection = 0; b.phase_timer = 0.0f;
    b.item_menu_open = false; b.item_menu_selection = 0;
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
        if (b.item_menu_open) {
            // ── Item submenu navigation ──
            auto battle_items = game.inventory.get_battle_items();
            int item_count = (int)battle_items.size();
            if (item_count == 0) {
                b.item_menu_open = false; // No items, close
                break;
            }
            if (up && b.item_menu_selection > 0) b.item_menu_selection--;
            if (down && b.item_menu_selection < item_count - 1) b.item_menu_selection++;
            if (confirm) {
                // Use selected item
                auto* item = battle_items[b.item_menu_selection];
                b.phase_timer = 0.0f; b.attack_anim_timer = 0.0f;

                if (game.script_engine && !item->sage_func.empty() &&
                    game.script_engine->has_function(item->sage_func)) {
                    game.script_engine->sync_battle_to_script();
                    game.script_engine->sync_item_to_script(item->id);
                    game.script_engine->call_function(item->sage_func);
                    game.script_engine->sync_battle_from_script();
                } else {
                    // Fallback: generic item use
                    const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                    if (item->heal_hp > 0) {
                        int heal = item->heal_hp;
                        if (b.active_fighter == 0)
                            b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                        else
                            b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
                        b.message = std::string(fighter) + " uses " + item->name + "! Healed " + std::to_string(heal) + " HP!";
                    } else if (item->damage > 0) {
                        int dmg = item->damage + (game.rng() % 5);
                        b.enemy_hp_actual -= dmg; b.last_damage = dmg;
                        b.message = std::string(fighter) + " uses " + item->name + "! " + std::to_string(dmg) + " damage!";
                    }
                    game.inventory.remove(item->id, 1);
                }
                b.item_menu_open = false;
                b.phase = BattlePhase::PlayerAttack;
            }
        } else {
            // ── Main battle menu: Attack / Items / Defend / Run ──
            if (up && b.menu_selection > 0) b.menu_selection--;
            if (down && b.menu_selection < 3) b.menu_selection++;
            if (confirm) {
                b.phase_timer = 0.0f; b.attack_anim_timer = 0.0f;

                if (b.menu_selection == 0) {
                    // Attack — use SageLang if available
                    if (game.script_engine && game.script_engine->has_function("attack_normal")) {
                        game.script_engine->sync_battle_to_script();
                        game.script_engine->call_function("attack_normal");
                        game.script_engine->sync_battle_from_script();
                    } else {
                        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                        int atk = (b.active_fighter == 0) ? b.player_atk : b.sam_atk;
                        int damage = atk + (game.rng() % 5) - 2;
                        if (damage < 1) damage = 1;
                        b.enemy_hp_actual -= damage; b.last_damage = damage;
                        b.message = std::string(fighter) + " attacks! " + std::to_string(damage) + " damage!";
                    }
                    b.phase = BattlePhase::PlayerAttack;
                } else if (b.menu_selection == 1) {
                    // Items — open item submenu
                    auto battle_items = game.inventory.get_battle_items();
                    if (battle_items.empty()) {
                        b.message = "No items!";
                        // Stay on player turn, don't advance phase
                    } else {
                        b.item_menu_open = true;
                        b.item_menu_selection = 0;
                    }
                } else if (b.menu_selection == 2) {
                    // Defend — use SageLang if available
                    if (game.script_engine && game.script_engine->has_function("defend")) {
                        game.script_engine->sync_battle_to_script();
                        game.script_engine->call_function("defend");
                        game.script_engine->sync_battle_from_script();
                    } else {
                        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                        int heal = 8 + game.rng() % 8;
                        if (b.active_fighter == 0)
                            b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                        else
                            b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
                        b.message = std::string(fighter) + " braces! Recovered " + std::to_string(heal) + " HP.";
                    }
                    b.phase = BattlePhase::PlayerAttack;
                } else {
                    // Run
                    if (b.random_encounter && (game.rng() % 3) != 0) {
                        b.message = "Got away safely!";
                        b.phase = BattlePhase::Victory; b.phase_timer = 0.0f;
                    } else {
                        b.message = "Can't escape!";
                        b.phase = BattlePhase::PlayerAttack;
                    }
                }
            }
        }
        break;

    case BattlePhase::PlayerAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.enemy_hp_actual <= 0) {
                b.phase = BattlePhase::Victory;
                int xp = (int)((b.enemy_hp_max / 2 + b.enemy_atk) * game.xp_multiplier);
                b.message = "Victory! Gained " + std::to_string(xp) + " XP!";
                game.player_xp += xp;
            } else if (b.active_fighter == 0 && b.sam_hp_actual > 0) {
                // Ally's turn next
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
        // Use SageLang enemy AI if available
        bool scripted = false;
        if (game.script_engine) {
            game.script_engine->sync_battle_to_script();
            // Try vampire-specific attack first, then generic
            if (b.enemy_name == "Vampire" && game.script_engine->has_function("vampire_attack")) {
                game.script_engine->call_function("vampire_attack");
                scripted = true;
            } else if (game.script_engine->has_function("enemy_turn")) {
                game.script_engine->call_function("enemy_turn");
                scripted = true;
            }
            if (scripted) game.script_engine->sync_battle_from_script();
        }

        if (!scripted) {
            // Fallback C++ logic
            int target = (game.rng() % 2 == 0 && b.sam_hp_actual > 0) ? 1 : 0;
            if (b.player_hp_actual <= 0) target = 1;
            if (b.sam_hp_actual <= 0) target = 0;
            int def = (target == 0) ? b.player_def : 2;
            int damage = b.enemy_atk + (game.rng() % 5) - 2 - def / 3;
            if (damage < 1) damage = 1;
            if (target == 0) {
                b.player_hp_actual -= damage;
                b.message = b.enemy_name + " attacks Hero! " + std::to_string(damage) + " damage!";
            } else {
                b.sam_hp_actual -= damage;
                b.message = b.enemy_name + " attacks Ally! " + std::to_string(damage) + " damage!";
            }
            b.last_damage = damage;
        }

        b.phase = BattlePhase::EnemyAttack; b.phase_timer = 0.0f;
        b.attack_anim_timer = 0.0f;
        break;
    }

    case BattlePhase::EnemyAttack:
        if (b.phase_timer > 1.2f || confirm) {
            bool player_down = b.player_hp_actual <= 0;
            bool ally_down = b.sam_hp_actual <= 0;
            if (player_down && ally_down) {
                b.player_hp_actual = 0; b.sam_hp_actual = 0;
                b.phase = BattlePhase::Defeat; b.message = "The party has fallen!";
            } else {
                b.active_fighter = 0; b.menu_selection = 0;
                // Skip player if they're down
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

            // Spawn loot drops from loot tables
            eb::Vec2 drop_pos = game.player_pos; // Drop near player
            // Find matching loot table for this enemy
            for (auto& table : game.loot_tables) {
                if (table.enemy_name == b.enemy_name || table.enemy_name == "*") {
                    for (auto& entry : table.entries) {
                        float roll = (game.rng() % 1000) / 1000.0f;
                        if (roll < entry.drop_chance) {
                            WorldDrop wd;
                            wd.item_id = entry.item_id;
                            wd.item_name = entry.item_name;
                            wd.description = entry.description;
                            wd.type = entry.type;
                            wd.heal_hp = entry.heal_hp;
                            wd.damage = entry.damage;
                            wd.element = entry.element;
                            wd.sage_func = entry.sage_func;
                            // Scatter drops around the player
                            float angle = (game.rng() % 628) / 100.0f;
                            float r = 20.0f + (game.rng() % 30);
                            wd.position = {drop_pos.x + std::cos(angle) * r,
                                           drop_pos.y + std::sin(angle) * r};
                            game.world_drops.push_back(wd);
                        }
                    }
                }
            }

            // Call loot_func + remove defeated NPC
            for (int ni = (int)game.npcs.size() - 1; ni >= 0; ni--) {
                auto& npc = game.npcs[ni];
                if (npc.battle_enemy_name == b.enemy_name && npc.has_triggered) {
                    // Call loot func if set
                    if (!npc.loot_func.empty() && game.script_engine) {
                        game.script_engine->set_string("drop_x", std::to_string(drop_pos.x));
                        game.script_engine->set_string("drop_y", std::to_string(drop_pos.y));
                        if (game.script_engine->has_function(npc.loot_func))
                            game.script_engine->call_function(npc.loot_func);
                    }
                    // Check if this NPC is a spawn template — if so, hide instead of delete
                    bool is_template = false;
                    for (auto& loop : game.spawn_loops)
                        if (loop.npc_template_name == npc.name) { is_template = true; break; }
                    if (is_template) {
                        // Hide the template: move off-screen, reset trigger so it can respawn
                        npc.position = {-9999, -9999};
                        npc.home_pos = npc.position;
                        npc.has_triggered = true;
                        npc.schedule.currently_visible = false;
                    } else {
                        game.npcs.erase(game.npcs.begin() + ni);
                    }
                    break;
                }
            }

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
    game.current_input = &input;

    // ── Screen effects decay ──
    if (game.shake_timer > 0) game.shake_timer -= dt;
    if (game.flash_timer > 0) { game.flash_timer -= dt; if (game.flash_timer <= 0) game.flash_a = 0; }
    if (game.fade_timer > 0) {
        game.fade_timer -= dt;
        float t = 1.0f - (game.fade_timer / game.fade_duration);
        game.fade_a = game.fade_a + (game.fade_target - game.fade_a) * std::min(1.0f, t);
    }

    // ── Tween engine ──
    // Apply tween values to targets
    for (auto& tw : game.tween_system.tweens) {
        if (!tw.active) continue;
        if (!tw.started) {
            // Capture start value on first frame
            if (tw.target == "camera") {
                if (tw.property == "x") tw.start_val = game.camera.position().x;
                else if (tw.property == "y") tw.start_val = game.camera.position().y;
            } else if (tw.target == "player") {
                if (tw.property == "x") tw.start_val = game.player_pos.x;
                else if (tw.property == "y") tw.start_val = game.player_pos.y;
                else if (tw.property == "scale") tw.start_val = game.player_sprite_scale;
            } else {
                // UI element
                for (auto& l : game.script_ui.labels) {
                    if (l.id == tw.target) {
                        if (tw.property == "x") tw.start_val = l.position.x;
                        else if (tw.property == "y") tw.start_val = l.position.y;
                        else if (tw.property == "scale") tw.start_val = l.scale;
                        else if (tw.property == "alpha") tw.start_val = l.color.w;
                    }
                }
                for (auto& p : game.script_ui.panels) {
                    if (p.id == tw.target) {
                        if (tw.property == "x") tw.start_val = p.position.x;
                        else if (tw.property == "y") tw.start_val = p.position.y;
                        else if (tw.property == "alpha") tw.start_val = p.opacity;
                        else if (tw.property == "scale") tw.start_val = p.scale;
                    }
                }
                for (auto& img : game.script_ui.images) {
                    if (img.id == tw.target) {
                        if (tw.property == "x") tw.start_val = img.position.x;
                        else if (tw.property == "y") tw.start_val = img.position.y;
                        else if (tw.property == "alpha") tw.start_val = img.opacity;
                        else if (tw.property == "scale") tw.start_val = img.scale;
                    }
                }
            }
            tw.started = true;
        }

        float v = tw.value();
        // Apply to target
        if (tw.target == "camera") {
            auto pos = game.camera.position();
            if (tw.property == "x") game.camera.set_position({v, pos.y});
            else if (tw.property == "y") game.camera.set_position({pos.x, v});
        } else if (tw.target == "player") {
            if (tw.property == "x") game.player_pos.x = v;
            else if (tw.property == "y") game.player_pos.y = v;
            else if (tw.property == "scale") game.player_sprite_scale = v;
        } else {
            // UI elements
            for (auto& l : game.script_ui.labels) {
                if (l.id == tw.target) {
                    if (tw.property == "x") l.position.x = v;
                    else if (tw.property == "y") l.position.y = v;
                    else if (tw.property == "scale") l.scale = v;
                    else if (tw.property == "alpha") l.color.w = v;
                }
            }
            for (auto& p : game.script_ui.panels) {
                if (p.id == tw.target) {
                    if (tw.property == "x") p.position.x = v;
                    else if (tw.property == "y") p.position.y = v;
                    else if (tw.property == "alpha") p.opacity = v;
                    else if (tw.property == "scale") p.scale = v;
                }
            }
            for (auto& img : game.script_ui.images) {
                if (img.id == tw.target) {
                    if (tw.property == "x") img.position.x = v;
                    else if (tw.property == "y") img.position.y = v;
                    else if (tw.property == "alpha") img.opacity = v;
                    else if (tw.property == "scale") img.scale = v;
                }
            }
        }
    }
    auto tween_callbacks = game.tween_system.update(dt);

    // ── Particle system ──
    for (auto& emitter : game.emitters) emitter.update(dt, game.rng);
    game.emitters.erase(std::remove_if(game.emitters.begin(), game.emitters.end(),
        [](const eb::ParticleEmitter& e) { return e.all_dead(); }), game.emitters.end());

    // ── Screen transition ──
    {
        auto cb = game.transition.update(dt);
        if (!cb.empty()) tween_callbacks.push_back(cb);
    }

    // ── Playtime tracking ──
    game.playtime_seconds += dt;

    // ── Debug stats ──
    static float fps_timer = 0;
    static int fps_frames = 0;
    fps_timer += dt;
    fps_frames++;
    if (fps_timer >= 0.5f) {
        game.debug_fps = fps_frames / fps_timer;
        fps_timer = 0;
        fps_frames = 0;
    }
    game.debug_particle_count = 0;
    for (auto& e : game.emitters)
        for (auto& p : e.particles) if (p.alive) game.debug_particle_count++;

    // ── World systems (always tick, even during menus) ──
    eb::update_day_night(game.day_night, dt);

    // Survival stats
    if (game.survival.enabled) {
        float minutes_elapsed = dt * game.day_night.day_speed / 60.0f;
        eb::update_survival(game.survival, minutes_elapsed,
                            game.player_speed, game.player_hp, 120.0f);
    }

    // Spawn loops
    eb::update_spawn_loops(game, dt);

    // Script UI notification timers
    for (auto& n : game.script_ui.notifications) n.timer += dt;
    game.script_ui.notifications.erase(
        std::remove_if(game.script_ui.notifications.begin(), game.script_ui.notifications.end(),
                        [](auto& n) { return n.timer >= n.duration; }),
        game.script_ui.notifications.end());

    // ── Weather system update ──
    {
        auto& w = game.weather;
        float sw = game.hud.screen_w, sh = game.hud.screen_h;

        // Wind affects rain angle and snow drift
        float wind_rad = w.wind_direction * 3.14159f / 180.0f;
        float wind_x = std::sin(wind_rad) * w.wind_strength;

        // Rain update
        if (w.rain_active) {
            int target = (int)(w.rain_max_drops * w.rain_intensity);
            while ((int)w.rain_drops.size() < target) {
                RainDrop d;
                d.x = (float)(game.rng() % (int)(sw + 200)) - 100;
                d.y = -(float)(game.rng() % (int)sh);
                d.speed = w.rain_speed * (0.8f + (game.rng() % 40) / 100.0f);
                d.length = 8.0f + (game.rng() % 12);
                d.opacity = w.rain_opacity * (0.6f + (game.rng() % 40) / 100.0f);
                w.rain_drops.push_back(d);
            }
            float angle_rad = (w.rain_angle + wind_x * 30.0f) * 3.14159f / 180.0f;
            for (auto& d : w.rain_drops) {
                d.y += d.speed * dt;
                d.x += std::sin(angle_rad) * d.speed * dt;
                if (d.y > sh + 20) {
                    d.y = -(float)(game.rng() % 60);
                    d.x = (float)(game.rng() % (int)(sw + 200)) - 100;
                }
            }
        } else {
            w.rain_drops.clear();
        }

        // Snow update
        if (w.snow_active) {
            int target = (int)(w.snow_max_flakes * w.snow_intensity);
            while ((int)w.snow_flakes.size() < target) {
                SnowFlake f;
                f.x = (float)(game.rng() % (int)sw);
                f.y = -(float)(game.rng() % (int)sh);
                f.speed = w.snow_speed * (0.5f + (game.rng() % 50) / 100.0f);
                f.drift = w.snow_drift * (0.5f + (game.rng() % 50) / 100.0f);
                f.size = 1.5f + (game.rng() % 20) / 10.0f;
                f.phase = (game.rng() % 628) / 100.0f;
                f.opacity = (0.5f + (game.rng() % 50) / 100.0f);
                w.snow_flakes.push_back(f);
            }
            for (auto& f : w.snow_flakes) {
                f.y += f.speed * dt;
                f.x += std::sin(f.phase + game.game_time * 1.5f) * f.drift * dt + wind_x * 40.0f * dt;
                if (f.y > sh + 10) {
                    f.y = -(float)(game.rng() % 30);
                    f.x = (float)(game.rng() % (int)sw);
                }
                if (f.x < -10) f.x = sw + 5;
                if (f.x > sw + 10) f.x = -5;
            }
        } else {
            w.snow_flakes.clear();
        }

        // Lightning update
        if (w.lightning_active) {
            w.lightning_timer += dt;
            if (w.lightning_timer >= w.lightning_interval) {
                w.lightning_timer = 0;
                if ((game.rng() % 100) < (int)(w.lightning_chance * 100)) {
                    // Create a lightning bolt
                    LightningBolt bolt;
                    bolt.x1 = (float)(game.rng() % (int)sw);
                    bolt.y1 = 0;
                    bolt.x2 = bolt.x1 + (game.rng() % 200) - 100;
                    bolt.y2 = sh * (0.4f + (game.rng() % 40) / 100.0f);
                    bolt.timer = w.lightning_flash_duration;
                    bolt.brightness = 0.8f + (game.rng() % 20) / 100.0f;
                    // Generate fork segments
                    float cx = bolt.x1, cy = bolt.y1;
                    int segs = 8 + game.rng() % 6;
                    float dx = (bolt.x2 - bolt.x1) / segs;
                    float dy = (bolt.y2 - bolt.y1) / segs;
                    for (int s = 0; s < segs; s++) {
                        float jx = (game.rng() % 40) - 20.0f;
                        float nx = cx + dx + jx;
                        float ny = cy + dy;
                        bolt.segments.push_back({nx, ny});
                        cx = nx; cy = ny;
                    }
                    w.bolts.push_back(bolt);
                    // Screen flash
                    game.flash_r = 0.9f; game.flash_g = 0.9f; game.flash_b = 1.0f;
                    game.flash_a = bolt.brightness * 0.6f;
                    game.flash_timer = w.lightning_flash_duration;
                }
            }
            // Decay bolts
            for (auto& b : w.bolts) b.timer -= dt;
            w.bolts.erase(std::remove_if(w.bolts.begin(), w.bolts.end(),
                [](auto& b) { return b.timer <= 0; }), w.bolts.end());
        } else {
            w.bolts.clear();
        }

        // Cloud shadows update
        if (w.clouds_active) {
            while ((int)w.cloud_shadows.size() < w.cloud_max) {
                CloudShadow c;
                c.x = (float)(game.rng() % (int)(sw * 2)) - sw * 0.5f;
                c.y = (float)(game.rng() % (int)(sh * 2)) - sh * 0.5f;
                c.radius = 60.0f + (game.rng() % 120);
                c.opacity = w.cloud_shadow_opacity * (0.5f + (game.rng() % 50) / 100.0f);
                float dir_rad = w.cloud_direction * 3.14159f / 180.0f;
                c.speed_x = std::cos(dir_rad) * w.cloud_speed * (0.7f + (game.rng() % 30) / 100.0f);
                c.speed_y = std::sin(dir_rad) * w.cloud_speed * (0.7f + (game.rng() % 30) / 100.0f);
                w.cloud_shadows.push_back(c);
            }
            for (auto& c : w.cloud_shadows) {
                c.x += (c.speed_x + wind_x * 15.0f) * dt;
                c.y += c.speed_y * dt;
                // Wrap around
                if (c.x > sw * 1.5f) c.x = -c.radius * 2;
                if (c.x < -c.radius * 3) c.x = sw * 1.2f;
                if (c.y > sh * 1.5f) c.y = -c.radius * 2;
                if (c.y < -c.radius * 3) c.y = sh * 1.2f;
            }
        } else {
            w.cloud_shadows.clear();
        }

        // God ray sway animation
        if (w.god_rays_active) {
            w.god_ray_sway = std::sin(game.game_time * 0.3f) * 15.0f;
        }
    }

    // Pause menu toggle (ESC / Menu)
    if (input.is_pressed(eb::InputAction::Menu)) {
        game.paused = !game.paused;
        game.pause_selection = 0;
    }

    // Pause menu input (keyboard + mouse)
    static constexpr int PAUSE_ITEM_COUNT = 6;
    // 0=Resume, 1=Editor, 2=Levels, 3=Reset, 4=Settings, 5=Quit
    if (game.paused) {
        // Level selector sub-menu
        if (game.level_select_open) {
            if (input.is_pressed(eb::InputAction::MoveUp) && game.level_select_cursor > 0)
                game.level_select_cursor--;
            if (input.is_pressed(eb::InputAction::MoveDown) && game.level_select_cursor < (int)game.level_select_ids.size() - 1)
                game.level_select_cursor++;
            if (input.is_pressed(eb::InputAction::Confirm) && !game.level_select_ids.empty()) {
                auto& id = game.level_select_ids[game.level_select_cursor];
                if (game.level_manager && game.level_manager->is_loaded(id)) {
                    game.level_manager->switch_level(id, game);
                    game.camera.center_on(game.player_pos);
                }
                game.level_select_open = false;
                game.paused = false;
            }
            if (input.is_pressed(eb::InputAction::Cancel)) {
                game.level_select_open = false;
            }
            return;
        }

        if (input.is_pressed(eb::InputAction::MoveUp) && game.pause_selection > 0)
            game.pause_selection--;
        if (input.is_pressed(eb::InputAction::MoveDown) && game.pause_selection < PAUSE_ITEM_COUNT - 1)
            game.pause_selection++;

        auto execute_pause_action = [&](int i) {
            switch (i) {
                case 0: game.paused = false; break;
                case 1: game.paused = false; game.pause_request_editor = true; break;
                case 2: { // Levels
                    game.level_select_ids.clear();
                    if (game.level_manager) {
                        for (auto& [lid, lvl] : game.level_manager->levels)
                            if (lvl.loaded) game.level_select_ids.push_back(lid);
                    }
                    game.level_select_cursor = 0;
                    game.level_select_open = true;
                    break;
                }
                case 3: game.pause_request_reset = true; game.paused = false; break;
                case 4: break; // Settings placeholder
                case 5: game.pause_request_quit = true; break;
            }
        };

        // Mouse hover and click — use script UI label positions
        // Convert native touch/mouse coords to UI virtual coords
        float mx = input.mouse.x * (game.hud.screen_w / game.hud.native_w);
        float my = input.mouse.y * (game.hud.screen_h / game.hud.native_h);
        static const char* pause_ids[PAUSE_ITEM_COUNT] = {"pause_item_0","pause_item_1","pause_item_2","pause_item_3","pause_item_4","pause_item_5"};
        for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
            for (auto& l : game.script_ui.labels) {
                if (l.id != pause_ids[i]) continue;
                float hit_w = 260, hit_h = 36;
                float lx = l.position.x - 20, ly = l.position.y - 4;
                if (mx >= lx && mx <= lx + hit_w && my >= ly && my <= ly + hit_h) {
                    game.pause_selection = i;
                    if (input.mouse.is_pressed(eb::MouseButton::Left))
                        execute_pause_action(i);
                }
                break;
            }
        }

        // Keyboard confirm
        if (input.is_pressed(eb::InputAction::Confirm))
            execute_pause_action(game.pause_selection);
        if (input.is_pressed(eb::InputAction::Cancel)) {
            game.paused = false;
        }
        return;
    }

    // Battle mode
    if (game.battle.phase != BattlePhase::None) {
        update_battle(game, dt,
                      input.is_pressed(eb::InputAction::Confirm),
                      input.is_pressed(eb::InputAction::MoveUp),
                      input.is_pressed(eb::InputAction::MoveDown));
        return;
    }

    // Merchant UI mode
    if (game.merchant_ui.is_open()) {
        game.merchant_ui.update(game,
            input.is_pressed(eb::InputAction::MoveUp),
            input.is_pressed(eb::InputAction::MoveDown),
            input.is_pressed(eb::InputAction::MoveLeft),
            input.is_pressed(eb::InputAction::MoveRight),
            input.is_pressed(eb::InputAction::Confirm),
            input.is_pressed(eb::InputAction::Cancel),
            dt);
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
            game.battle.enemy_sprite_key = npc.sprite_atlas_key;
            start_battle(game, npc.battle_enemy_name,
                         npc.battle_enemy_hp, npc.battle_enemy_atk, false,
                         npc.sprite_atlas_id);
            game.pending_battle_npc = -1;
        }
        return;
    }

    // ── Inventory quick-use (Cancel/X toggles, Left/Right browses, Confirm uses) ──
    if (game.hud.inv_use_cooldown > 0) game.hud.inv_use_cooldown -= dt;
    if (input.is_pressed(eb::InputAction::Cancel)) {
        game.hud.inv_open = !game.hud.inv_open;
        if (game.hud.inv_open) game.hud.inv_selected = 0;
    }
    if (game.hud.inv_open && !game.inventory.items.empty()) {
        int max_idx = std::min(game.hud.inv_max_slots, (int)game.inventory.items.size()) - 1;
        if (input.is_pressed(eb::InputAction::MoveRight) && game.hud.inv_selected < max_idx)
            game.hud.inv_selected++;
        if (input.is_pressed(eb::InputAction::MoveLeft) && game.hud.inv_selected > 0)
            game.hud.inv_selected--;
        // Clamp if items were removed
        if (game.hud.inv_selected > max_idx) game.hud.inv_selected = max_idx;

        // Use item on Confirm
        if (input.is_pressed(eb::InputAction::Confirm) && game.hud.inv_use_cooldown <= 0) {
            int idx = game.hud.inv_selected;
            if (idx >= 0 && idx < (int)game.inventory.items.size()) {
                auto& item = game.inventory.items[idx];
                bool used = false;
                // Direct healing (no sage_func needed)
                if (item.heal_hp > 0 && game.player_hp < game.player_hp_max) {
                    game.player_hp = std::min(game.player_hp + item.heal_hp, game.player_hp_max);
                    used = true;
                }
                // Call sage_func if defined (for custom item effects)
                if (!item.sage_func.empty() && game.script_engine) {
                    if (game.script_engine->has_function(item.sage_func)) {
                        game.script_engine->call_function(item.sage_func);
                        used = true;
                    }
                }
                if (used) {
                    std::string msg = "Used " + item.name;
                    game.script_ui.notifications.push_back({msg, 2.0f, 0.0f});
                    game.inventory.remove(item.id, 1);
                    if (game.hud.inv_selected >= (int)game.inventory.items.size())
                        game.hud.inv_selected = std::max(0, (int)game.inventory.items.size() - 1);
                    game.hud.inv_use_cooldown = 0.3f;
                }
                if (game.inventory.items.empty()) game.hud.inv_open = false;
            }
        }
        // While inventory is open, don't move the player
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

    // ── NPC-NPC separation (spatial grid — O(n) average) ──
    static constexpr float NPC_SEP_DIST = 40.0f;
    static constexpr float NPC_SEP_FORCE = 200.0f;
    static constexpr float GRID_CELL = NPC_SEP_DIST;  // Cell size matches check radius
    {
        // Build spatial grid of visible NPC indices
        std::unordered_map<int64_t, std::vector<int>> sep_grid;
        auto cell_key = [](int cx, int cy) -> int64_t { return ((int64_t)cx << 32) | (uint32_t)cy; };
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            if (!game.npcs[i].schedule.currently_visible) continue;
            int cx = (int)(game.npcs[i].position.x / GRID_CELL);
            int cy = (int)(game.npcs[i].position.y / GRID_CELL);
            sep_grid[cell_key(cx, cy)].push_back(i);
        }
        // Check neighbors in 3x3 grid around each NPC
        for (auto& [key, indices] : sep_grid) {
            int cx0 = (int)(key >> 32), cy0 = (int)(key & 0xFFFFFFFF);
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    auto nit = sep_grid.find(cell_key(cx0+dx, cy0+dy));
                    if (nit == sep_grid.end()) continue;
                    for (int i : indices) {
                        for (int j : nit->second) {
                            if (j <= i) continue;
                            auto& a = game.npcs[i];
                            auto& b = game.npcs[j];
                            float sx = a.position.x - b.position.x;
                            float sy = a.position.y - b.position.y;
                            float sd2 = sx*sx + sy*sy;
                            if (sd2 > NPC_SEP_DIST * NPC_SEP_DIST || sd2 < 0.01f) {
                                if (sd2 < 0.01f) {
                                    float angle = (game.rng() % 628) / 100.0f;
                                    a.position.x += std::cos(angle) * 8.0f;
                                    a.position.y += std::sin(angle) * 8.0f;
                                }
                                continue;
                            }
                            float sd = std::sqrt(sd2);
                            float overlap = 1.0f - (sd / NPC_SEP_DIST);
                            float push = overlap * overlap * NPC_SEP_FORCE * dt;
                            float nx = sx / sd, ny = sy / sd;
                            a.position.x += nx * push; a.position.y += ny * push;
                            b.position.x -= nx * push; b.position.y -= ny * push;
                        }
                    }
                }
            }
        }
    }

    // ── Despawn hostile mobs at day ──
    if (game.day_night.game_hours >= 6.0f && game.day_night.game_hours < 7.0f) {
        for (int i = (int)game.npcs.size() - 1; i >= 0; i--) {
            auto& npc = game.npcs[i];
            if (!npc.despawn_at_day || !npc.hostile) continue;
            if (!npc.schedule.currently_visible) continue; // Already hidden

            // Drop loot from loot tables before despawning
            for (auto& table : game.loot_tables) {
                if (table.enemy_name == npc.battle_enemy_name || table.enemy_name == "*") {
                    for (auto& entry : table.entries) {
                        float roll = (game.rng() % 1000) / 1000.0f;
                        if (roll < entry.drop_chance) {
                            WorldDrop wd;
                            wd.item_id = entry.item_id; wd.item_name = entry.item_name;
                            wd.description = entry.description; wd.type = entry.type;
                            wd.heal_hp = entry.heal_hp; wd.damage = entry.damage;
                            wd.element = entry.element; wd.sage_func = entry.sage_func;
                            float angle = (game.rng() % 628) / 100.0f;
                            float r = 10.0f + (game.rng() % 20);
                            wd.position = {npc.position.x + std::cos(angle) * r,
                                           npc.position.y + std::sin(angle) * r};
                            game.world_drops.push_back(wd);
                        }
                    }
                }
            }

            // Call loot_func if set
            if (!npc.loot_func.empty() && game.script_engine) {
                game.script_engine->set_string("drop_x", std::to_string(npc.position.x));
                game.script_engine->set_string("drop_y", std::to_string(npc.position.y));
                if (game.script_engine->has_function(npc.loot_func))
                    game.script_engine->call_function(npc.loot_func);
            }

            // Don't delete spawn templates — just hide them
            bool is_template = false;
            for (auto& loop : game.spawn_loops)
                if (loop.npc_template_name == npc.name) { is_template = true; break; }

            if (is_template) {
                npc.position = {-9999, -9999};
                npc.home_pos = npc.position;
                npc.schedule.currently_visible = false;
                npc.has_triggered = false;
            } else {
                game.npcs.erase(game.npcs.begin() + i);
            }
        }
    }

    // ── World item drops: pickup + lifetime ──
    for (int di = (int)game.world_drops.size() - 1; di >= 0; di--) {
        auto& drop = game.world_drops[di];
        drop.anim_timer += dt;
        drop.lifetime -= dt;
        if (drop.lifetime <= 0) { game.world_drops.erase(game.world_drops.begin() + di); continue; }

        // Check player pickup
        float pdx = game.player_pos.x - drop.position.x;
        float pdy = game.player_pos.y - drop.position.y;
        float pdist = std::sqrt(pdx*pdx + pdy*pdy);
        if (pdist < drop.pickup_radius) {
            game.inventory.add(drop.item_id, drop.item_name, 1, drop.type, drop.description,
                               drop.heal_hp, drop.damage, drop.element, drop.sage_func);
            game.script_ui.notifications.push_back({"Picked up " + drop.item_name, 2.0f, 0.0f});
            game.world_drops.erase(game.world_drops.begin() + di);
        }
    }

    // ── NPC AI update ──
    int ts = game.tile_map.tile_size();
    for (int i = 0; i < (int)game.npcs.size(); i++) {
        auto& npc = game.npcs[i];

        // Schedule check: hide NPCs outside their active hours
        if (npc.schedule.has_schedule) {
            bool in_range = eb::is_hour_in_range(game.day_night.game_hours,
                                                  npc.schedule.start_hour, npc.schedule.end_hour);
            if (in_range && !npc.schedule.currently_visible) {
                npc.position = npc.schedule.spawn_point;
                npc.home_pos = npc.schedule.spawn_point;
                npc.wander_target = npc.position;
                npc.schedule.currently_visible = true;
            } else if (!in_range) {
                npc.schedule.currently_visible = false;
                npc.moving = false;
                continue;
            }
        }

        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        // Priority 1: Hostile chase
        if (npc.hostile && !npc.has_triggered && dist < npc.aggro_range) {
            npc.aggro_active = true;
            float nx = dx / dist, ny = dy / dist;
            npc.position.x += nx * npc.move_speed * dt;
            npc.position.y += ny * npc.move_speed * dt;
            npc.moving = true;
            if (std::abs(dx) > std::abs(dy))
                npc.dir = (dx > 0) ? 3 : 2;
            else
                npc.dir = (dy > 0) ? 0 : 1;
            if (dist < npc.attack_range) {
                npc.has_triggered = true;
                npc.aggro_active = false;
                npc.moving = false;
                game.dialogue.start(npc.dialogue);
                game.pending_battle_npc = npc.has_battle ? i : -1;
            }
        }
        // Priority 2: Route following (non-blocking, each NPC fully independent)
        else if (npc.route.active && !npc.route.waypoints.empty()) {
            npc.moving = true;
            npc.route.stuck_timer += dt;

            auto& wp = npc.route.waypoints[npc.route.current_waypoint];
            float rx = wp.x - npc.position.x, ry = wp.y - npc.position.y;
            float rd = std::sqrt(rx*rx + ry*ry);

            // Advance waypoint helper
            auto advance_waypoint = [&]() {
                npc.route.stuck_timer = 0;
                npc.route.pathfind_failures = 0;
                npc.path_active = false;
                npc.current_path.clear();
                int wps = (int)npc.route.waypoints.size();
                if (npc.route.mode == RouteMode::Patrol) {
                    npc.route.current_waypoint = (npc.route.current_waypoint + 1) % wps;
                } else if (npc.route.mode == RouteMode::Once) {
                    if (npc.route.current_waypoint + 1 < wps) npc.route.current_waypoint++;
                    else { npc.route.active = false; npc.moving = false; }
                } else if (npc.route.mode == RouteMode::PingPong) {
                    if (npc.route.forward) {
                        if (npc.route.current_waypoint + 1 < wps) npc.route.current_waypoint++;
                        else { npc.route.forward = false; if (npc.route.current_waypoint > 0) npc.route.current_waypoint--; }
                    } else {
                        if (npc.route.current_waypoint > 0) npc.route.current_waypoint--;
                        else { npc.route.forward = true; if (wps > 1) npc.route.current_waypoint++; }
                    }
                }
            };

            // Reached waypoint — advance
            if (rd <= 12.0f) {
                advance_waypoint();
            }
            // Stuck too long — skip to next waypoint
            else if (npc.route.stuck_timer > npc.route.stuck_timeout) {
                advance_waypoint();
            }
            // Move toward waypoint
            else {
                // Use direct movement (simple and reliable, works with separation forces)
                // Only use A* for long distances with obstacles
                bool use_pathfind = (rd > 120.0f) && (npc.route.pathfind_failures < 3);

                if (use_pathfind && !npc.path_active) {
                    int sx2 = (int)(npc.position.x / ts), sy2 = (int)(npc.position.y / ts);
                    int ex2 = (int)(wp.x / ts), ey2 = (int)(wp.y / ts);
                    npc.current_path = eb::find_path(game.tile_map, sx2, sy2, ex2, ey2);
                    npc.path_index = 0;
                    npc.path_active = !npc.current_path.empty();
                    if (!npc.path_active) npc.route.pathfind_failures++;
                }

                if (npc.path_active && !npc.current_path.empty()) {
                    // Follow A* path
                    auto& target = npc.current_path[npc.path_index];
                    float ptx = target.x * ts + ts * 0.5f;
                    float pty = target.y * ts + ts * 0.5f;
                    float px = ptx - npc.position.x, py = pty - npc.position.y;
                    float pd = std::sqrt(px*px + py*py);
                    if (pd > 2.0f) {
                        float mx = (px/pd) * npc.move_speed * dt;
                        float my = (py/pd) * npc.move_speed * dt;
                        npc.position.x += mx; npc.position.y += my;
                        if (std::abs(px) > std::abs(py)) npc.dir = (px > 0) ? 3 : 2;
                        else npc.dir = (py > 0) ? 0 : 1;
                    } else {
                        npc.path_index++;
                        if (npc.path_index >= (int)npc.current_path.size()) {
                            npc.path_active = false;
                            npc.current_path.clear();
                        }
                    }
                } else {
                    // Direct movement toward waypoint (always works)
                    float mx = (rx/rd) * npc.move_speed * dt;
                    float my = (ry/rd) * npc.move_speed * dt;
                    // Collision check
                    float new_x = npc.position.x + mx;
                    float new_y = npc.position.y + my;
                    if (!game.tile_map.is_solid_world(new_x, npc.position.y)) npc.position.x = new_x;
                    if (!game.tile_map.is_solid_world(npc.position.x, new_y)) npc.position.y = new_y;
                    if (std::abs(rx) > std::abs(ry)) npc.dir = (rx > 0) ? 3 : 2;
                    else npc.dir = (ry > 0) ? 0 : 1;
                }
            }
        }
        // Priority 2b: One-shot path following (from npc_move_to, no route)
        else if (npc.path_active && !npc.current_path.empty()) {
            auto& target = npc.current_path[npc.path_index];
            float ptx = target.x * ts + ts * 0.5f;
            float pty = target.y * ts + ts * 0.5f;
            float px = ptx - npc.position.x, py = pty - npc.position.y;
            float pd = std::sqrt(px*px + py*py);
            if (pd > 2.0f) {
                npc.position.x += (px/pd) * npc.move_speed * dt;
                npc.position.y += (py/pd) * npc.move_speed * dt;
                npc.moving = true;
                if (std::abs(px) > std::abs(py)) npc.dir = (px > 0) ? 3 : 2;
                else npc.dir = (py > 0) ? 0 : 1;
            } else {
                npc.path_index++;
                if (npc.path_index >= (int)npc.current_path.size()) {
                    npc.path_active = false;
                    npc.current_path.clear();
                    npc.moving = false;
                }
            }
        }
        // Priority 4: Idle wander (with collision check)
        else if (!npc.hostile || npc.has_triggered) {
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
                float new_x = npc.position.x + (wx/wd) * npc.move_speed * dt;
                float new_y = npc.position.y + (wy/wd) * npc.move_speed * dt;
                // Collision check before moving
                if (!game.tile_map.is_solid_world(new_x, new_y))
                    { npc.position.x = new_x; npc.position.y = new_y; }
                npc.moving = true;
                if (std::abs(wx) > std::abs(wy)) npc.dir = (wx > 0) ? 3 : 2;
                else npc.dir = (wy > 0) ? 0 : 1;
            } else {
                npc.moving = false;
            }
        }
        // Animation
        if (npc.moving) {
            npc.anim_timer += dt;
            if (npc.anim_timer >= 0.2f) { npc.anim_timer -= 0.2f; npc.frame = 1 - npc.frame; }
        } else {
            npc.frame = 0; npc.anim_timer = 0.0f;
        }
    }

    // ── NPC-to-NPC meet triggers ──
    for (auto& trigger : game.npc_meet_triggers) {
        if (trigger.fired && !trigger.repeatable) continue;
        NPC* a = nullptr; NPC* b = nullptr;
        for (auto& n : game.npcs) {
            if (n.name == trigger.npc1_name) a = &n;
            if (n.name == trigger.npc2_name) b = &n;
        }
        if (!a || !b) continue;
        if (!a->schedule.currently_visible || !b->schedule.currently_visible) continue;
        float mdx = b->position.x - a->position.x, mdy = b->position.y - a->position.y;
        float mdist = std::sqrt(mdx*mdx + mdy*mdy);
        if (mdist < trigger.trigger_radius) {
            trigger.fired = true;
            if (game.script_engine && game.script_engine->has_function(trigger.callback_func))
                game.script_engine->call_function(trigger.callback_func);
        }
    }

    // Manual NPC interaction (Z/A button for friendly NPCs)
    if (input.is_pressed(eb::InputAction::Confirm)) {
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            auto& npc = game.npcs[i];
            if (npc.hostile && !npc.has_triggered) continue;
            float dx = game.player_pos.x - npc.position.x;
            float dy = game.player_pos.y - npc.position.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < npc.interact_radius) {
                // All dialogue driven through SageLang scripts
                // Try npc-name-specific function first, then generic "greeting"
                bool handled = false;
                if (game.script_engine) {
                    // Build lowercase NPC name for function lookup
                    std::string npc_lower = npc.name;
                    for (auto& c : npc_lower) c = std::tolower(c);
                    // Replace spaces/special chars with underscore
                    for (auto& c : npc_lower) if (c == ' ' || c == '?' || c == '!') c = '_';

                    // Check if this NPC has a shop (e.g. "merchant_shop_items")
                    std::string shop_func = npc_lower + "_shop_items";
                    if (game.script_engine->has_function(shop_func)) {
                        game.script_engine->call_function(shop_func);
                        handled = true;
                    }
                    // Try: npc_name_greeting (e.g. "merchant_greeting")
                    else {
                        std::string specific = npc_lower + "_greeting";
                        if (game.script_engine->has_function(specific)) {
                            game.script_engine->call_function(specific);
                            handled = true;
                        }
                        // Try: just "greeting" (last-loaded script wins)
                        else if (game.script_engine->has_function("greeting")) {
                            game.script_engine->call_function("greeting");
                            handled = true;
                        }
                    }
                }
                // Fallback: use static dialogue lines (from .dialogue file or default)
                if (!handled && !npc.dialogue.empty()) {
                    game.dialogue.start(npc.dialogue);
                }
                game.pending_battle_npc = npc.has_battle ? i : -1;
                break;
            }
        }
    }

    // ── Portal auto-transition ──
    if (game.level_manager) {
        int ts = game.tile_map.tile_size();
        if (ts > 0) {
            int ptx = (int)(game.player_pos.x / ts);
            int pty = (int)(game.player_pos.y / ts);
            if (game.tile_map.collision_at(ptx, pty) == eb::CollisionType::Portal) {
                auto* portal = game.tile_map.get_portal_at(ptx, pty);
                if (portal && !portal->target_map.empty()) {
                    std::string target = portal->target_map;
                    float tx = portal->target_x * (float)ts;
                    float ty = portal->target_y * (float)ts;
                    // Load target level if not cached
                    if (!game.level_manager->is_loaded(target)) {
                        game.level_manager->load_level(target, "assets/maps/" + target, game);
                    }
                    game.level_manager->switch_level_at(target, tx, ty, game);
                    game.script_ui.notifications.push_back({"Entered: " + target, 2.0f, 0.0f});
                }
            }
        }

        // Tick background levels
        game.level_manager->tick_background(game, dt);
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
        if (obj.sprite_id > 4 || obj.sprite_id < 0 || obj.sprite_id >= (int)game.object_defs.size()) continue;
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

    if (!enemy_flash) {
        eb::TextureAtlas* battle_atlas = nullptr;
        VkDescriptorSet battle_desc = VK_NULL_HANDLE;
        if (!b.enemy_sprite_key.empty()) {
            auto it = game.atlas_cache.find(b.enemy_sprite_key);
            if (it != game.atlas_cache.end()) {
                battle_atlas = it->second.get();
                battle_desc = game.atlas_descs[b.enemy_sprite_key];
            }
        }
        if (!battle_atlas && b.enemy_sprite_id >= 0 && b.enemy_sprite_id < (int)game.npc_atlases.size()) {
            battle_atlas = game.npc_atlases[b.enemy_sprite_id].get();
            battle_desc = game.npc_descs[b.enemy_sprite_id];
        }
        if (battle_atlas && battle_desc) {
            auto sr = get_character_sprite(*battle_atlas, 0, false, 0);
            batch.set_texture(battle_desc);
            batch.draw_quad({enemy_cx - ew*0.5f, enemy_y}, {ew, eh}, sr.uv_min, sr.uv_max);
        }
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

    // ── Player + Ally sprites (lower area, backs to us) ──
    float party_y = sh * 0.42f;
    float party_cx = sw * 0.5f;
    float player_x = party_cx - sprite_w - 10.0f;
    float ally_x = party_cx + 10.0f;

    // Attack animation: lunge forward during PlayerAttack phase
    float player_offset_y = 0, ally_offset_y = 0;
    if (b.phase == BattlePhase::PlayerAttack && b.attack_anim_timer < 0.5f) {
        float t = b.attack_anim_timer / 0.5f;
        float lunge = std::sin(t * 3.14159f) * 30.0f; // Lunge forward and back
        if (b.active_fighter == 0) player_offset_y = -lunge;
        else ally_offset_y = -lunge;
    }

    // Player (left) — flash red if hit
    bool player_hit = (b.phase == BattlePhase::EnemyAttack &&
                     b.message.find("Hero") != std::string::npos &&
                     b.attack_anim_timer < 0.6f &&
                     std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!player_hit && b.player_hp_actual > 0 && game.dean_atlas) {
        bool player_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 0);
        int player_frame = player_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.dean_atlas, 1, player_attacking, player_frame);
        batch.set_texture(game.dean_desc);
        batch.draw_quad({player_x, party_y + player_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // Ally (right)
    bool ally_hit = (b.phase == BattlePhase::EnemyAttack &&
                    b.message.find("Ally") != std::string::npos &&
                    b.attack_anim_timer < 0.6f &&
                    std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!ally_hit && b.sam_hp_actual > 0 && game.sam_atlas) {
        bool ally_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 1);
        int ally_frame = ally_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.sam_atlas, 1, ally_attacking, ally_frame);
        batch.set_texture(game.sam_desc);
        batch.draw_quad({ally_x, party_y + ally_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // ── Party stats (bottom right) ──
    float pbx = sw - 260, pby = sh - 100;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx,pby},{240,92},{0,0},{1,1},{0.08f,0.08f,0.18f,0.9f});
    batch.draw_quad({pbx,pby},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});
    batch.draw_quad({pbx,pby+90},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});

    // Player HP
    eb::Vec4 player_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 0)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Hero",{pbx+8,pby+4},player_name_col,0.7f);
    float dhp = b.player_hp_display;
    float dp = std::max(0.0f,dhp/b.player_hp_max);
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+60,pby+8},{120,10},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 dc=dp>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:dp>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+60,pby+8},{120*dp,10},{0,0},{1,1},dc);
    char dhs[32]; std::snprintf(dhs,sizeof(dhs),"%d/%d",(int)std::ceil(dhp),b.player_hp_max);
    text.draw_text(batch,game.font_desc,dhs,{pbx+186,pby+5},{1,1,1,1},0.5f);

    // Ally HP
    eb::Vec4 ally_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 1)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Ally",{pbx+8,pby+24},ally_name_col,0.7f);
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
        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";

        if (b.item_menu_open) {
            // ── Item submenu ──
            auto battle_items = game.inventory.get_battle_items();
            int item_count = (int)battle_items.size();
            int visible = std::min(item_count, 6); // Max 6 visible at once
            float imh = 26.0f + visible * 24.0f + 8.0f;
            float imx = 16, imy = sh - imh - 10;

            batch.set_texture(game.white_desc);
            batch.draw_quad({imx,imy},{210,imh},{0,0},{1,1},{0.06f,0.04f,0.14f,0.95f});
            batch.draw_quad({imx,imy},{210,2},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx,imy+imh-2},{210,2},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx,imy},{2,imh},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx+208,imy},{2,imh},{0,0},{1,1},{0.5f,0.5f,0.9f,1});

            text.draw_text(batch,game.font_desc,"Items",{imx+8,imy+4},{0.4f,0.8f,1,1},0.7f);

            // Scroll offset if more items than visible
            int scroll = 0;
            if (b.item_menu_selection >= visible) scroll = b.item_menu_selection - visible + 1;

            for (int i = 0; i < visible && (i + scroll) < item_count; i++) {
                int idx = i + scroll;
                auto* item = battle_items[idx];
                eb::Vec4 c = (idx == b.item_menu_selection) ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{0.8f,0.8f,0.8f,1};
                std::string pfx = (idx == b.item_menu_selection) ? "> " : "  ";
                std::string label = pfx + item->name + " x" + std::to_string(item->quantity);
                text.draw_text(batch,game.font_desc,label,{imx+8,imy+26+i*24.0f},c,0.7f);
            }
        } else {
            // ── Main battle menu ──
            float mx = 16, my = sh - 152;
            batch.set_texture(game.white_desc);
            batch.draw_quad({mx,my},{170,144},{0,0},{1,1},{0.08f,0.05f,0.15f,0.95f});
            batch.draw_quad({mx,my},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx,my+142},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx,my},{2,144},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx+168,my},{2,144},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            // Fighter name header
            text.draw_text(batch,game.font_desc,fighter,{mx+8,my+4},{0.4f,0.8f,1,1},0.7f);
            const char* opts[]={"Attack","Items","Defend","Run"};
            for(int i=0;i<4;i++){
                eb::Vec4 c=(i==b.menu_selection)?eb::Vec4{1,1,0.3f,1}:eb::Vec4{0.8f,0.8f,0.8f,1};
                std::string pfx=(i==b.menu_selection)?"> ":"  ";
                text.draw_text(batch,game.font_desc,pfx+opts[i],{mx+8,my+26+i*28.0f},c,0.9f);
            }
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

static void draw_ui_region(eb::SpriteBatch& batch, GameState& game,
                            const char* name, float x, float y, float w, float h) {
    // Check flat UI pack first for flat_ prefixed names
    if (name[0] == 'f' && name[1] == 'l' && game.ui_flat_atlas) {
        auto* r = game.ui_flat_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_flat_desc);
            batch.draw_quad({x, y}, {w, h}, r->uv_min, r->uv_max);
            return;
        }
    }
    // Standard UI atlas
    if (game.ui_atlas) {
        auto* r = game.ui_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_desc);
            batch.draw_quad({x, y}, {w, h}, r->uv_min, r->uv_max);
            return;
        }
    }
    // Fallback: solid dark panel
    batch.set_texture(game.white_desc);
    batch.draw_quad({x, y}, {w, h}, {0,0}, {1,1}, {0.05f, 0.05f, 0.12f, 0.88f});
}

static void draw_ui_icon(eb::SpriteBatch& batch, GameState& game,
                          const char* name, float x, float y, float sz) {
    // Fantasy icon by index: "fi_42" = fantasy icon #42
    if (name[0] == 'f' && name[1] == 'i' && name[2] == '_') {
        int idx = std::atoi(name + 3);
        if (game.fantasy_icons_atlas && idx >= 0 && idx < game.fantasy_icons_atlas->region_count()) {
            auto r = game.fantasy_icons_atlas->region(idx);
            batch.set_texture(game.fantasy_icons_desc);
            batch.draw_quad({x, y}, {sz, sz}, r.uv_min, r.uv_max);
            return;
        }
    }
    // Standard UI atlas icon
    if (game.ui_atlas) {
        auto* r = game.ui_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_desc);
            batch.draw_quad({x, y}, {sz, sz}, r->uv_min, r->uv_max);
            return;
        }
    }
    // Fantasy icons atlas by name search (fallback)
    if (game.fantasy_icons_atlas) {
        auto* r = game.fantasy_icons_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.fantasy_icons_desc);
            batch.draw_quad({x, y}, {sz, sz}, r->uv_min, r->uv_max);
        }
    }
}

static void render_hud(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                        float screen_w, float screen_h) {
    auto& H = game.hud;
    float S = H.scale;

    // ── Player Stats Panel (top-left) ──
    if (H.show_player) {
        float hx = H.player_x, hy = H.player_y;
        float hw = H.player_w * S, hh = H.player_h * S;
        draw_ui_region(batch, game, "panel_hud_wide", hx, hy, hw, hh);

        // Name + Level
        char ns[64]; std::snprintf(ns, sizeof(ns), "Mage  Lv.%d", game.player_level);
        text.draw_text(batch, game.font_desc, ns, {hx + 14*S, hy + 10*S}, {1,1,1,1}, H.text_scale * S);

        // HP bar with heart icon
        float icon_sz = 18 * S;
        draw_ui_icon(batch, game, "icon_heart_red", hx + 12*S, hy + 34*S, icon_sz);
        float hp_pct = std::max(0.0f, (float)game.player_hp / game.player_hp_max);
        float bx = hx + 14*S + icon_sz + 4*S, by = hy + 36*S;
        float bw = H.hp_bar_w * S, bh = H.hp_bar_h * S;
        batch.set_texture(game.white_desc);
        batch.draw_quad({bx, by}, {bw, bh}, {0,0}, {1,1}, {0.15f, 0.15f, 0.2f, 0.9f});
        eb::Vec4 hc = hp_pct > 0.5f ? eb::Vec4{0.2f, 0.8f, 0.2f, 1}
                    : hp_pct > 0.25f ? eb::Vec4{0.9f, 0.7f, 0.1f, 1}
                    : eb::Vec4{0.9f, 0.2f, 0.2f, 1};
        batch.draw_quad({bx, by}, {bw * hp_pct, bh}, {0,0}, {1,1}, hc);
        char hs[32]; std::snprintf(hs, sizeof(hs), "%d/%d", game.player_hp, game.player_hp_max);
        text.draw_text(batch, game.font_desc, hs, {bx + bw + 8*S, by - 1}, {1,1,1,1}, 0.65f * S);

        // Gold with coin icon
        draw_ui_icon(batch, game, "icon_coin", hx + hw - 70*S, hy + 10*S, icon_sz);
        char gs[32]; std::snprintf(gs, sizeof(gs), "%d", game.gold);
        text.draw_text(batch, game.font_desc, gs, {hx + hw - 48*S, hy + 12*S}, {1.0f, 0.95f, 0.3f, 1}, 0.7f * S);

        // ── Survival Bars (below player panel) ──
        if (game.survival.enabled && H.show_survival) {
            float sy = hy + hh + 4*S;
            float bar_w = H.surv_bar_w * S, bar_h = H.surv_bar_h * S, bar_pad = 3*S;

            batch.set_texture(game.white_desc);
            batch.draw_quad({hx, sy}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, sy}, {bar_w * (game.survival.hunger / 100.0f), bar_h}, {0,0}, {1,1}, {0.85f, 0.55f, 0.15f, 0.9f});

            float ty2 = sy + bar_h + bar_pad;
            batch.draw_quad({hx, ty2}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, ty2}, {bar_w * (game.survival.thirst / 100.0f), bar_h}, {0,0}, {1,1}, {0.2f, 0.5f, 0.9f, 0.9f});

            float ey = ty2 + bar_h + bar_pad;
            batch.draw_quad({hx, ey}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, ey}, {bar_w * (game.survival.energy / 100.0f), bar_h}, {0,0}, {1,1}, {0.9f, 0.8f, 0.2f, 0.9f});
        }
    }

    // ── Time of Day Panel (top-right) ──
    if (H.show_time) {
        float tw = H.time_w * S, th = H.time_h * S;
        float tx = screen_w - tw - 8, ty = 8;
        draw_ui_region(batch, game, "panel_hud_sq", tx, ty, tw, th);

        int hour = (int)game.day_night.game_hours;
        int minute = (int)((game.day_night.game_hours - hour) * 60.0f);
        bool pm = hour >= 12;
        int dh = hour % 12; if (dh == 0) dh = 12;
        char time_str[16]; std::snprintf(time_str, sizeof(time_str), "%d:%02d %s", dh, minute, pm ? "PM" : "AM");
        text.draw_text(batch, game.font_desc, time_str, {tx + 12*S, ty + 10*S}, {1,1,0.9f,1}, H.time_text_scale * S);

        const char* period; eb::Vec4 pc;
        if (hour >= 6 && hour < 10)       { period = "Morning"; pc = {1.0f, 0.85f, 0.4f, 1}; }
        else if (hour >= 10 && hour < 16)  { period = "Day";     pc = {1.0f, 1.0f, 0.8f, 1}; }
        else if (hour >= 16 && hour < 19)  { period = "Evening"; pc = {1.0f, 0.6f, 0.3f, 1}; }
        else if (hour >= 19 && hour < 21)  { period = "Dusk";    pc = {0.7f, 0.5f, 0.8f, 1}; }
        else                               { period = "Night";   pc = {0.4f, 0.5f, 0.9f, 1}; }
        text.draw_text(batch, game.font_desc, period, {tx + 12*S, ty + 34*S}, pc, 0.65f * S);

        float ico = 20 * S;
        draw_ui_icon(batch, game, (hour >= 6 && hour < 18) ? "icon_star" : "icon_gem_blue",
                     tx + tw - ico - 8*S, ty + 10*S, ico);
    }

    // ── Inventory Quick Bar (bottom-left) ──
    if (H.show_inventory && !game.inventory.items.empty()) {
        float slot_w = H.inv_slot_size * S, slot_h = slot_w;
        float pad = H.inv_padding * S;
        float ix = 8, iy_base = screen_h - H.inv_y_offset * S;
        int max_slots = std::min(H.inv_max_slots, (int)game.inventory.items.size());
        bool sel_mode = game.hud.inv_open;

        float strip_w = max_slots * (slot_w + pad) + pad;
        draw_ui_region(batch, game, "panel_dark", ix - 3, iy_base - 3, strip_w + 6, slot_h + 8);

        // Hint text above the bar
        if (sel_mode) {
            const char* hint = "[Left/Right] Select  [Z] Use  [X] Close";
            float hint_scale = 0.5f * S;
            auto hsz = text.measure_text(hint, hint_scale);
            batch.set_texture(game.white_desc);
            batch.draw_quad({ix, iy_base - 22*S}, {hsz.x + 12, hsz.y + 6},
                            {0,0}, {1,1}, {0, 0, 0, 0.7f});
            text.draw_text(batch, game.font_desc, hint,
                           {ix + 6, iy_base - 20*S}, {0.7f, 0.8f, 1.0f, 0.9f}, hint_scale);
        } else if (game.inventory.items.size() > 0) {
            // Show "X: Items" hint
            const char* open_hint = "[X] Items";
            float oh_scale = 0.45f * S;
            text.draw_text(batch, game.font_desc, open_hint,
                           {ix, iy_base - 14*S}, {0.5f, 0.5f, 0.55f, 0.6f}, oh_scale);
        }

        for (int i = 0; i < max_slots; i++) {
            auto& item = game.inventory.items[i];
            float sx = ix + i * (slot_w + pad) + pad;
            bool selected = sel_mode && i == game.hud.inv_selected;

            // Slot background
            batch.set_texture(game.white_desc);
            if (selected) {
                // Bright highlight for selected slot
                batch.draw_quad({sx - 2, iy_base - 2}, {slot_w + 4, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.9f, 0.3f, 0.35f});
                batch.draw_quad({sx - 2, iy_base - 2}, {slot_w + 4, 2.5f},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx - 2, iy_base + slot_h}, {slot_w + 4, 2.5f},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx - 2, iy_base - 2}, {2.5f, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx + slot_w, iy_base - 2}, {2.5f, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
            }
            batch.draw_quad({sx, iy_base}, {slot_w, slot_h}, {0,0}, {1,1},
                            selected ? eb::Vec4{0.15f, 0.15f, 0.25f, 0.9f}
                                     : eb::Vec4{0.1f, 0.1f, 0.18f, 0.8f});
            batch.draw_quad({sx, iy_base}, {slot_w, 1.5f}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.6f});
            batch.draw_quad({sx, iy_base + slot_h - 1.5f}, {slot_w, 1.5f}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.6f});

            // Item icon
            const char* icon_name = "icon_gem_blue";
            if (item.damage > 0) icon_name = "icon_sword";
            else if (item.heal_hp > 0) icon_name = "icon_potion";
            else if (item.element == "fire") icon_name = "icon_heart_red";
            else if (item.element == "holy") icon_name = "icon_star";
            else if (item.element == "ice") icon_name = "icon_gem_green";
            else if (item.element == "lightning") icon_name = "icon_ring";
            float icon_sz = slot_w * 0.55f;
            draw_ui_icon(batch, game, icon_name, sx + (slot_w - icon_sz) * 0.5f, iy_base + 4*S, icon_sz);

            // Quantity badge
            if (item.quantity > 1) {
                char qty[8]; std::snprintf(qty, sizeof(qty), "%d", item.quantity);
                text.draw_text(batch, game.font_desc, qty,
                               {sx + slot_w - 14*S, iy_base + slot_h - 16*S},
                               {1, 1, 1, 0.9f}, 0.5f * S);
            }
        }

        // "..." if more items
        if ((int)game.inventory.items.size() > max_slots) {
            float dx = ix + max_slots * (slot_w + pad) + pad + 4;
            text.draw_text(batch, game.font_desc, "...", {dx, iy_base + slot_h * 0.3f},
                           {0.6f, 0.6f, 0.6f, 0.8f}, 0.6f * S);
        }

        // Show selected item name + description below the bar
        if (sel_mode && game.hud.inv_selected < (int)game.inventory.items.size()) {
            auto& sel_item = game.inventory.items[game.hud.inv_selected];
            float desc_y = iy_base + slot_h + 8*S;
            float desc_w = strip_w + 6;

            // Description panel
            draw_ui_region(batch, game, "panel_mini", ix - 3, desc_y, desc_w, 36*S);

            // Item name (bright)
            text.draw_text(batch, game.font_desc, sel_item.name,
                           {ix + 6, desc_y + 4*S}, {1, 1, 0.9f, 1}, 0.7f * S);

            // Description (dim)
            if (!sel_item.description.empty()) {
                text.draw_text(batch, game.font_desc, sel_item.description,
                               {ix + 6, desc_y + 20*S}, {0.7f, 0.7f, 0.65f, 0.9f}, 0.5f * S);
            }
        }
    }

    // ── Minimap (bottom-right corner) ──
    if (H.show_minimap && game.tile_map.width() > 0 && game.tile_map.height() > 0) {
        float mm_base = H.minimap_size * S;
        float mw = (float)game.tile_map.width();
        float mh = (float)game.tile_map.height();
        float aspect = mw / mh;
        float mm_w, mm_h;
        if (aspect >= 1.0f) { mm_w = mm_base; mm_h = mm_base / aspect; }
        else { mm_h = mm_base; mm_w = mm_base * aspect; }

        float mm_x = screen_w - mm_w - 10;
        float mm_y = screen_h - mm_h - 10;
        float px_w = mm_w / mw;  // Pixels per tile
        float px_h = mm_h / mh;

        // Background panel
        draw_ui_region(batch, game, "panel_hud_sq", mm_x - 4, mm_y - 4, mm_w + 8, mm_h + 8);

        // Draw tile colors (every Nth tile for performance on large maps)
        batch.set_texture(game.white_desc);
        int step = std::max(1, (int)mw / 80);  // Limit to ~80 samples across
        for (int y = 0; y < (int)mh; y += step) {
            for (int x = 0; x < (int)mw; x += step) {
                int t = game.tile_map.tile_at(0, x, y);
                eb::Vec4 col;
                if (t == 0)                          col = {0.08f, 0.08f, 0.12f, 1};
                else if (t >= 42 && t <= 50)         col = {0.12f, 0.24f, 0.47f, 1}; // Water
                else if (t >= 25 && t <= 36)         col = {0.31f, 0.31f, 0.31f, 1}; // Roads
                else if (t >= 5 && t <= 8)           col = {0.47f, 0.35f, 0.24f, 1}; // Dirt
                else if (t >= 19 && t <= 24)         col = {0.20f, 0.12f, 0.16f, 1}; // Dark
                else                                 col = {0.20f, 0.39f, 0.16f, 1}; // Grass
                if (game.tile_map.collision_at(x, y) == eb::CollisionType::Solid && t > 0)
                    col = {col.x * 0.5f, col.y * 0.5f, col.z * 0.5f, 1};

                batch.draw_quad({mm_x + x * px_w, mm_y + y * px_h},
                                {px_w * step + 0.5f, px_h * step + 0.5f}, {0,0}, {1,1}, col);
            }
        }

        // Player dot (yellow, pulsing)
        float ts = (float)game.tile_map.tile_size();
        float pp_x = mm_x + (game.player_pos.x / ts) * px_w;
        float pp_y = mm_y + (game.player_pos.y / ts) * px_h;
        float dot_sz = 4.0f * S;
        float pulse = 0.8f + 0.2f * std::sin(game.game_time * 5.0f);
        batch.draw_quad({pp_x - dot_sz*0.5f, pp_y - dot_sz*0.5f}, {dot_sz, dot_sz},
                        {0,0}, {1,1}, {1.0f, 1.0f, 0.3f, pulse});

        // NPC dots
        for (auto& npc : game.npcs) {
            if (!npc.schedule.currently_visible) continue;
            float np_x = mm_x + (npc.position.x / ts) * px_w;
            float np_y = mm_y + (npc.position.y / ts) * px_h;
            float nd = 2.5f * S;
            eb::Vec4 nc = npc.hostile ? eb::Vec4{1.0f, 0.2f, 0.2f, 0.9f}
                                      : eb::Vec4{0.2f, 0.8f, 1.0f, 0.7f};
            batch.draw_quad({np_x - nd*0.5f, np_y - nd*0.5f}, {nd, nd}, {0,0}, {1,1}, nc);
        }

        // Item drop dots (gold)
        for (auto& drop : game.world_drops) {
            float dp_x = mm_x + (drop.position.x / ts) * px_w;
            float dp_y = mm_y + (drop.position.y / ts) * px_h;
            batch.draw_quad({dp_x - 1.5f, dp_y - 1.5f}, {3, 3},
                            {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.8f});
        }

        // Border
        batch.draw_quad({mm_x, mm_y}, {mm_w, 1}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x, mm_y + mm_h - 1}, {mm_w, 1}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x, mm_y}, {1, mm_h}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x + mm_w - 1, mm_y}, {1, mm_h}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
    }
}

// ─── Render world ───

void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    // Apply camera shake offset
    eb::Mat4 proj = game.camera.projection_matrix();
    if (game.shake_timer > 0 && game.shake_intensity > 0) {
        float sx = ((game.rng() % 200) - 100) / 100.0f * game.shake_intensity;
        float sy = ((game.rng() % 200) - 100) / 100.0f * game.shake_intensity;
        proj[3][0] += sx;
        proj[3][1] += sy;
    }
    batch.set_projection(proj);

    // Tile map with water animation
    batch.set_texture(game.tileset_desc);
    game.tile_map.render(batch, game.camera, game.game_time);

    // Grass overlay
    render_grass_overlay(game, batch, game.game_time);

    // Y-sorted objects (with per-object scale and tint)
    for (const auto& obj : game.world_objects) {
        if (obj.sprite_id < 0 || obj.sprite_id >= (int)game.object_defs.size() ||
            obj.sprite_id >= (int)game.object_regions.size()) continue;
        const auto& def = game.object_defs[obj.sprite_id];
        const auto& region = game.object_regions[obj.sprite_id];
        float sw = def.render_size.x * obj.scale;
        float sh = def.render_size.y * obj.scale;
        eb::Vec2 dp = {obj.position.x - sw*0.5f, obj.position.y - sh};
        batch.draw_sorted(dp, {sw, sh}, region.uv_min, region.uv_max,
                         obj.position.y, game.tileset_desc, obj.tint);
    }

    // Leaf overlay
    render_leaf_overlay(game, batch, game.game_time);

    // NPCs (skip scheduled-out NPCs)
    for (const auto& npc : game.npcs) {
        if (!npc.schedule.currently_visible) continue;
        // Prefer string-keyed atlas cache (with grid-size-aware key)
        eb::TextureAtlas* atlas_ptr = nullptr;
        VkDescriptorSet desc = VK_NULL_HANDLE;
        if (!npc.sprite_atlas_key.empty()) {
            // Build cache key with optional grid size suffix
            std::string cache_key = npc.sprite_atlas_key;
            if (npc.sprite_grid_w > 0 && npc.sprite_grid_h > 0) {
                char buf[32]; std::snprintf(buf, sizeof(buf), "@%dx%d", npc.sprite_grid_w, npc.sprite_grid_h);
                cache_key += buf;
            }
            auto it = game.atlas_cache.find(cache_key);
            if (it != game.atlas_cache.end()) {
                atlas_ptr = it->second.get();
                desc = game.atlas_descs[cache_key];
            } else {
                // Try without grid suffix (backward compat)
                auto it2 = game.atlas_cache.find(npc.sprite_atlas_key);
                if (it2 != game.atlas_cache.end()) {
                    atlas_ptr = it2->second.get();
                    desc = game.atlas_descs[npc.sprite_atlas_key];
                }
            }
        }
        // Fallback to legacy indexed lookup
        if (!atlas_ptr && npc.sprite_atlas_id >= 0 && npc.sprite_atlas_id < (int)game.npc_atlases.size()) {
            atlas_ptr = game.npc_atlases[npc.sprite_atlas_id].get();
            desc = game.npc_descs[npc.sprite_atlas_id];
        }
        if (atlas_ptr && desc) {
            auto sr = get_character_sprite(*atlas_ptr, npc.dir, npc.moving, npc.frame);
            // Use per-NPC cell size for render dimensions, with 1.5x scale
            float base_w = (npc.sprite_grid_w > 0) ? (float)npc.sprite_grid_w : 32.0f;
            float base_h = (npc.sprite_grid_h > 0) ? (float)npc.sprite_grid_h : 32.0f;
            float rw = base_w * 1.5f * npc.sprite_scale;
            float rh = base_h * 1.5f * npc.sprite_scale;
            eb::Vec2 uv0 = sr.uv_min, uv1 = sr.uv_max;
            if (npc.sprite_flip_h) std::swap(uv0.x, uv1.x);
            eb::Vec2 dp = {npc.position.x-rw*0.5f, npc.position.y-rh+4};
            batch.draw_sorted(dp, {rw,rh}, uv0, uv1, npc.position.y, desc, npc.sprite_tint);
        }
    }

    // Party followers (with ally sprite scale)
    if (game.sam_atlas) {
        for (int pi = 0; pi < (int)game.party.size(); pi++) {
            auto& pm = game.party[pi];
            auto sr = get_character_sprite(*game.sam_atlas, pm.dir, pm.moving, pm.frame);
            float rw = 48 * game.ally_sprite_scale, rh = 64 * game.ally_sprite_scale;
            float bob = pm.moving ? std::sin(pm.anim_timer*15.0f)*2.0f : 0.0f;
            eb::Vec2 dp = {pm.position.x-rw*0.5f, pm.position.y-rh+4+bob};
            batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, pm.position.y, game.sam_desc);
        }
    }

    // Player character (with per-player sprite scale)
    // Render at same size as NPCs (48x64) so all characters are consistent
    if (game.dean_atlas) {
        auto sr = get_character_sprite(*game.dean_atlas, game.player_dir, game.player_moving, game.player_frame);
        float rw = 48.0f * game.player_sprite_scale;
        float rh = 64.0f * game.player_sprite_scale;
        float bob = game.player_moving ? std::sin(game.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {game.player_pos.x-rw*0.5f, game.player_pos.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, game.player_pos.y, game.dean_desc);
    }

    // World item drops (bouncing icons on the ground)
    for (auto& drop : game.world_drops) {
        float bob = std::sin(drop.anim_timer * 4.0f) * 3.0f;
        float glow = 0.5f + 0.3f * std::sin(drop.anim_timer * 3.0f);
        float drop_sz = 20.0f;
        eb::Vec2 dp = {drop.position.x - drop_sz * 0.5f, drop.position.y - drop_sz - 4.0f + bob};

        // Glow circle underneath
        batch.set_texture(game.white_desc);
        batch.draw_sorted({drop.position.x - 10, drop.position.y - 4},
                         {20, 8}, {0,0}, {1,1}, drop.position.y + 1,
                         game.white_desc, {1.0f, 0.9f, 0.3f, glow * 0.3f});

        // Item icon
        const char* icon = "icon_gem_blue";
        if (drop.damage > 0) icon = "icon_sword";
        else if (drop.heal_hp > 0) icon = "icon_potion";
        else if (drop.element == "fire") icon = "icon_heart_red";
        else if (drop.element == "holy") icon = "icon_star";
        if (game.ui_atlas) {
            auto* r = game.ui_atlas->find_region(icon);
            if (r) {
                batch.draw_sorted(dp, {drop_sz, drop_sz}, r->uv_min, r->uv_max,
                                 drop.position.y, game.ui_desc);
            }
        }
    }

    batch.flush_sorted();

    // ── Particles (rendered on top of world, before UI) ──
    // Use default white texture for colored quads
    batch.set_texture(game.white_desc);
    for (auto& emitter : game.emitters) {
        for (auto& p : emitter.particles) {
            if (!p.alive) continue;
            float t = p.progress();
            float sz = p.size + (p.size_end - p.size) * t;
            eb::Vec4 col = {
                p.color.x + (p.color_end.x - p.color.x) * t,
                p.color.y + (p.color_end.y - p.color.y) * t,
                p.color.z + (p.color_end.z - p.color.z) * t,
                p.color.w + (p.color_end.w - p.color.w) * t,
            };
            batch.draw_quad({p.pos.x - sz*0.5f, p.pos.y - sz*0.5f}, {sz, sz}, col);
        }
    }

    // ── 2D Lighting (darken scene then brighten around lights) ──
    if (game.lighting_enabled && game.ambient_light < 0.99f) {
        batch.set_texture(game.white_desc);
        auto vis = game.camera.visible_area();
        // Dark overlay covering entire visible area
        float darkness = 1.0f - game.ambient_light;
        batch.draw_quad({vis.x, vis.y}, {vis.w, vis.h}, {0,0}, {1,1},
                        {0, 0, 0, darkness});

        // Punch light circles as bright overlays (additive approximation)
        for (auto& light : game.lights) {
            if (!light.active) continue;
            float r = light.radius;
            // Draw concentric rings for gradient falloff
            int rings = 5;
            for (int i = 0; i < rings; i++) {
                float t = (float)i / rings;
                float ring_r = r * (1.0f - t);
                float alpha = light.intensity * darkness * (1.0f - t) * 0.25f;
                batch.draw_quad(
                    {light.x - ring_r, light.y - ring_r},
                    {ring_r * 2, ring_r * 2},
                    {0,0}, {1,1},
                    {light.color.x, light.color.y, light.color.z, alpha}
                );
            }
        }
    }

    batch.flush();
}

// ─── Sync game state → script UI components each frame ───
static void sync_hud_values(GameState& game) {
    auto find_label = [&](const std::string& id) -> ScriptUILabel* {
        for (auto& l : game.script_ui.labels) if (l.id == id) return &l;
        return nullptr;
    };
    auto find_bar = [&](const std::string& id) -> ScriptUIBar* {
        for (auto& b : game.script_ui.bars) if (b.id == id) return &b;
        return nullptr;
    };
    auto find_image = [&](const std::string& id) -> ScriptUIImage* {
        for (auto& img : game.script_ui.images) if (img.id == id) return &img;
        return nullptr;
    };

    // Player HP
    if (auto* b = find_bar("hud_hp")) { b->value = (float)game.player_hp; b->max_value = (float)game.player_hp_max; }
    if (auto* l = find_label("hud_hp_text")) {
        char s[32]; std::snprintf(s, sizeof(s), "%d/%d", game.player_hp, game.player_hp_max);
        l->text = s;
    }

    // Player name/level
    if (auto* l = find_label("hud_name")) {
        char s[64]; std::snprintf(s, sizeof(s), "Mage  Lv.%d", game.player_level);
        l->text = s;
    }

    // Gold
    if (auto* l = find_label("hud_gold")) { l->text = std::to_string(game.gold); }

    // Time
    if (auto* l = find_label("hud_time")) {
        int hour = (int)game.day_night.game_hours;
        int minute = (int)((game.day_night.game_hours - hour) * 60.0f);
        bool pm = hour >= 12;
        int dh = hour % 12; if (dh == 0) dh = 12;
        char s[16]; std::snprintf(s, sizeof(s), "%d:%02d %s", dh, minute, pm ? "PM" : "AM");
        l->text = s;
    }
    if (auto* l = find_label("hud_period")) {
        int hour = (int)game.day_night.game_hours;
        if (hour >= 6 && hour < 10)       { l->text = "Morning"; l->color = {1.0f, 0.85f, 0.4f, 1}; }
        else if (hour >= 10 && hour < 16)  { l->text = "Day";     l->color = {1.0f, 1.0f, 0.8f, 1}; }
        else if (hour >= 16 && hour < 19)  { l->text = "Evening"; l->color = {1.0f, 0.6f, 0.3f, 1}; }
        else if (hour >= 19 && hour < 21)  { l->text = "Dusk";    l->color = {0.7f, 0.5f, 0.8f, 1}; }
        else                               { l->text = "Night";   l->color = {0.4f, 0.5f, 0.9f, 1}; }
    }

    // Survival
    if (game.survival.enabled) {
        if (auto* b = find_bar("hud_hunger")) { b->value = game.survival.hunger; }
        if (auto* b = find_bar("hud_thirst")) { b->value = game.survival.thirst; }
        if (auto* b = find_bar("hud_energy")) { b->value = game.survival.energy; }
    }

    // Sun/moon icon swap (fantasy icons: 281=sun, 280=moon)
    if (auto* img = find_image("hud_sun")) {
        int hour = (int)game.day_night.game_hours;
        img->icon_name = (hour >= 6 && hour < 18) ? "fi_281" : "fi_280";
    }

    // HP bar color (green → yellow → red)
    if (auto* b = find_bar("hud_hp")) {
        float pct = b->max_value > 0 ? b->value / b->max_value : 0;
        if (pct > 0.5f) b->color = {0.2f, 0.8f, 0.2f, 1};
        else if (pct > 0.25f) b->color = {0.9f, 0.7f, 0.1f, 1};
        else b->color = {0.9f, 0.2f, 0.2f, 1};
    }

    // ── Pause menu sync ──
    auto set_vis = [&](const std::string& id, bool vis) {
        for (auto& l : game.script_ui.labels) if (l.id == id) { l.visible = vis; return; }
        for (auto& p : game.script_ui.panels) if (p.id == id) { p.visible = vis; return; }
        for (auto& img : game.script_ui.images) if (img.id == id) { img.visible = vis; return; }
    };

    bool paused = game.paused;
    bool show_pause = paused && !game.level_select_open;
    set_vis("pause_bg", paused);  // Keep bg visible during level select too
    set_vis("pause_icon", show_pause);
    set_vis("pause_title", show_pause);
    set_vis("pause_div", show_pause);
    set_vis("pause_cursor", show_pause);
    static const char* pause_item_ids[6] = {"pause_item_0","pause_item_1","pause_item_2","pause_item_3","pause_item_4","pause_item_5"};
    static const char* pause_icon_ids[6] = {"pause_icon_0","pause_icon_1","pause_icon_2","pause_icon_3","pause_icon_4","pause_icon_5"};
    for (int i = 0; i < 6; i++) {
        set_vis(pause_item_ids[i], show_pause);
        set_vis(pause_icon_ids[i], show_pause);

        // Highlight selected item
        if (auto* l = find_label(pause_item_ids[i])) {
            if (i == game.pause_selection && show_pause) {
                l->color = {1.0f, 1.0f, 0.9f, 1.0f};
            } else {
                l->color = {0.85f, 0.82f, 0.75f, 1.0f};
            }
        }
    }

    // Move pause cursor to selected item
    if (show_pause) {
        for (auto& img : game.script_ui.images) {
            if (img.id == "pause_cursor") {
                if (auto* l = find_label(pause_item_ids[game.pause_selection])) {
                    img.position.y = l->position.y;
                }
                break;
            }
        }
    }

    // ── Level selector sub-menu ──
    // Create/update level list labels dynamically
    if (paused && game.level_select_open) {
        // Show title
        static bool level_title_created = false;
        if (!level_title_created) {
            float cx = game.hud.screen_w / 2.0f;
            float cy = game.hud.screen_h / 2.0f;
            game.script_ui.labels.push_back({"lvl_sel_title", "SELECT LEVEL", {cx - 80, cy - 110}, {1, 0.9f, 0.5f, 1}, 1.2f, true});
            level_title_created = true;
        }
        set_vis("lvl_sel_title", true);

        // Remove old level list labels
        auto& labels = game.script_ui.labels;
        labels.erase(std::remove_if(labels.begin(), labels.end(),
            [](auto& l) { return l.id.size() > 8 && l.id.compare(0, 8, "lvl_sel_") == 0 && l.id != "lvl_sel_title"; }),
            labels.end());

        // Create labels for each loaded level
        float cx = game.hud.screen_w / 2.0f;
        float cy = game.hud.screen_h / 2.0f;
        for (int i = 0; i < (int)game.level_select_ids.size(); i++) {
            std::string lid = "lvl_sel_" + std::to_string(i);
            // Strip .json for display
            std::string display = game.level_select_ids[i];
            auto dot = display.rfind('.');
            if (dot != std::string::npos) display = display.substr(0, dot);
            // Capitalize first letter
            if (!display.empty()) display[0] = std::toupper(display[0]);
            // Mark active level
            if (game.level_manager && game.level_select_ids[i] == game.level_manager->active_level)
                display += "  (current)";

            eb::Vec4 color = (i == game.level_select_cursor)
                ? eb::Vec4{1.0f, 1.0f, 0.9f, 1.0f}
                : eb::Vec4{0.85f, 0.82f, 0.75f, 1.0f};
            labels.push_back({lid, display, {cx - 60, cy - 70 + i * 36.0f}, color, 1.0f, true});
        }
    } else {
        // Hide level selector labels
        set_vis("lvl_sel_title", false);
        auto& labels = game.script_ui.labels;
        labels.erase(std::remove_if(labels.begin(), labels.end(),
            [](auto& l) { return l.id.size() > 8 && l.id.compare(0, 8, "lvl_sel_") == 0 && l.id != "lvl_sel_title"; }),
            labels.end());
    }
}

// ─── Render UI overlay ───

void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh) {
    batch.set_projection(screen_proj);
    game.hud.screen_w = sw;
    game.hud.screen_h = sh;

    // Sync game values into script UI components
    sync_hud_values(game);

    // Day-night tint overlay (drawn first, under HUD)
    auto& tint = game.day_night.current_tint;
    if (tint.w > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, tint);
    }

    // NPC labels (skip hidden NPCs)
    // Render in world-space projection so labels track NPC position exactly
    batch.set_projection(game.camera.projection_matrix());
    for (const auto& npc : game.npcs) {
        if (!npc.schedule.currently_visible) continue;
        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < npc.interact_radius * 1.5f) {
            // Position label directly above the NPC sprite
            float label_x = npc.position.x;
            float label_y = npc.position.y - 72.0f; // Just above the 64px sprite

            float ns = 1.1f;
            auto name_size = text.measure_text(npc.name, ns);
            float lp = 8.0f;
            batch.set_texture(game.white_desc);
            batch.draw_quad({label_x - name_size.x*0.5f - lp, label_y - lp*0.5f},
                {name_size.x + lp*2, name_size.y + lp}, {0,0},{1,1}, {0.05f, 0.05f, 0.12f, 0.75f});
            // Border
            batch.draw_quad({label_x - name_size.x*0.5f - lp, label_y - lp*0.5f},
                {name_size.x + lp*2, 1.5f}, {0,0},{1,1}, {0.5f, 0.5f, 0.7f, 0.6f});
            text.draw_text(batch, game.font_desc, npc.name,
                {label_x - name_size.x*0.5f, label_y}, {1, 1, 0.5f, 1}, ns);

            if (dist < npc.interact_radius) {
                const char* hint_text =
#ifdef __ANDROID__
                    "[A] Talk";
#else
                    "[Z] Talk";
#endif
                float hs = 0.85f;
                auto hint_size = text.measure_text(hint_text, hs);
                float hy = label_y + name_size.y + 6.0f;
                batch.set_texture(game.white_desc);
                batch.draw_quad({label_x - hint_size.x*0.5f - 6, hy - 3},
                    {hint_size.x + 12, hint_size.y + 6}, {0,0},{1,1}, {0.05f, 0.05f, 0.12f, 0.65f});
                text.draw_text(batch, game.font_desc, hint_text,
                    {label_x - hint_size.x*0.5f, hy}, {0.8f, 0.9f, 1.0f, 0.95f}, hs);
            }
        }
    }
    // Switch back to screen projection for HUD
    batch.set_projection(screen_proj);

    // HUD
    render_hud(game, batch, text, sw, sh);

    // Dialogue
    if (game.dialogue.is_active()) {
        game.dialogue.render(batch, text, game.font_desc, game.white_desc, sw, sh);
    }

    // Merchant UI
    if (game.merchant_ui.is_open()) {
        game.merchant_ui.render(batch, text, game, sw, sh);
    }

    // ── Script-driven UI elements ──
    // Panels (rendered first as backgrounds)
    for (auto& panel : game.script_ui.panels) {
        if (!panel.visible) continue;
        draw_ui_region(batch, game, panel.sprite_region.c_str(),
                       panel.position.x, panel.position.y, panel.width, panel.height);
    }
    // Images
    for (auto& img : game.script_ui.images) {
        if (!img.visible) continue;
        draw_ui_icon(batch, game, img.icon_name.c_str(),
                     img.position.x, img.position.y, img.width);
    }
    // Labels
    for (auto& label : game.script_ui.labels) {
        if (!label.visible) continue;
        text.draw_text(batch, game.font_desc, label.text,
                       label.position, label.color, label.scale);
    }
    // Bars
    for (auto& bar : game.script_ui.bars) {
        if (!bar.visible) continue;
        batch.set_texture(game.white_desc);
        batch.draw_quad(bar.position, {bar.width, bar.height},
                        {0,0}, {1,1}, bar.bg_color);
        float pct = bar.max_value > 0 ? bar.value / bar.max_value : 0;
        batch.draw_quad(bar.position, {bar.width * pct, bar.height},
                        {0,0}, {1,1}, bar.color);
    }
    // Notifications (centered at top)
    for (auto& n : game.script_ui.notifications) {
        float alpha = std::min(1.0f, n.duration - n.timer);
        if (alpha <= 0) continue;
        float ns = 0.8f;
        auto sz = text.measure_text(n.text, ns);
        float nx = (sw - sz.x) * 0.5f;
        float ny = 60.0f;
        batch.set_texture(game.white_desc);
        batch.draw_quad({nx - 12, ny - 4}, {sz.x + 24, sz.y + 8},
                        {0,0}, {1,1}, {0, 0, 0, 0.7f * alpha});
        text.draw_text(batch, game.font_desc, n.text,
                       {nx, ny}, {1, 1, 1, alpha}, ns);
    }

    // ── Weather Rendering ──
    {
        auto& w = game.weather;
        batch.set_texture(game.white_desc);

        // Cloud shadows (drawn first, darkens the scene)
        if (w.clouds_active) {
            for (auto& c : w.cloud_shadows) {
                // Draw overlapping ellipses for soft cloud shadow
                float r = c.radius;
                eb::Vec4 shadow_col = {0, 0, 0, c.opacity};
                batch.draw_quad({c.x - r, c.y - r * 0.6f}, {r * 2, r * 1.2f}, {0,0}, {1,1}, shadow_col);
                // Secondary blob offset for organic shape
                batch.draw_quad({c.x - r * 0.7f + 20, c.y - r * 0.4f - 10}, {r * 1.4f, r * 0.8f}, {0,0}, {1,1},
                    {0, 0, 0, c.opacity * 0.6f});
            }
        }

        // God rays (additive light beams through cloud gaps)
        if (w.god_rays_active && w.clouds_active) {
            float ray_w = sw / w.god_ray_count;
            float angle_offset = w.god_ray_angle + w.god_ray_sway;
            for (int i = 0; i < w.god_ray_count; i++) {
                float base_x = (i + 0.5f) * ray_w;
                // Vary opacity per ray using sine
                float ray_alpha = w.god_ray_color.w * (0.5f + 0.5f * std::sin(game.game_time * 0.5f + i * 1.7f));
                if (ray_alpha < 0.02f) continue;
                float top_x = base_x + angle_offset;
                float beam_w = ray_w * 0.3f;
                // Draw as a tall narrow quad (approximation of a light beam)
                eb::Vec4 rc = {w.god_ray_color.x, w.god_ray_color.y, w.god_ray_color.z, ray_alpha};
                batch.draw_quad({top_x - beam_w * 0.5f, 0}, {beam_w, sh}, {0,0}, {1,1}, rc);
                // Wider, more transparent base
                batch.draw_quad({top_x - beam_w, sh * 0.3f}, {beam_w * 2, sh * 0.7f}, {0,0}, {1,1},
                    {rc.x, rc.y, rc.z, ray_alpha * 0.3f});
            }
        }

        // Rain particles
        if (w.rain_active) {
            float angle_rad = (w.rain_angle + w.wind_strength * 30.0f) * 3.14159f / 180.0f;
            float sin_a = std::sin(angle_rad);
            for (auto& d : w.rain_drops) {
                float x2 = d.x + sin_a * d.length;
                float y2 = d.y + d.length;
                // Draw rain drop as a thin vertical quad
                float rw = 1.0f;
                eb::Vec4 rc = {w.rain_color.x, w.rain_color.y, w.rain_color.z, d.opacity};
                batch.draw_quad({d.x, d.y}, {rw, d.length}, {0,0}, {1,1}, rc);
            }
        }

        // Snow particles
        if (w.snow_active) {
            for (auto& f : w.snow_flakes) {
                eb::Vec4 sc = {w.snow_color.x, w.snow_color.y, w.snow_color.z, f.opacity * w.snow_color.w};
                batch.draw_quad({f.x, f.y}, {f.size, f.size}, {0,0}, {1,1}, sc);
            }
        }

        // Lightning bolts
        for (auto& bolt : w.bolts) {
            float alpha = bolt.brightness * (bolt.timer / w.lightning_flash_duration);
            if (alpha < 0.01f) continue;
            eb::Vec4 lc = {0.9f, 0.9f, 1.0f, alpha};
            // Draw bolt segments
            float px = bolt.x1, py = bolt.y1;
            for (auto& [sx, sy] : bolt.segments) {
                // Draw each segment as a thin quad
                float dx = sx - px, dy = sy - py;
                float len = std::sqrt(dx * dx + dy * dy);
                batch.draw_quad({std::min(px, sx) - 1, std::min(py, sy)},
                    {std::abs(dx) + 2, std::abs(dy) + 1}, {0,0}, {1,1}, lc);
                // Glow around bolt
                batch.draw_quad({std::min(px, sx) - 4, std::min(py, sy) - 2},
                    {std::abs(dx) + 8, std::abs(dy) + 4}, {0,0}, {1,1},
                    {0.7f, 0.7f, 1.0f, alpha * 0.2f});
                px = sx; py = sy;
            }
        }

        // Fog overlay
        if (w.fog_active && w.fog_density > 0.01f) {
            eb::Vec4 fc = {w.fog_color.x, w.fog_color.y, w.fog_color.z, w.fog_density * w.fog_color.w};
            batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, fc);
        }
    }

    // ── Screen Effects ──
    // Flash overlay
    if (game.flash_a > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1},
                        {game.flash_r, game.flash_g, game.flash_b, game.flash_a});
    }
    // Fade overlay
    if (game.fade_a > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1},
                        {game.fade_r, game.fade_g, game.fade_b, game.fade_a});
    }

    // ── Screen Transitions ──
    if (game.transition.active || game.transition.progress > 0.01f) {
        batch.set_texture(game.white_desc);
        float p = game.transition.progress;
        switch (game.transition.type) {
            case eb::TransitionType::Fade:
                batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, p});
                break;
            case eb::TransitionType::Iris: {
                // Circle wipe: draw 4 quads around a circular hole
                float radius = (1.0f - p) * std::max(sw, sh) * 0.8f;
                float cx = sw * 0.5f, cy = sh * 0.5f;
                // Approximate with rectangle coverage leaving center open
                if (p > 0.01f) {
                    float top = std::max(0.0f, cy - radius);
                    float bot = std::min(sh, cy + radius);
                    float lft = std::max(0.0f, cx - radius);
                    float rgt = std::min(sw, cx + radius);
                    batch.draw_quad({0, 0}, {sw, top}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({0, bot}, {sw, sh - bot}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({0, top}, {lft, bot - top}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({rgt, top}, {sw - rgt, bot - top}, {0,0}, {1,1}, {0,0,0,1});
                }
                break;
            }
            case eb::TransitionType::Wipe: {
                float w_size = sw * p;
                float h_size = sh * p;
                if (game.transition.direction == 0) // left
                    batch.draw_quad({0, 0}, {w_size, sh}, {0,0}, {1,1}, {0,0,0,1});
                else if (game.transition.direction == 1) // right
                    batch.draw_quad({sw - w_size, 0}, {w_size, sh}, {0,0}, {1,1}, {0,0,0,1});
                else if (game.transition.direction == 2) // up
                    batch.draw_quad({0, 0}, {sw, h_size}, {0,0}, {1,1}, {0,0,0,1});
                else // down
                    batch.draw_quad({0, sh - h_size}, {sw, h_size}, {0,0}, {1,1}, {0,0,0,1});
                break;
            }
            case eb::TransitionType::Pixelate:
                // Approximate pixelation with a semi-transparent black overlay that increases
                batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, p * 0.9f});
                break;
            default: break;
        }
    }

    // ── Debug Overlay (F1) ──
    if (game.show_debug_overlay) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({sw - 200, 0}, {200, 80}, {0,0}, {1,1}, {0, 0, 0, 0.7f});
        char buf[128];
        std::snprintf(buf, sizeof(buf), "FPS: %.0f", game.debug_fps);
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 8}, {0.2f, 1, 0.2f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "Particles: %d", game.debug_particle_count);
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 28}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "NPCs: %d", (int)game.npcs.size());
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 48}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "Tweens: %d", (int)game.tween_system.tweens.size());
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 68}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
    }

    // ── Pause Menu (dim overlay only — layout is script-driven) ──
    if (game.paused) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, 0.65f});
    }
}
