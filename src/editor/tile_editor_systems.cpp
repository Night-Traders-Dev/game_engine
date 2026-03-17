#include "editor/tile_editor.h"
#include "game/game.h"
#ifndef EB_ANDROID
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#endif
#include "engine/graphics/texture_atlas.h"
#include "engine/graphics/text_renderer.h"
#include "engine/graphics/renderer.h"
#include "engine/platform/input.h"
#include "engine/core/debug_log.h"
#include "engine/resource/file_io.h"
#include "engine/scripting/script_engine.h"
#include "game/overworld/camera.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>

namespace eb {

void TileEditor::render_imgui_game_systems(GameState& game) {
#ifndef EB_ANDROID
    // ═══════════ GAME SYSTEMS PANEL (new systems overview) ═══════════
    {
        ImGui::SetNextWindowPos(ImVec2(500, 28), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);
        static bool show_systems = false;
        // Toggle via Tools menu or add a keyboard shortcut
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Systems")) {
                ImGui::MenuItem("Game Systems", "F5", &show_systems);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if (show_systems) {
            if (ImGui::Begin("Game Systems##editor", &show_systems)) {
                if (ImGui::CollapsingHeader("Tweens", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Active: %d", (int)game.tween_system.tweens.size());
                    ImGui::Text("Delays: %d", (int)game.tween_system.delays.size());
                    if (ImGui::Button("Clear All Tweens")) game.tween_system.clear();
                    for (auto& tw : game.tween_system.tweens) {
                        ImGui::BulletText("%s.%s -> %.1f (%.0f%%)",
                            tw.target.c_str(), tw.property.c_str(),
                            tw.end_val, tw.progress() * 100);
                    }
                }

                if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Emitters: %d", (int)game.emitters.size());
                    ImGui::Text("Particles: %d", game.debug_particle_count);
                    if (ImGui::Button("Clear Particles")) game.emitters.clear();
                    ImGui::Separator();
                    ImGui::Text("Spawn Preset:");
                    static float spawn_x = 480, spawn_y = 320;
                    ImGui::DragFloat("X##pspawn", &spawn_x, 1, 0, 2000);
                    ImGui::DragFloat("Y##pspawn", &spawn_y, 1, 0, 2000);
                    const char* presets[] = {"fire","smoke","sparkle","blood","dust","magic","explosion","heal","rain_splash"};
                    for (int i = 0; i < 9; i++) {
                        if (i > 0) ImGui::SameLine();
                        if (ImGui::SmallButton(presets[i])) {
                            game.emitters.push_back(eb::make_preset(presets[i], spawn_x, spawn_y));
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Visual FX", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Water Reflections", &game.water_reflections);
                    ImGui::SliderFloat("Reflection Alpha", &game.reflection_alpha, 0.0f, 1.0f);
                    ImGui::Separator();
                    ImGui::Checkbox("Bloom / Glow", &game.bloom_enabled);
                    ImGui::SliderFloat("Bloom Intensity", &game.bloom_intensity, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bloom Threshold", &game.bloom_threshold, 0.0f, 1.0f);
                }

                if (ImGui::CollapsingHeader("Lighting")) {
                    ImGui::Checkbox("Enable Lighting", &game.lighting_enabled);
                    ImGui::SliderFloat("Ambient", &game.ambient_light, 0.0f, 1.0f);
                    ImGui::Text("Lights: %d", (int)game.lights.size());
                    if (ImGui::Button("Add Light at Player")) {
                        game.lights.push_back({game.player_pos.x, game.player_pos.y, 128, 1.0f, {1,0.9f,0.7f,1}, true});
                    }
                    for (int i = 0; i < (int)game.lights.size(); i++) {
                        auto& l = game.lights[i];
                        if (!l.active) continue;
                        ImGui::PushID(i + 5000);
                        ImGui::DragFloat("Radius", &l.radius, 1, 16, 512);
                        ImGui::SliderFloat("Intensity", &l.intensity, 0, 1);
                        if (ImGui::SmallButton("Remove")) l.active = false;
                        ImGui::PopID();
                        ImGui::Separator();
                    }
                }

                if (ImGui::CollapsingHeader("Quests")) {
                    ImGui::Text("Active Quests: %d", (int)game.quests.size());
                    for (auto& q : game.quests) {
                        const char* states[] = {"Not Started","Active","Complete","Failed"};
                        ImGui::BulletText("%s [%s]", q.title.c_str(), states[(int)q.state]);
                        for (auto& obj : q.objectives) {
                            ImGui::Text("  %s %s", obj.complete ? "[x]" : "[ ]", obj.desc.c_str());
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Equipment")) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "Weapon: %s", game.equipment.weapon.empty() ? "(none)" : game.equipment.weapon.c_str());
                    ImGui::Text("%s", buf);
                    std::snprintf(buf, sizeof(buf), "Armor: %s", game.equipment.armor.empty() ? "(none)" : game.equipment.armor.c_str());
                    ImGui::Text("%s", buf);
                    std::snprintf(buf, sizeof(buf), "Accessory: %s", game.equipment.accessory.empty() ? "(none)" : game.equipment.accessory.c_str());
                    ImGui::Text("%s", buf);
                    ImGui::Text("Bonuses: ATK+%d DEF+%d HP+%d", game.equipment.atk_bonus, game.equipment.def_bonus, game.equipment.hp_bonus);
                }

                if (ImGui::CollapsingHeader("Save/Load")) {
                    ImGui::Text("Playtime: %.0fs", game.playtime_seconds);
                    ImGui::Text("Flags: %d", (int)game.flags.data.size());
                    for (int slot = 1; slot <= 3; slot++) {
                        char label[32];
                        std::snprintf(label, sizeof(label), "Save Slot %d", slot);
                        if (ImGui::Button(label)) {
                            eb::SaveData data;
                            data.player_x = game.player_pos.x; data.player_y = game.player_pos.y;
                            data.player_hp = game.player_hp; data.player_hp_max = game.player_hp_max;
                            data.player_atk = game.player_atk; data.player_def = game.player_def;
                            data.player_level = game.player_level; data.gold = game.gold;
                            data.playtime_seconds = game.playtime_seconds; data.flags = game.flags;
                            eb::SaveSystem::save(slot, data);
                        }
                        ImGui::SameLine();
                        std::snprintf(label, sizeof(label), "Load %d", slot);
                        bool has = eb::SaveSystem::has_save(slot);
                        if (!has) ImGui::BeginDisabled();
                        if (ImGui::Button(label)) {
                            eb::SaveData data;
                            if (eb::SaveSystem::load(slot, data)) {
                                game.player_pos = {data.player_x, data.player_y};
                                game.player_hp = data.player_hp; game.player_hp_max = data.player_hp_max;
                                game.gold = data.gold; game.playtime_seconds = data.playtime_seconds;
                            }
                        }
                        if (!has) ImGui::EndDisabled();
                    }
                }

                if (ImGui::CollapsingHeader("Settings")) {
                    ImGui::SliderFloat("Master Vol", &game.settings.master_volume, 0, 1);
                    ImGui::SliderFloat("Music Vol", &game.settings.music_volume, 0, 1);
                    ImGui::SliderFloat("SFX Vol", &game.settings.sfx_volume, 0, 1);
                    ImGui::SliderInt("Text Speed", &game.settings.text_speed, 1, 3);
                    ImGui::Checkbox("Fullscreen", &game.settings.fullscreen);
                }

                if (ImGui::CollapsingHeader("Achievements")) {
                    ImGui::Text("Unlocked: %d", (int)game.achievements.size());
                    for (auto& a : game.achievements) {
                        ImGui::BulletText("%s %s", a.unlocked ? "[*]" : "[ ]", a.title.c_str());
                    }
                }

                if (ImGui::CollapsingHeader("Transitions")) {
                    ImGui::Text("Active: %s", game.transition.active ? "Yes" : "No");
                    ImGui::Text("Progress: %.2f", game.transition.progress);
                    if (ImGui::Button("Fade In")) game.transition.start(eb::TransitionType::Fade, 1.0f, false);
                    ImGui::SameLine();
                    if (ImGui::Button("Fade Out")) game.transition.start(eb::TransitionType::Fade, 1.0f, true);
                    ImGui::SameLine();
                    if (ImGui::Button("Iris In")) game.transition.start(eb::TransitionType::Iris, 1.0f, false);
                    if (ImGui::Button("Wipe Left")) game.transition.start(eb::TransitionType::Wipe, 0.8f, false, 0);
                    ImGui::SameLine();
                    if (ImGui::Button("Wipe Right")) game.transition.start(eb::TransitionType::Wipe, 0.8f, false, 1);
                }

                if (ImGui::CollapsingHeader("Map")) {
                    ImGui::Text("Size: %dx%d", map_ ? map_->width() : 0, map_ ? map_->height() : 0);
                    static int new_w = 30, new_h = 22;
                    ImGui::InputInt("New Width", &new_w);
                    ImGui::InputInt("New Height", &new_h);
                    new_w = std::max(4, std::min(200, new_w));
                    new_h = std::max(4, std::min(200, new_h));
                    if (ImGui::Button("Resize Map")) {
                        resize_map(new_w, new_h);
                    }
                    ImGui::Separator();
                    const char* game_type_items[] = { "TopDown", "Platformer" };
                    int current_type = (game.game_type == GameType::Platformer) ? 1 : 0;
                    if (ImGui::Combo("Game Type", &current_type, game_type_items, 2)) {
                        game.game_type = (current_type == 1) ? GameType::Platformer : GameType::TopDown;
                    }
                }

                if (ImGui::CollapsingHeader("Parallax Backgrounds")) {
                    ImGui::Text("Layers: %d", (int)game.parallax_layers.size());

                    // Preset loader
                    static const char* bg_presets[] = {"forest", "cave", "night", "sunset", "snow", "desert", "forest_sunset", "forest_trees"};
                    static int preset_idx = 0;
                    ImGui::Combo("Preset", &preset_idx, bg_presets, IM_ARRAYSIZE(bg_presets));
                    ImGui::SameLine();
                    if (ImGui::Button("Load Preset")) {
                        if (game.script_engine) {
                            game.script_engine->set_string("_bg_preset", bg_presets[preset_idx]);
                            game.script_engine->execute("load_parallax_preset(_bg_preset)");
                            set_status(std::string("Loaded parallax: ") + bg_presets[preset_idx]);
                        }
                    }

                    if (ImGui::Button("Clear All Backgrounds")) {
                        game.parallax_layers.clear();
                        set_status("Cleared parallax layers");
                    }

                    ImGui::Separator();

                    // Per-layer properties
                    for (int i = 0; i < (int)game.parallax_layers.size(); i++) {
                        auto& l = game.parallax_layers[i];
                        std::string label = "Layer " + std::to_string(i);
                        if (!l.texture_path.empty()) {
                            auto slash = l.texture_path.rfind('/');
                            if (slash != std::string::npos) label += " (" + l.texture_path.substr(slash + 1) + ")";
                        }
                        if (ImGui::TreeNode(label.c_str())) {
                            ImGui::Checkbox("Active", &l.active);
                            ImGui::SliderFloat("Scroll X", &l.scroll_x, 0, 1.0f);
                            ImGui::SliderFloat("Scroll Y", &l.scroll_y, 0, 0.5f);
                            ImGui::DragFloat("Auto Scroll X", &l.auto_scroll_x, 0.5f, -100, 100, "%.1f px/s");
                            ImGui::SliderFloat("Scale", &l.scale, 0.25f, 4.0f);
                            ImGui::ColorEdit4("Tint", &l.tint.x);
                            ImGui::Checkbox("Repeat X", &l.repeat_x);
                            ImGui::SameLine();
                            ImGui::Checkbox("Pin Bottom", &l.pin_bottom);
                            ImGui::SameLine();
                            ImGui::Checkbox("Fill VP", &l.fill_viewport);
                            ImGui::InputInt("Z Order", &l.z_order);
                            if (ImGui::Button(("Remove##plx" + std::to_string(i)).c_str())) {
                                game.parallax_layers.erase(game.parallax_layers.begin() + i);
                                i--;
                            }
                            ImGui::TreePop();
                        }
                    }

                    // Add custom layer
                    ImGui::Separator();
                    static char new_bg_path[256] = "assets/textures/parallax/forest/layer_0_sky.png";
                    ImGui::InputText("Texture Path", new_bg_path, sizeof(new_bg_path));
                    if (ImGui::Button("+ Add Layer")) {
                        if (game.resource_manager && game.renderer) {
                            try {
                                auto* tex = game.resource_manager->load_texture(new_bg_path);
                                if (tex) {
                                    eb::ParallaxLayer pl;
                                    pl.texture_path = new_bg_path;
                                    pl.texture_desc = (void*)game.renderer->get_texture_descriptor(*tex);
                                    pl.tex_width = tex->width();
                                    pl.tex_height = tex->height();
                                    pl.z_order = (int)game.parallax_layers.size();
                                    pl.repeat_x = true;
                                    pl.pin_bottom = true;
                                    game.parallax_layers.push_back(pl);
                                    set_status("Added parallax layer: " + std::string(new_bg_path));
                                }
                            } catch (...) { set_status("Failed to load: " + std::string(new_bg_path)); }
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Auto-Tiling")) {
                    ImGui::Checkbox("Enable Auto-Tile", &auto_tile_enabled_);
                    ImGui::InputInt("Terrain A Tile", &auto_tile_config_.terrain_a_tile);
                    ImGui::InputInt("Terrain B Tile", &auto_tile_config_.terrain_b_tile);
                    ImGui::InputInt("Transition Start", &auto_tile_config_.transition_start);
                    auto_tile_config_.configured = (auto_tile_config_.transition_start > 0);
                }

                if (ImGui::CollapsingHeader("Gamepad")) {
                    auto& gp = game.current_input ? game.current_input->gamepad : *(InputState::GamepadState*)nullptr;
                    if (game.current_input && game.current_input->gamepad.connected) {
                        ImGui::TextColored(ImVec4(0.2f,1,0.2f,1), "Gamepad Connected");
                        ImGui::Text("L Stick: %.2f, %.2f", gp.axes[0], gp.axes[1]);
                        ImGui::Text("R Stick: %.2f, %.2f", gp.axes[2], gp.axes[3]);
                        ImGui::Text("Triggers: L=%.2f R=%.2f", gp.axes[4], gp.axes[5]);
                        std::string btns;
                        const char* names[] = {"A","B","X","Y","LB","RB","Bk","St","Gd","LS","RS","DU","DR","DD","DL"};
                        for (int i = 0; i < 15; i++) if (gp.buttons[i]) { if (!btns.empty()) btns += " "; btns += names[i]; }
                        ImGui::Text("Buttons: %s", btns.empty() ? "(none)" : btns.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "No Gamepad");
                    }
                }

                if (ImGui::CollapsingHeader("Dialogue History")) {
                    ImGui::Text("Entries: %d", (int)game.dialogue_history.size());
                    ImGui::Text("Talked to: %d NPCs", (int)game.talked_to.size());
                    for (auto& [name, _] : game.talked_to) {
                        ImGui::BulletText("%s", name.c_str());
                    }
                }

                if (ImGui::CollapsingHeader("Events")) {
                    ImGui::Text("Listeners: %d", (int)game.event_listeners.size());
                    for (auto& e : game.event_listeners) {
                        ImGui::BulletText("%s -> %s()", e.event_name.c_str(), e.callback.c_str());
                    }
                }

            }
            ImGui::End();
        }
    }
#endif
}

} // namespace eb
