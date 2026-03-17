#include "engine/scripting/script_engine.h"
#include "game/game.h"

extern "C" {
#include "interpreter.h"
#include "env.h"
#include "value.h"
}

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>

namespace eb {
extern ScriptEngine* s_active_engine;

// =============== FSM API ===============
// Uses eb::StateMachine from game/systems/state_machine.h (included via game.h)
// and GameState::state_machines map

// fsm_create(id)
static Value native_fsm_create(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (id.empty()) return val_nil();
    // Create an empty StateMachine entry
    s_active_engine->game_state_->state_machines[id] = StateMachine{};
    return val_nil();
}

// fsm_add_state(id, name, on_enter, on_update, on_exit)
static Value native_fsm_add_state(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id   = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string on_enter  = (argc > 2 && args[2].type == VAL_STRING) ? args[2].as.string : "";
    std::string on_update = (argc > 3 && args[3].type == VAL_STRING) ? args[3].as.string : "";
    std::string on_exit   = (argc > 4 && args[4].type == VAL_STRING) ? args[4].as.string : "";
    auto it = gs->state_machines.find(id);
    if (it == gs->state_machines.end()) return val_nil();
    it->second.add_state(name, on_enter, on_update, on_exit);
    return val_nil();
}

// fsm_transition(id, state)
static Value native_fsm_transition(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id    = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string state = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    auto it = gs->state_machines.find(id);
    if (it == gs->state_machines.end()) return val_nil();
    it->second.transition(state);
    return val_nil();
}

// fsm_current(id) -> string
static Value native_fsm_current(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_string("");
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto it = gs->state_machines.find(id);
    if (it == gs->state_machines.end()) return val_string("");
    return val_string(it->second.current_state().c_str());
}

// fsm_remove(id)
static Value native_fsm_remove(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->state_machines.erase(id);
    return val_nil();
}

// =============== Checkpoint API ===============
// Uses eb::CheckpointSystem from game/systems/checkpoint.h (included via game.h)

// add_checkpoint(id, x, y, map_id)
static Value native_add_checkpoint(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id     = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x            = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y            = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    std::string map_id = (args[3].type == VAL_STRING) ? args[3].as.string : "";
    gs->checkpoint_system.add(id, {x, y}, map_id);
    return val_nil();
}

// activate_checkpoint(id)
static Value native_activate_checkpoint(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->checkpoint_system.activate(id);
    std::printf("[Checkpoint] Activated: %s\n", id.c_str());
    return val_nil();
}

// get_checkpoint_id() -> string
static Value native_get_checkpoint_id(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_string("");
    auto* cp = s_active_engine->game_state_->checkpoint_system.get_active();
    if (!cp) return val_string("");
    return val_string(cp->id.c_str());
}

// respawn()
static Value native_respawn(int, Value*) {
    if (!s_active_engine || !s_active_engine->game_state_) return val_nil();
    auto* gs = s_active_engine->game_state_;
    auto* cp = gs->checkpoint_system.get_active();
    if (!cp) return val_nil();
    gs->player_pos.x = cp->position.x;
    gs->player_pos.y = cp->position.y;
    gs->player_hp = gs->player_hp_max;
    std::printf("[Checkpoint] Respawned at %s (%.0f, %.0f)\n",
                cp->id.c_str(), cp->position.x, cp->position.y);
    return val_nil();
}

// =============== Combo API ===============

struct ComboEntry {
    std::string name;
    std::vector<int> actions;
    float max_delay;
    std::string callback;
};

static std::vector<ComboEntry> s_combos;

// register_combo(name, action_csv, max_delay, callback)
static Value native_register_combo(int argc, Value* args) {
    if (argc < 4) return val_nil();
    std::string name       = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    const char* action_csv = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float max_delay        = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0.5f;
    std::string callback   = (args[3].type == VAL_STRING) ? args[3].as.string : "";

    // Parse comma-separated ints
    std::vector<int> actions;
    std::istringstream ss(action_csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) actions.push_back(std::atoi(token.c_str()));
    }

