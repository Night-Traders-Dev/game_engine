#include "editor/tile_editor.h"
#include "game/game.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/texture.h"
#ifndef EB_ANDROID
#include <imgui.h>
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

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#ifndef EB_ANDROID
extern "C" {
#include <tinyfiledialogs.h>
}
#endif
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <queue>
#include <fstream>

namespace eb {

TileEditor::TileEditor() {
    tool_panel_.title = "Tools";
    tile_panel_.title = "Tiles";
}
TileEditor::~TileEditor() = default;

void TileEditor::set_map(TileMap* map) {
    map_ = map;
    selection_ = {};
    clipboard_ = {};
    last_paint_tile_ = {-1, -1};
    undo_stack_.clear();
    redo_stack_.clear();
}

void TileEditor::set_tileset(TextureAtlas* atlas, VkDescriptorSet desc, Texture* tex) {
    tileset_ = atlas; tileset_desc_ = desc;
    tileset_texture_ = tex ? tex : (atlas ? atlas->texture() : nullptr);
    imgui_texture_registered_ = false; // Re-register on next frame
}

void TileEditor::ensure_imgui_texture() {
#ifndef EB_ANDROID
    if (imgui_texture_registered_ || !tileset_texture_) return;
    imgui_tileset_id_ = (void*)ImGui_ImplVulkan_AddTexture(
        tileset_texture_->sampler(),
        tileset_texture_->image_view(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    imgui_texture_registered_ = true;
#endif
}

void TileEditor::set_text_renderer(TextRenderer* text, VkDescriptorSet font_desc) {
    text_ = text; font_desc_ = font_desc;
}

void TileEditor::set_status(const std::string& msg) {
    status_msg_ = msg; status_timer_ = 3.0f;
}

void TileEditor::init_panels(int screen_w, int screen_h) {
    tool_panel_.title = "Tools";
    tool_panel_.x = 10; tool_panel_.y = 10;
    tool_panel_.w = 200; tool_panel_.h = 340;

    tile_panel_.title = "Tiles";
    tile_panel_.x = screen_w - 220.0f; tile_panel_.y = 10;
    tile_panel_.w = 210; tile_panel_.h = screen_h * 0.55f;

    building_panel_.title = "Buildings";
    building_panel_.x = screen_w - 430.0f; building_panel_.y = 10;
    building_panel_.w = 200; building_panel_.h = 250;

    vehicle_panel_.title = "Vehicles";
    vehicle_panel_.x = screen_w - 430.0f; vehicle_panel_.y = 270;
    vehicle_panel_.w = 200; vehicle_panel_.h = 200;

    tree_panel_.title = "Trees";
    tree_panel_.x = screen_w - 220.0f; tree_panel_.y = tile_panel_.h + 20;
    tree_panel_.w = 210; tree_panel_.h = screen_h - tile_panel_.h - 60;

    misc_panel_.title = "Misc";
    misc_panel_.x = 10; misc_panel_.y = 360;
    misc_panel_.w = 200; misc_panel_.h = screen_h - 400;

    all_panels_ = {&tool_panel_, &tile_panel_, &building_panel_,
                   &vehicle_panel_, &tree_panel_, &misc_panel_};
    panels_initialized_ = true;
}

bool TileEditor::point_in_panel(const EditorPanel& p, float px, float py) const {
    return px >= p.x && px < p.x + p.w && py >= p.y && py < p.y + p.h;
}

bool TileEditor::point_in_panel_title(const EditorPanel& p, float px, float py) const {
    return px >= p.x && px < p.x + p.w && py >= p.y && py < p.y + PANEL_TITLE_H;
}

void TileEditor::render_panel_bg(SpriteBatch& batch, const EditorPanel& p) const {
    // Panel shadow
    batch.draw_quad({p.x+3, p.y+3}, {p.w, p.h}, {0,0,0,0.3f});
    // Panel body
    batch.draw_quad({p.x, p.y}, {p.w, p.h}, {0.12f,0.12f,0.16f,0.95f});
    // Title bar
    batch.draw_quad({p.x, p.y}, {p.w, PANEL_TITLE_H}, {0.18f,0.18f,0.24f,1.0f});
    // Border
    Vec4 bc = {0.35f,0.35f,0.45f,0.8f};
    batch.draw_quad({p.x, p.y}, {p.w, 1}, bc);
    batch.draw_quad({p.x, p.y+p.h-1}, {p.w, 1}, bc);
    batch.draw_quad({p.x, p.y}, {1, p.h}, bc);
    batch.draw_quad({p.x+p.w-1, p.y}, {1, p.h}, bc);
    // Title text
    if (text_) {
        text_->draw_text(batch, font_desc_, p.title,
                         Vec2(p.x + 8, p.y + 5), Vec4(0.8f,0.8f,0.9f,1), 1.6f);
    }
}

// ── Coordinate conversion ──

Vec2 TileEditor::screen_to_world(float sx, float sy, const Camera& camera) const {
    Vec2 cam_off = camera.offset();
    return {cam_off.x + sx / zoom_, cam_off.y + sy / zoom_};
}

Vec2i TileEditor::world_to_tile(Vec2 world) const {
    if (!map_) return {0, 0};
    int ts = map_->tile_size();
    return {(int)std::floor(world.x / ts), (int)std::floor(world.y / ts)};
}

// ── Undo/Redo ──

void TileEditor::begin_action(const std::string& desc) {
    current_action_ = {}; current_action_.description = desc;
    action_in_progress_ = true;
}

void TileEditor::record_tile_change(int layer, int x, int y, int old_t, int new_t) {
    if (!action_in_progress_) begin_action("edit");
    if (old_t != new_t) current_action_.tile_changes.push_back({layer, x, y, old_t, new_t});
}

void TileEditor::record_collision_change(int x, int y, CollisionType old_c, CollisionType new_c) {
    if (!action_in_progress_) begin_action("collision");
    if (old_c != new_c) current_action_.collision_changes.push_back({x, y, old_c, new_c});
}

void TileEditor::commit_action() {
    if (!action_in_progress_) return;
    if (!current_action_.tile_changes.empty() || !current_action_.collision_changes.empty() ||
        !current_action_.object_changes.empty()) {
        undo_stack_.push_back(std::move(current_action_));
        if ((int)undo_stack_.size() > MAX_UNDO) undo_stack_.erase(undo_stack_.begin());
        redo_stack_.clear();
    }
    current_action_ = {}; action_in_progress_ = false;
}

void TileEditor::undo() {
    if (undo_stack_.empty()) { set_status("Nothing to undo"); return; }
    auto& a = undo_stack_.back();
    for (auto it = a.tile_changes.rbegin(); it != a.tile_changes.rend(); ++it)
        map_->set_tile(it->layer, it->x, it->y, it->old_tile);
    for (auto it = a.collision_changes.rbegin(); it != a.collision_changes.rend(); ++it)
        map_->set_collision_at(it->x, it->y, it->old_type);
    // Undo object changes
    if (game_state_) {
        for (auto it = a.object_changes.rbegin(); it != a.object_changes.rend(); ++it) {
            if (it->added) {
                // Was added — remove it
                auto& objs = game_state_->world_objects;
                for (int i = (int)objs.size()-1; i >= 0; i--) {
                    if (objs[i].sprite_id == it->obj_id &&
                        std::abs(objs[i].position.x - it->x) < 1.0f &&
                        std::abs(objs[i].position.y - it->y) < 1.0f) {
                        objs.erase(objs.begin() + i);
                        break;
                    }
                }
            } else {
                // Was removed — add it back
                game_state_->world_objects.push_back({it->obj_id, {it->x, it->y}});
            }
        }
    }
    redo_stack_.push_back(std::move(a)); undo_stack_.pop_back();
    set_status("Undo");
}

void TileEditor::redo() {
    if (redo_stack_.empty()) { set_status("Nothing to redo"); return; }
    auto& a = redo_stack_.back();
    for (auto& tc : a.tile_changes) map_->set_tile(tc.layer, tc.x, tc.y, tc.new_tile);
    for (auto& cc : a.collision_changes) map_->set_collision_at(cc.x, cc.y, cc.new_type);
    // Redo object changes
    if (game_state_) {
        for (auto& oc : a.object_changes) {
            if (oc.added) {
                game_state_->world_objects.push_back({oc.obj_id, {oc.x, oc.y}});
            } else {
                auto& objs = game_state_->world_objects;
                for (int i = (int)objs.size()-1; i >= 0; i--) {
                    if (objs[i].sprite_id == oc.obj_id &&
                        std::abs(objs[i].position.x - oc.x) < 1.0f &&
                        std::abs(objs[i].position.y - oc.y) < 1.0f) {
                        objs.erase(objs.begin() + i);
                        break;
                    }
                }
            }
        }
    }
    undo_stack_.push_back(std::move(a)); redo_stack_.pop_back();
    set_status("Redo");
}

// ── Update ──

// Run zenity/kdialog directly via popen() to avoid forking our Vulkan process.
// This is safer than fork() which inherits GPU state that can corrupt on exit.
#ifdef __linux__
static std::string run_zenity_dialog(bool save, const char* default_path) {
    // Build zenity command
    std::string cmd;
    if (save) {
        cmd = "zenity --file-selection --save --filename=\"";
        cmd += default_path;
        cmd += "\" --file-filter=\"Map Files | *.json\" 2>/dev/null";
    } else {
        cmd = "zenity --file-selection --filename=\"";
        cmd += default_path;
        cmd += "\" --file-filter=\"Map Files | *.json\" 2>/dev/null";
    }

    // Run via popen — zenity runs as a completely separate process
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";

    char buf[4096] = {};
    if (fgets(buf, sizeof(buf), fp)) {
        // Strip trailing newline
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    }
    int status = pclose(fp);

    // zenity returns 0 on OK, 1 on cancel
    if (status == 0 && buf[0] != '\0') {
        return std::string(buf);
    }
    return "";
}
#endif

void TileEditor::process_pending_dialog() {
#ifndef EB_ANDROID
    if (pending_dialog_ == PendingDialog::None || !game_state_) return;

    PendingDialog action = pending_dialog_;
    pending_dialog_ = PendingDialog::None;

    // Drain GPU work before blocking
    if (renderer_) {
        renderer_->vulkan_context().wait_idle();
    }

    std::string path;
#ifdef __linux__
    // Use zenity directly via popen() — completely separate process, no fork of Vulkan state
    if (action == PendingDialog::Save) {
        path = run_zenity_dialog(true, "assets/maps/current.json");
    } else {
        path = run_zenity_dialog(false, "assets/maps/");
    }
#else
    // Windows/Mac — tinyfiledialogs works fine without fork
    const char* filters[] = {"*.json"};
    const char* result = nullptr;
    if (action == PendingDialog::Save) {
        result = tinyfd_saveFileDialog("Save Map", "assets/maps/current.json", 1, filters, "Map Files");
    } else {
        result = tinyfd_openFileDialog("Load Map", "assets/maps/", 1, filters, "Map Files", 0);
    }
    if (result) path = result;
#endif

    if (action == PendingDialog::ImportAsset) {
        // Import asset — use different dialog filters
        std::string import_path;
#ifdef __linux__
        FILE* fp = popen("zenity --file-selection --title='Import Asset' --file-filter='Images | *.png *.jpg *.bmp' 2>/dev/null", "r");
        if (fp) { char buf[4096]; if (fgets(buf, sizeof(buf), fp)) { import_path = buf; while (!import_path.empty() && (import_path.back()=='\n'||import_path.back()=='\r')) import_path.pop_back(); } pclose(fp); }
#else
        const char* img_filters[] = {"*.png", "*.jpg", "*.bmp"};
        const char* r = tinyfd_openFileDialog("Import Asset", "assets/textures/", 3, img_filters, "Image Files", 0);
        if (r) import_path = r;
#endif
        if (!import_path.empty()) {
            // Copy file to assets/textures/ if not already there
            std::string filename = import_path;
            auto slash = filename.rfind('/');
            if (slash != std::string::npos) filename = filename.substr(slash + 1);
            auto bslash = filename.rfind('\\');
            if (bslash != std::string::npos) filename = filename.substr(bslash + 1);

            std::string dest = "assets/textures/" + filename;
            if (import_path != dest) {
                std::ifstream src(import_path, std::ios::binary);
                std::ofstream dst(dest, std::ios::binary);
                if (src && dst) {
                    dst << src.rdbuf();
                    set_status("Imported: " + filename);
                    std::printf("[Editor] Imported asset: %s -> %s\n", import_path.c_str(), dest.c_str());
                } else {
                    set_status("Import failed!");
                }
            } else {
                set_status("Asset already in textures/");
            }
        }
        glfwPollEvents();
        return;
    }

    if (!path.empty()) {
        if (action == PendingDialog::Save) {
            save_map_file(*game_state_, path.c_str()) ? set_status("Saved") : set_status("Failed!");
        } else {
            load_map(path.c_str()) ? set_status("Loaded") : set_status("Failed!");
        }
    }

    // Process accumulated window events
    glfwPollEvents();
#endif
}

void TileEditor::update(const InputState& input, Camera& camera, float dt,
                         int screen_w, int screen_h) {
    if (!active_ || !map_) return;
    if (!panels_initialized_) init_panels(screen_w, screen_h);

    // Note: process_pending_dialog() is called from main.cpp before this
    mouse_x_ = input.mouse.x;
    mouse_y_ = input.mouse.y;
    if (status_timer_ > 0) status_timer_ -= dt;

    camera.set_viewport(screen_w / zoom_, screen_h / zoom_);

    handle_shortcuts(input);

    // Panel dragging
    bool any_dragging = false;
    for (auto* panel : all_panels_) {
        if (input.mouse.is_pressed(MouseButton::Left) && point_in_panel_title(*panel, mouse_x_, mouse_y_)) {
            panel->dragging = true;
            panel->drag_ox = mouse_x_ - panel->x;
            panel->drag_oy = mouse_y_ - panel->y;
        }
        if (panel->dragging && input.mouse.is_held(MouseButton::Left)) {
            panel->x = mouse_x_ - panel->drag_ox;
            panel->y = mouse_y_ - panel->drag_oy;
        }
        if (input.mouse.is_released(MouseButton::Left)) panel->dragging = false;
        if (panel->dragging) any_dragging = true;
    }

    bool over_any_panel = false;
    for (auto* panel : all_panels_) {
        if (point_in_panel(*panel, mouse_x_, mouse_y_)) { over_any_panel = true; break; }
    }
    bool over_status = mouse_y_ >= screen_h - STATUS_BAR_HEIGHT;
    // ImGui takes priority — if ImGui wants the mouse, don't send clicks to the map
    bool imgui_wants = ImGui::GetIO().WantCaptureMouse;
    bool over_ui = over_any_panel || over_status || any_dragging || imgui_wants;

    // Old panel-based tile palette disabled — ImGui handles all UI now

    // Zoom map
    if (!over_ui && input.mouse.scroll_y != 0.0f) {
        float old_zoom = zoom_;
        zoom_ *= (input.mouse.scroll_y > 0) ? 1.15f : (1.0f / 1.15f);
        zoom_ = std::clamp(zoom_, ZOOM_MIN, ZOOM_MAX);
        if (zoom_ != old_zoom) {
            Vec2 wb = screen_to_world(mouse_x_, mouse_y_, camera);
            camera.set_viewport(screen_w / zoom_, screen_h / zoom_);
            Vec2 wa = screen_to_world(mouse_x_, mouse_y_, camera);
            camera.set_position(Vec2(camera.position().x + wb.x - wa.x,
                                     camera.position().y + wb.y - wa.y));
        }
    }

    // Middle mouse: pan
    if (input.mouse.is_pressed(MouseButton::Middle)) {
        panning_ = true; pan_start_ = {mouse_x_, mouse_y_}; camera_start_ = camera.position();
    }
    if (panning_ && input.mouse.is_held(MouseButton::Middle)) {
        float dx = (mouse_x_ - pan_start_.x) / zoom_;
        float dy = (mouse_y_ - pan_start_.y) / zoom_;
        camera.set_position(Vec2(camera_start_.x - dx, camera_start_.y - dy));
    }
    if (input.mouse.is_released(MouseButton::Middle)) panning_ = false;

    // Map click (only if not over UI — ImGui handles all panel interactions)
    if (!over_ui) {
        if (input.mouse.is_pressed(MouseButton::Left)) {
            last_paint_tile_ = {-1, -1}; begin_action("paint");
            handle_map_click(mouse_x_, mouse_y_, camera, false);
        } else if (input.mouse.is_held(MouseButton::Left)) {
            handle_map_click(mouse_x_, mouse_y_, camera, true);
        }
        if (input.mouse.is_released(MouseButton::Left)) {
            // Commit Line/Rect on release
            if (tool_ == EditorTool::Line && selection_.x1 >= 0) {
                begin_action("line");
                commit_line(selection_.x1, selection_.y1, selection_.x2, selection_.y2, selected_tile_);
                commit_action();
                selection_.x1 = -1;
                set_status("Line drawn");
            } else if (tool_ == EditorTool::Rect && selection_.x1 >= 0) {
                begin_action("rect");
                commit_rect(selection_.x1, selection_.y1, selection_.x2, selection_.y2, selected_tile_, rect_filled_);
                commit_action();
                selection_.x1 = -1;
                set_status("Rect drawn");
            } else {
                last_paint_tile_ = {-1, -1}; commit_action();
            }
        }
        if (input.mouse.is_pressed(MouseButton::Right)) begin_action("erase");
        if (input.mouse.is_pressed(MouseButton::Right) || input.mouse.is_held(MouseButton::Right))
            handle_map_right_click(mouse_x_, mouse_y_, camera);
        if (input.mouse.is_released(MouseButton::Right)) commit_action();
    }
}

void TileEditor::handle_shortcuts(const InputState& input) {
    if (input.key_pressed(GLFW_KEY_P)) { tool_ = EditorTool::Paint; set_status("Paint"); }
    if (input.key_pressed(GLFW_KEY_E) && !input.mods.ctrl) { tool_ = EditorTool::Erase; set_status("Erase"); }
    if (input.key_pressed(GLFW_KEY_F)) { tool_ = EditorTool::Fill; set_status("Fill"); }
    if (input.key_pressed(GLFW_KEY_I)) { tool_ = EditorTool::Eyedrop; set_status("Eyedrop"); }
    if (input.key_pressed(GLFW_KEY_R)) { tool_ = EditorTool::Select; set_status("Select"); }
    if (input.key_pressed(GLFW_KEY_C) && !input.mods.ctrl) { tool_ = EditorTool::Collision; set_status("Collision"); }
    if (input.key_pressed(GLFW_KEY_L) && !input.mods.ctrl) { tool_ = EditorTool::Line; set_status("Line"); }
    if (input.key_pressed(GLFW_KEY_B)) { tool_ = EditorTool::Rect; set_status("Rectangle"); }
    if (input.key_pressed(GLFW_KEY_T) && !input.mods.ctrl) { tool_ = EditorTool::Portal; set_status("Portal"); }
    // Window toggles
    if (input.key_pressed(GLFW_KEY_F2)) show_npc_spawner_ = !show_npc_spawner_;
    if (input.key_pressed(GLFW_KEY_F3)) show_script_ide_ = !show_script_ide_;
    if (input.key_pressed(GLFW_KEY_F4)) show_debug_console_ = !show_debug_console_;
    if (input.key_pressed(GLFW_KEY_G)) { show_grid_ = !show_grid_; set_status(show_grid_ ? "Grid ON" : "Grid OFF"); }
    if (input.key_pressed(GLFW_KEY_V) && !input.mods.ctrl) show_collision_ = !show_collision_;
    for (int k = GLFW_KEY_1; k <= GLFW_KEY_9; k++) {
        if (input.key_pressed(k)) {
            int layer = k - GLFW_KEY_1;
            if (!input.mods.shift && layer < map_->layer_count()) { active_layer_ = layer; set_status("Layer " + std::to_string(layer+1)); }
            if (input.mods.shift && layer < MAX_LAYERS) { layer_visible_[layer] = !layer_visible_[layer]; }
        }
    }
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_Z)) { if (input.mods.shift) redo(); else undo(); }
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_Y)) redo();
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_C)) copy_selection();
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_V) && clipboard_.has_data) { tool_ = EditorTool::Paint; set_status("Click to paste"); }
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_S)) {
        // Quick save (no dialog, no Vulkan conflict)
        if (game_state_) { save_map_file(*game_state_, "assets/maps/current.json") ? set_status("Saved") : set_status("Failed!"); }
        else { save_map("assets/maps/current.json") ? set_status("Saved") : set_status("Failed!"); }
    }
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_H)) flip_clipboard_h();
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_J)) flip_clipboard_v();
    if (input.key_pressed(GLFW_KEY_DELETE) && selection_.active) {
        begin_action("delete");
        for (int y = selection_.top(); y <= selection_.bottom(); y++)
            for (int x = selection_.left(); x <= selection_.right(); x++) {
                int old_t = map_->tile_at(active_layer_, x, y);
                record_tile_change(active_layer_, x, y, old_t, 0);
                map_->set_tile(active_layer_, x, y, 0);
            }
        commit_action(); set_status("Deleted");
    }
    if (input.key_pressed(GLFW_KEY_ESCAPE)) clear_selection();
    if (input.key_pressed(GLFW_KEY_0) && input.mods.ctrl) { zoom_ = 1.0f; set_status("Zoom reset"); }
}

