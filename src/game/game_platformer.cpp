#include "game/game.h"
#include "engine/platform/input.h"
#include "engine/scripting/script_engine.h"
#include <cmath>
#include <algorithm>

// Forward declarations
void update_platformer(GameState& game, const eb::InputState& input, float dt);
void update_platformer_npcs(GameState& game, float dt);

// ─── Collision helpers ───

static eb::CollisionType get_collision_type(const eb::TileMap& map, float world_x, float world_y) {
    int tx = (int)(world_x / map.tile_size());
    int ty = (int)(world_y / map.tile_size());
    if (tx < 0 || tx >= map.width() || ty < 0 || ty >= map.height())
        return eb::CollisionType::Solid; // Out of bounds = solid
    return map.collision_at(tx, ty);
}

static bool is_solid_at(const eb::TileMap& map, float wx, float wy) {
    auto ct = get_collision_type(map, wx, wy);
    return ct == eb::CollisionType::Solid;
}

// Check if any corner of an AABB hits solid tiles
static bool aabb_hits_solid(const eb::TileMap& map, float x, float y, float w, float h) {
    // Check all four corners plus midpoints for thin walls
    return is_solid_at(map, x, y) || is_solid_at(map, x + w, y) ||
           is_solid_at(map, x, y + h) || is_solid_at(map, x + w, y + h) ||
           is_solid_at(map, x + w * 0.5f, y) || is_solid_at(map, x + w * 0.5f, y + h);
}

// Check for one-way platform collision (only solid when falling onto from above)
static bool check_one_way(const eb::TileMap& map, float x, float y, float w, float old_bottom) {
    int ts = map.tile_size();
    int left_tile = (int)(x / ts);
    int right_tile = (int)((x + w) / ts);
    int ty = (int)(y / ts);
    if (ty < 0 || ty >= map.height()) return false;

    for (int tx = left_tile; tx <= right_tile; tx++) {
        if (tx < 0 || tx >= map.width()) continue;
        if (map.collision_at(tx, ty) == eb::CollisionType::OneWayUp) {
            float tile_top = (float)(ty * ts);
            // Only collide if player was above the tile top before this frame
            if (old_bottom <= tile_top + 2.0f) {
                return true;
            }
        }
    }
    return false;
}

// Check slope surface Y at a given world X within a slope tile
static float slope_surface_y(const eb::TileMap& map, float wx, float wy) {
    int ts = map.tile_size();
    int tx = (int)(wx / ts);
    int ty = (int)(wy / ts);
    if (tx < 0 || tx >= map.width() || ty < 0 || ty >= map.height())
        return wy;

    auto ct = map.collision_at(tx, ty);
    float tile_x = (float)(tx * ts);
    float tile_y = (float)(ty * ts);
    float local_x = wx - tile_x;
    float frac = local_x / (float)ts;

    if (ct == eb::CollisionType::Slope45Up) {
        // Rising left to right: surface goes from bottom-left to top-right
        return tile_y + (float)ts * (1.0f - frac);
    } else if (ct == eb::CollisionType::Slope45Down) {
        // Falling left to right: surface goes from top-left to bottom-right
        return tile_y + (float)ts * frac;
    }
    return wy;
}

// Check if position is on a ladder tile
static bool is_on_ladder(const eb::TileMap& map, float wx, float wy) {
    return get_collision_type(map, wx, wy) == eb::CollisionType::Ladder;
}

// Check if position is on a hazard tile
static bool is_on_hazard(const eb::TileMap& map, float wx, float wy) {
    return get_collision_type(map, wx, wy) == eb::CollisionType::Hazard;
}

// Check moving platform collision (returns platform index or -1)
static int check_platform_landing(const std::vector<MovingPlatform>& platforms,
                                   float px, float py, float pw, float ph, float old_bottom) {
    for (int i = 0; i < (int)platforms.size(); i++) {
        const auto& plat = platforms[i];
        if (!plat.active) continue;
        // Player AABB bottom overlaps platform top
        float plat_left = plat.position.x - plat.width * 0.5f;
        float plat_right = plat.position.x + plat.width * 0.5f;
        float plat_top = plat.position.y;
        if (px + pw > plat_left && px < plat_right &&
            py + ph >= plat_top && py + ph <= plat_top + plat.height + 4.0f &&
            old_bottom <= plat_top + 2.0f) {
            return i;
        }
    }
    return -1;
}

