#include "game/game.h"
#include "engine/core/engine.h"
#include "engine/scripting/script_engine.h"
#include "engine/audio/audio_engine.h"
#include "engine/resource/game_manifest.h"
#include "engine/core/debug_log.h"
#include "engine/resource/file_io.h"

#ifndef EB_ANDROID
#include "editor/tile_editor.h"
#include "editor/imgui_integration.h"
#include "engine/platform/platform_desktop.h"
#include <GLFW/glfw3.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <set>
#include <sys/stat.h>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        // ─── Load game manifest ───
        eb::GameManifest manifest;
        if (!eb::load_game_manifest(manifest, "game.json")) {
            std::fprintf(stderr, "[Main] No game.json found, using defaults\n");
            manifest.title = "Twilight Engine";
            manifest.window_width = 960;
            manifest.window_height = 720;
        }

        eb::EngineConfig config;
        config.title = manifest.title + " (Tab = Editor)";
        config.width = manifest.window_width;
        config.height = manifest.window_height;
        config.vsync = true;
        config.fullscreen = true;

        eb::Engine engine(config);
        engine.renderer().set_shader_dir("shaders/");
        engine.renderer().set_clear_color(0.05f, 0.05f, 0.12f);

        GameState game;

        // Text renderer
        std::string font_path = manifest.default_font.empty()
            ? "assets/fonts/default.ttf" : manifest.default_font;
        eb::TextRenderer text_renderer(engine.renderer().vulkan_context(),
                                        font_path, 7.0f);
        text_renderer.set_letter_spacing(6.0f);
        game.font_desc = engine.renderer().get_texture_descriptor(*text_renderer.texture());
        game.white_desc = engine.renderer().default_texture_descriptor();

        // Initialize shared game logic (manifest-driven if available)
        if (manifest.loaded) {
            init_game_from_manifest(game, engine.renderer(), engine.resources(),
                                    (float)config.width, (float)config.height, manifest);
        } else {
            init_game(game, engine.renderer(), engine.resources(),
                      (float)config.width, (float)config.height);
        }

        // ─── SageLang scripting engine ───
        eb::ScriptEngine script_engine;
        script_engine.set_game_state(&game);

        // Auto-discover all .sage scripts in assets/scripts/ (recursive)
        {
            std::set<std::string> loaded_scripts;
            namespace fs = std::filesystem;
            try {
                for (auto& entry : fs::recursive_directory_iterator("assets/scripts/")) {
                    if (entry.is_regular_file() && entry.path().extension() == ".sage") {
                        std::string p = entry.path().string();
                        script_engine.load_file(p);
                        loaded_scripts.insert(p);
                    }
                }
                std::printf("[Scripts] Auto-loaded %d .sage files from assets/scripts/\n",
                            (int)loaded_scripts.size());
            } catch (...) {
                // Fallback: filesystem scan failed, load from manifest list
                for (const auto& sp : manifest.scripts) {
                    if (loaded_scripts.find(sp) == loaded_scripts.end())
                        script_engine.load_file(sp);
                }
            }
            // Also load any manifest scripts not already found by scan
            for (const auto& sp : manifest.scripts) {
                if (loaded_scripts.find(sp) == loaded_scripts.end())
                    script_engine.load_file(sp);
            }
        }
        game.script_engine = &script_engine;

        // Run init functions from manifest
        for (const auto& init_func : manifest.init_scripts) {
            if (script_engine.has_function(init_func)) {
                script_engine.call_function(init_func);
            }
        }

        // Load and execute map scripts (auto-scan scripts/maps/ directory)
        {
            // Try loading the default map script
            std::string default_map_script = "assets/scripts/maps/default.sage";
            auto map_data = eb::FileIO::read_file(default_map_script);
            if (!map_data.empty()) {
                std::string src(map_data.begin(), map_data.end());
                script_engine.execute(src);
                if (script_engine.has_function("map_init")) {
                    script_engine.call_function("map_init");
                    std::printf("[Main] Executed map script: %s\n", default_map_script.c_str());
                }
            }
            // Also try map script matching the default_map from manifest
            if (!manifest.default_map.empty()) {
                std::string name = manifest.default_map;
                auto slash = name.rfind('/');
                if (slash != std::string::npos) name = name.substr(slash + 1);
                auto dot = name.rfind('.');
                if (dot != std::string::npos) name = name.substr(0, dot);
                std::string map_script = "assets/scripts/maps/" + name + ".sage";
                auto ms_data = eb::FileIO::read_file(map_script);
                if (!ms_data.empty()) {
                    std::string src(ms_data.begin(), ms_data.end());
                    script_engine.execute(src);
                    if (script_engine.has_function("map_init")) {
                        script_engine.call_function("map_init");
                        std::printf("[Main] Executed map script: %s\n", map_script.c_str());
                    }
                }
            }
        }

        // ─── Audio engine ───
        eb::AudioEngine audio;
        if (audio.is_initialized() && !manifest.audio.overworld.empty()) {
            audio.set_music_volume(0.5f);
            audio.play_music(manifest.audio.overworld, true);
        }

#ifndef EB_ANDROID
        // Editor (desktop only)
        eb::TileEditor editor;
        editor.set_map(&game.tile_map);
        if (game.tileset_atlas)
            editor.set_tileset(game.tileset_atlas.get(), game.tileset_desc);
        editor.set_text_renderer(&text_renderer, game.font_desc);
        editor.set_object_stamps(&game.object_stamps);
        editor.set_game_state(&game);
        editor.set_renderer(&engine.renderer());

        // ImGui for editor UI
        eb::ImGuiIntegration imgui;
        auto* desktop_platform = dynamic_cast<eb::PlatformDesktop*>(&engine.platform());
        if (desktop_platform) {
            imgui.init(desktop_platform->glfw_window(), engine.renderer());
        }
