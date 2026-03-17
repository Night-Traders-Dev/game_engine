#pragma once

#include <cmath>
#include <algorithm>
#include "engine/core/types.h"
#include "engine/physics/collision.h"

namespace eb {

struct Ray {
    Vec2 origin;
    Vec2 direction; // should be normalized
};

struct RayHit {
    bool hit = false;
    Vec2 point = {0, 0};
    Vec2 normal = {0, 0};
    float distance = 0;
    int entity_id = -1;
};

// Slab method for ray vs AABB
inline RayHit raycast_rect(const Ray& ray, const Rect& rect, float max_dist) {
    RayHit result;

    float inv_dx = (std::abs(ray.direction.x) > 1e-8f) ? 1.0f / ray.direction.x : 1e18f;
    float inv_dy = (std::abs(ray.direction.y) > 1e-8f) ? 1.0f / ray.direction.y : 1e18f;

    float t1 = (rect.x - ray.origin.x) * inv_dx;
    float t2 = (rect.x + rect.w - ray.origin.x) * inv_dx;
    float t3 = (rect.y - ray.origin.y) * inv_dy;
    float t4 = (rect.y + rect.h - ray.origin.y) * inv_dy;

    float tmin = std::max(std::min(t1, t2), std::min(t3, t4));
    float tmax = std::min(std::max(t1, t2), std::max(t3, t4));

    if (tmax < 0 || tmin > tmax || tmin > max_dist) {
        return result;
    }

    float t = (tmin >= 0) ? tmin : tmax;
    if (t < 0 || t > max_dist) return result;

    result.hit = true;
    result.distance = t;
    result.point = {ray.origin.x + ray.direction.x * t,
                    ray.origin.y + ray.direction.y * t};

    // Determine hit normal
    if (t == std::min(t1, t2)) {
        // We could also check which of t1/t2 is smaller to get sign
    }
    // Recompute: find which face was hit
    float eps = 1e-4f;
    if (std::abs(result.point.x - rect.x) < eps)              result.normal = {-1, 0};
    else if (std::abs(result.point.x - (rect.x + rect.w)) < eps) result.normal = {1, 0};
    else if (std::abs(result.point.y - rect.y) < eps)              result.normal = {0, -1};
    else if (std::abs(result.point.y - (rect.y + rect.h)) < eps) result.normal = {0, 1};

    return result;
}

inline RayHit raycast_circle(const Ray& ray, const Circle& circle, float max_dist) {
    RayHit result;

    Vec2 oc = {ray.origin.x - circle.center.x, ray.origin.y - circle.center.y};
    float a = ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y;
    float b = 2.0f * (oc.x * ray.direction.x + oc.y * ray.direction.y);
    float c = oc.x * oc.x + oc.y * oc.y - circle.radius * circle.radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0) return result;

    float sqrt_disc = std::sqrt(discriminant);
    float t = (-b - sqrt_disc) / (2.0f * a);

    // If t < 0, we're inside the circle; try the far intersection
    if (t < 0) {
        t = (-b + sqrt_disc) / (2.0f * a);
    }
    if (t < 0 || t > max_dist) return result;

    result.hit = true;
    result.distance = t;
    result.point = {ray.origin.x + ray.direction.x * t,
                    ray.origin.y + ray.direction.y * t};
    Vec2 diff = {result.point.x - circle.center.x, result.point.y - circle.center.y};
    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (len > 1e-8f) {
        result.normal = {diff.x / len, diff.y / len};
    }

    return result;
}

// DDA algorithm for raycasting through a tile grid
// collision_grid: pointer to bool/int array where true/non-zero = solid
inline RayHit raycast_tilemap(const Ray& ray, const bool* collision_grid,
                               int map_w, int map_h, float tile_size, float max_dist) {
    RayHit result;
    if (!collision_grid) return result;

    // Starting tile coords
    int tile_x = static_cast<int>(std::floor(ray.origin.x / tile_size));
    int tile_y = static_cast<int>(std::floor(ray.origin.y / tile_size));

    // Step direction
    int step_x = (ray.direction.x >= 0) ? 1 : -1;
    int step_y = (ray.direction.y >= 0) ? 1 : -1;

    // Distance along ray to cross one full tile in each axis
    float t_delta_x = (std::abs(ray.direction.x) > 1e-8f)
        ? std::abs(tile_size / ray.direction.x) : 1e18f;
    float t_delta_y = (std::abs(ray.direction.y) > 1e-8f)
        ? std::abs(tile_size / ray.direction.y) : 1e18f;

    // Distance to next tile boundary
    float t_max_x, t_max_y;
    if (std::abs(ray.direction.x) > 1e-8f) {
        float next_x = (step_x > 0) ? (tile_x + 1) * tile_size : tile_x * tile_size;
        t_max_x = (next_x - ray.origin.x) / ray.direction.x;
    } else {
        t_max_x = 1e18f;
    }
    if (std::abs(ray.direction.y) > 1e-8f) {
        float next_y = (step_y > 0) ? (tile_y + 1) * tile_size : tile_y * tile_size;
        t_max_y = (next_y - ray.origin.y) / ray.direction.y;
    } else {
        t_max_y = 1e18f;
    }

    float t = 0;
    while (t <= max_dist) {
        // Check current tile
        if (tile_x >= 0 && tile_x < map_w && tile_y >= 0 && tile_y < map_h) {
            if (collision_grid[tile_y * map_w + tile_x]) {
                // Hit a solid tile -- compute exact intersection with tile rect
                Rect tile_rect = {tile_x * tile_size, tile_y * tile_size, tile_size, tile_size};
                RayHit tile_hit = raycast_rect(ray, tile_rect, max_dist);
                if (tile_hit.hit) return tile_hit;
            }
        } else {
            // Out of bounds
            break;
        }

        // Step to next tile
        if (t_max_x < t_max_y) {
            t = t_max_x;
            t_max_x += t_delta_x;
            tile_x += step_x;
        } else {
            t = t_max_y;
            t_max_y += t_delta_y;
            tile_y += step_y;
        }
    }

    return result;
}

// Returns true if there is a clear line of sight (no solid tiles) between two points
inline bool line_of_sight(Vec2 from, Vec2 to, const bool* collision_grid,
                           int map_w, int map_h, float tile_size) {
    Vec2 diff = {to.x - from.x, to.y - from.y};
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist < 1e-8f) return true;

    Ray ray;
    ray.origin = from;
    ray.direction = {diff.x / dist, diff.y / dist};

    RayHit hit = raycast_tilemap(ray, collision_grid, map_w, map_h, tile_size, dist);
    return !hit.hit;
}

} // namespace eb
