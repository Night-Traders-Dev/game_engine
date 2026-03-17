#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "engine/core/types.h"

namespace eb {

struct TrailPoint {
    Vec2 position;
    float time_added;
    Vec4 color;
    float width;
};

struct Trail {
    std::string id;
    std::vector<TrailPoint> points;
    int max_points = 64;
    float lifetime = 0.5f;
    float base_width = 8.0f;
    Vec4 color_start = {1, 1, 1, 1};
    Vec4 color_end = {1, 1, 1, 0};
    bool active = true;
    bool emitting = true;
};

inline void trail_add_point(Trail& trail, Vec2 pos, float game_time) {
    if (!trail.active || !trail.emitting) return;

    TrailPoint pt;
    pt.position = pos;
    pt.time_added = game_time;
    pt.color = trail.color_start;
    pt.width = trail.base_width;
    trail.points.push_back(pt);

    // Enforce max points by removing oldest
    while (static_cast<int>(trail.points.size()) > trail.max_points) {
        trail.points.erase(trail.points.begin());
    }
}

inline void trail_update(Trail& trail, float game_time) {
    if (!trail.active) return;

    trail.points.erase(
        std::remove_if(trail.points.begin(), trail.points.end(),
            [&](const TrailPoint& pt) {
                return (game_time - pt.time_added) >= trail.lifetime;
            }),
        trail.points.end());
}

struct TrailQuad {
    Vec2 p0, p1, p2, p3;
    Vec4 c0, c1;
};

inline std::vector<TrailQuad> trail_build_mesh(Trail& trail, float game_time) {
    std::vector<TrailQuad> quads;
    if (trail.points.size() < 2) return quads;

    quads.reserve(trail.points.size() - 1);

    for (size_t i = 0; i < trail.points.size() - 1; ++i) {
        const TrailPoint& a = trail.points[i];
        const TrailPoint& b = trail.points[i + 1];

        float age_a = game_time - a.time_added;
        float age_b = game_time - b.time_added;
        float t_a = (trail.lifetime > 0) ? std::clamp(age_a / trail.lifetime, 0.0f, 1.0f) : 1.0f;
        float t_b = (trail.lifetime > 0) ? std::clamp(age_b / trail.lifetime, 0.0f, 1.0f) : 1.0f;

        // Interpolate width by age (full width at birth, zero at death)
        float w_a = trail.base_width * (1.0f - t_a);
        float w_b = trail.base_width * (1.0f - t_b);

        // Interpolate color by age
        Vec4 c_a = trail.color_start * (1.0f - t_a) + trail.color_end * t_a;
        Vec4 c_b = trail.color_start * (1.0f - t_b) + trail.color_end * t_b;

        // Direction from a to b
        Vec2 dir = b.position - a.position;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.0001f) continue;

        // Perpendicular (normal)
        Vec2 perp = {-dir.y / len, dir.x / len};

        TrailQuad q;
        q.p0 = a.position + perp * (w_a * 0.5f);
        q.p1 = a.position - perp * (w_a * 0.5f);
        q.p2 = b.position - perp * (w_b * 0.5f);
        q.p3 = b.position + perp * (w_b * 0.5f);
        q.c0 = c_a;
        q.c1 = c_b;
        quads.push_back(q);
    }

    return quads;
}

} // namespace eb
