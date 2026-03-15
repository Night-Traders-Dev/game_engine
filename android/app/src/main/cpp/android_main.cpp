#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "game/game.h"
#include "engine/core/engine.h"
#include "engine/scripting/script_engine.h"
#include "engine/audio/audio_engine.h"
#include "engine/platform/platform_android.h"
#include "engine/resource/file_io.h"

#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#define LOG_TAG "TWEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct AppState {
    struct android_app* app;
    std::unique_ptr<eb::PlatformAndroid> platform;
    std::unique_ptr<eb::Renderer> renderer;
    std::unique_ptr<eb::ResourceManager> resources;
    std::unique_ptr<eb::TextRenderer> text_renderer;
    eb::Timer timer;
    float virtual_w = 1040.0f, virtual_h = 480.0f;
    GameState game;
    std::unique_ptr<eb::AudioEngine> audio;
    bool has_focus = false;
    bool has_window = false;
    bool running = true;
};

static bool init_all(AppState& state) {
    if (state.game.initialized) return true;
    if (!state.renderer || !state.resources) return false;

    LOGI("Initializing game...");

    try {
        // Text renderer
        state.text_renderer = std::make_unique<eb::TextRenderer>(
            state.renderer->vulkan_context(), "assets/fonts/default.ttf", 7.0f);
        state.text_renderer->set_letter_spacing(6.0f);
        state.game.font_desc = state.renderer->get_texture_descriptor(
            *state.text_renderer->texture());
        state.game.white_desc = state.renderer->default_texture_descriptor();

        // Use virtual resolution for game — native res is too large on high-DPI phones.
        // Use 720 for the shorter dimension, scale the other by aspect ratio.
        float native_w = static_cast<float>(state.platform->get_width());
        float native_h = static_cast<float>(state.platform->get_height());
        float aspect = native_w / native_h;
        float virtual_w, virtual_h;
        // Use 480 virtual height — makes the game world larger on screen
        // and gives the dialogue box proper proportions
        if (native_w >= native_h) {
            virtual_h = 480.0f;
            virtual_w = 480.0f * aspect;
        } else {
            virtual_w = 480.0f;
            virtual_h = 480.0f / aspect;
        }

        state.virtual_w = virtual_w;
        state.virtual_h = virtual_h;

        if (!init_game(state.game, *state.renderer, *state.resources, virtual_w, virtual_h)) {
            LOGE("init_game failed");
            return false;
        }

        // SageLang scripting
        static eb::ScriptEngine script_engine;
        script_engine.set_game_state(&state.game);

        // Battle scripts (modular)
        script_engine.load_file("assets/scripts/battle/battle_core.sage");
        script_engine.load_file("assets/scripts/battle/dean_battle.sage");
        script_engine.load_file("assets/scripts/battle/sam_battle.sage");
        script_engine.load_file("assets/scripts/battle/vampire_battle.sage");
        script_engine.load_file("assets/scripts/battle/demon_battle.sage");

        // Inventory scripts (modular)
        script_engine.load_file("assets/scripts/inventory/inventory_core.sage");
        script_engine.load_file("assets/scripts/inventory/dean_inventory.sage");
        script_engine.load_file("assets/scripts/inventory/sam_inventory.sage");
        script_engine.load_file("assets/scripts/inventory/brothers_inventory.sage");
        script_engine.load_file("assets/scripts/inventory/battle_inventory.sage");

        // Skills & NPC dialogue
        script_engine.load_file("assets/scripts/skills.sage");
        script_engine.load_file("assets/scripts/bobby.sage");
        script_engine.load_file("assets/scripts/vampire.sage");
        state.game.script_engine = &script_engine;

        // Give starter items via SageLang
        if (script_engine.has_function("give_starter_items")) {
            script_engine.call_function("give_starter_items");
        }
        if (script_engine.has_function("restock_food")) {
            script_engine.call_function("restock_food");
        }

        // Initialize H.U.N.T.E.R. skills
        if (script_engine.has_function("init_dean_skills"))
            script_engine.call_function("init_dean_skills");
        if (script_engine.has_function("init_sam_skills"))
            script_engine.call_function("init_sam_skills");

        // Audio engine
        state.audio = std::make_unique<eb::AudioEngine>();
        if (state.audio->is_initialized()) {
            state.audio->set_music_volume(0.5f);
            state.audio->play_music("assets/audio/overworld.wav", true);
        }

        LOGI("Game initialized (virtual %.0fx%.0f, native %.0fx%.0f)",
             virtual_w, virtual_h, native_w, native_h);
        return true;

    } catch (const std::exception& e) {
        LOGE("Game init failed: %s", e.what());
        return false;
    }
}

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
                        state->renderer->set_shader_dir("shaders/");
                        state->renderer->set_clear_color(0.05f, 0.05f, 0.12f);
                        state->resources = std::make_unique<eb::ResourceManager>(
                            state->renderer->vulkan_context());
                        LOGI("Renderer initialized");
                    } catch (const std::exception& e) {
                        LOGE("Failed to init renderer: %s", e.what());
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
            state->game.initialized = false;
            state->game.tileset_atlas.reset();
            state->game.dean_atlas.reset();
            state->game.sam_atlas.reset();
            state->game.npc_atlases.clear();
            state->game.npc_descs.clear();
            state->text_renderer.reset();
            state->resources.reset();
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
                if (state->game.initialized) {
                    float nw = static_cast<float>(state->platform->get_width());
                    float nh = static_cast<float>(state->platform->get_height());
                    state->virtual_h = 480.0f;
                    state->virtual_w = 480.0f * (nw / nh);
                    state->game.camera.set_viewport(state->virtual_w, state->virtual_h);
                }
            }
            break;
    }
}

