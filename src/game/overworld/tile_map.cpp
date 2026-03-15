#include "game/overworld/tile_map.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/texture_atlas.h"
#include "game/overworld/camera.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace eb {

TileMap::TileMap() = default;
TileMap::~TileMap() = default;

void TileMap::create(int width, int height, int tile_size) {
    width_ = width;
    height_ = height;
    tile_size_ = tile_size;
    layers_.clear();
    collision_types_.assign(width * height, CollisionType::None);
    portals_.clear();
}

void TileMap::resize(int new_width, int new_height) {
    for (auto& layer : layers_) {
        std::vector<int> new_data(new_width * new_height, 0);
        int copy_w = std::min(width_, new_width);
        int copy_h = std::min(height_, new_height);
        for (int y = 0; y < copy_h; y++) {
            for (int x = 0; x < copy_w; x++) {
                new_data[y * new_width + x] = layer.data[y * width_ + x];
            }
        }
        layer.data = std::move(new_data);
    }

    std::vector<CollisionType> new_col(new_width * new_height, CollisionType::None);
    int copy_w = std::min(width_, new_width);
    int copy_h = std::min(height_, new_height);
    for (int y = 0; y < copy_h; y++) {
        for (int x = 0; x < copy_w; x++) {
            new_col[y * new_width + x] = collision_types_[y * width_ + x];
        }
    }
    collision_types_ = std::move(new_col);

    width_ = new_width;
    height_ = new_height;
}

void TileMap::set_tileset(TextureAtlas* atlas) {
    tileset_ = atlas;
}

void TileMap::add_layer(const std::string& name, const std::vector<int>& data) {
    layers_.push_back({name, data});
}

void TileMap::set_collision(const std::vector<int>& data) {
    collision_types_.resize(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        collision_types_[i] = (data[i] != 0) ? CollisionType::Solid : CollisionType::None;
    }
}

void TileMap::set_tile(int layer, int x, int y, int tile_id) {
    if (layer < 0 || layer >= static_cast<int>(layers_.size())) return;
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    layers_[layer].data[y * width_ + x] = tile_id;
}

void TileMap::set_collision_at(int x, int y, CollisionType type) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    collision_types_[y * width_ + x] = type;
}

CollisionType TileMap::collision_at(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return CollisionType::Solid;
    return collision_types_[y * width_ + x];
}

int TileMap::add_portal(const Portal& portal) {
    portals_.push_back(portal);
    set_collision_at(portal.tile_x, portal.tile_y, CollisionType::Portal);
    return static_cast<int>(portals_.size()) - 1;
}

void TileMap::remove_portal(int index) {
    if (index < 0 || index >= static_cast<int>(portals_.size())) return;
    auto& p = portals_[index];
    set_collision_at(p.tile_x, p.tile_y, CollisionType::None);
    portals_.erase(portals_.begin() + index);
}

Portal* TileMap::get_portal_at(int tile_x, int tile_y) {
    for (auto& p : portals_) {
        if (p.tile_x == tile_x && p.tile_y == tile_y) return &p;
    }
    return nullptr;
}

const Portal* TileMap::get_portal_at(int tile_x, int tile_y) const {
    for (const auto& p : portals_) {
        if (p.tile_x == tile_x && p.tile_y == tile_y) return &p;
    }
    return nullptr;
}

void TileMap::set_animated_tiles(int first_id, int last_id) {
    animated_first_ = first_id;
    animated_last_ = last_id;
}

