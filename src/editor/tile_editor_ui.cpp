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

// ═══════════════════════════════════════════════════════════════
// Helper: Write UI element changes back to HUDConfig so sync_hud_values
// uses updated values when the editor closes.
// ═══════════════════════════════════════════════════════════════

// Map child HUD elements to their parent group
static std::string hud_parent_group(const std::string& id) {
    // Player group
    if (id == "hud_player_bg" || id == "hud_name" || id == "hud_heart" ||
        id == "hud_hp" || id == "hud_hp_text" || id == "hud_coin" || id == "hud_gold")
        return "player";
    // Time group
    if (id == "hud_time_bg" || id == "hud_sun" || id == "hud_time" || id == "hud_period")
        return "time";
    // Survival group
    if (id == "hud_hunger" || id == "hud_thirst" || id == "hud_energy")
        return "survival";
    return "";  // Not a managed HUD element
}

static void sync_to_hud_config(GameState& game, const std::string& id) {
    auto& h = game.hud;
    float S = h.scale;
    if (S < 0.01f) S = 1.0f;

    auto find_panel = [&](const std::string& pid) -> ScriptUIPanel* {
        for (auto& p : game.script_ui.panels) if (p.id == pid) return &p;
        return nullptr;
    };
    auto find_bar = [&](const std::string& bid) -> ScriptUIBar* {
        for (auto& b : game.script_ui.bars) if (b.id == bid) return &b;
        return nullptr;
    };

    std::string group = hud_parent_group(id);

    // Player panel group — reverse the transform: HUD value = screen value / S
    if (group == "player") {
        if (auto* p = find_panel("hud_player_bg")) {
            h.player_x = p->position.x / S;
            h.player_y = p->position.y / S;
            h.player_w = p->width / S;
            h.player_h = p->height / S;
        }
        if (auto* b = find_bar("hud_hp")) {
            h.hp_bar_w = b->width / S;
            h.hp_bar_h = b->height / S;
        }
    }
    // Time panel — position is: tx = sw - h.time_x_offset * S
    else if (group == "time") {
        if (auto* p = find_panel("hud_time_bg")) {
            h.time_x_offset = (h.screen_w - p->position.x) / S;
            h.time_w = p->width / S;
            h.time_h = p->height / S;
        }
    }
}

