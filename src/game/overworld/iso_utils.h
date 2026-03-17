#pragma once
#include "engine/core/types.h"
#include <cmath>

namespace eb {

// Isometric coordinate conversion utilities
// Diamond (staggered) isometric projection

inline Vec2 iso_to_screen(int tile_x, int tile_y, int tile_w, int tile_h) {
    float sx = (tile_x - tile_y) * (tile_w * 0.5f);
    float sy = (tile_x + tile_y) * (tile_h * 0.5f);
    return {sx, sy};
}

inline Vec2i screen_to_iso(float screen_x, float screen_y, int tile_w, int tile_h) {
    float tw = (float)tile_w, th = (float)tile_h;
    float tx = (screen_x / (tw * 0.5f) + screen_y / (th * 0.5f)) * 0.5f;
    float ty = (screen_y / (th * 0.5f) - screen_x / (tw * 0.5f)) * 0.5f;
    return {(int)std::floor(tx), (int)std::floor(ty)};
}

// Isometric draw order: back-to-front by (x + y), then by y
inline int iso_sort_key(int tile_x, int tile_y) {
    return (tile_x + tile_y) * 1000 + tile_y;
}

} // namespace eb