// ── Map interaction ──

void TileEditor::handle_map_click(float mx, float my, const Camera& camera, bool is_drag) {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);
    if (tile.x < 0 || tile.x >= map_->width() || tile.y < 0 || tile.y >= map_->height()) return;
    if (is_drag && tile.x == last_paint_tile_.x && tile.y == last_paint_tile_.y) return;
    last_paint_tile_ = tile;
    switch (tool_) {
        case EditorTool::Paint:
            if (clipboard_.has_data && !is_drag) {
                begin_action("paste"); paste_at(tile.x, tile.y); clipboard_.has_data = false; commit_action();
            } else if (stamp_mode_ && !is_drag && game_state_ && object_stamps_ &&
                       selected_stamp_ >= 0 && selected_stamp_ < (int)object_stamps_->size()) {
                // Place stamp as a world object
                const auto& stamp = (*object_stamps_)[selected_stamp_];
                float ts = (float)map_->tile_size();
                eb::Vec2 world_pos = {tile.x * ts + stamp.place_w * 0.5f,
                                      tile.y * ts + stamp.place_h};
                // Add to object defs if not already present for this stamp region
                int obj_id = -1;
                for (int oi = 0; oi < (int)game_state_->object_defs.size(); oi++) {
                    auto& od = game_state_->object_defs[oi];
                    auto& region = game_state_->object_regions[oi];
                    if (stamp.region_id < tileset_->region_count()) {
                        auto r = tileset_->region(stamp.region_id);
                        if (region.pixel_x == r.pixel_x && region.pixel_y == r.pixel_y) {
                            obj_id = oi; break;
                        }
                    }
                }
                if (obj_id < 0 && stamp.region_id < tileset_->region_count()) {
                    // Create new object def
                    auto r = tileset_->region(stamp.region_id);
                    float tw = (float)tileset_texture_->width();
                    float th = (float)tileset_texture_->height();
                    ObjectDef def;
                    def.src_pos = {(float)r.pixel_x, (float)r.pixel_y};
                    def.src_size = {(float)r.pixel_w, (float)r.pixel_h};
                    def.render_size = {stamp.place_w, stamp.place_h};
                    game_state_->object_defs.push_back(def);
                    eb::AtlasRegion ar;
                    ar.pixel_x = r.pixel_x; ar.pixel_y = r.pixel_y;
                    ar.pixel_w = r.pixel_w; ar.pixel_h = r.pixel_h;
                    ar.uv_min = r.uv_min; ar.uv_max = r.uv_max;
                    game_state_->object_regions.push_back(ar);
                    obj_id = (int)game_state_->object_defs.size() - 1;
                }
                if (obj_id >= 0) {
                    begin_action("place " + stamp.name);
                    current_action_.object_changes.push_back(
                        {obj_id, world_pos.x, world_pos.y, true});
                    game_state_->world_objects.push_back({obj_id, world_pos});
                    commit_action();
                    set_status("Placed: " + stamp.name);
                }
            } else {
                paint_tile(tile.x, tile.y);
            }
            break;
        case EditorTool::Erase: erase_tile(tile.x, tile.y); break;
        case EditorTool::Fill: if (!is_drag) { begin_action("fill"); flood_fill(tile.x, tile.y, selected_tile_); commit_action(); } break;
        case EditorTool::Eyedrop: if (!is_drag) { int p = map_->tile_at(active_layer_, tile.x, tile.y); if (p > 0) { selected_tile_ = p; tool_ = EditorTool::Paint; set_status("Picked " + std::to_string(p)); } } break;
        case EditorTool::Select:
            if (!is_drag) { selection_.x1 = tile.x; selection_.y1 = tile.y; selection_.x2 = tile.x; selection_.y2 = tile.y; selection_.active = true; }
            else { selection_.x2 = tile.x; selection_.y2 = tile.y; }
            break;
        case EditorTool::Collision: if (!is_drag) { begin_action("collision"); cycle_collision(tile.x, tile.y); commit_action(); } break;
        case EditorTool::Line: case EditorTool::Rect:
            if (!is_drag) { selection_.x1 = tile.x; selection_.y1 = tile.y; }
            selection_.x2 = tile.x; selection_.y2 = tile.y;
            break;
        case EditorTool::Portal:
            if (!is_drag) {
                begin_action("portal");
                auto cur = map_->collision_at(tile.x, tile.y);
                if (cur == CollisionType::Portal) {
                    // Remove portal
                    record_collision_change(tile.x, tile.y, cur, CollisionType::None);
                    map_->set_collision_at(tile.x, tile.y, CollisionType::None);
                    for (int pi = (int)map_->portals().size()-1; pi >= 0; pi--)
                        if (map_->portals()[pi].tile_x == tile.x && map_->portals()[pi].tile_y == tile.y)
                            map_->remove_portal(pi);
                    set_status("Portal removed");
                } else {
                    // Place portal
                    record_collision_change(tile.x, tile.y, cur, CollisionType::Portal);
                    Portal p; p.tile_x = tile.x; p.tile_y = tile.y;
                    p.label = "portal"; p.target_map = "";
                    p.target_x = 0; p.target_y = 0;
                    map_->portals().push_back(p);
                    map_->set_collision_at(tile.x, tile.y, CollisionType::Portal);
                    set_status("Portal placed at " + std::to_string(tile.x) + "," + std::to_string(tile.y));
                }
                commit_action();
            }
            break;
    }
}

