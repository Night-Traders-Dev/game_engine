#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>

namespace eb {

// ─── Easing Functions ───

enum class EaseType {
    Linear, SineIn, SineOut, SineInOut,
    QuadIn, QuadOut, QuadInOut,
    CubicIn, CubicOut, CubicInOut,
    BackIn, BackOut, BackInOut,
    BounceIn, BounceOut, BounceInOut,
    ElasticIn, ElasticOut, ElasticInOut,
};

inline float ease(EaseType type, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    constexpr float PI = 3.14159265f;
    switch (type) {
        case EaseType::Linear: return t;
        case EaseType::SineIn: return 1.0f - std::cos(t * PI * 0.5f);
        case EaseType::SineOut: return std::sin(t * PI * 0.5f);
        case EaseType::SineInOut: return -(std::cos(PI * t) - 1.0f) * 0.5f;
        case EaseType::QuadIn: return t * t;
        case EaseType::QuadOut: return 1.0f - (1.0f - t) * (1.0f - t);
        case EaseType::QuadInOut: return t < 0.5f ? 2*t*t : 1 - std::pow(-2*t + 2, 2) / 2;
        case EaseType::CubicIn: return t * t * t;
        case EaseType::CubicOut: return 1.0f - std::pow(1.0f - t, 3);
        case EaseType::CubicInOut: return t < 0.5f ? 4*t*t*t : 1 - std::pow(-2*t + 2, 3) / 2;
        case EaseType::BackIn: { float c = 1.70158f; return (c+1)*t*t*t - c*t*t; }
        case EaseType::BackOut: { float c = 1.70158f; float t1 = t-1; return 1 + (c+1)*t1*t1*t1 + c*t1*t1; }
        case EaseType::BackInOut: {
            float c = 1.70158f * 1.525f;
            return t < 0.5f
                ? (std::pow(2*t, 2) * ((c+1)*2*t - c)) / 2
                : (std::pow(2*t-2, 2) * ((c+1)*(t*2-2) + c) + 2) / 2;
        }
        case EaseType::BounceOut: {
            if (t < 1/2.75f) return 7.5625f*t*t;
            else if (t < 2/2.75f) { t -= 1.5f/2.75f; return 7.5625f*t*t + 0.75f; }
            else if (t < 2.5f/2.75f) { t -= 2.25f/2.75f; return 7.5625f*t*t + 0.9375f; }
            else { t -= 2.625f/2.75f; return 7.5625f*t*t + 0.984375f; }
        }
        case EaseType::BounceIn: return 1.0f - ease(EaseType::BounceOut, 1.0f - t);
        case EaseType::BounceInOut:
            return t < 0.5f
                ? (1.0f - ease(EaseType::BounceOut, 1.0f - 2*t)) * 0.5f
                : (1.0f + ease(EaseType::BounceOut, 2*t - 1.0f)) * 0.5f;
        case EaseType::ElasticIn: {
            if (t == 0 || t == 1) return t;
            return -std::pow(2, 10*t - 10) * std::sin((t*10 - 10.75f) * (2*PI / 3));
        }
        case EaseType::ElasticOut: {
            if (t == 0 || t == 1) return t;
            return std::pow(2, -10*t) * std::sin((t*10 - 0.75f) * (2*PI / 3)) + 1;
        }
        case EaseType::ElasticInOut: {
            if (t == 0 || t == 1) return t;
            return t < 0.5f
                ? -(std::pow(2, 20*t - 10) * std::sin((20*t - 11.125f) * (2*PI / 4.5f))) / 2
                : (std::pow(2, -20*t + 10) * std::sin((20*t - 11.125f) * (2*PI / 4.5f))) / 2 + 1;
        }
    }
    return t;
}

inline EaseType ease_from_string(const std::string& name) {
    if (name == "linear") return EaseType::Linear;
    if (name == "sine_in") return EaseType::SineIn;
    if (name == "sine_out") return EaseType::SineOut;
    if (name == "sine_in_out" || name == "sine") return EaseType::SineInOut;
    if (name == "quad_in") return EaseType::QuadIn;
    if (name == "quad_out") return EaseType::QuadOut;
    if (name == "quad_in_out" || name == "quad") return EaseType::QuadInOut;
    if (name == "cubic_in") return EaseType::CubicIn;
    if (name == "cubic_out") return EaseType::CubicOut;
    if (name == "cubic_in_out" || name == "cubic") return EaseType::CubicInOut;
    if (name == "back_in") return EaseType::BackIn;
    if (name == "back_out") return EaseType::BackOut;
    if (name == "back_in_out" || name == "back") return EaseType::BackInOut;
    if (name == "bounce_in") return EaseType::BounceIn;
    if (name == "bounce_out" || name == "bounce") return EaseType::BounceOut;
    if (name == "bounce_in_out") return EaseType::BounceInOut;
    if (name == "elastic_in") return EaseType::ElasticIn;
    if (name == "elastic_out" || name == "elastic") return EaseType::ElasticOut;
    if (name == "elastic_in_out") return EaseType::ElasticInOut;
    if (name == "ease_in") return EaseType::QuadIn;
    if (name == "ease_out") return EaseType::QuadOut;
    if (name == "ease_in_out") return EaseType::QuadInOut;
    return EaseType::Linear;
}

// ─── Tween ───

struct Tween {
    int id = -1;
    std::string target;       // UI element ID, "camera", "player", NPC name
    std::string property;     // "x", "y", "alpha", "scale", "rotation", "zoom"
    float start_val = 0;
    float end_val = 0;
    float duration = 1.0f;
    float elapsed = 0;
    EaseType ease_type = EaseType::Linear;
    bool active = true;
    bool loop = false;
    bool yoyo = false;
    bool started = false;     // start_val captured on first update
    std::string on_complete;  // SageLang callback function name

