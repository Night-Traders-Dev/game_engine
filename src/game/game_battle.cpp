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
#include <cstdio>
#include <cmath>
#include <queue>


// ─── Battle logic ───

void start_battle(GameState& game, const std::string& enemy, int hp, int atk, bool random, int sprite_id) {
    auto& b = game.battle;
    b.phase = BattlePhase::Intro;
    b.enemy_name = enemy;
    b.enemy_hp_actual = hp; b.enemy_hp_max = hp; b.enemy_atk = atk;
    b.enemy_sprite_id = sprite_id;
    b.sam_hp_actual = game.sam_hp; b.sam_hp_max = game.sam_hp_max;
    b.sam_hp_display = static_cast<float>(game.sam_hp);
    b.sam_atk = game.sam_atk + game.ally_stats.weapon_damage_bonus();
    b.active_fighter = 0; // Player goes first
    b.attack_anim_timer = 0.0f;
    b.player_hp_actual = game.player_hp;
    b.player_hp_max = game.player_hp_max;
    b.player_hp_display = static_cast<float>(game.player_hp);
    // Apply character stat bonuses
    b.player_atk = game.player_atk + game.player_stats.weapon_damage_bonus();
    b.player_def = game.player_def + game.player_stats.defense_bonus();
    b.menu_selection = 0; b.phase_timer = 0.0f;
    b.item_menu_open = false; b.item_menu_selection = 0;
    b.message = "A " + enemy + " appeared!";
    b.last_damage = 0; b.random_encounter = random;
}

