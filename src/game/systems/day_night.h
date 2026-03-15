#pragma once

#include "engine/core/types.h"

struct DayNightCycle;

namespace eb {

// Advance the day-night clock and compute current tint color
void update_day_night(DayNightCycle& cycle, float dt);

// Get tint color for a given hour (0-24)
Vec4 compute_day_tint(float hour);

// Check if hour falls within a time range (handles wrapping, e.g. 22-6)
bool is_hour_in_range(float hour, float start, float end);

} // namespace eb
