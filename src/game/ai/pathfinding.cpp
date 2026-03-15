#include "game/ai/pathfinding.h"
#include "game/game.h"

#include <queue>
#include <cmath>
#include <algorithm>

namespace eb {

struct AStarNode {
    int x, y;
    float g, f;  // g = cost from start, f = g + heuristic
    int parent;  // index into closed list
};

struct CompareF {
    bool operator()(const std::pair<float, int>& a, const std::pair<float, int>& b) const {
        return a.first > b.first;
    }
};

std::vector<PathNode> find_path(const TileMap& map, int sx, int sy, int ex, int ey,
                                 int max_steps) {
    int w = map.width(), h = map.height();

    // Bounds check
    if (sx < 0 || sx >= w || sy < 0 || sy >= h) return {};
    if (ex < 0 || ex >= w || ey < 0 || ey >= h) return {};
    if (map.is_solid(ex, ey)) return {};

    // Already there
    if (sx == ex && sy == ey) return {{sx, sy}};

    // Grid for visited state + g-cost
    std::vector<float> g_cost(w * h, 1e9f);
    std::vector<int> parent(w * h, -1);
    std::vector<bool> closed(w * h, false);

    auto idx = [w](int x, int y) { return y * w + x; };
    auto heuristic = [](int x1, int y1, int x2, int y2) -> float {
        return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
    };

    // Open set: (f_score, grid_index)
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, CompareF> open;

    int start_idx = idx(sx, sy);
    g_cost[start_idx] = 0;
    open.push({heuristic(sx, sy, ex, ey), start_idx});

    // 4-directional neighbors
    static const int dx[] = {0, 0, -1, 1};
    static const int dy[] = {-1, 1, 0, 0};

    int steps = 0;
    while (!open.empty() && steps < max_steps) {
        auto [f, ci] = open.top();
        open.pop();

        if (closed[ci]) continue;
        closed[ci] = true;
        steps++;

        int cx = ci % w;
        int cy = ci / w;

        // Found goal
        if (cx == ex && cy == ey) {
            // Reconstruct path
            std::vector<PathNode> path;
            int pi = ci;
            while (pi != -1) {
                path.push_back({pi % w, pi / w});
                pi = parent[pi];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        // Expand neighbors
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (map.is_solid(nx, ny)) continue;

            int ni = idx(nx, ny);
            if (closed[ni]) continue;

            float ng = g_cost[ci] + 1.0f;
            if (ng < g_cost[ni]) {
                g_cost[ni] = ng;
                parent[ni] = ci;
                float nf = ng + heuristic(nx, ny, ex, ey);
                open.push({nf, ni});
            }
        }
    }

    return {};  // No path found
}

} // namespace eb
