#include "game/systems/survival.h"
#include "game/game.h"
#include <algorithm>

namespace eb {

void update_survival(SurvivalStats& stats, float minutes_elapsed,
                      float& player_speed, int& player_hp, float base_speed) {
    if (!stats.enabled) return;

    // Deplete stats
    stats.hunger = std::max(0.0f, stats.hunger - stats.hunger_rate * minutes_elapsed);
    stats.thirst = std::max(0.0f, stats.thirst - stats.thirst_rate * minutes_elapsed);
    stats.energy = std::max(0.0f, stats.energy - stats.energy_rate * minutes_elapsed);

    // Effects: speed reduction when hungry or tired
    float speed_mult = 1.0f;
    if (stats.hunger < 25.0f) speed_mult -= 0.2f;
    if (stats.thirst < 25.0f) speed_mult -= 0.15f;
    if (stats.energy < 20.0f) speed_mult -= 0.25f;
    player_speed = base_speed * std::max(0.3f, speed_mult);

    // HP drain when any stat hits 0
    if (stats.hunger <= 0.0f || stats.thirst <= 0.0f) {
        // Drain 1 HP per game-minute when starving/dehydrated
        static float drain_accum = 0.0f;
        drain_accum += minutes_elapsed;
        if (drain_accum >= 1.0f) {
            int drain = static_cast<int>(drain_accum);
            player_hp = std::max(1, player_hp - drain);
            drain_accum -= drain;
        }
    }
}

} // namespace eb
