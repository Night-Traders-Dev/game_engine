#pragma once

#include "engine/core/types.h"
#include "game/overworld/tile_map.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct ObjectStamp;  // Forward declaration from game.h
struct GameState;    // Forward declaration from game.h

namespace eb {

class SpriteBatch;
class TextureAtlas;
class Texture;
class TextRenderer;
class Renderer;
class Camera;
struct InputState;

enum class EditorTool {
    Paint,
    Erase,
    Fill,
    Eyedrop,
    Select,
    Collision,
    Line,       // Draw straight lines
    Rect,       // Draw filled/outlined rectangles
    Portal,     // Place entrance/exit teleport tiles
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
    int width = 0, height = 0;
    bool has_data = false;
};

// Undo/redo action
struct EditorAction {
    struct TileChange {
        int layer, x, y;
        int old_tile, new_tile;
    };
    struct CollisionChange {
        int x, y;
        CollisionType old_type, new_type;
    };
    struct ObjectChange {
        int obj_id;       // sprite_id
        float x, y;       // position
        bool added;        // true = added, false = removed
    };
    std::vector<TileChange> tile_changes;
    std::vector<CollisionChange> collision_changes;
    std::vector<ObjectChange> object_changes;
    std::string description;
};

class TileEditor {
public:
    TileEditor();
    ~TileEditor();

    void set_map(TileMap* map);
    void set_tileset(TextureAtlas* atlas, VkDescriptorSet desc, Texture* tex = nullptr);
    void set_text_renderer(TextRenderer* text, VkDescriptorSet font_desc);
    void set_object_stamps(const std::vector<ObjectStamp>* stamps) { object_stamps_ = stamps; }
    void set_game_state(GameState* game) { game_state_ = game; }
    void set_renderer(eb::Renderer* r) { renderer_ = r; }

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
    float zoom() const { return zoom_; }

    // ImGui-based editor UI rendering
    void render_imgui(GameState& game);

    bool save_map(const std::string& path) const;
    bool load_map(const std::string& path);

    // Map script (Visual Basic-style editor code generation)
    void load_map_script(const std::string& map_path);

private:
    void handle_palette_click(float mx, float my, int screen_w, int screen_h);
    void handle_map_click(float mx, float my, const Camera& camera, bool is_drag);
    void handle_map_right_click(float mx, float my, const Camera& camera);
    void handle_shortcuts(const InputState& input);

    // Tools with undo support
    void paint_tile(int tx, int ty);
    void erase_tile(int tx, int ty);
    void flood_fill(int tx, int ty, int new_tile);
    void cycle_collision(int tx, int ty);
    void commit_line(int x1, int y1, int x2, int y2, int tile);
    void commit_rect(int x1, int y1, int x2, int y2, int tile, bool filled);
    void import_asset_dialog();  // Import single tile / tileset / sprite

    // Undo/Redo
    void begin_action(const std::string& desc);
    void record_tile_change(int layer, int x, int y, int old_t, int new_t);
    void record_collision_change(int x, int y, CollisionType old_c, CollisionType new_c);
    void commit_action();
    void undo();
    void redo();

    // Selection / clipboard
    void copy_selection();
    void paste_at(int tx, int ty);
    void clear_selection();
    void flip_clipboard_h();
    void flip_clipboard_v();

    // Coordinate conversion
    Vec2 screen_to_world(float sx, float sy, const Camera& camera) const;
    Vec2i world_to_tile(Vec2 world) const;

    // Editor panels
    struct EditorPanel {
        float x, y, w, h;
        bool dragging = false;
        float drag_ox = 0, drag_oy = 0;
        const char* title = "";
    };

    // Rendering helpers
    void render_grid(SpriteBatch& batch, const Camera& camera) const;
    void render_collision_overlay(SpriteBatch& batch, const Camera& camera) const;
    void render_selection(SpriteBatch& batch, const Camera& camera) const;
    void render_cursor(SpriteBatch& batch, const Camera& camera, float mx, float my) const;
    void render_tool_panel(SpriteBatch& batch, int screen_w, int screen_h) const;
    void render_tile_panel(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                           int screen_w, int screen_h) const;
    void render_object_panel(SpriteBatch& batch, VkDescriptorSet tileset_desc,
                             const EditorPanel& panel, const char* category,
                             int screen_w, int screen_h) const;
    void render_status_bar(SpriteBatch& batch, int screen_w, int screen_h) const;
    void render_panel_bg(SpriteBatch& batch, const EditorPanel& panel) const;
    bool point_in_panel(const EditorPanel& panel, float px, float py) const;
    bool point_in_panel_title(const EditorPanel& panel, float px, float py) const;
    void init_panels(int screen_w, int screen_h);

    // State
    bool active_ = false;
    EditorTool tool_ = EditorTool::Paint;
    int selected_tile_ = 1;
    int tile_rotation_ = 0;     // Current brush rotation (0-3: 0°/90°/180°/270°)
    bool tile_flip_h_ = false;  // Current brush horizontal flip
    bool tile_flip_v_ = false;  // Current brush vertical flip
    int active_layer_ = 0;
    bool show_grid_ = true;
    bool show_collision_ = false;
    bool rect_filled_ = true;

    // Layer visibility
    static constexpr int MAX_LAYERS = 9;
    bool layer_visible_[MAX_LAYERS] = {true,true,true,true,true,true,true,true,true};

