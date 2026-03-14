#include "editor/tile_editor.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/texture_atlas.h"
#include "engine/platform/input.h"
#include "game/overworld/camera.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <queue>

namespace eb {

TileEditor::TileEditor() = default;
TileEditor::~TileEditor() = default;

void TileEditor::set_map(TileMap* map) {
    map_ = map;
    selection_ = {};
    clipboard_ = {};
    last_paint_tile_ = {-1, -1};
}

void TileEditor::set_tileset(TextureAtlas* atlas, VkDescriptorSet desc) {
    tileset_ = atlas;
    tileset_desc_ = desc;
}

// ── Coordinate conversion ──

Vec2 TileEditor::screen_to_world(float sx, float sy, const Camera& camera) const {
    Vec2 cam_off = camera.offset();
    return {sx + cam_off.x, sy + cam_off.y};
}

Vec2i TileEditor::world_to_tile(Vec2 world) const {
    if (!map_) return {0, 0};
    int ts = map_->tile_size();
    return {static_cast<int>(std::floor(world.x / ts)),
            static_cast<int>(std::floor(world.y / ts))};
}

// ── Update ──

void TileEditor::update(const InputState& input, Camera& camera, float /*dt*/,
                         int screen_w, int screen_h) {
    if (!active_ || !map_) return;

    mouse_x_ = input.mouse.x;
    mouse_y_ = input.mouse.y;

    handle_shortcuts(input);

    // Scroll palette with mouse wheel when cursor is over palette
    bool over_palette = mouse_x_ >= screen_w - PALETTE_WIDTH;
    if (over_palette && input.mouse.scroll_y != 0.0f) {
        palette_scroll_ -= static_cast<int>(input.mouse.scroll_y * 2);
        if (palette_scroll_ < 0) palette_scroll_ = 0;
    }

    // Middle mouse: pan camera
    if (input.mouse.is_pressed(MouseButton::Middle)) {
        panning_ = true;
        pan_start_ = {mouse_x_, mouse_y_};
        camera_start_ = camera.position();
    }
    if (panning_ && input.mouse.is_held(MouseButton::Middle)) {
        float dx = mouse_x_ - pan_start_.x;
        float dy = mouse_y_ - pan_start_.y;
        camera.set_position({camera_start_.x - dx, camera_start_.y - dy});
    }
    if (input.mouse.is_released(MouseButton::Middle)) {
        panning_ = false;
    }

    // Zoom with scroll wheel when not over palette
    if (!over_palette && input.mouse.scroll_y != 0.0f) {
        // Could implement zoom here in the future
    }

    // Left click
    if (over_palette) {
        if (input.mouse.is_pressed(MouseButton::Left)) {
            handle_palette_click(mouse_x_, mouse_y_, screen_w, screen_h);
        }
    } else {
        if (input.mouse.is_pressed(MouseButton::Left)) {
            last_paint_tile_ = {-1, -1};
            handle_map_click(mouse_x_, mouse_y_, camera, false);
        } else if (input.mouse.is_held(MouseButton::Left)) {
            handle_map_click(mouse_x_, mouse_y_, camera, true);
        }
        if (input.mouse.is_released(MouseButton::Left)) {
            last_paint_tile_ = {-1, -1};
        }
    }

    // Right click: erase
    if (!over_palette) {
        if (input.mouse.is_pressed(MouseButton::Right) ||
            input.mouse.is_held(MouseButton::Right)) {
            handle_map_right_click(mouse_x_, mouse_y_, camera);
        }
    }
}

void TileEditor::handle_shortcuts(const InputState& input) {
    // Tool shortcuts
    if (input.key_pressed(GLFW_KEY_P)) tool_ = EditorTool::Paint;
    if (input.key_pressed(GLFW_KEY_E) && !input.mods.ctrl) tool_ = EditorTool::Erase;
    if (input.key_pressed(GLFW_KEY_F)) tool_ = EditorTool::Fill;
    if (input.key_pressed(GLFW_KEY_I)) tool_ = EditorTool::Eyedrop;
    if (input.key_pressed(GLFW_KEY_R)) tool_ = EditorTool::Select;
    if (input.key_pressed(GLFW_KEY_C) && !input.mods.ctrl) tool_ = EditorTool::Collision;

    // Toggle overlays
    if (input.key_pressed(GLFW_KEY_G)) show_grid_ = !show_grid_;
    if (input.key_pressed(GLFW_KEY_V) && !input.mods.ctrl) show_collision_ = !show_collision_;

    // Layer switching (1-9)
    for (int k = GLFW_KEY_1; k <= GLFW_KEY_9; k++) {
        if (input.key_pressed(k)) {
            int layer = k - GLFW_KEY_1;
            if (layer < map_->layer_count()) {
                active_layer_ = layer;
            }
        }
    }

    // Ctrl+C: copy
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_C)) {
        copy_selection();
    }
    // Ctrl+V: paste
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_V)) {
        Vec2 world = screen_to_world(mouse_x_, mouse_y_, *static_cast<Camera*>(nullptr));
        // Paste handled on next click instead
    }
    // Ctrl+S: save
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_S)) {
        save_map("assets/maps/current.json");
    }
    // Ctrl+L: load
    if (input.mods.ctrl && input.key_pressed(GLFW_KEY_L)) {
        load_map("assets/maps/current.json");
    }

    // Delete selection content
    if (input.key_pressed(GLFW_KEY_DELETE) && selection_.active) {
        for (int y = selection_.top(); y <= selection_.bottom(); y++) {
            for (int x = selection_.left(); x <= selection_.right(); x++) {
                map_->set_tile(active_layer_, x, y, 0);
            }
        }
    }

    // Escape: clear selection
    if (input.key_pressed(GLFW_KEY_ESCAPE)) {
        clear_selection();
    }
}