// ─── Main platformer update ───

void update_platformer(GameState& game, const eb::InputState& input, float dt) {
    if (game.input_locked) return;

    auto& p = game.platformer;
    auto& map = game.tile_map;
    int ts = map.tile_size();

    // ── Timers ──
    if (p.jump_buffer_timer > 0) p.jump_buffer_timer -= dt;
    if (p.coyote_timer > 0) p.coyote_timer -= dt;
    if (p.dash_cooldown > 0) p.dash_cooldown -= dt;

    bool was_on_ground = p.on_ground;

    // ── Input handling ──
    float move_x = 0.0f;
    if (input.is_held(eb::InputAction::MoveLeft))  move_x -= 1.0f;
    if (input.is_held(eb::InputAction::MoveRight)) move_x += 1.0f;

    float speed = p.move_speed;
    if (input.is_held(eb::InputAction::Run)) speed = p.run_speed;

    p.velocity.x = move_x * speed;

    // Jump buffer
    if (input.is_pressed(eb::InputAction::Jump)) {
        p.jump_buffer_timer = p.jump_buffer_time;
    }
    p.jump_held = input.is_held(eb::InputAction::Jump);

    // Crouch
    if (p.on_ground && input.is_held(eb::InputAction::MoveDown) && move_x == 0.0f) {
        p.state = eb::PlayerPlatformState::Crouch;
        p.velocity.x = 0.0f;
    }

    // Dash
    if (p.dash_enabled && p.can_dash && p.dash_cooldown <= 0.0f &&
        input.is_pressed(eb::InputAction::Run) && !p.on_ground) {
        p.dash_timer = p.dash_duration;
        p.dash_dir = p.facing_dir;
        p.can_dash = false;
        p.dash_cooldown = 0.3f;
    }

    // ── Coyote time ──
    if (was_on_ground && !p.on_ground && p.velocity.y >= 0.0f) {
        p.coyote_timer = p.coyote_time;
    }

    // ── Jump logic ──
    if (p.jump_buffer_timer > 0) {
        // Ground jump or coyote jump
        if (p.on_ground || p.coyote_timer > 0) {
            p.velocity.y = p.jump_force;
            p.jump_buffer_timer = 0;
            p.coyote_timer = 0;
            p.used_double_jump = false;
        }
        // Wall jump
        else if (p.on_wall && p.wall_slide_enabled) {
            p.velocity.x = p.wall_jump_force_x * (-p.wall_dir);
            p.velocity.y = p.wall_jump_force_y;
            p.jump_buffer_timer = 0;
        }
        // Double jump
        else if (p.can_double_jump && !p.used_double_jump) {
            p.velocity.y = p.jump_force;
            p.jump_buffer_timer = 0;
            p.used_double_jump = true;
        }
    }

    // ── Ladder ──
    float hitbox_left = game.player_pos.x - p.hitbox_w * 0.5f;
    float hitbox_top = game.player_pos.y - p.hitbox_h;
    float center_x = game.player_pos.x;
    float center_y = game.player_pos.y - p.hitbox_h * 0.5f;

    p.on_ladder = is_on_ladder(map, center_x, center_y);

    if (p.on_ladder) {
        // On ladder: disable gravity, allow vertical movement
        p.velocity.y = 0;
        if (input.is_held(eb::InputAction::MoveUp)) p.velocity.y = -speed;
        if (input.is_held(eb::InputAction::MoveDown)) p.velocity.y = speed;
    }

    // ── Dash override ──
    if (p.dash_timer > 0) {
        p.velocity.x = p.dash_speed * (float)p.dash_dir;
        p.velocity.y = 0;
        p.dash_timer -= dt;
    }

    // ── Gravity ──
    if (!p.on_ladder && p.dash_timer <= 0) {
        float grav = p.gravity;
        // Variable jump height: lower gravity while holding jump and going up
        if (p.jump_held && p.velocity.y < 0) {
            grav = p.jump_hold_gravity;
        }
        p.velocity.y += grav * dt;
        if (p.velocity.y > p.max_fall_speed) p.velocity.y = p.max_fall_speed;
    }

    // ── Wall slide ──
    if (p.on_wall && !p.on_ground && p.velocity.y > 0 && p.wall_slide_enabled) {
        if (p.velocity.y > p.wall_slide_speed)
            p.velocity.y = p.wall_slide_speed;
    }

    // ── Move and collide: X axis ──
    float old_bottom = game.player_pos.y;
    {
        float new_x = game.player_pos.x + p.velocity.x * dt;
        float new_left = new_x - p.hitbox_w * 0.5f;
        float new_top = game.player_pos.y - p.hitbox_h;

        if (aabb_hits_solid(map, new_left, new_top, p.hitbox_w, p.hitbox_h)) {
            // Push back to tile edge
            if (p.velocity.x > 0) {
                int tile_x = (int)((new_left + p.hitbox_w) / ts);
                new_x = (float)(tile_x * ts) - p.hitbox_w * 0.5f - 0.01f;
            } else if (p.velocity.x < 0) {
                int tile_x = (int)(new_left / ts);
                new_x = (float)((tile_x + 1) * ts) + p.hitbox_w * 0.5f + 0.01f;
            }
            p.velocity.x = 0;
        }
        game.player_pos.x = new_x;
    }

    // ── Move and collide: Y axis ──
    p.on_ground = false;
    {
        float new_y = game.player_pos.y + p.velocity.y * dt;
        float new_left = game.player_pos.x - p.hitbox_w * 0.5f;
        float new_top = new_y - p.hitbox_h;

        bool collided_y = false;

        // Check solid tiles
        if (aabb_hits_solid(map, new_left, new_top, p.hitbox_w, p.hitbox_h)) {
            collided_y = true;
            if (p.velocity.y > 0) {
                // Landing
                int tile_y = (int)(new_y / ts);
                new_y = (float)(tile_y * ts) - 0.01f;
                p.on_ground = true;
                p.used_double_jump = false;
                p.can_dash = true;
            } else if (p.velocity.y < 0) {
                // Hit ceiling
                int tile_y = (int)(new_top / ts);
                new_y = (float)((tile_y + 1) * ts) + p.hitbox_h + 0.01f;
            }
            p.velocity.y = 0;
        }

        // Check one-way platforms (only when falling)
        if (!collided_y && p.velocity.y >= 0) {
            if (check_one_way(map, new_left, new_y, p.hitbox_w, old_bottom)) {
                int ty = (int)(new_y / ts);
                new_y = (float)(ty * ts) - 0.01f;
                p.velocity.y = 0;
                p.on_ground = true;
                p.used_double_jump = false;
                p.can_dash = true;
                collided_y = true;
            }
        }

        // Check slopes
        if (!collided_y) {
            float foot_x = game.player_pos.x;
            auto ct = get_collision_type(map, foot_x, new_y);
            if (ct == eb::CollisionType::Slope45Up || ct == eb::CollisionType::Slope45Down) {
                float surface_y = slope_surface_y(map, foot_x, new_y);
                if (new_y >= surface_y) {
                    new_y = surface_y - 0.01f;
                    p.velocity.y = 0;
                    p.on_ground = true;
                    p.used_double_jump = false;
                    p.can_dash = true;
                }
            }
        }

        // Check moving platforms (only when falling)
        if (!p.on_ground && p.velocity.y >= 0) {
            int pi = check_platform_landing(game.moving_platforms,
                game.player_pos.x - p.hitbox_w * 0.5f, new_y - p.hitbox_h,
                p.hitbox_w, p.hitbox_h, old_bottom);
            if (pi >= 0) {
                new_y = game.moving_platforms[pi].position.y - 0.01f;
                p.velocity.y = 0;
                p.on_ground = true;
                p.used_double_jump = false;
                p.can_dash = true;
                // Carry player with platform
                game.player_pos.x += game.moving_platforms[pi].velocity.x * dt;
                game.player_pos.y += game.moving_platforms[pi].velocity.y * dt;
            }
        }

        game.player_pos.y = new_y;
    }

    // ── Wall detection ──
    p.on_wall = false;
    p.wall_dir = 0;
    if (!p.on_ground) {
        float check_dist = 2.0f;
        float mid_y = game.player_pos.y - p.hitbox_h * 0.5f;
        // Check right
        if (is_solid_at(map, game.player_pos.x + p.hitbox_w * 0.5f + check_dist, mid_y)) {
            p.on_wall = true;
            p.wall_dir = 1;
        }
        // Check left
        else if (is_solid_at(map, game.player_pos.x - p.hitbox_w * 0.5f - check_dist, mid_y)) {
            p.on_wall = true;
            p.wall_dir = -1;
        }
    }

    // ── Hazard check ──
    if (is_on_hazard(map, game.player_pos.x, game.player_pos.y - 1.0f) ||
        is_on_hazard(map, game.player_pos.x, game.player_pos.y - p.hitbox_h * 0.5f)) {
        game.player_hp -= 1;
        // Knockback upward
        p.velocity.y = p.jump_force * 0.5f;
    }

    // ── Update facing direction ──
    if (p.velocity.x > 0.1f) p.facing_dir = 1;
    else if (p.velocity.x < -0.1f) p.facing_dir = -1;

    // ── Update state machine ──
    p.prev_state = p.state;
    if (p.dash_timer > 0) {
        p.state = eb::PlayerPlatformState::Dash;
    } else if (p.on_ladder) {
        p.state = eb::PlayerPlatformState::Climb;
    } else if (p.on_wall && !p.on_ground && p.wall_slide_enabled) {
        p.state = eb::PlayerPlatformState::WallSlide;
    } else if (p.on_ground) {
        if (input.is_held(eb::InputAction::MoveDown) && move_x == 0.0f)
            p.state = eb::PlayerPlatformState::Crouch;
        else if (std::abs(p.velocity.x) > 0.1f)
            p.state = eb::PlayerPlatformState::Run;
        else
            p.state = eb::PlayerPlatformState::Idle;
    } else {
        if (p.velocity.y < 0)
            p.state = eb::PlayerPlatformState::Jump;
        else
            p.state = eb::PlayerPlatformState::Fall;
    }

    // ── Update player_dir for sprite rendering ──
    // In platformer: 2=left, 3=right
    game.player_dir = (p.facing_dir < 0) ? 2 : 3;
    game.player_moving = (std::abs(p.velocity.x) > 0.1f || std::abs(p.velocity.y) > 0.1f);

    // ── Animation timer ──
    game.anim_timer += dt;
    if (game.anim_timer >= 0.15f) {
        game.anim_timer -= 0.15f;
        game.player_frame = 1 - game.player_frame;
    }

    // ── Update moving platforms ──
    update_moving_platforms(game.moving_platforms, dt);

    // ── Update platformer NPCs ──
    update_platformer_npcs(game, dt);

    // ── Camera ──
    game.camera.follow_platformer(game.player_pos, p.facing_dir, p.on_ground, 5.0f);
    game.camera.update(dt);
}