void update_battle(GameState& game, float dt, bool confirm, bool up, bool down) {
    auto& b = game.battle;
    b.phase_timer += dt;
    b.attack_anim_timer += dt;

    // Roll HP displays toward actual
    auto roll_hp = [&](float& display, int actual) {
        if (display > actual) {
            display -= 40.0f * dt;
            if (display < actual) display = static_cast<float>(actual);
        }
    };
    roll_hp(b.player_hp_display, b.player_hp_actual);
    roll_hp(b.sam_hp_display, b.sam_hp_actual);

    switch (b.phase) {
    case BattlePhase::Intro:
        if (b.phase_timer > 1.5f || confirm) {
            b.phase = BattlePhase::PlayerTurn; b.phase_timer = 0.0f;
            b.active_fighter = 0; b.menu_selection = 0; b.message = "";
        }
        break;

    case BattlePhase::PlayerTurn:
        if (b.item_menu_open) {
            // ── Item submenu navigation ──
            auto battle_items = game.inventory.get_battle_items();
            int item_count = (int)battle_items.size();
            if (item_count == 0) {
                b.item_menu_open = false; // No items, close
                break;
            }
            if (up && b.item_menu_selection > 0) b.item_menu_selection--;
            if (down && b.item_menu_selection < item_count - 1) b.item_menu_selection++;
            if (confirm) {
                // Use selected item
                auto* item = battle_items[b.item_menu_selection];
                b.phase_timer = 0.0f; b.attack_anim_timer = 0.0f;

                if (game.script_engine && !item->sage_func.empty() &&
                    game.script_engine->has_function(item->sage_func)) {
                    game.script_engine->sync_battle_to_script();
                    game.script_engine->sync_item_to_script(item->id);
                    game.script_engine->call_function(item->sage_func);
                    game.script_engine->sync_battle_from_script();
                } else {
                    // Fallback: generic item use
                    const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                    if (item->heal_hp > 0) {
                        int heal = item->heal_hp;
                        if (b.active_fighter == 0)
                            b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                        else
                            b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
                        b.message = std::string(fighter) + " uses " + item->name + "! Healed " + std::to_string(heal) + " HP!";
                    } else if (item->damage > 0) {
                        int dmg = item->damage + (game.rng() % 5);
                        b.enemy_hp_actual -= dmg; b.last_damage = dmg;
                        b.message = std::string(fighter) + " uses " + item->name + "! " + std::to_string(dmg) + " damage!";
                    }
                    game.inventory.remove(item->id, 1);
                }
                b.item_menu_open = false;
                b.phase = BattlePhase::PlayerAttack;
            }
        } else {
            // ── Main battle menu: Attack / Items / Defend / Run ──
            if (up && b.menu_selection > 0) b.menu_selection--;
            if (down && b.menu_selection < 3) b.menu_selection++;
            if (confirm) {
                b.phase_timer = 0.0f; b.attack_anim_timer = 0.0f;

                if (b.menu_selection == 0) {
                    // Attack — use SageLang if available
                    if (game.script_engine && game.script_engine->has_function("attack_normal")) {
                        game.script_engine->sync_battle_to_script();
                        game.script_engine->call_function("attack_normal");
                        game.script_engine->sync_battle_from_script();
                    } else {
                        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                        int atk = (b.active_fighter == 0) ? b.player_atk : b.sam_atk;
                        int damage = atk + (game.rng() % 5) - 2;
                        if (damage < 1) damage = 1;
                        b.enemy_hp_actual -= damage; b.last_damage = damage;
                        b.message = std::string(fighter) + " attacks! " + std::to_string(damage) + " damage!";
                    }
                    b.phase = BattlePhase::PlayerAttack;
                } else if (b.menu_selection == 1) {
                    // Items — open item submenu
                    auto battle_items = game.inventory.get_battle_items();
                    if (battle_items.empty()) {
                        b.message = "No items!";
                        // Stay on player turn, don't advance phase
                    } else {
                        b.item_menu_open = true;
                        b.item_menu_selection = 0;
                    }
                } else if (b.menu_selection == 2) {
                    // Defend — use SageLang if available
                    if (game.script_engine && game.script_engine->has_function("defend")) {
                        game.script_engine->sync_battle_to_script();
                        game.script_engine->call_function("defend");
                        game.script_engine->sync_battle_from_script();
                    } else {
                        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";
                        int heal = 8 + game.rng() % 8;
                        if (b.active_fighter == 0)
                            b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
                        else
                            b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
                        b.message = std::string(fighter) + " braces! Recovered " + std::to_string(heal) + " HP.";
                    }
                    b.phase = BattlePhase::PlayerAttack;
                } else {
                    // Run
                    if (b.random_encounter && (game.rng() % 3) != 0) {
                        b.message = "Got away safely!";
                        b.phase = BattlePhase::Victory; b.phase_timer = 0.0f;
                    } else {
                        b.message = "Can't escape!";
                        b.phase = BattlePhase::PlayerAttack;
                    }
                }
            }
        }
        break;

    case BattlePhase::PlayerAttack:
        if (b.phase_timer > 1.2f || confirm) {
            if (b.enemy_hp_actual <= 0) {
                b.phase = BattlePhase::Victory;
                int xp = (int)((b.enemy_hp_max / 2 + b.enemy_atk) * game.xp_multiplier);
                b.message = "Victory! Gained " + std::to_string(xp) + " XP!";
                game.player_xp += xp;
            } else if (b.active_fighter == 0 && b.sam_hp_actual > 0) {
                // Ally's turn next
                b.active_fighter = 1; b.menu_selection = 0;
                b.phase = BattlePhase::PlayerTurn; b.message = "";
            } else {
                // Enemy turn
                b.phase = BattlePhase::EnemyTurn;
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::EnemyTurn: {
        // Use SageLang enemy AI if available
        bool scripted = false;
        if (game.script_engine) {
            game.script_engine->sync_battle_to_script();
            // Try vampire-specific attack first, then generic
            if (b.enemy_name == "Vampire" && game.script_engine->has_function("vampire_attack")) {
                game.script_engine->call_function("vampire_attack");
                scripted = true;
            } else if (game.script_engine->has_function("enemy_turn")) {
                game.script_engine->call_function("enemy_turn");
                scripted = true;
            }
            if (scripted) game.script_engine->sync_battle_from_script();
        }

        if (!scripted) {
            // Fallback C++ logic
            int target = (game.rng() % 2 == 0 && b.sam_hp_actual > 0) ? 1 : 0;
            if (b.player_hp_actual <= 0) target = 1;
            if (b.sam_hp_actual <= 0) target = 0;
            int def = (target == 0) ? b.player_def : 2;
            int damage = b.enemy_atk + (game.rng() % 5) - 2 - def / 3;
            if (damage < 1) damage = 1;
            if (target == 0) {
                b.player_hp_actual -= damage;
                b.message = b.enemy_name + " attacks Hero! " + std::to_string(damage) + " damage!";
            } else {
                b.sam_hp_actual -= damage;
                b.message = b.enemy_name + " attacks Ally! " + std::to_string(damage) + " damage!";
            }
            b.last_damage = damage;
        }

        b.phase = BattlePhase::EnemyAttack; b.phase_timer = 0.0f;
        b.attack_anim_timer = 0.0f;
        break;
    }

    case BattlePhase::EnemyAttack:
        if (b.phase_timer > 1.2f || confirm) {
            bool player_down = b.player_hp_actual <= 0;
            bool ally_down = b.sam_hp_actual <= 0;
            if (player_down && ally_down) {
                b.player_hp_actual = 0; b.sam_hp_actual = 0;
                b.phase = BattlePhase::Defeat; b.message = "The party has fallen!";
            } else {
                b.active_fighter = 0; b.menu_selection = 0;
                // Skip player if they're down
                if (b.player_hp_actual <= 0) b.active_fighter = 1;
                b.phase = BattlePhase::PlayerTurn; b.message = "";
            }
            b.phase_timer = 0.0f;
        }
        break;

    case BattlePhase::Victory:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = std::max(b.player_hp_actual, 1);
            game.sam_hp = std::max(b.sam_hp_actual, 1);

            // Spawn loot drops from loot tables
            eb::Vec2 drop_pos = game.player_pos; // Drop near player
            // Find matching loot table for this enemy
            for (auto& table : game.loot_tables) {
                if (table.enemy_name == b.enemy_name || table.enemy_name == "*") {
                    for (auto& entry : table.entries) {
                        float roll = (game.rng() % 1000) / 1000.0f;
                        if (roll < entry.drop_chance) {
                            WorldDrop wd;
                            wd.item_id = entry.item_id;
                            wd.item_name = entry.item_name;
                            wd.description = entry.description;
                            wd.type = entry.type;
                            wd.heal_hp = entry.heal_hp;
                            wd.damage = entry.damage;
                            wd.element = entry.element;
                            wd.sage_func = entry.sage_func;
                            // Scatter drops around the player
                            float angle = (game.rng() % 628) / 100.0f;
                            float r = 20.0f + (game.rng() % 30);
                            wd.position = {drop_pos.x + std::cos(angle) * r,
                                           drop_pos.y + std::sin(angle) * r};
                            game.world_drops.push_back(wd);
                        }
                    }
                }
            }

            // Call loot_func + remove defeated NPC
            for (int ni = (int)game.npcs.size() - 1; ni >= 0; ni--) {
                auto& npc = game.npcs[ni];
                if (npc.battle_enemy_name == b.enemy_name && npc.has_triggered) {
                    // Call loot func if set
                    if (!npc.loot_func.empty() && game.script_engine) {
                        game.script_engine->set_string("drop_x", std::to_string(drop_pos.x));
                        game.script_engine->set_string("drop_y", std::to_string(drop_pos.y));
                        if (game.script_engine->has_function(npc.loot_func))
                            game.script_engine->call_function(npc.loot_func);
                    }
                    // Check if this NPC is a spawn template — if so, hide instead of delete
                    bool is_template = false;
                    for (auto& loop : game.spawn_loops)
                        if (loop.npc_template_name == npc.name) { is_template = true; break; }
                    if (is_template) {
                        // Hide the template: move off-screen, reset trigger so it can respawn
                        npc.position = {-9999, -9999};
                        npc.home_pos = npc.position;
                        npc.has_triggered = true;
                        npc.schedule.currently_visible = false;
                    } else {
                        game.npcs.erase(game.npcs.begin() + ni);
                    }
                    break;
                }
            }

            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::Defeat:
        if (b.phase_timer > 2.0f || confirm) {
            game.player_hp = game.player_hp_max / 2;
            game.sam_hp = game.sam_hp_max / 2;
            b.phase = BattlePhase::None;
        }
        break;

    case BattlePhase::None: break;
    }
}


// ─── Render battle ───

void render_battle(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float sw, float sh) {
    auto& b = game.battle;
    float sprite_w = 72.0f, sprite_h = 96.0f;

    // Background
    batch.set_texture(game.white_desc);
    batch.draw_quad({0,0},{sw,sh},{0,0},{1,1},{0.02f,0.02f,0.08f,1.0f});

    // ── Enemy sprite (top center, facing down toward player) ──
    float enemy_cx = sw * 0.5f;
    float enemy_y = 20.0f;
    float ew = sprite_w * 1.5f, eh = sprite_h * 1.5f;

    // Enemy hit flash during player attack
    bool enemy_flash = (b.phase == BattlePhase::PlayerAttack &&
                        b.attack_anim_timer < 0.8f &&
                        std::fmod(b.attack_anim_timer, 0.15f) < 0.075f);

    if (!enemy_flash) {
        eb::TextureAtlas* battle_atlas = nullptr;
        VkDescriptorSet battle_desc = VK_NULL_HANDLE;
        if (!b.enemy_sprite_key.empty()) {
            auto it = game.atlas_cache.find(b.enemy_sprite_key);
            if (it != game.atlas_cache.end()) {
                battle_atlas = it->second.get();
                battle_desc = game.atlas_descs[b.enemy_sprite_key];
            }
        }
        if (!battle_atlas && b.enemy_sprite_id >= 0 && b.enemy_sprite_id < (int)game.npc_atlases.size()) {
            battle_atlas = game.npc_atlases[b.enemy_sprite_id].get();
            battle_desc = game.npc_descs[b.enemy_sprite_id];
        }
        if (battle_atlas && battle_desc) {
            auto sr = get_character_sprite(*battle_atlas, 0, false, 0);
            batch.set_texture(battle_desc);
            batch.draw_quad({enemy_cx - ew*0.5f, enemy_y}, {ew, eh}, sr.uv_min, sr.uv_max);
        }
    }

    // Enemy name + HP bar
    float ebx = sw*0.5f - 120, eby = enemy_y + eh + 6;
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx,eby},{240,45},{0,0},{1,1},{0.1f,0.08f,0.15f,0.8f});
    text.draw_text(batch, game.font_desc, b.enemy_name, {ebx+8,eby+4}, {1,0.4f,0.4f,1}, 0.9f);
    float hp_pct = b.enemy_hp_max > 0 ? std::max(0.0f,(float)b.enemy_hp_actual/b.enemy_hp_max) : 0.0f;
    batch.set_texture(game.white_desc);
    batch.draw_quad({ebx+8,eby+24},{170,12},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 ehc = hp_pct>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:hp_pct>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({ebx+8,eby+24},{170*hp_pct,12},{0,0},{1,1},ehc);
    text.draw_text(batch,game.font_desc,std::to_string(std::max(0,b.enemy_hp_actual))+"/"+std::to_string(b.enemy_hp_max),
                   {ebx+185,eby+22},{1,1,1,1},0.6f);

    // ── Player + Ally sprites (lower area, backs to us) ──
    float party_y = sh * 0.42f;
    float party_cx = sw * 0.5f;
    float player_x = party_cx - sprite_w - 10.0f;
    float ally_x = party_cx + 10.0f;

    // Attack animation: lunge forward during PlayerAttack phase
    float player_offset_y = 0, ally_offset_y = 0;
    if (b.phase == BattlePhase::PlayerAttack && b.attack_anim_timer < 0.5f) {
        float t = b.attack_anim_timer / 0.5f;
        float lunge = std::sin(t * 3.14159f) * 30.0f; // Lunge forward and back
        if (b.active_fighter == 0) player_offset_y = -lunge;
        else ally_offset_y = -lunge;
    }

    // Player (left) — flash red if hit
    bool player_hit = (b.phase == BattlePhase::EnemyAttack &&
                     b.message.find("Hero") != std::string::npos &&
                     b.attack_anim_timer < 0.6f &&
                     std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!player_hit && b.player_hp_actual > 0 && game.dean_atlas) {
        bool player_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 0);
        int player_frame = player_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.dean_atlas, 1, player_attacking, player_frame);
        batch.set_texture(game.dean_desc);
        batch.draw_quad({player_x, party_y + player_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // Ally (right)
    bool ally_hit = (b.phase == BattlePhase::EnemyAttack &&
                    b.message.find("Ally") != std::string::npos &&
                    b.attack_anim_timer < 0.6f &&
                    std::fmod(b.attack_anim_timer, 0.12f) < 0.06f);
    if (!ally_hit && b.sam_hp_actual > 0 && game.sam_atlas) {
        bool ally_attacking = (b.phase == BattlePhase::PlayerAttack && b.active_fighter == 1);
        int ally_frame = ally_attacking ? ((int)(b.attack_anim_timer * 8) % 2) : 0;
        auto sr = get_character_sprite(*game.sam_atlas, 1, ally_attacking, ally_frame);
        batch.set_texture(game.sam_desc);
        batch.draw_quad({ally_x, party_y + ally_offset_y}, {sprite_w, sprite_h}, sr.uv_min, sr.uv_max);
    }

    // ── Party stats (bottom right) ──
    float pbx = sw - 260, pby = sh - 100;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx,pby},{240,92},{0,0},{1,1},{0.08f,0.08f,0.18f,0.9f});
    batch.draw_quad({pbx,pby},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});
    batch.draw_quad({pbx,pby+90},{240,2},{0,0},{1,1},{0.5f,0.5f,0.8f,1});

    // Player HP
    eb::Vec4 player_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 0)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Hero",{pbx+8,pby+4},player_name_col,0.7f);
    float dhp = b.player_hp_display;
    float dp = b.player_hp_max > 0 ? std::max(0.0f,dhp/b.player_hp_max) : 0.0f;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+60,pby+8},{120,10},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 dc=dp>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:dp>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+60,pby+8},{120*dp,10},{0,0},{1,1},dc);
    char dhs[32]; std::snprintf(dhs,sizeof(dhs),"%d/%d",(int)std::ceil(dhp),b.player_hp_max);
    text.draw_text(batch,game.font_desc,dhs,{pbx+186,pby+5},{1,1,1,1},0.5f);

    // Ally HP
    eb::Vec4 ally_name_col = (b.phase == BattlePhase::PlayerTurn && b.active_fighter == 1)
        ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{1,1,1,1};
    text.draw_text(batch,game.font_desc,"Ally",{pbx+8,pby+24},ally_name_col,0.7f);
    float shp = b.sam_hp_display;
    float sp = b.sam_hp_max > 0 ? std::max(0.0f,shp/b.sam_hp_max) : 0.0f;
    batch.set_texture(game.white_desc);
    batch.draw_quad({pbx+60,pby+28},{120,10},{0,0},{1,1},{0.2f,0.2f,0.2f,1});
    eb::Vec4 sc2=sp>0.5f?eb::Vec4{0.2f,0.8f,0.2f,1}:sp>0.25f?eb::Vec4{0.9f,0.7f,0.1f,1}:eb::Vec4{0.9f,0.2f,0.2f,1};
    batch.draw_quad({pbx+60,pby+28},{120*sp,10},{0,0},{1,1},sc2);
    char shs[32]; std::snprintf(shs,sizeof(shs),"%d/%d",(int)std::ceil(shp),b.sam_hp_max);
    text.draw_text(batch,game.font_desc,shs,{pbx+186,pby+25},{1,1,1,1},0.5f);

    // Level
    text.draw_text(batch,game.font_desc,"Lv."+std::to_string(game.player_level),{pbx+8,pby+46},{0.7f,0.7f,0.7f,1},0.5f);

    // ── Battle menu (player turn) ──
    if (b.phase == BattlePhase::PlayerTurn) {
        const char* fighter = (b.active_fighter == 0) ? "Hero" : "Ally";

        if (b.item_menu_open) {
            // ── Item submenu ──
            auto battle_items = game.inventory.get_battle_items();
            int item_count = (int)battle_items.size();
            int visible = std::min(item_count, 6); // Max 6 visible at once
            float imh = 26.0f + visible * 24.0f + 8.0f;
            float imx = 16, imy = sh - imh - 10;

            batch.set_texture(game.white_desc);
            batch.draw_quad({imx,imy},{210,imh},{0,0},{1,1},{0.06f,0.04f,0.14f,0.95f});
            batch.draw_quad({imx,imy},{210,2},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx,imy+imh-2},{210,2},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx,imy},{2,imh},{0,0},{1,1},{0.5f,0.5f,0.9f,1});
            batch.draw_quad({imx+208,imy},{2,imh},{0,0},{1,1},{0.5f,0.5f,0.9f,1});

            text.draw_text(batch,game.font_desc,"Items",{imx+8,imy+4},{0.4f,0.8f,1,1},0.7f);

            // Scroll offset if more items than visible
            int scroll = 0;
            if (b.item_menu_selection >= visible) scroll = b.item_menu_selection - visible + 1;

            for (int i = 0; i < visible && (i + scroll) < item_count; i++) {
                int idx = i + scroll;
                auto* item = battle_items[idx];
                eb::Vec4 c = (idx == b.item_menu_selection) ? eb::Vec4{1,1,0.3f,1} : eb::Vec4{0.8f,0.8f,0.8f,1};
                std::string pfx = (idx == b.item_menu_selection) ? "> " : "  ";
                std::string label = pfx + item->name + " x" + std::to_string(item->quantity);
                text.draw_text(batch,game.font_desc,label,{imx+8,imy+26+i*24.0f},c,0.7f);
            }
        } else {
            // ── Main battle menu ──
            float mx = 16, my = sh - 152;
            batch.set_texture(game.white_desc);
            batch.draw_quad({mx,my},{170,144},{0,0},{1,1},{0.08f,0.05f,0.15f,0.95f});
            batch.draw_quad({mx,my},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx,my+142},{170,2},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx,my},{2,144},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            batch.draw_quad({mx+168,my},{2,144},{0,0},{1,1},{0.6f,0.6f,0.8f,1});
            // Fighter name header
            text.draw_text(batch,game.font_desc,fighter,{mx+8,my+4},{0.4f,0.8f,1,1},0.7f);
            const char* opts[]={"Attack","Items","Defend","Run"};
            for(int i=0;i<4;i++){
                eb::Vec4 c=(i==b.menu_selection)?eb::Vec4{1,1,0.3f,1}:eb::Vec4{0.8f,0.8f,0.8f,1};
                std::string pfx=(i==b.menu_selection)?"> ":"  ";
                text.draw_text(batch,game.font_desc,pfx+opts[i],{mx+8,my+26+i*28.0f},c,0.9f);
            }
        }
    }

    // ── Message box ──
    if (!b.message.empty()) {
        float mw = sw*0.55f, mh = 36;
        float mx2 = (sw-mw)*0.5f, my2 = sh*0.40f;
        batch.set_texture(game.white_desc);
        batch.draw_quad({mx2,my2},{mw,mh},{0,0},{1,1},{0.05f,0.05f,0.12f,0.92f});
        batch.draw_quad({mx2,my2},{mw,2},{0,0},{1,1},{0.5f,0.5f,0.7f,1});
        text.draw_text(batch,game.font_desc,b.message,{mx2+10,my2+8},{1,1,1,1},0.8f);
    }
}