// Redirect drags on HUD child elements to move the parent panel instead
static bool apply_hud_drag(GameState& game, const std::string& id, float dx, float dy) {
    std::string group = hud_parent_group(id);
    if (group.empty()) return false;  // Not a HUD element, use normal drag

    auto& h = game.hud;
    float S = h.scale;
    if (S < 0.01f) S = 1.0f;

    if (group == "player") {
        // Move the parent panel position in HUDConfig
        h.player_x += dx / S;
        h.player_y += dy / S;
        // Immediately reposition the parent and all children
        auto set_panel_pos = [&](const std::string& pid, float x, float y) {
            for (auto& p : game.script_ui.panels) if (p.id == pid) { p.position = {x, y}; return; }
        };
        auto set_label_pos = [&](const std::string& lid, float x, float y) {
            for (auto& l : game.script_ui.labels) if (l.id == lid) { l.position = {x, y}; return; }
        };
        auto set_image_pos = [&](const std::string& iid, float x, float y, float w = -1, float ih = -1) {
            for (auto& img : game.script_ui.images) if (img.id == iid) { img.position = {x, y}; if (w > 0) { img.width = w; img.height = ih; } return; }
        };
        auto set_bar_pos = [&](const std::string& bid, float x, float y) {
            for (auto& b : game.script_ui.bars) if (b.id == bid) { b.position = {x, y}; return; }
        };
        set_panel_pos("hud_player_bg", h.player_x * S, h.player_y * S);
        set_label_pos("hud_name", (h.player_x + 10) * S, (h.player_y + 6) * S);
        set_image_pos("hud_heart", (h.player_x + 8) * S, (h.player_y + 32) * S, 16 * S, 16 * S);
        set_bar_pos("hud_hp", (h.player_x + 28) * S, (h.player_y + 34) * S);
        set_label_pos("hud_hp_text", (h.player_x + 28 + h.hp_bar_w + 6) * S, (h.player_y + 32) * S);
        set_image_pos("hud_coin", (h.player_x + 28 + h.hp_bar_w + 6) * S, (h.player_y + 6) * S, 14 * S, 14 * S);
        set_label_pos("hud_gold", (h.player_x + 28 + h.hp_bar_w + 24) * S, (h.player_y + 6) * S);
        return true;
    }
    else if (group == "time") {
        // Move time panel offset
        h.time_x_offset -= dx / S;  // negative because offset is from right edge
        // Reposition
        float tx = h.screen_w - h.time_x_offset * S;
        auto set_panel_pos = [&](const std::string& pid, float x, float y) {
            for (auto& p : game.script_ui.panels) if (p.id == pid) { p.position = {x, y}; return; }
        };
        auto set_label_pos = [&](const std::string& lid, float x, float y) {
            for (auto& l : game.script_ui.labels) if (l.id == lid) { l.position = {x, y}; return; }
        };
        auto set_image_pos = [&](const std::string& iid, float x, float y, float w, float ih) {
            for (auto& img : game.script_ui.images) if (img.id == iid) { img.position = {x, y}; img.width = w; img.height = ih; return; }
        };
        set_panel_pos("hud_time_bg", tx, h.player_y * S);
        set_image_pos("hud_sun", tx + 8 * S, (h.player_y + 4) * S, 20 * S, 20 * S);
        set_label_pos("hud_time", tx + 32 * S, (h.player_y + 6) * S);
        set_label_pos("hud_period", tx + 32 * S, (h.player_y + 28) * S);
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// Helper: Find UI element under mouse in screen space
// ═══════════════════════════════════════════════════════════════

struct UIHitResult {
    std::string id;
    std::string type;  // "label", "panel", "bar", "image"
    float x, y, w, h;  // bounds
};

static UIHitResult hit_test_ui(GameState& game, float mx, float my) {
    // Test in reverse draw order (topmost first)
    // Images
    for (int i = (int)game.script_ui.images.size() - 1; i >= 0; i--) {
        auto& img = game.script_ui.images[i];
        if (!img.visible) continue;
        float iw = img.width * img.scale, ih = img.height * img.scale;
        if (mx >= img.position.x && mx <= img.position.x + iw &&
            my >= img.position.y && my <= img.position.y + ih) {
            return {img.id, "image", img.position.x, img.position.y, iw, ih};
        }
    }
    // Bars
    for (int i = (int)game.script_ui.bars.size() - 1; i >= 0; i--) {
        auto& b = game.script_ui.bars[i];
        if (!b.visible) continue;
        if (mx >= b.position.x && mx <= b.position.x + b.width &&
            my >= b.position.y && my <= b.position.y + b.height) {
            return {b.id, "bar", b.position.x, b.position.y, b.width, b.height};
        }
    }
    // Labels
    for (int i = (int)game.script_ui.labels.size() - 1; i >= 0; i--) {
        auto& l = game.script_ui.labels[i];
        if (!l.visible) continue;
        float lw = l.text.size() * 8.0f * l.scale; // approximate width
        float lh = 16.0f * l.scale;
        if (mx >= l.position.x && mx <= l.position.x + lw &&
            my >= l.position.y && my <= l.position.y + lh) {
            return {l.id, "label", l.position.x, l.position.y, lw, lh};
        }
    }
    // Panels (check last — they're backgrounds)
    for (int i = (int)game.script_ui.panels.size() - 1; i >= 0; i--) {
        auto& p = game.script_ui.panels[i];
        if (!p.visible) continue;
        float pw = p.width * p.scale, ph = p.height * p.scale;
        if (mx >= p.position.x && mx <= p.position.x + pw &&
            my >= p.position.y && my <= p.position.y + ph) {
            return {p.id, "panel", p.position.x, p.position.y, pw, ph};
        }
    }
    return {"", "", 0, 0, 0, 0};
}

// Resize handle hit test (returns edge: 0=none, 1=right, 2=bottom, 3=corner)
static int hit_resize_handle(float mx, float my, float ex, float ey, float ew, float eh, float grab = 6.0f) {
    bool on_right = std::abs(mx - (ex + ew)) < grab && my >= ey && my <= ey + eh;
    bool on_bottom = std::abs(my - (ey + eh)) < grab && mx >= ex && mx <= ex + ew;
    if (on_right && on_bottom) return 3; // corner
    if (on_right) return 1;
    if (on_bottom) return 2;
    return 0;
}

void TileEditor::render_imgui_ui_editor(GameState& game) {
#ifndef EB_ANDROID
    if (!show_ui_editor_) return;

    // ═══════════ VIEWPORT OVERLAY: Selection & Drag Handles ═══════════
    // Draw directly on the game viewport using ImGui's foreground draw list
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec2 mouse = ImGui::GetMousePos();
        float mx = mouse.x, my = mouse.y;
        bool mouse_over_imgui = ImGui::GetIO().WantCaptureMouse;

        // Draw highlight rectangle around all visible UI elements
        auto draw_element_outline = [&](float x, float y, float w, float h, const std::string& id, ImU32 color) {
            draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0, 0, 1.5f);
            // Resize handle (bottom-right corner)
            draw->AddRectFilled(ImVec2(x + w - 5, y + h - 5), ImVec2(x + w + 1, y + h + 1), color);
        };

        ImU32 col_normal = IM_COL32(100, 180, 255, 80);
        ImU32 col_selected = IM_COL32(255, 200, 60, 200);
        ImU32 col_hover = IM_COL32(100, 255, 100, 120);

        // Draw outlines for all UI elements
        for (auto& p : game.script_ui.panels) {
            if (!p.visible) continue;
            ImU32 col = (p.id == ui_selected_id_) ? col_selected : col_normal;
            draw_element_outline(p.position.x, p.position.y, p.width * p.scale, p.height * p.scale, p.id, col);
        }
        for (auto& b : game.script_ui.bars) {
            if (!b.visible) continue;
            ImU32 col = (b.id == ui_selected_id_) ? col_selected : col_normal;
            draw_element_outline(b.position.x, b.position.y, b.width, b.height, b.id, col);
        }
        for (auto& l : game.script_ui.labels) {
            if (!l.visible) continue;
            float lw = l.text.size() * 8.0f * l.scale;
            float lh = 16.0f * l.scale;
            ImU32 col = (l.id == ui_selected_id_) ? col_selected : col_normal;
            draw_element_outline(l.position.x, l.position.y, lw, lh, l.id, col);
        }
        for (auto& img : game.script_ui.images) {
            if (!img.visible) continue;
            ImU32 col = (img.id == ui_selected_id_) ? col_selected : col_normal;
            draw_element_outline(img.position.x, img.position.y, img.width * img.scale, img.height * img.scale, img.id, col);
        }

        // Hover highlight
        if (!mouse_over_imgui && !ui_drag_active_) {
            auto hit = hit_test_ui(game, mx, my);
            if (!hit.id.empty() && hit.id != ui_selected_id_) {
                draw->AddRect(ImVec2(hit.x, hit.y), ImVec2(hit.x + hit.w, hit.y + hit.h), col_hover, 0, 0, 2.0f);
                // Tooltip with element ID
                draw->AddRectFilled(ImVec2(mx + 12, my - 4), ImVec2(mx + 14 + hit.id.size() * 7.0f, my + 14), IM_COL32(0,0,0,200));
                draw->AddText(ImVec2(mx + 14, my - 2), IM_COL32(255,255,255,255), hit.id.c_str());
            }
        }

        // Click to select
        static float prev_drag_x = 0, prev_drag_y = 0;
        if (!mouse_over_imgui && ImGui::IsMouseClicked(0) && !ui_drag_active_) {
            auto hit = hit_test_ui(game, mx, my);
            if (!hit.id.empty()) {
                ui_selected_id_ = hit.id;
                ui_editor_type_ = hit.type;
                ui_drag_active_ = true;
                ui_drag_ox_ = mx - hit.x;
                ui_drag_oy_ = my - hit.y;
                prev_drag_x = hit.x;
                prev_drag_y = hit.y;
            } else {
                ui_selected_id_.clear();
                ui_editor_type_.clear();
            }
        }

        // Drag to move
        static int resize_edge = 0; // 0=move, 1=right, 2=bottom, 3=corner
        if (ui_drag_active_ && !ui_selected_id_.empty()) {
            if (ImGui::IsMouseDown(0)) {
                float nx = mx - ui_drag_ox_;
                float ny = my - ui_drag_oy_;
                // For HUD child elements, move the whole group via HUDConfig
                float dx = nx - prev_drag_x;
                float dy = ny - prev_drag_y;
                if (!apply_hud_drag(game, ui_selected_id_, dx, dy)) {
                    // Non-HUD element: move directly
                    for (auto& p : game.script_ui.panels) {
                        if (p.id == ui_selected_id_) { p.position = {nx, ny}; break; }
                    }
                    for (auto& b : game.script_ui.bars) {
                        if (b.id == ui_selected_id_) { b.position = {nx, ny}; break; }
                    }
                    for (auto& l : game.script_ui.labels) {
                        if (l.id == ui_selected_id_) { l.position = {nx, ny}; break; }
                    }
                    for (auto& img : game.script_ui.images) {
                        if (img.id == ui_selected_id_) { img.position = {nx, ny}; break; }
                    }
                }
                prev_drag_x = nx;
                prev_drag_y = ny;
            } else {
                // Mouse released — end drag, generate script
                ui_drag_active_ = false;
                // Write back to HUDConfig so sync_hud_values uses updated positions
                sync_to_hud_config(game, ui_selected_id_);
                // Find final position and generate SageLang for both X and Y
                auto gen_move = [&](const std::string& id, float x, float y) {
                    append_map_script("ui_set(\"" + id + "\", \"x\", " + std::to_string((int)x) + ")");
                    append_map_script("ui_set(\"" + id + "\", \"y\", " + std::to_string((int)y) + ")");
                };
                for (auto& p : game.script_ui.panels) { if (p.id == ui_selected_id_) { gen_move(p.id, p.position.x, p.position.y); break; } }
                for (auto& b : game.script_ui.bars) { if (b.id == ui_selected_id_) { gen_move(b.id, b.position.x, b.position.y); break; } }
                for (auto& l : game.script_ui.labels) { if (l.id == ui_selected_id_) { gen_move(l.id, l.position.x, l.position.y); break; } }
                for (auto& img : game.script_ui.images) { if (img.id == ui_selected_id_) { gen_move(img.id, img.position.x, img.position.y); break; } }
                set_status("Moved: " + ui_selected_id_);
            }
        }

        // Right-click drag to resize (panels and bars only)
        static bool resizing = false;
        static float resize_start_w = 0, resize_start_h = 0, resize_mx = 0, resize_my = 0;
        if (!mouse_over_imgui && ImGui::IsMouseClicked(1) && !resizing) {
            // Check if we're on a resize handle
            for (auto& p : game.script_ui.panels) {
                if (p.id != ui_selected_id_ || !p.visible) continue;
                float pw = p.width * p.scale, ph = p.height * p.scale;
                int edge = hit_resize_handle(mx, my, p.position.x, p.position.y, pw, ph);
                if (edge > 0) {
                    resizing = true;
                    resize_edge = edge;
                    resize_start_w = p.width;
                    resize_start_h = p.height;
                    resize_mx = mx;
                    resize_my = my;
                }
            }
            for (auto& b : game.script_ui.bars) {
                if (b.id != ui_selected_id_ || !b.visible) continue;
                int edge = hit_resize_handle(mx, my, b.position.x, b.position.y, b.width, b.height);
                if (edge > 0) {
                    resizing = true;
                    resize_edge = edge;
                    resize_start_w = b.width;
                    resize_start_h = b.height;
                    resize_mx = mx;
                    resize_my = my;
                }
            }
        }
        if (resizing && ImGui::IsMouseDown(1)) {
            float dx = mx - resize_mx, dy = my - resize_my;
            for (auto& p : game.script_ui.panels) {
                if (p.id != ui_selected_id_) continue;
                if (resize_edge == 1 || resize_edge == 3) p.width = std::max(20.0f, resize_start_w + dx / p.scale);
                if (resize_edge == 2 || resize_edge == 3) p.height = std::max(20.0f, resize_start_h + dy / p.scale);
            }
            for (auto& b : game.script_ui.bars) {
                if (b.id != ui_selected_id_) continue;
                if (resize_edge == 1 || resize_edge == 3) b.width = std::max(10.0f, resize_start_w + dx);
                if (resize_edge == 2 || resize_edge == 3) b.height = std::max(4.0f, resize_start_h + dy);
            }
        }
        if (resizing && !ImGui::IsMouseDown(1)) {
            resizing = false;
            sync_to_hud_config(game, ui_selected_id_);
            // Generate resize script
            for (auto& p : game.script_ui.panels) {
                if (p.id == ui_selected_id_) {
                    append_map_script("ui_set(\"" + p.id + "\", \"w\", " + std::to_string((int)p.width) + ")");
                    append_map_script("ui_set(\"" + p.id + "\", \"h\", " + std::to_string((int)p.height) + ")");
                    break;
                }
            }
            for (auto& b : game.script_ui.bars) {
                if (b.id == ui_selected_id_) {
                    append_map_script("ui_set(\"" + b.id + "\", \"w\", " + std::to_string((int)b.width) + ")");
                    append_map_script("ui_set(\"" + b.id + "\", \"h\", " + std::to_string((int)b.height) + ")");
                    break;
                }
            }
            set_status("Resized: " + ui_selected_id_);
        }

        // Selected element info bar at bottom of viewport
        if (!ui_selected_id_.empty()) {
            auto hit = hit_test_ui(game, 0, 0); // just for display, not used for position
            // Find actual bounds
            float sx = 0, sy = 0, sw = 0, sh = 0;
            for (auto& p : game.script_ui.panels) { if (p.id == ui_selected_id_) { sx = p.position.x; sy = p.position.y; sw = p.width; sh = p.height; break; } }
            for (auto& b : game.script_ui.bars) { if (b.id == ui_selected_id_) { sx = b.position.x; sy = b.position.y; sw = b.width; sh = b.height; break; } }
            for (auto& l : game.script_ui.labels) { if (l.id == ui_selected_id_) { sx = l.position.x; sy = l.position.y; sw = l.text.size() * 8.0f * l.scale; sh = 16.0f * l.scale; break; } }
            for (auto& img : game.script_ui.images) { if (img.id == ui_selected_id_) { sx = img.position.x; sy = img.position.y; sw = img.width * img.scale; sh = img.height * img.scale; break; } }

            char info[128];
            std::snprintf(info, sizeof(info), "%s [%s] pos(%.0f,%.0f) size(%.0f x %.0f)",
                ui_selected_id_.c_str(), ui_editor_type_.c_str(), sx, sy, sw, sh);
            ImVec2 ts = ImGui::CalcTextSize(info);
            float bx = (ImGui::GetIO().DisplaySize.x - ts.x) * 0.5f;
            float by = ImGui::GetIO().DisplaySize.y - 24;
            draw->AddRectFilled(ImVec2(bx - 6, by - 2), ImVec2(bx + ts.x + 6, by + ts.y + 2), IM_COL32(0,0,0,200));
            draw->AddText(ImVec2(bx, by), IM_COL32(255, 220, 80, 255), info);
        }
    }

    // ═══════════ SIDE PANEL: Component List & Properties ═══════════
    ImGui::SetNextWindowPos(ImVec2(200, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("UI / HUD Editor##ui_editor", &show_ui_editor_)) {

        // ── Create New Component ──
        if (ImGui::CollapsingHeader("Create Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            static char new_id[64] = "my_element";
            ImGui::InputText("ID", new_id, sizeof(new_id));
            static float new_x = 100, new_y = 100;
            ImGui::DragFloat("X##new", &new_x, 1, 0, 1920);
            ImGui::SameLine();
            ImGui::DragFloat("Y##new", &new_y, 1, 0, 1080);

            // Style for new panels
            static const char* new_panel_styles[] = {
                "panel_window", "panel_hud_wide", "panel_hud_sq", "panel_hud_sq2",
                "panel_large", "panel_scroll", "panel_window_lg", "panel_dialogue",
                "panel_mini", "panel_wide", "panel_wide2", "panel_dark", "panel_settings",
                "flat_grey", "flat_blue", "flat_orange", "flat_cream",
                "flat_grey_sm", "flat_blue_sm", "flat_orange_sm", "flat_dark", "flat_dark_tall"
            };
            static int new_style_idx = 0;
            ImGui::Combo("Panel Style", &new_style_idx, new_panel_styles, IM_ARRAYSIZE(new_panel_styles));

            if (ImGui::Button("+ Label")) {
                game.script_ui.labels.push_back({new_id, "New Label", {new_x, new_y}, {1,1,1,1}, 0.8f});
                append_map_script(std::string("ui_label(\"") + new_id + "\", \"New Label\", " + std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 1, 1, 1, 1)");
                ui_selected_id_ = new_id; ui_editor_type_ = "label";
                set_status("Created label: " + std::string(new_id));
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Panel")) {
                std::string style = new_panel_styles[new_style_idx];
                game.script_ui.panels.push_back({new_id, {new_x, new_y}, 200, 100, style});
                append_map_script(std::string("ui_panel(\"") + new_id + "\", " + std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 200, 100, \"" + style + "\")");
                ui_selected_id_ = new_id; ui_editor_type_ = "panel";
                set_status("Created panel: " + std::string(new_id));
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Bar")) {
                game.script_ui.bars.push_back({new_id, 75, 100, {new_x, new_y}, 120, 14, {0.2f,0.8f,0.2f,1}});
                append_map_script(std::string("ui_bar(\"") + new_id + "\", 75, 100, " + std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 120, 14, 0.2, 0.8, 0.2, 1)");
                ui_selected_id_ = new_id; ui_editor_type_ = "bar";
                set_status("Created bar: " + std::string(new_id));
            }
            // Icon for new images
            static const char* new_icon_presets[] = {
                "fi_0", "icon_sword", "icon_shield", "icon_potion", "icon_heart_red",
                "icon_gem_blue", "icon_gem_green", "icon_ring", "icon_star", "icon_coin",
                "icon_book", "icon_scroll", "icon_heart_orange"
            };
            static int new_icon_idx = 0;
            ImGui::Combo("Image Icon", &new_icon_idx, new_icon_presets, IM_ARRAYSIZE(new_icon_presets));

            ImGui::SameLine();
            if (ImGui::Button("+ Image")) {
                std::string icon = new_icon_presets[new_icon_idx];
                game.script_ui.images.push_back({new_id, {new_x, new_y}, 32, 32, icon});
                append_map_script(std::string("ui_image(\"") + new_id + "\", " + std::to_string((int)new_x) + ", " + std::to_string((int)new_y) + ", 32, 32, \"" + icon + "\")");
                ui_selected_id_ = new_id; ui_editor_type_ = "image";
                set_status("Created image: " + std::string(new_id));
            }
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f,0.8f,1,1), "Click components in viewport to select. Drag to move. Right-drag edges to resize.");
        ImGui::Separator();

        // ── Selected Element Properties ──
        if (!ui_selected_id_.empty()) {
            ImGui::TextColored(ImVec4(1,0.9f,0.4f,1), "Selected: %s [%s]", ui_selected_id_.c_str(), ui_editor_type_.c_str());
            ImGui::Separator();

            // Label properties
            for (auto& l : game.script_ui.labels) {
                if (l.id != ui_selected_id_) continue;
                char buf[256];
                std::strncpy(buf, l.text.c_str(), sizeof(buf) - 1); buf[sizeof(buf)-1] = 0;
                if (ImGui::InputText("Text", buf, sizeof(buf))) l.text = buf;
                ImGui::DragFloat2("Position", &l.position.x, 1, 0, 2000);
                ImGui::SliderFloat("Scale", &l.scale, 0.3f, 3.0f);
                ImGui::ColorEdit4("Color", &l.color.x);
                ImGui::SliderFloat("Opacity", &l.opacity, 0, 1);
                ImGui::SliderFloat("Rotation", &l.rotation, 0, 360);
                ImGui::SliderInt("Layer", &l.layer, 0, 20);
                ImGui::Checkbox("Visible", &l.visible);
                char cb[64] = {}; std::strncpy(cb, l.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) l.on_click = cb;
                if (ImGui::Button("Delete")) {
                    append_map_script("ui_remove(\"" + l.id + "\")");
                    game.script_ui.labels.erase(std::find_if(game.script_ui.labels.begin(), game.script_ui.labels.end(), [&](auto& x) { return x.id == ui_selected_id_; }));
                    ui_selected_id_.clear();
                }
                break;
            }

            // Panel properties
            for (auto& p : game.script_ui.panels) {
                if (p.id != ui_selected_id_) continue;
                if (ImGui::DragFloat2("Position", &p.position.x, 1, 0, 2000)) sync_to_hud_config(game, p.id);
                if (ImGui::DragFloat("Width", &p.width, 1, 20, 1000)) sync_to_hud_config(game, p.id);
                if (ImGui::DragFloat("Height", &p.height, 1, 20, 800)) sync_to_hud_config(game, p.id);
                // Style picker — combo of all panel regions from UI atlases
                {
                    static std::vector<std::string> panel_styles;
                    static bool styles_cached = false;
                    if (!styles_cached) {
                        if (game.ui_atlas) {
                            for (auto& n : game.ui_atlas->region_names())
                                if (n.find("panel") != std::string::npos || n.find("btn") != std::string::npos)
                                    panel_styles.push_back(n);
                        }
                        if (game.ui_flat_atlas) {
                            for (auto& n : game.ui_flat_atlas->region_names())
                                panel_styles.push_back(n);
                        }
                        styles_cached = true;
                    }
                    if (ImGui::BeginCombo("Style", p.sprite_region.c_str())) {
                        for (auto& s : panel_styles) {
                            bool selected = (p.sprite_region == s);
                            if (ImGui::Selectable(s.c_str(), selected))
                                p.sprite_region = s;
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::SliderFloat("Rotation", &p.rotation, 0, 360);
                ImGui::SliderFloat("Opacity", &p.opacity, 0, 1);
                ImGui::SliderFloat("Scale", &p.scale, 0.1f, 4.0f);
                ImGui::ColorEdit4("Tint", &p.color.x);
                ImGui::SliderInt("Layer", &p.layer, 0, 20);
                ImGui::Checkbox("Visible", &p.visible);
                char cb[64] = {}; std::strncpy(cb, p.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) p.on_click = cb;
                if (ImGui::Button("Delete")) {
                    append_map_script("ui_remove(\"" + p.id + "\")");
                    game.script_ui.panels.erase(std::find_if(game.script_ui.panels.begin(), game.script_ui.panels.end(), [&](auto& x) { return x.id == ui_selected_id_; }));
                    ui_selected_id_.clear();
                }
                break;
            }

            // Bar properties
            for (auto& b : game.script_ui.bars) {
                if (b.id != ui_selected_id_) continue;
                ImGui::DragFloat("Value", &b.value, 1, 0, b.max_value);
                ImGui::DragFloat("Max", &b.max_value, 1, 1, 10000);
                if (ImGui::DragFloat2("Position", &b.position.x, 1, 0, 2000)) sync_to_hud_config(game, b.id);
                if (ImGui::DragFloat("Width", &b.width, 1, 10, 500)) sync_to_hud_config(game, b.id);
                if (ImGui::DragFloat("Height", &b.height, 1, 4, 40)) sync_to_hud_config(game, b.id);
                ImGui::SliderFloat("Rotation", &b.rotation, 0, 360);
                ImGui::ColorEdit4("Bar Color", &b.color.x);
                ImGui::ColorEdit4("BG Color", &b.bg_color.x);
                ImGui::SliderFloat("Opacity", &b.opacity, 0, 1);
                ImGui::Checkbox("Show Text", &b.show_text);
                ImGui::Checkbox("Visible", &b.visible);
                if (ImGui::Button("Delete")) {
                    append_map_script("ui_remove(\"" + b.id + "\")");
                    game.script_ui.bars.erase(std::find_if(game.script_ui.bars.begin(), game.script_ui.bars.end(), [&](auto& x) { return x.id == ui_selected_id_; }));
                    ui_selected_id_.clear();
                }
                break;
            }

            // Image properties
            for (auto& img : game.script_ui.images) {
                if (img.id != ui_selected_id_) continue;
                ImGui::DragFloat2("Position", &img.position.x, 1, 0, 2000);
                ImGui::DragFloat("Width", &img.width, 1, 4, 256);
                ImGui::DragFloat("Height", &img.height, 1, 4, 256);
                // Icon picker — combo of icon regions + fantasy icon indices
                {
                    static std::vector<std::string> icon_styles;
                    static bool icons_cached = false;
                    if (!icons_cached) {
                        if (game.ui_atlas) {
                            for (auto& n : game.ui_atlas->region_names())
                                if (n.find("icon") != std::string::npos || n.find("arrow") != std::string::npos ||
                                    n.find("checkbox") != std::string::npos || n.find("cursor") != std::string::npos)
                                    icon_styles.push_back(n);
                        }
                        // Fantasy icons (fi_0 through fi_431)
                        if (game.fantasy_icons_atlas) {
                            int count = game.fantasy_icons_atlas->region_count();
                            for (int i = 0; i < count && i < 432; i++)
                                icon_styles.push_back("fi_" + std::to_string(i));
                        }
                        icons_cached = true;
                    }
                    if (ImGui::BeginCombo("Icon", img.icon_name.c_str())) {
                        // Filter box
                        static char icon_filter[32] = "";
                        ImGui::InputText("Filter", icon_filter, sizeof(icon_filter));
                        for (auto& s : icon_styles) {
                            if (icon_filter[0] && s.find(icon_filter) == std::string::npos) continue;
                            bool selected = (img.icon_name == s);
                            if (ImGui::Selectable(s.c_str(), selected))
                                img.icon_name = s;
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::ColorEdit4("Tint", &img.tint.x);
                ImGui::SliderFloat("Rotation", &img.rotation, 0, 360);
                ImGui::SliderFloat("Opacity", &img.opacity, 0, 1);
                ImGui::SliderFloat("Scale", &img.scale, 0.1f, 4.0f);
                ImGui::Checkbox("Flip H", &img.flip_h); ImGui::SameLine(); ImGui::Checkbox("Flip V", &img.flip_v);
                ImGui::SliderInt("Layer", &img.layer, 0, 20);
                ImGui::Checkbox("Visible", &img.visible);
                char cb[64] = {}; std::strncpy(cb, img.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) img.on_click = cb;
                if (ImGui::Button("Delete")) {
                    append_map_script("ui_remove(\"" + img.id + "\")");
                    game.script_ui.images.erase(std::find_if(game.script_ui.images.begin(), game.script_ui.images.end(), [&](auto& x) { return x.id == ui_selected_id_; }));
                    ui_selected_id_.clear();
                }
                break;
            }
        } else {
            ImGui::TextDisabled("No element selected. Click a component in the viewport.");
        }

        ImGui::Separator();

        // ── Component List (click to select) ──
        if (ImGui::CollapsingHeader(("All Components (" + std::to_string(
            game.script_ui.labels.size() + game.script_ui.panels.size() +
            game.script_ui.bars.size() + game.script_ui.images.size()) + ")").c_str())) {

            if (ImGui::TreeNode("Panels")) {
                for (auto& p : game.script_ui.panels) {
                    bool sel = (ui_selected_id_ == p.id);
                    if (ImGui::Selectable(p.id.c_str(), sel)) { ui_selected_id_ = p.id; ui_editor_type_ = "panel"; }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Labels")) {
                for (auto& l : game.script_ui.labels) {
                    bool sel = (ui_selected_id_ == l.id);
                    if (ImGui::Selectable(l.id.c_str(), sel)) { ui_selected_id_ = l.id; ui_editor_type_ = "label"; }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Bars")) {
                for (auto& b : game.script_ui.bars) {
                    bool sel = (ui_selected_id_ == b.id);
                    if (ImGui::Selectable(b.id.c_str(), sel)) { ui_selected_id_ = b.id; ui_editor_type_ = "bar"; }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Images")) {
                for (auto& img : game.script_ui.images) {
                    bool sel = (ui_selected_id_ == img.id);
                    if (ImGui::Selectable(img.id.c_str(), sel)) { ui_selected_id_ = img.id; ui_editor_type_ = "image"; }
                }
                ImGui::TreePop();
            }
        }

        // ── HUD Configuration ──
        if (ImGui::CollapsingHeader("HUD Layout")) {
            ImGui::SliderFloat("HUD Scale", &game.hud.scale, 0.5f, 3.0f);
            ImGui::Separator();
            ImGui::Text("Player Panel");
            ImGui::DragFloat2("Player Pos", &game.hud.player_x, 1, 0, 1000);
            ImGui::DragFloat2("Player Size", &game.hud.player_w, 1, 50, 500);
            ImGui::DragFloat("HP Bar W", &game.hud.hp_bar_w, 1, 50, 300);
            ImGui::Separator();
            ImGui::Text("Time Panel");
            ImGui::DragFloat("Time Offset (from right)", &game.hud.time_x_offset, 1, 50, 500);
            ImGui::Separator();
            ImGui::Text("Inventory Bar");
            ImGui::DragFloat("Slot Size", &game.hud.inv_slot_size, 1, 20, 80);
            ImGui::SliderInt("Max Slots", &game.hud.inv_max_slots, 4, 12);
            ImGui::DragFloat("Minimap Size", &game.hud.minimap_size, 1, 50, 300);
        }

        // ── Window Templates ──
        if (ImGui::CollapsingHeader("Templates")) {
            float sw = game.hud.screen_w, sh = game.hud.screen_h;
            if (ImGui::Button("Dialog Box")) {
                float bx = 40, by = sh - 160, bw = sw - 80, bh = 140;
                game.script_ui.panels.push_back({"dlg_bg", {bx, by}, bw, bh, "panel_window"});
                game.script_ui.labels.push_back({"dlg_text", "Hello, adventurer!", {bx + 20, by + 20}, {1,1,1,1}, 0.8f});
                append_map_script("ui_panel(\"dlg_bg\", " + std::to_string((int)bx) + ", " + std::to_string((int)by) + ", " + std::to_string((int)bw) + ", " + std::to_string((int)bh) + ", \"panel_window\")");
                append_map_script("ui_label(\"dlg_text\", \"Hello, adventurer!\", " + std::to_string((int)(bx+20)) + ", " + std::to_string((int)(by+20)) + ", 1, 1, 1, 1)");
            }
            ImGui::SameLine();
            if (ImGui::Button("Quest Tracker")) {
                float qx = sw - 230, qy = 80;
                game.script_ui.panels.push_back({"quest_bg", {qx, qy}, 220, 80, "panel_window"});
                game.script_ui.images.push_back({"quest_icon", {qx + 8, qy + 8}, 20, 20, "fi_119"});
                game.script_ui.labels.push_back({"quest_title", "Current Quest", {qx + 34, qy + 8}, {1,0.9f,0.4f,1}, 0.7f});
                game.script_ui.labels.push_back({"quest_desc", "Find the crystal", {qx + 14, qy + 34}, {0.85f,0.82f,0.75f,1}, 0.55f});
                append_map_script("ui_panel(\"quest_bg\", " + std::to_string((int)qx) + ", " + std::to_string((int)qy) + ", 220, 80, \"panel_window\")");
            }
            ImGui::SameLine();
            if (ImGui::Button("Status Bar")) {
                game.script_ui.panels.push_back({"status_bg", {10, 10}, 250, 50, "panel_hud_wide"});
                game.script_ui.labels.push_back({"status_label", "Status", {20, 18}, {1,0.9f,0.5f,1}, 0.7f});
                game.script_ui.bars.push_back({"status_bar", 75, 100, {20, 36}, 220, 10, {0.2f,0.8f,0.2f,1}});
                append_map_script("ui_panel(\"status_bg\", 10, 10, 250, 50, \"panel_hud_wide\")");
            }
        }
    }
    ImGui::End();

#endif
}

} // namespace eb
