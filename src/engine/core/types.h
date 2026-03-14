#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace eb {

using Vec2 = glm::vec2;
using Vec2i = glm::ivec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

struct Rect {
    float x, y, w, h;

    bool contains(Vec2 point) const {
        return point.x >= x && point.x <= x + w &&
               point.y >= y && point.y <= y + h;
    }

    bool intersects(const Rect& other) const {
        return x < other.x + other.w && x + w > other.x &&
               y < other.y + other.h && y + h > other.y;
    }
};

struct Color {
    float r, g, b, a;

    static Color white()   { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color black()   { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color red()     { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color green()   { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color blue()    { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static Color clear()   { return {0.0f, 0.0f, 0.0f, 0.0f}; }
};

} // namespace eb