// ── Palette interaction ──

void TileEditor::handle_palette_click(float mx, float my, int screen_w, int /*screen_h*/) {
    float palette_x = static_cast<float>(screen_w - PALETTE_WIDTH + PALETTE_PADDING);
    float palette_y = static_cast<float>(TOOLBAR_HEIGHT + PALETTE_PADDING);

    float rel_x = mx - palette_x;
    float rel_y = my - palette_y + palette_scroll_ * PALETTE_TILE_SIZE;

    if (rel_x < 0 || rel_y < 0) return;

    int col = static_cast<int>(rel_x / PALETTE_TILE_SIZE);
    int row = static_cast<int>(rel_y / PALETTE_TILE_SIZE);

    if (col >= palette_cols_) return;

    int tile_index = row * palette_cols_ + col;
    int max_tiles = tileset_ ? tileset_->region_count() : 0;
    if (max_tiles == 0 && tileset_) {
        max_tiles = tileset_->columns() * tileset_->rows();
    }

    if (tile_index >= 0 && tile_index < max_tiles) {
        selected_tile_ = tile_index + 1; // 1-indexed
        tool_ = EditorTool::Paint;
    }
}

// ── Map interaction ──

void TileEditor::handle_map_click(float mx, float my, const Camera& camera, bool is_drag) {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);

    if (tile.x < 0 || tile.x >= map_->width() || tile.y < 0 || tile.y >= map_->height())
        return;

    // Skip if same tile as last drag position (avoid redundant operations)
    if (is_drag && tile.x == last_paint_tile_.x && tile.y == last_paint_tile_.y)
        return;
    last_paint_tile_ = tile;

    switch (tool_) {
        case EditorTool::Paint:
            if (clipboard_.has_data && !is_drag) {
                paste_at(tile.x, tile.y);
                clipboard_.has_data = false;
            } else {
                paint_tile(tile.x, tile.y);
            }
            break;
        case EditorTool::Erase:
            erase_tile(tile.x, tile.y);
            break;
        case EditorTool::Fill:
            if (!is_drag) {
                flood_fill(tile.x, tile.y, selected_tile_);
            }
            break;
        case EditorTool::Eyedrop:
            if (!is_drag) {
                int picked = map_->tile_at(active_layer_, tile.x, tile.y);
                if (picked > 0) {
                    selected_tile_ = picked;
                    tool_ = EditorTool::Paint;
                }
            }
            break;
        case EditorTool::Select:
            if (!is_drag) {
                selection_.x1 = tile.x;
                selection_.y1 = tile.y;
                selection_.x2 = tile.x;
                selection_.y2 = tile.y;
                selection_.active = true;
            } else {
                selection_.x2 = tile.x;
                selection_.y2 = tile.y;
            }
            break;
        case EditorTool::Collision:
            if (!is_drag) {
                cycle_collision(tile.x, tile.y);
            }
            break;
    }
}

