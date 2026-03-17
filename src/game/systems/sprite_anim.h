#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace eb {

struct AnimFrame {
    int atlas_index;    // Index into sprite atlas
    float duration;     // Seconds this frame displays
    std::string event;  // Callback function name (empty = no event on this frame)
};

struct Animation {
    std::string name;
    std::vector<AnimFrame> frames;
    bool loop = true;
};

struct AnimPlayer {
    std::string current_anim;
    int current_frame = 0;
    float timer = 0;
    bool playing = false;
    bool finished = false;
    std::unordered_map<std::string, Animation> animations;
    std::string last_event;  // Set during update when a frame with an event is entered; cleared each frame

    void define(const std::string& name, const std::vector<AnimFrame>& frames, bool loop = true) {
        animations[name] = {name, frames, loop};
    }

    void play(const std::string& name) {
        if (animations.find(name) == animations.end()) return;
        if (current_anim == name && playing && !finished) return; // already playing
        current_anim = name;
        current_frame = 0;
        timer = 0;
        playing = true;
        finished = false;
    }

    void stop() {
        playing = false;
    }

    int update(float dt) {
        last_event.clear();
        if (!playing || finished) return frame_index();

        timer += dt;
        auto it = animations.find(current_anim);
        if (it == animations.end() || it->second.frames.empty()) return 0;

        auto& anim = it->second;
        while (timer >= anim.frames[current_frame].duration) {
            timer -= anim.frames[current_frame].duration;
            current_frame++;
            if (current_frame >= (int)anim.frames.size()) {
                if (anim.loop) {
                    current_frame = 0;
                } else {
                    current_frame = (int)anim.frames.size() - 1;
                    finished = true;
                    playing = false;
                    break;
                }
            }
            // Check for frame event on the new frame
            if (!anim.frames[current_frame].event.empty()) {
                last_event = anim.frames[current_frame].event;
            }
        }
        return frame_index();
    }

    int frame_index() const {
        auto it = animations.find(current_anim);
        if (it == animations.end() || it->second.frames.empty()) return 0;
        int idx = std::min(current_frame, (int)it->second.frames.size() - 1);
        return it->second.frames[idx].atlas_index;
    }

    bool is_playing() const { return playing && !finished; }

    void add_event(const std::string& anim, int frame, const std::string& cb) {
        auto it = animations.find(anim);
        if (it == animations.end()) return;
        if (frame >= 0 && frame < (int)it->second.frames.size()) {
            it->second.frames[frame].event = cb;
        }
    }
};

// ─── Screen Transitions ───

enum class TransitionType { None, Fade, Iris, Wipe, Pixelate, Slide };

struct ScreenTransition {
    TransitionType type = TransitionType::None;
    float progress = 0;       // 0 = no effect, 1 = full coverage
    float duration = 0.5f;
    float elapsed = 0;
    int direction = 0;        // Wipe: 0=left, 1=right, 2=up, 3=down. Slide: same
    bool out = false;         // true = transition out (reveal), false = transition in (cover)
    bool active = false;
    std::string on_complete;  // callback when done

    void start(TransitionType t, float dur, bool is_out, int dir = 0, const std::string& cb = "") {
        type = t;
        duration = dur;
        elapsed = 0;
        direction = dir;
        out = is_out;
        active = true;
        on_complete = cb;
        progress = is_out ? 1.0f : 0.0f;
    }

    std::string update(float dt) {
        if (!active) return "";
        elapsed += dt;
        float t = (duration > 0) ? std::min(1.0f, elapsed / duration) : 1.0f;
        progress = out ? (1.0f - t) : t;
        if (elapsed >= duration) {
            active = false;
            std::string cb = on_complete;
            on_complete.clear();
            return cb;
        }
        return "";
    }
};

// ─── Parallax Background Layer ───

struct ParallaxLayer {
    std::string texture_path;
    float scroll_x = 0.5f;   // Scroll multiplier (0.5 = half speed = far background)
    float scroll_y = 0.5f;
    float offset_x = 0;
    float offset_y = 0;
    bool repeat_x = true;
    bool repeat_y = false;
    bool active = true;
    // Runtime (set during rendering)
    void* texture_desc = nullptr; // VkDescriptorSet, set by renderer
    int tex_width = 0, tex_height = 0;
};

} // namespace eb
