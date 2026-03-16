#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <random>
#include "engine/core/types.h"

namespace eb {

struct Particle {
    Vec2 pos;
    Vec2 vel;
    Vec4 color;
    Vec4 color_end;
    float size = 4.0f;
    float size_end = 0.0f;
    float life = 0;
    float max_life = 1.0f;
    float rotation = 0;
    float rot_speed = 0;
    bool alive = false;

    float progress() const { return max_life > 0 ? life / max_life : 1.0f; }
};

enum class EmitShape { Point, Circle, Rect, Line };

struct ParticleEmitter {
    std::string id;
    Vec2 pos;
    bool active = true;
    bool world_space = true;   // if false, particles move with emitter

    // Emission config
    float rate = 10.0f;        // particles per second
    float emit_timer = 0;
    int burst_count = 0;       // if > 0, emit this many immediately then deactivate
    int max_particles = 100;

    // Particle property ranges (randomized between min/max)
    Vec2 vel_min = {-20, -50};
    Vec2 vel_max = {20, -10};
    Vec4 color_start = {1, 1, 1, 1};
    Vec4 color_end = {1, 1, 1, 0};
    float size_min = 2.0f, size_max = 6.0f;
    float size_end_min = 0.0f, size_end_max = 0.0f;
    float life_min = 0.5f, life_max = 1.5f;
    float gravity = 0.0f;     // pixels/sec^2 (positive = down)
    float spread_angle = 360.0f; // degrees, 360 = omnidirectional
    float base_angle = 270.0f;   // degrees, 270 = upward

    // Shape
    EmitShape shape = EmitShape::Point;
    float shape_radius = 0.0f;
    Vec2 shape_size = {0, 0};  // for Rect shape

    // Duration
    float duration = -1.0f;    // -1 = infinite, >0 = auto-stop after seconds
    float elapsed = 0;

    std::vector<Particle> particles;

    void emit_one(std::mt19937& rng) {
        if ((int)particles.size() >= max_particles) {
            // Recycle oldest dead particle
            for (auto& p : particles) {
                if (!p.alive) { init_particle(p, rng); return; }
            }
            return; // all alive, skip
        }
        Particle p;
        init_particle(p, rng);
        particles.push_back(p);
    }

    void init_particle(Particle& p, std::mt19937& rng) {
        std::uniform_real_distribution<float> d01(0.0f, 1.0f);
        float t = d01(rng);

        // Position (based on shape)
        p.pos = pos;
        if (shape == EmitShape::Circle && shape_radius > 0) {
            float a = d01(rng) * 6.2832f;
            float r = d01(rng) * shape_radius;
            p.pos.x += std::cos(a) * r;
            p.pos.y += std::sin(a) * r;
        } else if (shape == EmitShape::Rect) {
            p.pos.x += (d01(rng) - 0.5f) * shape_size.x;
            p.pos.y += (d01(rng) - 0.5f) * shape_size.y;
        } else if (shape == EmitShape::Line) {
            float t = d01(rng);
            p.pos.x += (t - 0.5f) * shape_size.x;
        }

        // Velocity (within spread angle from base angle)
        float half_spread = spread_angle * 0.5f * 3.14159f / 180.0f;
        float base_rad = base_angle * 3.14159f / 180.0f;
        float angle = base_rad + (d01(rng) - 0.5f) * 2.0f * half_spread;
        float speed_x = vel_min.x + (vel_max.x - vel_min.x) * d01(rng);
        float speed_y = vel_min.y + (vel_max.y - vel_min.y) * d01(rng);
        float speed = std::sqrt(speed_x * speed_x + speed_y * speed_y);
        p.vel = {std::cos(angle) * speed, std::sin(angle) * speed};

        // Properties
        p.color = color_start;
        p.color_end = color_end;
        p.size = size_min + (size_max - size_min) * d01(rng);
        p.size_end = size_end_min + (size_end_max - size_end_min) * d01(rng);
        p.max_life = life_min + (life_max - life_min) * d01(rng);
        p.life = 0;
        p.rotation = d01(rng) * 6.2832f;
        p.rot_speed = (d01(rng) - 0.5f) * 4.0f;
        p.alive = true;
    }

    void update(float dt, std::mt19937& rng) {
        if (!active) return;

        elapsed += dt;
        if (duration > 0 && elapsed >= duration) {
            active = false;
            return;
        }

        // Burst mode
        if (burst_count > 0) {
            for (int i = 0; i < burst_count; i++) emit_one(rng);
            burst_count = 0;
            active = false; // one-shot
            // Keep particles alive to finish their animation
        }

        // Continuous emission
        if (rate > 0 && burst_count == 0) {
            emit_timer += dt;
            float interval = 1.0f / rate;
            while (emit_timer >= interval) {
                emit_timer -= interval;
                emit_one(rng);
            }
        }

        // Update particles
        for (auto& p : particles) {
            if (!p.alive) continue;
            p.life += dt;
            if (p.life >= p.max_life) { p.alive = false; continue; }

            p.vel.y += gravity * dt;
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;
            p.rotation += p.rot_speed * dt;
        }
    }