void TileEditor::handle_map_right_click(float mx, float my, const Camera& camera) {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);
    if (tile.x == last_paint_tile_.x && tile.y == last_paint_tile_.y) return;
    last_paint_tile_ = tile;

    // Try to delete a world object near the click first
    if (game_state_) {
        float ts = (float)map_->tile_size();
        float click_x = world.x;
        float click_y = world.y;
        int best = -1;
        float best_dist = 48.0f; // Max click distance to delete an object

        for (int i = 0; i < (int)game_state_->world_objects.size(); i++) {
            auto& obj = game_state_->world_objects[i];
            auto& def = game_state_->object_defs[obj.sprite_id];
            // Object center
            float cx = obj.position.x;
            float cy = obj.position.y - def.render_size.y * 0.5f;
            float dx = click_x - cx;
            float dy = click_y - cy;
            float d = std::sqrt(dx*dx + dy*dy);
            if (d < best_dist) {
                best_dist = d;
                best = i;
            }
        }

        if (best >= 0) {
            auto& obj = game_state_->world_objects[best];
            begin_action("delete object");
            current_action_.object_changes.push_back(
                {obj.sprite_id, obj.position.x, obj.position.y, false});
            game_state_->world_objects.erase(game_state_->world_objects.begin() + best);
            commit_action();
            set_status("Deleted object");
            return;
        }
    }

    // Otherwise erase tile
    erase_tile(tile.x, tile.y);
}

// ── Tool operations ──

