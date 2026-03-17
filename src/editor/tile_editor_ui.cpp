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
            for (auto& img : game.script_ui.images) {
                if (img.id != ui_selected_id_ || !img.visible) continue;
                float iw = img.width * img.scale, ih = img.height * img.scale;
                int edge = hit_resize_handle(mx, my, img.position.x, img.position.y, iw, ih);
                if (edge > 0) {
                    resizing = true;
                    resize_edge = edge;
                    resize_start_w = img.width;
                    resize_start_h = img.height;
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
            for (auto& img : game.script_ui.images) {
                if (img.id != ui_selected_id_) continue;
                if (resize_edge == 1 || resize_edge == 3) img.width = std::max(4.0f, resize_start_w + dx / img.scale);
                if (resize_edge == 2 || resize_edge == 3) img.height = std::max(4.0f, resize_start_h + dy / img.scale);
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
            for (auto& img : game.script_ui.images) {
                if (img.id == ui_selected_id_) {
                    append_map_script("ui_set(\"" + img.id + "\", \"w\", " + std::to_string((int)img.width) + ")");
                    append_map_script("ui_set(\"" + img.id + "\", \"h\", " + std::to_string((int)img.height) + ")");
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
                append_map_script(std::string("ui_set(\"") + new_id + "\", \"scale\", 0.800)");
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

            // Script generation helpers — emit ui_set() on widget commit
            auto fmt_f = [](float v) -> std::string { char b[32]; std::snprintf(b, sizeof(b), "%.3f", v); return b; };
            auto emit_f = [&](const std::string& prop, float val) {
                append_map_script("ui_set(\"" + ui_selected_id_ + "\", \"" + prop + "\", " + fmt_f(val) + ")");
            };
            auto emit_i = [&](const std::string& prop, int val) {
                append_map_script("ui_set(\"" + ui_selected_id_ + "\", \"" + prop + "\", " + std::to_string(val) + ")");
            };
            auto emit_s = [&](const std::string& prop, const std::string& val) {
                append_map_script("ui_set(\"" + ui_selected_id_ + "\", \"" + prop + "\", \"" + val + "\")");
            };
            auto emit_b = [&](const std::string& prop, bool val) {
                append_map_script("ui_set(\"" + ui_selected_id_ + "\", \"" + prop + "\", " + std::string(val ? "true" : "false") + ")");
            };
            auto emit_pos = [&](float x, float y) { emit_i("x", (int)x); emit_i("y", (int)y); };
            auto emit_color = [&](const char* prefix, float r, float g, float b, float a) {
                std::string p(prefix);
                emit_f(p + "r", r); emit_f(p + "g", g); emit_f(p + "b", b); emit_f(p + "a", a);
            };
            // Shorthand: check last widget and emit on commit
            #define EMIT_ON_COMMIT(code) if (ImGui::IsItemDeactivatedAfterEdit()) { code; }

            // Label properties
            for (auto& l : game.script_ui.labels) {
                if (l.id != ui_selected_id_) continue;
                char buf[256];
                std::strncpy(buf, l.text.c_str(), sizeof(buf) - 1); buf[sizeof(buf)-1] = 0;
                if (ImGui::InputText("Text", buf, sizeof(buf))) l.text = buf;
                EMIT_ON_COMMIT(emit_s("text", l.text));
                ImGui::DragFloat2("Position", &l.position.x, 1, 0, 2000);
                EMIT_ON_COMMIT(emit_pos(l.position.x, l.position.y));
                ImGui::SliderFloat("Scale", &l.scale, 0.3f, 3.0f);
                EMIT_ON_COMMIT(emit_f("scale", l.scale));
                ImGui::ColorEdit4("Color", &l.color.x);
                EMIT_ON_COMMIT(emit_color("", l.color.x, l.color.y, l.color.z, l.color.w));
                ImGui::SliderFloat("Opacity", &l.opacity, 0, 1);
                EMIT_ON_COMMIT(emit_f("opacity", l.opacity));
                ImGui::SliderFloat("Rotation", &l.rotation, 0, 360);
                EMIT_ON_COMMIT(emit_f("rotation", l.rotation));
                ImGui::SliderInt("Layer", &l.layer, 0, 20);
                EMIT_ON_COMMIT(emit_i("layer", l.layer));
                if (ImGui::Checkbox("Visible", &l.visible)) emit_b("visible", l.visible);
                char cb[64] = {}; std::strncpy(cb, l.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) l.on_click = cb;
                EMIT_ON_COMMIT(emit_s("on_click", l.on_click));
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
                EMIT_ON_COMMIT(emit_pos(p.position.x, p.position.y));
                if (ImGui::DragFloat("Width", &p.width, 1, 20, 1000)) sync_to_hud_config(game, p.id);
                EMIT_ON_COMMIT(emit_i("w", (int)p.width));
                if (ImGui::DragFloat("Height", &p.height, 1, 20, 800)) sync_to_hud_config(game, p.id);
                EMIT_ON_COMMIT(emit_i("h", (int)p.height));
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
                    std::string prev_style = p.sprite_region;
                    if (ImGui::BeginCombo("Style", p.sprite_region.c_str())) {
                        for (auto& s : panel_styles) {
                            bool selected = (p.sprite_region == s);
                            if (ImGui::Selectable(s.c_str(), selected))
                                p.sprite_region = s;
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (p.sprite_region != prev_style) emit_s("sprite", p.sprite_region);
                }
                ImGui::SliderFloat("Rotation", &p.rotation, 0, 360);
                EMIT_ON_COMMIT(emit_f("rotation", p.rotation));
                ImGui::SliderFloat("Opacity", &p.opacity, 0, 1);
                EMIT_ON_COMMIT(emit_f("opacity", p.opacity));
                ImGui::SliderFloat("Scale", &p.scale, 0.1f, 4.0f);
                EMIT_ON_COMMIT(emit_f("scale", p.scale));
                ImGui::ColorEdit4("Tint", &p.color.x);
                EMIT_ON_COMMIT(emit_color("", p.color.x, p.color.y, p.color.z, p.color.w));
                ImGui::SliderInt("Layer", &p.layer, 0, 20);
                EMIT_ON_COMMIT(emit_i("layer", p.layer));
                if (ImGui::Checkbox("9-Slice", &p.nine_slice)) emit_b("nine_slice", p.nine_slice);
                if (p.nine_slice) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120);
                    ImGui::SliderFloat("Border##9s", &p.border, 2, 64);
                    EMIT_ON_COMMIT(emit_f("border", p.border));
                }
                if (ImGui::Checkbox("Visible", &p.visible)) emit_b("visible", p.visible);
                char cb[64] = {}; std::strncpy(cb, p.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) p.on_click = cb;
                EMIT_ON_COMMIT(emit_s("on_click", p.on_click));
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
                EMIT_ON_COMMIT(emit_f("value", b.value));
                ImGui::DragFloat("Max", &b.max_value, 1, 1, 10000);
                EMIT_ON_COMMIT(emit_f("max", b.max_value));
                if (ImGui::DragFloat2("Position", &b.position.x, 1, 0, 2000)) sync_to_hud_config(game, b.id);
                EMIT_ON_COMMIT(emit_pos(b.position.x, b.position.y));
                if (ImGui::DragFloat("Width", &b.width, 1, 10, 500)) sync_to_hud_config(game, b.id);
                EMIT_ON_COMMIT(emit_i("w", (int)b.width));
                if (ImGui::DragFloat("Height", &b.height, 1, 4, 40)) sync_to_hud_config(game, b.id);
                EMIT_ON_COMMIT(emit_i("h", (int)b.height));
                ImGui::SliderFloat("Rotation", &b.rotation, 0, 360);
                EMIT_ON_COMMIT(emit_f("rotation", b.rotation));
                ImGui::ColorEdit4("Bar Color", &b.color.x);
                EMIT_ON_COMMIT(emit_color("", b.color.x, b.color.y, b.color.z, b.color.w));
                ImGui::ColorEdit4("BG Color", &b.bg_color.x);
                EMIT_ON_COMMIT(emit_color("bg_", b.bg_color.x, b.bg_color.y, b.bg_color.z, b.bg_color.w));
                ImGui::SliderFloat("Opacity", &b.opacity, 0, 1);
                EMIT_ON_COMMIT(emit_f("opacity", b.opacity));
                if (ImGui::Checkbox("Show Text", &b.show_text)) emit_b("show_text", b.show_text);
                if (ImGui::Checkbox("Visible", &b.visible)) emit_b("visible", b.visible);
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
                EMIT_ON_COMMIT(emit_pos(img.position.x, img.position.y));
                ImGui::DragFloat("Width", &img.width, 1, 4, 256);
                EMIT_ON_COMMIT(emit_i("w", (int)img.width));
                ImGui::DragFloat("Height", &img.height, 1, 4, 256);
                EMIT_ON_COMMIT(emit_i("h", (int)img.height));
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
                    std::string prev_icon = img.icon_name;
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
                    if (img.icon_name != prev_icon) emit_s("icon", img.icon_name);
                }
                ImGui::ColorEdit4("Tint", &img.tint.x);
                EMIT_ON_COMMIT(emit_color("", img.tint.x, img.tint.y, img.tint.z, img.tint.w));
                ImGui::SliderFloat("Rotation", &img.rotation, 0, 360);
                EMIT_ON_COMMIT(emit_f("rotation", img.rotation));
                ImGui::SliderFloat("Opacity", &img.opacity, 0, 1);
                EMIT_ON_COMMIT(emit_f("opacity", img.opacity));
                ImGui::SliderFloat("Scale", &img.scale, 0.1f, 4.0f);
                EMIT_ON_COMMIT(emit_f("scale", img.scale));
                if (ImGui::Checkbox("Flip H", &img.flip_h)) emit_b("flip_h", img.flip_h);
                ImGui::SameLine();
                if (ImGui::Checkbox("Flip V", &img.flip_v)) emit_b("flip_v", img.flip_v);
                ImGui::SliderInt("Layer", &img.layer, 0, 20);
                EMIT_ON_COMMIT(emit_i("layer", img.layer));
                if (ImGui::Checkbox("Visible", &img.visible)) emit_b("visible", img.visible);
                char cb[64] = {}; std::strncpy(cb, img.on_click.c_str(), sizeof(cb)-1);
                if (ImGui::InputText("On Click", cb, sizeof(cb))) img.on_click = cb;
                EMIT_ON_COMMIT(emit_s("on_click", img.on_click));
                if (ImGui::Button("Delete")) {
                    append_map_script("ui_remove(\"" + img.id + "\")");
                    game.script_ui.images.erase(std::find_if(game.script_ui.images.begin(), game.script_ui.images.end(), [&](auto& x) { return x.id == ui_selected_id_; }));
                    ui_selected_id_.clear();
                }
                break;
            }
            #undef EMIT_ON_COMMIT
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
        if (ImGui::CollapsingHeader("Templates", ImGuiTreeNodeFlags_DefaultOpen)) {
            float sw = game.hud.screen_w, sh = game.hud.screen_h;
            // Helper lambdas for terse script generation
            auto si = [](int v) { return std::to_string(v); };
            auto sf = [](float v, int d = 2) { char b[32]; std::snprintf(b, sizeof(b), "%.*f", d, v); return std::string(b); };

            ImGui::TextColored(ImVec4(0.7f,0.85f,1,1), "Dialogs & Windows");

            // ── Dialog Box ──
            if (ImGui::Button("Dialog Box")) {
                float bx = 40, by = sh - 160, bw = sw - 80, bh = 140;
                game.script_ui.panels.push_back({"dlg_bg", {bx, by}, bw, bh, "panel_dialogue"});
                game.script_ui.labels.push_back({"dlg_speaker", "NPC Name", {bx + 20, by + 12}, {1,0.85f,0.4f,1}, 0.75f});
                game.script_ui.labels.push_back({"dlg_text", "Hello, adventurer!", {bx + 20, by + 38}, {1,1,1,1}, 0.7f});
                append_map_script("ui_panel(\"dlg_bg\", " + si((int)bx) + ", " + si((int)by) + ", " + si((int)bw) + ", " + si((int)bh) + ", \"panel_dialogue\")");
                append_map_script("ui_label(\"dlg_speaker\", \"NPC Name\", " + si((int)(bx+20)) + ", " + si((int)(by+12)) + ", 1, 0.85, 0.4, 1)");
                append_map_script("ui_set(\"dlg_speaker\", \"scale\", 0.750)");
                append_map_script("ui_label(\"dlg_text\", \"Hello, adventurer!\", " + si((int)(bx+20)) + ", " + si((int)(by+38)) + ", 1, 1, 1, 1)");
                set_status("Created Dialog Box template");
            }
            ImGui::SameLine();

            // ── Confirm Dialog ──
            if (ImGui::Button("Confirm Dialog")) {
                float cx = (sw - 300) * 0.5f, cy = (sh - 140) * 0.5f;
                game.script_ui.panels.push_back({"confirm_bg", {cx, cy}, 300, 140, "panel_window"});
                game.script_ui.labels.push_back({"confirm_title", "Are you sure?", {cx + 20, cy + 14}, {1,0.95f,0.7f,1}, 0.8f});
                game.script_ui.labels.push_back({"confirm_desc", "This action cannot be undone.", {cx + 20, cy + 44}, {0.8f,0.8f,0.8f,1}, 0.6f});
                game.script_ui.panels.push_back({"confirm_yes", {cx + 30, cy + 90}, 100, 32, "btn_medium_normal"});
                game.script_ui.labels.push_back({"confirm_yes_t", "Yes", {cx + 62, cy + 96}, {0.2f,1,0.3f,1}, 0.7f});
                game.script_ui.panels.push_back({"confirm_no", {cx + 170, cy + 90}, 100, 32, "btn_medium_normal"});
                game.script_ui.labels.push_back({"confirm_no_t", "No", {cx + 206, cy + 96}, {1,0.3f,0.3f,1}, 0.7f});
                append_map_script("ui_panel(\"confirm_bg\", " + si((int)cx) + ", " + si((int)cy) + ", 300, 140, \"panel_window\")");
                append_map_script("ui_label(\"confirm_title\", \"Are you sure?\", " + si((int)(cx+20)) + ", " + si((int)(cy+14)) + ", 1, 0.95, 0.7, 1)");
                append_map_script("ui_set(\"confirm_title\", \"scale\", 0.800)");
                append_map_script("ui_label(\"confirm_desc\", \"This action cannot be undone.\", " + si((int)(cx+20)) + ", " + si((int)(cy+44)) + ", 0.8, 0.8, 0.8, 1)");
                append_map_script("ui_set(\"confirm_desc\", \"scale\", 0.600)");
                append_map_script("ui_panel(\"confirm_yes\", " + si((int)(cx+30)) + ", " + si((int)(cy+90)) + ", 100, 32, \"btn_medium_normal\")");
                append_map_script("ui_label(\"confirm_yes_t\", \"Yes\", " + si((int)(cx+62)) + ", " + si((int)(cy+96)) + ", 0.2, 1, 0.3, 1)");
                append_map_script("ui_panel(\"confirm_no\", " + si((int)(cx+170)) + ", " + si((int)(cy+90)) + ", 100, 32, \"btn_medium_normal\")");
                append_map_script("ui_label(\"confirm_no_t\", \"No\", " + si((int)(cx+206)) + ", " + si((int)(cy+96)) + ", 1, 0.3, 0.3, 1)");
                set_status("Created Confirm Dialog template");
            }
            ImGui::SameLine();

            // ── Notification Toast ──
            if (ImGui::Button("Toast")) {
                float tx = (sw - 320) * 0.5f, ty = 20;
                game.script_ui.panels.push_back({"toast_bg", {tx, ty}, 320, 44, "panel_hud_wide"});
                game.script_ui.images.push_back({"toast_icon", {tx + 10, ty + 10}, 24, 24, "fi_119"});
                game.script_ui.labels.push_back({"toast_text", "Achievement unlocked!", {tx + 42, ty + 14}, {1,0.95f,0.6f,1}, 0.65f});
                append_map_script("ui_panel(\"toast_bg\", " + si((int)tx) + ", " + si((int)ty) + ", 320, 44, \"panel_hud_wide\")");
                append_map_script("ui_image(\"toast_icon\", " + si((int)(tx+10)) + ", " + si((int)(ty+10)) + ", 24, 24, \"fi_119\")");
                append_map_script("ui_label(\"toast_text\", \"Achievement unlocked!\", " + si((int)(tx+42)) + ", " + si((int)(ty+14)) + ", 1, 0.95, 0.6, 1)");
                append_map_script("ui_set(\"toast_text\", \"scale\", 0.650)");
                set_status("Created Toast template");
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f,0.85f,1,1), "HUD Elements");

            // ── Quest Tracker ──
            if (ImGui::Button("Quest Tracker")) {
                float qx = sw - 230, qy = 80;
                game.script_ui.panels.push_back({"quest_bg", {qx, qy}, 220, 80, "panel_window"});
                game.script_ui.images.push_back({"quest_icon", {qx + 8, qy + 8}, 20, 20, "fi_119"});
                game.script_ui.labels.push_back({"quest_title", "Current Quest", {qx + 34, qy + 8}, {1,0.9f,0.4f,1}, 0.7f});
                game.script_ui.labels.push_back({"quest_desc", "Find the crystal", {qx + 14, qy + 34}, {0.85f,0.82f,0.75f,1}, 0.55f});
                append_map_script("ui_panel(\"quest_bg\", " + si((int)qx) + ", " + si((int)qy) + ", 220, 80, \"panel_window\")");
                append_map_script("ui_image(\"quest_icon\", " + si((int)(qx+8)) + ", " + si((int)(qy+8)) + ", 20, 20, \"fi_119\")");
                append_map_script("ui_label(\"quest_title\", \"Current Quest\", " + si((int)(qx+34)) + ", " + si((int)(qy+8)) + ", 1, 0.9, 0.4, 1)");
                append_map_script("ui_label(\"quest_desc\", \"Find the crystal\", " + si((int)(qx+14)) + ", " + si((int)(qy+34)) + ", 0.85, 0.82, 0.75, 1)");
                append_map_script("ui_set(\"quest_desc\", \"scale\", 0.550)");
                set_status("Created Quest Tracker template");
            }
            ImGui::SameLine();

            // ── Status Bar ──
            if (ImGui::Button("Status Bar")) {
                game.script_ui.panels.push_back({"status_bg", {10, 10}, 250, 50, "panel_hud_wide"});
                game.script_ui.labels.push_back({"status_label", "Status", {20, 18}, {1,0.9f,0.5f,1}, 0.7f});
                game.script_ui.bars.push_back({"status_bar", 75, 100, {20, 36}, 220, 10, {0.2f,0.8f,0.2f,1}});
                append_map_script("ui_panel(\"status_bg\", 10, 10, 250, 50, \"panel_hud_wide\")");
                append_map_script("ui_label(\"status_label\", \"Status\", 20, 18, 1, 0.9, 0.5, 1)");
                append_map_script("ui_bar(\"status_bar\", 75, 100, 20, 36, 220, 10, 0.2, 0.8, 0.2, 1)");
                set_status("Created Status Bar template");
            }
            ImGui::SameLine();

            // ── Boss HP Bar ──
            if (ImGui::Button("Boss HP")) {
                float bw = sw * 0.6f, bx = (sw - bw) * 0.5f, by = 16;
                game.script_ui.panels.push_back({"boss_frame", {bx - 8, by - 6}, bw + 16, 42, "panel_dark"});
                game.script_ui.labels.push_back({"boss_name", "BOSS NAME", {bx, by}, {1,0.3f,0.2f,1}, 0.6f});
                game.script_ui.bars.push_back({"boss_hp", 100, 100, {bx, by + 16}, bw, 16, {0.85f,0.15f,0.1f,1}});
                // Enable text on the bar
                for (auto& b : game.script_ui.bars) if (b.id == "boss_hp") b.show_text = true;
                append_map_script("ui_panel(\"boss_frame\", " + si((int)(bx-8)) + ", " + si((int)(by-6)) + ", " + si((int)(bw+16)) + ", 42, \"panel_dark\")");
                append_map_script("ui_label(\"boss_name\", \"BOSS NAME\", " + si((int)bx) + ", " + si((int)by) + ", 1, 0.3, 0.2, 1)");
                append_map_script("ui_set(\"boss_name\", \"scale\", 0.600)");
                append_map_script("ui_bar(\"boss_hp\", 100, 100, " + si((int)bx) + ", " + si((int)(by+16)) + ", " + si((int)bw) + ", 16, 0.85, 0.15, 0.1, 1)");
                append_map_script("ui_set(\"boss_hp\", \"show_text\", true)");
                set_status("Created Boss HP template");
            }

            // ── XP Bar (full-width bottom) ──
            if (ImGui::Button("XP Bar")) {
                float by = sh - 12;
                game.script_ui.bars.push_back({"xp_bar", 35, 100, {0, by}, sw, 12, {0.3f,0.5f,0.95f,1}});
                for (auto& b : game.script_ui.bars) if (b.id == "xp_bar") { b.bg_color = {0.1f,0.1f,0.15f,0.7f}; b.show_text = true; }
                game.script_ui.labels.push_back({"xp_label", "LV 5", {6, by - 14}, {0.6f,0.75f,1,1}, 0.5f});
                append_map_script("ui_bar(\"xp_bar\", 35, 100, 0, " + si((int)by) + ", " + si((int)sw) + ", 12, 0.3, 0.5, 0.95, 1)");
                append_map_script("ui_set(\"xp_bar\", \"bg_r\", 0.1)");
                append_map_script("ui_set(\"xp_bar\", \"bg_g\", 0.1)");
                append_map_script("ui_set(\"xp_bar\", \"bg_b\", 0.15)");
                append_map_script("ui_set(\"xp_bar\", \"bg_a\", 0.7)");
                append_map_script("ui_set(\"xp_bar\", \"show_text\", true)");
                append_map_script("ui_label(\"xp_label\", \"LV 5\", 6, " + si((int)(by-14)) + ", 0.6, 0.75, 1, 1)");
                append_map_script("ui_set(\"xp_label\", \"scale\", 0.500)");
                set_status("Created XP Bar template");
            }
            ImGui::SameLine();

            // ── Buff/Debuff Row ──
            if (ImGui::Button("Buff Row")) {
                float bx = sw - 180, by = sh - 50;
                game.script_ui.panels.push_back({"buff_bg", {bx - 4, by - 4}, 176, 40, "panel_mini"});
                for (int i = 0; i < 5; i++) {
                    std::string bid = "buff_" + std::to_string(i);
                    // Varied icons: sword, shield, potion, star, heart
                    const char* icons[] = {"fi_8", "fi_12", "fi_24", "fi_48", "fi_64"};
                    float ix = bx + i * 34;
                    game.script_ui.images.push_back({bid, {ix, by}, 30, 30, icons[i]});
                }
                append_map_script("ui_panel(\"buff_bg\", " + si((int)(bx-4)) + ", " + si((int)(by-4)) + ", 176, 40, \"panel_mini\")");
                for (int i = 0; i < 5; i++) {
                    std::string bid = "buff_" + std::to_string(i);
                    const char* icons[] = {"fi_8", "fi_12", "fi_24", "fi_48", "fi_64"};
                    float ix = bx + i * 34;
                    append_map_script("ui_image(\"" + bid + "\", " + si((int)ix) + ", " + si((int)by) + ", 30, 30, \"" + icons[i] + "\")");
                }
                set_status("Created Buff Row template");
            }
            ImGui::SameLine();

            // ── Location Banner ──
            if (ImGui::Button("Location Banner")) {
                float bw = 360, bx = (sw - bw) * 0.5f, by = 60;
                game.script_ui.panels.push_back({"loc_bg", {bx, by}, bw, 50, "panel_hud_wide"});
                game.script_ui.labels.push_back({"loc_name", "Mystic Forest", {bx + 20, by + 8}, {1,0.95f,0.75f,1}, 0.9f});
                game.script_ui.labels.push_back({"loc_sub", "Northern Reaches", {bx + 20, by + 30}, {0.7f,0.7f,0.65f,1}, 0.55f});
                // Set layer high so it appears on top
                for (auto& p : game.script_ui.panels) if (p.id == "loc_bg") p.layer = 10;
                for (auto& l : game.script_ui.labels) if (l.id == "loc_name" || l.id == "loc_sub") l.layer = 10;
                append_map_script("ui_panel(\"loc_bg\", " + si((int)bx) + ", " + si((int)by) + ", " + si((int)bw) + ", 50, \"panel_hud_wide\")");
                append_map_script("ui_set(\"loc_bg\", \"layer\", 10)");
                append_map_script("ui_label(\"loc_name\", \"Mystic Forest\", " + si((int)(bx+20)) + ", " + si((int)(by+8)) + ", 1, 0.95, 0.75, 1)");
                append_map_script("ui_set(\"loc_name\", \"scale\", 0.900)");
                append_map_script("ui_set(\"loc_name\", \"layer\", 10)");
                append_map_script("ui_label(\"loc_sub\", \"Northern Reaches\", " + si((int)(bx+20)) + ", " + si((int)(by+30)) + ", 0.7, 0.7, 0.65, 1)");
                append_map_script("ui_set(\"loc_sub\", \"scale\", 0.550)");
                append_map_script("ui_set(\"loc_sub\", \"layer\", 10)");
                set_status("Created Location Banner template");
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f,0.85f,1,1), "Character & Stats");

            // ── Character Stats Window ──
            if (ImGui::Button("Character Stats")) {
                float wx = 60, wy = 60, ww = 280, wh = 300;
                game.script_ui.panels.push_back({"stats_bg", {wx, wy}, ww, wh, "panel_window_lg"});
                // Portrait area
                game.script_ui.panels.push_back({"stats_portrait_bg", {wx + 14, wy + 14}, 64, 64, "panel_hud_sq"});
                game.script_ui.images.push_back({"stats_portrait", {wx + 22, wy + 22}, 48, 48, "fi_0"});
                // Name & Level
                game.script_ui.labels.push_back({"stats_name", "Hero", {wx + 90, wy + 18}, {1,0.95f,0.7f,1}, 0.85f});
                game.script_ui.labels.push_back({"stats_level", "Level 5 Warrior", {wx + 90, wy + 42}, {0.7f,0.75f,0.8f,1}, 0.6f});
                // HP Bar
                game.script_ui.labels.push_back({"stats_hp_l", "HP", {wx + 14, wy + 92}, {1,0.4f,0.3f,1}, 0.6f});
                game.script_ui.bars.push_back({"stats_hp", 80, 100, {wx + 50, wy + 92}, 200, 14, {0.85f,0.2f,0.15f,1}});
                for (auto& b : game.script_ui.bars) if (b.id == "stats_hp") b.show_text = true;
                // MP Bar
                game.script_ui.labels.push_back({"stats_mp_l", "MP", {wx + 14, wy + 114}, {0.3f,0.5f,1,1}, 0.6f});
                game.script_ui.bars.push_back({"stats_mp", 45, 60, {wx + 50, wy + 114}, 200, 14, {0.2f,0.35f,0.9f,1}});
                for (auto& b : game.script_ui.bars) if (b.id == "stats_mp") b.show_text = true;
                // XP Bar
                game.script_ui.labels.push_back({"stats_xp_l", "XP", {wx + 14, wy + 136}, {0.5f,0.8f,0.3f,1}, 0.6f});
                game.script_ui.bars.push_back({"stats_xp", 350, 1000, {wx + 50, wy + 136}, 200, 14, {0.4f,0.75f,0.25f,1}});
                for (auto& b : game.script_ui.bars) if (b.id == "stats_xp") b.show_text = true;
                // Stat lines
                const char* stat_names[] = {"ATK", "DEF", "SPD", "LCK"};
                const char* stat_vals[]  = {"12", "8", "10", "5"};
                float stat_colors[][3] = {{1,0.5f,0.3f}, {0.4f,0.7f,1}, {0.5f,1,0.5f}, {1,0.9f,0.3f}};
                for (int i = 0; i < 4; i++) {
                    float sy = wy + 170 + i * 28;
                    std::string sid = "stats_s" + std::to_string(i);
                    std::string vid = "stats_v" + std::to_string(i);
                    game.script_ui.images.push_back({sid + "_ico", {wx + 14, sy}, 18, 18, "fi_" + std::to_string(8 + i * 4)});
                    game.script_ui.labels.push_back({sid, stat_names[i], {wx + 38, sy + 2}, {stat_colors[i][0], stat_colors[i][1], stat_colors[i][2], 1}, 0.6f});
                    game.script_ui.labels.push_back({vid, stat_vals[i], {wx + 220, sy + 2}, {1,1,1,1}, 0.65f});
                }
                // Script generation
                append_map_script("ui_panel(\"stats_bg\", " + si((int)wx) + ", " + si((int)wy) + ", " + si((int)ww) + ", " + si((int)wh) + ", \"panel_window_lg\")");
                append_map_script("ui_panel(\"stats_portrait_bg\", " + si((int)(wx+14)) + ", " + si((int)(wy+14)) + ", 64, 64, \"panel_hud_sq\")");
                append_map_script("ui_image(\"stats_portrait\", " + si((int)(wx+22)) + ", " + si((int)(wy+22)) + ", 48, 48, \"fi_0\")");
                append_map_script("ui_label(\"stats_name\", \"Hero\", " + si((int)(wx+90)) + ", " + si((int)(wy+18)) + ", 1, 0.95, 0.7, 1)");
                append_map_script("ui_set(\"stats_name\", \"scale\", 0.850)");
                append_map_script("ui_label(\"stats_level\", \"Level 5 Warrior\", " + si((int)(wx+90)) + ", " + si((int)(wy+42)) + ", 0.7, 0.75, 0.8, 1)");
                append_map_script("ui_set(\"stats_level\", \"scale\", 0.600)");
                append_map_script("ui_label(\"stats_hp_l\", \"HP\", " + si((int)(wx+14)) + ", " + si((int)(wy+92)) + ", 1, 0.4, 0.3, 1)");
                append_map_script("ui_set(\"stats_hp_l\", \"scale\", 0.600)");
                append_map_script("ui_bar(\"stats_hp\", 80, 100, " + si((int)(wx+50)) + ", " + si((int)(wy+92)) + ", 200, 14, 0.85, 0.2, 0.15, 1)");
                append_map_script("ui_set(\"stats_hp\", \"show_text\", true)");
                append_map_script("ui_label(\"stats_mp_l\", \"MP\", " + si((int)(wx+14)) + ", " + si((int)(wy+114)) + ", 0.3, 0.5, 1, 1)");
                append_map_script("ui_set(\"stats_mp_l\", \"scale\", 0.600)");
                append_map_script("ui_bar(\"stats_mp\", 45, 60, " + si((int)(wx+50)) + ", " + si((int)(wy+114)) + ", 200, 14, 0.2, 0.35, 0.9, 1)");
                append_map_script("ui_set(\"stats_mp\", \"show_text\", true)");
                append_map_script("ui_label(\"stats_xp_l\", \"XP\", " + si((int)(wx+14)) + ", " + si((int)(wy+136)) + ", 0.5, 0.8, 0.3, 1)");
                append_map_script("ui_set(\"stats_xp_l\", \"scale\", 0.600)");
                append_map_script("ui_bar(\"stats_xp\", 350, 1000, " + si((int)(wx+50)) + ", " + si((int)(wy+136)) + ", 200, 14, 0.4, 0.75, 0.25, 1)");
                append_map_script("ui_set(\"stats_xp\", \"show_text\", true)");
                for (int i = 0; i < 4; i++) {
                    float sy = wy + 170 + i * 28;
                    std::string sid = "stats_s" + std::to_string(i);
                    std::string vid = "stats_v" + std::to_string(i);
                    append_map_script("ui_image(\"" + sid + "_ico\", " + si((int)(wx+14)) + ", " + si((int)sy) + ", 18, 18, \"fi_" + std::to_string(8 + i*4) + "\")");
                    append_map_script("ui_label(\"" + sid + "\", \"" + stat_names[i] + "\", " + si((int)(wx+38)) + ", " + si((int)(sy+2)) + ", " + sf(stat_colors[i][0]) + ", " + sf(stat_colors[i][1]) + ", " + sf(stat_colors[i][2]) + ", 1)");
                    append_map_script("ui_set(\"" + sid + "\", \"scale\", 0.600)");
                    append_map_script("ui_label(\"" + vid + "\", \"" + stat_vals[i] + "\", " + si((int)(wx+220)) + ", " + si((int)(sy+2)) + ", 1, 1, 1, 1)");
                    append_map_script("ui_set(\"" + vid + "\", \"scale\", 0.650)");
                }
                set_status("Created Character Stats template");
            }
            ImGui::SameLine();

            // ── Party HUD ──
            if (ImGui::Button("Party HUD")) {
                float px = sw - 200, py = sh - 200;
                game.script_ui.panels.push_back({"party_bg", {px, py}, 190, 190, "panel_window"});
                const char* names[] = {"Dean", "Sam", "Castiel"};
                float hp_vals[] = {85, 60, 45};
                float hp_maxs[] = {100, 100, 80};
                float colors[][3] = {{0.2f,0.85f,0.3f}, {0.9f,0.7f,0.1f}, {0.3f,0.6f,0.95f}};
                append_map_script("ui_panel(\"party_bg\", " + si((int)px) + ", " + si((int)py) + ", 190, 190, \"panel_window\")");
                for (int i = 0; i < 3; i++) {
                    float my = py + 12 + i * 58;
                    std::string pre = "party_" + std::to_string(i);
                    game.script_ui.images.push_back({pre + "_ico", {px + 10, my}, 24, 24, "fi_" + std::to_string(i)});
                    game.script_ui.labels.push_back({pre + "_name", names[i], {px + 40, my + 2}, {1,0.95f,0.8f,1}, 0.6f});
                    game.script_ui.bars.push_back({pre + "_hp", hp_vals[i], hp_maxs[i], {px + 40, my + 22}, 136, 10, {colors[i][0], colors[i][1], colors[i][2], 1}});
                    game.script_ui.labels.push_back({pre + "_hpt", std::to_string((int)hp_vals[i]) + "/" + std::to_string((int)hp_maxs[i]), {px + 40, my + 36}, {0.7f,0.7f,0.7f,1}, 0.45f});
                    append_map_script("ui_image(\"" + pre + "_ico\", " + si((int)(px+10)) + ", " + si((int)my) + ", 24, 24, \"fi_" + std::to_string(i) + "\")");
                    append_map_script("ui_label(\"" + pre + "_name\", \"" + names[i] + "\", " + si((int)(px+40)) + ", " + si((int)(my+2)) + ", 1, 0.95, 0.8, 1)");
                    append_map_script("ui_set(\"" + pre + "_name\", \"scale\", 0.600)");
                    append_map_script("ui_bar(\"" + pre + "_hp\", " + si((int)hp_vals[i]) + ", " + si((int)hp_maxs[i]) + ", " + si((int)(px+40)) + ", " + si((int)(my+22)) + ", 136, 10, " + sf(colors[i][0]) + ", " + sf(colors[i][1]) + ", " + sf(colors[i][2]) + ", 1)");
                    append_map_script("ui_label(\"" + pre + "_hpt\", \"" + std::to_string((int)hp_vals[i]) + "/" + std::to_string((int)hp_maxs[i]) + "\", " + si((int)(px+40)) + ", " + si((int)(my+36)) + ", 0.7, 0.7, 0.7, 1)");
                    append_map_script("ui_set(\"" + pre + "_hpt\", \"scale\", 0.450)");
                }
                set_status("Created Party HUD template");
            }

            // ── Equipment Slots ──
            if (ImGui::Button("Equipment Slots")) {
                float ex = 80, ey = 100;
                game.script_ui.panels.push_back({"equip_bg", {ex, ey}, 220, 260, "panel_large"});
                game.script_ui.labels.push_back({"equip_title", "Equipment", {ex + 14, ey + 10}, {1,0.9f,0.5f,1}, 0.75f});
                const char* slot_names[] = {"Weapon", "Armor", "Helm", "Boots", "Ring", "Amulet"};
                const char* slot_icons[] = {"fi_8", "fi_12", "fi_16", "fi_20", "fi_48", "fi_52"};
                append_map_script("ui_panel(\"equip_bg\", " + si((int)ex) + ", " + si((int)ey) + ", 220, 260, \"panel_large\")");
                append_map_script("ui_label(\"equip_title\", \"Equipment\", " + si((int)(ex+14)) + ", " + si((int)(ey+10)) + ", 1, 0.9, 0.5, 1)");
                append_map_script("ui_set(\"equip_title\", \"scale\", 0.750)");
                for (int i = 0; i < 6; i++) {
                    int col = i % 2, row = i / 2;
                    float sx = ex + 14 + col * 104, sy = ey + 38 + row * 72;
                    std::string sid = "equip_" + std::to_string(i);
                    game.script_ui.panels.push_back({sid + "_bg", {sx, sy}, 90, 60, "panel_hud_sq2"});
                    game.script_ui.images.push_back({sid + "_ico", {sx + 33, sy + 4}, 24, 24, slot_icons[i]});
                    game.script_ui.labels.push_back({sid + "_name", slot_names[i], {sx + 8, sy + 34}, {0.75f,0.75f,0.7f,1}, 0.5f});
                    append_map_script("ui_panel(\"" + sid + "_bg\", " + si((int)sx) + ", " + si((int)sy) + ", 90, 60, \"panel_hud_sq2\")");
                    append_map_script("ui_image(\"" + sid + "_ico\", " + si((int)(sx+33)) + ", " + si((int)(sy+4)) + ", 24, 24, \"" + slot_icons[i] + "\")");
                    append_map_script("ui_label(\"" + sid + "_name\", \"" + slot_names[i] + "\", " + si((int)(sx+8)) + ", " + si((int)(sy+34)) + ", 0.75, 0.75, 0.7, 1)");
                    append_map_script("ui_set(\"" + sid + "_name\", \"scale\", 0.500)");
                }
                set_status("Created Equipment Slots template");
            }
            ImGui::SameLine();

            // ── Inventory Grid ──
            if (ImGui::Button("Inventory Grid")) {
                float ix = (sw - 320) * 0.5f, iy = (sh - 280) * 0.5f;
                game.script_ui.panels.push_back({"inv_bg", {ix, iy}, 320, 280, "panel_window_lg"});
                game.script_ui.labels.push_back({"inv_title", "Inventory", {ix + 14, iy + 10}, {1,0.95f,0.6f,1}, 0.8f});
                game.script_ui.labels.push_back({"inv_gold", "Gold: 1250", {ix + 200, iy + 12}, {1,0.85f,0.2f,1}, 0.6f});
                game.script_ui.images.push_back({"inv_coin", {ix + 182, iy + 10}, 16, 16, "icon_coin"});
                append_map_script("ui_panel(\"inv_bg\", " + si((int)ix) + ", " + si((int)iy) + ", 320, 280, \"panel_window_lg\")");
                append_map_script("ui_label(\"inv_title\", \"Inventory\", " + si((int)(ix+14)) + ", " + si((int)(iy+10)) + ", 1, 0.95, 0.6, 1)");
                append_map_script("ui_set(\"inv_title\", \"scale\", 0.800)");
                append_map_script("ui_label(\"inv_gold\", \"Gold: 1250\", " + si((int)(ix+200)) + ", " + si((int)(iy+12)) + ", 1, 0.85, 0.2, 1)");
                append_map_script("ui_set(\"inv_gold\", \"scale\", 0.600)");
                append_map_script("ui_image(\"inv_coin\", " + si((int)(ix+182)) + ", " + si((int)(iy+10)) + ", 16, 16, \"icon_coin\")");
                // 4x5 grid of item slots
                for (int r = 0; r < 5; r++) {
                    for (int c = 0; c < 4; c++) {
                        int idx = r * 4 + c;
                        float sx = ix + 16 + c * 72, sy = iy + 38 + r * 46;
                        std::string sid = "inv_slot_" + std::to_string(idx);
                        game.script_ui.panels.push_back({sid, {sx, sy}, 64, 40, "panel_mini"});
                        append_map_script("ui_panel(\"" + sid + "\", " + si((int)sx) + ", " + si((int)sy) + ", 64, 40, \"panel_mini\")");
                    }
                }
                set_status("Created Inventory Grid template");
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f,0.85f,1,1), "Menus & Overlays");

            // ── Pause Overlay ──
            if (ImGui::Button("Pause Menu")) {
                float cx = (sw - 240) * 0.5f, cy = (sh - 260) * 0.5f;
                game.script_ui.panels.push_back({"pause_bg", {cx, cy}, 240, 260, "panel_settings"});
                game.script_ui.labels.push_back({"pause_title", "PAUSED", {cx + 70, cy + 14}, {1,1,1,1}, 1.0f});
                const char* menu_items[] = {"Resume", "Settings", "Save Game", "Quit"};
                float menu_colors[][4] = {{0.3f,1,0.4f,1}, {0.7f,0.8f,1,1}, {1,0.9f,0.4f,1}, {1,0.4f,0.35f,1}};
                append_map_script("ui_panel(\"pause_bg\", " + si((int)cx) + ", " + si((int)cy) + ", 240, 260, \"panel_settings\")");
                append_map_script("ui_label(\"pause_title\", \"PAUSED\", " + si((int)(cx+70)) + ", " + si((int)(cy+14)) + ", 1, 1, 1, 1)");
                for (int i = 0; i < 4; i++) {
                    float by = cy + 56 + i * 48;
                    std::string bid = "pause_btn_" + std::to_string(i);
                    std::string lid = "pause_lbl_" + std::to_string(i);
                    game.script_ui.panels.push_back({bid, {cx + 20, by}, 200, 36, "btn_large_normal"});
                    game.script_ui.labels.push_back({lid, menu_items[i], {cx + 70, by + 8}, {menu_colors[i][0], menu_colors[i][1], menu_colors[i][2], menu_colors[i][3]}, 0.7f});
                    append_map_script("ui_panel(\"" + bid + "\", " + si((int)(cx+20)) + ", " + si((int)by) + ", 200, 36, \"btn_large_normal\")");
                    append_map_script("ui_label(\"" + lid + "\", \"" + menu_items[i] + "\", " + si((int)(cx+70)) + ", " + si((int)(by+8)) + ", " + sf(menu_colors[i][0]) + ", " + sf(menu_colors[i][1]) + ", " + sf(menu_colors[i][2]) + ", 1)");
                }
                set_status("Created Pause Menu template");
            }
            ImGui::SameLine();

            // ── Title Screen ──
            if (ImGui::Button("Title Screen")) {
                float cx = (sw - 400) * 0.5f;
                game.script_ui.panels.push_back({"title_bg", {cx, 40}, 400, 120, "panel_large"});
                game.script_ui.labels.push_back({"title_name", "GAME TITLE", {cx + 60, 70}, {1,0.95f,0.7f,1}, 1.4f});
                game.script_ui.labels.push_back({"title_sub", "A Supernatural Adventure", {cx + 80, 118}, {0.7f,0.7f,0.65f,1}, 0.6f});
                float my = sh * 0.55f;
                const char* opts[] = {"New Game", "Continue", "Options"};
                append_map_script("ui_panel(\"title_bg\", " + si((int)cx) + ", 40, 400, 120, \"panel_large\")");
                append_map_script("ui_label(\"title_name\", \"GAME TITLE\", " + si((int)(cx+60)) + ", 70, 1, 0.95, 0.7, 1)");
                append_map_script("ui_set(\"title_name\", \"scale\", 1.400)");
                append_map_script("ui_label(\"title_sub\", \"A Supernatural Adventure\", " + si((int)(cx+80)) + ", 118, 0.7, 0.7, 0.65, 1)");
                append_map_script("ui_set(\"title_sub\", \"scale\", 0.600)");
                for (int i = 0; i < 3; i++) {
                    float by = my + i * 52;
                    std::string bid = "title_btn_" + std::to_string(i);
                    std::string lid = "title_lbl_" + std::to_string(i);
                    game.script_ui.panels.push_back({bid, {(sw - 180) * 0.5f, by}, 180, 38, "btn_large_normal"});
                    game.script_ui.labels.push_back({lid, opts[i], {(sw - 60) * 0.5f, by + 10}, {1,1,1,1}, 0.7f});
                    append_map_script("ui_panel(\"" + bid + "\", " + si((int)((sw-180)*0.5f)) + ", " + si((int)by) + ", 180, 38, \"btn_large_normal\")");
                    append_map_script("ui_label(\"" + lid + "\", \"" + opts[i] + "\", " + si((int)((sw-60)*0.5f)) + ", " + si((int)(by+10)) + ", 1, 1, 1, 1)");
                }
                set_status("Created Title Screen template");
            }
            ImGui::SameLine();

            // ── Settings Panel ──
            if (ImGui::Button("Settings Panel")) {
                float wx = (sw - 340) * 0.5f, wy = (sh - 300) * 0.5f;
                game.script_ui.panels.push_back({"set_bg", {wx, wy}, 340, 300, "panel_settings"});
                game.script_ui.labels.push_back({"set_title", "Settings", {wx + 120, wy + 12}, {1,1,1,1}, 0.9f});
                // Settings rows with labels and controls
                const char* settings[] = {"Music Volume", "SFX Volume", "Brightness", "Text Speed"};
                float defaults[] = {80, 70, 100, 50};
                append_map_script("ui_panel(\"set_bg\", " + si((int)wx) + ", " + si((int)wy) + ", 340, 300, \"panel_settings\")");
                append_map_script("ui_label(\"set_title\", \"Settings\", " + si((int)(wx+120)) + ", " + si((int)(wy+12)) + ", 1, 1, 1, 1)");
                append_map_script("ui_set(\"set_title\", \"scale\", 0.900)");
                for (int i = 0; i < 4; i++) {
                    float ry = wy + 50 + i * 54;
                    std::string lid = "set_lbl_" + std::to_string(i);
                    std::string bid = "set_bar_" + std::to_string(i);
                    std::string vid = "set_val_" + std::to_string(i);
                    game.script_ui.labels.push_back({lid, settings[i], {wx + 20, ry}, {0.85f,0.85f,0.8f,1}, 0.6f});
                    game.script_ui.bars.push_back({bid, defaults[i], 100, {wx + 20, ry + 22}, 240, 12, {0.3f,0.6f,0.9f,1}});
                    game.script_ui.labels.push_back({vid, std::to_string((int)defaults[i]) + "%", {wx + 270, ry + 20}, {0.7f,0.7f,0.7f,1}, 0.55f});
                    append_map_script("ui_label(\"" + lid + "\", \"" + settings[i] + "\", " + si((int)(wx+20)) + ", " + si((int)ry) + ", 0.85, 0.85, 0.8, 1)");
                    append_map_script("ui_set(\"" + lid + "\", \"scale\", 0.600)");
                    append_map_script("ui_bar(\"" + bid + "\", " + si((int)defaults[i]) + ", 100, " + si((int)(wx+20)) + ", " + si((int)(ry+22)) + ", 240, 12, 0.3, 0.6, 0.9, 1)");
                    append_map_script("ui_label(\"" + vid + "\", \"" + std::to_string((int)defaults[i]) + "%\", " + si((int)(wx+270)) + ", " + si((int)(ry+20)) + ", 0.7, 0.7, 0.7, 1)");
                    append_map_script("ui_set(\"" + vid + "\", \"scale\", 0.550)");
                }
                set_status("Created Settings Panel template");
            }

            // ── Tooltip ──
            if (ImGui::Button("Tooltip")) {
                float tx = sw * 0.5f, ty = sh * 0.4f;
                game.script_ui.panels.push_back({"tip_bg", {tx, ty}, 200, 80, "panel_mini"});
                game.script_ui.labels.push_back({"tip_name", "Iron Sword", {tx + 10, ty + 8}, {1,0.9f,0.4f,1}, 0.7f});
                game.script_ui.labels.push_back({"tip_type", "Weapon - Common", {tx + 10, ty + 28}, {0.6f,0.6f,0.6f,1}, 0.5f});
                game.script_ui.labels.push_back({"tip_desc", "ATK +5", {tx + 10, ty + 48}, {0.4f,0.9f,0.4f,1}, 0.55f});
                game.script_ui.images.push_back({"tip_icon", {tx + 160, ty + 8}, 28, 28, "fi_8"});
                // Set high layer for tooltip
                for (auto& p : game.script_ui.panels) if (p.id == "tip_bg") p.layer = 15;
                for (auto& l : game.script_ui.labels) if (l.id.substr(0, 4) == "tip_") l.layer = 15;
                for (auto& img : game.script_ui.images) if (img.id == "tip_icon") img.layer = 15;
                append_map_script("ui_panel(\"tip_bg\", " + si((int)tx) + ", " + si((int)ty) + ", 200, 80, \"panel_mini\")");
                append_map_script("ui_set(\"tip_bg\", \"layer\", 15)");
                append_map_script("ui_label(\"tip_name\", \"Iron Sword\", " + si((int)(tx+10)) + ", " + si((int)(ty+8)) + ", 1, 0.9, 0.4, 1)");
                append_map_script("ui_set(\"tip_name\", \"layer\", 15)");
                append_map_script("ui_label(\"tip_type\", \"Weapon - Common\", " + si((int)(tx+10)) + ", " + si((int)(ty+28)) + ", 0.6, 0.6, 0.6, 1)");
                append_map_script("ui_set(\"tip_type\", \"scale\", 0.500)");
                append_map_script("ui_set(\"tip_type\", \"layer\", 15)");
                append_map_script("ui_label(\"tip_desc\", \"ATK +5\", " + si((int)(tx+10)) + ", " + si((int)(ty+48)) + ", 0.4, 0.9, 0.4, 1)");
                append_map_script("ui_set(\"tip_desc\", \"scale\", 0.550)");
                append_map_script("ui_set(\"tip_desc\", \"layer\", 15)");
                append_map_script("ui_image(\"tip_icon\", " + si((int)(tx+160)) + ", " + si((int)(ty+8)) + ", 28, 28, \"fi_8\")");
                append_map_script("ui_set(\"tip_icon\", \"layer\", 15)");
                set_status("Created Tooltip template");
            }
            ImGui::SameLine();

            // ── Save Indicator ──
            if (ImGui::Button("Save Indicator")) {
                float sx = sw - 140, sy = sh - 40;
                game.script_ui.images.push_back({"save_icon", {sx, sy}, 20, 20, "fi_119"});
                game.script_ui.labels.push_back({"save_text", "Saving...", {sx + 26, sy + 2}, {1,1,1,0.85f}, 0.6f});
                for (auto& img : game.script_ui.images) if (img.id == "save_icon") img.layer = 18;
                for (auto& l : game.script_ui.labels) if (l.id == "save_text") l.layer = 18;
                append_map_script("ui_image(\"save_icon\", " + si((int)sx) + ", " + si((int)sy) + ", 20, 20, \"fi_119\")");
                append_map_script("ui_set(\"save_icon\", \"layer\", 18)");
                append_map_script("ui_label(\"save_text\", \"Saving...\", " + si((int)(sx+26)) + ", " + si((int)(sy+2)) + ", 1, 1, 1, 0.85)");
                append_map_script("ui_set(\"save_text\", \"scale\", 0.600)");
                append_map_script("ui_set(\"save_text\", \"layer\", 18)");
                set_status("Created Save Indicator template");
            }
            ImGui::SameLine();

            // ── Message Log ──
            if (ImGui::Button("Message Log")) {
                float lx = 10, ly = sh - 180;
                game.script_ui.panels.push_back({"log_bg", {lx, ly}, 320, 160, "panel_scroll"});
                for (int i = 0; i < 6; i++) {
                    float ry = ly + 8 + i * 24;
                    std::string lid = "log_" + std::to_string(i);
                    float fade = 1.0f - i * 0.12f;
                    game.script_ui.labels.push_back({lid, (i == 0 ? ">> Battle won! +50 XP" : i == 1 ? ">> Found Iron Sword" : i == 2 ? ">> Quest updated" : i == 3 ? ">> Sam joined the party" : i == 4 ? ">> Entered Mystic Forest" : ">> Game saved"), {lx + 10, ry}, {0.7f*fade, 0.8f*fade, 0.7f*fade, fade}, 0.5f});
                }
                // Set low layer so log stays behind other UI
                for (auto& p : game.script_ui.panels) if (p.id == "log_bg") { p.opacity = 0.75f; }
                append_map_script("ui_panel(\"log_bg\", " + si((int)lx) + ", " + si((int)ly) + ", 320, 160, \"panel_scroll\")");
                append_map_script("ui_set(\"log_bg\", \"opacity\", 0.750)");
                const char* log_msgs[] = {">> Battle won! +50 XP", ">> Found Iron Sword", ">> Quest updated", ">> Sam joined the party", ">> Entered Mystic Forest", ">> Game saved"};
                for (int i = 0; i < 6; i++) {
                    float ry = ly + 8 + i * 24;
                    std::string lid = "log_" + std::to_string(i);
                    float fade = 1.0f - i * 0.12f;
                    append_map_script("ui_label(\"" + lid + "\", \"" + log_msgs[i] + "\", " + si((int)(lx+10)) + ", " + si((int)ry) + ", " + sf(0.7f*fade) + ", " + sf(0.8f*fade) + ", " + sf(0.7f*fade) + ", " + sf(fade) + ")");
                    append_map_script("ui_set(\"" + lid + "\", \"scale\", 0.500)");
                }
                set_status("Created Message Log template");
            }

            // ── Minimap Frame ──
            if (ImGui::Button("Minimap Frame")) {
                float mx = sw - 150, my = sh - 150;
                game.script_ui.panels.push_back({"mmap_frame", {mx - 6, my - 6}, 142, 142, "panel_hud_sq"});
                game.script_ui.labels.push_back({"mmap_label", "MAP", {mx + 50, my - 4}, {0.7f,0.8f,0.9f,1}, 0.5f});
                game.script_ui.images.push_back({"mmap_n", {mx + 60, my + 2}, 12, 12, "arrow_up"});
                append_map_script("ui_panel(\"mmap_frame\", " + si((int)(mx-6)) + ", " + si((int)(my-6)) + ", 142, 142, \"panel_hud_sq\")");
                append_map_script("ui_label(\"mmap_label\", \"MAP\", " + si((int)(mx+50)) + ", " + si((int)(my-4)) + ", 0.7, 0.8, 0.9, 1)");
                append_map_script("ui_set(\"mmap_label\", \"scale\", 0.500)");
                append_map_script("ui_image(\"mmap_n\", " + si((int)(mx+60)) + ", " + si((int)(my+2)) + ", 12, 12, \"arrow_up\")");
                set_status("Created Minimap Frame template");
            }
            ImGui::SameLine();

            // ── Shop Window ──
            if (ImGui::Button("Shop Window")) {
                float wx = (sw - 380) * 0.5f, wy = (sh - 320) * 0.5f;
                game.script_ui.panels.push_back({"shop_bg", {wx, wy}, 380, 320, "panel_window_lg"});
                game.script_ui.labels.push_back({"shop_title", "General Store", {wx + 120, wy + 12}, {1,0.95f,0.6f,1}, 0.85f});
                game.script_ui.images.push_back({"shop_coin", {wx + 290, wy + 12}, 16, 16, "icon_coin"});
                game.script_ui.labels.push_back({"shop_gold", "1250", {wx + 310, wy + 12}, {1,0.85f,0.2f,1}, 0.65f});
                append_map_script("ui_panel(\"shop_bg\", " + si((int)wx) + ", " + si((int)wy) + ", 380, 320, \"panel_window_lg\")");
                append_map_script("ui_label(\"shop_title\", \"General Store\", " + si((int)(wx+120)) + ", " + si((int)(wy+12)) + ", 1, 0.95, 0.6, 1)");
                append_map_script("ui_set(\"shop_title\", \"scale\", 0.850)");
                append_map_script("ui_image(\"shop_coin\", " + si((int)(wx+290)) + ", " + si((int)(wy+12)) + ", 16, 16, \"icon_coin\")");
                append_map_script("ui_label(\"shop_gold\", \"1250\", " + si((int)(wx+310)) + ", " + si((int)(wy+12)) + ", 1, 0.85, 0.2, 1)");
                append_map_script("ui_set(\"shop_gold\", \"scale\", 0.650)");
                // Item rows
                const char* items[] = {"Health Potion", "Mana Potion", "Iron Sword", "Leather Armor"};
                const char* prices[] = {"50", "75", "200", "150"};
                const char* icons[] = {"fi_24", "fi_28", "fi_8", "fi_12"};
                for (int i = 0; i < 4; i++) {
                    float ry = wy + 46 + i * 64;
                    std::string pre = "shop_item_" + std::to_string(i);
                    game.script_ui.panels.push_back({pre + "_bg", {wx + 12, ry}, 356, 52, "panel_wide"});
                    game.script_ui.images.push_back({pre + "_ico", {wx + 22, ry + 10}, 32, 32, icons[i]});
                    game.script_ui.labels.push_back({pre + "_name", items[i], {wx + 62, ry + 8}, {1,1,1,1}, 0.65f});
                    game.script_ui.labels.push_back({pre + "_price", std::string(prices[i]) + "g", {wx + 62, ry + 30}, {1,0.85f,0.3f,1}, 0.5f});
                    game.script_ui.panels.push_back({pre + "_buy", {wx + 286, ry + 10}, 70, 30, "btn_small_normal"});
                    game.script_ui.labels.push_back({pre + "_buyt", "Buy", {wx + 308, ry + 16}, {0.3f,1,0.4f,1}, 0.6f});
                    append_map_script("ui_panel(\"" + pre + "_bg\", " + si((int)(wx+12)) + ", " + si((int)ry) + ", 356, 52, \"panel_wide\")");
                    append_map_script("ui_image(\"" + pre + "_ico\", " + si((int)(wx+22)) + ", " + si((int)(ry+10)) + ", 32, 32, \"" + icons[i] + "\")");
                    append_map_script("ui_label(\"" + pre + "_name\", \"" + items[i] + "\", " + si((int)(wx+62)) + ", " + si((int)(ry+8)) + ", 1, 1, 1, 1)");
                    append_map_script("ui_set(\"" + pre + "_name\", \"scale\", 0.650)");
                    append_map_script("ui_label(\"" + pre + "_price\", \"" + prices[i] + "g\", " + si((int)(wx+62)) + ", " + si((int)(ry+30)) + ", 1, 0.85, 0.3, 1)");
                    append_map_script("ui_set(\"" + pre + "_price\", \"scale\", 0.500)");
                    append_map_script("ui_panel(\"" + pre + "_buy\", " + si((int)(wx+286)) + ", " + si((int)(ry+10)) + ", 70, 30, \"btn_small_normal\")");
                    append_map_script("ui_label(\"" + pre + "_buyt\", \"Buy\", " + si((int)(wx+308)) + ", " + si((int)(ry+16)) + ", 0.3, 1, 0.4, 1)");
                    append_map_script("ui_set(\"" + pre + "_buyt\", \"scale\", 0.600)");
                }
                set_status("Created Shop Window template");
            }

            // ── Damage Numbers ──
            if (ImGui::Button("Damage Numbers")) {
                // Creates a set of floating damage number labels at various positions
                float cx = sw * 0.5f, cy = sh * 0.4f;
                const char* dmg_texts[] = {"-24", "MISS", "-8", "CRIT -52", "+15"};
                float dmg_colors[][4] = {{1,0.3f,0.2f,1}, {0.6f,0.6f,0.6f,0.8f}, {1,0.5f,0.3f,1}, {1,0.9f,0.1f,1}, {0.2f,1,0.3f,1}};
                float dmg_scales[] = {0.7f, 0.55f, 0.6f, 0.9f, 0.65f};
                float offsets[][2] = {{0,0}, {40,-30}, {-30,20}, {-10,-50}, {50,10}};
                for (int i = 0; i < 5; i++) {
                    std::string did = "dmg_" + std::to_string(i);
                    float dx = cx + offsets[i][0], dy = cy + offsets[i][1];
                    game.script_ui.labels.push_back({did, dmg_texts[i], {dx, dy},
                        {dmg_colors[i][0], dmg_colors[i][1], dmg_colors[i][2], dmg_colors[i][3]}, dmg_scales[i]});
                    for (auto& l : game.script_ui.labels) if (l.id == did) l.layer = 19;
                    append_map_script("ui_label(\"" + did + "\", \"" + dmg_texts[i] + "\", " + si((int)dx) + ", " + si((int)dy) + ", " + sf(dmg_colors[i][0]) + ", " + sf(dmg_colors[i][1]) + ", " + sf(dmg_colors[i][2]) + ", " + sf(dmg_colors[i][3]) + ")");
                    append_map_script("ui_set(\"" + did + "\", \"scale\", " + sf(dmg_scales[i], 3) + ")");
                    append_map_script("ui_set(\"" + did + "\", \"layer\", 19)");
                }
                set_status("Created Damage Numbers template");
            }
            ImGui::SameLine();

            // ── Interaction Prompt ──
            if (ImGui::Button("Interact Prompt")) {
                float px = (sw - 160) * 0.5f, py = sh * 0.6f;
                game.script_ui.panels.push_back({"interact_bg", {px, py}, 160, 36, "panel_mini"});
                game.script_ui.images.push_back({"interact_key", {px + 10, py + 6}, 22, 22, "btn_square"});
                game.script_ui.labels.push_back({"interact_k", "Z", {px + 17, py + 8}, {1,1,1,1}, 0.65f});
                game.script_ui.labels.push_back({"interact_t", "Talk to NPC", {px + 40, py + 10}, {0.9f,0.9f,0.85f,1}, 0.6f});
                for (auto& p : game.script_ui.panels) if (p.id == "interact_bg") p.layer = 12;
                for (auto& l : game.script_ui.labels) if (l.id.substr(0, 8) == "interact") l.layer = 12;
                for (auto& img : game.script_ui.images) if (img.id == "interact_key") img.layer = 12;
                append_map_script("ui_panel(\"interact_bg\", " + si((int)px) + ", " + si((int)py) + ", 160, 36, \"panel_mini\")");
                append_map_script("ui_set(\"interact_bg\", \"layer\", 12)");
                append_map_script("ui_image(\"interact_key\", " + si((int)(px+10)) + ", " + si((int)(py+6)) + ", 22, 22, \"btn_square\")");
                append_map_script("ui_set(\"interact_key\", \"layer\", 12)");
                append_map_script("ui_label(\"interact_k\", \"Z\", " + si((int)(px+17)) + ", " + si((int)(py+8)) + ", 1, 1, 1, 1)");
                append_map_script("ui_set(\"interact_k\", \"scale\", 0.650)");
                append_map_script("ui_set(\"interact_k\", \"layer\", 12)");
                append_map_script("ui_label(\"interact_t\", \"Talk to NPC\", " + si((int)(px+40)) + ", " + si((int)(py+10)) + ", 0.9, 0.9, 0.85, 1)");
                append_map_script("ui_set(\"interact_t\", \"scale\", 0.600)");
                append_map_script("ui_set(\"interact_t\", \"layer\", 12)");
                set_status("Created Interact Prompt template");
            }
        }
    }
    ImGui::End();

#endif
}

} // namespace eb