void TileEditor::handle_map_right_click(float mx, float my, const Camera& camera) {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);
    if (tile.x == last_paint_tile_.x && tile.y == last_paint_tile_.y) return;
    last_paint_tile_ = tile;
    erase_tile(tile.x, tile.y);
}

// ── Tool operations ──

void TileEditor::paint_tile(int tx, int ty) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    map_->set_tile(active_layer_, tx, ty, selected_tile_);
}

void TileEditor::erase_tile(int tx, int ty) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    map_->set_tile(active_layer_, tx, ty, 0);
}

void TileEditor::flood_fill(int tx, int ty, int new_tile) {
    if (!map_ || active_layer_ < 0 || active_layer_ >= map_->layer_count()) return;
    int old_tile = map_->tile_at(active_layer_, tx, ty);
    if (old_tile == new_tile) return;

    std::queue<Vec2i> q;
    q.push({tx, ty});

    int w = map_->width(), h = map_->height();
    std::vector<bool> visited(w * h, false);

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        if (cx < 0 || cx >= w || cy < 0 || cy >= h) continue;
        int idx = cy * w + cx;
        if (visited[idx]) continue;
        if (map_->tile_at(active_layer_, cx, cy) != old_tile) continue;

        visited[idx] = true;
        map_->set_tile(active_layer_, cx, cy, new_tile);

        q.push({cx - 1, cy});
        q.push({cx + 1, cy});
        q.push({cx, cy - 1});
        q.push({cx, cy + 1});
    }
}

void TileEditor::cycle_collision(int tx, int ty) {
    if (!map_) return;
    auto current = map_->collision_at(tx, ty);
    CollisionType next;
    switch (current) {
        case CollisionType::None:   next = CollisionType::Solid; break;
        case CollisionType::Solid:  next = CollisionType::Portal; break;
        case CollisionType::Portal:
            // Remove any portal at this position
            for (int i = static_cast<int>(map_->portals().size()) - 1; i >= 0; i--) {
                if (map_->portals()[i].tile_x == tx && map_->portals()[i].tile_y == ty) {
                    map_->remove_portal(i);
                }
            }
            next = CollisionType::None;
            break;
    }
    map_->set_collision_at(tx, ty, next);

    // If setting to portal, create a default portal entry
    if (next == CollisionType::Portal) {
        Portal p;
        p.tile_x = tx;
        p.tile_y = ty;
        p.target_map = "";
        p.target_x = 0;
        p.target_y = 0;
        p.label = "portal";
        map_->portals().push_back(p);
    }
}

// ── Selection / Clipboard ──

void TileEditor::copy_selection() {
    if (!map_ || !selection_.active) return;

    int sw = selection_.sel_width();
    int sh = selection_.sel_height();
    int sx = selection_.left();
    int sy = selection_.top();

    clipboard_.width = sw;
    clipboard_.height = sh;
    clipboard_.tiles.resize(sw * sh);
    clipboard_.collision.resize(sw * sh);
    clipboard_.has_data = true;

    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            clipboard_.tiles[y * sw + x] = map_->tile_at(active_layer_, sx + x, sy + y);
            clipboard_.collision[y * sw + x] = map_->collision_at(sx + x, sy + y);
        }
    }
    std::printf("[Editor] Copied %dx%d region\n", sw, sh);
}

void TileEditor::paste_at(int tx, int ty) {
    if (!map_ || !clipboard_.has_data) return;

    for (int y = 0; y < clipboard_.height; y++) {
        for (int x = 0; x < clipboard_.width; x++) {
            int tile_id = clipboard_.tiles[y * clipboard_.width + x];
            map_->set_tile(active_layer_, tx + x, ty + y, tile_id);
            map_->set_collision_at(tx + x, ty + y, clipboard_.collision[y * clipboard_.width + x]);
        }
    }
    std::printf("[Editor] Pasted %dx%d region at (%d,%d)\n",
                clipboard_.width, clipboard_.height, tx, ty);
}

void TileEditor::clear_selection() {
    selection_ = {};
}

// ── Save/Load ──

bool TileEditor::save_map(const std::string& path) const {
    if (!map_) return false;
    return map_->save_json(path);
}

