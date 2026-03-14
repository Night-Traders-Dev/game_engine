#include "game/overworld/camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace eb {

Camera::Camera() = default;

Camera::Camera(float viewport_w, float viewport_h)
    : viewport_(viewport_w, viewport_h) {}

void Camera::set_viewport(float w, float h) {
    viewport_ = {w, h};
}

void Camera::set_position(Vec2 pos) {
    position_ = pos;
    clamp_to_bounds();
}

void Camera::set_bounds(float min_x, float min_y, float max_x, float max_y) {
    has_bounds_ = true;
    bounds_min_x_ = min_x;
    bounds_min_y_ = min_y;
    bounds_max_x_ = max_x;
    bounds_max_y_ = max_y;
    clamp_to_bounds();
}

void Camera::clear_bounds() {
    has_bounds_ = false;
}

void Camera::follow(Vec2 target, float speed) {
    // Apply follow offset so player appears below center (EarthBound style)
    target_ = {target.x + follow_offset_.x, target.y + follow_offset_.y};
    following_ = true;
    follow_speed_ = speed;
}

void Camera::center_on(Vec2 target) {
    position_ = target;
    following_ = false;
    clamp_to_bounds();
}

void Camera::update(float dt) {
    if (following_) {
        float t = 1.0f - std::exp(-follow_speed_ * dt);
        position_.x += (target_.x - position_.x) * t;
        position_.y += (target_.y - position_.y) * t;
        clamp_to_bounds();
    }
}

Vec2 Camera::offset() const {
    return {position_.x - viewport_.x * 0.5f, position_.y - viewport_.y * 0.5f};
}

Rect Camera::visible_area() const {
    Vec2 off = offset();
    return {off.x, off.y, viewport_.x, viewport_.y};
}

Mat4 Camera::projection_matrix() const {
    Vec2 off = offset();
    // Vulkan clip space has Y going down (+Y = bottom of screen)
    // so bottom < top to get correct orientation
    return glm::ortho(off.x, off.x + viewport_.x,
                      off.y, off.y + viewport_.y,
                      -1.0f, 1.0f);
}

void Camera::clamp_to_bounds() {
    if (!has_bounds_) return;

    float half_w = viewport_.x * 0.5f;
    float half_h = viewport_.y * 0.5f;

    // If map is smaller than viewport, center the map
    if (bounds_max_x_ - bounds_min_x_ < viewport_.x) {
        position_.x = (bounds_min_x_ + bounds_max_x_) * 0.5f;
    } else {
        position_.x = std::clamp(position_.x, bounds_min_x_ + half_w, bounds_max_x_ - half_w);
    }

    if (bounds_max_y_ - bounds_min_y_ < viewport_.y) {
        position_.y = (bounds_min_y_ + bounds_max_y_) * 0.5f;
    } else {
        position_.y = std::clamp(position_.y, bounds_min_y_ + half_h, bounds_max_y_ - half_h);
    }
}

} // namespace eb
