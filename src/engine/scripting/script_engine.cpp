#include "engine/scripting/script_engine.h"
#include "engine/resource/file_io.h"
#include "engine/core/debug_log.h"
#include "game/game.h"
#include "game/ui/merchant_ui.h"
#include "game/ai/pathfinding.h"
#include "game/systems/day_night.h"
#include "engine/audio/audio_engine.h"

extern "C" {
#include <setjmp.h>
#include "interpreter.h"
#include "parser.h"
#include "lexer.h"
#include "env.h"
#include "value.h"
#include "ast.h"
#include "gc.h"
#include "module.h"

void init_lexer(const char* source, const char* filename);
void parser_init(void);
Stmt* parse(void);

// These are normally in main.c which we don't compile
int g_repl_mode = 0;
jmp_buf g_repl_error_jmp;

static Stmt** s_program_stmts = nullptr;
static int s_program_count = 0;
static int s_program_cap = 0;

void retain_program_stmt(Stmt* stmt) {
    if (s_program_count >= s_program_cap) {
        s_program_cap = s_program_cap < 16 ? 16 : s_program_cap * 2;
        s_program_stmts = (Stmt**)realloc(s_program_stmts, sizeof(Stmt*) * s_program_cap);
    }
    s_program_stmts[s_program_count++] = stmt;
}

// Stubs for network modules we don't need in the game engine
Module* create_socket_module(ModuleCache* c) { (void)c; return NULL; }
Module* create_tcp_module(ModuleCache* c)    { (void)c; return NULL; }
Module* create_http_module(ModuleCache* c)   { (void)c; return NULL; }
Module* create_ssl_module(ModuleCache* c)    { (void)c; return NULL; }

#ifdef _WIN32
// Windows doesn't have realpath — provide a simple shim
char* realpath(const char* path, char* resolved) {
    if (!resolved) resolved = (char*)malloc(4096);
    if (resolved) strncpy(resolved, path, 4095);
    return resolved;
}
#endif
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

namespace eb {

static ScriptEngine* s_active_engine = nullptr;

// ═══════════════ Core API ═══════════════

static Value native_say(int argc, Value* args) {
    if (argc < 2) return val_nil();
    const char* speaker = (args[0].type == VAL_STRING) ? args[0].as.string : "???";
    const char* text = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    DLOG_SCRIPT("%s: %s", speaker, text);
    // Queue dialogue line into the game's dialogue box if available
    if (s_active_engine && s_active_engine->game_state_) {
        auto& dlg = s_active_engine->game_state_->dialogue;
        if (!dlg.is_active()) {
            // Start new dialogue with this line
            dlg.start({{speaker, text}});
        } else {
            // Queue additional line
            dlg.queue_line({speaker, text});
        }
    }
    return val_nil();
}

static Value native_log(int argc, Value* args) {
    if (argc < 1) return val_nil();
    if (args[0].type == VAL_STRING) DLOG_SCRIPT("%s", args[0].as.string);
    else if (args[0].type == VAL_NUMBER) DLOG_SCRIPT("%.2f", args[0].as.number);
    else if (args[0].type == VAL_BOOL) DLOG_SCRIPT("%s", args[0].as.boolean ? "true" : "false");
    return val_nil();
}

static std::vector<std::pair<std::string, Value>> s_flags;

static Value native_set_flag(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    const char* name = args[0].as.string;
    for (auto& f : s_flags) {
        if (f.first == name) { f.second = args[1]; return val_nil(); }
    }
    s_flags.push_back({name, args[1]});
    return val_nil();
}

static Value native_get_flag(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_number(0);
    for (auto& f : s_flags) {
        if (f.first == args[0].as.string) return f.second;
    }
    return val_number(0);
}

// ═══════════════ Battle API ═══════════════

// random(min, max) — returns random int in [min, max]
static Value native_random(int argc, Value* args) {
    if (argc < 2) return val_number(0);
    int mn = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int mx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 1;
    if (mx <= mn) return val_number(mn);
    return val_number(mn + (rand() % (mx - mn + 1)));
}

// clamp(value, min, max)
static Value native_clamp(int argc, Value* args) {
    if (argc < 3) return val_number(0);
    double v = (args[0].type == VAL_NUMBER) ? args[0].as.number : 0;
    double lo = (args[1].type == VAL_NUMBER) ? args[1].as.number : 0;
    double hi = (args[2].type == VAL_NUMBER) ? args[2].as.number : 0;
    return val_number(v < lo ? lo : (v > hi ? hi : v));
}

// ═══════════════ Inventory API ═══════════════

// add_item(id, name, qty, type_str, desc, heal, dmg, element, sage_func)
static Value native_add_item(int argc, Value* args) {
    if (!s_active_engine || argc < 2) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs) return val_bool(0);

    const char* id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* name = (args[1].type == VAL_STRING) ? args[1].as.string : id;
    int qty = (argc > 2 && args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 1;
    const char* type_str = (argc > 3 && args[3].type == VAL_STRING) ? args[3].as.string : "consumable";
    const char* desc = (argc > 4 && args[4].type == VAL_STRING) ? args[4].as.string : "";
    int heal = (argc > 5 && args[5].type == VAL_NUMBER) ? (int)args[5].as.number : 0;
    int dmg = (argc > 6 && args[6].type == VAL_NUMBER) ? (int)args[6].as.number : 0;
    const char* elem = (argc > 7 && args[7].type == VAL_STRING) ? args[7].as.string : "";
    const char* sage = (argc > 8 && args[8].type == VAL_STRING) ? args[8].as.string : "";

    ItemType t = ItemType::Consumable;
    if (std::strcmp(type_str, "weapon") == 0) t = ItemType::Weapon;
    else if (std::strcmp(type_str, "key") == 0) t = ItemType::KeyItem;

    bool ok = gs->inventory.add(id, name, qty, t, desc, heal, dmg, elem, sage);
    std::printf("[Inventory] +%d %s (%s)\n", qty, name, id);
    return val_bool(ok ? 1 : 0);
}

// remove_item(id, qty)
static Value native_remove_item(int argc, Value* args) {
    if (!s_active_engine || argc < 1) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs) return val_bool(0);

    const char* id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    int qty = (argc > 1 && args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 1;

    bool ok = gs->inventory.remove(id, qty);
    std::printf("[Inventory] -%d %s\n", qty, id);
    return val_bool(ok ? 1 : 0);
}

// has_item(id) -> bool
static Value native_has_item(int argc, Value* args) {
    if (!s_active_engine || argc < 1) return val_bool(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs) return val_bool(0);

    const char* id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_bool(gs->inventory.count(id) > 0 ? 1 : 0);
}

// item_count(id) -> number
static Value native_item_count(int argc, Value* args) {
    if (!s_active_engine || argc < 1) return val_number(0);
    auto* gs = s_active_engine->game_state_;
    if (!gs) return val_number(0);

    const char* id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    return val_number(gs->inventory.count(id));
}

// ═══════════════ Skills API ═══════════════

static CharacterStats* resolve_skills(const char* character) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    auto* gs = s_active_engine->game_state_;
    if (std::strcmp(character, "player") == 0 || std::strcmp(character, "Player") == 0)
        return &gs->player_stats;
    if (std::strcmp(character, "ally") == 0 || std::strcmp(character, "Ally") == 0)
        return &gs->ally_stats;
    return nullptr;
}

static int* resolve_skill_field(CharacterStats* skills, const char* name) {
    if (!skills) return nullptr;
    if (std::strcmp(name, "vitality") == 0)   return &skills->vitality;
    if (std::strcmp(name, "arcana") == 0)     return &skills->arcana;
    if (std::strcmp(name, "agility") == 0)    return &skills->agility;
    if (std::strcmp(name, "tactics") == 0)    return &skills->tactics;
    if (std::strcmp(name, "spirit") == 0)     return &skills->spirit;
    if (std::strcmp(name, "strength") == 0)   return &skills->strength;
    return nullptr;
}

// get_skill(character, skill_name) -> number
static Value native_get_skill(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_number(0);
    auto* skills = resolve_skills(args[0].as.string);
    auto* field = resolve_skill_field(skills, args[1].as.string);
    return val_number(field ? *field : 0);
}

// set_skill(character, skill_name, value)
static Value native_set_skill(int argc, Value* args) {
    if (argc < 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_nil();
    auto* skills = resolve_skills(args[0].as.string);
    auto* field = resolve_skill_field(skills, args[1].as.string);
    if (field) {
        int val = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 5;
        *field = std::max(CharacterStats::MIN_STAT, std::min(CharacterStats::MAX_STAT, val));
    }
    return val_nil();
}

// get_skill_bonus(character, bonus_type) -> number
static Value native_get_skill_bonus(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_number(0);
    auto* skills = resolve_skills(args[0].as.string);
    if (!skills) return val_number(0);
    const char* bonus = args[1].as.string;
    if (std::strcmp(bonus, "hp") == 0)         return val_number(skills->hp_bonus());
    if (std::strcmp(bonus, "crit") == 0)       return val_number(skills->crit_chance() * 100);
    if (std::strcmp(bonus, "defense") == 0)    return val_number(skills->defense_bonus());
    if (std::strcmp(bonus, "magic_mult") == 0)  return val_number(skills->magic_damage_mult() * 100);
    if (std::strcmp(bonus, "weapon_dmg") == 0)  return val_number(skills->weapon_damage_bonus());
    if (std::strcmp(bonus, "dodge") == 0)       return val_number(skills->dodge_chance() * 100);
    if (std::strcmp(bonus, "spell_mult") == 0)  return val_number(skills->spell_power_mult() * 100);
    return val_number(0);
}

// ═══════════════ Debug API ═══════════════

static Value native_debug_log(int argc, Value* args) {
    if (argc < 1) return val_nil();
    if (args[0].type == VAL_STRING) DLOG_DEBUG("%s", args[0].as.string);
    else if (args[0].type == VAL_NUMBER) DLOG_DEBUG("%.2f", args[0].as.number);
    else if (args[0].type == VAL_BOOL) DLOG_DEBUG("%s", args[0].as.boolean ? "true" : "false");
    return val_nil();
}

static Value native_debug_warn(int argc, Value* args) {
    if (argc < 1) return val_nil();
    const char* msg = (args[0].type == VAL_STRING) ? args[0].as.string : "?";
    DLOG_WARN("%s", msg);
    return val_nil();
}

static Value native_debug_error(int argc, Value* args) {
    if (argc < 1) return val_nil();
    const char* msg = (args[0].type == VAL_STRING) ? args[0].as.string : "?";
    DLOG_ERROR("%s", msg);
    return val_nil();
}

static Value native_debug_info(int argc, Value* args) {
    if (argc < 1) return val_nil();
    const char* msg = (args[0].type == VAL_STRING) ? args[0].as.string : "?";
    DLOG_INFO("%s", msg);
    return val_nil();
}

static Value native_debug_print(int argc, Value* args) {
    // Print multiple args as a single line
    std::string out;
    for (int i = 0; i < argc; i++) {
        if (i > 0) out += " ";
        if (args[i].type == VAL_STRING) out += args[i].as.string;
        else if (args[i].type == VAL_NUMBER) {
            char buf[32]; std::snprintf(buf, sizeof(buf), "%.4g", args[i].as.number);
            out += buf;
        }
        else if (args[i].type == VAL_BOOL) out += args[i].as.boolean ? "true" : "false";
        else out += "<nil>";
    }
    DLOG_SCRIPT("%s", out.c_str());
    return val_nil();
}

static Value native_debug_assert(int argc, Value* args) {
    if (argc < 1) return val_nil();
    bool cond = (args[0].type == VAL_BOOL) ? args[0].as.boolean :
                (args[0].type == VAL_NUMBER) ? (args[0].as.number != 0) : false;
    if (!cond) {
        const char* msg = (argc > 1 && args[1].type == VAL_STRING) ? args[1].as.string : "Assertion failed";
        DLOG_ERROR("ASSERT: %s", msg);
    }
    return val_nil();
}

// ═══════════════ Shop API ═══════════════

// Temporary shop item list built up by script calls, then opened
static std::vector<eb::ShopItem> s_pending_shop_items;
static std::string s_pending_shop_name;

// add_shop_item(id, name, price, type, description, heal, dmg, element, sage_func)
static Value native_add_shop_item(int argc, Value* args) {
    if (argc < 3) return val_nil();
    eb::ShopItem item;
    item.id   = (args[0].type == VAL_STRING) ? args[0].as.string : "item";
    item.name = (args[1].type == VAL_STRING) ? args[1].as.string : "Item";
    item.price = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 10;
    if (argc > 3) {
        const char* type_str = (args[3].type == VAL_STRING) ? args[3].as.string : "consumable";
        if (std::strcmp(type_str, "weapon") == 0) item.type = eb::ShopItemType::Weapon;
        else if (std::strcmp(type_str, "key") == 0) item.type = eb::ShopItemType::KeyItem;
        else item.type = eb::ShopItemType::Consumable;
    }
    if (argc > 4 && args[4].type == VAL_STRING) item.description = args[4].as.string;
    if (argc > 5 && args[5].type == VAL_NUMBER) item.heal_hp = (int)args[5].as.number;
    if (argc > 6 && args[6].type == VAL_NUMBER) item.damage = (int)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_STRING) item.element = args[7].as.string;
    if (argc > 8 && args[8].type == VAL_STRING) item.sage_func = args[8].as.string;
    s_pending_shop_items.push_back(item);
    return val_nil();
}

// open_shop(merchant_name) — opens the merchant UI with all pending items
static Value native_open_shop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    std::string name = "Merchant";
    if (argc > 0 && args[0].type == VAL_STRING) name = args[0].as.string;
    s_active_engine->game_state_->merchant_ui.open(s_pending_shop_items, name);
    s_pending_shop_items.clear();
    return val_nil();
}

