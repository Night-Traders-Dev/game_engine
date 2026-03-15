#include "engine/resource/game_manifest.h"
#include "engine/resource/file_io.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace eb {

// Minimal JSON helpers (same pattern as tile_map loader)
static void jskip(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
}
static std::string jstr(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '"') return "";
    i++; std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i]=='\\' && i+1<s.size()) { i++; out+=s[i]; } else out+=s[i]; i++;
    }
    if (i < s.size()) i++;
    return out;
}
static double jnum(const std::string& s, size_t& i) {
    size_t start = i;
    if (i<s.size() && s[i]=='-') i++;
    while (i<s.size() && ((s[i]>='0'&&s[i]<='9')||s[i]=='.')) i++;
    if (i == start) return 0;
    return std::stod(s.substr(start, i-start));
}
static bool jbool(const std::string& s, size_t& i) {
    if (s.compare(i, 4, "true") == 0) { i+=4; return true; }
    if (s.compare(i, 5, "false") == 0) { i+=5; return false; }
    return false;
}
static void jskipval(const std::string& s, size_t& i);
static void jskipobj(const std::string& s, size_t& i) {
    if (i>=s.size()||s[i]!='{') return; i++;
    while (i<s.size()&&s[i]!='}') { jskip(s,i); if(s[i]=='}')break; jstr(s,i); jskip(s,i); if(s[i]==':')i++; jskip(s,i); jskipval(s,i); jskip(s,i); if(s[i]==',')i++; }
    if (i<s.size()) i++;
}
static void jskiparr(const std::string& s, size_t& i) {
    if (i>=s.size()||s[i]!='[') return; i++;
    while (i<s.size()&&s[i]!=']') { jskip(s,i); if(s[i]==']')break; jskipval(s,i); jskip(s,i); if(s[i]==',')i++; }
    if (i<s.size()) i++;
}
static void jskipval(const std::string& s, size_t& i) {
    jskip(s,i); if(i>=s.size()) return;
    if(s[i]=='"') jstr(s,i);
    else if(s[i]=='{') jskipobj(s,i);
    else if(s[i]=='[') jskiparr(s,i);
    else while(i<s.size()&&s[i]!=','&&s[i]!='}'&&s[i]!=']'&&s[i]!=' '&&s[i]!='\n') i++;
}

static SkillsDef parse_skills(const std::string& s, size_t& i) {
    SkillsDef sk;
    if (s[i]!='{') return sk; i++;
    while (i<s.size()&&s[i]!='}') {
        jskip(s,i); if(s[i]=='}')break;
        std::string k=jstr(s,i); jskip(s,i); if(s[i]==':')i++; jskip(s,i);
        if (k=="hardiness") sk.hardiness=(int)jnum(s,i);
        else if (k=="unholiness") sk.unholiness=(int)jnum(s,i);
        else if (k=="nerve") sk.nerve=(int)jnum(s,i);
        else if (k=="tactics") sk.tactics=(int)jnum(s,i);
        else if (k=="exorcism") sk.exorcism=(int)jnum(s,i);
        else if (k=="riflery") sk.riflery=(int)jnum(s,i);
        else jskipval(s,i);
        jskip(s,i); if(s[i]==',')i++;
    }
    if (i<s.size()) i++;
    return sk;
}