void TileMap::render(SpriteBatch& batch, const Camera& camera, float time) const {
    if (!tileset_ || layers_.empty()) return;

    Rect view = camera.visible_area();
    float ts = static_cast<float>(tile_size_);

    int start_x = std::max(0, static_cast<int>(std::floor(view.x / ts)));
    int start_y = std::max(0, static_cast<int>(std::floor(view.y / ts)));
    int end_x = std::min(width_, static_cast<int>(std::ceil((view.x + view.w) / ts)) + 1);
    int end_y = std::min(height_, static_cast<int>(std::ceil((view.y + view.h) / ts)) + 1);

    for (const auto& layer : layers_) {
        for (int y = start_y; y < end_y; y++) {
            for (int x = start_x; x < end_x; x++) {
                int tile_id = layer.data[y * width_ + x];
                if (tile_id <= 0) continue;

                auto region = tileset_->region(tile_id - 1);

                Vec2 pos = {x * ts, y * ts};
                Vec2 size = {ts, ts};

                bool is_animated = (animated_first_ >= 0 &&
                                    tile_id >= animated_first_ &&
                                    tile_id <= animated_last_);

                if (is_animated && time > 0.0f) {
                    // Wave effect: offset position and shimmer color
                    float phase = x * 0.8f + y * 1.2f + time * 2.5f;
                    float wave_y = std::sin(phase) * 1.5f;
                    float wave_x = std::cos(phase * 0.7f + 0.5f) * 0.8f;
                    pos.y += wave_y;
                    pos.x += wave_x;

                    // UV distortion for ripple effect
                    float uv_w = region.uv_max.x - region.uv_min.x;
                    float uv_h = region.uv_max.y - region.uv_min.y;
                    float uv_off = std::sin(time * 1.8f + x * 1.5f) * uv_w * 0.06f;
                    region.uv_min.x += uv_off;
                    region.uv_max.x += uv_off;

                    // Color shimmer
                    float shimmer = 0.85f + 0.15f * std::sin(time * 3.0f + x * 0.9f + y * 1.1f);
                    Vec4 color = {shimmer * 0.8f, shimmer * 0.9f, shimmer, 1.0f};
                    batch.draw_quad(pos, size, region.uv_min, region.uv_max, color);
                } else {
                    batch.draw_quad(pos, size, region.uv_min, region.uv_max);
                }
            }
        }
    }
}

bool TileMap::is_solid(int tile_x, int tile_y) const {
    if (tile_x < 0 || tile_x >= width_ || tile_y < 0 || tile_y >= height_) {
        return true;
    }
    if (collision_types_.empty()) return false;
    return collision_types_[tile_y * width_ + tile_x] == CollisionType::Solid;
}

bool TileMap::is_solid_world(float world_x, float world_y) const {
    int tx = static_cast<int>(std::floor(world_x / tile_size_));
    int ty = static_cast<int>(std::floor(world_y / tile_size_));
    return is_solid(tx, ty);
}

int TileMap::tile_at(int layer, int x, int y) const {
    if (layer < 0 || layer >= static_cast<int>(layers_.size())) return 0;
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return 0;
    return layers_[layer].data[y * width_ + x];
}

// ── JSON helpers (minimal, no external dependency) ──

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

