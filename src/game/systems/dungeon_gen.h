#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <queue>
#include "engine/core/types.h"

namespace eb {

struct DungeonRoom {
    int x, y, w, h;
    Vec2 center() const {
        return {static_cast<float>(x) + w * 0.5f,
                static_cast<float>(y) + h * 0.5f};
    }
};

struct DungeonConfig {
    int map_w = 64, map_h = 64;
    int min_room_size = 5, max_room_size = 12;
    int max_rooms = 15;
    int corridor_width = 2;
    int wall_tile = 1, floor_tile = 2, corridor_tile = 3;
    unsigned seed = 0;
};

struct DungeonResult {
    std::vector<int> tiles;
    std::vector<int> collision;
    std::vector<DungeonRoom> rooms;
    int width, height;
};

// ─── BSP Dungeon Generation ───

namespace detail {

struct BSPNode {
    int x, y, w, h;
    int left = -1, right = -1;
    DungeonRoom room = {0, 0, 0, 0};
    bool has_room = false;
};

inline void bsp_split(std::vector<BSPNode>& nodes, int idx, int min_size,
                       std::mt19937& rng) {
    BSPNode& node = nodes[idx];
    if (node.w <= min_size * 2 + 2 && node.h <= min_size * 2 + 2) return;

    bool split_h;
    if (node.w > node.h * 1.25f) split_h = false;
    else if (node.h > node.w * 1.25f) split_h = true;
    else split_h = std::uniform_int_distribution<>(0, 1)(rng) == 0;

    int max_split = (split_h ? node.h : node.w) - min_size - 1;
    if (max_split <= min_size + 1) return;

    int split = std::uniform_int_distribution<>(min_size + 1, max_split)(rng);

    BSPNode a, b;
    if (split_h) {
        a = {node.x, node.y, node.w, split, -1, -1, {}, false};
        b = {node.x, node.y + split, node.w, node.h - split, -1, -1, {}, false};
    } else {
        a = {node.x, node.y, split, node.h, -1, -1, {}, false};
        b = {node.x + split, node.y, node.w - split, node.h, -1, -1, {}, false};
    }

    node.left = static_cast<int>(nodes.size());
    nodes.push_back(a);
    node.right = static_cast<int>(nodes.size());
    nodes.push_back(b);

    bsp_split(nodes, node.left, min_size, rng);
    bsp_split(nodes, node.right, min_size, rng);
}

inline void bsp_place_rooms(std::vector<BSPNode>& nodes, int idx,
                             const DungeonConfig& cfg, std::mt19937& rng,
                             std::vector<DungeonRoom>& rooms) {
    BSPNode& node = nodes[idx];
    if (node.left >= 0 && node.right >= 0) {
        bsp_place_rooms(nodes, node.left, cfg, rng, rooms);
        bsp_place_rooms(nodes, node.right, cfg, rng, rooms);
        return;
    }

    // Leaf node — place a room
    if (static_cast<int>(rooms.size()) >= cfg.max_rooms) return;

    int max_w = std::min(cfg.max_room_size, node.w - 2);
    int max_h = std::min(cfg.max_room_size, node.h - 2);
    if (max_w < cfg.min_room_size || max_h < cfg.min_room_size) return;

    int rw = std::uniform_int_distribution<>(cfg.min_room_size, max_w)(rng);
    int rh = std::uniform_int_distribution<>(cfg.min_room_size, max_h)(rng);
    int rx = node.x + std::uniform_int_distribution<>(1, node.w - rw - 1)(rng);
    int ry = node.y + std::uniform_int_distribution<>(1, node.h - rh - 1)(rng);

    DungeonRoom room = {rx, ry, rw, rh};
    node.room = room;
    node.has_room = true;
    rooms.push_back(room);
}

inline DungeonRoom bsp_get_room(std::vector<BSPNode>& nodes, int idx) {
    BSPNode& node = nodes[idx];
    if (node.has_room) return node.room;
    if (node.left >= 0) {
        DungeonRoom r = bsp_get_room(nodes, node.left);
        if (r.w > 0) return r;
    }
    if (node.right >= 0) {
        return bsp_get_room(nodes, node.right);
    }
    return {0, 0, 0, 0};
}

inline void bsp_connect(std::vector<BSPNode>& nodes, int idx,
                          std::vector<int>& tiles, const DungeonConfig& cfg,
                          std::mt19937& rng) {
    BSPNode& node = nodes[idx];
    if (node.left < 0 || node.right < 0) return;

    bsp_connect(nodes, node.left, tiles, cfg, rng);
    bsp_connect(nodes, node.right, tiles, cfg, rng);

    DungeonRoom ra = bsp_get_room(nodes, node.left);
    DungeonRoom rb = bsp_get_room(nodes, node.right);
    if (ra.w <= 0 || rb.w <= 0) return;

    Vec2 ca = ra.center();
    Vec2 cb = rb.center();
    int cx1 = static_cast<int>(ca.x);
    int cy1 = static_cast<int>(ca.y);
    int cx2 = static_cast<int>(cb.x);
    int cy2 = static_cast<int>(cb.y);

    // L-shaped corridor
    bool horiz_first = std::uniform_int_distribution<>(0, 1)(rng) == 0;

    auto carve_h = [&](int from_x, int to_x, int y) {
        int lo = std::min(from_x, to_x);
        int hi = std::max(from_x, to_x);
        for (int x = lo; x <= hi; ++x) {
            for (int dw = 0; dw < cfg.corridor_width; ++dw) {
                int py = y + dw;
                if (x >= 0 && x < cfg.map_w && py >= 0 && py < cfg.map_h) {
                    int idx = py * cfg.map_w + x;
                    if (tiles[idx] == cfg.wall_tile)
                        tiles[idx] = cfg.corridor_tile;
                }
            }
        }
    };
    auto carve_v = [&](int from_y, int to_y, int x) {
        int lo = std::min(from_y, to_y);
        int hi = std::max(from_y, to_y);
        for (int y = lo; y <= hi; ++y) {
            for (int dw = 0; dw < cfg.corridor_width; ++dw) {
                int px = x + dw;
                if (px >= 0 && px < cfg.map_w && y >= 0 && y < cfg.map_h) {
                    int idx = y * cfg.map_w + px;
                    if (tiles[idx] == cfg.wall_tile)
                        tiles[idx] = cfg.corridor_tile;
                }
            }
        }
    };

    if (horiz_first) {
        carve_h(cx1, cx2, cy1);
        carve_v(cy1, cy2, cx2);
    } else {
        carve_v(cy1, cy2, cx1);
        carve_h(cx1, cx2, cy2);
    }
}

} // namespace detail

inline DungeonResult generate_dungeon_bsp(const DungeonConfig& cfg) {
    DungeonResult result;
    result.width = cfg.map_w;
    result.height = cfg.map_h;
    result.tiles.assign(cfg.map_w * cfg.map_h, cfg.wall_tile);
    result.collision.assign(cfg.map_w * cfg.map_h, 1);

    std::mt19937 rng(cfg.seed ? cfg.seed : std::random_device{}());

    std::vector<detail::BSPNode> nodes;
    nodes.push_back({0, 0, cfg.map_w, cfg.map_h, -1, -1, {}, false});
    detail::bsp_split(nodes, 0, cfg.min_room_size, rng);
    detail::bsp_place_rooms(nodes, 0, cfg, rng, result.rooms);

    // Carve rooms
    for (auto& room : result.rooms) {
        for (int ry = room.y; ry < room.y + room.h; ++ry) {
            for (int rx = room.x; rx < room.x + room.w; ++rx) {
                if (rx >= 0 && rx < cfg.map_w && ry >= 0 && ry < cfg.map_h) {
                    result.tiles[ry * cfg.map_w + rx] = cfg.floor_tile;
                    result.collision[ry * cfg.map_w + rx] = 0;
                }
            }
        }
    }

    // Connect rooms via BSP tree
    detail::bsp_connect(nodes, 0, result.tiles, cfg, rng);

    // Mark corridor tiles as non-collision
    for (int i = 0; i < cfg.map_w * cfg.map_h; ++i) {
        if (result.tiles[i] == cfg.corridor_tile) {
            result.collision[i] = 0;
        }
    }

    return result;
}

// ─── Cellular Automata Dungeon Generation ───

inline DungeonResult generate_dungeon_cellular(const DungeonConfig& cfg) {
    DungeonResult result;
    result.width = cfg.map_w;
    result.height = cfg.map_h;
    result.tiles.assign(cfg.map_w * cfg.map_h, cfg.wall_tile);
    result.collision.assign(cfg.map_w * cfg.map_h, 1);

    std::mt19937 rng(cfg.seed ? cfg.seed : std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    int w = cfg.map_w;
    int h = cfg.map_h;

    // Random fill ~45% alive (wall=1, floor=0)
    std::vector<int> grid(w * h, 1);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            grid[y * w + x] = (dist(rng) < 0.45f) ? 1 : 0;
        }
    }

