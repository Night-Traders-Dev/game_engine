#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "engine/core/engine.h"
#include "engine/platform/platform_android.h"
#include "engine/graphics/renderer.h"

#include <memory>

#define LOG_TAG "TWEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct AppState {
    struct android_app* app;
    std::unique_ptr<eb::PlatformAndroid> platform;
    std::unique_ptr<eb::Renderer> renderer;
    eb::Timer timer;
    bool has_focus = false;
    bool has_window = false;
    bool running = true;
};

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    auto* state = static_cast<AppState*>(app->userData);
    if (state && state->platform) {
        return state->platform->handle_input(event);
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    auto* state = static_cast<AppState*>(app->userData);
    if (!state) return;

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window) {
                state->has_window = true;
                if (!state->platform) {
                    state->platform = std::make_unique<eb::PlatformAndroid>(app->window);
                    try {
                        state->renderer = std::make_unique<eb::Renderer>(*state->platform, true);
                        state->renderer->set_shader_dir(""); // Shaders loaded from APK assets
                        state->renderer->set_clear_color(0.05f, 0.05f, 0.12f);
                        LOGI("Renderer initialized");
                    } catch (const std::exception& e) {
                        LOGE("Failed to initialize renderer: %s", e.what());
                        state->running = false;
                    }
                } else {
                    state->platform->set_window(app->window);
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            state->has_window = false;
            if (state->renderer) {
                state->renderer->vulkan_context().wait_idle();
            }
            state->renderer.reset();
            break;

        case APP_CMD_GAINED_FOCUS:
            state->has_focus = true;
            break;

        case APP_CMD_LOST_FOCUS:
            state->has_focus = false;
            break;

        case APP_CMD_DESTROY:
            state->running = false;
            break;

        case APP_CMD_CONFIG_CHANGED:
            if (state->platform && app->window) {
                int w = ANativeWindow_getWidth(app->window);
                int h = ANativeWindow_getHeight(app->window);
                state->platform->handle_resize(w, h);
            }
            break;
    }
}

void android_main(struct android_app* app) {
    LOGI("android_main started");

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    state.timer.tick(); // Reset

    while (state.running) {
        int events;
        struct android_poll_source* source;

        // Process all pending events (ALooper_pollOnce replaces deprecated ALooper_pollAll)
        int timeout = state.has_focus ? 0 : -1;
        int result;
        while ((result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&source))) >= 0) {
            if (source) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                state.running = false;
                break;
            }
            // After processing one event, use 0 timeout to drain remaining events
            timeout = 0;
        }

        if (!state.running) break;

        // Render frame if we have focus and a window
        if (state.has_focus && state.has_window && state.renderer) {
            float dt = state.timer.tick();
            state.platform->poll_events();

            // Check menu/back button
            if (state.platform->input().is_pressed(eb::InputAction::Menu)) {
                ANativeActivity_finish(app->activity);
            }

            if (state.renderer->begin_frame()) {
                auto& batch = state.renderer->sprite_batch();

                // Demo quads
                batch.draw_quad({100.0f, 100.0f}, {200.0f, 150.0f}, {0.8f, 0.2f, 0.3f, 1.0f});
                batch.draw_quad({350.0f, 200.0f}, {180.0f, 180.0f}, {0.2f, 0.6f, 0.9f, 1.0f});
                batch.draw_quad({600.0f, 150.0f}, {150.0f, 200.0f}, {0.3f, 0.8f, 0.4f, 1.0f});

                state.renderer->end_frame();
            }

            (void)dt;
        }
    }

    state.renderer.reset();
    state.platform.reset();

    LOGI("android_main exiting");
}