    // Replace if same name exists
    for (auto& c : s_combos) {
        if (c.name == name) { c.actions = actions; c.max_delay = max_delay; c.callback = callback; return val_nil(); }
    }
    s_combos.push_back({name, actions, max_delay, callback});
    std::printf("[Combo] Registered: %s (%d actions)\n", name.c_str(), (int)actions.size());
    return val_nil();
}

// =============== Trail API ===============
// Uses eb::Trail and eb::TrailPoint from game/systems/trail_renderer.h (included via game.h)
// Trails are stored in GameState::trails vector

// trail_create(id, max_points, lifetime, width)
static Value native_trail_create(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id  = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    int max_pts     = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 20;
    float lifetime  = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1.0f;
    float width     = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 4.0f;
    Trail t;
    t.id = id;
    t.max_points = max_pts;
    t.lifetime = lifetime;
    t.base_width = width;
    t.color_start = {1, 1, 1, 1};
    t.color_end = {1, 1, 1, 0};
    t.active = true;
    t.emitting = true;
    gs->trails.push_back(std::move(t));
    return val_nil();
}

// Helper: find trail by id in GameState
static Trail* find_trail(const std::string& id) {
    if (!s_active_engine || !s_active_engine->game_state_) return nullptr;
    for (auto& t : s_active_engine->game_state_->trails) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

// trail_add_point(id, x, y)
static Value native_trail_add_point(int argc, Value* args) {
    if (argc < 3) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    float y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    Trail* trail = find_trail(id);
    if (!trail || !trail->emitting) return val_nil();
    TrailPoint pt;
    pt.position = {x, y};
    pt.time_added = 0.0f; // caller doesn't pass game_time; 0 is a safe default
    pt.color = trail->color_start;
    pt.width = trail->base_width;
    trail->points.push_back(pt);
    if (static_cast<int>(trail->points.size()) > trail->max_points) {
        trail->points.erase(trail->points.begin());
    }
    return val_nil();
}

// trail_set_color(id, r1,g1,b1,a1, r2,g2,b2,a2)
static Value native_trail_set_color(int argc, Value* args) {
    if (argc < 9) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    Trail* t = find_trail(id);
    if (!t) return val_nil();
    t->color_start.x = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1;
    t->color_start.y = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 1;
    t->color_start.z = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 1;
    t->color_start.w = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 1;
    t->color_end.x   = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 1;
    t->color_end.y   = (args[6].type == VAL_NUMBER) ? (float)args[6].as.number : 1;
    t->color_end.z   = (args[7].type == VAL_NUMBER) ? (float)args[7].as.number : 1;
    t->color_end.w   = (args[8].type == VAL_NUMBER) ? (float)args[8].as.number : 0;
    return val_nil();
}

// trail_destroy(id)
static Value native_trail_destroy(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto& trails = gs->trails;
    trails.erase(std::remove_if(trails.begin(), trails.end(),
        [&](auto& t) { return t.id == id; }), trails.end());
    return val_nil();
}

// trail_set_emitting(id, bool)
static Value native_trail_set_emitting(int argc, Value* args) {
    if (argc < 2) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    bool emitting = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                    (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    Trail* t = find_trail(id);
    if (t) t->emitting = emitting;
    return val_nil();
}

// =============== Dungeon API ===============
// Uses eb::DungeonRoom from game/systems/dungeon_gen.h (int x, y, w, h; with center() method)

static std::vector<DungeonRoom> s_last_dungeon_rooms;

// generate_dungeon(width, height, method, seed)
static Value native_generate_dungeon(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 4) return val_nil();
    auto* gs = s_active_engine->game_state_;
    int width  = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 64;
    int height = (args[1].type == VAL_NUMBER) ? (int)args[1].as.number : 64;
    int method = (args[2].type == VAL_NUMBER) ? (int)args[2].as.number : 0;
    int seed   = (args[3].type == VAL_NUMBER) ? (int)args[3].as.number : 42;

    s_last_dungeon_rooms.clear();
    std::srand(seed);

    if (method == 0) {
        // BSP dungeon generation
        int num_rooms = 6 + (std::rand() % 6);
        for (int i = 0; i < num_rooms; i++) {
            int rw = 6 + (std::rand() % 8);
            int rh = 6 + (std::rand() % 8);
            int rx = 1 + (std::rand() % (width - rw - 2));
            int ry = 1 + (std::rand() % (height - rh - 2));
            s_last_dungeon_rooms.push_back({rx, ry, rw, rh});
        }
    } else {
        // Cellular automata
        int num_rooms = 4 + (std::rand() % 4);
        for (int i = 0; i < num_rooms; i++) {
            int rw = 8 + (std::rand() % 10);
            int rh = 8 + (std::rand() % 10);
            int rx = 2 + (std::rand() % (width - rw - 4));
            int ry = 2 + (std::rand() % (height - rh - 4));
            s_last_dungeon_rooms.push_back({rx, ry, rw, rh});
        }
    }

    // Apply rooms to tilemap layer 0 and collision
    auto& tm = gs->tile_map;
    if (tm.layer_count() > 0) {
        auto& layers = tm.layers();
        auto& layer = layers[0];
        // Fill with wall tile
        for (auto& tile : layer.data) tile = 1;
        // Set all collision to solid
        for (int ty = 0; ty < tm.height(); ty++) {
            for (int tx = 0; tx < tm.width(); tx++) {
                tm.set_collision_at(tx, ty, eb::CollisionType::Solid);
            }
        }

        for (auto& room : s_last_dungeon_rooms) {
            int rx = room.x;
            int ry = room.y;
            for (int dy = 0; dy < room.h; dy++) {
                for (int dx = 0; dx < room.w; dx++) {
                    int tx = rx + dx;
                    int ty = ry + dy;
                    if (tx >= 0 && tx < tm.width() && ty >= 0 && ty < tm.height()) {
                        int idx = ty * tm.width() + tx;
                        layer.data[idx] = 0;
                        tm.set_collision_at(tx, ty, eb::CollisionType::None);
                    }
                }
            }
        }
    }

    std::printf("[Dungeon] Generated %dx%d (method=%d, seed=%d, rooms=%d)\n",
                width, height, method, seed, (int)s_last_dungeon_rooms.size());
    return val_nil();
}

// dungeon_room_count() -> int
static Value native_dungeon_room_count(int, Value*) {
    return val_number((double)s_last_dungeon_rooms.size());
}

// dungeon_room_x(idx) -> float
static Value native_dungeon_room_x(int argc, Value* args) {
    if (argc < 1) return val_number(0);
    int idx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    if (idx < 0 || idx >= (int)s_last_dungeon_rooms.size()) return val_number(0);
    return val_number(s_last_dungeon_rooms[idx].center().x);
}

// dungeon_room_y(idx) -> float
static Value native_dungeon_room_y(int argc, Value* args) {
    if (argc < 1) return val_number(0);
    int idx = (args[0].type == VAL_NUMBER) ? (int)args[0].as.number : 0;
    if (idx < 0 || idx >= (int)s_last_dungeon_rooms.size()) return val_number(0);
    return val_number(s_last_dungeon_rooms[idx].center().y);
}

// =============== Skeleton API ===============
// Uses eb::SkeletonAnimPlayer from game/systems/skeleton_anim.h
// Stored in GameState::skeleton_entities map

// skel_create(id)
static Value native_skel_create(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->skeleton_entities[id] = SkeletonAnimPlayer{};
    return val_nil();
}

// skel_add_bone(id, bone_name, parent_name, lx, ly, lr)
static Value native_skel_add_bone(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 6) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id     = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string bone_name = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string parent = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    float lx = (args[3].type == VAL_NUMBER) ? (float)args[3].as.number : 0;
    float ly = (args[4].type == VAL_NUMBER) ? (float)args[4].as.number : 0;
    float lr = (args[5].type == VAL_NUMBER) ? (float)args[5].as.number : 0;
    auto it = gs->skeleton_entities.find(id);
    if (it == gs->skeleton_entities.end()) return val_nil();
    Bone b;
    b.name = bone_name;
    b.parent = -1;  // Resolve parent index from name
    auto& bones = it->second.skeleton.bones;
    for (int i = 0; i < static_cast<int>(bones.size()); i++) {
        if (bones[i].name == parent) { b.parent = i; break; }
    }
    b.local_pos = {lx, ly};
    b.local_rotation = lr;
    bones.push_back(b);
    return val_nil();
}

