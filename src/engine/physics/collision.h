#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include "engine/core/types.h"

namespace eb {

struct Circle {
    Vec2 center;
    float radius;
};

struct Polygon {
    std::vector<Vec2> vertices; // convex only
};

struct CollisionResult {
    bool hit = false;
    Vec2 normal = {0, 0};
    float depth = 0;
};

enum class ColliderType { None, AABB, CircleShape, Poly };

struct Collider {
    ColliderType type = ColliderType::None;
    Rect aabb = {};
    Circle circle = {};
    Polygon polygon = {};
};

// --- Helpers ---

inline Vec2 rect_center(const Rect& r) {
    return {r.x + r.w * 0.5f, r.y + r.h * 0.5f};
}

inline float vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

inline float vec2_length(Vec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

inline Vec2 vec2_normalize(Vec2 v) {
    float len = vec2_length(v);
    if (len < 1e-8f) return {0, 0};
    return {v.x / len, v.y / len};
}

// --- Point tests ---

inline CollisionResult point_in_rect(Vec2 p, const Rect& r) {
    CollisionResult res;
    if (p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h) {
        res.hit = true;
        // push-out: find smallest penetration axis
        float dl = p.x - r.x;
        float dr = (r.x + r.w) - p.x;
        float dt = p.y - r.y;
        float db = (r.y + r.h) - p.y;
        float min_d = dl;
        res.normal = {-1, 0};
        res.depth = dl;
        if (dr < min_d) { min_d = dr; res.normal = {1, 0}; res.depth = dr; }
        if (dt < min_d) { min_d = dt; res.normal = {0, -1}; res.depth = dt; }
        if (db < min_d) { min_d = db; res.normal = {0, 1}; res.depth = db; }
    }
    return res;
}

inline CollisionResult point_in_circle(Vec2 p, const Circle& c) {
    CollisionResult res;
    Vec2 diff = {p.x - c.center.x, p.y - c.center.y};
    float dist_sq = diff.x * diff.x + diff.y * diff.y;
    if (dist_sq <= c.radius * c.radius) {
        res.hit = true;
        float dist = std::sqrt(dist_sq);
        if (dist > 1e-8f) {
            res.normal = {diff.x / dist, diff.y / dist};
            res.depth = c.radius - dist;
        } else {
            res.normal = {0, -1};
            res.depth = c.radius;
        }
    }
    return res;
}

inline CollisionResult point_in_polygon(Vec2 p, const Polygon& poly) {
    CollisionResult res;
    int n = static_cast<int>(poly.vertices.size());
    if (n < 3) return res;

    // Check if point is on the same side of every edge (convex polygon)
    bool all_positive = true;
    bool all_negative = true;
    float min_depth = std::numeric_limits<float>::max();
    Vec2 best_normal = {0, 0};

    for (int i = 0; i < n; i++) {
        Vec2 a = poly.vertices[i];
        Vec2 b = poly.vertices[(i + 1) % n];
        Vec2 edge = {b.x - a.x, b.y - a.y};
        // outward normal (assuming CCW winding)
        Vec2 normal = vec2_normalize({edge.y, -edge.x});
        float d = vec2_dot({p.x - a.x, p.y - a.y}, normal);

        if (d > 0) all_negative = false;
        if (d < 0) all_positive = false;

        // track separation along this axis
        if (-d < min_depth) {
            min_depth = -d;
            best_normal = {-normal.x, -normal.y};
        }
    }

    if (all_positive || all_negative) {
        res.hit = true;
        res.depth = min_depth;
        res.normal = best_normal;
    }
    return res;
}

// --- Shape vs shape ---

inline CollisionResult rect_overlap(const Rect& a, const Rect& b) {
    CollisionResult res;
    float ox = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
    float oy = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);

    if (ox > 0 && oy > 0) {
        res.hit = true;
        Vec2 ca = rect_center(a);
        Vec2 cb = rect_center(b);
        if (ox < oy) {
            res.depth = ox;
            res.normal = (ca.x < cb.x) ? Vec2{-1, 0} : Vec2{1, 0};
        } else {
            res.depth = oy;
            res.normal = (ca.y < cb.y) ? Vec2{0, -1} : Vec2{0, 1};
        }
    }
    return res;
}

inline CollisionResult rect_vs_rect(const Rect& a, const Rect& b) {
    return rect_overlap(a, b);
}

inline CollisionResult rect_vs_circle(const Rect& r, const Circle& c) {
    CollisionResult res;
    // Closest point on rect to circle center
    float cx = std::max(r.x, std::min(c.center.x, r.x + r.w));
    float cy = std::max(r.y, std::min(c.center.y, r.y + r.h));

    float dx = c.center.x - cx;
    float dy = c.center.y - cy;
    float dist_sq = dx * dx + dy * dy;

    if (dist_sq <= c.radius * c.radius) {
        res.hit = true;
        float dist = std::sqrt(dist_sq);
        if (dist > 1e-8f) {
            res.normal = {dx / dist, dy / dist};
            res.depth = c.radius - dist;
        } else {
            // Circle center is inside the rect
            // Find the shortest push-out
            float dl = c.center.x - r.x;
            float dr_dist = (r.x + r.w) - c.center.x;
            float dt = c.center.y - r.y;
            float db = (r.y + r.h) - c.center.y;
            float min_d = dl; res.normal = {-1, 0};
            if (dr_dist < min_d) { min_d = dr_dist; res.normal = {1, 0}; }
            if (dt < min_d) { min_d = dt; res.normal = {0, -1}; }
            if (db < min_d) { min_d = db; res.normal = {0, 1}; }
            res.depth = c.radius + min_d;
        }
    }
    return res;
}

inline CollisionResult circle_vs_circle(const Circle& a, const Circle& b) {
    CollisionResult res;
    Vec2 diff = {a.center.x - b.center.x, a.center.y - b.center.y};
    float dist_sq = diff.x * diff.x + diff.y * diff.y;
    float radii = a.radius + b.radius;

    if (dist_sq <= radii * radii) {
        res.hit = true;
        float dist = std::sqrt(dist_sq);
        if (dist > 1e-8f) {
            res.normal = {diff.x / dist, diff.y / dist};
            res.depth = radii - dist;
        } else {
            res.normal = {0, -1};
            res.depth = radii;
        }
    }
    return res;
}

// --- SAT helpers ---

namespace detail {

inline void project_polygon(const Polygon& poly, Vec2 axis, float& out_min, float& out_max) {
    out_min = std::numeric_limits<float>::max();
    out_max = -std::numeric_limits<float>::max();
    for (auto& v : poly.vertices) {
        float p = vec2_dot(v, axis);
        if (p < out_min) out_min = p;
        if (p > out_max) out_max = p;
    }
}

inline void project_circle(const Circle& c, Vec2 axis, float& out_min, float& out_max) {
    float center_proj = vec2_dot(c.center, axis);
    out_min = center_proj - c.radius;
    out_max = center_proj + c.radius;
}

} // namespace detail

inline CollisionResult polygon_vs_polygon(const Polygon& a, const Polygon& b) {
    CollisionResult res;
    int na = static_cast<int>(a.vertices.size());
    int nb = static_cast<int>(b.vertices.size());
    if (na < 3 || nb < 3) return res;

    float min_overlap = std::numeric_limits<float>::max();
    Vec2 best_axis = {0, 0};

    // Test all edge normals from both polygons
    const Polygon* polys[2] = {&a, &b};
    int counts[2] = {na, nb};

    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < counts[p]; i++) {
            Vec2 va = polys[p]->vertices[i];
            Vec2 vb = polys[p]->vertices[(i + 1) % counts[p]];
            Vec2 edge = {vb.x - va.x, vb.y - va.y};
            Vec2 axis = vec2_normalize({-edge.y, edge.x});

            float a_min, a_max, b_min, b_max;
            detail::project_polygon(a, axis, a_min, a_max);
            detail::project_polygon(b, axis, b_min, b_max);

            if (a_max < b_min || b_max < a_min) {
                return res; // separating axis found, no collision
            }

            float overlap = std::min(a_max - b_min, b_max - a_min);
            if (overlap < min_overlap) {
                min_overlap = overlap;
                best_axis = axis;
            }
        }
    }

    res.hit = true;
    res.depth = min_overlap;

    // Ensure normal points from a toward b
    Vec2 ca = {0, 0}, cb = {0, 0};
    for (auto& v : a.vertices) { ca.x += v.x; ca.y += v.y; }
    for (auto& v : b.vertices) { cb.x += v.x; cb.y += v.y; }
    ca.x /= na; ca.y /= na;
    cb.x /= nb; cb.y /= nb;
    Vec2 dir = {cb.x - ca.x, cb.y - ca.y};
    if (vec2_dot(dir, best_axis) < 0) {
        best_axis = {-best_axis.x, -best_axis.y};
    }
    res.normal = best_axis;

    return res;
}