    bool all_dead() const {
        if (active) return false;
        for (auto& p : particles) if (p.alive) return false;
        return true;
    }
};

// ─── Preset factory ───

inline ParticleEmitter make_preset(const std::string& name, float x, float y) {
    ParticleEmitter e;
    e.pos = {x, y};
    e.id = name;

    if (name == "fire") {
        e.rate = 30; e.max_particles = 60;
        e.vel_min = {-8, -40}; e.vel_max = {8, -15};
        e.color_start = {1.0f, 0.6f, 0.1f, 1.0f};
        e.color_end = {1.0f, 0.2f, 0.0f, 0.0f};
        e.size_min = 3; e.size_max = 8; e.size_end_max = 1;
        e.life_min = 0.3f; e.life_max = 0.8f;
        e.shape = EmitShape::Circle; e.shape_radius = 8;
    } else if (name == "smoke") {
        e.rate = 12; e.max_particles = 40;
        e.vel_min = {-5, -20}; e.vel_max = {5, -8};
        e.color_start = {0.4f, 0.4f, 0.4f, 0.6f};
        e.color_end = {0.3f, 0.3f, 0.3f, 0.0f};
        e.size_min = 6; e.size_max = 14; e.size_end_min = 12; e.size_end_max = 20;
        e.life_min = 0.8f; e.life_max = 2.0f;
    } else if (name == "sparkle") {
        e.rate = 20; e.max_particles = 50;
        e.vel_min = {-30, -30}; e.vel_max = {30, 30};
        e.color_start = {1.0f, 1.0f, 0.5f, 1.0f};
        e.color_end = {1.0f, 1.0f, 1.0f, 0.0f};
        e.size_min = 1; e.size_max = 4;
        e.life_min = 0.2f; e.life_max = 0.6f;
        e.shape = EmitShape::Circle; e.shape_radius = 16;
    } else if (name == "blood") {
        e.burst_count = 15; e.max_particles = 15;
        e.vel_min = {-60, -80}; e.vel_max = {60, -20};
        e.color_start = {0.8f, 0.1f, 0.1f, 1.0f};
        e.color_end = {0.5f, 0.0f, 0.0f, 0.0f};
        e.size_min = 2; e.size_max = 5;
        e.life_min = 0.3f; e.life_max = 0.7f;
        e.gravity = 200;
    } else if (name == "dust") {
        e.burst_count = 8; e.max_particles = 8;
        e.vel_min = {-20, -10}; e.vel_max = {20, -5};
        e.color_start = {0.7f, 0.6f, 0.4f, 0.5f};
        e.color_end = {0.7f, 0.6f, 0.4f, 0.0f};
        e.size_min = 3; e.size_max = 7;
        e.life_min = 0.3f; e.life_max = 0.6f;
    } else if (name == "magic") {
        e.rate = 25; e.max_particles = 50;
        e.vel_min = {-25, -25}; e.vel_max = {25, 25};
        e.color_start = {0.4f, 0.6f, 1.0f, 1.0f};
        e.color_end = {0.8f, 0.4f, 1.0f, 0.0f};
        e.size_min = 2; e.size_max = 5;
        e.life_min = 0.3f; e.life_max = 0.8f;
        e.shape = EmitShape::Circle; e.shape_radius = 20;
    } else if (name == "explosion") {
        e.burst_count = 40; e.max_particles = 40;
        e.vel_min = {-100, -100}; e.vel_max = {100, 100};
        e.color_start = {1.0f, 0.8f, 0.2f, 1.0f};
        e.color_end = {0.8f, 0.2f, 0.0f, 0.0f};
        e.size_min = 4; e.size_max = 12; e.size_end_min = 1; e.size_end_max = 2;
        e.life_min = 0.2f; e.life_max = 0.6f;
        e.gravity = 80;
    } else if (name == "rain_splash") {
        e.burst_count = 5; e.max_particles = 5;
        e.vel_min = {-15, -20}; e.vel_max = {15, -5};
        e.color_start = {0.6f, 0.7f, 0.9f, 0.6f};
        e.color_end = {0.6f, 0.7f, 0.9f, 0.0f};
        e.size_min = 1; e.size_max = 3;
        e.life_min = 0.1f; e.life_max = 0.3f;
    } else if (name == "heal") {
        e.rate = 15; e.max_particles = 30; e.duration = 1.0f;
        e.vel_min = {-10, -40}; e.vel_max = {10, -20};
        e.color_start = {0.2f, 1.0f, 0.4f, 1.0f};
        e.color_end = {0.4f, 1.0f, 0.6f, 0.0f};
        e.size_min = 2; e.size_max = 5;
        e.life_min = 0.4f; e.life_max = 0.8f;
    }

    return e;
}

} // namespace eb