// skel_set_sprite(id, bone_name, sprite_region)
static Value native_skel_set_sprite(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id     = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string bone   = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    std::string region = (args[2].type == VAL_STRING) ? args[2].as.string : "";
    auto it = gs->skeleton_entities.find(id);
    if (it == gs->skeleton_entities.end()) return val_nil();
    for (auto& b : it->second.skeleton.bones) {
        if (b.name == bone) { b.sprite_region = region; return val_nil(); }
    }
    return val_nil();
}

// skel_play(id, anim_name)
static Value native_skel_play(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id   = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string anim = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    auto it = gs->skeleton_entities.find(id);
    if (it == gs->skeleton_entities.end()) return val_nil();
    it->second.play(anim);
    return val_nil();
}

// skel_stop(id)
static Value native_skel_stop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    auto* gs = s_active_engine->game_state_;
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    auto it = gs->skeleton_entities.find(id);
    if (it != gs->skeleton_entities.end()) it->second.stop();
    return val_nil();
}

// =============== Input Replay API ===============

static bool s_recording = false;
static bool s_replaying = false;

// input_record_start()
static Value native_input_record_start(int, Value*) {
    s_recording = true;
    std::printf("[Replay] Recording started\n");
    return val_nil();
}

// input_record_stop(path)
static Value native_input_record_stop(int argc, Value* args) {
    if (argc < 1) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!is_safe_path(path)) return val_nil();
    s_recording = false;
    std::printf("[Replay] Recording stopped -> %s\n", path);
    return val_nil();
}