// ─── Platformer NPC update ───

void update_platformer_npcs(GameState& game, float dt) {
    auto& map = game.tile_map;
    int ts = map.tile_size();

    for (auto& npc : game.npcs) {
        if (npc.platform_ai == NPC::PlatformAI::None) continue;
        if (!npc.schedule.currently_visible) continue;

        auto& p = game.platformer;

        if (npc.platform_ai == NPC::PlatformAI::Fly) {
            // Fly: follow route waypoints, ignore gravity
            if (npc.route.active && !npc.route.waypoints.empty()) {
                auto& wp = npc.route.waypoints[npc.route.current_waypoint];
                float dx = wp.x - npc.position.x;
                float dy = wp.y - npc.position.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < npc.move_speed * dt) {
                    npc.position = wp;
                    npc.route.current_waypoint = (npc.route.current_waypoint + 1) %
                        (int)npc.route.waypoints.size();
                } else {
                    npc.position.x += (dx / dist) * npc.move_speed * dt;
                    npc.position.y += (dy / dist) * npc.move_speed * dt;
                }
            }
        } else {
            // Patrol or Jump: apply gravity
            npc.npc_velocity.y += npc.npc_gravity * dt;
            if (npc.npc_velocity.y > 600.0f) npc.npc_velocity.y = 600.0f;

            // Patrol movement
            npc.npc_velocity.x = npc.move_speed * (float)npc.patrol_dir;

            // Reverse at patrol bounds
            if (npc.patrol_max_x > npc.patrol_min_x) {
                if (npc.position.x >= npc.patrol_max_x && npc.patrol_dir > 0)
                    npc.patrol_dir = -1;
                if (npc.position.x <= npc.patrol_min_x && npc.patrol_dir < 0)
                    npc.patrol_dir = 1;
            }

            // Edge detection: check tile below next step
            if (npc.patrol_edge_detect && npc.npc_on_ground) {
                float check_x = npc.position.x + (float)npc.patrol_dir * 16.0f;
                float check_y = npc.position.y + 4.0f;
                if (!is_solid_at(map, check_x, check_y)) {
                    // No ground ahead — reverse
                    npc.patrol_dir = -npc.patrol_dir;
                    npc.npc_velocity.x = npc.move_speed * (float)npc.patrol_dir;
                }
            }

            // Jump AI
            if (npc.platform_ai == NPC::PlatformAI::Jump) {
                npc.npc_jump_timer += dt;
                if (npc.npc_jump_timer >= npc.npc_jump_interval && npc.npc_on_ground) {
                    npc.npc_velocity.y = npc.npc_jump_force;
                    npc.npc_jump_timer = 0.0f;
                }
            }

            // Move X
            float new_x = npc.position.x + npc.npc_velocity.x * dt;
            float npc_hw = 8.0f, npc_hh = 16.0f;
            if (is_solid_at(map, new_x + npc_hw * (float)npc.patrol_dir, npc.position.y - npc_hh * 0.5f)) {
                npc.patrol_dir = -npc.patrol_dir;
                npc.npc_velocity.x = 0;
            } else {
                npc.position.x = new_x;
            }

            // Move Y
            npc.npc_on_ground = false;
            float new_y = npc.position.y + npc.npc_velocity.y * dt;
            if (npc.npc_velocity.y > 0 && is_solid_at(map, npc.position.x, new_y)) {
                int tile_y = (int)(new_y / ts);
                new_y = (float)(tile_y * ts) - 0.01f;
                npc.npc_velocity.y = 0;
                npc.npc_on_ground = true;
            }
            npc.position.y = new_y;
        }

        // ── Stomp detection ──
        if (npc.stompable && p.velocity.y > 0) {
            float npc_hw = 12.0f, npc_hh = 16.0f;
            float npc_left = npc.position.x - npc_hw;
            float npc_right = npc.position.x + npc_hw;
            float npc_top = npc.position.y - npc_hh;
            float player_left = game.player_pos.x - p.hitbox_w * 0.5f;
            float player_right = game.player_pos.x + p.hitbox_w * 0.5f;
            float player_bottom = game.player_pos.y;

            // Player bottom overlaps NPC top half
            if (player_right > npc_left && player_left < npc_right &&
                player_bottom >= npc_top && player_bottom <= npc_top + npc_hh * 0.5f) {
                // Stomp!
                p.velocity.y = p.jump_force * 0.6f;
                if (!npc.on_stomp_func.empty() && game.script_engine) {
                    game.script_engine->call_function(npc.on_stomp_func);
                }
                // Default behavior: remove NPC
                if (npc.on_stomp_func.empty()) {
                    npc.schedule.currently_visible = false;
                }
                continue;
            }
        }

        // ── Contact damage ──
        if (npc.contact_damage > 0) {
            float npc_hw = 12.0f, npc_hh = 16.0f;
            float npc_left = npc.position.x - npc_hw;
            float npc_right = npc.position.x + npc_hw;
            float npc_top = npc.position.y - npc_hh;
            float npc_bottom = npc.position.y;
            float player_left = game.player_pos.x - p.hitbox_w * 0.5f;
            float player_right = game.player_pos.x + p.hitbox_w * 0.5f;
            float player_top = game.player_pos.y - p.hitbox_h;
            float player_bottom = game.player_pos.y;

            if (player_right > npc_left && player_left < npc_right &&
                player_bottom > npc_top && player_top < npc_bottom) {
                // Contact from side/below
                game.player_hp -= npc.contact_damage;
                // Knockback
                float kb_dir = (game.player_pos.x < npc.position.x) ? -1.0f : 1.0f;
                p.velocity.x = 200.0f * kb_dir;
                p.velocity.y = p.jump_force * 0.4f;
                if (!npc.on_contact_func.empty() && game.script_engine) {
                    game.script_engine->call_function(npc.on_contact_func);
                }
            }
        }

        // Update NPC direction for sprite
        if (npc.npc_velocity.x < -0.1f) npc.dir = 2;
        else if (npc.npc_velocity.x > 0.1f) npc.dir = 3;
        npc.moving = (std::abs(npc.npc_velocity.x) > 0.1f);
    }
}
