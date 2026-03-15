#include "engine/core/engine.h"
#include "engine/platform/platform.h"
#include "engine/platform/input.h"
#include "engine/graphics/renderer.h"
#include "engine/resource/resource_manager.h"

#ifndef __ANDROID__
#include "engine/platform/platform_desktop.h"
#endif

#include <stdexcept>
#include <cstdio>

namespace eb {

Engine::Engine(const EngineConfig& config) {
    init(config);
}

Engine::~Engine() {
    shutdown();
}

void Engine::init(const EngineConfig& config) {
#ifndef __ANDROID__
    platform_ = std::make_unique<PlatformDesktop>(config.title, config.width, config.height, config.fullscreen);
    renderer_ = std::make_unique<Renderer>(*platform_, config.vsync);
    resources_ = std::make_unique<ResourceManager>(renderer_->vulkan_context());
#endif
    running_ = true;
    std::printf("[Engine] Initialized (%dx%d)\n", platform_->get_width(), platform_->get_height());
}

void Engine::shutdown() {
    // Clear callbacks before destroying subsystems
    on_update = nullptr;
    on_render = nullptr;

    if (resources_) resources_.reset();
    if (renderer_) renderer_.reset();
    if (platform_) platform_.reset();
    std::printf("[Engine] Shutdown complete\n");
}

void Engine::run() {
    timer_.tick();

    while (running_) {
        dt_ = timer_.tick();
        platform_->poll_events();

        if (platform_->should_close()) {
            running_ = false;
            break;
        }

        // ESC to quit
        if (platform_->input().is_pressed(InputAction::Menu)) {
            quit();
            break;
        }

        // Update
        if (on_update) on_update(dt_);

        // Render
        if (renderer_->begin_frame()) {
            if (on_render) on_render();
            renderer_->end_frame();
        }
    }
}

void Engine::quit() {
    running_ = false;
}

} // namespace eb
