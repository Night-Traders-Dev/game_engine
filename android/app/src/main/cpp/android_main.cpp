#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>

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

    // Mobile editor
    bool editor_active = false;
    int editor_panel = 0;  // 0=menu, 1=tools, 2=layers, 3=tiles, 4=NPCs, 5=info
    int editor_tool = 0;   // 0=paint, 1=erase, 2=fill, 3=collision
    int editor_layer = 0;
    int editor_tile = 1;
    int editor_brush = 1;
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

        // Set screen dimensions before scripts run
        state.game.hud.screen_w = state.virtual_w;
        state.game.hud.screen_h = state.virtual_h;
        state.game.hud.native_w = static_cast<float>(state.platform->get_width());
        state.game.hud.native_h = static_cast<float>(state.platform->get_height());

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

        // Detect Meta Quest devices
        {
            char manufacturer[PROP_VALUE_MAX] = {0};
            char model[PROP_VALUE_MAX] = {0};
            __system_property_get("ro.product.manufacturer", manufacturer);
            __system_property_get("ro.product.model", model);
            bool is_quest = (std::strstr(manufacturer, "Oculus") != nullptr ||
                              std::strstr(manufacturer, "Meta") != nullptr ||
                              std::strstr(model, "Quest") != nullptr);
            if (is_quest) {
                state.script_engine->set_bool("IS_QUEST", true);
                state.script_engine->set_string("PLATFORM", "quest");
                LOGI("Meta Quest device detected: %s %s", manufacturer, model);
            }
        }

        // Execute map script if map_init exists
        if (state.script_engine->has_function("map_init")) {
            state.script_engine->call_function("map_init");
            LOGI("Executed map_init()");
        }

        // Audio engine
        state.audio = std::make_unique<eb::AudioEngine>();
        state.game.audio_engine = state.audio.get();
        state.game.renderer = state.renderer.get();
        state.game.resource_manager = state.resources.get();
        if (state.audio->is_initialized() && !state.manifest.audio.overworld.empty()) {
            state.audio->set_music_volume(0.5f);
            state.audio->play_music(state.manifest.audio.overworld, true);
        }

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
                }
            }
            break;
    }
}

// ── Mobile editor: touch-friendly menu system ──

// Helper: draw a large touch button, returns true if tapped this frame
static bool touch_button(eb::SpriteBatch& batch, eb::TextRenderer& text,
                          VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                          const char* label, float x, float y, float w, float h,
                          bool selected, float touch_x, float touch_y, bool touch_down) {
    eb::Vec4 bg = selected ? eb::Vec4{0.25f, 0.45f, 0.65f, 0.92f}
                           : eb::Vec4{0.18f, 0.18f, 0.30f, 0.88f};
    batch.set_texture(white_desc);
    batch.draw_quad({x, y}, {w, h}, {0,0}, {1,1}, bg);
    // Border
    eb::Vec4 bdr = selected ? eb::Vec4{0.4f, 0.7f, 1.0f, 0.9f}
                            : eb::Vec4{0.35f, 0.35f, 0.50f, 0.6f};
    batch.draw_quad({x, y}, {w, 2}, {0,0}, {1,1}, bdr);
    batch.draw_quad({x, y+h-2}, {w, 2}, {0,0}, {1,1}, bdr);
    batch.draw_quad({x, y}, {2, h}, {0,0}, {1,1}, bdr);
    batch.draw_quad({x+w-2, y}, {2, h}, {0,0}, {1,1}, bdr);
    // Label centered
    float scale = 1.0f;
    auto sz = text.measure_text(label, scale);
    // If text too wide, shrink
    if (sz.x > w - 16) { scale = (w - 16) / sz.x; sz = text.measure_text(label, scale); }
    text.draw_text(batch, font_desc, label,
                   {x + (w - sz.x) * 0.5f, y + (h - sz.y) * 0.5f},
                   selected ? eb::Vec4{1,1,0.9f,1} : eb::Vec4{0.85f,0.85f,0.80f,1}, scale);
    // Hit test
    return touch_down && touch_x >= x && touch_x <= x + w &&
           touch_y >= y && touch_y <= y + h;
}

