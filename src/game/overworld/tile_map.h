#pragma once

#include "engine/core/types.h"
#include <string>
#include <vector>

namespace eb {

class SpriteBatch;
class TextureAtlas;
class Camera;

// Tile rotation encoding — stored in upper bits of tile ID
// Bits 0-23:  tile ID (supports up to 16M tiles)
// Bits 24-25: rotation (0=0°, 1=90°CW, 2=180°, 3=270°CW)
// Bit 26:     flip horizontal
// Bit 27:     flip vertical
static constexpr int TILE_ID_MASK   = 0x00FFFFFF;
static constexpr int TILE_ROT_SHIFT = 24;
static constexpr int TILE_ROT_MASK  = 0x03000000;  // 2 bits for rotation
static constexpr int TILE_FLIP_H    = 0x04000000;  // Bit 26
static constexpr int TILE_FLIP_V    = 0x08000000;  // Bit 27

inline int tile_id(int raw)       { return raw & TILE_ID_MASK; }
inline int tile_rotation(int raw) { return (raw & TILE_ROT_MASK) >> TILE_ROT_SHIFT; }  // 0-3
inline bool tile_flip_h(int raw)  { return (raw & TILE_FLIP_H) != 0; }
inline bool tile_flip_v(int raw)  { return (raw & TILE_FLIP_V) != 0; }
inline int make_tile(int id, int rot = 0, bool fh = false, bool fv = false) {
    return (id & TILE_ID_MASK) | ((rot & 3) << TILE_ROT_SHIFT)
           | (fh ? TILE_FLIP_H : 0) | (fv ? TILE_FLIP_V : 0);
}

// Collision types for each tile
enum class CollisionType : uint8_t {
    None = 0,     // Passable
    Solid = 1,    // Can't walk through
    Portal = 2,   // Door/transition to another map
    OneWayUp = 3,    // Pass through from below, solid from above
    Slope45Up = 4,   // 45° slope rising left to right
    Slope45Down = 5, // 45° slope falling left to right
    Ladder = 6,      // Climbable
    Hazard = 7,      // Deals damage on contact
};

struct Portal {
    int tile_x = 0;
    int tile_y = 0;
    std::string target_map;
    int target_x = 0;
    int target_y = 0;
    std::string label;  // Optional display name
};

struct TileLayer {
    std::string name;
    std::vector<int> data; // tile IDs, 0 = empty
};

class TileMap {
public:
    TileMap();
    ~TileMap();

    // Create programmatically
    void create(int width, int height, int tile_size);
    void resize(int new_width, int new_height);
    void set_tileset(TextureAtlas* atlas);
    void set_tileset_path(const std::string& path) { tileset_path_ = path; }
    const std::string& tileset_path() const { return tileset_path_; }
    void add_layer(const std::string& name, const std::vector<int>& data);
    void set_collision(const std::vector<int>& data);

    // Tile editing
    void set_tile(int layer, int x, int y, int tile_id);
    void set_collision_at(int x, int y, CollisionType type);
    CollisionType collision_at(int x, int y) const;

    // Portal management
    int add_portal(const Portal& portal);
    void remove_portal(int index);
    Portal* get_portal_at(int tile_x, int tile_y);
    const Portal* get_portal_at(int tile_x, int tile_y) const;
    std::vector<Portal>& portals() { return portals_; }
    const std::vector<Portal>& portals() const { return portals_; }

    // Mark a range of tile IDs as animated (e.g. water)
    void set_animated_tiles(int first_id, int last_id);

    // Rendering — draws visible tiles through the sprite batch
    void render(SpriteBatch& batch, const Camera& camera, float time = 0.0f) const;

    // Collision queries
    bool is_solid(int tile_x, int tile_y) const;
    bool is_solid_world(float world_x, float world_y) const;

    // Serialization
    bool save_json(const std::string& path) const;
    bool load_json(const std::string& path);

    // Accessors
    int width() const { return width_; }
    int height() const { return height_; }
    int tile_size() const { return tile_size_; }
    float world_width() const { return static_cast<float>(width_ * tile_size_); }
    float world_height() const { return static_cast<float>(height_ * tile_size_); }
    int tile_at(int layer, int x, int y) const;  // Returns raw value (use tile_id() to extract ID)
    int layer_count() const { return static_cast<int>(layers_.size()); }
    std::vector<TileLayer>& layers() { return layers_; }
    const std::vector<TileLayer>& layers() const { return layers_; }
    const std::vector<CollisionType>& collision_types() const { return collision_types_; }

    // Reflection grid (per-tile: true = reflective surface like water/ice)
    void set_reflective(const std::vector<int>& data);
    void set_reflective_at(int x, int y, bool reflective);
    bool is_reflective(int x, int y) const;
    const std::vector<uint8_t>& reflection_grid() const { return reflection_grid_; }

private:
    int width_ = 0;
    int height_ = 0;
    int tile_size_ = 32;
    std::string tileset_path_;

    TextureAtlas* tileset_ = nullptr;
    std::vector<TileLayer> layers_;
    std::vector<CollisionType> collision_types_;
    std::vector<uint8_t> reflection_grid_;  // 1 = reflective, 0 = not
    std::vector<Portal> portals_;
    int animated_first_ = -1;
    int animated_last_ = -1;
};

} // namespace eb
