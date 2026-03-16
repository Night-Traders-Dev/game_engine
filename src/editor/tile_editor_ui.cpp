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

void TileEditor::render_imgui_ui_editor(GameState& game) {
#ifndef EB_ANDROID
    // ═══════════ UI / HUD / WINDOW EDITOR (F6) ═══════════
    if (show_ui_editor_) {
        ImGui::SetNextWindowPos(ImVec2(200, 28), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 700), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("UI / HUD Editor##ui_editor", &show_ui_editor_)) {

            // ── Create New Component ──
            if (ImGui::CollapsingHeader("Create Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                static char new_id[64] = "my_element";
                ImGui::InputText("ID", new_id, sizeof(new_id));
                static float new_x = 100, new_y = 100;
                ImGui::DragFloat("X##new", &new_x, 1, 0, 1920);
                ImGui::DragFloat("Y##new", &new_y, 1, 0, 1080);

                if (ImGui::Button("+ Label")) {
                    game.script_ui.labels.push_back({new_id, "New Label", {new_x, new_y}, {1,1,1,1}, 0.8f});
                    append_map_script(std::string("ui_label(\"") + new_id + "\", \"New Label\", " +
                        std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 1, 1, 1, 1)");
                    set_status("Created label: " + std::string(new_id));
                }
                ImGui::SameLine();
                if (ImGui::Button("+ Panel")) {
                    game.script_ui.panels.push_back({new_id, {new_x, new_y}, 200, 100, "panel_window"});
                    append_map_script(std::string("ui_panel(\"") + new_id + "\", " +
                        std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 200, 100, \"panel_window\")");
                    set_status("Created panel: " + std::string(new_id));
                }
                ImGui::SameLine();
                if (ImGui::Button("+ Bar")) {
                    game.script_ui.bars.push_back({new_id, 75, 100, {new_x, new_y}, 120, 14, {0.2f,0.8f,0.2f,1}});
                    append_map_script(std::string("ui_bar(\"") + new_id + "\", 75, 100, " +
                        std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 120, 14, 0.2, 0.8, 0.2, 1)");
                    set_status("Created bar: " + std::string(new_id));
                }
                ImGui::SameLine();
                if (ImGui::Button("+ Image")) {
                    game.script_ui.images.push_back({new_id, {new_x, new_y}, 32, 32, "fi_0"});
                    append_map_script(std::string("ui_image(\"") + new_id + "\", " +
                        std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 32, 32, \"fi_0\")");
                    set_status("Created image: " + std::string(new_id));
                }
            }

            ImGui::Separator();

            // ── Labels ──
            if (ImGui::CollapsingHeader(("Labels (" + std::to_string(game.script_ui.labels.size()) + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < (int)game.script_ui.labels.size(); i++) {
                    auto& l = game.script_ui.labels[i];
                    ImGui::PushID(i + 10000);
                    bool selected = (ui_selected_id_ == l.id && ui_editor_type_ == "label");
                    if (ImGui::Selectable(l.id.c_str(), selected)) {
                        ui_selected_id_ = l.id; ui_editor_type_ = "label";
                    }
                    if (selected) {
                        char buf[256];
                        std::strncpy(buf, l.text.c_str(), sizeof(buf) - 1); buf[sizeof(buf)-1] = 0;
                        if (ImGui::InputText("Text", buf, sizeof(buf))) {
                            l.text = buf;
                            append_map_script("ui_set(\"" + l.id + "\", \"text\", \"" + l.text + "\")");
                        }
                        if (ImGui::DragFloat2("Pos##lbl", &l.position.x, 1, 0, 2000)) {
                            append_map_script("ui_set(\"" + l.id + "\", \"x\", " + std::to_string((int)l.position.x) + ")");
                            append_map_script("ui_set(\"" + l.id + "\", \"y\", " + std::to_string((int)l.position.y) + ")");
                        }
                        ImGui::SliderFloat("Scale##lbl", &l.scale, 0.3f, 3.0f);
                        ImGui::ColorEdit4("Color##lbl", &l.color.x);
                        ImGui::SliderFloat("Opacity##lbl", &l.opacity, 0, 1);
                        ImGui::SliderInt("Layer##lbl", &l.layer, 0, 20);
                        ImGui::Checkbox("Visible##lbl", &l.visible);
                        char click_buf[64] = {};
                        std::strncpy(click_buf, l.on_click.c_str(), sizeof(click_buf) - 1);
                        if (ImGui::InputText("On Click##lbl", click_buf, sizeof(click_buf))) l.on_click = click_buf;
                        if (ImGui::SmallButton("Delete##lbl")) {
                            append_map_script("ui_remove(\"" + l.id + "\")");
                            game.script_ui.labels.erase(game.script_ui.labels.begin() + i);
                            ui_selected_id_.clear();
                            ImGui::PopID(); break;
                        }
                    }
                    ImGui::PopID();
                }
            }

            // ── Panels ──
            if (ImGui::CollapsingHeader(("Panels (" + std::to_string(game.script_ui.panels.size()) + ")").c_str())) {
                for (int i = 0; i < (int)game.script_ui.panels.size(); i++) {
                    auto& p = game.script_ui.panels[i];
                    ImGui::PushID(i + 11000);
                    bool selected = (ui_selected_id_ == p.id && ui_editor_type_ == "panel");
                    if (ImGui::Selectable(p.id.c_str(), selected)) {
                        ui_selected_id_ = p.id; ui_editor_type_ = "panel";
                    }
                    if (selected) {
                        if (ImGui::DragFloat2("Pos##pnl", &p.position.x, 1, 0, 2000)) {
                            append_map_script("ui_set(\"" + p.id + "\", \"x\", " + std::to_string((int)p.position.x) + ")");
                            append_map_script("ui_set(\"" + p.id + "\", \"y\", " + std::to_string((int)p.position.y) + ")");
                        }
                        ImGui::DragFloat("Width##pnl", &p.width, 1, 10, 1000);
                        ImGui::DragFloat("Height##pnl", &p.height, 1, 10, 800);
                        char region[64] = {};
                        std::strncpy(region, p.sprite_region.c_str(), sizeof(region) - 1);
                        if (ImGui::InputText("Sprite Region##pnl", region, sizeof(region))) p.sprite_region = region;
                        ImGui::SliderFloat("Opacity##pnl", &p.opacity, 0, 1);
                        ImGui::SliderFloat("Scale##pnl", &p.scale, 0.1f, 4.0f);
                        ImGui::SliderInt("Layer##pnl", &p.layer, 0, 20);
                        ImGui::Checkbox("Visible##pnl", &p.visible);
                        char click_buf[64] = {};
                        std::strncpy(click_buf, p.on_click.c_str(), sizeof(click_buf) - 1);
                        if (ImGui::InputText("On Click##pnl", click_buf, sizeof(click_buf))) p.on_click = click_buf;
                        if (ImGui::SmallButton("Delete##pnl")) {
                            append_map_script("ui_remove(\"" + p.id + "\")");
                            game.script_ui.panels.erase(game.script_ui.panels.begin() + i);
                            ui_selected_id_.clear();
                            ImGui::PopID(); break;
                        }
                    }
                    ImGui::PopID();
                }
            }

            // ── Bars ──
            if (ImGui::CollapsingHeader(("Bars (" + std::to_string(game.script_ui.bars.size()) + ")").c_str())) {
                for (int i = 0; i < (int)game.script_ui.bars.size(); i++) {
                    auto& b = game.script_ui.bars[i];
                    ImGui::PushID(i + 12000);
                    bool selected = (ui_selected_id_ == b.id && ui_editor_type_ == "bar");
                    if (ImGui::Selectable(b.id.c_str(), selected)) {
                        ui_selected_id_ = b.id; ui_editor_type_ = "bar";
                    }
                    if (selected) {
                        ImGui::DragFloat("Value##bar", &b.value, 1, 0, b.max_value);
                        ImGui::DragFloat("Max##bar", &b.max_value, 1, 1, 10000);
                        if (ImGui::DragFloat2("Pos##bar", &b.position.x, 1, 0, 2000)) {
                            append_map_script("ui_set(\"" + b.id + "\", \"x\", " + std::to_string((int)b.position.x) + ")");
                        }
                        ImGui::DragFloat("Width##bar", &b.width, 1, 10, 500);
                        ImGui::DragFloat("Height##bar", &b.height, 1, 4, 40);
                        ImGui::ColorEdit4("Color##bar", &b.color.x);
                        ImGui::ColorEdit4("BG Color##bar", &b.bg_color.x);
                        ImGui::SliderFloat("Opacity##bar", &b.opacity, 0, 1);
                        ImGui::Checkbox("Show Text##bar", &b.show_text);
                        ImGui::Checkbox("Visible##bar", &b.visible);
                        if (ImGui::SmallButton("Delete##bar")) {
                            append_map_script("ui_remove(\"" + b.id + "\")");
                            game.script_ui.bars.erase(game.script_ui.bars.begin() + i);
                            ui_selected_id_.clear();
                            ImGui::PopID(); break;
                        }
                    }
                    ImGui::PopID();
                }
            }

            // ── Images ──
            if (ImGui::CollapsingHeader(("Images (" + std::to_string(game.script_ui.images.size()) + ")").c_str())) {
                for (int i = 0; i < (int)game.script_ui.images.size(); i++) {
                    auto& img = game.script_ui.images[i];
                    ImGui::PushID(i + 13000);
                    bool selected = (ui_selected_id_ == img.id && ui_editor_type_ == "image");
                    if (ImGui::Selectable(img.id.c_str(), selected)) {
                        ui_selected_id_ = img.id; ui_editor_type_ = "image";
                    }
                    if (selected) {
                        if (ImGui::DragFloat2("Pos##img", &img.position.x, 1, 0, 2000)) {
                            append_map_script("ui_set(\"" + img.id + "\", \"x\", " + std::to_string((int)img.position.x) + ")");
                        }
                        ImGui::DragFloat("Width##img", &img.width, 1, 4, 256);
                        ImGui::DragFloat("Height##img", &img.height, 1, 4, 256);
                        char icon[64] = {};
                        std::strncpy(icon, img.icon_name.c_str(), sizeof(icon) - 1);
                        if (ImGui::InputText("Icon##img", icon, sizeof(icon))) {
                            img.icon_name = icon;
                            append_map_script("ui_set(\"" + img.id + "\", \"icon\", \"" + img.icon_name + "\")");
                        }
                        ImGui::ColorEdit4("Tint##img", &img.tint.x);
                        ImGui::SliderFloat("Rotation##img", &img.rotation, 0, 360);
                        ImGui::SliderFloat("Opacity##img", &img.opacity, 0, 1);
                        ImGui::SliderFloat("Scale##img", &img.scale, 0.1f, 4.0f);
                        ImGui::Checkbox("Flip H##img", &img.flip_h);
                        ImGui::SameLine();
                        ImGui::Checkbox("Flip V##img", &img.flip_v);
                        ImGui::SliderInt("Layer##img", &img.layer, 0, 20);
                        ImGui::Checkbox("Visible##img", &img.visible);
                        char click_buf[64] = {};
                        std::strncpy(click_buf, img.on_click.c_str(), sizeof(click_buf) - 1);
                        if (ImGui::InputText("On Click##img", click_buf, sizeof(click_buf))) img.on_click = click_buf;
                        if (ImGui::SmallButton("Delete##img")) {
                            append_map_script("ui_remove(\"" + img.id + "\")");
                            game.script_ui.images.erase(game.script_ui.images.begin() + i);
                            ui_selected_id_.clear();
                            ImGui::PopID(); break;
                        }
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Separator();

            // ── HUD Configuration ──
            if (ImGui::CollapsingHeader("HUD Layout")) {
                ImGui::Text("Player Panel");
                ImGui::DragFloat2("Player Pos", &game.hud.player_x, 1, 0, 1000);
                ImGui::DragFloat2("Player Size", &game.hud.player_w, 1, 50, 500);
                ImGui::DragFloat("HP Bar W", &game.hud.hp_bar_w, 1, 50, 300);
                ImGui::DragFloat("HP Bar H", &game.hud.hp_bar_h, 1, 4, 30);
                ImGui::SliderFloat("Text Scale", &game.hud.text_scale, 0.5f, 2.0f);
                ImGui::Separator();
                ImGui::Text("Time Panel");
                ImGui::DragFloat("Time Offset (from right)", &game.hud.time_x_offset, 1, 50, 500);
                ImGui::DragFloat2("Time Size", &game.hud.time_w, 1, 50, 300);
                ImGui::Separator();
                ImGui::Text("Inventory Bar");
                ImGui::DragFloat("Slot Size", &game.hud.inv_slot_size, 1, 20, 80);
                ImGui::DragFloat("Padding", &game.hud.inv_padding, 0.5f, 0, 10);
                ImGui::SliderInt("Max Slots", &game.hud.inv_max_slots, 4, 12);
                ImGui::DragFloat("Y Offset (from bottom)", &game.hud.inv_y_offset, 1, 20, 200);
                ImGui::Separator();
                ImGui::Text("Global");
                ImGui::SliderFloat("HUD Scale", &game.hud.scale, 0.5f, 3.0f);
                ImGui::DragFloat("Minimap Size", &game.hud.minimap_size, 1, 50, 300);
                ImGui::Checkbox("Show Inventory", &game.hud.show_inventory);
                ImGui::Checkbox("Show Minimap", &game.hud.show_minimap);
                ImGui::Checkbox("Show Survival", &game.hud.show_survival);
            }

            // ── Window Templates ──
            if (ImGui::CollapsingHeader("Window Templates")) {
                ImGui::Text("Quick-create common UI layouts:");
                if (ImGui::Button("Dialog Box")) {
                    float sw = game.hud.screen_w, sh = game.hud.screen_h;
                    float bx = 40, by = sh - 160, bw = sw - 80, bh = 140;
                    std::string pid = "dlg_bg";
                    game.script_ui.panels.push_back({pid, {bx, by}, bw, bh, "panel_window"});
                    game.script_ui.labels.push_back({"dlg_text", "Hello, adventurer!", {bx + 20, by + 20}, {1,1,1,1}, 0.8f});
                    append_map_script("ui_panel(\"" + pid + "\", " + std::to_string((int)bx) + ", " + std::to_string((int)by) + ", " +
                        std::to_string((int)bw) + ", " + std::to_string((int)bh) + ", \"panel_window\")");
                    append_map_script("ui_label(\"dlg_text\", \"Hello, adventurer!\", " + std::to_string((int)(bx+20)) + ", " + std::to_string((int)(by+20)) + ", 1, 1, 1, 1)");
                    set_status("Created dialog box template");
                }
                ImGui::SameLine();
                if (ImGui::Button("Status Bar")) {
                    game.script_ui.panels.push_back({"status_bg", {10, 10}, 250, 50, "panel_hud_wide"});
                    game.script_ui.labels.push_back({"status_label", "Status", {20, 18}, {1,0.9f,0.5f,1}, 0.7f});
                    game.script_ui.bars.push_back({"status_bar", 75, 100, {20, 36}, 220, 10, {0.2f,0.8f,0.2f,1}});
                    append_map_script("ui_panel(\"status_bg\", 10, 10, 250, 50, \"panel_hud_wide\")");
                    append_map_script("ui_label(\"status_label\", \"Status\", 20, 18, 1, 0.9, 0.5, 1)");
                    append_map_script("ui_bar(\"status_bar\", 75, 100, 20, 36, 220, 10, 0.2, 0.8, 0.2, 1)");
                    set_status("Created status bar template");
                }
                ImGui::SameLine();
                if (ImGui::Button("Tooltip")) {
                    ImVec2 mouse = ImGui::GetMousePos();
                    float mx = mouse.x, my = mouse.y;
                    game.script_ui.panels.push_back({"tooltip_bg", {mx, my}, 160, 40, "panel_mini"});
                    game.script_ui.labels.push_back({"tooltip_text", "Tooltip text", {mx + 10, my + 10}, {1,1,0.9f,1}, 0.6f});
                    append_map_script("ui_panel(\"tooltip_bg\", " + std::to_string((int)mx) + ", " + std::to_string((int)my) + ", 160, 40, \"panel_mini\")");
                    append_map_script("ui_label(\"tooltip_text\", \"Tooltip text\", " + std::to_string((int)(mx+10)) + ", " + std::to_string((int)(my+10)) + ", 1, 1, 0.9, 1)");
                    set_status("Created tooltip template");
                }
                if (ImGui::Button("Quest Tracker")) {
                    float qx = game.hud.screen_w - 230, qy = 80;
                    game.script_ui.panels.push_back({"quest_bg", {qx, qy}, 220, 80, "panel_window"});
                    game.script_ui.images.push_back({"quest_icon", {qx + 8, qy + 8}, 20, 20, "fi_119"});
                    game.script_ui.labels.push_back({"quest_title", "Current Quest", {qx + 34, qy + 8}, {1,0.9f,0.4f,1}, 0.7f});
                    game.script_ui.labels.push_back({"quest_desc", "Find the crystal", {qx + 14, qy + 34}, {0.85f,0.82f,0.75f,1}, 0.55f});
                    append_map_script("ui_panel(\"quest_bg\", " + std::to_string((int)qx) + ", " + std::to_string((int)qy) + ", 220, 80, \"panel_window\")");
                    append_map_script("ui_image(\"quest_icon\", " + std::to_string((int)(qx+8)) + ", " + std::to_string((int)(qy+8)) + ", 20, 20, \"fi_119\")");
                    append_map_script("ui_label(\"quest_title\", \"Current Quest\", " + std::to_string((int)(qx+34)) + ", " + std::to_string((int)(qy+8)) + ", 1, 0.9, 0.4, 1)");
                    append_map_script("ui_label(\"quest_desc\", \"Find the crystal\", " + std::to_string((int)(qx+14)) + ", " + std::to_string((int)(qy+34)) + ", 0.85, 0.82, 0.75, 1)");
                    set_status("Created quest tracker template");
                }
                ImGui::SameLine();
                if (ImGui::Button("Notification")) {
                    float nx = game.hud.screen_w / 2 - 100, ny = 30;
                    game.script_ui.panels.push_back({"notify_bg", {nx, ny}, 200, 36, "panel_mini"});
                    game.script_ui.labels.push_back({"notify_text", "Achievement unlocked!", {nx + 10, ny + 8}, {1,1,0.7f,1}, 0.6f});
                    append_map_script("ui_panel(\"notify_bg\", " + std::to_string((int)nx) + ", " + std::to_string((int)ny) + ", 200, 36, \"panel_mini\")");
                    append_map_script("ui_label(\"notify_text\", \"Achievement unlocked!\", " + std::to_string((int)(nx+10)) + ", " + std::to_string((int)(ny+8)) + ", 1, 1, 0.7, 1)");
                    set_status("Created notification template");
                }
            }

            // ── SageLang Code Preview ──
            if (ImGui::CollapsingHeader("Generated SageLang")) {
                ImGui::TextWrapped("UI changes auto-generate SageLang in the map script. "
                    "Open the Script IDE (F3) to view/edit the companion .sage file.");
                int total = (int)(game.script_ui.labels.size() + game.script_ui.panels.size() +
                                  game.script_ui.bars.size() + game.script_ui.images.size());
                ImGui::Text("Active UI Components: %d", total);
                ImGui::Text("  Labels: %d", (int)game.script_ui.labels.size());
                ImGui::Text("  Panels: %d", (int)game.script_ui.panels.size());
                ImGui::Text("  Bars: %d", (int)game.script_ui.bars.size());
                ImGui::Text("  Images: %d", (int)game.script_ui.images.size());
            }
        }
        ImGui::End();
    }

#endif
}

} // namespace eb