// set_gold(amount) — set player gold
static Value native_set_gold(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    if (argc > 0 && args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->gold = (int)args[0].as.number;
    return val_nil();
}

// get_gold() — get player gold
static Value native_get_gold(int argc, Value* /*args*/) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number(s_active_engine->game_state_->gold);
}

// ═══════════════ Helper ═══════════════

static NPC* find_npc_by_name(const std::string& name) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    for (auto& npc : s_active_engine->game_state_->npcs)
        if (npc.name == name) return &npc;
    return nullptr;
}

// ═══════════════ Day-Night API ═══════════════

// get_hour() -> number (0-23)
static Value native_get_hour(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    return val_number((int)s_active_engine->game_state_->day_night.game_hours);
}

// get_minute() -> number (0-59)
static Value native_get_minute(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_number(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_number((int)((h - (int)h) * 60.0f));
}

// set_time(hour, minute)
static Value native_set_time(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    float h = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float m = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    s_active_engine->game_state_->day_night.game_hours = h + m / 60.0f;
    return val_nil();
}

// set_day_speed(multiplier)
static Value native_set_day_speed(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    if (args[0].type == VAL_NUMBER)
        s_active_engine->game_state_->day_night.day_speed = (float)args[0].as.number;
    return val_nil();
}

// is_day() -> bool (6-18)
static Value native_is_day(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_bool(h >= 6.0f && h < 18.0f);
}

// is_night() -> bool
static Value native_is_night(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    float h = s_active_engine->game_state_->day_night.game_hours;
    return val_bool(h >= 18.0f || h < 6.0f);
}

// ═══════════════ UI API ═══════════════

// ui_label(id, text, x, y, r, g, b, a)
static Value native_ui_label(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string text = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float x = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float y = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float r=1,g=1,b=1,a=1;
    if (argc > 4 && args[4].type == VAL_NUMBER) r = (float)args[4].as.number;
    if (argc > 5 && args[5].type == VAL_NUMBER) g = (float)args[5].as.number;
    if (argc > 6 && args[6].type == VAL_NUMBER) b = (float)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) a = (float)args[7].as.number;
    for (auto& l : gs->script_ui.labels) {
        if (l.id == id) { l.text = text; l.position = {x,y}; l.color = {r,g,b,a}; return val_nil(); }
    }
    gs->script_ui.labels.push_back({id, text, {x,y}, {r,g,b,a}});
    return val_nil();
}

