#pragma once
#include <string>
#include <vector>
#include <functional>

namespace eb {

struct CoroutineStep {
    std::string function;  // SageLang function to call
    float delay;           // Seconds to wait AFTER calling this step before next
};

struct Coroutine {
    std::string id;
    std::vector<CoroutineStep> steps;
    int current_step = 0;
    float timer = 0;
    bool running = false;
    bool waiting = false;   // Currently waiting between steps
    bool loop = false;
    bool finished = false;
};

class CoroutineManager {
public:
    void add(const std::string& id) {
        for (auto& c : coroutines_) if (c.id == id) return;
        coroutines_.push_back({id, {}, 0, 0, false, false, false, false});
    }

    void add_step(const std::string& id, const std::string& func, float delay) {
        for (auto& c : coroutines_) {
            if (c.id == id) { c.steps.push_back({func, delay}); return; }
        }
    }

    void start(const std::string& id) {
        for (auto& c : coroutines_) {
            if (c.id == id) { c.current_step = 0; c.timer = 0; c.running = true; c.waiting = false; c.finished = false; return; }
        }
    }

    void stop(const std::string& id) {
        for (auto& c : coroutines_) {
            if (c.id == id) { c.running = false; return; }
        }
    }

    void remove(const std::string& id) {
        coroutines_.erase(std::remove_if(coroutines_.begin(), coroutines_.end(),
            [&](auto& c) { return c.id == id; }), coroutines_.end());
    }

    void set_loop(const std::string& id, bool loop) {
        for (auto& c : coroutines_) if (c.id == id) c.loop = loop;
    }

    // Returns list of function names to call this frame
    std::vector<std::string> update(float dt) {
        std::vector<std::string> callbacks;
        for (auto& c : coroutines_) {
            if (!c.running || c.finished || c.steps.empty()) continue;

            if (c.waiting) {
                c.timer -= dt;
                if (c.timer <= 0) {
                    c.waiting = false;
                    c.current_step++;
                    if (c.current_step >= (int)c.steps.size()) {
                        if (c.loop) { c.current_step = 0; }
                        else { c.finished = true; c.running = false; continue; }
                    }
                } else {
                    continue;
                }
            }

            // Execute current step
            auto& step = c.steps[c.current_step];
            if (!step.function.empty()) callbacks.push_back(step.function);
            if (step.delay > 0) {
                c.timer = step.delay;
                c.waiting = true;
            } else {
                // No delay -- advance immediately (but only one step per frame to avoid infinite loops)
                c.current_step++;
                if (c.current_step >= (int)c.steps.size()) {
                    if (c.loop) { c.current_step = 0; }
                    else { c.finished = true; c.running = false; }
                }
            }
        }
        return callbacks;
    }

    void clear() { coroutines_.clear(); }

private:
    std::vector<Coroutine> coroutines_;
};

} // namespace eb
