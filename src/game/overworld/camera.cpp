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
    // Apply follow offset so player appears below center (Twilight style)
    target_ = {target.x + follow_offset_.x, target.y + follow_offset_.y};
    following_ = true;
    follow_speed_ = speed;
}

void Camera::follow_platformer(Vec2 target, int facing_dir, bool on_ground, float speed) {
    platformer_mode_ = true;
    following_ = true;
    follow_speed_ = speed;

    // Horizontal: lerp toward target.x + lookahead in facing direction
    float desired_lookahead = facing_dir * lookahead_x_;
    float la_t = 1.0f - std::exp(-lookahead_speed_ * 0.016f); // approximate dt
    current_lookahead_ += (desired_lookahead - current_lookahead_) * la_t;
    target_.x = target.x + current_lookahead_;

    // Vertical: only adjust when target is outside deadzone band
    float half_h = viewport_.y * 0.5f;
    float top_band = position_.y - half_h * deadzone_y_top_;
    float bottom_band = position_.y + half_h * deadzone_y_bottom_;

    if (target.y < top_band) {
        float snap = on_ground ? vertical_snap_speed_ : (vertical_snap_speed_ * 0.5f);
        float vt = 1.0f - std::exp(-snap * 0.016f);
        target_.y = position_.y + (target.y - top_band) * vt + (target.y - position_.y) * vt;
    } else if (target.y > bottom_band) {
        float snap = on_ground ? vertical_snap_speed_ : (vertical_snap_speed_ * 0.5f);
        float vt = 1.0f - std::exp(-snap * 0.016f);
        target_.y = position_.y + (target.y - bottom_band) * vt + (target.y - position_.y) * vt;
    } else {
        // Inside deadzone — only snap if on ground
        if (on_ground) {
            float snap = vertical_snap_speed_ * 0.3f;
            float vt = 1.0f - std::exp(-snap * 0.016f);
            target_.y = position_.y + (target.y - position_.y) * vt;
        } else {
            target_.y = position_.y;
        }
    }
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

        // Smooth zoom
        if (std::abs(zoom_ - target_zoom_) > 0.001f) {
            zoom_ += (target_zoom_ - zoom_) * std::min(1.0f, zoom_speed_ * dt);
        } else {
            zoom_ = target_zoom_;
        }

        // Perlin shake
        if (perlin_shake_timer_ > 0) {
            perlin_shake_timer_ -= dt;
            perlin_shake_time_ += dt;
            float t = perlin_shake_time_ * perlin_shake_freq_;
            float fade = std::min(1.0f, perlin_shake_timer_ / std::max(0.1f, perlin_shake_duration_ * 0.3f));
            shake_offset_.x = eb::perlin2d(t, 0.0f) * perlin_shake_intensity_ * fade;
            shake_offset_.y = eb::perlin2d(0.0f, t) * perlin_shake_intensity_ * fade;
            position_ += shake_offset_;
        } else {
            shake_offset_ = {0, 0};
        }

        clamp_to_bounds();
    }
}

void Camera::zoom_to(float target, float speed) {
    target_zoom_ = std::max(0.1f, target);
    zoom_speed_ = speed;
}

void Camera::set_zoom(float z) {
    zoom_ = target_zoom_ = std::max(0.1f, z);
}

void Camera::shake_perlin(float intensity, float duration, float frequency) {
    perlin_shake_intensity_ = intensity;
    perlin_shake_duration_ = duration;
    perlin_shake_timer_ = duration;
    perlin_shake_freq_ = frequency;
    perlin_shake_time_ = 0;
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
    float vw = viewport_.x / zoom_;
    float vh = viewport_.y / zoom_;
    Vec2 zoom_off = {position_.x - vw * 0.5f, position_.y - vh * 0.5f};
    return glm::ortho(zoom_off.x, zoom_off.x + vw,
                      zoom_off.y, zoom_off.y + vh,
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