// ui_bar(id, value, max, x, y, w, h, r, g, b, a)
static Value native_ui_bar(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float val = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float mx = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 100;
    float x = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float y = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float w = (argc > 5 && args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 100;
    float h = (argc > 6 && args[6].type == VAL_NUMBER) ? (float)args[6].as.number : 12;
    float r=0.2f,g=0.8f,b=0.2f,a=1;
    if (argc > 7 && args[7].type == VAL_NUMBER) r = (float)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_NUMBER) g = (float)args[8].as.number;
    if (argc > 9 && args[9].type == VAL_NUMBER) b = (float)args[9].as.number;
    if (argc > 10 && args[10].type == VAL_NUMBER) a = (float)args[10].as.number;
    for (auto& bar : gs->script_ui.bars) {
        if (bar.id == id) { bar.value=val; bar.max_value=mx; bar.position={x,y}; bar.width=w; bar.height=h; bar.color={r,g,b,a}; return val_nil(); }
    }
    gs->script_ui.bars.push_back({id, val, mx, {x,y}, w, h, {r,g,b,a}});
    return val_nil();
}

// ui_remove(id)
static Value native_ui_remove(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& labels = gs->script_ui.labels;
    labels.erase(std::remove_if(labels.begin(), labels.end(), [&](auto& l){return l.id==id;}), labels.end());
    auto& bars = gs->script_ui.bars;
    bars.erase(std::remove_if(bars.begin(), bars.end(), [&](auto& b){return b.id==id;}), bars.end());
    return val_nil();
}

// ui_notify(text, duration)
static Value native_ui_notify(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string text = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float dur = (argc > 1 && args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 3.0f;
    s_active_engine->game_state_->script_ui.notifications.push_back({text, dur, 0.0f});
    return val_nil();
}

// ui_panel(id, x, y, w, h, sprite_region)
static Value native_ui_panel(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 5) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 100;
    float h = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 60;
    std::string region = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "panel_hud_wide";
    for (auto& p : gs->script_ui.panels) {
        if (p.id == id) { p.position={x,y}; p.width=w; p.height=h; p.sprite_region=region; return val_nil(); }
    }
    gs->script_ui.panels.push_back({id, {x,y}, w, h, region});
    return val_nil();
}

// ui_image(id, x, y, w, h, icon_name)
static Value native_ui_image(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 6) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    float w = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 32;
    float h = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 32;
    std::string icon = (args[5].type == VAL_STRING) ? args[5].as.string : "";
    for (auto& img : gs->script_ui.images) {
        if (img.id == id) { img.position={x,y}; img.width=w; img.height=h; img.icon_name=icon; return val_nil(); }
    }
    gs->script_ui.images.push_back({id, {x,y}, w, h, icon});
    return val_nil();
}

// ui_set(id, property, value) — modify any UI component's properties
static Value native_ui_set(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* prop = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float nv = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    bool bv = (args[2].type == VAL_BOOL) ? args[2].as.boolean : (nv != 0);
    const char* sv = (args[2].type == VAL_STRING) ? args[2].as.string : "";

    // Search labels
    for (auto& l : gs->script_ui.labels) {
        if (l.id != id) continue;
        if (std::strcmp(prop, "x") == 0) l.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) l.position.y = nv;
        else if (std::strcmp(prop, "scale") == 0) l.scale = nv;
        else if (std::strcmp(prop, "text") == 0) l.text = sv;
        else if (std::strcmp(prop, "visible") == 0) l.visible = bv;
        else if (std::strcmp(prop, "r") == 0) l.color.x = nv;
        else if (std::strcmp(prop, "g") == 0) l.color.y = nv;
        else if (std::strcmp(prop, "b") == 0) l.color.z = nv;
        else if (std::strcmp(prop, "a") == 0) l.color.w = nv;
        return val_nil();
    }
    // Search bars
    for (auto& b : gs->script_ui.bars) {
        if (b.id != id) continue;
        if (std::strcmp(prop, "x") == 0) b.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) b.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) b.width = nv;
        else if (std::strcmp(prop, "h") == 0) b.height = nv;
        else if (std::strcmp(prop, "value") == 0) b.value = nv;
        else if (std::strcmp(prop, "max") == 0) b.max_value = nv;
        else if (std::strcmp(prop, "visible") == 0) b.visible = bv;
        else if (std::strcmp(prop, "r") == 0) b.color.x = nv;
        else if (std::strcmp(prop, "g") == 0) b.color.y = nv;
        else if (std::strcmp(prop, "b") == 0) b.color.z = nv;
        else if (std::strcmp(prop, "a") == 0) b.color.w = nv;
        return val_nil();
    }
    // Search panels
    for (auto& p : gs->script_ui.panels) {
        if (p.id != id) continue;
        if (std::strcmp(prop, "x") == 0) p.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) p.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) p.width = nv;
        else if (std::strcmp(prop, "h") == 0) p.height = nv;
        else if (std::strcmp(prop, "sprite") == 0) p.sprite_region = sv;
        else if (std::strcmp(prop, "visible") == 0) p.visible = bv;
        return val_nil();
    }
    // Search images
    for (auto& img : gs->script_ui.images) {
        if (img.id != id) continue;
        if (std::strcmp(prop, "x") == 0) img.position.x = nv;
        else if (std::strcmp(prop, "y") == 0) img.position.y = nv;
        else if (std::strcmp(prop, "w") == 0) img.width = nv;
        else if (std::strcmp(prop, "h") == 0) img.height = nv;
        else if (std::strcmp(prop, "icon") == 0) img.icon_name = sv;
        else if (std::strcmp(prop, "visible") == 0) img.visible = bv;
        return val_nil();
    }
    return val_nil();
}

// ═══════════════ HUD Config API ═══════════════

