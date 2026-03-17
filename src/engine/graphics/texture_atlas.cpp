#include "engine/graphics/texture_atlas.h"
#include "engine/graphics/texture.h"
#include <algorithm>

namespace eb {

TextureAtlas::TextureAtlas(Texture* texture, int tile_width, int tile_height)
    : texture_(texture)
    , tile_width_(tile_width)
    , tile_height_(tile_height)
    , uniform_grid_(true) {
    columns_ = static_cast<int>(texture->width()) / tile_width;
    rows_ = static_cast<int>(texture->height()) / tile_height;
}

TextureAtlas::TextureAtlas(Texture* texture)
    : texture_(texture)
    , uniform_grid_(false) {}

AtlasRegion TextureAtlas::make_region(int x, int y, int w, int h) const {
    float tex_w = static_cast<float>(texture_->width());
    float tex_h = static_cast<float>(texture_->height());

    // Half-texel inset prevents sampling neighboring pixels at region edges
    float half_px_u = 0.5f / tex_w;
    float half_px_v = 0.5f / tex_h;

    AtlasRegion r;
    r.pixel_x = x;
    r.pixel_y = y;
    r.pixel_w = w;
    r.pixel_h = h;
    r.uv_min = {static_cast<float>(x) / tex_w + half_px_u,
                static_cast<float>(y) / tex_h + half_px_v};
    r.uv_max = {static_cast<float>(x + w) / tex_w - half_px_u,
                static_cast<float>(y + h) / tex_h - half_px_v};
    return r;
}

AtlasRegion TextureAtlas::region(int index) const {
    // Check custom regions first
    if (!custom_regions_.empty() && index >= 0 &&
        index < static_cast<int>(custom_regions_.size())) {
        return custom_regions_[index];
    }

    // Fall back to uniform grid
    if (uniform_grid_ && columns_ > 0) {
        int col = index % columns_;
        int row = index / columns_;
        return region(col, row);
    }

    // Return zero region for invalid index
    return {};
}

AtlasRegion TextureAtlas::region(int col, int row) const {
    return make_region(col * tile_width_, row * tile_height_, tile_width_, tile_height_);
}

int TextureAtlas::add_region(int x, int y, int w, int h) {
    int idx = static_cast<int>(custom_regions_.size());
    custom_regions_.push_back(make_region(x, y, w, h));
    return idx;
}

void TextureAtlas::define_region(const std::string& name, int x, int y, int w, int h) {
    named_regions_[name] = make_region(x, y, w, h);
}

const AtlasRegion* TextureAtlas::find_region(const std::string& name) const {
    auto it = named_regions_.find(name);
    return it != named_regions_.end() ? &it->second : nullptr;
}

std::vector<std::string> TextureAtlas::region_names() const {
    std::vector<std::string> names;
    names.reserve(named_regions_.size());
    for (auto& kv : named_regions_) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace eb
