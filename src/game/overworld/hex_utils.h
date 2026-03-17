#pragma once
#include "engine/core/types.h"
#include <cmath>
#include <array>

namespace eb {

// Hex grid utilities (pointy-top, offset coordinates — odd-r layout)

inline Vec2 hex_to_screen(int col, int row, float hex_size) {
    float w = std::sqrt(3.0f) * hex_size;
    float h = 2.0f * hex_size;
    float x = w * (col + 0.5f * (row & 1));
    float y = h * 0.75f * row;
    return {x, y};
}

inline Vec2i screen_to_hex(float sx, float sy, float hex_size) {
    float w = std::sqrt(3.0f) * hex_size;
    float h = 2.0f * hex_size;
    int row = (int)std::round(sy / (h * 0.75f));
    int col = (int)std::round((sx - 0.5f * (row & 1) * w) / w);
    return {col, row};
}

// 6 neighbor offsets for odd-r hex grid
inline std::array<Vec2i, 6> hex_neighbors(int col, int row) {
    if (row & 1) { // odd row
        return {{ {col+1,row}, {col,row-1}, {col-1,row-1}, {col-1,row}, {col-1,row+1}, {col,row+1} }};
    } else { // even row
        return {{ {col+1,row}, {col+1,row-1}, {col,row-1}, {col-1,row}, {col,row+1}, {col+1,row+1} }};
    }
}

inline int hex_distance(int c1, int r1, int c2, int r2) {
    // Convert to cube coordinates for distance
    auto to_cube = [](int col, int row) -> Vec3 {
        float x = (float)col - (float)(row - (row & 1)) / 2.0f;
        float z = (float)row;
        float y = -x - z;
        return {x, y, z};
    };
    auto a = to_cube(c1, r1), b = to_cube(c2, r2);
    return (int)((std::abs(a.x-b.x) + std::abs(a.y-b.y) + std::abs(a.z-b.z)) / 2.0f);
}

} // namespace eb