// hud_set(property, value) — configure HUD dimensions and visibility
static Value native_hud_set(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto& H = s_active_engine->game_state_->hud;
    const char* prop = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float v = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    bool bv = (args[1].type == VAL_BOOL) ? args[1].as.boolean : (v != 0);

    if (std::strcmp(prop, "scale") == 0)          H.scale = v;
    else if (std::strcmp(prop, "player_x") == 0)  H.player_x = v;
    else if (std::strcmp(prop, "player_y") == 0)  H.player_y = v;
    else if (std::strcmp(prop, "player_w") == 0)  H.player_w = v;
    else if (std::strcmp(prop, "player_h") == 0)  H.player_h = v;
    else if (std::strcmp(prop, "hp_bar_w") == 0)  H.hp_bar_w = v;
    else if (std::strcmp(prop, "hp_bar_h") == 0)  H.hp_bar_h = v;
    else if (std::strcmp(prop, "text_scale") == 0) H.text_scale = v;
    else if (std::strcmp(prop, "time_w") == 0)    H.time_w = v;
    else if (std::strcmp(prop, "time_h") == 0)    H.time_h = v;
    else if (std::strcmp(prop, "time_text_scale") == 0) H.time_text_scale = v;
    else if (std::strcmp(prop, "inv_slot_size") == 0) H.inv_slot_size = v;
    else if (std::strcmp(prop, "inv_padding") == 0)   H.inv_padding = v;
    else if (std::strcmp(prop, "inv_max_slots") == 0) H.inv_max_slots = (int)v;
    else if (std::strcmp(prop, "inv_y_offset") == 0)  H.inv_y_offset = v;
    else if (std::strcmp(prop, "surv_bar_w") == 0) H.surv_bar_w = v;
    else if (std::strcmp(prop, "surv_bar_h") == 0) H.surv_bar_h = v;
    else if (std::strcmp(prop, "show_player") == 0)    H.show_player = bv;
    else if (std::strcmp(prop, "show_time") == 0)      H.show_time = bv;
    else if (std::strcmp(prop, "show_inventory") == 0)  H.show_inventory = bv;
    else if (std::strcmp(prop, "show_survival") == 0)   H.show_survival = bv;
    else if (std::strcmp(prop, "show_minimap") == 0)   H.show_minimap = bv;
    else if (std::strcmp(prop, "minimap_size") == 0)   H.minimap_size = v;
    return val_nil();
}

// hud_get(property) -> number
static Value native_hud_get(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_number(0);
    auto& H = s_active_engine->game_state_->hud;
    const char* prop = (args[0].type == VAL_STRING) ? args[0].as.string : "";

    if (std::strcmp(prop, "scale") == 0)          return val_number(H.scale);
    if (std::strcmp(prop, "player_x") == 0)       return val_number(H.player_x);
    if (std::strcmp(prop, "player_y") == 0)       return val_number(H.player_y);
    if (std::strcmp(prop, "player_w") == 0)       return val_number(H.player_w);
    if (std::strcmp(prop, "player_h") == 0)       return val_number(H.player_h);
    if (std::strcmp(prop, "hp_bar_w") == 0)       return val_number(H.hp_bar_w);
    if (std::strcmp(prop, "hp_bar_h") == 0)       return val_number(H.hp_bar_h);
    if (std::strcmp(prop, "text_scale") == 0)     return val_number(H.text_scale);
    if (std::strcmp(prop, "time_w") == 0)         return val_number(H.time_w);
    if (std::strcmp(prop, "time_h") == 0)         return val_number(H.time_h);
    if (std::strcmp(prop, "inv_slot_size") == 0)  return val_number(H.inv_slot_size);
    if (std::strcmp(prop, "inv_max_slots") == 0)  return val_number(H.inv_max_slots);
    if (std::strcmp(prop, "show_player") == 0)    return val_bool(H.show_player);
    if (std::strcmp(prop, "show_time") == 0)      return val_bool(H.show_time);
    if (std::strcmp(prop, "show_inventory") == 0) return val_bool(H.show_inventory);
    if (std::strcmp(prop, "show_survival") == 0)  return val_bool(H.show_survival);
    if (std::strcmp(prop, "show_minimap") == 0)  return val_bool(H.show_minimap);
    if (std::strcmp(prop, "minimap_size") == 0)  return val_number(H.minimap_size);
    if (std::strcmp(prop, "screen_w") == 0)     return val_number(H.screen_w);
    if (std::strcmp(prop, "screen_h") == 0)     return val_number(H.screen_h);
    return val_number(0);
}

// ═══════════════ Survival API ═══════════════

// enable_survival(bool)
static Value native_enable_survival(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    bool en = (args[0].type == VAL_BOOL) ? args[0].as.boolean : (args[0].type == VAL_NUMBER && args[0].as.number != 0);
    s_active_engine->game_state_->survival.enabled = en;
    return val_nil();
}

// get/set hunger, thirst, energy — all follow same pattern
static Value native_get_hunger(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.hunger); }
static Value native_set_hunger(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.hunger=(float)args[0].as.number; return val_nil(); }
static Value native_get_thirst(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.thirst); }
static Value native_set_thirst(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.thirst=(float)args[0].as.number; return val_nil(); }
static Value native_get_energy(int, Value*) { if (!s_active_engine||!s_active_engine->game_state_) return val_number(0); return val_number(s_active_engine->game_state_->survival.energy); }
static Value native_set_energy(int argc, Value* args) { if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil(); if(args[0].type==VAL_NUMBER) s_active_engine->game_state_->survival.energy=(float)args[0].as.number; return val_nil(); }

// set_survival_rate(stat, rate) — stat: "hunger", "thirst", "energy"
static Value native_set_survival_rate(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    const char* stat = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float rate = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    auto& s = s_active_engine->game_state_->survival;
    if (std::strcmp(stat, "hunger") == 0) s.hunger_rate = rate;
    else if (std::strcmp(stat, "thirst") == 0) s.thirst_rate = rate;
    else if (std::strcmp(stat, "energy") == 0) s.energy_rate = rate;
    return val_nil();
}

// ═══════════════ Pathfinding API ═══════════════

// npc_move_to(name, tile_x, tile_y)
static Value native_npc_move_to(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    NPC* npc = find_npc_by_name(name);
    if (!npc) return val_nil();
    auto& map = s_active_engine->game_state_->tile_map;
    int ts = map.tile_size();
    int sx = (int)(npc->position.x / ts);
    int sy = (int)(npc->position.y / ts);
    npc->current_path = eb::find_path(map, sx, sy, tx, ty);
    npc->path_index = 0;
    npc->path_active = !npc->current_path.empty();
    return val_bool(npc->path_active);
}

// ═══════════════ Route API ═══════════════

// npc_add_waypoint(name, x, y)
static Value native_npc_add_waypoint(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    npc->route.waypoints.push_back({x, y});
    return val_nil();
}

// npc_set_route(name, mode) — "patrol", "once", "pingpong"
static Value native_npc_set_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    const char* mode = (args[1].type == VAL_STRING) ? args[1].as.string : "patrol";
    if (std::strcmp(mode, "once") == 0) npc->route.mode = RouteMode::Once;
    else if (std::strcmp(mode, "pingpong") == 0) npc->route.mode = RouteMode::PingPong;
    else npc->route.mode = RouteMode::Patrol;
    return val_nil();
}