void TileEditor::paint_tile(int tx, int ty) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    int w = map_->width(), h = map_->height();
    int half = (brush_size_ - 1) / 2;
    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            int bx = tx + dx, by = ty + dy;
            if (bx < 0 || bx >= w || by < 0 || by >= h) continue;
            int old_t = map_->tile_at(active_layer_, bx, by);
            if (old_t != selected_tile_) {
                record_tile_change(active_layer_, bx, by, old_t, selected_tile_);
                map_->set_tile(active_layer_, bx, by, selected_tile_);
            }
        }
    }
}

void TileEditor::erase_tile(int tx, int ty) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    int w = map_->width(), h = map_->height();
    int half = (brush_size_ - 1) / 2;
    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            int bx = tx + dx, by = ty + dy;
            if (bx < 0 || bx >= w || by < 0 || by >= h) continue;
            int old_t = map_->tile_at(active_layer_, bx, by);
            if (old_t != 0) {
                record_tile_change(active_layer_, bx, by, old_t, 0);
                map_->set_tile(active_layer_, bx, by, 0);
            }
        }
    }
}

void TileEditor::flood_fill(int tx, int ty, int new_tile) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    int old_tile = map_->tile_at(active_layer_, tx, ty);
    if (old_tile == new_tile) return;
    std::queue<Vec2i> q; q.push(Vec2i(tx, ty));
    int w = map_->width(), h = map_->height();
    std::vector<bool> visited(w*h, false);
    while (!q.empty()) {
        Vec2i c = q.front(); q.pop();
        if (c.x<0||c.x>=w||c.y<0||c.y>=h) continue;
        int idx = c.y*w+c.x; if (visited[idx]) continue;
        if (map_->tile_at(active_layer_, c.x, c.y) != old_tile) continue;
        visited[idx] = true;
        record_tile_change(active_layer_, c.x, c.y, old_tile, new_tile);
        map_->set_tile(active_layer_, c.x, c.y, new_tile);
        q.push(Vec2i(c.x-1,c.y)); q.push(Vec2i(c.x+1,c.y));
        q.push(Vec2i(c.x,c.y-1)); q.push(Vec2i(c.x,c.y+1));
    }
}

void TileEditor::cycle_collision(int tx, int ty) {
    if (!map_) return;
    auto cur = map_->collision_at(tx, ty);
    CollisionType next;
    switch (cur) {
        case CollisionType::None: next = CollisionType::Solid; break;
        case CollisionType::Solid: next = CollisionType::Portal; break;
        case CollisionType::Portal:
            for (int i=(int)map_->portals().size()-1; i>=0; i--)
                if (map_->portals()[i].tile_x==tx && map_->portals()[i].tile_y==ty) map_->remove_portal(i);
            next = CollisionType::None; break;
    }
    record_collision_change(tx, ty, cur, next);
    map_->set_collision_at(tx, ty, next);
    if (next == CollisionType::Portal) {
        Portal p; p.tile_x=tx; p.tile_y=ty; p.label="portal";
        map_->portals().push_back(p);
    }
}

void TileEditor::commit_line(int x1, int y1, int x2, int y2, int tile) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    int cx = x1, cy = y1;
    while (true) {
        if (cx >= 0 && cx < map_->width() && cy >= 0 && cy < map_->height()) {
            int old_t = map_->tile_at(active_layer_, cx, cy);
            if (old_t != tile) {
                record_tile_change(active_layer_, cx, cy, old_t, tile);
                map_->set_tile(active_layer_, cx, cy, tile);
            }
        }
        if (cx == x2 && cy == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }
}

void TileEditor::commit_rect(int x1, int y1, int x2, int y2, int tile, bool filled) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    int left = std::min(x1, x2), right = std::max(x1, x2);
    int top = std::min(y1, y2), bottom = std::max(y1, y2);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            if (x < 0 || x >= map_->width() || y < 0 || y >= map_->height()) continue;
            if (filled || x == left || x == right || y == top || y == bottom) {
                int old_t = map_->tile_at(active_layer_, x, y);
                if (old_t != tile) {
                    record_tile_change(active_layer_, x, y, old_t, tile);
                    map_->set_tile(active_layer_, x, y, tile);
                }
            }
        }
    }
}

// ── Clipboard ──

void TileEditor::copy_selection() {
    if (!map_ || !selection_.active) return;
    int sw=selection_.sel_width(), sh=selection_.sel_height(), sx=selection_.left(), sy=selection_.top();
    clipboard_.width=sw; clipboard_.height=sh;
    clipboard_.tiles.resize(sw*sh); clipboard_.collision.resize(sw*sh); clipboard_.has_data=true;
    for (int y=0;y<sh;y++) for (int x=0;x<sw;x++) {
        clipboard_.tiles[y*sw+x] = map_->tile_at(active_layer_, sx+x, sy+y);
        clipboard_.collision[y*sw+x] = map_->collision_at(sx+x, sy+y);
    }
    set_status("Copied " + std::to_string(sw) + "x" + std::to_string(sh));
}

void TileEditor::paste_at(int tx, int ty) {
    if (!map_ || !clipboard_.has_data) return;
    for (int y=0;y<clipboard_.height;y++) for (int x=0;x<clipboard_.width;x++) {
        int tid=clipboard_.tiles[y*clipboard_.width+x];
        int old_t=map_->tile_at(active_layer_,tx+x,ty+y);
        record_tile_change(active_layer_,tx+x,ty+y,old_t,tid);
        map_->set_tile(active_layer_,tx+x,ty+y,tid);
        auto oc=map_->collision_at(tx+x,ty+y);
        auto nc=clipboard_.collision[y*clipboard_.width+x];
        record_collision_change(tx+x,ty+y,oc,nc);
        map_->set_collision_at(tx+x,ty+y,nc);
    }
    set_status("Pasted");
}

void TileEditor::clear_selection() { selection_ = {}; }

void TileEditor::flip_clipboard_h() {
    if (!clipboard_.has_data) return;
    int w=clipboard_.width, h=clipboard_.height;
    std::vector<int> f(w*h); std::vector<CollisionType> fc(w*h);
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) { f[y*w+(w-1-x)]=clipboard_.tiles[y*w+x]; fc[y*w+(w-1-x)]=clipboard_.collision[y*w+x]; }
    clipboard_.tiles=f; clipboard_.collision=fc; set_status("Flipped H");
}

void TileEditor::flip_clipboard_v() {
    if (!clipboard_.has_data) return;
    int w=clipboard_.width, h=clipboard_.height;
    std::vector<int> f(w*h); std::vector<CollisionType> fc(w*h);
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) { f[(h-1-y)*w+x]=clipboard_.tiles[y*w+x]; fc[(h-1-y)*w+x]=clipboard_.collision[y*w+x]; }
    clipboard_.tiles=f; clipboard_.collision=fc; set_status("Flipped V");
}

bool TileEditor::save_map(const std::string& path) const { return map_ ? map_->save_json(path) : false; }
bool TileEditor::load_map(const std::string& path) {
    if (game_state_ && renderer_) {
        return load_map_file(*game_state_, *renderer_, path);
    }
    return map_ ? map_->load_json(path) : false;
}

// ══════════════════════════════ Rendering ══════════════════════════════

void TileEditor::render(SpriteBatch& batch, const Camera& camera,
                         VkDescriptorSet tileset_desc, int screen_w, int screen_h) {
    if (!active_ || !map_) return;

    // World-space overlays only (grid, collision, selection, cursor)
    batch.set_projection(camera.projection_matrix());
    if (show_grid_) render_grid(batch, camera);
    if (show_collision_ || tool_ == EditorTool::Portal || tool_ == EditorTool::Collision)
        render_collision_overlay(batch, camera);
    if (selection_.active) render_selection(batch, camera);
    render_cursor(batch, camera, mouse_x_, mouse_y_);

    // No screen-space panel rendering — ImGui handles all UI now
}

void TileEditor::render_grid(SpriteBatch& batch, const Camera& camera) const {
    Rect view = camera.visible_area();
    float ts = (float)map_->tile_size();
    int sx=std::max(0,(int)std::floor(view.x/ts)), sy=std::max(0,(int)std::floor(view.y/ts));
    int ex=std::min(map_->width(),(int)std::ceil((view.x+view.w)/ts)+1);
    int ey=std::min(map_->height(),(int)std::ceil((view.y+view.h)/ts)+1);
    Vec4 gc={1,1,1,0.12f};
    for (int x=sx;x<=ex;x++) batch.draw_quad({x*ts-0.5f, sy*ts}, {1, (ey-sy)*ts}, gc);
    for (int y=sy;y<=ey;y++) batch.draw_quad({sx*ts, y*ts-0.5f}, {(ex-sx)*ts, 1}, gc);
}

