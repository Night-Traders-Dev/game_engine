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

void TileEditor::render_imgui_npc_spawner(GameState& game) {
#ifndef EB_ANDROID
    if (show_npc_spawner_) {
    ImGui::SetNextWindowPos(ImVec2(250, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 350), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("NPC Spawner##editor", &show_npc_spawner_)) {
        static char npc_name[64] = "Villager";
        static int npc_dir = 0;
        static bool npc_hostile = false;
        static bool npc_has_battle = false;
        static int npc_battle_hp = 30;
        static int npc_battle_atk = 8;
        static float npc_speed = 25.0f;
        static float npc_aggro = 120.0f;
        static int npc_sprite_id = -1;

        ImGui::InputText("Name", npc_name, sizeof(npc_name));
        ImGui::Combo("Direction", &npc_dir, "Down\0Up\0Left\0Right\0");
        ImGui::Checkbox("Hostile", &npc_hostile);
        if (npc_hostile) {
            ImGui::Checkbox("Has Battle", &npc_has_battle);
            if (npc_has_battle) {
                ImGui::SliderInt("HP", &npc_battle_hp, 10, 500);
                ImGui::SliderInt("ATK", &npc_battle_atk, 1, 50);
            }
            ImGui::SliderFloat("Aggro Range", &npc_aggro, 50, 300);
        }
        ImGui::SliderFloat("Speed", &npc_speed, 10, 100);

        // NPC sprite selection
        ImGui::Text("Sprite Atlas ID: %d", npc_sprite_id);
        if ((int)game.npc_atlases.size() > 0) {
            ImGui::Text("Available: 0-%d", (int)game.npc_atlases.size()-1);
            ImGui::SliderInt("Sprite", &npc_sprite_id, -1, (int)game.npc_atlases.size()-1);
        }

        ImGui::Separator();
        ImGui::Text("Presets:");
        // Presets include sprite_atlas_id matching the order NPCs were loaded
        // -1 = no sprite (invisible), 0+ = index into npc_atlases
        struct NPCPreset { const char* name; bool hostile; bool battle; int hp; int atk; float spd; int sprite; };
        int max_sprite = (int)game.npc_atlases.size() - 1;
        NPCPreset presets[] = {
            {"Chicken",  false, false,  0,  0, 15, std::min(3, max_sprite)},
            {"Cow",      false, false,  0,  0, 10, std::min(4, max_sprite)},
            {"Pig",      false, false,  0,  0, 12, std::min(5, max_sprite)},
            {"Sheep",    false, false,  0,  0, 12, std::min(6, max_sprite)},
            {"Villager", false, false,  0,  0, 20, std::min(0, max_sprite)},
            {"Slime",    true,  true,  30,  6, 30, std::min(2, max_sprite)},
            {"Skeleton", true,  true,  50, 12, 45, std::min(1, max_sprite)},
            {"Guard",    false, false,  0,  0, 25, std::min(0, max_sprite)},
        };
        for (int i = 0; i < 8; i++) {
            if (i % 4 != 0) ImGui::SameLine();
            if (ImGui::SmallButton(presets[i].name)) {
                std::strncpy(npc_name, presets[i].name, sizeof(npc_name));
                npc_hostile = presets[i].hostile;
                npc_has_battle = presets[i].battle;
                npc_battle_hp = presets[i].hp;
                npc_battle_atk = presets[i].atk;
                npc_speed = presets[i].spd;
                npc_sprite_id = presets[i].sprite;
            }
        }

        // Helper lambda to spawn NPC at a position
        static bool click_spawn_mode = false;
        auto spawn_npc_at = [&](float wx, float wy) {
            NPC npc;
            npc.name = npc_name;
            npc.position = {wx, wy};
            npc.home_pos = npc.position;
            npc.wander_target = npc.position;
            npc.dir = npc_dir;
            npc.hostile = npc_hostile;
            npc.has_battle = npc_has_battle;
            npc.battle_enemy_name = npc_name;
            npc.battle_enemy_hp = npc_battle_hp;
            npc.battle_enemy_atk = npc_battle_atk;
            npc.move_speed = npc_speed;
            npc.aggro_range = npc_aggro;
            npc.sprite_atlas_id = npc_sprite_id;
            npc.dialogue = {{std::string(npc_name), "..."}};
            game.npcs.push_back(npc);
            // Generate map script line
            char script_line[512];
            std::snprintf(script_line, sizeof(script_line),
                "spawn_npc(\"%s\", %.0f, %.0f, %d, %s, %d, %d, %d, %.0f, %.0f)",
                npc_name, wx, wy, npc_dir,
                npc_hostile ? "true" : "false",
                npc_sprite_id, npc_battle_hp, npc_battle_atk,
                npc_speed, npc_aggro);
            append_map_script(script_line);
            set_status("Spawned NPC: " + std::string(npc_name));
        };

        ImGui::Separator();
        if (ImGui::Button("Spawn at Player", ImVec2(-1, 0))) {
            spawn_npc_at(game.player_pos.x, game.player_pos.y);
        }
        bool was_active = click_spawn_mode;
        if (was_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1));
        }
        if (ImGui::Button(click_spawn_mode ? "Click Map to Spawn (active)" : "Click Map to Spawn", ImVec2(-1, 0))) {
            click_spawn_mode = !click_spawn_mode;
        }
        if (was_active) {
            ImGui::PopStyleColor();
            // Intercept next map click for NPC placement
            if (!ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseClicked(0)) {
                ImVec2 mpos = ImGui::GetMousePos();
                Vec2 world = screen_to_world(mpos.x, mpos.y, game.camera);
                spawn_npc_at(world.x, world.y);
                click_spawn_mode = false;
            }
        }
        ImGui::Text("NPCs on map: %d", (int)game.npcs.size());
    }
    ImGui::End();
    } // show_npc_spawner_
#endif
}

} // namespace eb