// npc_start_route(name)
static Value native_npc_start_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc && !npc->route.waypoints.empty()) { npc->route.active = true; npc->route.current_waypoint = 0; }
    return val_nil();
}

// npc_stop_route(name)
static Value native_npc_stop_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->route.active = false;
    return val_nil();
}

// npc_clear_route(name)
static Value native_npc_clear_route(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) { npc->route.waypoints.clear(); npc->route.active = false; npc->route.current_waypoint = 0; }
    return val_nil();
}

// ═══════════════ Schedule API ═══════════════

// npc_set_schedule(name, start_hour, end_hour)
static Value native_npc_set_schedule(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    npc->schedule.start_hour = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc->schedule.end_hour = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 24;
    npc->schedule.has_schedule = true;
    return val_nil();
}

// npc_set_spawn_point(name, x, y)
static Value native_npc_set_spawn_point(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (!npc) return val_nil();
    npc->schedule.spawn_point.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc->schedule.spawn_point.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    return val_nil();
}

// npc_clear_schedule(name)
static Value native_npc_clear_schedule(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) { npc->schedule.has_schedule = false; npc->schedule.currently_visible = true; }
    return val_nil();
}

// ═══════════════ NPC Interact API ═══════════════

// npc_on_meet(npc1, npc2, callback_func)
static Value native_npc_on_meet(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    NPCMeetTrigger t;
    t.npc1_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    t.npc2_name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    t.callback_func = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    s_active_engine->game_state_->npc_meet_triggers.push_back(t);
    return val_nil();
}

// npc_set_meet_radius(npc1, npc2, radius)
static Value native_npc_set_meet_radius(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    std::string n1 = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string n2 = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float r = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 40;
    for (auto& t : s_active_engine->game_state_->npc_meet_triggers)
        if (t.npc1_name == n1 && t.npc2_name == n2) { t.trigger_radius = r; break; }
    return val_nil();
}

// npc_face_each_other(npc1, npc2)
static Value native_npc_face_each_other(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    NPC* a = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    NPC* b = find_npc_by_name((args[1].type == VAL_STRING) ? args[1].as.string : "");
    if (!a || !b) return val_nil();
    float dx = b->position.x - a->position.x;
    float dy = b->position.y - a->position.y;
    if (std::abs(dx) > std::abs(dy)) { a->dir = (dx > 0) ? 3 : 2; b->dir = (dx > 0) ? 2 : 3; }
    else { a->dir = (dy > 0) ? 0 : 1; b->dir = (dy > 0) ? 1 : 0; }
    return val_nil();
}

// ═══════════════ Spawn API ═══════════════

// spawn_loop(npc_name, interval, max_count)
static Value native_spawn_loop(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    SpawnLoop loop;
    loop.npc_template_name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    loop.interval = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 60;
    loop.max_count = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 5;
    loop.active = true;
    s_active_engine->game_state_->spawn_loops.push_back(loop);
    return val_nil();
}

// stop_spawn_loop(npc_name)
static Value native_stop_spawn_loop(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<1) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops)
        if (l.npc_template_name == name) l.active = false;
    return val_nil();
}

// set_spawn_area(name, x1, y1, x2, y2)
static Value native_set_spawn_area(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<5) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops) {
        if (l.npc_template_name == name) {
            l.area_min.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
            l.area_min.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
            l.area_max.x = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
            l.area_max.y = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
            l.has_area = true;
            break;
        }
    }
    return val_nil();
}

// set_spawn_callback(npc_name, callback_func)
static Value native_set_spawn_callback(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<2) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string func = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    for (auto& l : s_active_engine->game_state_->spawn_loops)
        if (l.npc_template_name == name) { l.on_spawn_func = func; break; }
    return val_nil();
}

// set_spawn_time(name, start_hour, end_hour) — only spawn during these hours
static Value native_set_spawn_time(int argc, Value* args) {
    if (!s_active_engine||!s_active_engine->game_state_||argc<3) return val_nil();
    std::string name = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float start = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float end = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 24;
    for (auto& l : s_active_engine->game_state_->spawn_loops) {
        if (l.npc_template_name == name) {
            l.spawn_start_hour = start;
            l.spawn_end_hour = end;
            l.has_time_gate = true;
            break;
        }
    }
    return val_nil();
}

// ═══════════════ Map API ═══════════════

// spawn_npc(name, x, y, dir, hostile, sprite_id, hp, atk, speed, aggro)
static Value native_spawn_npc_map(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    NPC npc;
    npc.name = (args[0].type == VAL_STRING) ? args[0].as.string : "NPC";
    npc.position.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    npc.position.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    npc.home_pos = npc.position;
    npc.wander_target = npc.position;
    if (argc > 3 && args[3].type == VAL_NUMBER) npc.dir = (int)args[3].as.number;
    if (argc > 4) npc.hostile = (args[4].type == VAL_BOOL) ? args[4].as.boolean : (args[4].type == VAL_NUMBER && args[4].as.number != 0);
    if (argc > 5 && args[5].type == VAL_NUMBER) npc.sprite_atlas_id = (int)args[5].as.number;
    if (argc > 6 && args[6].type == VAL_NUMBER) { npc.battle_enemy_hp = (int)args[6].as.number; npc.has_battle = npc.battle_enemy_hp > 0; }
    if (argc > 7 && args[7].type == VAL_NUMBER) npc.battle_enemy_atk = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_NUMBER) npc.move_speed = (float)args[8].as.number;
    if (argc > 9 && args[9].type == VAL_NUMBER) npc.aggro_range = (float)args[9].as.number;
    npc.battle_enemy_name = npc.name;
    npc.dialogue = {{npc.name, "..."}};
    gs->npcs.push_back(npc);
    return val_nil();
}

// place_object(x, y, stamp_name)
static Value native_place_object(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    const char* stamp_name = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    // Find stamp by name
    for (int i = 0; i < (int)gs->object_stamps.size(); i++) {
        if (gs->object_stamps[i].name == stamp_name) {
            WorldObject obj;
            obj.sprite_id = i;
            obj.position = {x, y};
            gs->world_objects.push_back(obj);
            return val_nil();
        }
    }
    return val_nil();
}

// remove_object(x, y)
static Value native_remove_object(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float best_dist = 48.0f;
    int best_idx = -1;
    for (int i = 0; i < (int)gs->world_objects.size(); i++) {
        float dx = gs->world_objects[i].position.x - x;
        float dy = gs->world_objects[i].position.y - y;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < best_dist) { best_dist = d; best_idx = i; }
    }
    if (best_idx >= 0) gs->world_objects.erase(gs->world_objects.begin() + best_idx);
    return val_nil();
}

