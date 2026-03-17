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
struct NPC;

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

    // Hot reload: track loaded files and reload them
    const std::vector<std::string>& loaded_files() const { return loaded_files_; }
    bool reload_all();

    // Set game state pointer for battle API access
    void set_game_state(GameState* game) { game_state_ = game; }

    // Sync battle state variables to/from SageLang environment
    void sync_battle_to_script();
    void sync_battle_from_script();

    // Sync inventory item info to script before item use
    void sync_item_to_script(const std::string& item_id);

    // Public for static native function access
    GameState* game_state_ = nullptr;

private:
    void register_engine_api();
    void register_battle_api();
    void register_inventory_api();
    void register_skills_api();
    void register_debug_api();
    void register_shop_api();
    void register_daynight_api();
    void register_ui_api();
    void register_survival_api();
    void register_pathfinding_api();
    void register_route_api();
    void register_schedule_api();
    void register_npc_interact_api();
    void register_spawn_api();
    void register_map_api();
    void register_audio_api();
    void register_player_api();
    void register_camera_api();
    void register_platform_api();
    void register_npc_runtime_api();
    void register_effects_api();
    void register_tilemap_api();
    void register_input_api();
    void register_dialogue_ext_api();
    void register_battle_ext_api();
    void register_renderer_api();
    void register_weather_api();
    void register_level_api();
    void register_tween_api();
    void register_particle_api();
    void register_save_api();
    void register_transition_api();
    void register_quest_api();
    void register_equipment_api();
    void register_dialogue_history_api();
    void register_event_api();
    void register_locale_api();
    void register_achievement_api();
    void register_lighting_api();
    void register_anim_api();
    void register_visual_fx_api();
    void register_parallax_api();
    void register_platformer_api();
    void register_collision_api();
    void register_raycast_api();
    void register_trigger_api();
    void register_fsm_api();
    void register_checkpoint_api();
    void register_combo_api();
    void register_trail_api();
    void register_dungeon_api();
    void register_skeleton_api();
    void register_replay_api();
    void register_postprocess_api();
    void register_audio_bus_api();
    void register_coroutine_api();

    Env* env_ = nullptr;

    // Keep source strings alive — SageLang AST references them
    std::vector<char*> source_buffers_;
    std::vector<std::string> loaded_files_;
};

// Shared helpers used by script API translation units
bool is_safe_path(const char* path);
NPC* find_npc_by_name(const std::string& name);

} // namespace eb
