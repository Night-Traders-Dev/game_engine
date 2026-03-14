#pragma once

#include <chrono>

namespace eb {

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    Timer() : last_time_(Clock::now()) {}

    float tick() {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_time_).count();
        last_time_ = now;
        // Clamp to avoid spiral of death on long frames
        if (dt > 0.1f) dt = 0.1f;
        return dt;
    }

    float elapsed_since_start() const {
        return std::chrono::duration<float>(Clock::now() - start_time_).count();
    }

private:
    TimePoint start_time_ = Clock::now();
    TimePoint last_time_;
};

} // namespace eb
