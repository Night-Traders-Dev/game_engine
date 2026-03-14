#include "engine/scripting/script_engine.h"
#include "engine/resource/file_io.h"

extern "C" {
#include "interpreter.h"
#include "parser.h"
#include "lexer.h"
#include "env.h"
#include "value.h"
#include "ast.h"
#include "gc.h"
#include "module.h"

// SageLang internal functions we need
void init_lexer(const char* source, const char* filename);
void parser_init(void);
Stmt* parse(void);
void retain_program_stmt(Stmt* stmt);
}

#include <cstdio>
#include <cstring>
#include <vector>

namespace eb {

// ── Static callback pointers (SageLang uses C callbacks, no closures) ──
static ScriptEngine* s_active_engine = nullptr;

// ── Native function: dialogue(speaker, text, ...) ──
static Value native_dialogue(int argc, Value* args) {
    if (argc < 2) return val_nil();
    // Just log — actual dialogue triggering goes through say()
    for (int i = 0; i + 1 < argc; i += 2) {
        const char* speaker = (args[i].type == VAL_STRING) ? args[i].as.string : "???";
        const char* text = (args[i+1].type == VAL_STRING) ? args[i+1].as.string : "";
        std::printf("[Script] %s: %s\n", speaker, text);
    }
    return val_nil();
}

// ── Native function: say(speaker, text) — single dialogue line ──
static Value native_say(int argc, Value* args) {
    if (argc < 2) return val_nil();
    const char* speaker = (args[0].type == VAL_STRING) ? args[0].as.string : "???";
    const char* text = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::printf("[Script] %s: %s\n", speaker, text);
    return val_nil();
}

// ── Native function: start_battle(enemy_name, hp, atk) ──
static Value native_start_battle(int argc, Value* args) {
    if (!s_active_engine || argc < 3) return val_nil();
    const char* enemy = (args[0].type == VAL_STRING) ? args[0].as.string : "Enemy";
    int hp = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 50;
    int atk = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 10;
    std::printf("[Script] Battle: %s (HP:%d ATK:%d)\n", enemy, hp, atk);
    return val_nil();
}

// ── Native function: spawn_npc(name, x, y) ──
static Value native_spawn_npc(int argc, Value* args) {
    if (argc < 3) return val_nil();
    const char* name = (args[0].type == VAL_STRING) ? args[0].as.string : "npc";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    std::printf("[Script] Spawn NPC: %s at (%.0f, %.0f)\n", name, x, y);
    return val_nil();
}

// ── Native function: teleport(x, y) ──
static Value native_teleport(int argc, Value* args) {
    if (argc < 2) return val_nil();
    float x = (args[0].type == VAL_NUMBER) ? (float)args[0].as.number : 0;
    float y = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    std::printf("[Script] Teleport to (%.0f, %.0f)\n", x, y);
    return val_nil();
}

// ── Native function: log(message) — debug output ──
static Value native_log(int argc, Value* args) {
    if (argc < 1) return val_nil();
    if (args[0].type == VAL_STRING)
        std::printf("[Script] %s\n", args[0].as.string);
    else if (args[0].type == VAL_NUMBER)
        std::printf("[Script] %.2f\n", args[0].as.number);
    else if (args[0].type == VAL_BOOL)
        std::printf("[Script] %s\n", args[0].as.boolean ? "true" : "false");
    return val_nil();
}

// ── Native function: set_flag(name, value) / get_flag(name) ──
static std::vector<std::pair<std::string, bool>> s_flags;

static Value native_set_flag(int argc, Value* args) {
    if (argc < 2 || args[0].type != VAL_STRING) return val_nil();
    const char* name = args[0].as.string;
    bool val = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
               (args[1].type == VAL_NUMBER) ? (args[1].as.number != 0) : false;
    for (auto& f : s_flags) {
        if (f.first == name) { f.second = val; return val_nil(); }
    }
    s_flags.push_back({name, val});
    return val_nil();
}

static Value native_get_flag(int argc, Value* args) {
    if (argc < 1 || args[0].type != VAL_STRING) return val_bool(0);
    const char* name = args[0].as.string;
    for (auto& f : s_flags) {
        if (f.first == name) return val_bool(f.second ? 1 : 0);
    }
    return val_bool(0);
}

// ═══════════════════════════════════════════════════

ScriptEngine::ScriptEngine() {
    gc_init();
    init_module_system();
    env_ = env_create(nullptr);
    if (env_) {
        init_stdlib(env_);
        register_engine_api();
        s_active_engine = this;
    }
}

ScriptEngine::~ScriptEngine() {
    if (s_active_engine == this) s_active_engine = nullptr;
    if (env_) {
        env_cleanup_all();
        env_ = nullptr;
    }
}

void ScriptEngine::register_engine_api() {
    if (!env_) return;

    // Register all native engine functions
    env_define(env_, "say", 3, val_native(native_say));
    env_define(env_, "dialogue", 8, val_native(native_dialogue));
    env_define(env_, "start_battle", 12, val_native(native_start_battle));
    env_define(env_, "spawn_npc", 9, val_native(native_spawn_npc));
    env_define(env_, "teleport", 8, val_native(native_teleport));
    env_define(env_, "log", 3, val_native(native_log));
    env_define(env_, "set_flag", 8, val_native(native_set_flag));
    env_define(env_, "get_flag", 8, val_native(native_get_flag));

    std::printf("[ScriptEngine] Engine API registered (8 functions)\n");
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
                std::fprintf(stderr, "[ScriptEngine] Exception: %s\n",
                              result.exception_value.as.string);
            success = false;
            break;
        }
    }

    return success;
}

bool ScriptEngine::call_function(const std::string& name) {
    // Call by executing: function_name()
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
