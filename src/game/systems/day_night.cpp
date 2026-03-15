#include "game/systems/day_night.h"
#include "game/game.h"
#include <cmath>

namespace eb {

static Vec4 lerp_color(Vec4 a, Vec4 b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

Vec4 compute_day_tint(float hour) {
    // Tint colors at key times of day
    // These are overlay colors: {r, g, b, alpha}
    // Alpha controls darkness intensity
    Vec4 night   = {0.10f, 0.10f, 0.25f, 0.55f};  // Dark blue
    Vec4 dawn    = {0.40f, 0.25f, 0.15f, 0.30f};   // Warm orange
    Vec4 morning = {0.0f, 0.0f, 0.0f, 0.0f};       // Clear (no tint)
    Vec4 day     = {0.0f, 0.0f, 0.0f, 0.0f};       // Clear
    Vec4 sunset  = {0.50f, 0.25f, 0.05f, 0.25f};   // Orange
    Vec4 dusk    = {0.15f, 0.10f, 0.30f, 0.40f};   // Purple-blue

    if (hour < 5.0f)        return night;
    if (hour < 6.0f)        return lerp_color(night, dawn, (hour - 5.0f));
    if (hour < 7.5f)        return lerp_color(dawn, morning, (hour - 6.0f) / 1.5f);
    if (hour < 16.0f)       return day;
    if (hour < 18.0f)       return lerp_color(day, sunset, (hour - 16.0f) / 2.0f);
    if (hour < 20.0f)       return lerp_color(sunset, dusk, (hour - 18.0f) / 2.0f);
    if (hour < 21.0f)       return lerp_color(dusk, night, (hour - 20.0f));
    return night;
}

void update_day_night(DayNightCycle& cycle, float dt) {
    // Advance clock: day_speed minutes per real second
    float minutes = dt * cycle.day_speed;
    cycle.game_hours += minutes / 60.0f;

    // Wrap to 0-24
    while (cycle.game_hours >= 24.0f) cycle.game_hours -= 24.0f;
    while (cycle.game_hours < 0.0f) cycle.game_hours += 24.0f;

    cycle.current_tint = compute_day_tint(cycle.game_hours);
}

bool is_hour_in_range(float hour, float start, float end) {
    if (start <= end) {
        return hour >= start && hour < end;
    }
    // Wrapping range (e.g. 22:00 to 06:00)
    return hour >= start || hour < end;
}

} // namespace eb
