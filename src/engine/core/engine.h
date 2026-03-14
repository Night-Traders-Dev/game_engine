#pragma once

#include "engine/core/timer.h"
#include <memory>
#include <string>

namespace eb {

class Platform;
class Renderer;

struct EngineConfig {
    std::string title = "EarthBound Engine";
    int width = 960;
    int height = 720;
    bool vsync = true;
};

class Engine {
public:
    Engine(const EngineConfig& config = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void run();
    void quit();

    Platform& platform() { return *platform_; }
    Renderer& renderer() { return *renderer_; }
    float delta_time() const { return dt_; }

private:
    void init(const EngineConfig& config);
    void shutdown();
    void update(float dt);
    void render();

    std::unique_ptr<Platform> platform_;
    std::unique_ptr<Renderer> renderer_;
    Timer timer_;
    float dt_ = 0.0f;
    bool running_ = false;
};

} // namespace eb