void TileEditor::render_collision_overlay(SpriteBatch& batch, const Camera& camera) const {
    Rect view = camera.visible_area();
    float ts = (float)map_->tile_size();
    int sx=std::max(0,(int)std::floor(view.x/ts)), sy=std::max(0,(int)std::floor(view.y/ts));
    int ex=std::min(map_->width(),(int)std::ceil((view.x+view.w)/ts)+1);
    int ey=std::min(map_->height(),(int)std::ceil((view.y+view.h)/ts)+1);
    for (int y=sy;y<ey;y++) for (int x=sx;x<ex;x++) {
        auto ct=map_->collision_at(x,y);
        if (ct==CollisionType::None) continue;
        if (ct==CollisionType::Solid) {
            batch.draw_quad({x*ts+2,y*ts+2},{ts-4,ts-4},{1,0.2f,0.2f,0.35f});
        } else {
            // Portal: cyan diamond marker
            float cx=x*ts+ts*0.5f, cy=y*ts+ts*0.5f, r=ts*0.35f;
            batch.draw_quad({cx-r,cy-2},{r*2,4},{0.2f,1,0.8f,0.6f});
            batch.draw_quad({cx-2,cy-r},{4,r*2},{0.2f,1,0.8f,0.6f});
            batch.draw_quad({x*ts+1,y*ts+1},{ts-2,ts-2},{0.1f,0.6f,1,0.25f});
            // Border
            batch.draw_quad({x*ts,y*ts},{ts,2},{0.2f,1,0.8f,0.8f});
            batch.draw_quad({x*ts,y*ts+ts-2},{ts,2},{0.2f,1,0.8f,0.8f});
            batch.draw_quad({x*ts,y*ts},{2,ts},{0.2f,1,0.8f,0.8f});
            batch.draw_quad({x*ts+ts-2,y*ts},{2,ts},{0.2f,1,0.8f,0.8f});
        }
    }
}

void TileEditor::render_selection(SpriteBatch& batch, const Camera& camera) const {
    float ts=(float)map_->tile_size();
    Vec2 pos={selection_.left()*ts, selection_.top()*ts};
    Vec2 sz={selection_.sel_width()*ts, selection_.sel_height()*ts};
    batch.draw_quad(pos, sz, {0.3f,0.8f,1,0.2f});
    Vec4 bc={0.3f,0.8f,1,0.7f};
    batch.draw_quad({pos.x,pos.y},{sz.x,2},bc); batch.draw_quad({pos.x,pos.y+sz.y-2},{sz.x,2},bc);
    batch.draw_quad({pos.x,pos.y},{2,sz.y},bc); batch.draw_quad({pos.x+sz.x-2,pos.y},{2,sz.y},bc);
}

void TileEditor::render_cursor(SpriteBatch& batch, const Camera& camera, float mx, float my) const {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);
    if (tile.x<0||tile.x>=map_->width()||tile.y<0||tile.y>=map_->height()) return;
    float ts=(float)map_->tile_size();
    Vec2 pos={tile.x*ts, tile.y*ts};
    Vec4 cc;
    switch (tool_) {
        case EditorTool::Paint:cc={1,1,1,0.3f};break; case EditorTool::Erase:cc={1,0.3f,0.3f,0.3f};break;
        case EditorTool::Fill:cc={0.3f,1,0.3f,0.3f};break; case EditorTool::Eyedrop:cc={1,1,0.3f,0.3f};break;
        case EditorTool::Select:cc={0.3f,0.8f,1,0.3f};break; case EditorTool::Collision:cc={1,0.5f,0,0.3f};break;
        case EditorTool::Line:cc={0.8f,0.3f,1,0.3f};break; case EditorTool::Rect:cc={1,0.6f,0.2f,0.3f};break;
        case EditorTool::Portal:cc={0.2f,1,0.8f,0.5f};break;
    }
    batch.draw_quad(pos, {ts,ts}, cc);
    Vec4 bc={cc.x,cc.y,cc.z,0.8f};
    batch.draw_quad({pos.x,pos.y},{ts,2},bc); batch.draw_quad({pos.x,pos.y+ts-2},{ts,2},bc);
    batch.draw_quad({pos.x,pos.y},{2,ts},bc); batch.draw_quad({pos.x+ts-2,pos.y},{2,ts},bc);
    if (tool_==EditorTool::Paint && tileset_ && selected_tile_>0) {
        auto r=tileset_->region(selected_tile_-1);
        batch.draw_quad(pos,{ts,ts},r.uv_min,r.uv_max,{1,1,1,0.5f});
    }
    if (clipboard_.has_data && tool_==EditorTool::Paint && tileset_) {
        for (int py=0;py<clipboard_.height;py++) for (int px=0;px<clipboard_.width;px++) {
            int tid=clipboard_.tiles[py*clipboard_.width+px]; if (tid<=0) continue;
            auto r=tileset_->region(tid-1);
            batch.draw_quad({(tile.x+px)*ts,(tile.y+py)*ts},{ts,ts},r.uv_min,r.uv_max,{1,1,1,0.35f});
        }
    }
}

// ── Tool Panel ──

void TileEditor::render_tool_panel(SpriteBatch& batch, int /*screen_w*/, int /*screen_h*/) const {
    render_panel_bg(batch, tool_panel_);

    float ts_btn = 1.5f; // Text scale for button labels
    float ts_info = 1.2f;

    struct ToolDef { EditorTool tool; Vec4 color; const char* name; };
    ToolDef tools[] = {
        {EditorTool::Paint,     {0.2f,0.8f,0.2f,1}, "Paint"},
        {EditorTool::Erase,     {0.9f,0.2f,0.2f,1}, "Erase"},
        {EditorTool::Fill,      {0.2f,0.6f,0.9f,1}, "Fill"},
        {EditorTool::Eyedrop,   {0.9f,0.9f,0.2f,1}, "Eyedrop"},
        {EditorTool::Select,    {0.2f,0.9f,0.9f,1}, "Select"},
        {EditorTool::Collision, {0.9f,0.5f,0.1f,1}, "Collide"},
        {EditorTool::Line,      {0.7f,0.3f,0.9f,1}, "Line"},
        {EditorTool::Rect,      {0.9f,0.6f,0.2f,1}, "Rect"},
    };

    float bx = tool_panel_.x + PANEL_PAD;
    float by = tool_panel_.y + PANEL_TITLE_H + PANEL_PAD;
    float btn_w = 88, btn_h = 32;

    for (int i = 0; i < 8; i++) {
        int col = i % 2, row = i / 2;
        float x = bx + col * (btn_w + 4);
        float y = by + row * (btn_h + 4);

        if (tools[i].tool == tool_)
            batch.draw_quad({x-2, y-2}, {btn_w+4, btn_h+4}, {1,1,1,0.8f});
        batch.draw_quad({x, y}, {btn_w, btn_h}, tools[i].color);
        if (text_)
            text_->draw_text(batch, font_desc_, tools[i].name,
                             Vec2(x+6, y+8), Vec4(1,1,1,0.95f), ts_btn);
    }

    // Separator
    float sep_y = by + 4 * (btn_h + 4) + 4;
    batch.draw_quad({bx, sep_y}, {tool_panel_.w - PANEL_PAD*2, 1}, {0.4f,0.4f,0.5f,0.5f});

    // Grid / Collision toggles
    float tog_y = sep_y + 8;
    if (text_) {
        Vec4 gc = show_grid_ ? Vec4{0.3f,1,0.3f,1} : Vec4{0.3f,0.3f,0.3f,1};
        batch.draw_quad({bx, tog_y}, {85, 26}, gc);
        text_->draw_text(batch, font_desc_, "Grid(G)", Vec2(bx+6, tog_y+5), Vec4(1,1,1,0.9f), ts_info);

        Vec4 cc = show_collision_ ? Vec4{1,0.5f,0.1f,1} : Vec4{0.3f,0.3f,0.3f,1};
        batch.draw_quad({bx+92, tog_y}, {95, 26}, cc);
        text_->draw_text(batch, font_desc_, "Coll(V)", Vec2(bx+98, tog_y+5), Vec4(1,1,1,0.9f), ts_info);
    }

    // Layers
    float layer_y = tog_y + 34;
    if (text_) {
        text_->draw_text(batch, font_desc_, "Layers:", Vec2(bx, layer_y), Vec4(0.7f,0.7f,0.8f,1), ts_info);
        layer_y += 18;
        for (int i = 0; i < map_->layer_count(); i++) {
            Vec4 lc = (i==active_layer_) ? Vec4{0.3f,0.7f,1,1}
                     : !layer_visible_[i] ? Vec4{0.2f,0.15f,0.15f,1}
                     : Vec4{0.2f,0.2f,0.3f,1};
            float lx = bx + i * 34;
            batch.draw_quad({lx, layer_y}, {30, 26}, lc);
            text_->draw_text(batch, font_desc_, std::to_string(i+1),
                             Vec2(lx+8, layer_y+5), Vec4(1,1,1,0.9f), ts_info);
        }
    }

    // Zoom info
    float info_y = layer_y + 36;
    if (text_) {
        char zs[32]; std::snprintf(zs, sizeof(zs), "Zoom: %.0f%%", zoom_ * 100);
        text_->draw_text(batch, font_desc_, zs, Vec2(bx, info_y), Vec4(0.6f,0.6f,0.7f,1), ts_info);
        char us[32]; std::snprintf(us, sizeof(us), "Undo: %d", (int)undo_stack_.size());
        text_->draw_text(batch, font_desc_, us, Vec2(bx, info_y+18), Vec4(0.6f,0.6f,0.7f,1), ts_info);
    }
}

// ── Tile Panel ──