// set_portal(tx, ty, target_map, target_x, target_y, label)
static Value native_set_portal(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    gs->tile_map.set_collision_at(tx, ty, eb::CollisionType::Portal);
    eb::Portal p;
    p.tile_x = tx; p.tile_y = ty;
    p.target_map = (argc > 2 && args[2].type == VAL_STRING) ? args[2].as.string : "";
    p.target_x = (argc > 3 && args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    p.target_y = (argc > 4 && args[4].type == VAL_NUMBER) ? (int)args[4].as.number : 0;
    p.label = (argc > 5 && args[5].type == VAL_STRING) ? args[5].as.string : "portal";
    // Remove existing portal at same tile
    auto& portals = gs->tile_map.portals();
    for (int i = (int)portals.size()-1; i >= 0; i--)
        if (portals[i].tile_x == tx && portals[i].tile_y == ty)
            gs->tile_map.remove_portal(i);
    portals.push_back(p);
    return val_nil();
}

// remove_portal(tx, ty)
static Value native_remove_portal(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    gs->tile_map.set_collision_at(tx, ty, eb::CollisionType::None);
    auto& portals = gs->tile_map.portals();
    for (int i = (int)portals.size()-1; i >= 0; i--)
        if (portals[i].tile_x == tx && portals[i].tile_y == ty)
            gs->tile_map.remove_portal(i);
    return val_nil();
}

// set_collision(tx, ty, type) — type: 0=None, 1=Solid, 2=Portal
static Value native_set_collision(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    int tx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int ty = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int type = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    s_active_engine->game_state_->tile_map.set_collision_at(tx, ty, static_cast<eb::CollisionType>(type));
    return val_nil();
}

// set_tile(layer, tx, ty, tile_id)
static Value native_set_tile(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    int layer = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    int tx = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 0;
    int ty = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    int tile_id = (args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 0;
    s_active_engine->game_state_->tile_map.set_tile(layer, tx, ty, tile_id);
    return val_nil();
}

// ═══════════════ ScriptEngine Implementation ═══════════════

ScriptEngine::ScriptEngine() {
    gc_init();
    init_module_system();
    // Add script search paths for imports
    if (global_module_cache) {
        add_search_path(global_module_cache, "assets/scripts");
        add_search_path(global_module_cache, "assets/scripts/lib");
        add_search_path(global_module_cache, "assets/scripts/battle");
        add_search_path(global_module_cache, "assets/scripts/inventory");
        add_search_path(global_module_cache, "assets/scripts/maps");
        add_search_path(global_module_cache, ".");
    }
    env_ = env_create(nullptr);
    if (env_) {
        g_global_env = env_;  // Set global env for module imports
        init_stdlib(env_);
        register_engine_api();
        register_battle_api();
        register_inventory_api();
        register_skills_api();
        register_debug_api();
        register_shop_api();
        register_daynight_api();
        register_ui_api();
        register_survival_api();
        register_pathfinding_api();
        register_route_api();
        register_schedule_api();
        register_npc_interact_api();
        register_spawn_api();
        register_map_api();
        register_audio_api();
        s_active_engine = this;
    }
}

ScriptEngine::~ScriptEngine() {
    if (s_active_engine == this) s_active_engine = nullptr;
    if (env_) { env_cleanup_all(); env_ = nullptr; }
    for (auto* buf : source_buffers_) free(buf);
    source_buffers_.clear();
}

void ScriptEngine::register_engine_api() {
    if (!env_) return;
    env_define(env_, "say", 3, val_native(native_say));
    env_define(env_, "log", 3, val_native(native_log));
    env_define(env_, "set_flag", 8, val_native(native_set_flag));
    env_define(env_, "get_flag", 8, val_native(native_get_flag));
    env_define(env_, "random", 6, val_native(native_random));
    env_define(env_, "clamp", 5, val_native(native_clamp));
}

void ScriptEngine::register_battle_api() {
    if (!env_) return;
    // Battle state variables are synced before/after script calls
    // These are readable/writable SageLang globals:
    // enemy_hp, enemy_max_hp, enemy_atk, enemy_name
    // player_hp, player_max_hp, player_atk, player_def
    // ally_hp, ally_max_hp, ally_atk
    // active_fighter (0=Player, 1=Ally)
    // battle_result: set to "damage", "heal", "miss" etc by scripts
    // battle_damage: the amount of damage/healing
    // battle_target: "enemy", "player", "ally"
    std::printf("[ScriptEngine] Battle API registered\n");
}

void ScriptEngine::register_skills_api() {
    if (!env_) return;
    env_define(env_, "get_skill", 9, val_native(native_get_skill));
    env_define(env_, "set_skill", 9, val_native(native_set_skill));
    env_define(env_, "get_skill_bonus", 15, val_native(native_get_skill_bonus));
    std::printf("[ScriptEngine] Skills API registered\n");
}

void ScriptEngine::register_debug_api() {
    if (!env_) return;
    env_define(env_, "debug", 5, val_native(native_debug_log));
    env_define(env_, "warn", 4, val_native(native_debug_warn));
    env_define(env_, "error", 5, val_native(native_debug_error));
    env_define(env_, "info", 4, val_native(native_debug_info));
    env_define(env_, "print", 5, val_native(native_debug_print));
    env_define(env_, "assert_true", 11, val_native(native_debug_assert));
    std::printf("[ScriptEngine] Debug API registered\n");
}

void ScriptEngine::register_inventory_api() {
    if (!env_) return;
    env_define(env_, "add_item", 8, val_native(native_add_item));
    env_define(env_, "remove_item", 11, val_native(native_remove_item));
    env_define(env_, "has_item", 8, val_native(native_has_item));
    env_define(env_, "item_count", 10, val_native(native_item_count));
    std::printf("[ScriptEngine] Inventory API registered\n");
}

void ScriptEngine::register_shop_api() {
    if (!env_) return;
    env_define(env_, "add_shop_item", 13, val_native(native_add_shop_item));
    env_define(env_, "open_shop", 9, val_native(native_open_shop));
    env_define(env_, "set_gold", 8, val_native(native_set_gold));
    env_define(env_, "get_gold", 8, val_native(native_get_gold));
    std::printf("[ScriptEngine] Shop API registered\n");
}

void ScriptEngine::register_daynight_api() {
    if (!env_) return;
    env_define(env_, "get_hour", 8, val_native(native_get_hour));
    env_define(env_, "get_minute", 10, val_native(native_get_minute));
    env_define(env_, "set_time", 8, val_native(native_set_time));
    env_define(env_, "set_day_speed", 13, val_native(native_set_day_speed));
    env_define(env_, "is_day", 6, val_native(native_is_day));
    env_define(env_, "is_night", 8, val_native(native_is_night));
    std::printf("[ScriptEngine] Day-Night API registered\n");
}

void ScriptEngine::register_ui_api() {
    if (!env_) return;
    env_define(env_, "ui_label", 8, val_native(native_ui_label));
    env_define(env_, "ui_bar", 6, val_native(native_ui_bar));
    env_define(env_, "ui_remove", 9, val_native(native_ui_remove));
    env_define(env_, "ui_notify", 9, val_native(native_ui_notify));
    env_define(env_, "ui_panel", 8, val_native(native_ui_panel));
    env_define(env_, "ui_image", 8, val_native(native_ui_image));
    env_define(env_, "ui_set", 6, val_native(native_ui_set));
    env_define(env_, "hud_set", 7, val_native(native_hud_set));
    env_define(env_, "hud_get", 7, val_native(native_hud_get));
    std::printf("[ScriptEngine] UI API registered\n");
}

void ScriptEngine::register_survival_api() {
    if (!env_) return;
    env_define(env_, "enable_survival", 15, val_native(native_enable_survival));
    env_define(env_, "get_hunger", 10, val_native(native_get_hunger));
    env_define(env_, "set_hunger", 10, val_native(native_set_hunger));
    env_define(env_, "get_thirst", 10, val_native(native_get_thirst));
    env_define(env_, "set_thirst", 10, val_native(native_set_thirst));
    env_define(env_, "get_energy", 10, val_native(native_get_energy));
    env_define(env_, "set_energy", 10, val_native(native_set_energy));
    env_define(env_, "set_survival_rate", 17, val_native(native_set_survival_rate));
    std::printf("[ScriptEngine] Survival API registered\n");
}

void ScriptEngine::register_pathfinding_api() {
    if (!env_) return;
    env_define(env_, "npc_move_to", 11, val_native(native_npc_move_to));
    std::printf("[ScriptEngine] Pathfinding API registered\n");
}

void ScriptEngine::register_route_api() {
    if (!env_) return;
    env_define(env_, "npc_add_waypoint", 16, val_native(native_npc_add_waypoint));
    env_define(env_, "npc_set_route", 13, val_native(native_npc_set_route));
    env_define(env_, "npc_start_route", 15, val_native(native_npc_start_route));
    env_define(env_, "npc_stop_route", 14, val_native(native_npc_stop_route));
    env_define(env_, "npc_clear_route", 15, val_native(native_npc_clear_route));
    std::printf("[ScriptEngine] Route API registered\n");
}

void ScriptEngine::register_schedule_api() {
    if (!env_) return;
    env_define(env_, "npc_set_schedule", 16, val_native(native_npc_set_schedule));
    env_define(env_, "npc_set_spawn_point", 19, val_native(native_npc_set_spawn_point));
    env_define(env_, "npc_clear_schedule", 18, val_native(native_npc_clear_schedule));
    std::printf("[ScriptEngine] Schedule API registered\n");
}

void ScriptEngine::register_npc_interact_api() {
    if (!env_) return;
    env_define(env_, "npc_on_meet", 11, val_native(native_npc_on_meet));
    env_define(env_, "npc_set_meet_radius", 19, val_native(native_npc_set_meet_radius));
    env_define(env_, "npc_face_each_other", 19, val_native(native_npc_face_each_other));
    std::printf("[ScriptEngine] NPC Interact API registered\n");
}

void ScriptEngine::register_spawn_api() {
    if (!env_) return;
    env_define(env_, "spawn_loop", 10, val_native(native_spawn_loop));
    env_define(env_, "stop_spawn_loop", 15, val_native(native_stop_spawn_loop));
    env_define(env_, "set_spawn_area", 14, val_native(native_set_spawn_area));
    env_define(env_, "set_spawn_callback", 18, val_native(native_set_spawn_callback));
    env_define(env_, "set_spawn_time", 14, val_native(native_set_spawn_time));
    std::printf("[ScriptEngine] Spawn API registered\n");
}

// add_loot(enemy_name, item_id, item_name, drop_chance, type, desc, heal, dmg, element, sage_func)
static Value native_add_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string enemy = (args[0].type == VAL_STRING) ? args[0].as.string : "*";
    LootEntry entry;
    entry.item_id = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    entry.item_name = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    entry.drop_chance = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0.5f;
    if (argc > 4 && args[4].type == VAL_STRING) {
        const char* t = args[4].as.string;
        if (std::strcmp(t, "weapon") == 0) entry.type = ItemType::Weapon;
        else if (std::strcmp(t, "key") == 0) entry.type = ItemType::KeyItem;
    }
    if (argc > 5 && args[5].type == VAL_STRING) entry.description = args[5].as.string;
    if (argc > 6 && args[6].type == VAL_NUMBER) entry.heal_hp = (int)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) entry.damage = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_STRING) entry.element = args[8].as.string;
    if (argc > 9 && args[9].type == VAL_STRING) entry.sage_func = args[9].as.string;

    // Find or create table for this enemy
    for (auto& table : gs->loot_tables) {
        if (table.enemy_name == enemy) { table.entries.push_back(entry); return val_nil(); }
    }
    gs->loot_tables.push_back({enemy, {entry}});
    return val_nil();
}