static CharacterDef parse_character(const std::string& s, size_t& i) {
    CharacterDef ch;
    if (s[i]!='{') return ch; i++;
    while (i<s.size()&&s[i]!='}') {
        jskip(s,i); if(s[i]=='}')break;
        std::string k=jstr(s,i); jskip(s,i); if(s[i]==':')i++; jskip(s,i);
        if (k=="name") ch.name=jstr(s,i);
        else if (k=="sprite") ch.sprite_path=jstr(s,i);
        else if (k=="sprite_grid") {
            if(s[i]=='['){i++; jskip(s,i); ch.sprite_grid_w=(int)jnum(s,i); jskip(s,i); if(s[i]==',')i++; jskip(s,i); ch.sprite_grid_h=(int)jnum(s,i); jskip(s,i); if(s[i]==']')i++;}
        }
        else if (k=="sprite_regions") {
            if(s[i]=='{'){i++;
                while(i<s.size()&&s[i]!='}'){
                    jskip(s,i); if(s[i]=='}')break;
                    std::string rn=jstr(s,i); jskip(s,i); if(s[i]==':')i++; jskip(s,i);
                    if(s[i]=='['){i++; jskip(s,i);
                        int rx=(int)jnum(s,i); jskip(s,i); if(s[i]==',')i++; jskip(s,i);
                        int ry=(int)jnum(s,i); jskip(s,i); if(s[i]==',')i++; jskip(s,i);
                        int rw=(int)jnum(s,i); jskip(s,i); if(s[i]==',')i++; jskip(s,i);
                        int rh=(int)jnum(s,i); jskip(s,i); if(s[i]==']')i++;
                        ch.custom_regions.push_back({rn,rx,ry,rw,rh});
                    }
                    jskip(s,i); if(s[i]==',')i++;
                }
                if(i<s.size())i++;
            }
        }
        else if (k=="portrait") ch.portrait_path=jstr(s,i);
        else if (k=="hp") ch.hp=(int)jnum(s,i);
        else if (k=="hp_max") ch.hp_max=(int)jnum(s,i);
        else if (k=="atk") ch.atk=(int)jnum(s,i);
        else if (k=="def") ch.def=(int)jnum(s,i);
        else if (k=="level") ch.level=(int)jnum(s,i);
        else if (k=="xp") ch.xp=(int)jnum(s,i);
        else if (k=="start_x") ch.start_x=(float)jnum(s,i);
        else if (k=="start_y") ch.start_y=(float)jnum(s,i);
        else if (k=="skills") ch.skills=parse_skills(s,i);
        else jskipval(s,i);
        jskip(s,i); if(s[i]==',')i++;
    }
    if (i<s.size()) i++;
    return ch;
}

static NPCDef parse_npc(const std::string& s, size_t& i) {
    NPCDef npc;
    if (s[i]!='{') return npc; i++;
    while (i<s.size()&&s[i]!='}') {
        jskip(s,i); if(s[i]=='}')break;
        std::string k=jstr(s,i); jskip(s,i); if(s[i]==':')i++; jskip(s,i);
        if (k=="name") npc.name=jstr(s,i);
        else if (k=="x") npc.x=(float)jnum(s,i);
        else if (k=="y") npc.y=(float)jnum(s,i);
        else if (k=="dir") npc.dir=(int)jnum(s,i);
        else if (k=="sprite") npc.sprite_path=jstr(s,i);
        else if (k=="sprite_grid") {
            if(s[i]=='['){i++; jskip(s,i); npc.sprite_grid_w=(int)jnum(s,i); jskip(s,i); if(s[i]==',')i++; jskip(s,i); npc.sprite_grid_h=(int)jnum(s,i); jskip(s,i); if(s[i]==']')i++;}
        }
        else if (k=="portrait") npc.portrait_path=jstr(s,i);
        else if (k=="dialogue_file") npc.dialogue_file=jstr(s,i);
        else if (k=="interact_radius") npc.interact_radius=(float)jnum(s,i);
        else if (k=="hostile") npc.hostile=jbool(s,i);
        else if (k=="aggro_range") npc.aggro_range=(float)jnum(s,i);
        else if (k=="attack_range") npc.attack_range=(float)jnum(s,i);
        else if (k=="move_speed") npc.move_speed=(float)jnum(s,i);
        else if (k=="wander_interval") npc.wander_interval=(float)jnum(s,i);
        else if (k=="has_battle") npc.has_battle=jbool(s,i);
        else if (k=="battle_enemy") npc.battle_enemy=jstr(s,i);
        else if (k=="battle_hp") npc.battle_hp=(int)jnum(s,i);
        else if (k=="battle_atk") npc.battle_atk=(int)jnum(s,i);
        else jskipval(s,i);
        jskip(s,i); if(s[i]==',')i++;
    }
    if (i<s.size()) i++;
    return npc;
}