inline CollisionResult polygon_vs_circle(const Polygon& poly, const Circle& c) {
    CollisionResult res;
    int n = static_cast<int>(poly.vertices.size());
    if (n < 3) return res;

    float min_overlap = std::numeric_limits<float>::max();
    Vec2 best_axis = {0, 0};

    // Test polygon edge normals
    for (int i = 0; i < n; i++) {
        Vec2 va = poly.vertices[i];
        Vec2 vb = poly.vertices[(i + 1) % n];
        Vec2 edge = {vb.x - va.x, vb.y - va.y};
        Vec2 axis = vec2_normalize({-edge.y, edge.x});

        float p_min, p_max, c_min, c_max;
        detail::project_polygon(poly, axis, p_min, p_max);
        detail::project_circle(c, axis, c_min, c_max);

        if (p_max < c_min || c_max < p_min) return res;

        float overlap = std::min(p_max - c_min, c_max - p_min);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = axis;
        }
    }

    // Test axis from circle center to closest vertex
    float closest_dist_sq = std::numeric_limits<float>::max();
    Vec2 closest_vert = poly.vertices[0];
    for (auto& v : poly.vertices) {
        float dx = v.x - c.center.x;
        float dy = v.y - c.center.y;
        float d = dx * dx + dy * dy;
        if (d < closest_dist_sq) {
            closest_dist_sq = d;
            closest_vert = v;
        }
    }
    Vec2 axis = vec2_normalize({closest_vert.x - c.center.x, closest_vert.y - c.center.y});
    if (vec2_length(axis) > 1e-8f) {
        float p_min, p_max, c_min, c_max;
        detail::project_polygon(poly, axis, p_min, p_max);
        detail::project_circle(c, axis, c_min, c_max);

        if (p_max < c_min || c_max < p_min) return res;

        float overlap = std::min(p_max - c_min, c_max - p_min);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_axis = axis;
        }
    }

    res.hit = true;
    res.depth = min_overlap;

    // Normal points from polygon toward circle
    Vec2 poly_center = {0, 0};
    for (auto& v : poly.vertices) { poly_center.x += v.x; poly_center.y += v.y; }
    poly_center.x /= n; poly_center.y /= n;
    Vec2 dir = {c.center.x - poly_center.x, c.center.y - poly_center.y};
    if (vec2_dot(dir, best_axis) < 0) {
        best_axis = {-best_axis.x, -best_axis.y};
    }
    res.normal = best_axis;

    return res;
}