// clear_loot(enemy_name) — remove all loot entries for an enemy
static Value native_clear_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string enemy = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& tables = s_active_engine->game_state_->loot_tables;
    tables.erase(std::remove_if(tables.begin(), tables.end(),
        [&](auto& t) { return t.enemy_name == enemy; }), tables.end());
    return val_nil();
}

// drop_item(x, y, id, name, type, desc, heal, dmg, element, sage_func)
static Value native_drop_item(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    WorldDrop drop;
    drop.position.x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    drop.position.y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    drop.item_id = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    drop.item_name = (args[3].type == VAL_STRING) ? args[3].as.string : "";
    if (argc > 4 && args[4].type == VAL_STRING) {
        const char* t = args[4].as.string;
        if (std::strcmp(t, "weapon") == 0) drop.type = ItemType::Weapon;
        else if (std::strcmp(t, "key") == 0) drop.type = ItemType::KeyItem;
    }
    if (argc > 5 && args[5].type == VAL_STRING) drop.description = args[5].as.string;
    if (argc > 6 && args[6].type == VAL_NUMBER) drop.heal_hp = (int)args[6].as.number;
    if (argc > 7 && args[7].type == VAL_NUMBER) drop.damage = (int)args[7].as.number;
    if (argc > 8 && args[8].type == VAL_STRING) drop.element = args[8].as.string;
    if (argc > 9 && args[9].type == VAL_STRING) drop.sage_func = args[9].as.string;
    gs->world_drops.push_back(drop);
    return val_nil();
}

// npc_set_despawn_day(name, enabled) — hostile NPCs disappear at dawn
static Value native_npc_set_despawn_day(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->despawn_at_day = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                                    (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    return val_nil();
}

// npc_set_loot(name, loot_func) — SageLang function called when NPC dies/despawns
static Value native_npc_set_loot(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    NPC* npc = find_npc_by_name((args[0].type == VAL_STRING) ? args[0].as.string : "");
    if (npc) npc->loot_func = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    return val_nil();
}

void ScriptEngine::register_map_api() {
    if (!env_) return;
    env_define(env_, "spawn_npc", 9, val_native(native_spawn_npc_map));
    env_define(env_, "place_object", 12, val_native(native_place_object));
    env_define(env_, "remove_object", 13, val_native(native_remove_object));
    env_define(env_, "set_portal", 10, val_native(native_set_portal));
    env_define(env_, "remove_portal", 13, val_native(native_remove_portal));
    env_define(env_, "set_collision", 13, val_native(native_set_collision));
    env_define(env_, "set_tile", 8, val_native(native_set_tile));
    env_define(env_, "drop_item", 9, val_native(native_drop_item));
    env_define(env_, "add_loot", 8, val_native(native_add_loot));
    env_define(env_, "clear_loot", 10, val_native(native_clear_loot));
    env_define(env_, "npc_set_despawn_day", 19, val_native(native_npc_set_despawn_day));
    env_define(env_, "npc_set_loot", 12, val_native(native_npc_set_loot));
    std::printf("[ScriptEngine] Map API registered\n");
}

// ═══════════════ Audio API ═══════════════

static Value native_play_music(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    bool loop = (argc > 1 && args[1].type == VAL_BOOL) ? args[1].as.boolean : true;
    audio->play_music(path, loop);
    return val_nil();
}

static Value native_stop_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->stop_music();
    return val_nil();
}

static Value native_pause_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->pause_music();
    return val_nil();
}

static Value native_resume_music(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio) audio->resume_music();
    return val_nil();
}

static Value native_set_music_volume(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio && args[0].type == VAL_NUMBER) audio->set_music_volume((float)args[0].as.number);
    return val_nil();
}