#endif

#ifdef _WIN32
        mkdir("assets/maps");
#elif !defined(__ANDROID__)
        mkdir("assets/maps", 0755);
#endif

        // ─── Update ───
        engine.on_update = [&](float dt) {
            auto& input = engine.platform().input();
            eb::DebugLog::instance().set_time(game.game_time);

#ifndef EB_ANDROID
            // Tab toggles editor
            if (input.key_pressed(GLFW_KEY_TAB) && !imgui.wants_keyboard()) {
                editor.toggle();
                if (editor.is_active()) {
                    game.camera.clear_bounds();
                } else {
                    // Restore camera to game viewport (manifest size, not native screen)
                    float gw = (float)config.width;
                    float gh = (float)config.height;
                    game.camera.set_viewport(gw, gh);
                    game.camera.set_bounds(0, 0,
                        game.tile_map.world_width(), game.tile_map.world_height());
                    game.camera.set_follow_offset(eb::Vec2(0.0f, -gh * 0.1f));
                    game.camera.follow(game.player_pos, 100.0f);
                    game.camera.center_on(game.player_pos);
                }
            }

            // ESC in editor mode → close editor (pause menu handles ESC in game mode)
            if (input.is_pressed(eb::InputAction::Menu) && editor.is_active()) {
                editor.toggle();
                float gw = (float)config.width;
                float gh = (float)config.height;
                game.camera.set_viewport(gw, gh);
                game.camera.set_bounds(0, 0,
                    game.tile_map.world_width(), game.tile_map.world_height());
                game.camera.set_follow_offset(eb::Vec2(0.0f, -gh * 0.1f));
                game.camera.follow(game.player_pos, 100.0f);
                game.camera.center_on(game.player_pos);
            }

            // Handle pause menu requests
            if (game.pause_request_quit) {
                engine.quit();
                return;
            }
            if (game.pause_request_editor) {
                game.pause_request_editor = false;
                if (!editor.is_active()) {
                    editor.toggle();
                    game.camera.clear_bounds();
                }
            }
            if (game.pause_request_reset) {
                game.pause_request_reset = false;
                game.player_pos = {15.0f * 32.0f, 10.0f * 32.0f};
                game.player_hp = game.player_hp_max;
                game.sam_hp = game.sam_hp_max;
                game.camera.center_on(game.player_pos);
                game.script_ui.notifications.push_back({"Game Reset", 2.0f, 0.0f});
            }

            if (editor.is_active()) {
                float sw = (float)engine.platform().get_width();
                float sh = (float)engine.platform().get_height();

                // Always process deferred dialogs (runs before Vulkan frame)
                editor.process_pending_dialog();

                // Only pass input to editor if ImGui doesn't want it
                if (!imgui.wants_mouse() && !imgui.wants_keyboard()) {
                    editor.update(input, game.camera, dt, (int)sw, (int)sh);
                } else {
                    // Still need camera viewport for zoom
                    game.camera.set_viewport(sw / editor.zoom(), sh / editor.zoom());
                }
                game.camera.update(dt);
                return;
            }
#else
            if (input.is_pressed(eb::InputAction::Menu)) {
                engine.quit();
                return;
            }
#endif
            // Audio: crossfade between overworld/battle music
            if (audio.is_initialized()) {
                audio.update(dt);
                if (game.battle.phase != BattlePhase::None && !manifest.audio.battle.empty()) {
                    audio.crossfade_music(manifest.audio.battle, 0.5f, true);
                } else if (!manifest.audio.overworld.empty()) {
                    audio.crossfade_music(manifest.audio.overworld, 1.0f, true);
                }
            }

            // Shared game update
            update_game(game, input, dt);
        };

        // ─── Render ───
        engine.on_render = [&]() {
            auto& batch = engine.renderer().sprite_batch();
            float sw = (float)engine.platform().get_width();
            float sh = (float)engine.platform().get_height();

            // Battle screen
            if (game.battle.phase != BattlePhase::None) {
                batch.set_projection(engine.renderer().screen_projection());
                render_battle(game, batch, text_renderer, sw, sh);
                return;
            }

            // World rendering
            render_game_world(game, batch, text_renderer);

#ifndef EB_ANDROID
            if (editor.is_active()) {
                // Editor overlays (grid, collision, cursor)
                batch.set_texture(engine.renderer().default_texture_descriptor());
                editor.render(batch, game.camera, game.tileset_desc, (int)sw, (int)sh);

                // Flush sprite batch before ImGui renders
                batch.flush();

                // ImGui editor UI
                if (imgui.is_initialized()) {
                    imgui.new_frame();
                    editor.render_imgui(game);
                    imgui.render(engine.renderer().current_command_buffer());
                }
            } else
#endif
            {
                render_game_ui(game, batch, text_renderer,
                               engine.renderer().screen_projection(), sw, sh);
            }
        };

        std::printf("[Main] Starting %s\n", manifest.title.c_str());
        std::printf("  WASD/Arrows - Move\n");
        std::printf("  Shift       - Run\n");
        std::printf("  Z/Enter     - Talk / Confirm\n");
        std::printf("  Tab         - Toggle editor\n");
        std::printf("  ESC         - Quit\n");
        engine.run();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Fatal] %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
