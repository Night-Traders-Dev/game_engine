#pragma once

#include "engine/core/types.h"
#include "game/overworld/tile_map.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace eb {

class SpriteBatch;
class TextureAtlas;
class Camera;
struct InputState;

enum class EditorTool {
    Paint,      // Place tiles
    Erase,      // Remove tiles
    Fill,       // Flood fill
    Eyedrop,    // Pick tile from map
    Select,     // Select rectangular region
    Collision,  // Edit collision types
};

struct TileSelection {
    int x1 = -1, y1 = -1;
    int x2 = -1, y2 = -1;
    bool active = false;

    int left() const { return std::min(x1, x2); }
    int top() const { return std::min(y1, y2); }
    int right() const { return std::max(x1, x2); }
    int bottom() const { return std::max(y1, y2); }
    int sel_width() const { return right() - left() + 1; }
    int sel_height() const { return bottom() - top() + 1; }
};

struct Clipboard {
    std::vector<int> tiles;
    std::vector<CollisionType> collision;
    int width = 0;
    int height = 0;
    bool has_data = false;
};

class TileEditor {
public:
    TileEditor();
    ~TileEditor();

    void set_map(TileMap* map);
    void set_tileset(TextureAtlas* atlas, VkDescriptorSet desc);

    void update(const InputState& input, Camera& camera, float dt,
                int screen_w, int screen_h);
    void render(SpriteBatch& batch, const Camera& camera,
                VkDescriptorSet tileset_desc, int screen_w, int screen_h);

    bool is_active() const { return active_; }
    void set_active(bool active) { active_ = active; }
    void toggle() { active_ = !active_; }

    EditorTool current_tool() const { return tool_; }
    int selected_tile() const { return selected_tile_; }
    int active_layer() const { return active_layer_; }
    bool show_grid() const { return show_grid_; }
    bool show_collision() const { return show_collision_; }

    // Save/Load
    bool save_map(const std::string& path) const;
    bool load_map(const std::string& path);

private:
    // Input handling
    void handle_palette_click(float mx, float my, int screen_w, int screen_h);
    void handle_map_click(float mx, float my, const Camera& camera, bool is_drag);
    void handle_map_right_click(float mx, float my, const Camera& camera);
    void handle_shortcuts(const InputState& input);

    // Tools
    void paint_tile(int tx, int ty);
    void erase_tile(int tx, int ty);
    void flood_fill(int tx, int ty, int new_tile);
    void cycle_collision(int tx, int ty);

    // Selection / clipboard
    void copy_selection();
    void paste_at(int tx, int ty);
    void clear_selection();

    // Coordinate conversion
    Vec2 screen_to_world(float sx, float sy, const Camera& camera) const;
    Vec2i world_to_tile(Vec2 world) const;

    // Rendering helpers
    void render_grid(SpriteBatch& batch, const Camera& camera) const;
    void render_collision_overlay(SpriteBatch& batch, const Camera& camera) const;
    void render_selection(SpriteBatch& batch, const Camera& camera) const;
    void render_cursor(SpriteBatch& batch, const Camera& camera,
                       float mx, float my) const;
    void render_palette(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                        int screen_w, int screen_h) const;
    void render_toolbar(SpriteBatch& batch, int screen_w, int screen_h) const;

    // State
    bool active_ = false;
    EditorTool tool_ = EditorTool::Paint;
    int selected_tile_ = 1;  // 1-indexed tile ID
    int active_layer_ = 0;
    bool show_grid_ = true;
    bool show_collision_ = false;

    // Palette
    int palette_scroll_ = 0;
    int palette_cols_ = 4;
    static constexpr int PALETTE_WIDTH = 160;
    static constexpr int PALETTE_TILE_SIZE = 36;
    static constexpr int PALETTE_PADDING = 4;
    static constexpr int TOOLBAR_HEIGHT = 40;

    // Map reference
    TileMap* map_ = nullptr;
    TextureAtlas* tileset_ = nullptr;
    VkDescriptorSet tileset_desc_ = VK_NULL_HANDLE;

    // Selection and clipboard
    TileSelection selection_;
    Clipboard clipboard_;

    // Camera pan state
    bool panning_ = false;
    Vec2 pan_start_ = {0, 0};
    Vec2 camera_start_ = {0, 0};

    // Drag painting state
    Vec2i last_paint_tile_ = {-1, -1};

    // Mouse state tracking
    float mouse_x_ = 0, mouse_y_ = 0;
};

} // namespace eb