void TileEditor::render_tile_panel(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                                    int /*screen_w*/, int screen_h) const {
    if (!tileset_) return;
    render_panel_bg(batch, tile_panel_);

    int max_tiles = tileset_->region_count();
    if (max_tiles == 0) return;

    float tsz = PALETTE_TILE_SIZE;
    float start_x = tile_panel_.x + PANEL_PAD;
    float start_y = tile_panel_.y + PANEL_TITLE_H + PANEL_PAD;
    float panel_bottom = tile_panel_.y + tile_panel_.h - PANEL_PAD;

    batch.set_texture(tileset_desc);

    for (int i = 0; i < max_tiles; i++) {
        int col = i % palette_cols_;
        int row = i / palette_cols_ - palette_scroll_;
        if (row < 0) continue;
        float ty = start_y + row * tsz;
        if (ty + tsz > panel_bottom) break;
        float tx = start_x + col * tsz;
        auto region = tileset_->region(i);

        batch.draw_quad({tx, ty}, {tsz-2, tsz-2}, {0.08f,0.08f,0.1f,1});
        batch.draw_quad({tx+1, ty+1}, {tsz-4, tsz-4}, region.uv_min, region.uv_max);

        if (i + 1 == selected_tile_) {
            Vec4 hc = {1, 0.8f, 0.2f, 1};
            batch.draw_quad({tx-1,ty-1},{tsz,2},hc); batch.draw_quad({tx-1,ty+tsz-3},{tsz,2},hc);
            batch.draw_quad({tx-1,ty-1},{2,tsz},hc); batch.draw_quad({tx+tsz-3,ty-1},{2,tsz},hc);
        }
    }
}

// ── Object Panels (Buildings, Vehicles, Trees, Misc) ──

void TileEditor::render_object_panel(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                                      const EditorPanel& panel, const char* category,
                                      int /*screen_w*/, int /*screen_h*/) const {
    if (!tileset_ || !object_stamps_) return;
    render_panel_bg(batch, panel);

    float item_h = 48.0f;
    float start_x = panel.x + PANEL_PAD;
    float start_y = panel.y + PANEL_TITLE_H + PANEL_PAD;
    float panel_bottom = panel.y + panel.h - PANEL_PAD;
    float item_w = panel.w - PANEL_PAD * 2;
    int drawn = 0;

    for (int i = 0; i < (int)object_stamps_->size(); i++) {
        const auto& stamp = (*object_stamps_)[i];
        if (std::string(stamp.category) != std::string(category)) continue;

        float ty = start_y + drawn * (item_h + 4);
        if (ty + item_h > panel_bottom) break;

        // Item background
        bool selected = (stamp_mode_ && selected_stamp_ == i);
        Vec4 bg = selected ? Vec4{0.3f,0.4f,0.6f,0.8f} : Vec4{0.08f,0.08f,0.1f,0.8f};
        batch.draw_quad({start_x, ty}, {item_w, item_h}, bg);

        // Sprite preview
        if (stamp.region_id >= 0 && stamp.region_id < tileset_->region_count()) {
            auto r = tileset_->region(stamp.region_id);
            float preview_h = item_h - 4;
            float preview_w = preview_h * (stamp.place_w / std::max(1.0f, stamp.place_h));
            if (preview_w > item_w * 0.4f) {
                preview_w = item_w * 0.4f;
                preview_h = preview_w * (stamp.place_h / std::max(1.0f, stamp.place_w));
            }
            batch.set_texture(tileset_desc);
            batch.draw_quad({start_x + 2, ty + 2}, {preview_w, preview_h},
                            r.uv_min, r.uv_max);
        }

        // Name label
        if (text_) {
            text_->draw_text(batch, font_desc_, stamp.name,
                             Vec2(start_x + item_w * 0.45f, ty + 12),
                             Vec4(0.8f, 0.8f, 0.9f, 1), 1.2f);
        }

        // Border if selected
        if (selected) {
            Vec4 hc = {1, 0.8f, 0.2f, 1};
            batch.draw_quad({start_x, ty}, {item_w, 2}, hc);
            batch.draw_quad({start_x, ty+item_h-2}, {item_w, 2}, hc);
        }

        drawn++;
    }
}

// ── Status Bar ──

void TileEditor::render_status_bar(SpriteBatch& batch, int screen_w, int screen_h) const {
    float sw = (float)screen_w, sh = (float)screen_h;
    float sby = sh - STATUS_BAR_HEIGHT;
    batch.draw_quad({0, sby}, {sw, STATUS_BAR_HEIGHT}, {0.1f,0.1f,0.14f,0.92f});
    batch.draw_quad({0, sby}, {sw, 1}, {0.35f,0.35f,0.45f,1});
    if (!text_) return;

    float ts = 1.3f;
    const char* names[]={"Paint","Erase","Fill","Eyedrop","Select","Collision","Line","Rect"};
    text_->draw_text(batch, font_desc_, names[(int)tool_], Vec2(10, sby+5), Vec4(0.9f,0.9f,1,1), ts);

    if (map_) {
        char info[80]; std::snprintf(info, sizeof(info), "Tile:%d  Layer:%d  Map:%dx%d",
                                     selected_tile_, active_layer_+1, map_->width(), map_->height());
        text_->draw_text(batch, font_desc_, info, Vec2(120, sby+6), Vec4(0.6f,0.6f,0.7f,1), 1.1f);
    }

    if (status_timer_ > 0 && !status_msg_.empty()) {
        float alpha = std::min(1.0f, status_timer_);
        text_->draw_text(batch, font_desc_, status_msg_, Vec2(sw*0.5f, sby+5), Vec4(1,1,0.4f,alpha), ts);
    }

    text_->draw_text(batch, font_desc_, "Ctrl+Z:Undo  Ctrl+S:Save  Tab:Exit",
                     Vec2(sw - 350, sby+7), Vec4(0.4f,0.4f,0.5f,0.8f), 1.0f);
}

#ifndef EB_ANDROID
// ══════════════════ ImGui Editor UI ══════════════════

