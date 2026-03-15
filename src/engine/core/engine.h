#pragma once

#include "engine/core/timer.h"
#include <memory>
#include <string>
#include <functional>

namespace eb {

class Platform;
class Renderer;
class ResourceManager;

struct EngineConfig {
    std::string title = "Twilight Engine";
    int width = 960;
    int height = 720;
    bool vsync = true;
    bool fullscreen = false;
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
    ResourceManager& resources() { return *resources_; }
    float delta_time() const { return dt_; }

    // Application callbacks
    std::function<void(float dt)> on_update;
    std::function<void()> on_render;

private:
    void init(const EngineConfig& config);
    void shutdown();

    std::unique_ptr<Platform> platform_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ResourceManager> resources_;
    Timer timer_;
    float dt_ = 0.0f;
    bool running_ = false;
};

} // namespace eb