bool TileMap::save_json(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[TileMap] Failed to save: %s\n", path.c_str());
        return false;
    }

    f << "{\n";
    f << "  \"version\": 1,\n";
    f << "  \"width\": " << width_ << ",\n";
    f << "  \"height\": " << height_ << ",\n";
    f << "  \"tile_size\": " << tile_size_ << ",\n";
    f << "  \"tileset\": \"" << escape_json(tileset_path_) << "\",\n";

    // Layers
    f << "  \"layers\": [\n";
    for (size_t li = 0; li < layers_.size(); li++) {
        const auto& layer = layers_[li];
        f << "    {\n";
        f << "      \"name\": \"" << escape_json(layer.name) << "\",\n";
        f << "      \"data\": [";
        for (size_t i = 0; i < layer.data.size(); i++) {
            if (i > 0) f << ",";
            if (i % width_ == 0) f << "\n        ";
            f << layer.data[i];
        }
        f << "\n      ]\n";
        f << "    }";
        if (li + 1 < layers_.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Collision
    f << "  \"collision\": [";
    for (size_t i = 0; i < collision_types_.size(); i++) {
        if (i > 0) f << ",";
        if (i % width_ == 0) f << "\n    ";
        f << static_cast<int>(collision_types_[i]);
    }
    f << "\n  ],\n";

    // Portals
    f << "  \"portals\": [\n";
    for (size_t pi = 0; pi < portals_.size(); pi++) {
        const auto& p = portals_[pi];
        f << "    {";
        f << "\"x\":" << p.tile_x << ",";
        f << "\"y\":" << p.tile_y << ",";
        f << "\"target_map\":\"" << escape_json(p.target_map) << "\",";
        f << "\"target_x\":" << p.target_x << ",";
        f << "\"target_y\":" << p.target_y << ",";
        f << "\"label\":\"" << escape_json(p.label) << "\"";
        f << "}";
        if (pi + 1 < portals_.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n";

    f << "}\n";

    std::printf("[TileMap] Saved: %s (%dx%d, %zu layers, %zu portals)\n",
                path.c_str(), width_, height_, layers_.size(), portals_.size());
    return true;
}

// ── Minimal JSON parser ──

static void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
}

static std::string parse_string(const std::string& s, size_t& i) {
    if (s[i] != '"') return "";
    i++;
    std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            i++;
            switch (s[i]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
        i++;
    }
    if (i < s.size()) i++; // skip closing "
    return out;
}

static int parse_int(const std::string& s, size_t& i) {
    int sign = 1;
    if (i < s.size() && s[i] == '-') { sign = -1; i++; }
    int val = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        val = val * 10 + (s[i] - '0');
        i++;
    }
    return val * sign;
}

static std::vector<int> parse_int_array(const std::string& s, size_t& i) {
    std::vector<int> arr;
    if (s[i] != '[') return arr;
    i++;
    while (i < s.size() && s[i] != ']') {
        skip_ws(s, i);
        if (s[i] == ']') break;
        arr.push_back(parse_int(s, i));
        skip_ws(s, i);
        if (s[i] == ',') i++;
    }
    if (i < s.size()) i++; // skip ]
    return arr;
}

static void skip_value(const std::string& s, size_t& i);

static void skip_object(const std::string& s, size_t& i) {
    if (s[i] != '{') return;
    i++;
    while (i < s.size() && s[i] != '}') {
        skip_ws(s, i);
        if (s[i] == '}') break;
        parse_string(s, i); // key
        skip_ws(s, i);
        if (s[i] == ':') i++;
        skip_ws(s, i);
        skip_value(s, i);
        skip_ws(s, i);
        if (s[i] == ',') i++;
    }
    if (i < s.size()) i++;
}

static void skip_array(const std::string& s, size_t& i) {
    if (s[i] != '[') return;
    i++;
    while (i < s.size() && s[i] != ']') {
        skip_ws(s, i);
        if (s[i] == ']') break;
        skip_value(s, i);
        skip_ws(s, i);
        if (s[i] == ',') i++;
    }
    if (i < s.size()) i++;
}

static void skip_value(const std::string& s, size_t& i) {
    skip_ws(s, i);
    if (i >= s.size()) return;
    if (s[i] == '"') { parse_string(s, i); }
    else if (s[i] == '{') { skip_object(s, i); }
    else if (s[i] == '[') { skip_array(s, i); }
    else {
        // number or bool or null
        while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
               s[i] != ' ' && s[i] != '\n' && s[i] != '\r' && s[i] != '\t') i++;
    }
}

static Portal parse_portal(const std::string& s, size_t& i) {
    Portal p;
    if (s[i] != '{') return p;
    i++;
    while (i < s.size() && s[i] != '}') {
        skip_ws(s, i);
        if (s[i] == '}') break;
        std::string key = parse_string(s, i);
        skip_ws(s, i);
        if (s[i] == ':') i++;
        skip_ws(s, i);
        if (key == "x") p.tile_x = parse_int(s, i);
        else if (key == "y") p.tile_y = parse_int(s, i);
        else if (key == "target_map") p.target_map = parse_string(s, i);
        else if (key == "target_x") p.target_x = parse_int(s, i);
        else if (key == "target_y") p.target_y = parse_int(s, i);
        else if (key == "label") p.label = parse_string(s, i);
        else skip_value(s, i);
        skip_ws(s, i);
        if (s[i] == ',') i++;
    }
    if (i < s.size()) i++;
    return p;
}