void TileEditor::render_imgui(GameState& game) {
    if (!active_ || !map_ || !tileset_) return;
    ensure_imgui_texture();

    ImTextureID tex_id = (ImTextureID)imgui_tileset_id_;
    float tex_w = tileset_texture_ ? (float)tileset_texture_->width() : 1.0f;
    float tex_h = tileset_texture_ ? (float)tileset_texture_->height() : 1.0f;

    // Helper: draw an atlas region as an ImGui image button
    auto region_button = [&](int region_idx, float btn_size, int id_offset = 0) -> bool {
        auto r = tileset_->region(region_idx);
        ImVec2 uv0(r.uv_min.x, r.uv_min.y);
        ImVec2 uv1(r.uv_max.x, r.uv_max.y);
        ImGui::PushID(id_offset + region_idx);
        bool clicked = ImGui::ImageButton("##img", tex_id, ImVec2(btn_size, btn_size), uv0, uv1);
        ImGui::PopID();
        return clicked;
    };

    // Helper: draw a region as image (not button)
    auto region_image = [&](int region_idx, float w, float h) {
        auto r = tileset_->region(region_idx);
        ImGui::Image(tex_id, ImVec2(w, h), ImVec2(r.uv_min.x, r.uv_min.y),
                     ImVec2(r.uv_max.x, r.uv_max.y));
    };

    // ═══════════ MAIN MENU BAR ═══════════
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Map...", "Ctrl+S"))
                pending_dialog_ = PendingDialog::Save;
            if (ImGui::MenuItem("Load Map..."))
                pending_dialog_ = PendingDialog::Load;
            if (ImGui::MenuItem("Import Asset..."))
                pending_dialog_ = PendingDialog::ImportAsset;
            ImGui::Separator();
            if (ImGui::MenuItem("Quick Save"))
                if (game_state_)
                    save_map_file(*game_state_, "assets/maps/current.json") ? set_status("Quick saved") : set_status("Failed!");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) redo();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Tools", "1", &show_tools_window_);
            ImGui::MenuItem("Assets", "2", &show_assets_window_);
            ImGui::MenuItem("Minimap", "3", &show_minimap_window_);
            ImGui::Separator();
            ImGui::MenuItem("NPC Spawner", "F2", &show_npc_spawner_);
            ImGui::MenuItem("Script IDE", "F3", &show_script_ide_);
            ImGui::MenuItem("Debug Console", "F4", &show_debug_console_);
            ImGui::Separator();
            ImGui::MenuItem("Grid", "G", &show_grid_);
            ImGui::MenuItem("Collision", "V", &show_collision_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Paint", "P")) tool_ = EditorTool::Paint;
            if (ImGui::MenuItem("Erase", "E")) tool_ = EditorTool::Erase;
            if (ImGui::MenuItem("Fill", "F")) tool_ = EditorTool::Fill;
            if (ImGui::MenuItem("Eyedrop", "I")) tool_ = EditorTool::Eyedrop;
            if (ImGui::MenuItem("Select", "R")) tool_ = EditorTool::Select;
            if (ImGui::MenuItem("Collision", "C")) tool_ = EditorTool::Collision;
            if (ImGui::MenuItem("Line", "L")) tool_ = EditorTool::Line;
            if (ImGui::MenuItem("Rectangle", "B")) tool_ = EditorTool::Rect;
            if (ImGui::MenuItem("Portal", "T")) tool_ = EditorTool::Portal;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ═══════════ TOOLS WINDOW ═══════════
    if (show_tools_window_) {
    ImGui::SetNextWindowPos(ImVec2(8, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Tools##editor", &show_tools_window_)) {
        // Tool buttons in 2 columns
        struct TD { EditorTool t; const char* n; };
        TD tools[] = {
            {EditorTool::Paint,"Paint"}, {EditorTool::Erase,"Erase"},
            {EditorTool::Fill,"Fill"}, {EditorTool::Eyedrop,"Eyedrop"},
            {EditorTool::Select,"Select"}, {EditorTool::Collision,"Collision"},
            {EditorTool::Line,"Line"}, {EditorTool::Rect,"Rectangle"},
            {EditorTool::Portal,"Portal"},
        };
        for (int i = 0; i < 9; i++) {
            if (i % 2 != 0) ImGui::SameLine();
            bool sel = (tool_ == tools[i].t);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.45f,0.75f,1));
            if (ImGui::Button(tools[i].n, ImVec2(108, 28))) tool_ = tools[i].t;
            if (sel) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Checkbox("Grid (G)", &show_grid_);
        ImGui::SameLine();
        ImGui::Checkbox("Collision (V)", &show_collision_);

        ImGui::Separator();
        ImGui::Text("Layers:");
        for (int i = 0; i < map_->layer_count(); i++) {
            ImGui::PushID(100+i);
            bool act = (i == active_layer_);
            if (act) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f,0.6f,1,1));
            char lb[8]; std::snprintf(lb,sizeof(lb),"%d",i+1);
            if (ImGui::Button(lb, ImVec2(26,26))) active_layer_ = i;
            if (act) ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Checkbox("##v", &layer_visible_[i]);
            if ((i+1) % 3 != 0 && i < map_->layer_count()-1) ImGui::SameLine();
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::Text("Zoom: %.0f%%", zoom_*100);
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##z")) zoom_ = 1.0f;

        if (ImGui::Button("Undo", ImVec2(70,0))) undo();
        ImGui::SameLine();
        if (ImGui::Button("Redo", ImVec2(70,0))) redo();
        ImGui::SameLine();
        ImGui::TextDisabled("(%d/%d)", (int)undo_stack_.size(), (int)redo_stack_.size());

        ImGui::Separator();
        ImGui::Text("Map: %dx%d", map_->width(), map_->height());
        if (ImGui::Button("Save As...", ImVec2(90,0))) {
            pending_dialog_ = PendingDialog::Save;
            std::printf("[Editor] Save button clicked, pending_dialog set\n");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load...", ImVec2(70,0))) {
            pending_dialog_ = PendingDialog::Load;
            std::printf("[Editor] Load button clicked, pending_dialog set\n");
        }
        // Quick save (no dialog)
        if (ImGui::Button("Quick Save (Ctrl+S)", ImVec2(-1,0))) {
            if (game_state_)
                save_map_file(*game_state_, "assets/maps/current.json") ? set_status("Quick saved") : set_status("Failed!");
        }

        ImGui::Separator();
        // Brush size
        ImGui::Text("Brush:");
        ImGui::SameLine();
        if (ImGui::RadioButton("1x1", brush_size_ == 1)) brush_size_ = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("2x2", brush_size_ == 2)) brush_size_ = 2;
        ImGui::SameLine();
        if (ImGui::RadioButton("3x3", brush_size_ == 3)) brush_size_ = 3;

        // Rect filled toggle (only visible when Rect tool is selected)
        if (tool_ == EditorTool::Rect) {
            ImGui::Checkbox("Filled Rectangle", &rect_filled_);
        }

        ImGui::Separator();
        // Import asset
        if (ImGui::Button("Import Asset...", ImVec2(-1,0))) {
            pending_dialog_ = PendingDialog::ImportAsset;
        }

        // Selection/Clipboard
        if (selection_.active) {
            ImGui::Separator();
            ImGui::Text("Selection: %dx%d", selection_.sel_width(), selection_.sel_height());
            if (ImGui::Button("Copy")) copy_selection();
            ImGui::SameLine();
            if (ImGui::Button("Fill##sel")) {
                // Fill entire selection with current tile
                begin_action("fill selection");
                for (int y = selection_.top(); y <= selection_.bottom(); y++)
                    for (int x = selection_.left(); x <= selection_.right(); x++) {
                        int ot = map_->tile_at(active_layer_, x, y);
                        record_tile_change(active_layer_, x, y, ot, selected_tile_);
                        map_->set_tile(active_layer_, x, y, selected_tile_);
                    }
                commit_action();
                set_status("Filled selection with tile " + std::to_string(selected_tile_));
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                begin_action("delete");
                for (int y=selection_.top();y<=selection_.bottom();y++)
                    for (int x=selection_.left();x<=selection_.right();x++) {
                        int ot=map_->tile_at(active_layer_,x,y);
                        record_tile_change(active_layer_,x,y,ot,0);
                        map_->set_tile(active_layer_,x,y,0);
                    }
                commit_action();
            }
            if (clipboard_.has_data) {
                ImGui::SameLine();
                if (ImGui::Button("FlipH")) flip_clipboard_h();
                ImGui::SameLine();
                if (ImGui::Button("FlipV")) flip_clipboard_v();
            }
        }

        // Status
        if (status_timer_ > 0 && !status_msg_.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1,1,0.3f,std::min(1.0f,status_timer_)),
                               "%s", status_msg_.c_str());
        }
    }
    ImGui::End();
    } // show_tools_window_

    // ═══════════ ASSETS WINDOW (tabbed) ═══════════
    if (show_assets_window_) {
    ImGui::SetNextWindowPos(ImVec2(700, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Assets##editor", &show_assets_window_)) {
        if (ImGui::BeginTabBar("AssetTabs")) {

            // ── Ground Tiles ──
            if (ImGui::BeginTabItem("Tiles")) {
                int tile_count = std::min(tileset_->region_count(), (int)TILE_COUNT - 1);
                float avail = ImGui::GetContentRegionAvail().x;
                float btn_sz = 40.0f;
                int cols = std::max(1, (int)(avail / (btn_sz + 6)));

                for (int i = 0; i < tile_count; i++) {
                    if (i % cols != 0) ImGui::SameLine();
                    bool sel = (selected_tile_ == i+1 && !stamp_mode_);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f,0.6f,1,1));

                    if (imgui_tileset_id_ && region_button(i, btn_sz, 2000)) {
                        selected_tile_ = i+1;
                        stamp_mode_ = false;
                        tool_ = EditorTool::Paint;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tile %d", i+1);
                    if (sel) ImGui::PopStyleColor();
                }
                ImGui::EndTabItem();
            }

            // ── Category tabs with image previews ──
            auto stamp_tab = [&](const char* name, const char* cat) {
                if (ImGui::BeginTabItem(name)) {
                    for (int i = 0; i < (int)game.object_stamps.size(); i++) {
                        auto& s = game.object_stamps[i];
                        if (s.category != std::string(cat)) continue;
                        ImGui::PushID(3000+i);

                        bool sel = (stamp_mode_ && selected_stamp_ == i);
                        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f,0.5f,0.8f,1));

                        // Selectable row with image preview
                        if (ImGui::Selectable("##sel", sel, 0, ImVec2(0, 48))) {
                            stamp_mode_ = true;
                            selected_stamp_ = i;
                            tool_ = EditorTool::Paint;
                            set_status("Stamp: " + s.name);
                        }
                        if (sel) ImGui::PopStyleColor();

                        // Image preview on the same line
                        ImGui::SameLine(8);
                        if (imgui_tileset_id_ && s.region_id < tileset_->region_count()) {
                            float aspect = s.place_w / std::max(1.0f, s.place_h);
                            float ph = 44.0f;
                            float pw = ph * aspect;
                            if (pw > 80) { pw = 80; ph = pw / aspect; }
                            region_image(s.region_id, pw, ph);
                            ImGui::SameLine();
                        }
                        ImGui::Text("%s", s.name.c_str());
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s\nPlace size: %.0fx%.0f", s.name.c_str(), s.place_w, s.place_h);

                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }
            };

            stamp_tab("Buildings", "building");
            stamp_tab("Vehicles", "vehicle");
            stamp_tab("Trees", "tree");
            stamp_tab("Misc", "misc");

            ImGui::EndTabBar();
        }

        // Selected item preview at bottom
        ImGui::Separator();
        if (stamp_mode_ && selected_stamp_ >= 0 && selected_stamp_ < (int)game.object_stamps.size()) {
            auto& s = game.object_stamps[selected_stamp_];
            ImGui::Text("Selected: %s", s.name.c_str());
            if (imgui_tileset_id_ && s.region_id < tileset_->region_count()) {
                float aspect = s.place_w / std::max(1.0f, s.place_h);
                float pw = ImGui::GetContentRegionAvail().x - 10;
                float ph = pw / aspect;
                if (ph > 120) { ph = 120; pw = ph * aspect; }
                region_image(s.region_id, pw, ph);
            }
        } else if (!stamp_mode_ && selected_tile_ > 0 && selected_tile_ <= tileset_->region_count()) {
            ImGui::Text("Selected: Tile %d", selected_tile_);
            if (imgui_tileset_id_) {
                region_image(selected_tile_ - 1, 64, 64);
            }
        }
    }
    ImGui::End();
    } // show_assets_window_

    // ═══════════ MINIMAP WINDOW ═══════════
    if (show_minimap_window_) {
    ImGui::SetNextWindowPos(ImVec2(8, 520), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 160), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Minimap##editor", &show_minimap_window_)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float mw = (float)map_->width(), mh = (float)map_->height();
        float scale = std::min(avail.x / mw, avail.y / mh);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Draw tile colors
        for (int y = 0; y < (int)mh; y++) {
            for (int x = 0; x < (int)mw; x++) {
                int t = map_->tile_at(0, x, y);
                ImU32 col;
                if (t == 0) col = IM_COL32(20, 20, 30, 255);
                else if (t >= 42 && t <= 50) col = IM_COL32(30, 60, 120, 255);  // Water
                else if (t >= 25 && t <= 36) col = IM_COL32(80, 80, 80, 255);   // Roads
                else if (t >= 5 && t <= 8) col = IM_COL32(120, 90, 60, 255);    // Dirt
                else if (t >= 19 && t <= 24) col = IM_COL32(50, 30, 40, 255);   // Dark/special
                else col = IM_COL32(50, 100, 40, 255);  // Grass
                if (map_->collision_at(x, y) == CollisionType::Solid && t > 0)
                    col = (col & 0xFF000000) | ((col & 0x00FEFEFE) >> 1); // Darken solid

                ImVec2 p0(origin.x + x * scale, origin.y + y * scale);
                ImVec2 p1(origin.x + (x + 1) * scale, origin.y + (y + 1) * scale);
                dl->AddRectFilled(p0, p1, col);
            }
        }

        // Player position marker
        if (game_state_) {
            float ts = (float)map_->tile_size();
            float px = game_state_->player_pos.x / ts * scale;
            float py = game_state_->player_pos.y / ts * scale;
            dl->AddCircleFilled(ImVec2(origin.x + px, origin.y + py), 3.0f, IM_COL32(255, 255, 50, 255));
        }

        // NPC markers
        for (auto& npc : game.npcs) {
            float ts = (float)map_->tile_size();
            float nx = npc.position.x / ts * scale;
            float ny = npc.position.y / ts * scale;
            ImU32 npc_col = npc.hostile ? IM_COL32(255, 60, 60, 255) : IM_COL32(60, 200, 255, 255);
            dl->AddCircleFilled(ImVec2(origin.x + nx, origin.y + ny), 2.0f, npc_col);
        }

        // Border
        dl->AddRect(origin, ImVec2(origin.x + mw * scale, origin.y + mh * scale),
                    IM_COL32(100, 100, 140, 200));

        // Click to teleport camera
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            ImVec2 mouse = ImGui::GetMousePos();
            float tx = (mouse.x - origin.x) / scale * map_->tile_size();
            float ty = (mouse.y - origin.y) / scale * map_->tile_size();
            game_state_->player_pos = {tx, ty};
        }
    }
    ImGui::End();
    } // show_minimap_window_

    // ═══════════ NPC SPAWNER WINDOW ═══════════
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
            set_status("Spawned NPC: " + std::string(npc_name));
        };

        ImGui::Separator();
        if (ImGui::Button("Spawn at Player", ImVec2(-1, 0))) {
            spawn_npc_at(game.player_pos.x, game.player_pos.y);
        }
        if (click_spawn_mode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1));
        }
        if (ImGui::Button(click_spawn_mode ? "Click Map to Spawn (active)" : "Click Map to Spawn", ImVec2(-1, 0))) {
            click_spawn_mode = !click_spawn_mode;
        }
        if (click_spawn_mode) {
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

    // ═══════════ SCRIPT MANAGER / IDE ═══════════
    if (show_script_ide_) {
    ImGui::SetNextWindowPos(ImVec2(250, 370), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 330), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Script IDE##editor", &show_script_ide_)) {
        static int selected_script = -1;
        static char script_buffer[8192] = "";
        static bool script_dirty = false;
        static std::string current_file;

        auto* se = game.script_engine;
        if (se) {
            auto& files = se->loaded_files();

            // File list
            if (ImGui::BeginChild("ScriptList", ImVec2(160, -30), true)) {
                for (int i = 0; i < (int)files.size(); i++) {
                    // Show just filename
                    std::string label = files[i];
                    auto slash = label.rfind('/');
                    if (slash != std::string::npos) label = label.substr(slash + 1);

                    bool sel = (selected_script == i);
                    if (ImGui::Selectable(label.c_str(), sel)) {
                        if (i != selected_script) {
                            selected_script = i;
                            current_file = files[i];
                            // Load file content
                            auto data = eb::FileIO::read_file(current_file);
                            if (!data.empty()) {
                                int len = std::min((int)data.size(), (int)sizeof(script_buffer) - 1);
                                std::memcpy(script_buffer, data.data(), len);
                                script_buffer[len] = '\0';
                                script_dirty = false;
                            }
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Editor pane
            if (ImGui::BeginChild("ScriptEdit", ImVec2(0, -30))) {
                if (selected_script >= 0) {
                    ImGui::Text("%s%s", current_file.c_str(), script_dirty ? " *" : "");
                    ImGui::Separator();
                    if (ImGui::InputTextMultiline("##code", script_buffer, sizeof(script_buffer),
                            ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput)) {
                        script_dirty = true;
                    }
                } else {
                    ImGui::TextDisabled("Select a script file to edit");
                }
            }
            ImGui::EndChild();

            // Bottom buttons
            if (selected_script >= 0) {
                if (ImGui::Button("Save")) {
                    std::ofstream f(current_file);
                    if (f.is_open()) {
                        f << script_buffer;
                        f.close();
                        script_dirty = false;
                        set_status("Saved: " + current_file);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Save & Reload")) {
                    std::ofstream f(current_file);
                    if (f.is_open()) {
                        f << script_buffer;
                        f.close();
                        script_dirty = false;
                    }
                    se->reload_all();
                    set_status("Hot reloaded all scripts");
                }
                ImGui::SameLine();
                if (ImGui::Button("Reload All")) {
                    se->reload_all();
                    set_status("Hot reloaded all scripts");
                }
            }
        } else {
            ImGui::TextDisabled("No script engine available");
        }
    }
    ImGui::End();
    } // show_script_ide_

    // ═══════════ DEBUG CONSOLE ═══════════
    if (show_debug_console_) {
    ImGui::SetNextWindowPos(ImVec2(8, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(940, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug Console##editor", &show_debug_console_)) {
        auto& dlog = eb::DebugLog::instance();

        // Filter buttons
        ImGui::Checkbox("Debug", &dlog.show_debug); ImGui::SameLine();
        ImGui::Checkbox("Info", &dlog.show_info); ImGui::SameLine();
        ImGui::Checkbox("Warn", &dlog.show_warning); ImGui::SameLine();
        ImGui::Checkbox("Error", &dlog.show_error); ImGui::SameLine();
        ImGui::Checkbox("Script", &dlog.show_script); ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) dlog.clear();
        ImGui::SameLine();
        ImGui::Text("(%d entries)", (int)dlog.entries().size());

        ImGui::Separator();

        // Scrollable log
        if (ImGui::BeginChild("LogScroll", ImVec2(0, -30), true)) {
            for (auto& entry : dlog.entries()) {
                bool show = false;
                ImVec4 color;
                const char* prefix;
                switch (entry.level) {
                    case eb::LogLevel::Debug:   show = dlog.show_debug;   color = ImVec4(0.5f,0.5f,0.5f,1); prefix = "[DBG]"; break;
                    case eb::LogLevel::Info:    show = dlog.show_info;    color = ImVec4(0.7f,0.9f,0.7f,1); prefix = "[INF]"; break;
                    case eb::LogLevel::Warning: show = dlog.show_warning; color = ImVec4(1,0.9f,0.3f,1);    prefix = "[WRN]"; break;
                    case eb::LogLevel::Error:   show = dlog.show_error;   color = ImVec4(1,0.3f,0.3f,1);    prefix = "[ERR]"; break;
                    case eb::LogLevel::Script:  show = dlog.show_script;  color = ImVec4(0.4f,0.8f,1,1);    prefix = "[SCR]"; break;
                }
                if (show) {
                    ImGui::TextColored(color, "%s %.1fs %s", prefix, entry.timestamp, entry.message.c_str());
                }
            }
            // Auto-scroll to bottom
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // Command input
        static char cmd_buf[256] = "";
        if (ImGui::InputText("##cmd", cmd_buf, sizeof(cmd_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (game.script_engine && cmd_buf[0] != '\0') {
                DLOG_SCRIPT("> %s", cmd_buf);
                game.script_engine->execute(cmd_buf);
                cmd_buf[0] = '\0';
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Enter SageLang expression");
    }
    ImGui::End();
    } // show_debug_console_

    // ═══════════ GHOST CURSOR (floating overlay) ═══════════
    // Show what will be placed at the mouse position
    if (!ImGui::GetIO().WantCaptureMouse && imgui_tileset_id_) {
        ImVec2 mouse = ImGui::GetMousePos();
        float ghost_alpha = 0.5f;

        if (stamp_mode_ && selected_stamp_ >= 0 && selected_stamp_ < (int)game.object_stamps.size()) {
            auto& s = game.object_stamps[selected_stamp_];
            if (s.region_id < tileset_->region_count()) {
                auto r = tileset_->region(s.region_id);
                float w = s.place_w, h = s.place_h;
                ImGui::GetForegroundDrawList()->AddImage(
                    tex_id, ImVec2(mouse.x, mouse.y), ImVec2(mouse.x+w, mouse.y+h),
                    ImVec2(r.uv_min.x, r.uv_min.y), ImVec2(r.uv_max.x, r.uv_max.y),
                    IM_COL32(255,255,255,(int)(ghost_alpha*255)));
            }
        } else if (tool_ == EditorTool::Paint && selected_tile_ > 0 &&
                   selected_tile_ <= tileset_->region_count()) {
            auto r = tileset_->region(selected_tile_ - 1);
            float ts = (float)map_->tile_size();
            ImGui::GetForegroundDrawList()->AddImage(
                tex_id, ImVec2(mouse.x, mouse.y), ImVec2(mouse.x+ts, mouse.y+ts),
                ImVec2(r.uv_min.x, r.uv_min.y), ImVec2(r.uv_max.x, r.uv_max.y),
                IM_COL32(255,255,255,(int)(ghost_alpha*255)));
        }
    }
}
#endif

} // namespace eb