bool load_game_manifest(GameManifest& manifest, const std::string& path) {
    auto data = FileIO::read_file(path);
    if (data.empty()) {
        std::fprintf(stderr, "[Manifest] Failed to load: %s\n", path.c_str());
        return false;
    }
    std::string json(data.begin(), data.end());

    // Extract base directory from path
    manifest.base_path = path;
    auto slash = manifest.base_path.rfind('/');
    if (slash != std::string::npos) manifest.base_path = manifest.base_path.substr(0, slash);
    else manifest.base_path = ".";

    size_t i = 0;
    jskip(json, i);
    if (i >= json.size() || json[i] != '{') return false;
    i++;

    while (i < json.size() && json[i] != '}') {
        jskip(json, i); if (json[i]=='}') break;
        std::string key = jstr(json, i);
        jskip(json, i); if (json[i]==':') i++; jskip(json, i);

        if (key == "game") {
            if (json[i]=='{'){i++;
                while(i<json.size()&&json[i]!='}'){
                    jskip(json,i); if(json[i]=='}')break;
                    std::string gk=jstr(json,i); jskip(json,i); if(json[i]==':')i++; jskip(json,i);
                    if (gk=="title") manifest.title=jstr(json,i);
                    else if (gk=="version") manifest.version=jstr(json,i);
                    else if (gk=="window_width") manifest.window_width=(int)jnum(json,i);
                    else if (gk=="window_height") manifest.window_height=(int)jnum(json,i);
                    else jskipval(json,i);
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "player") manifest.player = parse_character(json, i);
        else if (key == "party") {
            if(json[i]=='['){i++;
                while(i<json.size()&&json[i]!=']'){
                    jskip(json,i); if(json[i]==']')break;
                    manifest.party.push_back(parse_character(json,i));
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "tileset") manifest.tileset_path=jstr(json,i);
        else if (key == "dialog_bg") manifest.dialog_bg_path=jstr(json,i);
        else if (key == "default_font") manifest.default_font=jstr(json,i);
        else if (key == "npcs") {
            if(json[i]=='['){i++;
                while(i<json.size()&&json[i]!=']'){
                    jskip(json,i); if(json[i]==']')break;
                    manifest.npcs.push_back(parse_npc(json,i));
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "scripts") {
            if(json[i]=='['){i++;
                while(i<json.size()&&json[i]!=']'){
                    jskip(json,i); if(json[i]==']')break;
                    manifest.scripts.push_back(jstr(json,i));
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "init_scripts") {
            if(json[i]=='['){i++;
                while(i<json.size()&&json[i]!=']'){
                    jskip(json,i); if(json[i]==']')break;
                    manifest.init_scripts.push_back(jstr(json,i));
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "audio") {
            if(json[i]=='{'){i++;
                while(i<json.size()&&json[i]!='}'){
                    jskip(json,i); if(json[i]=='}')break;
                    std::string ak=jstr(json,i); jskip(json,i); if(json[i]==':')i++; jskip(json,i);
                    if (ak=="overworld") manifest.audio.overworld=jstr(json,i);
                    else if (ak=="battle") manifest.audio.battle=jstr(json,i);
                    else jskipval(json,i);
                    jskip(json,i); if(json[i]==',')i++;
                }
                if(i<json.size())i++;
            }
        }
        else if (key == "default_map") manifest.default_map=jstr(json,i);
        else jskipval(json, i);
        jskip(json, i); if (json[i]==',') i++;
    }

    manifest.loaded = true;
    std::printf("[Manifest] Loaded: %s (%s v%s)\n",
                manifest.title.c_str(), path.c_str(), manifest.version.c_str());
    std::printf("[Manifest] Player: %s, %zu party members, %zu NPCs, %zu scripts\n",
                manifest.player.name.c_str(), manifest.party.size(),
                manifest.npcs.size(), manifest.scripts.size());
    return true;
}

} // namespace eb