static Value native_set_master_volume(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (audio && args[0].type == VAL_NUMBER) audio->set_master_volume((float)args[0].as.number);
    return val_nil();
}

static Value native_play_sfx(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float vol = (argc > 1 && args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    audio->play_sfx(path, vol);
    return val_nil();
}

static Value native_crossfade_music(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* audio = s_active_engine->game_state_->audio_engine;
    if (!audio) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float dur = (argc > 1 && args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    bool loop = (argc > 2 && args[2].type == VAL_BOOL) ? args[2].as.boolean : true;
    audio->crossfade_music(path, dur, loop);
    return val_nil();
}

static Value native_is_music_playing(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_bool(0);
    auto* audio = s_active_engine->game_state_->audio_engine;
    return val_bool(audio && audio->is_music_playing());
}

void ScriptEngine::register_audio_api() {
    if (!env_) return;
    env_define(env_, "play_music", 10, val_native(native_play_music));
    env_define(env_, "stop_music", 10, val_native(native_stop_music));
    env_define(env_, "pause_music", 11, val_native(native_pause_music));
    env_define(env_, "resume_music", 12, val_native(native_resume_music));
    env_define(env_, "set_music_volume", 16, val_native(native_set_music_volume));
    env_define(env_, "set_master_volume", 17, val_native(native_set_master_volume));
    env_define(env_, "play_sfx", 8, val_native(native_play_sfx));
    env_define(env_, "crossfade_music", 15, val_native(native_crossfade_music));
    env_define(env_, "is_music_playing", 16, val_native(native_is_music_playing));
    std::printf("[ScriptEngine] Audio API registered\n");
}

void ScriptEngine::sync_item_to_script(const std::string& item_id) {
    if (!env_ || !game_state_) return;
    auto* item = game_state_->inventory.find(item_id);
    if (item) {
        set_string("item_id", item->id);
        set_string("item_name", item->name);
        set_number("item_heal", item->heal_hp);
        set_number("item_damage", item->damage);
        set_string("item_element", item->element);
    }
}

void ScriptEngine::sync_battle_to_script() {
    if (!env_ || !game_state_) return;
    auto& b = game_state_->battle;

    set_number("enemy_hp", b.enemy_hp_actual);
    set_number("enemy_max_hp", b.enemy_hp_max);
    set_number("enemy_atk", b.enemy_atk);
    set_string("enemy_name", b.enemy_name);
    set_number("player_hp", b.player_hp_actual);
    set_number("player_max_hp", b.player_hp_max);
    set_number("player_atk", b.player_atk);
    set_number("player_def", b.player_def);
    set_number("ally_hp", b.sam_hp_actual);
    set_number("ally_max_hp", b.sam_hp_max);
    set_number("ally_atk", b.sam_atk);
    set_number("active_fighter", b.active_fighter);

    // Sync skill values for current fighter
    auto& skills = (b.active_fighter == 0) ? game_state_->player_stats : game_state_->ally_stats;
    set_number("skill_vitality", skills.vitality);
    set_number("skill_arcana", skills.arcana);
    set_number("skill_agility", skills.agility);
    set_number("skill_tactics", skills.tactics);
    set_number("skill_spirit", skills.spirit);
    set_number("skill_strength", skills.strength);

    // Reset result vars
    set_string("battle_result", "");
    set_number("battle_damage", 0);
    set_string("battle_target", "");
    set_string("battle_msg", "");
}

void ScriptEngine::sync_battle_from_script() {
    if (!env_ || !game_state_) return;
    auto& b = game_state_->battle;

    // Read back modified values
    Value v;
    if (env_get(env_, "enemy_hp", 8, &v) && v.type == VAL_NUMBER)
        b.enemy_hp_actual = (int)v.as.number;
    if (env_get(env_, "player_hp", 9, &v) && v.type == VAL_NUMBER)
        b.player_hp_actual = (int)v.as.number;
    if (env_get(env_, "ally_hp", 7, &v) && v.type == VAL_NUMBER)
        b.sam_hp_actual = (int)v.as.number;
    if (env_get(env_, "battle_damage", 13, &v) && v.type == VAL_NUMBER)
        b.last_damage = (int)v.as.number;
    if (env_get(env_, "battle_msg", 10, &v) && v.type == VAL_STRING)
        b.message = v.as.string;
}

bool ScriptEngine::load_file(const std::string& path) {
    auto data = FileIO::read_file(path);
    if (data.empty()) {
        std::fprintf(stderr, "[ScriptEngine] Failed to read: %s\n", path.c_str());
        return false;
    }
    // Track loaded files for hot reload
    bool already_tracked = false;
    for (auto& f : loaded_files_) if (f == path) { already_tracked = true; break; }
    if (!already_tracked) loaded_files_.push_back(path);

    std::string source(data.begin(), data.end());
    return execute(source);
}

bool ScriptEngine::reload_all() {
    // Clear script-driven UI before reload (scripts will recreate what they need)
    if (game_state_) {
        game_state_->script_ui.labels.clear();
        game_state_->script_ui.bars.clear();
        game_state_->script_ui.panels.clear();
        game_state_->script_ui.images.clear();
        game_state_->npc_meet_triggers.clear();
    }

    int ok = 0, fail = 0;
    for (auto& path : loaded_files_) {
        auto data = FileIO::read_file(path);
        if (data.empty()) { fail++; continue; }
        std::string source(data.begin(), data.end());
        if (execute(source)) ok++; else fail++;
    }

    // Re-run map_init if it exists
    if (has_function("map_init")) {
        call_function("map_init");
    }

    std::printf("[ScriptEngine] Hot reload: %d OK, %d failed (of %d files)\n",
                ok, fail, (int)loaded_files_.size());
    return fail == 0;
}

bool ScriptEngine::execute(const std::string& code) {
    if (!env_) return false;

    // SageLang's AST keeps pointers into the source string,
    // so we must keep it alive for the lifetime of the interpreter.
    char* source = strdup(code.c_str());
    if (!source) return false;
    source_buffers_.push_back(source);

    init_lexer(source, "<script>");
    parser_init();
    bool success = true;
    while (true) {
        Stmt* stmt = parse();
        if (stmt == nullptr) break;
        retain_program_stmt(stmt);
        ExecResult result = interpret(stmt, env_);
        if (result.is_throwing) {
            if (result.exception_value.type == VAL_STRING)
                std::fprintf(stderr, "[ScriptEngine] Exception: %s\n", result.exception_value.as.string);
            success = false; break;
        }
    }
    return success;
}

bool ScriptEngine::call_function(const std::string& name) {
    std::string call = name + "()";
    return execute(call);
}

void ScriptEngine::set_number(const std::string& name, double value) {
    if (env_) env_define(env_, name.c_str(), (int)name.size(), val_number(value));
}

void ScriptEngine::set_string(const std::string& name, const std::string& value) {
    if (env_) env_define(env_, name.c_str(), (int)name.size(), val_string(value.c_str()));
}

void ScriptEngine::set_bool(const std::string& name, bool value) {
    if (env_) env_define(env_, name.c_str(), (int)name.size(), val_bool(value ? 1 : 0));
}

bool ScriptEngine::has_function(const std::string& name) const {
    if (!env_) return false;
    Value v;
    return env_get(const_cast<Env*>(env_), name.c_str(), (int)name.size(), &v) &&
           (v.type == VAL_FUNCTION || v.type == VAL_NATIVE);
}

} // namespace eb