    // 4-5 rule: become wall if >=5 wall neighbors (including self counts as 9-cell neighborhood)
    auto count_walls = [&](const std::vector<int>& g, int cx, int cy) -> int {
        int count = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                    count++; // Out of bounds = wall
                else
                    count += g[ny * w + nx];
            }
        }
        return count;
    };

    // 4 iterations of cellular automata
    for (int iter = 0; iter < 4; ++iter) {
        std::vector<int> next(w * h, 1);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int walls = count_walls(grid, x, y);
                // 4-5 rule: wall if >= 5 neighbors are walls
                next[y * w + x] = (walls >= 5) ? 1 : 0;
            }
        }
        grid = next;
    }

    // Flood fill to find connected regions of floor (0)
    std::vector<int> labels(w * h, -1);
    std::vector<std::vector<std::pair<int,int>>> regions;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (grid[y * w + x] == 0 && labels[y * w + x] < 0) {
                int region_id = static_cast<int>(regions.size());
                regions.push_back({});
                std::queue<std::pair<int,int>> q;
                q.push({x, y});
                labels[y * w + x] = region_id;
                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    regions[region_id].push_back({cx, cy});
                    const int dx[] = {1, -1, 0, 0};
                    const int dy[] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                            grid[ny * w + nx] == 0 && labels[ny * w + nx] < 0) {
                            labels[ny * w + nx] = region_id;
                            q.push({nx, ny});
                        }
                    }
                }
            }
        }
    }

    // Find the largest region
    int largest = -1;
    size_t largest_size = 0;
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].size() > largest_size) {
            largest_size = regions[i].size();
            largest = static_cast<int>(i);
        }
    }

    // Connect smaller regions to largest via corridors
    if (largest >= 0) {
        for (size_t i = 0; i < regions.size(); ++i) {
            if (static_cast<int>(i) == largest) continue;
            if (regions[i].empty()) continue;

            // Find closest pair of cells between this region and the largest
            int best_dist = w * h;
            std::pair<int,int> best_a = regions[i][0];
            std::pair<int,int> best_b = regions[largest][0];

            // Sample to keep performance reasonable
            int step_i = std::max(1, static_cast<int>(regions[i].size()) / 50);
            int step_l = std::max(1, static_cast<int>(regions[largest].size()) / 50);

            for (size_t ai = 0; ai < regions[i].size(); ai += step_i) {
                for (size_t bi = 0; bi < regions[largest].size(); bi += step_l) {
                    auto& a = regions[i][ai];
                    auto& b = regions[largest][bi];
                    int d = std::abs(a.first - b.first) + std::abs(a.second - b.second);
                    if (d < best_dist) {
                        best_dist = d;
                        best_a = a;
                        best_b = b;
                    }
                }
            }

            // Carve corridor between best_a and best_b
            int cx = best_a.first, cy = best_a.second;
            int tx = best_b.first, ty = best_b.second;
            while (cx != tx || cy != ty) {
                for (int dw = 0; dw < cfg.corridor_width; ++dw) {
                    for (int dh = 0; dh < cfg.corridor_width; ++dh) {
                        int px = cx + dw, py = cy + dh;
                        if (px >= 0 && px < w && py >= 0 && py < h) {
                            grid[py * w + px] = 0;
                        }
                    }
                }
                if (cx != tx) cx += (tx > cx) ? 1 : -1;
                else if (cy != ty) cy += (ty > cy) ? 1 : -1;
            }
        }
    }

    // Write tiles
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (grid[idx] == 0) {
                result.tiles[idx] = cfg.floor_tile;
                result.collision[idx] = 0;
            } else {
                result.tiles[idx] = cfg.wall_tile;
                result.collision[idx] = 1;
            }
        }
    }

    // Place rooms at cavern centers (find connected regions again after corridor carving)
    // Re-flood-fill for final room placement
    std::vector<int> final_labels(w * h, -1);
    std::vector<std::vector<std::pair<int,int>>> final_regions;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (grid[y * w + x] == 0 && final_labels[y * w + x] < 0) {
                int rid = static_cast<int>(final_regions.size());
                final_regions.push_back({});
                std::queue<std::pair<int,int>> q;
                q.push({x, y});
                final_labels[y * w + x] = rid;
                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    final_regions[rid].push_back({cx, cy});
                    const int dx[] = {1, -1, 0, 0};
                    const int dy[] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                            grid[ny * w + nx] == 0 && final_labels[ny * w + nx] < 0) {
                            final_labels[ny * w + nx] = rid;
                            q.push({nx, ny});
                        }
                    }
                }
            }
        }
    }

    // Create DungeonRoom entries centered on each region's bounding box
    for (auto& region : final_regions) {
        if (region.size() < static_cast<size_t>(cfg.min_room_size * cfg.min_room_size))
            continue;
        int min_x = w, min_y = h, max_x = 0, max_y = 0;
        for (auto& [px, py] : region) {
            min_x = std::min(min_x, px);
            min_y = std::min(min_y, py);
            max_x = std::max(max_x, px);
            max_y = std::max(max_y, py);
        }
        result.rooms.push_back({min_x, min_y, max_x - min_x + 1, max_y - min_y + 1});
        if (static_cast<int>(result.rooms.size()) >= cfg.max_rooms) break;
    }

    return result;
}

} // namespace eb