    float progress() const { return duration > 0 ? std::min(1.0f, elapsed / duration) : 1.0f; }
    float value() const {
        float t = ease(ease_type, progress());
        return start_val + (end_val - start_val) * t;
    }
    bool finished() const { return elapsed >= duration && !loop; }
};

// Delayed callback (tween_delay)
struct TweenDelay {
    float duration = 0;
    float elapsed = 0;
    std::string callback;
    bool active = true;
    bool finished() const { return elapsed >= duration; }
};

struct TweenSystem {
    std::vector<Tween> tweens;
    std::vector<TweenDelay> delays;
    int next_id = 1;

    int add(const std::string& target, const std::string& prop,
            float end, float duration, EaseType ease_type,
            const std::string& on_complete = "") {
        Tween tw;
        tw.id = next_id++;
        tw.target = target;
        tw.property = prop;
        tw.end_val = end;
        tw.duration = duration;
        tw.ease_type = ease_type;
        tw.on_complete = on_complete;
        tweens.push_back(tw);
        return tw.id;
    }

    void add_delay(float duration, const std::string& callback) {
        delays.push_back({duration, 0, callback, true});
    }

    void stop(int id) {
        for (auto& tw : tweens) {
            if (tw.id == id) { tw.active = false; break; }
        }
    }

    void stop_all(const std::string& target) {
        for (auto& tw : tweens) {
            if (tw.target == target) tw.active = false;
        }
    }

    // Returns list of callbacks to invoke (completed tweens/delays)
    std::vector<std::string> update(float dt) {
        std::vector<std::string> callbacks;

        // Update tweens
        for (auto& tw : tweens) {
            if (!tw.active) continue;
            tw.elapsed += dt;
            if (tw.finished()) {
                tw.active = false;
                if (!tw.on_complete.empty()) callbacks.push_back(tw.on_complete);
                if (tw.yoyo) {
                    // Reverse direction
                    std::swap(tw.start_val, tw.end_val);
                    tw.elapsed = 0;
                    tw.active = true;
                } else if (tw.loop) {
                    tw.elapsed = 0;
                }
            }
        }

        // Update delays
        for (auto& d : delays) {
            if (!d.active) continue;
            d.elapsed += dt;
            if (d.finished()) {
                d.active = false;
                if (!d.callback.empty()) callbacks.push_back(d.callback);
            }
        }

        // Cleanup finished
        tweens.erase(std::remove_if(tweens.begin(), tweens.end(),
            [](const Tween& tw) { return !tw.active; }), tweens.end());
        delays.erase(std::remove_if(delays.begin(), delays.end(),
            [](const TweenDelay& d) { return !d.active; }), delays.end());

        return callbacks;
    }

    void clear() { tweens.clear(); delays.clear(); }
};

} // namespace eb
