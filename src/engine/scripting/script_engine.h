#pragma once

#include <string>
#include <vector>
#include <functional>

// Forward declarations for SageLang C API
extern "C" {
    typedef struct Env Env;
    typedef struct Value Value;
}

struct BattleState;
struct GameState;

namespace eb {

class DialogueBox;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    bool load_file(const std::string& path);
    bool execute(const std::string& code);
    bool call_function(const std::string& name);

    void set_number(const std::string& name, double value);
    void set_string(const std::string& name, const std::string& value);
    void set_bool(const std::string& name, bool value);

    bool has_function(const std::string& name) const;
    bool is_initialized() const { return env_ != nullptr; }

    // Set game state pointer for battle API access
    void set_game_state(GameState* game) { game_state_ = game; }

    // Sync battle state variables to/from SageLang environment
    void sync_battle_to_script();
    void sync_battle_from_script();

    // Sync inventory item info to script before item use
    void sync_item_to_script(const std::string& item_id);

private:
    void register_engine_api();
    void register_battle_api();
    void register_inventory_api();

    Env* env_ = nullptr;
    GameState* game_state_ = nullptr;

    // Keep source strings alive — SageLang AST references them
    std::vector<char*> source_buffers_;
};

} // namespace eb