bool TileEditor::load_map(const std::string& path) {
    if (!map_) return false;
    return map_->load_json(path);
}

// ── Rendering ──

void TileEditor::render(SpriteBatch& batch, const Camera& camera,
                         VkDescriptorSet tileset_desc, int screen_w, int screen_h) {
    if (!active_ || !map_) return;

    // World-space overlays (grid, collision, selection, cursor)
    batch.set_projection(camera.projection_matrix());

    if (show_grid_) render_grid(batch, camera);
    if (show_collision_) render_collision_overlay(batch, camera);
    if (selection_.active) render_selection(batch, camera);
    render_cursor(batch, camera, mouse_x_, mouse_y_);

    // Screen-space UI (palette, toolbar)
    // Switch to screen projection for UI
    Mat4 screen_proj = glm::ortho(0.0f, static_cast<float>(screen_w),
                                   0.0f, static_cast<float>(screen_h),
                                   -1.0f, 1.0f);
    batch.set_projection(screen_proj);

    render_palette(batch, tileset_desc, screen_w, screen_h);
    render_toolbar(batch, screen_w, screen_h);
}

void TileEditor::render_grid(SpriteBatch& batch, const Camera& camera) const {
    Rect view = camera.visible_area();
    float ts = static_cast<float>(map_->tile_size());

    int start_x = std::max(0, static_cast<int>(std::floor(view.x / ts)));
    int start_y = std::max(0, static_cast<int>(std::floor(view.y / ts)));
    int end_x = std::min(map_->width(), static_cast<int>(std::ceil((view.x + view.w) / ts)) + 1);
    int end_y = std::min(map_->height(), static_cast<int>(std::ceil((view.y + view.h) / ts)) + 1);

    Vec4 grid_color = {1.0f, 1.0f, 1.0f, 0.15f};
    float line_w = 1.0f;

    // Vertical lines
    for (int x = start_x; x <= end_x; x++) {
        Vec2 pos = {x * ts - line_w * 0.5f, start_y * ts};
        Vec2 size = {line_w, (end_y - start_y) * ts};
        batch.draw_quad(pos, size, grid_color);
    }

    // Horizontal lines
    for (int y = start_y; y <= end_y; y++) {
        Vec2 pos = {start_x * ts, y * ts - line_w * 0.5f};
        Vec2 size = {(end_x - start_x) * ts, line_w};
        batch.draw_quad(pos, size, grid_color);
    }
}

void TileEditor::render_collision_overlay(SpriteBatch& batch, const Camera& camera) const {
    Rect view = camera.visible_area();
    float ts = static_cast<float>(map_->tile_size());

    int start_x = std::max(0, static_cast<int>(std::floor(view.x / ts)));
    int start_y = std::max(0, static_cast<int>(std::floor(view.y / ts)));
    int end_x = std::min(map_->width(), static_cast<int>(std::ceil((view.x + view.w) / ts)) + 1);
    int end_y = std::min(map_->height(), static_cast<int>(std::ceil((view.y + view.h) / ts)) + 1);

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            auto ct = map_->collision_at(x, y);
            Vec4 color;
            switch (ct) {
                case CollisionType::None: continue;
                case CollisionType::Solid:
                    color = {1.0f, 0.2f, 0.2f, 0.35f}; // Red
                    break;
                case CollisionType::Portal:
                    color = {0.2f, 0.5f, 1.0f, 0.45f}; // Blue
                    break;
            }
            Vec2 pos = {x * ts + 2.0f, y * ts + 2.0f};
            Vec2 size = {ts - 4.0f, ts - 4.0f};
            batch.draw_quad(pos, size, color);
        }
    }
}

void TileEditor::render_selection(SpriteBatch& batch, const Camera& camera) const {
    float ts = static_cast<float>(map_->tile_size());
    int sx = selection_.left();
    int sy = selection_.top();
    int sw = selection_.sel_width();
    int sh = selection_.sel_height();

    Vec4 sel_color = {0.3f, 0.8f, 1.0f, 0.25f};
    Vec4 border_color = {0.3f, 0.8f, 1.0f, 0.7f};

    // Fill
    Vec2 pos = {sx * ts, sy * ts};
    Vec2 size = {sw * ts, sh * ts};
    batch.draw_quad(pos, size, sel_color);

    // Border lines (2px)
    float bw = 2.0f;
    batch.draw_quad({pos.x, pos.y}, {size.x, bw}, border_color);         // top
    batch.draw_quad({pos.x, pos.y + size.y - bw}, {size.x, bw}, border_color); // bottom
    batch.draw_quad({pos.x, pos.y}, {bw, size.y}, border_color);         // left
    batch.draw_quad({pos.x + size.x - bw, pos.y}, {bw, size.y}, border_color); // right
}