static void render_editor_overlay(AppState& state, eb::SpriteBatch& batch,
                                   eb::TextRenderer& text, float sw, float sh) {
    auto& game = state.game;
    batch.set_texture(game.white_desc);

    // Get touch state for button hit testing
    float tx = 0, ty = 0;
    bool tap = false;
    auto& tc = state.platform->touch_controls();
    // Read primary finger position from input
    auto& input = state.platform->input();
    tx = input.mouse.x;
    ty = input.mouse.y;
    tap = input.mouse.is_pressed(eb::MouseButton::Left);

    // Scale for readability on high-DPI screens
    float dpi_scale = std::max(1.0f, std::min(sw, sh) / 720.0f);
    float btn_w = 240 * dpi_scale;
    float btn_h = 70 * dpi_scale;
    float pad = 12 * dpi_scale;

    // ── Status bar (always shown at top in editor) ──
    float bar_h = 44 * dpi_scale;
    batch.draw_quad({0, 0}, {sw, bar_h}, {0,0}, {1,1}, {0.08f, 0.08f, 0.16f, 0.9f});

    const char* tool_names[] = {"Paint", "Erase", "Fill", "Collision"};
    char status[128];
    std::snprintf(status, sizeof(status), "Tool: %s  |  Layer: %d  |  Tile: %d  |  Brush: %dx%d",
                  tool_names[state.editor_tool], state.editor_layer,
                  state.editor_tile, state.editor_brush, state.editor_brush);
    float status_scale = 0.8f * dpi_scale;
    text.draw_text(batch, game.font_desc, status, {12 * dpi_scale, 12 * dpi_scale},
                   {0.7f, 0.8f, 1.0f, 1.0f}, status_scale);

    if (state.editor_panel == 0) {
        // ═══ MAIN MENU ═══
        // Full-screen dimmed overlay with large menu buttons
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, bar_h}, {sw, sh - bar_h}, {0,0}, {1,1}, {0, 0, 0, 0.55f});

        float menu_w = btn_w * 1.2f;
        float menu_h = btn_h;
        float total_h = 6 * (menu_h + pad) - pad;
        float start_x = (sw - menu_w) * 0.5f;
        float start_y = (sh - total_h) * 0.5f;

        // Title
        const char* title = "EDITOR";
        float title_scale = 1.4f * dpi_scale;
        auto tsz = text.measure_text(title, title_scale);
        text.draw_text(batch, game.font_desc, title,
                       {(sw - tsz.x) * 0.5f, start_y - 50 * dpi_scale},
                       {1.0f, 0.9f, 0.5f, 1.0f}, title_scale);

        struct MenuItem { const char* label; int panel; };
        MenuItem items[] = {
            {"Tools",       1},
            {"Layers",      2},
            {"Tile Select", 3},
            {"Spawn NPC",   4},
            {"Map Info",    5},
            {"Resume Game", -1},
        };

        for (int i = 0; i < 6; i++) {
            float by = start_y + i * (menu_h + pad);
            bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                     items[i].label, start_x, by, menu_w, menu_h,
                                     false, tx, ty, tap);
            if (hit) {
                if (items[i].panel == -1) {
                    state.editor_active = false;
                } else {
                    state.editor_panel = items[i].panel;
                }
            }
        }

    } else {
        // ═══ SUB-PANELS ═══
        // Back button (top-left, always visible in sub-panels)
        float back_w = 100 * dpi_scale, back_h = bar_h - 4;
        bool back_hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                      "< Back", sw - back_w - 8, 2, back_w, back_h,
                                      false, tx, ty, tap);
        if (back_hit) state.editor_panel = 0;

        float panel_y = bar_h + pad;
        float panel_w = std::min(sw * 0.85f, 500.0f * dpi_scale);
        float panel_x = (sw - panel_w) * 0.5f;

        if (state.editor_panel == 1) {
            // ── Tools ──
            const char* tools[] = {"Paint", "Erase", "Fill", "Collision"};
            for (int i = 0; i < 4; i++) {
                float by = panel_y + i * (btn_h + pad);
                bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                         tools[i], panel_x, by, panel_w, btn_h,
                                         state.editor_tool == i, tx, ty, tap);
                if (hit) state.editor_tool = i;
            }
            // Brush size
            float brush_y = panel_y + 4 * (btn_h + pad) + pad;
            text.draw_text(batch, game.font_desc, "Brush Size:",
                           {panel_x, brush_y}, {0.8f, 0.8f, 0.8f, 1}, 0.9f * dpi_scale);
            brush_y += 30 * dpi_scale;
            float bsz_w = (panel_w - 2 * pad) / 3;
            for (int b = 1; b <= 3; b++) {
                char bl[8]; std::snprintf(bl, sizeof(bl), "%dx%d", b, b);
                float bx = panel_x + (b - 1) * (bsz_w + pad);
                bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                         bl, bx, brush_y, bsz_w, btn_h * 0.8f,
                                         state.editor_brush == b, tx, ty, tap);
                if (hit) state.editor_brush = b;
            }

        } else if (state.editor_panel == 2) {
            // ── Layers ──
            int max_layers = game.tile_map.layer_count();
            for (int i = 0; i < max_layers && i < 9; i++) {
                char lbl[32]; std::snprintf(lbl, sizeof(lbl), "Layer %d", i);
                float by = panel_y + i * (btn_h * 0.75f + pad * 0.5f);
                bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                         lbl, panel_x, by, panel_w, btn_h * 0.75f,
                                         state.editor_layer == i, tx, ty, tap);
                if (hit) state.editor_layer = i;
            }

        } else if (state.editor_panel == 3) {
            // ── Tile Select (grid of tile IDs) ──
            text.draw_text(batch, game.font_desc, "Select Tile:",
                           {panel_x, panel_y}, {0.8f, 0.8f, 0.8f, 1}, 0.9f * dpi_scale);
            float grid_y = panel_y + 30 * dpi_scale;
            int cols = 6;
            float cell = (panel_w - (cols - 1) * pad * 0.5f) / cols;
            int max_tile = game.tileset_atlas ? game.tileset_atlas->region_count() : 24;
            if (max_tile > 48) max_tile = 48; // Limit for touch
            for (int t = 0; t < max_tile; t++) {
                int r = t / cols, c = t % cols;
                float cx = panel_x + c * (cell + pad * 0.5f);
                float cy = grid_y + r * (cell + pad * 0.5f);
                char tl[8]; std::snprintf(tl, sizeof(tl), "%d", t + 1);
                bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                         tl, cx, cy, cell, cell,
                                         state.editor_tile == t + 1, tx, ty, tap);
                if (hit) state.editor_tile = t + 1;
            }

        } else if (state.editor_panel == 4) {
            // ── NPC Spawner ──
            struct NPCPreset { const char* label; const char* name; bool hostile; int hp; int atk; };
            NPCPreset presets[] = {
                {"Villager",  "Villager",  false, 0,  0},
                {"Chicken",   "Chicken",   false, 0,  0},
                {"Cow",       "Cow",       false, 0,  0},
                {"Slime",     "Slime",     true, 30,  6},
                {"Skeleton",  "Skeleton",  true, 50, 12},
            };
            for (int i = 0; i < 5; i++) {
                float by = panel_y + i * (btn_h + pad);
                char lbl[64];
                if (presets[i].hostile)
                    std::snprintf(lbl, sizeof(lbl), "%s (HP:%d ATK:%d)", presets[i].label, presets[i].hp, presets[i].atk);
                else
                    std::snprintf(lbl, sizeof(lbl), "%s (friendly)", presets[i].label);
                bool hit = touch_button(batch, text, game.font_desc, game.white_desc,
                                         lbl, panel_x, by, panel_w, btn_h,
                                         false, tx, ty, tap);
                if (hit) {
                    NPC npc;
                    npc.name = presets[i].name;
                    npc.position = game.player_pos;
                    npc.home_pos = npc.position;
                    npc.wander_target = npc.position;
                    npc.hostile = presets[i].hostile;
                    npc.has_battle = presets[i].hostile;
                    npc.battle_enemy_name = presets[i].name;
                    npc.battle_enemy_hp = presets[i].hp;
                    npc.battle_enemy_atk = presets[i].atk;
                    npc.dialogue = {{std::string(presets[i].name), "..."}};
                    game.npcs.push_back(npc);
                    state.editor_panel = 0; // Return to menu
                }
            }

        } else if (state.editor_panel == 5) {
            // ── Map Info ──
            float ty_pos = panel_y;
            float line_h = 32 * dpi_scale;
            float info_scale = 0.9f * dpi_scale;

            char buf[128];
            auto info_line = [&](const char* txt) {
                text.draw_text(batch, game.font_desc, txt,
                               {panel_x, ty_pos}, {0.8f, 0.85f, 0.9f, 1}, info_scale);
                ty_pos += line_h;
            };

            std::snprintf(buf, sizeof(buf), "Map: %d x %d tiles", game.tile_map.width(), game.tile_map.height());
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Tile Size: %d px", game.tile_map.tile_size());
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Layers: %d", game.tile_map.layer_count());
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "NPCs: %d", (int)game.npcs.size());
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Objects: %d", (int)game.world_objects.size());
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Player: %.0f, %.0f", game.player_pos.x, game.player_pos.y);
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Camera: %.0f, %.0f", game.camera.position().x, game.camera.position().y);
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Gold: %d", game.gold);
            info_line(buf);
            std::snprintf(buf, sizeof(buf), "Inventory: %d / %d items", (int)game.inventory.items.size(), game.inventory.MAX_SLOTS);
            info_line(buf);
        }
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

            // Menu button (on-screen hamburger) toggles editor
            if (state.platform->input().is_pressed(eb::InputAction::Menu)) {
                if (state.editor_active && state.editor_panel != 0) {
                    state.editor_panel = 0; // Return to main menu
                } else {
                    state.editor_active = !state.editor_active;
                    state.editor_panel = 0;
                }
                LOGI("Editor %s (panel %d)", state.editor_active ? "ON" : "OFF", state.editor_panel);
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

                    // Touch controls always rendered — menu button doubles as editor toggle
                    // Color the menu button green when editor is active
                    batch.set_texture(state.renderer->default_texture_descriptor());
                    state.platform->touch_controls().render(batch,
                        state.platform->get_width(), state.platform->get_height());

                    // Draw editor indicator on the menu button when active
                    if (state.editor_active) {
                        auto& tc = state.platform->touch_controls();
                        // Small green dot near the menu button area (top-right)
                        float ind_x = nw - 12, ind_y = 8;
                        batch.draw_quad({ind_x, ind_y}, {8, 8}, {0,0}, {1,1},
                                       {0.2f, 0.9f, 0.3f, 0.9f});
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
