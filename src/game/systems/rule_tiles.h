#pragma once

#include <vector>
#include <algorithm>

namespace eb {

struct RuleTileSet {
    int terrain_tile;
    int other_tile;
    int tiles[47];
    bool configured = false;
};

// Maps every 8-bit bitmask (256 values) to one of 47 blob tile cases.
// Bitmask bits:  NW=128 N=64 NE=32 W=16 E=8 SW=4 S=2 SE=1
// Corner bits are only relevant when both adjacent edge neighbors are present.
static const int BLOB_TO_INDEX[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x00-0x0F
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, // 0x10-0x1F (W only)
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x20-0x2F
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, // 0x30-0x3F
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, // 0x40-0x4F (N only)
     3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4, // 0x50-0x5F (N+W, +NW)
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, // 0x60-0x6F (N+NE)
     3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4, // 0x70-0x7F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x80-0x8F
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, // 0x90-0x9F
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0xA0-0xAF
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, // 0xB0-0xBF
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, // 0xC0-0xCF
     3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4, // 0xD0-0xDF
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, // 0xE0-0xEF
     3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4, // 0xF0-0xFF
};

// Proper 47-case blob lookup table.
// This maps the 256 possible 8-bit neighbor masks (after corner masking)
// to one of the 47 unique visual tile cases for blob autotiling.
//
// The canonical approach: mask out corners that don't have both adjacent edges,
// then map the resulting "reduced" bitmask to one of 47 unique cases.

namespace detail {

// Masks corner bits that aren't relevant (corner only matters if both adjacent edges present)
// Bits: NW=7 N=6 NE=5 W=4 E=3 SW=2 S=1 SE=0
inline int reduce_blob_mask(int mask) {
    // NW (bit 7) only relevant if N (bit 6) and W (bit 4) are set
    if (!(mask & 0x40) || !(mask & 0x10)) mask &= ~0x80;
    // NE (bit 5) only relevant if N (bit 6) and E (bit 3) are set
    if (!(mask & 0x40) || !(mask & 0x08)) mask &= ~0x20;
    // SW (bit 2) only relevant if S (bit 1) and W (bit 4) are set
    if (!(mask & 0x02) || !(mask & 0x10)) mask &= ~0x04;
    // SE (bit 0) only relevant if S (bit 1) and E (bit 3) are set
    if (!(mask & 0x02) || !(mask & 0x08)) mask &= ~0x01;
    return mask;
}

// Build a lookup from reduced mask to case index [0..46]
// The 47 unique reduced masks in sorted order form the cases.
inline const int* get_blob47_lut() {
    static int lut[256];
    static bool built = false;
    if (!built) {
        // Collect all unique reduced masks
        int unique[256];
        int count = 0;
        bool seen[256] = {};
        for (int m = 0; m < 256; ++m) {
            int r = reduce_blob_mask(m);
            if (!seen[r]) {
                seen[r] = true;
                unique[count++] = r;
            }
        }
        // Sort unique values to assign stable indices
        std::sort(unique, unique + count);
        // Build reverse map: reduced_mask -> index
        int index_map[256];
        for (int i = 0; i < 256; ++i) index_map[i] = 0;
        for (int i = 0; i < count; ++i) {
            index_map[unique[i]] = i;
        }
        // Fill LUT
        for (int m = 0; m < 256; ++m) {
            int r = reduce_blob_mask(m);
            lut[m] = index_map[r];
        }
        built = true;
    }
    return lut;
}

} // namespace detail

inline int resolve_blob_tile(const int* layer_data, int map_w, int map_h,
                              int tx, int ty, const RuleTileSet& rules) {
    if (!rules.configured) return layer_data[ty * map_w + tx];

    int center = layer_data[ty * map_w + tx];
    if (center != rules.terrain_tile) return center;

    auto is_terrain = [&](int x, int y) -> bool {
        if (x < 0 || x >= map_w || y < 0 || y >= map_h) return true; // treat edges as terrain
        return layer_data[y * map_w + x] == rules.terrain_tile;
    };

    // Build 8-bit bitmask: NW N NE W E SW S SE (bits 7..0)
    int mask = 0;
    if (is_terrain(tx - 1, ty - 1)) mask |= 0x80; // NW
    if (is_terrain(tx,     ty - 1)) mask |= 0x40; // N
    if (is_terrain(tx + 1, ty - 1)) mask |= 0x20; // NE
    if (is_terrain(tx - 1, ty    )) mask |= 0x10; // W
    if (is_terrain(tx + 1, ty    )) mask |= 0x08; // E
    if (is_terrain(tx - 1, ty + 1)) mask |= 0x04; // SW
    if (is_terrain(tx,     ty + 1)) mask |= 0x02; // S
    if (is_terrain(tx + 1, ty + 1)) mask |= 0x01; // SE

    const int* lut = detail::get_blob47_lut();
    int idx = lut[mask];
    if (idx < 0 || idx >= 47) idx = 0;

    return rules.tiles[idx];
}

inline void apply_blob_autotile(int* layer_data, int map_w, int map_h,
                                 const RuleTileSet& rules) {
    if (!rules.configured) return;

    // Work on a copy so neighbor reads aren't affected by writes
    std::vector<int> original(layer_data, layer_data + map_w * map_h);

    for (int y = 0; y < map_h; ++y) {
        for (int x = 0; x < map_w; ++x) {
            if (original[y * map_w + x] == rules.terrain_tile) {
                layer_data[y * map_w + x] = resolve_blob_tile(
                    original.data(), map_w, map_h, x, y, rules);
            }
        }
    }
}

} // namespace eb