void TileEditor::render_cursor(SpriteBatch& batch, const Camera& camera,
                                float mx, float my) const {
    Vec2 world = screen_to_world(mx, my, camera);
    Vec2i tile = world_to_tile(world);

    if (tile.x < 0 || tile.x >= map_->width() || tile.y < 0 || tile.y >= map_->height())
        return;

    float ts = static_cast<float>(map_->tile_size());
    Vec2 pos = {tile.x * ts, tile.y * ts};

    // Cursor highlight
    Vec4 cursor_color;
    switch (tool_) {
        case EditorTool::Paint:     cursor_color = {1.0f, 1.0f, 1.0f, 0.3f}; break;
        case EditorTool::Erase:     cursor_color = {1.0f, 0.3f, 0.3f, 0.3f}; break;
        case EditorTool::Fill:      cursor_color = {0.3f, 1.0f, 0.3f, 0.3f}; break;
        case EditorTool::Eyedrop:   cursor_color = {1.0f, 1.0f, 0.3f, 0.3f}; break;
        case EditorTool::Select:    cursor_color = {0.3f, 0.8f, 1.0f, 0.3f}; break;
        case EditorTool::Collision: cursor_color = {1.0f, 0.5f, 0.0f, 0.3f}; break;
    }

    batch.draw_quad(pos, {ts, ts}, cursor_color);

    // Border
    Vec4 border = {cursor_color.x, cursor_color.y, cursor_color.z, 0.8f};
    float bw = 2.0f;
    batch.draw_quad({pos.x, pos.y}, {ts, bw}, border);
    batch.draw_quad({pos.x, pos.y + ts - bw}, {ts, bw}, border);
    batch.draw_quad({pos.x, pos.y}, {bw, ts}, border);
    batch.draw_quad({pos.x + ts - bw, pos.y}, {bw, ts}, border);

    // If painting, show preview of the tile to be placed
    if (tool_ == EditorTool::Paint && tileset_ && selected_tile_ > 0) {
        auto region = tileset_->region(selected_tile_ - 1);
        batch.draw_quad(pos, {ts, ts}, region.uv_min, region.uv_max,
                        {1.0f, 1.0f, 1.0f, 0.5f});
    }
}

void TileEditor::render_palette(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                                 int screen_w, int screen_h) const {
    if (!tileset_) return;

    float px = static_cast<float>(screen_w - PALETTE_WIDTH);
    float py = static_cast<float>(TOOLBAR_HEIGHT);
    float pw = static_cast<float>(PALETTE_WIDTH);
    float ph = static_cast<float>(screen_h - TOOLBAR_HEIGHT);

    // Background
    batch.draw_quad({px, py}, {pw, ph}, {0.15f, 0.15f, 0.2f, 0.9f});

    // Separator line
    batch.draw_quad({px, py}, {2.0f, ph}, {0.4f, 0.4f, 0.5f, 1.0f});

    // Draw tile palette entries
    int max_tiles = tileset_->region_count();
    if (max_tiles == 0) {
        max_tiles = tileset_->columns() * tileset_->rows();
    }

    float tile_sz = static_cast<float>(PALETTE_TILE_SIZE);
    float pad = static_cast<float>(PALETTE_PADDING);
    float start_x = px + pad;
    float start_y = py + pad;

    // Switch to tileset texture for tile previews
    batch.set_texture(tileset_desc);

    for (int i = 0; i < max_tiles; i++) {
        int col = i % palette_cols_;
        int row = i / palette_cols_ - palette_scroll_;

        if (row < 0) continue;
        float ty = start_y + row * tile_sz;
        if (ty + tile_sz > static_cast<float>(screen_h)) break;

        float tx = start_x + col * tile_sz;

        auto region = tileset_->region(i);

        // Tile background
        batch.draw_quad({tx, ty}, {tile_sz - 2, tile_sz - 2},
                        {0.1f, 0.1f, 0.12f, 1.0f});

        // Tile image
        batch.draw_quad({tx + 1, ty + 1}, {tile_sz - 4, tile_sz - 4},
                        region.uv_min, region.uv_max);

        // Selection highlight
        if (i + 1 == selected_tile_) {
            batch.draw_quad({tx - 1, ty - 1}, {tile_sz, 2.0f},
                            {1.0f, 0.8f, 0.2f, 1.0f});
            batch.draw_quad({tx - 1, ty + tile_sz - 3}, {tile_sz, 2.0f},
                            {1.0f, 0.8f, 0.2f, 1.0f});
            batch.draw_quad({tx - 1, ty - 1}, {2.0f, tile_sz},
                            {1.0f, 0.8f, 0.2f, 1.0f});
            batch.draw_quad({tx + tile_sz - 3, ty - 1}, {2.0f, tile_sz},
                            {1.0f, 0.8f, 0.2f, 1.0f});
        }
    }
}

