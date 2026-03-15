#pragma once

struct SurvivalStats;

namespace eb {

// Update survival stats. minutes_elapsed = game minutes this frame.
// Modifies player_speed (slow when hungry) and player_hp (drain when starving).
void update_survival(SurvivalStats& stats, float minutes_elapsed,
                      float& player_speed, int& player_hp, float base_speed);

} // namespace eb
