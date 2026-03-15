#include "game/systems/spawn_system.h"
#include "game/game.h"
#include "game/systems/day_night.h"
#include "engine/scripting/script_engine.h"
#include <cstdlib>
#include <algorithm>

namespace eb {

static NPC* find_npc_template(GameState& game, const std::string& name) {
    for (auto& npc : game.npcs)
        if (npc.name == name) return &npc;
    return nullptr;
}

// Count how many living clones of this template exist
static int count_living_clones(GameState& game, const std::string& template_name) {
    int count = 0;
    std::string prefix = template_name + "_";
    for (auto& npc : game.npcs) {
        if (npc.name.size() > prefix.size() &&
            npc.name.compare(0, prefix.size(), prefix) == 0 &&
            npc.schedule.currently_visible) {
            count++;
        }
    }
    return count;
}

void update_spawn_loops(GameState& game, float dt) {
    for (auto& loop : game.spawn_loops) {
        if (!loop.active) continue;

        // Time-of-day gating — only spawn during allowed hours
        if (loop.has_time_gate) {
            if (!eb::is_hour_in_range(game.day_night.game_hours,
                                       loop.spawn_start_hour, loop.spawn_end_hour)) {
                loop.timer = 0; // Reset timer so spawns start fresh when time comes
                continue;
            }
        }

        // Count living clones instead of using static counter
        int alive = count_living_clones(game, loop.npc_template_name);
        if (alive >= loop.max_count) continue;

        loop.timer += dt;
        if (loop.timer < loop.interval) continue;
        loop.timer -= loop.interval;

        NPC* tmpl = find_npc_template(game, loop.npc_template_name);
        if (!tmpl) continue;

        NPC spawned = *tmpl;
        spawned.has_triggered = false;
        spawned.aggro_active = false;
        spawned.path_active = false;
        spawned.route.active = false;
        spawned.route.waypoints.clear();
        spawned.current_path.clear();
        spawned.schedule.currently_visible = true;

        // Unique name with incrementing counter
        loop.current_count++;
        spawned.name = loop.npc_template_name + "_" + std::to_string(loop.current_count);

        // Position: random within area or at template's original home
        if (loop.has_area) {
            int range_x = std::max(1, static_cast<int>(loop.area_max.x - loop.area_min.x));
            int range_y = std::max(1, static_cast<int>(loop.area_max.y - loop.area_min.y));
            float rx = loop.area_min.x + static_cast<float>(std::rand() % range_x);
            float ry = loop.area_min.y + static_cast<float>(std::rand() % range_y);
            spawned.position = {rx, ry};
        } else {
            spawned.position = tmpl->home_pos;
        }
        spawned.home_pos = spawned.position;
        spawned.wander_target = spawned.position;

        game.npcs.push_back(spawned);

        // Call on_spawn callback if set
        if (!loop.on_spawn_func.empty() && game.script_engine) {
            game.script_engine->set_string("spawned_npc", spawned.name);
            game.script_engine->set_number("spawned_index", loop.current_count - 1);
            if (game.script_engine->has_function(loop.on_spawn_func))
                game.script_engine->call_function(loop.on_spawn_func);
        }
    }
}

} // namespace eb
