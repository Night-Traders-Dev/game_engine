#pragma once

#include "game/overworld/tile_map.h"
#include <vector>

struct PathNode;

namespace eb {

// A* pathfinding on the tile collision grid.
// Returns a path from (sx,sy) to (ex,ey) in tile coordinates.
// Empty vector = no path found. max_steps limits search depth.
std::vector<PathNode> find_path(const TileMap& map, int sx, int sy, int ex, int ey,
                                 int max_steps = 200);

} // namespace eb
