#pragma once

#include "engine/core/types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace eb {

class Texture;

struct AtlasRegion {
    Vec2 uv_min;
    Vec2 uv_max;
    int pixel_x, pixel_y;
    int pixel_w, pixel_h;
};

class TextureAtlas {
public:
    // Create from a uniform grid (e.g., tileset or sprite sheet)
    TextureAtlas(Texture* texture, int tile_width, int tile_height);

    // Create from a texture with custom regions (no uniform grid)
    explicit TextureAtlas(Texture* texture);

    ~TextureAtlas() = default;

    // Get region by grid index (row-major, 0-based) or custom index
    AtlasRegion region(int index) const;

    // Get region by grid coordinates (only for uniform grid mode)
    AtlasRegion region(int col, int row) const;

    // Define a custom indexed region by pixel coordinates
    // Returns the assigned index (0-based)
    int add_region(int x, int y, int w, int h);

    // Define a named region by pixel coordinates
    void define_region(const std::string& name, int x, int y, int w, int h);

    // Look up a named region
    const AtlasRegion* find_region(const std::string& name) const;

    // Get all named region keys
    std::vector<std::string> region_names() const;

    Texture* texture() const { return texture_; }
    int columns() const { return columns_; }
    int rows() const { return rows_; }
    int tile_width() const { return tile_width_; }
    int tile_height() const { return tile_height_; }
    int region_count() const { return static_cast<int>(custom_regions_.size()); }

private:
    AtlasRegion make_region(int x, int y, int w, int h) const;

    Texture* texture_;
    int tile_width_ = 0;
    int tile_height_ = 0;
    int columns_ = 0;
    int rows_ = 0;
    bool uniform_grid_ = false;
    std::vector<AtlasRegion> custom_regions_;
    std::unordered_map<std::string, AtlasRegion> named_regions_;
};

} // namespace eb
