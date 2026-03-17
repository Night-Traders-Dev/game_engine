#include "game/systems/moving_platform.h"
#include <cmath>

void update_moving_platforms(std::vector<MovingPlatform>& platforms, float dt) {
    for (auto& plat : platforms) {
        if (!plat.active || plat.path.size() < 2) {
            plat.velocity = {0, 0};
            continue;
        }

        eb::Vec2 target = plat.path[plat.current_waypoint];
        float dx = target.x - plat.position.x;
        float dy = target.y - plat.position.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < plat.speed * dt) {
            // Arrived at waypoint
            eb::Vec2 old_pos = plat.position;
            plat.position = target;

            if (plat.ping_pong) {
                if (plat.forward) {
                    plat.current_waypoint++;
                    if (plat.current_waypoint >= (int)plat.path.size()) {
                        plat.current_waypoint = (int)plat.path.size() - 2;
                        plat.forward = false;
                        if (plat.current_waypoint < 0) plat.current_waypoint = 0;
                    }
                } else {
                    plat.current_waypoint--;
                    if (plat.current_waypoint < 0) {
                        plat.current_waypoint = 1;
                        plat.forward = true;
                        if (plat.current_waypoint >= (int)plat.path.size())
                            plat.current_waypoint = 0;
                    }
                }
            } else {
                plat.current_waypoint = (plat.current_waypoint + 1) % (int)plat.path.size();
            }

            // Velocity for this frame
            if (dt > 0.0f) {
                plat.velocity.x = (plat.position.x - old_pos.x) / dt;
                plat.velocity.y = (plat.position.y - old_pos.y) / dt;
            }
        } else {
            float nx = dx / dist;
            float ny = dy / dist;
            eb::Vec2 old_pos = plat.position;
            plat.position.x += nx * plat.speed * dt;
            plat.position.y += ny * plat.speed * dt;
            if (dt > 0.0f) {
                plat.velocity.x = (plat.position.x - old_pos.x) / dt;
                plat.velocity.y = (plat.position.y - old_pos.y) / dt;
            }
        }
    }
}
