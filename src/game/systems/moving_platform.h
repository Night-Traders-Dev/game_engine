#pragma once
#include "engine/core/types.h"
#include <vector>

struct MovingPlatform {
    eb::Vec2 position;
    float width = 64.0f, height = 16.0f;
    int tile_id = 1;
    std::vector<eb::Vec2> path;
    int current_waypoint = 0;
    float speed = 60.0f;
    bool ping_pong = true;
    bool forward = true;
    bool active = true;
    eb::Vec2 velocity = {0, 0};
};

void update_moving_platforms(std::vector<MovingPlatform>& platforms, float dt);
