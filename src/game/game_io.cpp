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
    try { return std::stod(s.substr(start, i-start)); }
    catch (...) { return 0.0; }
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

// Auto-mark water/ice tiles as reflective so reflections work without manual painting
void auto_mark_reflective_tiles(GameState& game) {
    int w = game.tile_map.width(), h = game.tile_map.height();
    if (w <= 0 || h <= 0) return;
    int count = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Check all layers for water tiles
            bool is_water = false;
            for (int layer = 0; layer < game.tile_map.layer_count(); layer++) {
                int raw = game.tile_map.tile_at(layer, x, y);
                int tile_id = raw & 0xFFFF;  // Extract tile ID (strip rotation/flip bits)
                if (tile_id >= TILE_WATER_DEEP && tile_id <= TILE_WATER_BLOOD) {
                    is_water = true;
                    break;
                }
                // Also check for shallow water and water objects
                if (tile_id >= TILE_BENCH_WATER && tile_id <= TILE_ROCK_WATER3) {
                    is_water = true;
                    break;
                }
            }
            if (is_water && !game.tile_map.is_reflective(x, y)) {
                game.tile_map.set_reflective_at(x, y, true);
                count++;
            }
        }
    }
    if (count > 0)
        std::printf("[Map] Auto-marked %d water tiles as reflective\n", count);
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

    // Auto-mark water tiles as reflective if no reflection data in map file
    auto_mark_reflective_tiles(game);

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