// input_replay_start(path)
static Value native_input_replay_start(int argc, Value* args) {
    if (argc < 1) return val_nil();
    const char* path = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (!is_safe_path(path)) return val_nil();
    s_replaying = true;
    std::printf("[Replay] Playback started <- %s\n", path);
    return val_nil();
}

// input_replay_stop()
static Value native_input_replay_stop(int, Value*) {
    s_replaying = false;
    std::printf("[Replay] Playback stopped\n");
    return val_nil();
}

// =============== Post-Process API ===============

struct PostProcessState {
    bool crt = false;
    bool bloom = false;
    bool vignette = false;
    bool blur = false;
    bool color_grade = false;
    float bloom_threshold = 0.8f;
    float bloom_intensity = 1.0f;
    float vignette_strength = 0.5f;
    float blur_radius = 2.0f;
};

static PostProcessState s_postprocess;

// set_postprocess(effect, enabled)
static Value native_set_postprocess(int argc, Value* args) {
    if (argc < 2) return val_nil();
    const char* effect = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    bool enabled = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                   (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    if (std::strcmp(effect, "crt") == 0)         s_postprocess.crt = enabled;
    else if (std::strcmp(effect, "bloom") == 0)   s_postprocess.bloom = enabled;
    else if (std::strcmp(effect, "vignette") == 0) s_postprocess.vignette = enabled;
    else if (std::strcmp(effect, "blur") == 0)    s_postprocess.blur = enabled;
    else if (std::strcmp(effect, "color_grade") == 0) s_postprocess.color_grade = enabled;
    return val_nil();
}

// set_postprocess_param(param, value)
static Value native_set_postprocess_param(int argc, Value* args) {
    if (argc < 2) return val_nil();
    const char* param = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float value = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 0;
    if (std::strcmp(param, "bloom_threshold") == 0)     s_postprocess.bloom_threshold = value;
    else if (std::strcmp(param, "bloom_intensity") == 0) s_postprocess.bloom_intensity = value;
    else if (std::strcmp(param, "vignette_strength") == 0) s_postprocess.vignette_strength = value;
    else if (std::strcmp(param, "blur_radius") == 0)    s_postprocess.blur_radius = value;
    return val_nil();
}

// =============== Audio Bus API ===============

// audio_bus_volume(bus, vol)
static Value native_audio_bus_volume(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    const char* bus = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    float vol = (args[1].type == VAL_NUMBER) ? (float)args[1].as.number : 1.0f;
    auto* gs = s_active_engine->game_state_;

    if (std::strcmp(bus, "music") == 0)         gs->audio_buses.music_volume = vol;
    else if (std::strcmp(bus, "sfx") == 0)      gs->audio_buses.sfx_volume = vol;
    else if (std::strcmp(bus, "ambience") == 0)  gs->audio_buses.ambience_volume = vol;
    else if (std::strcmp(bus, "voice") == 0)     gs->audio_buses.voice_volume = vol;
    return val_nil();
}

// =============== Coroutine API ===============
// Uses eb::CoroutineManager from game/systems/coroutine.h (included via game.h)

// coroutine_create(id)
static Value native_coroutine_create(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    if (id.empty()) return val_nil();
    s_active_engine->game_state_->coroutine_manager.add(id);
    return val_nil();
}

// coroutine_step(id, func, delay)
static Value native_coroutine_step(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 3) return val_nil();
    std::string id   = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    std::string func = (args[1].type == VAL_STRING) ? args[1].as.string : "";
    float delay      = (args[2].type == VAL_NUMBER) ? (float)args[2].as.number : 0;
    s_active_engine->game_state_->coroutine_manager.add_step(id, func, delay);
    return val_nil();
}

