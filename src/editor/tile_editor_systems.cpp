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
                    static int new_w = 40, new_h = 30;
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

                if (ImGui::CollapsingHeader("Player & Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "Position & Movement");
                    ImGui::DragFloat("Pos X", &game.player_pos.x, 1, 0, 10000);
                    ImGui::DragFloat("Pos Y", &game.player_pos.y, 1, 0, 10000);
                    ImGui::DragFloat("Speed", &game.player_speed, 1, 10, 500);
                    if (ImGui::Button("Teleport to Center")) {
                        game.player_pos = {game.tile_map.world_width() * 0.5f, game.tile_map.world_height() * 0.5f};
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Teleport to Origin")) {
                        game.player_pos = {3.0f * game.tile_map.tile_size(), 3.0f * game.tile_map.tile_size()};
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "Stats");
                    ImGui::DragInt("HP", &game.player_hp, 1, 0, 9999);
                    ImGui::DragInt("HP Max", &game.player_hp_max, 1, 1, 9999);
                    ImGui::DragInt("ATK", &game.player_atk, 1, 0, 999);
                    ImGui::DragInt("DEF", &game.player_def, 1, 0, 999);
                    ImGui::DragInt("Level", &game.player_level, 1, 1, 100);
                    ImGui::DragInt("XP", &game.player_xp, 1, 0, 99999);
                    ImGui::DragInt("Gold", &game.gold, 1, 0, 999999);
                    if (ImGui::Button("Full Heal")) {
                        game.player_hp = game.player_hp_max;
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "Sprite & Scale");
                    ImGui::DragFloat("Player Scale", &game.player_sprite_scale, 0.05f, 0.25f, 4.0f, "%.2f");
                    ImGui::DragFloat("Ally Scale", &game.ally_sprite_scale, 0.05f, 0.25f, 4.0f, "%.2f");

                    // Sprite asset picker — load a new player sprite from file path
                    static char player_sprite_path[256] = "assets/textures/dean_sprites.png";
                    ImGui::InputText("Player Sprite", player_sprite_path, sizeof(player_sprite_path));
                    static int player_grid_w = 158, player_grid_h = 210;
                    ImGui::InputInt("Grid W##plr", &player_grid_w);
                    ImGui::SameLine();
                    ImGui::InputInt("Grid H##plr", &player_grid_h);
                    if (ImGui::Button("Load Player Sprite")) {
                        if (game.resource_manager && game.renderer) {
                            try {
                                auto* tex = game.resource_manager->load_texture(player_sprite_path);
                                if (tex && player_grid_w > 0 && player_grid_h > 0) {
                                    game.dean_atlas = std::make_unique<eb::TextureAtlas>(tex, player_grid_w, player_grid_h);
                                    int cw = player_grid_w, ch = player_grid_h;
                                    game.dean_atlas->define_region("idle_down",     0,      0, cw, ch);
                                    game.dean_atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                                    game.dean_atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                                    game.dean_atlas->define_region("idle_up",       0,     ch, cw, ch);
                                    game.dean_atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                                    game.dean_atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                                    game.dean_atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                                    game.dean_atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                                    game.dean_atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                                    game.dean_desc = game.renderer->get_texture_descriptor(*tex);
                                    set_status("Player sprite loaded: " + std::string(player_sprite_path));
                                }
                            } catch (...) { set_status("Failed: " + std::string(player_sprite_path)); }
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "AI Character Presets");
                    {
                        static const char* char_presets[] = {
                            "assets/textures/ai/knight_sheet.png",
                            "assets/textures/ai/wizard_sheet.png",
                            "assets/textures/ai/skeleton_sheet.png",
                            "assets/textures/ai/slime_sheet.png",
                            "assets/textures/ai/goblin_sheet.png",
                            "assets/textures/ai/merchant_sheet.png",
                            "assets/textures/ai/princess_sheet.png",
                            "assets/textures/ai/dragon_sheet.png",
                            "assets/textures/blender_character.png",
                            "assets/textures/mage_player.png",
                        };
                        static const char* char_names[] = {
                            "Knight (AI)", "Wizard (AI)", "Skeleton (AI)", "Slime (AI)",
                            "Goblin (AI)", "Merchant (AI)", "Princess (AI)", "Dragon (AI)",
                            "Blender Sample", "Default Mage"
                        };
                        static int char_preset_idx = 0;
                        ImGui::Combo("Character##preset", &char_preset_idx, char_names, IM_ARRAYSIZE(char_names));
                        if (ImGui::Button("Set as Player Sprite")) {
                            std::strncpy(player_sprite_path, char_presets[char_preset_idx], sizeof(player_sprite_path)-1);
                            player_grid_w = 64; player_grid_h = 64;
                            // Trigger load
                            if (game.resource_manager && game.renderer) {
                                try {
                                    auto* tex = game.resource_manager->load_texture(player_sprite_path);
                                    if (tex) {
                                        game.dean_atlas = std::make_unique<eb::TextureAtlas>(tex, player_grid_w, player_grid_h);
                                        int cw = player_grid_w, ch = player_grid_h;
                                        game.dean_atlas->define_region("idle_down",     0,      0, cw, ch);
                                        game.dean_atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                                        game.dean_atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                                        game.dean_atlas->define_region("idle_up",       0,     ch, cw, ch);
                                        game.dean_atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                                        game.dean_atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                                        game.dean_atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                                        game.dean_atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                                        game.dean_atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                                        game.dean_desc = game.renderer->get_texture_descriptor(*tex);
                                        set_status("Player sprite: " + std::string(char_names[char_preset_idx]));
                                    }
                                } catch (...) { set_status("Failed to load character preset"); }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Spawn as NPC")) {
                            if (game.script_engine) {
                                char cmd[512];
                                std::snprintf(cmd, sizeof(cmd),
                                    "spawn_npc(\"%s\", %.0f, %.0f, 0, false, \"%s\", 0, 0, 50, 0, 64, 64)",
                                    char_names[char_preset_idx],
                                    game.player_pos.x + 64, game.player_pos.y,
                                    char_presets[char_preset_idx]);
                                game.script_engine->execute(cmd);
                                set_status("Spawned NPC: " + std::string(char_names[char_preset_idx]));
                            }
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "Character Stats (S.P.E.C.I.A.L.)");
                    ImGui::SliderInt("Vitality", &game.player_stats.vitality, 1, 10);
                    ImGui::SliderInt("Arcana", &game.player_stats.arcana, 1, 10);
                    ImGui::SliderInt("Agility", &game.player_stats.agility, 1, 10);
                    ImGui::SliderInt("Tactics", &game.player_stats.tactics, 1, 10);
                    ImGui::SliderInt("Spirit", &game.player_stats.spirit, 1, 10);
                    ImGui::SliderInt("Strength", &game.player_stats.strength, 1, 10);
                    ImGui::Text("Total: %d/%d", game.player_stats.total(), CharacterStats::STARTING_POINTS);
                    ImGui::Text("HP Bonus: +%d  Crit: %.0f%%  Dodge: %.0f%%",
                        game.player_stats.hp_bonus(),
                        game.player_stats.crit_chance() * 100,
                        game.player_stats.dodge_chance() * 100);
                }

                if (ImGui::CollapsingHeader("Party")) {
                    ImGui::Text("Party Size: %d", (int)game.party.size());
                    ImGui::Separator();

                    // Ally stats (Sam)
                    ImGui::TextColored(ImVec4(1,0.84f,0.31f,1), "Ally (Sam)");
                    ImGui::DragInt("Ally HP", &game.sam_hp, 1, 0, 9999);
                    ImGui::DragInt("Ally HP Max", &game.sam_hp_max, 1, 1, 9999);
                    ImGui::DragInt("Ally ATK", &game.sam_atk, 1, 0, 999);
                    if (ImGui::Button("Heal Ally")) game.sam_hp = game.sam_hp_max;

                    // Ally sprite picker
                    static char ally_sprite_path[256] = "assets/textures/sam_sprites.png";
                    ImGui::InputText("Ally Sprite", ally_sprite_path, sizeof(ally_sprite_path));
                    static int ally_grid_w = 0, ally_grid_h = 0;
                    ImGui::InputInt("Grid W##ally", &ally_grid_w);
                    ImGui::SameLine();
                    ImGui::InputInt("Grid H##ally", &ally_grid_h);
                    ImGui::TextDisabled("(0 = use custom regions from code)");
                    if (ImGui::Button("Load Ally Sprite")) {
                        if (game.resource_manager && game.renderer) {
                            try {
                                auto* tex = game.resource_manager->load_texture(ally_sprite_path);
                                if (tex) {
                                    if (ally_grid_w > 0 && ally_grid_h > 0) {
                                        game.sam_atlas = std::make_unique<eb::TextureAtlas>(tex, ally_grid_w, ally_grid_h);
                                        int cw = ally_grid_w, ch = ally_grid_h;
                                        game.sam_atlas->define_region("idle_down",     0,      0, cw, ch);
                                        game.sam_atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                                        game.sam_atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                                        game.sam_atlas->define_region("idle_up",       0,     ch, cw, ch);
                                        game.sam_atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                                        game.sam_atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                                        game.sam_atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                                        game.sam_atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                                        game.sam_atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                                    } else {
                                        game.sam_atlas = std::make_unique<eb::TextureAtlas>(tex);
                                    }
                                    game.sam_desc = game.renderer->get_texture_descriptor(*tex);
                                    set_status("Ally sprite loaded: " + std::string(ally_sprite_path));
                                }
                            } catch (...) { set_status("Failed: " + std::string(ally_sprite_path)); }
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1,0.84f,0.31f,1), "Ally Stats (S.P.E.C.I.A.L.)");
                    ImGui::SliderInt("Vitality##ally", &game.ally_stats.vitality, 1, 10);
                    ImGui::SliderInt("Arcana##ally", &game.ally_stats.arcana, 1, 10);
                    ImGui::SliderInt("Agility##ally", &game.ally_stats.agility, 1, 10);
                    ImGui::SliderInt("Tactics##ally", &game.ally_stats.tactics, 1, 10);
                    ImGui::SliderInt("Spirit##ally", &game.ally_stats.spirit, 1, 10);
                    ImGui::SliderInt("Strength##ally", &game.ally_stats.strength, 1, 10);

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1,0.84f,0.31f,1), "Party Members (%d / %d)", (int)game.party.size(), MAX_PARTY_MEMBERS);

                    // Per-member editor
                    int remove_idx = -1;
                    for (int i = 0; i < (int)game.party.size(); i++) {
                        auto& pm = game.party[i];
                        ImGui::PushID(i + 8000);
                        bool open = ImGui::TreeNode(("##pm" + std::to_string(i)).c_str(), "%s", pm.name.c_str());
                        // Quick remove button on the same line
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.33f, 0.31f, 0.8f));
                        if (ImGui::SmallButton("X")) remove_idx = i;
                        ImGui::PopStyleColor();

                        if (open) {
                            // Name
                            char nbuf[64];
                            std::strncpy(nbuf, pm.name.c_str(), sizeof(nbuf) - 1); nbuf[sizeof(nbuf)-1] = 0;
                            if (ImGui::InputText("Name", nbuf, sizeof(nbuf))) pm.name = nbuf;

                            // Position
                            ImGui::DragFloat2("Position", &pm.position.x, 1, 0, 10000);
                            ImGui::DragInt("Direction", &pm.dir, 1, 0, 3);
                            if (ImGui::Button("Teleport to Player")) pm.position = game.player_pos;

                            // Combat stats
                            ImGui::Separator();
                            ImGui::DragInt("HP", &pm.hp, 1, 0, 9999);
                            ImGui::DragInt("HP Max", &pm.hp_max, 1, 1, 9999);
                            ImGui::DragInt("ATK", &pm.atk, 1, 0, 999);
                            ImGui::DragInt("DEF", &pm.def, 1, 0, 999);
                            if (ImGui::Button("Full Heal##pm")) pm.hp = pm.hp_max;

                            // Scale
                            ImGui::DragFloat("Scale", &pm.sprite_scale, 0.05f, 0.25f, 4.0f, "%.2f");

                            // Sprite
                            ImGui::Separator();
                            char spbuf[256];
                            std::strncpy(spbuf, pm.sprite_path.c_str(), sizeof(spbuf) - 1); spbuf[sizeof(spbuf)-1] = 0;
                            if (ImGui::InputText("Sprite Path", spbuf, sizeof(spbuf))) pm.sprite_path = spbuf;
                            ImGui::InputInt("Grid W", &pm.sprite_grid_w);
                            ImGui::SameLine();
                            ImGui::InputInt("Grid H", &pm.sprite_grid_h);
                            if (ImGui::Button("Load Sprite##pm")) {
                                if (game.resource_manager && game.renderer && !pm.sprite_path.empty()) {
                                    try {
                                        auto* tex = game.resource_manager->load_texture(pm.sprite_path);
                                        if (tex) {
                                            // Store in atlas cache
                                            std::string key = pm.sprite_path;
                                            if (pm.sprite_grid_w > 0 && pm.sprite_grid_h > 0) {
                                                char kb[32]; std::snprintf(kb, sizeof(kb), "@%dx%d", pm.sprite_grid_w, pm.sprite_grid_h);
                                                key += kb;
                                                game.atlas_cache[key] = std::make_shared<eb::TextureAtlas>(tex, pm.sprite_grid_w, pm.sprite_grid_h);
                                                int cw = pm.sprite_grid_w, ch = pm.sprite_grid_h;
                                                auto& atlas = game.atlas_cache[key];
                                                atlas->define_region("idle_down",     0,      0, cw, ch);
                                                atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                                                atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                                                atlas->define_region("idle_up",       0,     ch, cw, ch);
                                                atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                                                atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                                                atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                                                atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                                                atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                                            } else {
                                                game.atlas_cache[key] = std::make_shared<eb::TextureAtlas>(tex);
                                            }
                                            game.atlas_descs[key] = game.renderer->get_texture_descriptor(*tex);
                                            set_status("Party sprite loaded: " + pm.sprite_path);
                                        }
                                    } catch (...) { set_status("Failed: " + pm.sprite_path); }
                                }
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }

                    // Remove deferred (safe — outside iteration)
                    if (remove_idx >= 0 && remove_idx < (int)game.party.size()) {
                        std::string removed_name = game.party[remove_idx].name;
                        game.party.erase(game.party.begin() + remove_idx);
                        set_status("Removed party member: " + removed_name);
                    }

                    // Add new member
                    if ((int)game.party.size() < MAX_PARTY_MEMBERS) {
                        ImGui::Separator();
                        static char new_member_name[64] = "Companion";
                        ImGui::InputText("New Name##addpm", new_member_name, sizeof(new_member_name));
                        if (ImGui::Button("+ Add Party Member")) {
                            PartyMember pm;
                            pm.name = new_member_name;
                            pm.position = game.player_pos;
                            pm.hp = 80; pm.hp_max = 80;
                            pm.atk = 12; pm.def = 4;
                            game.party.push_back(pm);
                            set_status("Added party member: " + std::string(new_member_name));
                        }
                    } else {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(1,0.65f,0.15f,1), "Party full (max %d members)", MAX_PARTY_MEMBERS);
                    }

                    if (game.party.empty()) {
                        ImGui::TextDisabled("No party members. Use + Add above or game.json manifest.");
                    }

                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1,0.84f,0.31f,1), "Trail");
                    ImGui::Text("Trail size: %d / %d", game.trail_count, GameState::TRAIL_SIZE);
                    ImGui::Text("Follow Distance: %d (compile-time)", GameState::FOLLOW_DISTANCE);
                }

                if (ImGui::CollapsingHeader("Blender Import")) {
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "Import .blend as Sprite Sheet");
                    static char blend_path[256] = "games/demo/assets/blender/sample_character.blend";
                    ImGui::InputText(".blend File", blend_path, sizeof(blend_path));

                    static char sprite_output[256] = "assets/textures/imported_sprite.png";
                    ImGui::InputText("Output PNG", sprite_output, sizeof(sprite_output));

                    static int blend_size = 64;
                    ImGui::SliderInt("Frame Size (px)", &blend_size, 16, 256);

                    static int blend_mode = 0;
                    const char* blend_modes[] = {"Top-Down RPG", "Side (Platformer)"};
                    ImGui::Combo("Camera Mode", &blend_mode, blend_modes, 2);

                    static char blend_status[256] = "";
                    if (ImGui::Button("Import to Sprite Sheet")) {
                        // Run blender_to_spritesheet.py via system()
                        char cmd[1024];
                        std::snprintf(cmd, sizeof(cmd),
                            "python3 tools/blender_to_spritesheet.py --blend \"%s\" --output \"%s\" --size %d --mode %s 2>&1",
                            blend_path, sprite_output, blend_size,
                            blend_mode == 0 ? "topdown" : "side");
                        std::FILE* pipe = popen(cmd, "r");
                        if (pipe) {
                            char buf[512]; std::string result;
                            while (std::fgets(buf, sizeof(buf), pipe)) result += buf;
                            int ret = pclose(pipe);
                            if (ret == 0) {
                                std::snprintf(blend_status, sizeof(blend_status), "OK: %s (%dx%d)", sprite_output, blend_size, blend_size);
                                set_status("Blender import complete: " + std::string(sprite_output));
                                // Auto-load into atlas cache
                                if (game.resource_manager && game.renderer) {
                                    try {
                                        auto* tex = game.resource_manager->load_texture(sprite_output);
                                        if (tex) {
                                            std::string key = std::string(sprite_output);
                                            char kb[32]; std::snprintf(kb, sizeof(kb), "@%dx%d", blend_size, blend_size);
                                            key += kb;
                                            game.atlas_cache[key] = std::make_shared<eb::TextureAtlas>(tex, blend_size, blend_size);
                                            auto& atlas = game.atlas_cache[key];
                                            int cw = blend_size, ch = blend_size;
                                            atlas->define_region("idle_down",     0,      0, cw, ch);
                                            atlas->define_region("walk_down_0",  cw,      0, cw, ch);
                                            atlas->define_region("walk_down_1",  cw*2,    0, cw, ch);
                                            atlas->define_region("idle_up",       0,     ch, cw, ch);
                                            atlas->define_region("walk_up_0",    cw,     ch, cw, ch);
                                            atlas->define_region("walk_up_1",    cw*2,   ch, cw, ch);
                                            atlas->define_region("idle_right",    0,   ch*2, cw, ch);
                                            atlas->define_region("walk_right_0", cw,   ch*2, cw, ch);
                                            atlas->define_region("walk_right_1", cw*2, ch*2, cw, ch);
                                            game.atlas_descs[key] = game.renderer->get_texture_descriptor(*tex);
                                        }
                                    } catch (...) {}
                                }
                            } else {
                                std::snprintf(blend_status, sizeof(blend_status), "FAILED (exit %d)", ret);
                                set_status("Blender import failed");
                            }
                        } else {
                            std::snprintf(blend_status, sizeof(blend_status), "Failed to run command");
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Create Sample .blend")) {
                        char cmd[512];
                        std::snprintf(cmd, sizeof(cmd),
                            "blender -b -P tools/blender_create_sample.py -- --output \"%s\" 2>/dev/null", blend_path);
                        int ret = std::system(cmd);
                        if (ret == 0) {
                            std::snprintf(blend_status, sizeof(blend_status), "Sample created: %s", blend_path);
                            set_status("Sample .blend created");
                        } else {
                            std::snprintf(blend_status, sizeof(blend_status), "Failed to create sample");
                        }
                    }

                    if (blend_status[0]) {
                        ImGui::TextColored(
                            blend_status[0] == 'O' ? ImVec4(0.4f,0.9f,0.4f,1) :
                            blend_status[0] == 'F' ? ImVec4(0.9f,0.3f,0.3f,1) :
                                                     ImVec4(0.8f,0.8f,0.8f,1),
                            "%s", blend_status);
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Pipeline: .blend -> Blender renders 3x3 frames -> Pillow assembles sprite sheet");
                    ImGui::TextDisabled("Output: 3-column x 3-row grid (down/up/right, idle/walk0/walk1)");
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
