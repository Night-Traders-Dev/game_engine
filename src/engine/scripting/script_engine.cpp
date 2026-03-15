#include "engine/scripting/script_engine.h"
#include "engine/resource/file_io.h"
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
    std::printf("[Script] %s: %s\n", speaker, text);
    return val_nil();
}

static Value native_log(int argc, Value* args) {
    if (argc < 1) return val_nil();
    if (args[0].type == VAL_STRING) std::printf("[Script] %s\n", args[0].as.string);
    else if (args[0].type == VAL_NUMBER) std::printf("[Script] %.2f\n", args[0].as.number);
    else if (args[0].type == VAL_BOOL) std::printf("[Script] %s\n", args[0].as.boolean ? "true" : "false");
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

// ═══════════════ ScriptEngine Implementation ═══════════════

ScriptEngine::ScriptEngine() {
    gc_init();
    init_module_system();
    env_ = env_create(nullptr);
    if (env_) {
        init_stdlib(env_);
        register_engine_api();
        register_battle_api();
        s_active_engine = this;
    }
}

ScriptEngine::~ScriptEngine() {
    if (s_active_engine == this) s_active_engine = nullptr;
    if (env_) { env_cleanup_all(); env_ = nullptr; }
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
    std::string source(data.begin(), data.end());
    return execute(source);
}

bool ScriptEngine::execute(const std::string& code) {
    if (!env_) return false;
    init_lexer(code.c_str(), "<script>");
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
