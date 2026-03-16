#include "game/game.h"
#include "engine/resource/file_io.h"
#include "engine/scripting/script_engine.h"
#include "game/ai/pathfinding.h"
#include "game/systems/day_night.h"
#include "game/systems/survival.h"
#include "game/systems/spawn_system.h"
#include "game/systems/level_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#if !defined(__ANDROID__) && !defined(EB_ANDROID)
#include <filesystem>
#endif

// ─── Core game update ───

void update_game(GameState& game, const eb::InputState& input, float dt) {
    game.game_time += dt;
    game.current_input = &input;

    // ── Screen effects decay ──
    if (game.shake_timer > 0) game.shake_timer -= dt;
    if (game.flash_timer > 0) { game.flash_timer -= dt; if (game.flash_timer <= 0) game.flash_a = 0; }
    if (game.fade_timer > 0) {
        game.fade_timer -= dt;
        float t = 1.0f - (game.fade_timer / game.fade_duration);
        game.fade_a = game.fade_a + (game.fade_target - game.fade_a) * std::min(1.0f, t);
    }

    // ── Tween engine ──
    // Apply tween values to targets
    for (auto& tw : game.tween_system.tweens) {
        if (!tw.active) continue;
        if (!tw.started) {
            // Capture start value on first frame
            if (tw.target == "camera") {
                if (tw.property == "x") tw.start_val = game.camera.position().x;
                else if (tw.property == "y") tw.start_val = game.camera.position().y;
            } else if (tw.target == "player") {
                if (tw.property == "x") tw.start_val = game.player_pos.x;
                else if (tw.property == "y") tw.start_val = game.player_pos.y;
                else if (tw.property == "scale") tw.start_val = game.player_sprite_scale;
            } else {
                // UI element
                for (auto& l : game.script_ui.labels) {
                    if (l.id == tw.target) {
                        if (tw.property == "x") tw.start_val = l.position.x;
                        else if (tw.property == "y") tw.start_val = l.position.y;
                        else if (tw.property == "scale") tw.start_val = l.scale;
                        else if (tw.property == "alpha") tw.start_val = l.color.w;
                    }
                }
                for (auto& p : game.script_ui.panels) {
                    if (p.id == tw.target) {
                        if (tw.property == "x") tw.start_val = p.position.x;
                        else if (tw.property == "y") tw.start_val = p.position.y;
                        else if (tw.property == "alpha") tw.start_val = p.opacity;
                        else if (tw.property == "scale") tw.start_val = p.scale;
                    }
                }
                for (auto& img : game.script_ui.images) {
                    if (img.id == tw.target) {
                        if (tw.property == "x") tw.start_val = img.position.x;
                        else if (tw.property == "y") tw.start_val = img.position.y;
                        else if (tw.property == "alpha") tw.start_val = img.opacity;
                        else if (tw.property == "scale") tw.start_val = img.scale;
                    }
                }
            }
            tw.started = true;
        }

        float v = tw.value();
        // Apply to target
        if (tw.target == "camera") {
            auto pos = game.camera.position();
            if (tw.property == "x") game.camera.set_position({v, pos.y});
            else if (tw.property == "y") game.camera.set_position({pos.x, v});
        } else if (tw.target == "player") {
            if (tw.property == "x") game.player_pos.x = v;
            else if (tw.property == "y") game.player_pos.y = v;
            else if (tw.property == "scale") game.player_sprite_scale = v;
        } else {
            // UI elements
            for (auto& l : game.script_ui.labels) {
                if (l.id == tw.target) {
                    if (tw.property == "x") l.position.x = v;
                    else if (tw.property == "y") l.position.y = v;
                    else if (tw.property == "scale") l.scale = v;
                    else if (tw.property == "alpha") l.color.w = v;
                }
            }
            for (auto& p : game.script_ui.panels) {
                if (p.id == tw.target) {
                    if (tw.property == "x") p.position.x = v;
                    else if (tw.property == "y") p.position.y = v;
                    else if (tw.property == "alpha") p.opacity = v;
                    else if (tw.property == "scale") p.scale = v;
                }
            }
            for (auto& img : game.script_ui.images) {
                if (img.id == tw.target) {
                    if (tw.property == "x") img.position.x = v;
                    else if (tw.property == "y") img.position.y = v;
                    else if (tw.property == "alpha") img.opacity = v;
                    else if (tw.property == "scale") img.scale = v;
                }
            }
        }
    }
    auto tween_callbacks = game.tween_system.update(dt);

    // ── Particle system ──
    game.debug_particle_count = 0;
    for (auto& emitter : game.emitters) {
        emitter.update(dt, game.rng);
        for (auto& p : emitter.particles) if (p.alive) game.debug_particle_count++;
    }
    game.emitters.erase(std::remove_if(game.emitters.begin(), game.emitters.end(),
        [](const eb::ParticleEmitter& e) { return e.all_dead(); }), game.emitters.end());

    // ── Screen transition ──
    {
        auto cb = game.transition.update(dt);
        if (!cb.empty()) tween_callbacks.push_back(cb);
    }

    // ── Playtime tracking ──
    game.playtime_seconds += dt;

    // ── Debug stats ──
    static float fps_timer = 0;
    static int fps_frames = 0;
    fps_timer += dt;
    fps_frames++;
    if (fps_timer >= 0.5f) {
        game.debug_fps = fps_frames / fps_timer;
        fps_timer = 0;
        fps_frames = 0;
    }
    // particle count already cached during update above

    // ── World systems (always tick, even during menus) ──
    eb::update_day_night(game.day_night, dt);

    // Survival stats
    if (game.survival.enabled) {
        float minutes_elapsed = dt * game.day_night.day_speed / 60.0f;
        eb::update_survival(game.survival, minutes_elapsed,
                            game.player_speed, game.player_hp, 120.0f);
    }

    // Spawn loops
    eb::update_spawn_loops(game, dt);

    // Script UI notification timers
    for (auto& n : game.script_ui.notifications) n.timer += dt;
    game.script_ui.notifications.erase(
        std::remove_if(game.script_ui.notifications.begin(), game.script_ui.notifications.end(),
                        [](auto& n) { return n.timer >= n.duration; }),
        game.script_ui.notifications.end());

    // ── Weather system update ──
    {
        auto& w = game.weather;
        float sw = game.hud.screen_w, sh = game.hud.screen_h;

        // Wind affects rain angle and snow drift
        float wind_rad = w.wind_direction * 3.14159f / 180.0f;
        float wind_x = std::sin(wind_rad) * w.wind_strength;

        // Rain update
        if (w.rain_active) {
            int target = (int)(w.rain_max_drops * w.rain_intensity);
            while ((int)w.rain_drops.size() < target) {
                RainDrop d;
                d.x = (float)(game.rng() % (int)(sw + 200)) - 100;
                d.y = -(float)(game.rng() % (int)sh);
                d.speed = w.rain_speed * (0.8f + (game.rng() % 40) / 100.0f);
                d.length = 8.0f + (game.rng() % 12);
                d.opacity = w.rain_opacity * (0.6f + (game.rng() % 40) / 100.0f);
                w.rain_drops.push_back(d);
            }
            float angle_rad = (w.rain_angle + wind_x * 30.0f) * 3.14159f / 180.0f;
            for (auto& d : w.rain_drops) {
                d.y += d.speed * dt;
                d.x += std::sin(angle_rad) * d.speed * dt;
                if (d.y > sh + 20) {
                    d.y = -(float)(game.rng() % 60);
                    d.x = (float)(game.rng() % (int)(sw + 200)) - 100;
                }
            }
        } else {
            w.rain_drops.clear();
        }

        // Snow update
        if (w.snow_active) {
            int target = (int)(w.snow_max_flakes * w.snow_intensity);
            while ((int)w.snow_flakes.size() < target) {
                SnowFlake f;
                f.x = (float)(game.rng() % (int)sw);
                f.y = -(float)(game.rng() % (int)sh);
                f.speed = w.snow_speed * (0.5f + (game.rng() % 50) / 100.0f);
                f.drift = w.snow_drift * (0.5f + (game.rng() % 50) / 100.0f);
                f.size = 1.5f + (game.rng() % 20) / 10.0f;
                f.phase = (game.rng() % 628) / 100.0f;
                f.opacity = (0.5f + (game.rng() % 50) / 100.0f);
                w.snow_flakes.push_back(f);
            }
            for (auto& f : w.snow_flakes) {
                f.y += f.speed * dt;
                f.x += std::sin(f.phase + game.game_time * 1.5f) * f.drift * dt + wind_x * 40.0f * dt;
                if (f.y > sh + 10) {
                    f.y = -(float)(game.rng() % 30);
                    f.x = (float)(game.rng() % (int)sw);
                }
                if (f.x < -10) f.x = sw + 5;
                if (f.x > sw + 10) f.x = -5;
            }
        } else {
            w.snow_flakes.clear();
        }

        // Lightning update
        if (w.lightning_active) {
            w.lightning_timer += dt;
            if (w.lightning_timer >= w.lightning_interval) {
                w.lightning_timer = 0;
                if ((game.rng() % 100) < (int)(w.lightning_chance * 100)) {
                    // Create a lightning bolt
                    LightningBolt bolt;
                    bolt.x1 = (float)(game.rng() % (int)sw);
                    bolt.y1 = 0;
                    bolt.x2 = bolt.x1 + (game.rng() % 200) - 100;
                    bolt.y2 = sh * (0.4f + (game.rng() % 40) / 100.0f);
                    bolt.timer = w.lightning_flash_duration;
                    bolt.brightness = 0.8f + (game.rng() % 20) / 100.0f;
                    // Generate fork segments
                    float cx = bolt.x1, cy = bolt.y1;
                    int segs = 8 + game.rng() % 6;
                    float dx = (bolt.x2 - bolt.x1) / segs;
                    float dy = (bolt.y2 - bolt.y1) / segs;
                    for (int s = 0; s < segs; s++) {
                        float jx = (game.rng() % 40) - 20.0f;
                        float nx = cx + dx + jx;
                        float ny = cy + dy;
                        bolt.segments.push_back({nx, ny});
                        cx = nx; cy = ny;
                    }
                    w.bolts.push_back(bolt);
                    // Screen flash
                    game.flash_r = 0.9f; game.flash_g = 0.9f; game.flash_b = 1.0f;
                    game.flash_a = bolt.brightness * 0.6f;
                    game.flash_timer = w.lightning_flash_duration;
                }
            }
            // Decay bolts
            for (auto& b : w.bolts) b.timer -= dt;
            w.bolts.erase(std::remove_if(w.bolts.begin(), w.bolts.end(),
                [](auto& b) { return b.timer <= 0; }), w.bolts.end());
        } else {
            w.bolts.clear();
        }

        // Cloud shadows update
        if (w.clouds_active) {
            while ((int)w.cloud_shadows.size() < w.cloud_max) {
                CloudShadow c;
                c.x = (float)(game.rng() % (int)(sw * 2)) - sw * 0.5f;
                c.y = (float)(game.rng() % (int)(sh * 2)) - sh * 0.5f;
                c.radius = 60.0f + (game.rng() % 120);
                c.opacity = w.cloud_shadow_opacity * (0.5f + (game.rng() % 50) / 100.0f);
                float dir_rad = w.cloud_direction * 3.14159f / 180.0f;
                c.speed_x = std::cos(dir_rad) * w.cloud_speed * (0.7f + (game.rng() % 30) / 100.0f);
                c.speed_y = std::sin(dir_rad) * w.cloud_speed * (0.7f + (game.rng() % 30) / 100.0f);
                w.cloud_shadows.push_back(c);
            }
            for (auto& c : w.cloud_shadows) {
                c.x += (c.speed_x + wind_x * 15.0f) * dt;
                c.y += c.speed_y * dt;
                // Wrap around
                if (c.x > sw * 1.5f) c.x = -c.radius * 2;
                if (c.x < -c.radius * 3) c.x = sw * 1.2f;
                if (c.y > sh * 1.5f) c.y = -c.radius * 2;
                if (c.y < -c.radius * 3) c.y = sh * 1.2f;
            }
        } else {
            w.cloud_shadows.clear();
        }

        // God ray sway animation
        if (w.god_rays_active) {
            w.god_ray_sway = std::sin(game.game_time * 0.3f) * 15.0f;
        }
    }

    // Pause menu toggle (ESC / Menu)
    if (input.is_pressed(eb::InputAction::Menu)) {
        game.paused = !game.paused;
        game.pause_selection = 0;
    }

    // Pause menu input (keyboard + mouse)
    static constexpr int PAUSE_ITEM_COUNT = 6;
    // 0=Resume, 1=Editor, 2=Levels, 3=Reset, 4=Settings, 5=Quit
    if (game.paused) {
        // Level selector sub-menu
        if (game.level_select_open) {
            if (input.is_pressed(eb::InputAction::MoveUp) && game.level_select_cursor > 0)
                game.level_select_cursor--;
            if (input.is_pressed(eb::InputAction::MoveDown) && game.level_select_cursor < (int)game.level_select_ids.size() - 1)
                game.level_select_cursor++;
            if (input.is_pressed(eb::InputAction::Confirm) && !game.level_select_ids.empty()) {
                auto& id = game.level_select_ids[game.level_select_cursor];
                if (game.level_manager && game.level_manager->is_loaded(id)) {
                    game.level_manager->switch_level(id, game);
                    game.camera.center_on(game.player_pos);
                }
                game.level_select_open = false;
                game.paused = false;
            }
            if (input.is_pressed(eb::InputAction::Cancel)) {
                game.level_select_open = false;
            }
            return;
        }

        if (input.is_pressed(eb::InputAction::MoveUp) && game.pause_selection > 0)
            game.pause_selection--;
        if (input.is_pressed(eb::InputAction::MoveDown) && game.pause_selection < PAUSE_ITEM_COUNT - 1)
            game.pause_selection++;

        auto execute_pause_action = [&](int i) {
            switch (i) {
                case 0: game.paused = false; break;
                case 1: game.paused = false; game.pause_request_editor = true; break;
                case 2: { // Levels
                    game.level_select_ids.clear();
                    if (game.level_manager) {
                        for (auto& [lid, lvl] : game.level_manager->levels)
                            if (lvl.loaded) game.level_select_ids.push_back(lid);
                    }
                    game.level_select_cursor = 0;
                    game.level_select_open = true;
                    break;
                }
                case 3: game.pause_request_reset = true; game.paused = false; break;
                case 4: break; // Settings placeholder
                case 5: game.pause_request_quit = true; break;
            }
        };

        // Mouse hover and click — use script UI label positions
        // Convert native touch/mouse coords to UI virtual coords
        float mx = input.mouse.x * (game.hud.screen_w / game.hud.native_w);
        float my = input.mouse.y * (game.hud.screen_h / game.hud.native_h);
        static const char* pause_ids[PAUSE_ITEM_COUNT] = {"pause_item_0","pause_item_1","pause_item_2","pause_item_3","pause_item_4","pause_item_5"};
        for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
            for (auto& l : game.script_ui.labels) {
                if (l.id != pause_ids[i]) continue;
                float hit_w = 260, hit_h = 36;
                float lx = l.position.x - 20, ly = l.position.y - 4;
                if (mx >= lx && mx <= lx + hit_w && my >= ly && my <= ly + hit_h) {
                    game.pause_selection = i;
                    if (input.mouse.is_pressed(eb::MouseButton::Left))
                        execute_pause_action(i);
                }
                break;
            }
        }

        // Keyboard confirm
        if (input.is_pressed(eb::InputAction::Confirm))
            execute_pause_action(game.pause_selection);
        if (input.is_pressed(eb::InputAction::Cancel)) {
            game.paused = false;
        }
        return;
    }

    // Battle mode
    if (game.battle.phase != BattlePhase::None) {
        update_battle(game, dt,
                      input.is_pressed(eb::InputAction::Confirm),
                      input.is_pressed(eb::InputAction::MoveUp),
                      input.is_pressed(eb::InputAction::MoveDown));
        return;
    }

    // Merchant UI mode
    if (game.merchant_ui.is_open()) {
        game.merchant_ui.update(game,
            input.is_pressed(eb::InputAction::MoveUp),
            input.is_pressed(eb::InputAction::MoveDown),
            input.is_pressed(eb::InputAction::MoveLeft),
            input.is_pressed(eb::InputAction::MoveRight),
            input.is_pressed(eb::InputAction::Confirm),
            input.is_pressed(eb::InputAction::Cancel),
            dt);
        return;
    }

    // Dialogue mode
    if (game.dialogue.is_active()) {
        int result = game.dialogue.update(dt,
            input.is_pressed(eb::InputAction::Confirm),
            input.is_pressed(eb::InputAction::MoveUp),
            input.is_pressed(eb::InputAction::MoveDown));
        if (result >= 0 && game.pending_battle_npc >= 0 &&
            game.pending_battle_npc < (int)game.npcs.size()) {
            auto& npc = game.npcs[game.pending_battle_npc];
            game.battle.enemy_sprite_key = npc.sprite_atlas_key;
            start_battle(game, npc.battle_enemy_name,
                         npc.battle_enemy_hp, npc.battle_enemy_atk, false,
                         npc.sprite_atlas_id);
            game.pending_battle_npc = -1;
        }
        return;
    }

    // ── Inventory quick-use (Cancel/X toggles, Left/Right browses, Confirm uses) ──
    if (game.hud.inv_use_cooldown > 0) game.hud.inv_use_cooldown -= dt;
    if (input.is_pressed(eb::InputAction::Cancel)) {
        game.hud.inv_open = !game.hud.inv_open;
        if (game.hud.inv_open) game.hud.inv_selected = 0;
    }
    if (game.hud.inv_open && !game.inventory.items.empty()) {
        int max_idx = std::min(game.hud.inv_max_slots, (int)game.inventory.items.size()) - 1;
        if (input.is_pressed(eb::InputAction::MoveRight) && game.hud.inv_selected < max_idx)
            game.hud.inv_selected++;
        if (input.is_pressed(eb::InputAction::MoveLeft) && game.hud.inv_selected > 0)
            game.hud.inv_selected--;
        // Clamp if items were removed
        if (game.hud.inv_selected > max_idx) game.hud.inv_selected = max_idx;

        // Use item on Confirm
        if (input.is_pressed(eb::InputAction::Confirm) && game.hud.inv_use_cooldown <= 0) {
            int idx = game.hud.inv_selected;
            if (idx >= 0 && idx < (int)game.inventory.items.size()) {
                auto& item = game.inventory.items[idx];
                bool used = false;
                // Direct healing (no sage_func needed)
                if (item.heal_hp > 0 && game.player_hp < game.player_hp_max) {
                    game.player_hp = std::min(game.player_hp + item.heal_hp, game.player_hp_max);
                    used = true;
                }
                // Call sage_func if defined (for custom item effects)
                if (!item.sage_func.empty() && game.script_engine) {
                    if (game.script_engine->has_function(item.sage_func)) {
                        game.script_engine->call_function(item.sage_func);
                        used = true;
                    }
                }
                if (used) {
                    std::string msg = "Used " + item.name;
                    game.script_ui.notifications.push_back({msg, 2.0f, 0.0f});
                    game.inventory.remove(item.id, 1);
                    if (game.hud.inv_selected >= (int)game.inventory.items.size())
                        game.hud.inv_selected = std::max(0, (int)game.inventory.items.size() - 1);
                    game.hud.inv_use_cooldown = 0.3f;
                }
                if (game.inventory.items.empty()) game.hud.inv_open = false;
            }
        }
        // While inventory is open, don't move the player
        return;
    }

    // Player movement (blocked during cutscenes via input_locked)
    if (game.input_locked) return;
    eb::Vec2 move = {0.0f, 0.0f};
    if (input.is_held(eb::InputAction::MoveUp))    move.y -= 1.0f;
    if (input.is_held(eb::InputAction::MoveDown))  move.y += 1.0f;
    if (input.is_held(eb::InputAction::MoveLeft))  move.x -= 1.0f;
    if (input.is_held(eb::InputAction::MoveRight)) move.x += 1.0f;

    game.player_moving = (move.x != 0.0f || move.y != 0.0f);

    if (game.player_moving) {
        float len = std::sqrt(move.x * move.x + move.y * move.y);
        if (len > 0.0f) { move.x /= len; move.y /= len; }

        float speed = game.player_speed;
        if (input.is_held(eb::InputAction::Run)) speed *= 1.8f;

        float pw = 20.0f, ph = 12.0f;
        float ox = -pw * 0.5f, oy = -ph;

        float new_x = game.player_pos.x + move.x * speed * dt;
        bool bx = game.tile_map.is_solid_world(new_x + ox, game.player_pos.y + oy)
               || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y + oy)
               || game.tile_map.is_solid_world(new_x + ox, game.player_pos.y)
               || game.tile_map.is_solid_world(new_x + ox + pw, game.player_pos.y);
        if (!bx) game.player_pos.x = new_x;

        float new_y = game.player_pos.y + move.y * speed * dt;
        bool by = game.tile_map.is_solid_world(game.player_pos.x + ox, new_y + oy)
               || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y + oy)
               || game.tile_map.is_solid_world(game.player_pos.x + ox, new_y)
               || game.tile_map.is_solid_world(game.player_pos.x + ox + pw, new_y);
        if (!by) game.player_pos.y = new_y;

        if (std::abs(move.x) > std::abs(move.y))
            game.player_dir = (move.x < 0) ? 2 : 3;
        else
            game.player_dir = (move.y < 0) ? 1 : 0;

        game.anim_timer += dt;
        if (game.anim_timer >= 0.2f) {
            game.anim_timer -= 0.2f;
            game.player_frame = 1 - game.player_frame;
        }

        // Breadcrumb trail for party followers
        game.trail_step_accum += speed * dt;
        const float TRAIL_STEP = 4.0f;
        while (game.trail_step_accum >= TRAIL_STEP) {
            game.trail_step_accum -= TRAIL_STEP;
            game.trail[game.trail_head] = {game.player_pos, game.player_dir};
            game.trail_head = (game.trail_head + 1) % GameState::TRAIL_SIZE;
            if (game.trail_count < GameState::TRAIL_SIZE) game.trail_count++;
        }

        // Update party followers
        for (int pi = 0; pi < (int)game.party.size(); pi++) {
            int delay = GameState::FOLLOW_DISTANCE * (pi + 1);
            if (game.trail_count >= delay) {
                int idx = (game.trail_head - delay + GameState::TRAIL_SIZE) % GameState::TRAIL_SIZE;
                auto& pm = game.party[pi];
                auto& target = game.trail[idx];
                float dx = target.pos.x - pm.position.x;
                float dy = target.pos.y - pm.position.y;
                float d = std::sqrt(dx*dx + dy*dy);
                pm.moving = (d > 2.0f);
                if (pm.moving) {
                    float lerp_speed = speed * 1.1f;
                    if (d <= lerp_speed * dt) { pm.position = target.pos; }
                    else { pm.position.x += (dx/d)*lerp_speed*dt; pm.position.y += (dy/d)*lerp_speed*dt; }
                    if (std::abs(dx) > std::abs(dy)) pm.dir = (dx < 0) ? 2 : 3;
                    else pm.dir = (dy < 0) ? 1 : 0;
                    pm.anim_timer += dt;
                    if (pm.anim_timer >= 0.2f) { pm.anim_timer -= 0.2f; pm.frame = 1 - pm.frame; }
                } else {
                    pm.dir = target.dir; pm.frame = 0; pm.anim_timer = 0.0f;
                }
            }
        }

        // (Random encounters disabled — battles only through NPC interaction)
    } else {
        game.player_frame = 0; game.anim_timer = 0.0f;
        for (auto& pm : game.party) { pm.moving = false; pm.frame = 0; pm.anim_timer = 0.0f; }
    }

    // ── NPC-NPC separation (spatial grid — O(n) average) ──
    static constexpr float NPC_SEP_DIST = 40.0f;
    static constexpr float NPC_SEP_FORCE = 200.0f;
    static constexpr float GRID_CELL = NPC_SEP_DIST;  // Cell size matches check radius
    {
        // Build spatial grid of visible NPC indices
        std::unordered_map<int64_t, std::vector<int>> sep_grid;
        auto cell_key = [](int cx, int cy) -> int64_t { return ((int64_t)cx << 32) | (uint32_t)cy; };
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            if (!game.npcs[i].schedule.currently_visible) continue;
            int cx = (int)(game.npcs[i].position.x / GRID_CELL);
            int cy = (int)(game.npcs[i].position.y / GRID_CELL);
            sep_grid[cell_key(cx, cy)].push_back(i);
        }
        // Check neighbors in 3x3 grid around each NPC
        for (auto& [key, indices] : sep_grid) {
            int cx0 = (int)(key >> 32), cy0 = (int)(key & 0xFFFFFFFF);
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    auto nit = sep_grid.find(cell_key(cx0+dx, cy0+dy));
                    if (nit == sep_grid.end()) continue;
                    for (int i : indices) {
                        for (int j : nit->second) {
                            if (j <= i) continue;
                            auto& a = game.npcs[i];
                            auto& b = game.npcs[j];
                            float sx = a.position.x - b.position.x;
                            float sy = a.position.y - b.position.y;
                            float sd2 = sx*sx + sy*sy;
                            if (sd2 > NPC_SEP_DIST * NPC_SEP_DIST || sd2 < 0.01f) {
                                if (sd2 < 0.01f) {
                                    float angle = (game.rng() % 628) / 100.0f;
                                    a.position.x += std::cos(angle) * 8.0f;
                                    a.position.y += std::sin(angle) * 8.0f;
                                }
                                continue;
                            }
                            float sd = std::sqrt(sd2);
                            float overlap = 1.0f - (sd / NPC_SEP_DIST);
                            float push = overlap * overlap * NPC_SEP_FORCE * dt;
                            float nx = sx / sd, ny = sy / sd;
                            a.position.x += nx * push; a.position.y += ny * push;
                            b.position.x -= nx * push; b.position.y -= ny * push;
                        }
                    }
                }
            }
        }
    }

    // ── Despawn hostile mobs at day ──
    if (game.day_night.game_hours >= 6.0f && game.day_night.game_hours < 7.0f) {
        for (int i = (int)game.npcs.size() - 1; i >= 0; i--) {
            auto& npc = game.npcs[i];
            if (!npc.despawn_at_day || !npc.hostile) continue;
            if (!npc.schedule.currently_visible) continue; // Already hidden

            // Drop loot from loot tables before despawning
            for (auto& table : game.loot_tables) {
                if (table.enemy_name == npc.battle_enemy_name || table.enemy_name == "*") {
                    for (auto& entry : table.entries) {
                        float roll = (game.rng() % 1000) / 1000.0f;
                        if (roll < entry.drop_chance) {
                            WorldDrop wd;
                            wd.item_id = entry.item_id; wd.item_name = entry.item_name;
                            wd.description = entry.description; wd.type = entry.type;
                            wd.heal_hp = entry.heal_hp; wd.damage = entry.damage;
                            wd.element = entry.element; wd.sage_func = entry.sage_func;
                            float angle = (game.rng() % 628) / 100.0f;
                            float r = 10.0f + (game.rng() % 20);
                            wd.position = {npc.position.x + std::cos(angle) * r,
                                           npc.position.y + std::sin(angle) * r};
                            game.world_drops.push_back(wd);
                        }
                    }
                }
            }

            // Call loot_func if set
            if (!npc.loot_func.empty() && game.script_engine) {
                game.script_engine->set_string("drop_x", std::to_string(npc.position.x));
                game.script_engine->set_string("drop_y", std::to_string(npc.position.y));
                if (game.script_engine->has_function(npc.loot_func))
                    game.script_engine->call_function(npc.loot_func);
            }

            // Don't delete spawn templates — just hide them
            bool is_template = false;
            for (auto& loop : game.spawn_loops)
                if (loop.npc_template_name == npc.name) { is_template = true; break; }

            if (is_template) {
                npc.position = {-9999, -9999};
                npc.home_pos = npc.position;
                npc.schedule.currently_visible = false;
                npc.has_triggered = false;
            } else {
                game.npcs.erase(game.npcs.begin() + i);
            }
        }
    }

    // ── World item drops: pickup + lifetime ──
    for (int di = (int)game.world_drops.size() - 1; di >= 0; di--) {
        auto& drop = game.world_drops[di];
        drop.anim_timer += dt;
        drop.lifetime -= dt;
        if (drop.lifetime <= 0) { game.world_drops.erase(game.world_drops.begin() + di); continue; }

        // Check player pickup
        float pdx = game.player_pos.x - drop.position.x;
        float pdy = game.player_pos.y - drop.position.y;
        float pdist = std::sqrt(pdx*pdx + pdy*pdy);
        if (pdist < drop.pickup_radius) {
            game.inventory.add(drop.item_id, drop.item_name, 1, drop.type, drop.description,
                               drop.heal_hp, drop.damage, drop.element, drop.sage_func);
            game.script_ui.notifications.push_back({"Picked up " + drop.item_name, 2.0f, 0.0f});
            game.world_drops.erase(game.world_drops.begin() + di);
        }
    }

    // ── NPC AI update ──
    int ts = game.tile_map.tile_size();
    for (int i = 0; i < (int)game.npcs.size(); i++) {
        auto& npc = game.npcs[i];

        // Schedule check: hide NPCs outside their active hours
        if (npc.schedule.has_schedule) {
            bool in_range = eb::is_hour_in_range(game.day_night.game_hours,
                                                  npc.schedule.start_hour, npc.schedule.end_hour);
            if (in_range && !npc.schedule.currently_visible) {
                npc.position = npc.schedule.spawn_point;
                npc.home_pos = npc.schedule.spawn_point;
                npc.wander_target = npc.position;
                npc.schedule.currently_visible = true;
            } else if (!in_range) {
                npc.schedule.currently_visible = false;
                npc.moving = false;
                continue;
            }
        }

        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        // Priority 1: Hostile chase
        if (npc.hostile && !npc.has_triggered && dist < npc.aggro_range) {
            npc.aggro_active = true;
            float nx = dx / dist, ny = dy / dist;
            npc.position.x += nx * npc.move_speed * dt;
            npc.position.y += ny * npc.move_speed * dt;
            npc.moving = true;
            if (std::abs(dx) > std::abs(dy))
                npc.dir = (dx > 0) ? 3 : 2;
            else
                npc.dir = (dy > 0) ? 0 : 1;
            if (dist < npc.attack_range) {
                npc.has_triggered = true;
                npc.aggro_active = false;
                npc.moving = false;
                game.dialogue.start(npc.dialogue);
                game.pending_battle_npc = npc.has_battle ? i : -1;
            }
        }
        // Priority 2: Route following (non-blocking, each NPC fully independent)
        else if (npc.route.active && !npc.route.waypoints.empty()) {
            npc.moving = true;
            npc.route.stuck_timer += dt;

            auto& wp = npc.route.waypoints[npc.route.current_waypoint];
            float rx = wp.x - npc.position.x, ry = wp.y - npc.position.y;
            float rd = std::sqrt(rx*rx + ry*ry);

            // Advance waypoint helper
            auto advance_waypoint = [&]() {
                npc.route.stuck_timer = 0;
                npc.route.pathfind_failures = 0;
                npc.path_active = false;
                npc.current_path.clear();
                int wps = (int)npc.route.waypoints.size();
                if (npc.route.mode == RouteMode::Patrol) {
                    npc.route.current_waypoint = (npc.route.current_waypoint + 1) % wps;
                } else if (npc.route.mode == RouteMode::Once) {
                    if (npc.route.current_waypoint + 1 < wps) npc.route.current_waypoint++;
                    else { npc.route.active = false; npc.moving = false; }
                } else if (npc.route.mode == RouteMode::PingPong) {
                    if (npc.route.forward) {
                        if (npc.route.current_waypoint + 1 < wps) npc.route.current_waypoint++;
                        else { npc.route.forward = false; if (npc.route.current_waypoint > 0) npc.route.current_waypoint--; }
                    } else {
                        if (npc.route.current_waypoint > 0) npc.route.current_waypoint--;
                        else { npc.route.forward = true; if (wps > 1) npc.route.current_waypoint++; }
                    }
                }
            };

            // Reached waypoint — advance
            if (rd <= 12.0f) {
                advance_waypoint();
            }
            // Stuck too long — skip to next waypoint
            else if (npc.route.stuck_timer > npc.route.stuck_timeout) {
                advance_waypoint();
            }
            // Move toward waypoint
            else {
                // Use direct movement (simple and reliable, works with separation forces)
                // Only use A* for long distances with obstacles
                bool use_pathfind = (rd > 120.0f) && (npc.route.pathfind_failures < 3);

                if (use_pathfind && !npc.path_active) {
                    int sx2 = (int)(npc.position.x / ts), sy2 = (int)(npc.position.y / ts);
                    int ex2 = (int)(wp.x / ts), ey2 = (int)(wp.y / ts);
                    npc.current_path = eb::find_path(game.tile_map, sx2, sy2, ex2, ey2);
                    npc.path_index = 0;
                    npc.path_active = !npc.current_path.empty();
                    if (!npc.path_active) npc.route.pathfind_failures++;
                }

                if (npc.path_active && !npc.current_path.empty()) {
                    // Follow A* path
                    auto& target = npc.current_path[npc.path_index];
                    float ptx = target.x * ts + ts * 0.5f;
                    float pty = target.y * ts + ts * 0.5f;
                    float px = ptx - npc.position.x, py = pty - npc.position.y;
                    float pd = std::sqrt(px*px + py*py);
                    if (pd > 2.0f) {
                        float mx = (px/pd) * npc.move_speed * dt;
                        float my = (py/pd) * npc.move_speed * dt;
                        npc.position.x += mx; npc.position.y += my;
                        if (std::abs(px) > std::abs(py)) npc.dir = (px > 0) ? 3 : 2;
                        else npc.dir = (py > 0) ? 0 : 1;
                    } else {
                        npc.path_index++;
                        if (npc.path_index >= (int)npc.current_path.size()) {
                            npc.path_active = false;
                            npc.current_path.clear();
                        }
                    }
                } else {
                    // Direct movement toward waypoint (always works)
                    float mx = (rx/rd) * npc.move_speed * dt;
                    float my = (ry/rd) * npc.move_speed * dt;
                    // Collision check
                    float new_x = npc.position.x + mx;
                    float new_y = npc.position.y + my;
                    if (!game.tile_map.is_solid_world(new_x, npc.position.y)) npc.position.x = new_x;
                    if (!game.tile_map.is_solid_world(npc.position.x, new_y)) npc.position.y = new_y;
                    if (std::abs(rx) > std::abs(ry)) npc.dir = (rx > 0) ? 3 : 2;
                    else npc.dir = (ry > 0) ? 0 : 1;
                }
            }
        }
        // Priority 2b: One-shot path following (from npc_move_to, no route)
        else if (npc.path_active && !npc.current_path.empty()) {
            auto& target = npc.current_path[npc.path_index];
            float ptx = target.x * ts + ts * 0.5f;
            float pty = target.y * ts + ts * 0.5f;
            float px = ptx - npc.position.x, py = pty - npc.position.y;
            float pd = std::sqrt(px*px + py*py);
            if (pd > 2.0f) {
                npc.position.x += (px/pd) * npc.move_speed * dt;
                npc.position.y += (py/pd) * npc.move_speed * dt;
                npc.moving = true;
                if (std::abs(px) > std::abs(py)) npc.dir = (px > 0) ? 3 : 2;
                else npc.dir = (py > 0) ? 0 : 1;
            } else {
                npc.path_index++;
                if (npc.path_index >= (int)npc.current_path.size()) {
                    npc.path_active = false;
                    npc.current_path.clear();
                    npc.moving = false;
                }
            }
        }
        // Priority 4: Idle wander (with collision check)
        else if (!npc.hostile || npc.has_triggered) {
            npc.wander_timer += dt;
            if (npc.wander_timer >= npc.wander_interval) {
                npc.wander_timer = 0.0f;
                float angle = (game.rng() % 628) / 100.0f;
                float r = 16.0f + (game.rng() % 32);
                npc.wander_target = eb::Vec2(
                    npc.home_pos.x + std::cos(angle) * r,
                    npc.home_pos.y + std::sin(angle) * r);
            }
            float wx = npc.wander_target.x - npc.position.x;
            float wy = npc.wander_target.y - npc.position.y;
            float wd = std::sqrt(wx*wx + wy*wy);
            if (wd > 3.0f) {
                float new_x = npc.position.x + (wx/wd) * npc.move_speed * dt;
                float new_y = npc.position.y + (wy/wd) * npc.move_speed * dt;
                // Collision check before moving
                if (!game.tile_map.is_solid_world(new_x, new_y))
                    { npc.position.x = new_x; npc.position.y = new_y; }
                npc.moving = true;
                if (std::abs(wx) > std::abs(wy)) npc.dir = (wx > 0) ? 3 : 2;
                else npc.dir = (wy > 0) ? 0 : 1;
            } else {
                npc.moving = false;
            }
        }
        // Animation
        if (npc.moving) {
            npc.anim_timer += dt;
            if (npc.anim_timer >= 0.2f) { npc.anim_timer -= 0.2f; npc.frame = 1 - npc.frame; }
        } else {
            npc.frame = 0; npc.anim_timer = 0.0f;
        }
    }

    // ── NPC-to-NPC meet triggers ──
    for (auto& trigger : game.npc_meet_triggers) {
        if (trigger.fired && !trigger.repeatable) continue;
        NPC* a = nullptr; NPC* b = nullptr;
        for (auto& n : game.npcs) {
            if (n.name == trigger.npc1_name) a = &n;
            if (n.name == trigger.npc2_name) b = &n;
        }
        if (!a || !b) continue;
        if (!a->schedule.currently_visible || !b->schedule.currently_visible) continue;
        float mdx = b->position.x - a->position.x, mdy = b->position.y - a->position.y;
        float mdist = std::sqrt(mdx*mdx + mdy*mdy);
        if (mdist < trigger.trigger_radius) {
            trigger.fired = true;
            if (game.script_engine && game.script_engine->has_function(trigger.callback_func))
                game.script_engine->call_function(trigger.callback_func);
        }
    }

    // Manual NPC interaction (Z/A button for friendly NPCs)
    if (input.is_pressed(eb::InputAction::Confirm)) {
        for (int i = 0; i < (int)game.npcs.size(); i++) {
            auto& npc = game.npcs[i];
            if (npc.hostile && !npc.has_triggered) continue;
            float dx = game.player_pos.x - npc.position.x;
            float dy = game.player_pos.y - npc.position.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < npc.interact_radius) {
                // All dialogue driven through SageLang scripts
                // Try npc-name-specific function first, then generic "greeting"
                bool handled = false;
                if (game.script_engine) {
                    // Build lowercase NPC name for function lookup
                    std::string npc_lower = npc.name;
                    for (auto& c : npc_lower) c = std::tolower(c);
                    // Replace spaces/special chars with underscore
                    for (auto& c : npc_lower) if (c == ' ' || c == '?' || c == '!') c = '_';

                    // Check if this NPC has a shop (e.g. "merchant_shop_items")
                    std::string shop_func = npc_lower + "_shop_items";
                    if (game.script_engine->has_function(shop_func)) {
                        game.script_engine->call_function(shop_func);
                        handled = true;
                    }
                    // Try: npc_name_greeting (e.g. "merchant_greeting")
                    else {
                        std::string specific = npc_lower + "_greeting";
                        if (game.script_engine->has_function(specific)) {
                            game.script_engine->call_function(specific);
                            handled = true;
                        }
                        // Try: just "greeting" (last-loaded script wins)
                        else if (game.script_engine->has_function("greeting")) {
                            game.script_engine->call_function("greeting");
                            handled = true;
                        }
                    }
                }
                // Fallback: use static dialogue lines (from .dialogue file or default)
                if (!handled && !npc.dialogue.empty()) {
                    game.dialogue.start(npc.dialogue);
                }
                game.pending_battle_npc = npc.has_battle ? i : -1;
                break;
            }
        }
    }

    // ── Portal auto-transition ──
    if (game.level_manager) {
        int ts = game.tile_map.tile_size();
        if (ts > 0) {
            int ptx = (int)(game.player_pos.x / ts);
            int pty = (int)(game.player_pos.y / ts);
            if (game.tile_map.collision_at(ptx, pty) == eb::CollisionType::Portal) {
                auto* portal = game.tile_map.get_portal_at(ptx, pty);
                if (portal && !portal->target_map.empty()) {
                    std::string target = portal->target_map;
                    float tx = portal->target_x * (float)ts;
                    float ty = portal->target_y * (float)ts;
                    // Load target level if not cached
                    if (!game.level_manager->is_loaded(target)) {
                        game.level_manager->load_level(target, "assets/maps/" + target, game);
                    }
                    game.level_manager->switch_level_at(target, tx, ty, game);
                    game.script_ui.notifications.push_back({"Entered: " + target, 2.0f, 0.0f});
                }
            }
        }

        // Tick background levels
        game.level_manager->tick_background(game, dt);
    }

    game.camera.follow(game.player_pos, 4.0f);
    game.camera.update(dt);
}

