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
#include "engine/resource/game_manifest.h"

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
    eb::GameManifest manifest;
    float virtual_w = 1040.0f, virtual_h = 480.0f;
    GameState game;
    std::unique_ptr<eb::ScriptEngine> script_engine;
    std::unique_ptr<eb::AudioEngine> audio;
    bool has_focus = false;
    bool has_window = false;
    bool running = true;

    // Mobile editor overlay
    bool editor_active = false;
    float editor_btn_x = 0, editor_btn_y = 0;
    float editor_btn_w = 80, editor_btn_h = 40;
};

static bool init_all(AppState& state) {
    if (state.game.initialized) return true;
    if (!state.renderer || !state.resources) return false;

    LOGI("Initializing game...");

    try {
        // Load manifest
        if (!eb::load_game_manifest(state.manifest, "game.json")) {
            LOGI("No game.json found, using defaults");
            state.manifest.title = "Twilight Engine";
        }

        // Text renderer
        std::string font_path = state.manifest.default_font.empty()
            ? "assets/fonts/default.ttf" : state.manifest.default_font;
        state.text_renderer = std::make_unique<eb::TextRenderer>(
            state.renderer->vulkan_context(), font_path, 7.0f);
        state.text_renderer->set_letter_spacing(6.0f);
        state.game.font_desc = state.renderer->get_texture_descriptor(
            *state.text_renderer->texture());
        state.game.white_desc = state.renderer->default_texture_descriptor();

        // Virtual resolution
        float native_w = static_cast<float>(state.platform->get_width());
        float native_h = static_cast<float>(state.platform->get_height());
        float aspect = native_w / native_h;
        if (native_w >= native_h) {
            state.virtual_h = 480.0f;
            state.virtual_w = 480.0f * aspect;
        } else {
            state.virtual_w = 480.0f;
            state.virtual_h = 480.0f / aspect;
        }

        // Initialize from manifest or fallback
        if (state.manifest.loaded) {
            if (!init_game_from_manifest(state.game, *state.renderer, *state.resources,
                                          state.virtual_w, state.virtual_h, state.manifest)) {
                LOGE("init_game_from_manifest failed");
                return false;
            }
        } else {
            if (!init_game(state.game, *state.renderer, *state.resources,
                           state.virtual_w, state.virtual_h)) {
                LOGE("init_game failed");
                return false;
            }
        }

        // SageLang scripting
        state.script_engine = std::make_unique<eb::ScriptEngine>();
        state.script_engine->set_game_state(&state.game);

        // Load scripts from manifest
        for (const auto& script_path : state.manifest.scripts) {
            state.script_engine->load_file(script_path);
        }
        state.game.script_engine = state.script_engine.get();

        // Run init functions from manifest
        for (const auto& init_func : state.manifest.init_scripts) {
            if (state.script_engine->has_function(init_func)) {
                state.script_engine->call_function(init_func);
            }
        }

        // Audio engine
        state.audio = std::make_unique<eb::AudioEngine>();
        if (state.audio->is_initialized() && !state.manifest.audio.overworld.empty()) {
            state.audio->set_music_volume(0.5f);
            state.audio->play_music(state.manifest.audio.overworld, true);
        }

        // Position editor toggle button (top-right corner)
        state.editor_btn_w = 80;
        state.editor_btn_h = 40;
        state.editor_btn_x = native_w - state.editor_btn_w - 16;
        state.editor_btn_y = 16;

        LOGI("Game initialized (virtual %.0fx%.0f, native %.0fx%.0f)",
             state.virtual_w, state.virtual_h, native_w, native_h);
        return true;

    } catch (const std::exception& e) {
        LOGE("Game init failed: %s", e.what());
        return false;
    }
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    auto* state = static_cast<AppState*>(app->userData);
    if (!state || !state->platform) return 0;

    // Check for editor toggle button tap (before passing to platform)
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            float tx = AMotionEvent_getX(event, 0);
            float ty = AMotionEvent_getY(event, 0);
            // Check editor toggle button
            if (tx >= state->editor_btn_x && tx <= state->editor_btn_x + state->editor_btn_w &&
                ty >= state->editor_btn_y && ty <= state->editor_btn_y + state->editor_btn_h) {
                state->editor_active = !state->editor_active;
                LOGI("Editor %s", state->editor_active ? "ON" : "OFF");
                return 1;
            }
        }
    }

    return state->platform->handle_input(event);
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
            state->game.ui_atlas.reset();
            state->game.icons_atlas.reset();
            state->game.npc_atlases.clear();
            state->game.npc_descs.clear();
            state->text_renderer.reset();
            state->script_engine.reset();
            state->game.script_engine = nullptr;
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
                    float nw = static_cast<float>(w);
                    float nh = static_cast<float>(h);
                    state->virtual_h = 480.0f;
                    state->virtual_w = 480.0f * (nw / nh);
                    state->game.camera.set_viewport(state->virtual_w, state->virtual_h);
                    // Reposition editor button
                    state->editor_btn_x = nw - state->editor_btn_w - 16;
                }
            }
            break;
    }
}

