#include "engine/core/engine.h"
#include "engine/platform/platform.h"
#include "engine/platform/input.h"
#include "engine/graphics/renderer.h"

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
    platform_ = std::make_unique<PlatformDesktop>(config.title, config.width, config.height);
    renderer_ = std::make_unique<Renderer>(*platform_, config.vsync);
#endif
    // On Android, platform and renderer are created in android_main
    running_ = true;
    std::printf("[Engine] Initialized (%dx%d)\n", config.width, config.height);
}

void Engine::shutdown() {
    if (renderer_) {
        renderer_.reset();
    }
    if (platform_) {
        platform_.reset();
    }
    std::printf("[Engine] Shutdown complete\n");
}

void Engine::run() {
    timer_.tick(); // Reset timer

    while (running_) {
        dt_ = timer_.tick();
        platform_->poll_events();

        if (platform_->should_close()) {
            running_ = false;
            break;
        }

        update(dt_);
        render();
    }
}

void Engine::quit() {
    running_ = false;
}

void Engine::update(float dt) {
    // Check for menu/ESC to quit
    if (platform_->input().is_pressed(InputAction::Menu)) {
        quit();
    }
    (void)dt;
}

void Engine::render() {
    if (renderer_->begin_frame()) {
        auto& batch = renderer_->sprite_batch();

        // Demo: draw a few colored quads to prove the engine works
        batch.draw_quad({100.0f, 100.0f}, {200.0f, 150.0f}, {0.8f, 0.2f, 0.3f, 1.0f});
        batch.draw_quad({350.0f, 200.0f}, {180.0f, 180.0f}, {0.2f, 0.6f, 0.9f, 1.0f});
        batch.draw_quad({600.0f, 150.0f}, {150.0f, 200.0f}, {0.3f, 0.8f, 0.4f, 1.0f});

        renderer_->end_frame();
    }
}

} // namespace eb