void android_main(struct android_app* app) {
    LOGI("android_main started");

    eb::FileIO::set_asset_manager(app->activity->assetManager);

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    state.timer.tick();

    while (state.running) {
        int events;
        struct android_poll_source* source;

        int timeout = state.has_focus ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) { state.running = false; break; }
            timeout = 0;
        }

        if (!state.running) break;

        if (state.has_focus && state.has_window && state.renderer) {
            if (!state.game.initialized) {
                init_all(state);
            }

            float dt = state.timer.tick();
            state.platform->poll_events();

            if (state.platform->input().is_pressed(eb::InputAction::Menu)) {
                ANativeActivity_finish(app->activity);
            }

            if (state.game.initialized) {
                // Audio: crossfade between overworld/battle music
                if (state.audio && state.audio->is_initialized()) {
                    state.audio->update(dt);
                    if (state.game.battle.phase != BattlePhase::None)
                        state.audio->crossfade_music("assets/audio/battle.wav", 0.5f, true);
                    else
                        state.audio->crossfade_music("assets/audio/overworld.wav", 1.0f, true);
                }

                // Shared game update
                update_game(state.game, state.platform->input(), dt);

                if (state.renderer->begin_frame()) {
                    auto& batch = state.renderer->sprite_batch();

                    // Virtual resolution projection for game content
                    float vw = state.virtual_w;
                    float vh = state.virtual_h;
                    eb::Mat4 virtual_proj = glm::ortho(0.0f, vw, 0.0f, vh, -1.0f, 1.0f);

                    // Battle screen
                    if (state.game.battle.phase != BattlePhase::None) {
                        batch.set_projection(virtual_proj);
                        render_battle(state.game, batch, *state.text_renderer, vw, vh);
                    } else {
                        // World rendering (camera uses virtual viewport)
                        render_game_world(state.game, batch, *state.text_renderer);

                        // UI overlay (virtual resolution)
                        render_game_ui(state.game, batch, *state.text_renderer,
                                       virtual_proj, vw, vh);
                    }

                    // Touch controls at native resolution (so hit areas match finger positions)
                    batch.set_texture(state.renderer->default_texture_descriptor());
                    state.platform->touch_controls().render(batch,
                        state.platform->get_width(), state.platform->get_height());

                    state.renderer->end_frame();
                }
            }
        }
    }

    state.text_renderer.reset();
    state.resources.reset();
    state.renderer.reset();
    state.platform.reset();

    LOGI("android_main exiting");
}
