#pragma once

struct GameState;

namespace eb {

// Update all active spawn loops. Spawns NPCs at intervals.
void update_spawn_loops(GameState& game, float dt);

} // namespace eb
