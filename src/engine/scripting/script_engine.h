#pragma once

#include <string>
#include <vector>
#include <functional>

// Forward declarations for SageLang C API
extern "C" {
    typedef struct Env Env;
    typedef struct Value Value;
}

namespace eb {

class DialogueBox;

// Callback types for engine integration
using DialogueCallback = std::function<void(const std::string& speaker, const std::string& text)>;
using DialogueStartCallback = std::function<void(const std::vector<std::pair<std::string,std::string>>& lines)>;
using BattleCallback = std::function<void(const std::string& enemy, int hp, int atk)>;
using SpawnCallback = std::function<void(const std::string& name, float x, float y)>;
using TeleportCallback = std::function<void(float x, float y)>;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // Load and execute a .sage script file
    bool load_file(const std::string& path);

    // Execute a string of SageLang code
    bool execute(const std::string& code);

    // Call a function defined in the script
    bool call_function(const std::string& name);

    // Define a global variable
    void set_number(const std::string& name, double value);
    void set_string(const std::string& name, const std::string& value);
    void set_bool(const std::string& name, bool value);

    // Register engine callbacks
    void set_dialogue_callback(DialogueStartCallback cb) { on_dialogue_ = cb; }
    void set_battle_callback(BattleCallback cb) { on_battle_ = cb; }
    void set_spawn_callback(SpawnCallback cb) { on_spawn_ = cb; }
    void set_teleport_callback(TeleportCallback cb) { on_teleport_ = cb; }

    // Check if a function exists in the loaded script
    bool has_function(const std::string& name) const;

    bool is_initialized() const { return env_ != nullptr; }

private:
    void register_engine_api();

    Env* env_ = nullptr;

    // Callbacks
    DialogueStartCallback on_dialogue_;
    BattleCallback on_battle_;
    SpawnCallback on_spawn_;
    TeleportCallback on_teleport_;
};

} // namespace eb