    // Zoom
    float zoom_ = 1.0f;
    static constexpr float ZOOM_MIN = 0.25f;
    static constexpr float ZOOM_MAX = 4.0f;

    EditorPanel tool_panel_;
    EditorPanel tile_panel_;
    EditorPanel building_panel_;
    EditorPanel vehicle_panel_;
    EditorPanel tree_panel_;
    EditorPanel misc_panel_;
    std::vector<EditorPanel*> all_panels_;

    // Object stamp placement
    int selected_stamp_ = -1; // Index into game's object_stamps
    bool stamp_mode_ = false;
    int palette_scroll_ = 0;
    int palette_cols_ = 4;
    static constexpr float PALETTE_TILE_SIZE = 44.0f;
    static constexpr float PANEL_TITLE_H = 28.0f;
    static constexpr float PANEL_PAD = 6.0f;
    static constexpr float STATUS_BAR_HEIGHT = 26.0f;
    bool panels_initialized_ = false;

    // Map reference
    TileMap* map_ = nullptr;
    TextureAtlas* tileset_ = nullptr;
    Texture* tileset_texture_ = nullptr;
    VkDescriptorSet tileset_desc_ = VK_NULL_HANDLE;
    const std::vector<ObjectStamp>* object_stamps_ = nullptr;
    GameState* game_state_ = nullptr;
    Renderer* renderer_ = nullptr;

    // ImGui texture ID for tileset previews
    void* imgui_tileset_id_ = nullptr;
    bool imgui_texture_registered_ = false;
    void ensure_imgui_texture();

    // Text renderer (optional, for labels)
    TextRenderer* text_ = nullptr;
    VkDescriptorSet font_desc_ = VK_NULL_HANDLE;

    // Selection and clipboard
    TileSelection selection_;
    Clipboard clipboard_;

    // Camera pan state
    bool panning_ = false;
    Vec2 pan_start_ = {0, 0};
    Vec2 camera_start_ = {0, 0};

    // Drag painting state
    Vec2i last_paint_tile_ = {-1, -1};

    // Mouse state
    float mouse_x_ = 0, mouse_y_ = 0;

    // Undo/Redo
    std::vector<EditorAction> undo_stack_;
    std::vector<EditorAction> redo_stack_;
    EditorAction current_action_;
    bool action_in_progress_ = false;
    static constexpr int MAX_UNDO = 100;

    // Status message
    std::string status_msg_;
    float status_timer_ = 0.0f;
    void set_status(const std::string& msg);

    // Brush size (1=1x1, 2=2x2, 3=3x3)
    int brush_size_ = 1;

    // Asset tab selection (0=Tiles, 1=Buildings, 2=Furniture, 3=Characters, 4=Trees, 5=Vehicles, 6=Misc)
    int asset_tab_index_ = -1;  // -1 = no forced tab, 0-6 = force select that tab
    static constexpr int ASSET_TAB_COUNT = 7;

    // Window visibility toggles
    bool show_tools_window_ = true;
    bool show_assets_window_ = true;
    bool show_minimap_window_ = false;
    bool show_npc_spawner_ = false;
    bool show_script_ide_ = false;
    bool show_debug_console_ = false;

    // Script IDE: asset highlight + API manual
    std::string highlighted_asset_;
    float highlight_timer_ = 0.0f;
    bool show_api_manual_ = false;

    // Map script generation (Visual Basic-style)
    std::string map_script_path_;
    std::string map_script_body_;   // Lines inside map_init()
    bool map_script_dirty_ = false;
    void append_map_script(const std::string& line);
    void save_map_script();

    // Auto-tiling configuration
    bool auto_tile_enabled_ = false;
    // Stores which tile IDs are "terrain A" vs "terrain B" for auto-transitions
    struct AutoTileConfig {
        int terrain_a_tile = 1;     // Primary terrain (e.g., grass)
        int terrain_b_tile = 2;     // Secondary terrain (e.g., dirt)
        int transition_start = 0;   // First transition tile ID (16 tiles in sequence)
        bool configured = false;
    };
    AutoTileConfig auto_tile_config_;

    // Object inspector
    bool show_object_inspector_ = false;
    int selected_world_object_ = -1;  // Index into game's world objects

    // Prefab system
    struct Prefab {
        std::string name;
        std::vector<int> tiles;
        std::vector<CollisionType> collision;
        int width = 0, height = 0;
    };
    std::vector<Prefab> prefabs_;
    int selected_prefab_ = -1;
    bool show_prefab_panel_ = false;

    // Map resize pending
    bool pending_resize_ = false;
    int resize_new_w_ = 0;
    int resize_new_h_ = 0;

    // Deferred file dialog (must not run during Vulkan rendering)
    enum class PendingDialog { None, Save, Load, ImportAsset };
    PendingDialog pending_dialog_ = PendingDialog::None;
public:
    void process_pending_dialog();

    // Auto-tiling
    void set_auto_tile(bool enabled) { auto_tile_enabled_ = enabled; }
    bool auto_tile_enabled() const { return auto_tile_enabled_; }

    // Map resize
    void resize_map(int new_w, int new_h);

    // Prefab
    void save_selection_as_prefab(const std::string& name);
    void paste_prefab(int tx, int ty, int prefab_index);
private:
};

} // namespace eb
