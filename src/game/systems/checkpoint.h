#pragma once

#include <string>
#include <vector>
#include "engine/core/types.h"

namespace eb {

// ─── Checkpoint System ───

struct Checkpoint {
    std::string id;
    Vec2 position;
    std::string map_id;
    bool activated = false;
};

struct CheckpointSystem {
    std::vector<Checkpoint> checkpoints;
    int active_idx = -1;

    void add(const std::string& id, Vec2 pos, const std::string& map) {
        checkpoints.push_back({id, pos, map, false});
    }

    void activate(const std::string& id) {
        for (int i = 0; i < static_cast<int>(checkpoints.size()); i++) {
            if (checkpoints[i].id == id) {
                checkpoints[i].activated = true;
                active_idx = i;
                return;
            }
        }
    }

    Checkpoint* get_active() {
        if (active_idx < 0 || active_idx >= static_cast<int>(checkpoints.size())) {
            return nullptr;
        }
        return &checkpoints[active_idx];
    }

    void clear() {
        checkpoints.clear();
        active_idx = -1;
    }
};

} // namespace eb