// Render the mobile editor overlay (simple touch-friendly panels)
static void render_editor_overlay(AppState& state, eb::SpriteBatch& batch,
                                   eb::TextRenderer& text, float sw, float sh) {
    auto& game = state.game;

    // Semi-transparent background for editor panel area
    batch.set_texture(game.white_desc);

    // ── Top toolbar ──
    float bar_h = 48;
    batch.draw_quad({0, 0}, {sw, bar_h}, {0,0}, {1,1}, {0.1f, 0.1f, 0.2f, 0.85f});

    // Tool buttons
    const char* tools[] = {"Paint", "Erase", "Fill", "Coll."};
    float btn_w = 70, btn_h = 34, btn_y = 7;
    for (int i = 0; i < 4; i++) {
        float bx = 8 + i * (btn_w + 6);
        eb::Vec4 col = {0.3f, 0.3f, 0.5f, 0.9f};
        batch.draw_quad({bx, btn_y}, {btn_w, btn_h}, {0,0}, {1,1}, col);
        auto tsz = text.measure_text(tools[i], 0.6f);
        text.draw_text(batch, game.font_desc, tools[i],
                       {bx + (btn_w - tsz.x) * 0.5f, btn_y + 8}, {1,1,1,1}, 0.6f);
    }

    // Layer indicator
    text.draw_text(batch, game.font_desc, "Layer: 0",
                   {sw - 200, btn_y + 8}, {0.8f, 0.8f, 1.0f, 1.0f}, 0.6f);

    // ── Bottom info bar ──
    float info_h = 36;
    float info_y = sh - info_h;
    batch.draw_quad({0, info_y}, {sw, info_h}, {0,0}, {1,1}, {0.1f, 0.1f, 0.2f, 0.85f});

    // Camera position and zoom info
    char info[128];
    std::snprintf(info, sizeof(info), "Cam: %.0f,%.0f  Zoom: %.1fx  Tile: %dx%d",
                  game.camera.position().x, game.camera.position().y,
                  1.0f, game.tile_map.width(), game.tile_map.height());
    text.draw_text(batch, game.font_desc, info,
                   {10, info_y + 10}, {0.7f, 0.7f, 0.7f, 1.0f}, 0.5f);
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
                // Audio crossfade
                if (state.audio && state.audio->is_initialized()) {
                    state.audio->update(dt);
                    if (state.game.battle.phase != BattlePhase::None &&
                        !state.manifest.audio.battle.empty())
                        state.audio->crossfade_music(state.manifest.audio.battle, 0.5f, true);
                    else if (!state.manifest.audio.overworld.empty())
                        state.audio->crossfade_music(state.manifest.audio.overworld, 1.0f, true);
                }

                // Game update (skip if editor is consuming input)
                if (!state.editor_active) {
                    update_game(state.game, state.platform->input(), dt);
                }

                if (state.renderer->begin_frame()) {
                    auto& batch = state.renderer->sprite_batch();

                    float vw = state.virtual_w;
                    float vh = state.virtual_h;
                    eb::Mat4 virtual_proj = glm::ortho(0.0f, vw, 0.0f, vh, -1.0f, 1.0f);

                    // Battle screen
                    if (state.game.battle.phase != BattlePhase::None) {
                        batch.set_projection(virtual_proj);
                        render_battle(state.game, batch, *state.text_renderer, vw, vh);
                    } else {
                        // World rendering
                        render_game_world(state.game, batch, *state.text_renderer);

                        // UI overlay
                        render_game_ui(state.game, batch, *state.text_renderer,
                                       virtual_proj, vw, vh);
                    }

                    // Editor overlay (native resolution)
                    float nw = static_cast<float>(state.platform->get_width());
                    float nh = static_cast<float>(state.platform->get_height());
                    eb::Mat4 native_proj = glm::ortho(0.0f, nw, 0.0f, nh, -1.0f, 1.0f);
                    batch.set_projection(native_proj);

                    if (state.editor_active) {
                        render_editor_overlay(state, batch, *state.text_renderer, nw, nh);
                    }

                    // Editor toggle button (always visible)
                    batch.set_texture(state.renderer->default_texture_descriptor());
                    eb::Vec4 btn_col = state.editor_active
                        ? eb::Vec4{0.2f, 0.6f, 0.3f, 0.85f}
                        : eb::Vec4{0.3f, 0.3f, 0.5f, 0.7f};
                    batch.draw_quad({state.editor_btn_x, state.editor_btn_y},
                                   {state.editor_btn_w, state.editor_btn_h},
                                   {0,0}, {1,1}, btn_col);
                    const char* edt_label = state.editor_active ? "PLAY" : "EDIT";
                    auto esz = state.text_renderer->measure_text(edt_label, 0.6f);
                    state.text_renderer->draw_text(batch, state.game.font_desc, edt_label,
                        {state.editor_btn_x + (state.editor_btn_w - esz.x) * 0.5f,
                         state.editor_btn_y + (state.editor_btn_h - esz.y) * 0.5f},
                        {1,1,1,1}, 0.6f);

                    // Touch controls (native resolution, only when not in editor)
                    if (!state.editor_active) {
                        batch.set_texture(state.renderer->default_texture_descriptor());
                        state.platform->touch_controls().render(batch,
                            state.platform->get_width(), state.platform->get_height());
                    }

                    state.renderer->end_frame();
                }
            }
        }
    }

    state.script_engine.reset();
    state.game.script_engine = nullptr;
    state.text_renderer.reset();
    state.resources.reset();
    state.renderer.reset();
    state.platform.reset();

    LOGI("android_main exiting");
}