// --- Collider dispatcher ---

inline Polygon rect_to_polygon(const Rect& r) {
    Polygon p;
    p.vertices = {
        {r.x, r.y},
        {r.x + r.w, r.y},
        {r.x + r.w, r.y + r.h},
        {r.x, r.y + r.h}
    };
    return p;
}

inline CollisionResult collider_vs_collider(const Collider& a, const Collider& b) {
    if (a.type == ColliderType::None || b.type == ColliderType::None) return {};

    if (a.type == ColliderType::AABB && b.type == ColliderType::AABB) {
        return rect_vs_rect(a.aabb, b.aabb);
    }
    if (a.type == ColliderType::CircleShape && b.type == ColliderType::CircleShape) {
        return circle_vs_circle(a.circle, b.circle);
    }
    if (a.type == ColliderType::AABB && b.type == ColliderType::CircleShape) {
        return rect_vs_circle(a.aabb, b.circle);
    }
    if (a.type == ColliderType::CircleShape && b.type == ColliderType::AABB) {
        auto r = rect_vs_circle(b.aabb, a.circle);
        r.normal = {-r.normal.x, -r.normal.y};
        return r;
    }
    if (a.type == ColliderType::Poly && b.type == ColliderType::Poly) {
        return polygon_vs_polygon(a.polygon, b.polygon);
    }
    if (a.type == ColliderType::Poly && b.type == ColliderType::CircleShape) {
        return polygon_vs_circle(a.polygon, b.circle);
    }
    if (a.type == ColliderType::CircleShape && b.type == ColliderType::Poly) {
        auto r = polygon_vs_circle(b.polygon, a.circle);
        r.normal = {-r.normal.x, -r.normal.y};
        return r;
    }
    if (a.type == ColliderType::AABB && b.type == ColliderType::Poly) {
        Polygon pa = rect_to_polygon(a.aabb);
        return polygon_vs_polygon(pa, b.polygon);
    }
    if (a.type == ColliderType::Poly && b.type == ColliderType::AABB) {
        Polygon pb = rect_to_polygon(b.aabb);
        return polygon_vs_polygon(a.polygon, pb);
    }

    return {};
}

} // namespace eb
