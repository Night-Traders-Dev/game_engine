#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <utility>
#include "engine/core/types.h"

namespace eb {

// ─── Trigger Zones ───

struct TriggerZone {
    std::string id;
    Rect area;
    bool is_circle = false;
    Vec2 center;
    float radius = 0.0f;
    std::string on_enter;
    std::string on_exit;
    std::string on_stay;
    std::unordered_set<int> occupants;
    bool active = true;
    bool one_shot = false;
    bool fired = false;
};

struct TriggerEvent {
    std::string zone_id;
    std::string callback;
    int entity_id = -1;
    enum Type { Enter, Exit, Stay } type = Enter;
};

inline bool point_in_zone(const TriggerZone& zone, Vec2 pos) {
    if (zone.is_circle) {
        float dx = pos.x - zone.center.x;
        float dy = pos.y - zone.center.y;
        return (dx * dx + dy * dy) <= (zone.radius * zone.radius);
    }
    return zone.area.contains(pos);
}

inline std::vector<TriggerEvent> update_triggers(
    std::vector<TriggerZone>& zones,
    const std::vector<std::pair<int, Vec2>>& entities)
{
    std::vector<TriggerEvent> events;

    for (auto& zone : zones) {
        if (!zone.active) continue;
        if (zone.one_shot && zone.fired) continue;

        std::unordered_set<int> current_occupants;

        // Determine which entities are currently inside
        for (const auto& [entity_id, pos] : entities) {
            if (point_in_zone(zone, pos)) {
                current_occupants.insert(entity_id);
            }
        }

        // Check for enter events (in current but not in previous)
        for (int id : current_occupants) {
            if (zone.occupants.find(id) == zone.occupants.end()) {
                if (!zone.on_enter.empty()) {
                    events.push_back({zone.id, zone.on_enter, id, TriggerEvent::Enter});
                }
                if (zone.one_shot) {
                    zone.fired = true;
                }
            }
        }

        // Check for exit events (in previous but not in current)
        for (int id : zone.occupants) {
            if (current_occupants.find(id) == current_occupants.end()) {
                if (!zone.on_exit.empty()) {
                    events.push_back({zone.id, zone.on_exit, id, TriggerEvent::Exit});
                }
            }
        }

        // Check for stay events (in both)
        for (int id : current_occupants) {
            if (zone.occupants.find(id) != zone.occupants.end()) {
                if (!zone.on_stay.empty()) {
                    events.push_back({zone.id, zone.on_stay, id, TriggerEvent::Stay});
                }
            }
        }

        zone.occupants = std::move(current_occupants);
    }

    return events;
}

} // namespace eb