void TileEditor::render_toolbar(SpriteBatch& batch, int screen_w, int /*screen_h*/) const {
    float tw = static_cast<float>(screen_w);
    float th = static_cast<float>(TOOLBAR_HEIGHT);

    // Background
    batch.draw_quad({0, 0}, {tw, th}, {0.12f, 0.12f, 0.18f, 0.92f});

    // Separator
    batch.draw_quad({0, th - 2.0f}, {tw, 2.0f}, {0.4f, 0.4f, 0.5f, 1.0f});

    // Tool indicators (colored squares)
    struct ToolDef {
        EditorTool tool;
        Vec4 color;
        float x;
    };
    ToolDef tools[] = {
        {EditorTool::Paint,     {0.2f, 0.8f, 0.2f, 1.0f}, 10.0f},
        {EditorTool::Erase,     {0.9f, 0.2f, 0.2f, 1.0f}, 46.0f},
        {EditorTool::Fill,      {0.2f, 0.6f, 0.9f, 1.0f}, 82.0f},
        {EditorTool::Eyedrop,   {0.9f, 0.9f, 0.2f, 1.0f}, 118.0f},
        {EditorTool::Select,    {0.2f, 0.9f, 0.9f, 1.0f}, 154.0f},
        {EditorTool::Collision, {0.9f, 0.5f, 0.1f, 1.0f}, 190.0f},
    };

    for (const auto& td : tools) {
        float bx = td.x;
        float by = 6.0f;
        float bs = 28.0f;

        // Active tool highlight
        if (td.tool == tool_) {
            batch.draw_quad({bx - 2, by - 2}, {bs + 4, bs + 4},
                            {1.0f, 1.0f, 1.0f, 0.8f});
        }

        batch.draw_quad({bx, by}, {bs, bs}, td.color);
    }

    // Status indicators on the right side of toolbar
    float sx = tw - PALETTE_WIDTH - 200.0f;

    // Grid toggle indicator
    Vec4 grid_col = show_grid_ ? Vec4(0.3f, 1.0f, 0.3f, 1.0f) : Vec4(0.4f, 0.4f, 0.4f, 1.0f);
    batch.draw_quad({sx, 10.0f}, {20.0f, 20.0f}, grid_col);

    // Collision toggle indicator
    Vec4 col_col = show_collision_ ? Vec4(1.0f, 0.5f, 0.1f, 1.0f) : Vec4(0.4f, 0.4f, 0.4f, 1.0f);
    batch.draw_quad({sx + 28.0f, 10.0f}, {20.0f, 20.0f}, col_col);

    // Layer indicator
    for (int i = 0; i < map_->layer_count(); i++) {
        Vec4 lc = (i == active_layer_)
            ? Vec4(0.3f, 0.7f, 1.0f, 1.0f)
            : Vec4(0.3f, 0.3f, 0.4f, 1.0f);
        batch.draw_quad({sx + 64.0f + i * 24.0f, 10.0f}, {20.0f, 20.0f}, lc);
    }

    // Clipboard indicator
    if (clipboard_.has_data) {
        batch.draw_quad({sx + 140.0f, 10.0f}, {20.0f, 20.0f},
                        {0.8f, 0.2f, 0.8f, 1.0f});
    }
}

} // namespace eb
