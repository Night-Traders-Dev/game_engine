#pragma once

#include "engine/core/types.h"
#include <string>
#include <vector>

namespace eb {

class SpriteBatch;
class TextureAtlas;
class Camera;

// Collision types for each tile
enum class CollisionType : uint8_t {
    None = 0,     // Passable
    Solid = 1,    // Can't walk through
    Portal = 2,   // Door/transition to another map
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

    // Rendering — draws visible tiles through the sprite batch
    void render(SpriteBatch& batch, const Camera& camera) const;

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
    int tile_at(int layer, int x, int y) const;
    int layer_count() const { return static_cast<int>(layers_.size()); }
    std::vector<TileLayer>& layers() { return layers_; }
    const std::vector<TileLayer>& layers() const { return layers_; }
    const std::vector<CollisionType>& collision_types() const { return collision_types_; }

private:
    int width_ = 0;
    int height_ = 0;
    int tile_size_ = 32;
    std::string tileset_path_;

    TextureAtlas* tileset_ = nullptr;
    std::vector<TileLayer> layers_;
    std::vector<CollisionType> collision_types_;
    std::vector<Portal> portals_;
};

} // namespace eb