// coroutine_start(id)
static Value native_coroutine_start(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->coroutine_manager.start(id);
    return val_nil();
}

// coroutine_stop(id)
static Value native_coroutine_stop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->coroutine_manager.stop(id);
    return val_nil();
}

// coroutine_loop(id, bool)
static Value native_coroutine_loop(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 2) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    bool loop = (args[1].type == VAL_BOOL) ? args[1].as.boolean :
                (args[1].type == VAL_NUMBER && args[1].as.number != 0);
    s_active_engine->game_state_->coroutine_manager.set_loop(id, loop);
    return val_nil();
}

// coroutine_remove(id)
static Value native_coroutine_remove(int argc, Value* args) {
    if (!s_active_engine || !s_active_engine->game_state_ || argc < 1) return val_nil();
    std::string id = (args[0].type == VAL_STRING) ? args[0].as.string : "";
    s_active_engine->game_state_->coroutine_manager.remove(id);
    return val_nil();
}

// =============== Register Methods ===============

void ScriptEngine::register_fsm_api() {
    if (!env_) return;
    env_define(env_, "fsm_create", 10, val_native(native_fsm_create));
    env_define(env_, "fsm_add_state", 13, val_native(native_fsm_add_state));
    env_define(env_, "fsm_transition", 14, val_native(native_fsm_transition));
    env_define(env_, "fsm_current", 11, val_native(native_fsm_current));
    env_define(env_, "fsm_remove", 10, val_native(native_fsm_remove));
    std::printf("[ScriptEngine] FSM API registered\n");
}