bool TileMap::load_json(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[TileMap] Failed to load: %s\n", path.c_str());
        return false;
    }

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    size_t i = 0;
    skip_ws(json, i);
    if (json[i] != '{') return false;
    i++;

    int version = 0;
    int w = 0, h = 0, ts = 32;
    std::string tileset;

    layers_.clear();
    collision_types_.clear();
    portals_.clear();

    while (i < json.size() && json[i] != '}') {
        skip_ws(json, i);
        if (json[i] == '}') break;
        std::string key = parse_string(json, i);
        skip_ws(json, i);
        if (json[i] == ':') i++;
        skip_ws(json, i);

        if (key == "version" || key == "format") {
            skip_value(json, i);
        } else if (key == "metadata") {
            // save_map_file wraps width/height/tile_size/tileset in metadata object
            if (json[i] == '{') {
                i++;
                while (i < json.size() && json[i] != '}') {
                    skip_ws(json, i);
                    if (json[i] == '}') break;
                    std::string mkey = parse_string(json, i);
                    skip_ws(json, i);
                    if (json[i] == ':') i++;
                    skip_ws(json, i);
                    if (mkey == "width") w = parse_int(json, i);
                    else if (mkey == "height") h = parse_int(json, i);
                    else if (mkey == "tile_size") ts = parse_int(json, i);
                    else if (mkey == "tileset") tileset = parse_string(json, i);
                    else skip_value(json, i);
                    skip_ws(json, i);
                    if (json[i] == ',') i++;
                }
                if (i < json.size()) i++; // skip }
            }
        } else if (key == "width") {
            w = parse_int(json, i);
        } else if (key == "height") {
            h = parse_int(json, i);
        } else if (key == "tile_size") {
            ts = parse_int(json, i);
        } else if (key == "tileset") {
            tileset = parse_string(json, i);
        } else if (key == "layers") {
            // Array of layer objects
            if (json[i] == '[') {
                i++;
                while (i < json.size() && json[i] != ']') {
                    skip_ws(json, i);
                    if (json[i] == ']') break;
                    if (json[i] == '{') {
                        i++;
                        TileLayer layer;
                        while (i < json.size() && json[i] != '}') {
                            skip_ws(json, i);
                            if (json[i] == '}') break;
                            std::string lkey = parse_string(json, i);
                            skip_ws(json, i);
                            if (json[i] == ':') i++;
                            skip_ws(json, i);
                            if (lkey == "name") layer.name = parse_string(json, i);
                            else if (lkey == "data") layer.data = parse_int_array(json, i);
                            else skip_value(json, i);
                            skip_ws(json, i);
                            if (json[i] == ',') i++;
                        }
                        if (i < json.size()) i++; // skip }
                        layers_.push_back(std::move(layer));
                    }
                    skip_ws(json, i);
                    if (json[i] == ',') i++;
                }
                if (i < json.size()) i++; // skip ]
            }
        } else if (key == "collision") {
            auto col = parse_int_array(json, i);
            collision_types_.resize(col.size());
            for (size_t ci = 0; ci < col.size(); ci++) {
                int cv = col[ci];
                collision_types_[ci] = (cv >= 0 && cv <= 2)
                    ? static_cast<CollisionType>(cv) : CollisionType::None;
            }
        } else if (key == "portals") {
            if (json[i] == '[') {
                i++;
                while (i < json.size() && json[i] != ']') {
                    skip_ws(json, i);
                    if (json[i] == ']') break;
                    portals_.push_back(parse_portal(json, i));
                    skip_ws(json, i);
                    if (json[i] == ',') i++;
                }
                if (i < json.size()) i++;
            }
        } else {
            skip_value(json, i);
        }
        skip_ws(json, i);
        if (json[i] == ',') i++;
    }

    width_ = w;
    height_ = h;
    tile_size_ = ts;
    tileset_path_ = tileset;

    if (collision_types_.empty()) {
        collision_types_.assign(w * h, CollisionType::None);
    }

    (void)version;
    std::printf("[TileMap] Loaded: %s (%dx%d, %zu layers, %zu portals)\n",
                path.c_str(), w, h, layers_.size(), portals_.size());
    return true;
}

} // namespace eb
