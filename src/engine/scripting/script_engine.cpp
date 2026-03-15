#include "engine/scripting/script_engine.h"
#include "engine/resource/file_io.h"
#include "engine/core/debug_log.h"
#include "game/game.h"

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

static std::vector<std::pair<std::string, bool>> s_flags;

static Value native_set_flag(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    const char* name = args[0].as.string;
    bool val = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
               (args[1].type == VAL_NUMBER) ? (args[1].as.number != 0) : false;
    for (auto& f : s_flags) { if (f.first == name) { f.second = val; return val_nil(); } }
    s_flags.push_back({name, val});
    return val_nil();
}

static Value native_get_flag(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_bool(0);
    for (auto& f : s_flags) { if (f.first == args[0].as.string) return val_bool(f.second ? 1 : 0); }
    return val_bool(0);
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

static HunterSkills* resolve_skills(const char* character) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    auto* gs = s_active_engine->game_state_;
    if (std::strcmp(character, "dean") == 0 || std::strcmp(character, "Dean") == 0)
        return &gs->dean_skills;
    if (std::strcmp(character, "sam") == 0 || std::strcmp(character, "Sam") == 0)
        return &gs->sam_skills;
    return nullptr;
}

static int* resolve_skill_field(HunterSkills* skills, const char* name) {
    if (!skills) return nullptr;
    if (std::strcmp(name, "hardiness") == 0)  return &skills->hardiness;
    if (std::strcmp(name, "unholiness") == 0) return &skills->unholiness;
    if (std::strcmp(name, "nerve") == 0)      return &skills->nerve;
    if (std::strcmp(name, "tactics") == 0)    return &skills->tactics;
    if (std::strcmp(name, "exorcism") == 0)   return &skills->exorcism;
    if (std::strcmp(name, "riflery") == 0)    return &skills->riflery;
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
        *field = std::max(HunterSkills::MIN_STAT, std::min(HunterSkills::MAX_STAT, val));
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
    if (std::strcmp(bonus, "holy_mult") == 0)  return val_number(skills->holy_damage_mult() * 100);
    if (std::strcmp(bonus, "weapon_dmg") == 0) return val_number(skills->weapon_damage_bonus());
    if (std::strcmp(bonus, "dodge") == 0)      return val_number(skills->dodge_chance() * 100);
    if (std::strcmp(bonus, "dark_mult") == 0)  return val_number(skills->dark_power_mult() * 100);
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

// ═══════════════ ScriptEngine Implementation ═══════════════

ScriptEngine::ScriptEngine() {
    gc_init();
    init_module_system();
    env_ = env_create(nullptr);
    if (env_) {
        init_stdlib(env_);
        register_engine_api();
        register_battle_api();
        register_inventory_api();
        register_skills_api();
        register_debug_api();
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
    // dean_hp, dean_max_hp, dean_atk, dean_def
    // sam_hp, sam_max_hp, sam_atk
    // active_fighter (0=Dean, 1=Sam)
    // battle_result: set to "damage", "heal", "miss" etc by scripts
    // battle_damage: the amount of damage/healing
    // battle_target: "enemy", "dean", "sam"
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
    set_number("dean_hp", b.player_hp_actual);
    set_number("dean_max_hp", b.player_hp_max);
    set_number("dean_atk", b.player_atk);
    set_number("dean_def", b.player_def);
    set_number("sam_hp", b.sam_hp_actual);
    set_number("sam_max_hp", b.sam_hp_max);
    set_number("sam_atk", b.sam_atk);
    set_number("active_fighter", b.active_fighter);

    // Sync skill values for current fighter
    auto& skills = (b.active_fighter == 0) ? game_state_->dean_skills : game_state_->sam_skills;
    set_number("skill_hardiness", skills.hardiness);
    set_number("skill_unholiness", skills.unholiness);
    set_number("skill_nerve", skills.nerve);
    set_number("skill_tactics", skills.tactics);
    set_number("skill_exorcism", skills.exorcism);
    set_number("skill_riflery", skills.riflery);

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
    if (env_get(env_, "dean_hp", 7, &v) && v.type == VAL_NUMBER)
        b.player_hp_actual = (int)v.as.number;
    if (env_get(env_, "sam_hp", 6, &v) && v.type == VAL_NUMBER)
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
    int ok = 0, fail = 0;
    for (auto& path : loaded_files_) {
        auto data = FileIO::read_file(path);
        if (data.empty()) { fail++; continue; }
        std::string source(data.begin(), data.end());
        if (execute(source)) ok++; else fail++;
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
