#include "game/systems/spawn_system.h"
#include "game/game.h"
#include <cstdlib>

namespace eb {

// Find an NPC by name (template for cloning)
static NPC* find_npc_template(GameState& game, const std::string& name) {
    for (auto& npc : game.npcs)
        if (npc.name == name) return &npc;
    return nullptr;
}

void update_spawn_loops(GameState& game, float dt) {
    for (auto& loop : game.spawn_loops) {
        if (!loop.active) continue;
        if (loop.current_count >= loop.max_count) continue;

        loop.timer += dt;
        if (loop.timer < loop.interval) continue;
        loop.timer -= loop.interval;

        // Find template NPC to clone
        NPC* tmpl = find_npc_template(game, loop.npc_template_name);
        if (!tmpl) continue;

        // Clone the template
        NPC spawned = *tmpl;
        spawned.has_triggered = false;
        spawned.aggro_active = false;
        spawned.path_active = false;
        spawned.route.active = false;

        // Unique name
        spawned.name = loop.npc_template_name + "_" + std::to_string(loop.current_count);

        // Position: random within area or at template position
        if (loop.has_area) {
            float rx = loop.area_min.x + static_cast<float>(std::rand() % static_cast<int>(
                loop.area_max.x - loop.area_min.x + 1));
            float ry = loop.area_min.y + static_cast<float>(std::rand() % static_cast<int>(
                loop.area_max.y - loop.area_min.y + 1));
            spawned.position = {rx, ry};
        }
        spawned.home_pos = spawned.position;
        spawned.wander_target = spawned.position;

        game.npcs.push_back(spawned);
        loop.current_count++;
    }
}

} // namespace eb