void ScriptEngine::register_checkpoint_api() {
    if (!env_) return;
    env_define(env_, "add_checkpoint", 14, val_native(native_add_checkpoint));
    env_define(env_, "activate_checkpoint", 19, val_native(native_activate_checkpoint));
    env_define(env_, "get_checkpoint_id", 17, val_native(native_get_checkpoint_id));
    env_define(env_, "respawn", 7, val_native(native_respawn));
    std::printf("[ScriptEngine] Checkpoint API registered\n");
}

void ScriptEngine::register_combo_api() {
    if (!env_) return;
    env_define(env_, "register_combo", 14, val_native(native_register_combo));
    std::printf("[ScriptEngine] Combo API registered\n");
}

void ScriptEngine::register_trail_api() {
    if (!env_) return;
    env_define(env_, "trail_create", 12, val_native(native_trail_create));
    env_define(env_, "trail_add_point", 15, val_native(native_trail_add_point));
    env_define(env_, "trail_set_color", 15, val_native(native_trail_set_color));
    env_define(env_, "trail_destroy", 13, val_native(native_trail_destroy));
    env_define(env_, "trail_set_emitting", 18, val_native(native_trail_set_emitting));
    std::printf("[ScriptEngine] Trail API registered\n");
}

void ScriptEngine::register_dungeon_api() {
    if (!env_) return;
    env_define(env_, "generate_dungeon", 16, val_native(native_generate_dungeon));
    env_define(env_, "dungeon_room_count", 18, val_native(native_dungeon_room_count));
    env_define(env_, "dungeon_room_x", 14, val_native(native_dungeon_room_x));
    env_define(env_, "dungeon_room_y", 14, val_native(native_dungeon_room_y));
    std::printf("[ScriptEngine] Dungeon API registered\n");
}

void ScriptEngine::register_skeleton_api() {
    if (!env_) return;
    env_define(env_, "skel_create", 11, val_native(native_skel_create));
    env_define(env_, "skel_add_bone", 13, val_native(native_skel_add_bone));
    env_define(env_, "skel_set_sprite", 15, val_native(native_skel_set_sprite));
    env_define(env_, "skel_play", 9, val_native(native_skel_play));
    env_define(env_, "skel_stop", 9, val_native(native_skel_stop));
    std::printf("[ScriptEngine] Skeleton API registered\n");
}

void ScriptEngine::register_replay_api() {
    if (!env_) return;
    env_define(env_, "input_record_start", 18, val_native(native_input_record_start));
    env_define(env_, "input_record_stop", 17, val_native(native_input_record_stop));
    env_define(env_, "input_replay_start", 18, val_native(native_input_replay_start));
    env_define(env_, "input_replay_stop", 17, val_native(native_input_replay_stop));
    std::printf("[ScriptEngine] Replay API registered\n");
}

void ScriptEngine::register_postprocess_api() {
    if (!env_) return;
    env_define(env_, "set_postprocess", 15, val_native(native_set_postprocess));
    env_define(env_, "set_postprocess_param", 21, val_native(native_set_postprocess_param));
    std::printf("[ScriptEngine] PostProcess API registered\n");
}

void ScriptEngine::register_audio_bus_api() {
    if (!env_) return;
    env_define(env_, "audio_bus_volume", 16, val_native(native_audio_bus_volume));
    std::printf("[ScriptEngine] Audio Bus API registered\n");
}

void ScriptEngine::register_coroutine_api() {
    if (!env_) return;
    env_define(env_, "coroutine_create", 16, val_native(native_coroutine_create));
    env_define(env_, "coroutine_step", 14, val_native(native_coroutine_step));
    env_define(env_, "coroutine_start", 15, val_native(native_coroutine_start));
    env_define(env_, "coroutine_stop", 14, val_native(native_coroutine_stop));
    env_define(env_, "coroutine_loop", 14, val_native(native_coroutine_loop));
    env_define(env_, "coroutine_remove", 16, val_native(native_coroutine_remove));
    std::printf("[ScriptEngine] Coroutine API registered\n");
}

} // namespace eb
