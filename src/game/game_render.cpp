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


// Forward declarations for helpers defined in this file
static void draw_ui_region(eb::SpriteBatch& batch, GameState& game, const char* name, float x, float y, float w, float h, float rotation = 0.0f);
static void draw_ui_icon(eb::SpriteBatch& batch, GameState& game, const char* icon, float x, float y, float sz, float rotation = 0.0f);
static void render_grass_overlay(const GameState& game, eb::SpriteBatch& batch, float time);
static void render_leaf_overlay(const GameState& game, eb::SpriteBatch& batch, float time);
static void render_hud(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text, float screen_w, float screen_h);
static void sync_hud_values(GameState& game);

static float tile_hash(int x, int y, int seed) {
    int h = (x * 374761393 + y * 668265263 + seed * 1274126177) ^ 0x5bf03635;
    h = ((h >> 13) ^ h) * 1274126177;
    return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// ─── Grass overlay ───

static void render_grass_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    eb::Rect view = game.camera.visible_area();
    float ts = (float)game.tile_map.tile_size();
    int sx = std::max(0, (int)std::floor(view.x / ts));
    int sy = std::max(0, (int)std::floor(view.y / ts));
    int ex = std::min(game.tile_map.width(), (int)std::ceil((view.x + view.w) / ts) + 1);
    int ey = std::min(game.tile_map.height(), (int)std::ceil((view.y + view.h) / ts) + 1);
    batch.set_texture(game.white_desc);
    const float margin = 6.0f;
    for (int ty = sy; ty < ey; ty++) {
        for (int tx = sx; tx < ex; tx++) {
            int tile = game.tile_map.tile_at(0, tx, ty);
            if (tile < TILE_GRASS_PURE || tile > TILE_GRASS_DARK) continue;
            auto is_grass = [&](int x, int y) {
                if (x < 0 || x >= game.tile_map.width() || y < 0 || y >= game.tile_map.height()) return false;
                int t = game.tile_map.tile_at(0, x, y);
                return t >= TILE_GRASS_PURE && t <= TILE_GRASS_DARK;
            };
            if (!is_grass(tx-1,ty)||!is_grass(tx+1,ty)||!is_grass(tx,ty-1)||!is_grass(tx,ty+1)) continue;
            float x_min = margin, x_max = ts - margin, y_min = margin, y_max = ts - margin;
            int tuft_count = 2 + (int)(tile_hash(tx, ty, 0) * 2.0f);
            for (int t = 0; t < tuft_count; t++) {
                float fx = tile_hash(tx,ty,t*5+1), fy = tile_hash(tx,ty,t*5+2);
                float base_x = tx*ts + x_min + fx*(x_max-x_min);
                float base_y = ty*ts + y_min + fy*(y_max-y_min);
                float wind = std::sin(time*2.0f + tx*0.7f + ty*0.5f + t*1.3f) * 1.5f;
                int blades = 2 + (int)(tile_hash(tx,ty,t*5+3)*1.5f);
                for (int b = 0; b < blades; b++) {
                    float blade_h = 2.0f + tile_hash(tx,ty,t*5+b+10)*2.0f;
                    float spread = (b-(blades-1)*0.5f)*1.5f;
                    float blade_sway = wind * (0.7f + tile_hash(tx,ty,t*5+b+20)*0.6f);
                    float shade = 0.7f + tile_hash(tx,ty,t*5+b+30)*0.3f;
                    batch.draw_quad({base_x+spread+blade_sway, base_y-blade_h},
                                    {1.0f, blade_h}, {0,0},{1,1},
                                    {0.2f*shade, 0.55f*shade, 0.12f*shade, 0.8f});
                }
            }
        }
    }
}

// ─── Leaf overlay ───

static void render_leaf_overlay(const GameState& game, eb::SpriteBatch& batch, float time) {
    batch.set_texture(game.white_desc);
    for (int oi = 0; oi < (int)game.world_objects.size(); oi++) {
        const auto& obj = game.world_objects[oi];
        if (obj.sprite_id > 4 || obj.sprite_id < 0 || obj.sprite_id >= (int)game.object_defs.size()) continue;
        const auto& def = game.object_defs[obj.sprite_id];
        float canopy_w = def.render_size.x * 0.8f, canopy_h = def.render_size.y * 0.5f;
        float canopy_cx = obj.position.x, canopy_cy = obj.position.y - def.render_size.y * 0.7f;
        int leaf_count = 6 + (int)(tile_hash(oi, 0, 99) * 6.0f);
        for (int l = 0; l < leaf_count; l++) {
            float lx_f = tile_hash(oi,l,10)-0.5f, ly_f = tile_hash(oi,l,20)-0.5f;
            float leaf_x = canopy_cx + lx_f*canopy_w, leaf_y = canopy_cy + ly_f*canopy_h;
            float leaf_sz = 2.0f + tile_hash(oi,l,30)*2.5f;
            float wind_phase = time*1.8f + oi*2.1f + l*0.9f;
            float sway_x = std::sin(wind_phase)*2.0f, sway_y = std::cos(wind_phase*0.7f)*1.0f;
            float flutter = std::sin(time*4.0f + l*3.7f);
            if (flutter > 0.7f) { sway_x *= 2.0f; sway_y *= 1.5f; }
            float shade = 0.6f + tile_hash(oi,l,40)*0.4f;
            float r = 0.15f + tile_hash(oi,l,50)*0.15f;
            batch.draw_sorted({leaf_x+sway_x-leaf_sz*0.5f, leaf_y+sway_y-leaf_sz*0.5f},
                              {leaf_sz, leaf_sz}, {0,0},{1,1},
                              obj.position.y - 0.05f, game.white_desc,
                              {r*shade, 0.55f*shade, 0.1f*shade, 0.75f});
        }
    }
}


// ─── Render HUD ───

static void draw_ui_region(eb::SpriteBatch& batch, GameState& game,
                            const char* name, float x, float y, float w, float h, float rotation) {
    // Check flat UI pack first for flat_ prefixed names
    if (name[0] == 'f' && name[1] == 'l' && game.ui_flat_atlas) {
        auto* r = game.ui_flat_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_flat_desc);
            batch.draw_quad_rotated({x, y}, {w, h}, r->uv_min, r->uv_max, rotation);
            return;
        }
    }
    // Standard UI atlas
    if (game.ui_atlas) {
        auto* r = game.ui_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_desc);
            batch.draw_quad_rotated({x, y}, {w, h}, r->uv_min, r->uv_max, rotation);
            return;
        }
    }
    // Fallback: solid dark panel
    batch.set_texture(game.white_desc);
    batch.draw_quad_rotated({x, y}, {w, h}, {0,0}, {1,1}, rotation, {0.05f, 0.05f, 0.12f, 0.88f});
}

static void draw_ui_icon(eb::SpriteBatch& batch, GameState& game,
                          const char* name, float x, float y, float sz, float rotation) {
    // Fantasy icon by index: "fi_42" = fantasy icon #42
    if (name[0] == 'f' && name[1] == 'i' && name[2] == '_') {
        int idx = std::atoi(name + 3);
        if (game.fantasy_icons_atlas && idx >= 0 && idx < game.fantasy_icons_atlas->region_count()) {
            auto r = game.fantasy_icons_atlas->region(idx);
            batch.set_texture(game.fantasy_icons_desc);
            batch.draw_quad_rotated({x, y}, {sz, sz}, r.uv_min, r.uv_max, rotation);
            return;
        }
    }
    // Standard UI atlas icon
    if (game.ui_atlas) {
        auto* r = game.ui_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.ui_desc);
            batch.draw_quad_rotated({x, y}, {sz, sz}, r->uv_min, r->uv_max, rotation);
            return;
        }
    }
    // Fantasy icons atlas by name search (fallback)
    if (game.fantasy_icons_atlas) {
        auto* r = game.fantasy_icons_atlas->find_region(name);
        if (r) {
            batch.set_texture(game.fantasy_icons_desc);
            batch.draw_quad_rotated({x, y}, {sz, sz}, r->uv_min, r->uv_max, rotation);
        }
    }
}

static void render_hud(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                        float screen_w, float screen_h) {
    auto& H = game.hud;
    float S = H.scale;

    // ── Player Stats Panel (top-left) ──
    if (H.show_player) {
        float hx = H.player_x, hy = H.player_y;
        float hw = H.player_w * S, hh = H.player_h * S;
        draw_ui_region(batch, game, "panel_hud_wide", hx, hy, hw, hh);

        // Name + Level
        char ns[64]; std::snprintf(ns, sizeof(ns), "Mage  Lv.%d", game.player_level);
        text.draw_text(batch, game.font_desc, ns, {hx + 14*S, hy + 10*S}, {1,1,1,1}, H.text_scale * S);

        // HP bar with heart icon
        float icon_sz = 18 * S;
        draw_ui_icon(batch, game, "icon_heart_red", hx + 12*S, hy + 34*S, icon_sz);
        float hp_pct = game.player_hp_max > 0 ? std::max(0.0f, (float)game.player_hp / game.player_hp_max) : 0.0f;
        float bx = hx + 14*S + icon_sz + 4*S, by = hy + 36*S;
        float bw = H.hp_bar_w * S, bh = H.hp_bar_h * S;
        batch.set_texture(game.white_desc);
        batch.draw_quad({bx, by}, {bw, bh}, {0,0}, {1,1}, {0.15f, 0.15f, 0.2f, 0.9f});
        eb::Vec4 hc = hp_pct > 0.5f ? eb::Vec4{0.2f, 0.8f, 0.2f, 1}
                    : hp_pct > 0.25f ? eb::Vec4{0.9f, 0.7f, 0.1f, 1}
                    : eb::Vec4{0.9f, 0.2f, 0.2f, 1};
        batch.draw_quad({bx, by}, {bw * hp_pct, bh}, {0,0}, {1,1}, hc);
        char hs[32]; std::snprintf(hs, sizeof(hs), "%d/%d", game.player_hp, game.player_hp_max);
        text.draw_text(batch, game.font_desc, hs, {bx + bw + 8*S, by - 1}, {1,1,1,1}, 0.65f * S);

        // Gold with coin icon
        draw_ui_icon(batch, game, "icon_coin", hx + hw - 70*S, hy + 10*S, icon_sz);
        char gs[32]; std::snprintf(gs, sizeof(gs), "%d", game.gold);
        text.draw_text(batch, game.font_desc, gs, {hx + hw - 48*S, hy + 12*S}, {1.0f, 0.95f, 0.3f, 1}, 0.7f * S);

        // ── Survival Bars (below player panel) ──
        if (game.survival.enabled && H.show_survival) {
            float sy = hy + hh + 4*S;
            float bar_w = H.surv_bar_w * S, bar_h = H.surv_bar_h * S, bar_pad = 3*S;

            batch.set_texture(game.white_desc);
            batch.draw_quad({hx, sy}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, sy}, {bar_w * (game.survival.hunger / 100.0f), bar_h}, {0,0}, {1,1}, {0.85f, 0.55f, 0.15f, 0.9f});

            float ty2 = sy + bar_h + bar_pad;
            batch.draw_quad({hx, ty2}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, ty2}, {bar_w * (game.survival.thirst / 100.0f), bar_h}, {0,0}, {1,1}, {0.2f, 0.5f, 0.9f, 0.9f});

            float ey = ty2 + bar_h + bar_pad;
            batch.draw_quad({hx, ey}, {bar_w, bar_h}, {0,0}, {1,1}, {0.15f, 0.15f, 0.15f, 0.7f});
            batch.draw_quad({hx, ey}, {bar_w * (game.survival.energy / 100.0f), bar_h}, {0,0}, {1,1}, {0.9f, 0.8f, 0.2f, 0.9f});
        }
    }

    // ── Time of Day Panel (top-right) ──
    if (H.show_time) {
        float tw = H.time_w * S, th = H.time_h * S;
        float tx = screen_w - tw - 8, ty = 8;
        draw_ui_region(batch, game, "panel_hud_sq", tx, ty, tw, th);

        int hour = (int)game.day_night.game_hours;
        int minute = (int)((game.day_night.game_hours - hour) * 60.0f);
        bool pm = hour >= 12;
        int dh = hour % 12; if (dh == 0) dh = 12;
        char time_str[16]; std::snprintf(time_str, sizeof(time_str), "%d:%02d %s", dh, minute, pm ? "PM" : "AM");
        text.draw_text(batch, game.font_desc, time_str, {tx + 12*S, ty + 10*S}, {1,1,0.9f,1}, H.time_text_scale * S);

        const char* period; eb::Vec4 pc;
        if (hour >= 6 && hour < 10)       { period = "Morning"; pc = {1.0f, 0.85f, 0.4f, 1}; }
        else if (hour >= 10 && hour < 16)  { period = "Day";     pc = {1.0f, 1.0f, 0.8f, 1}; }
        else if (hour >= 16 && hour < 19)  { period = "Evening"; pc = {1.0f, 0.6f, 0.3f, 1}; }
        else if (hour >= 19 && hour < 21)  { period = "Dusk";    pc = {0.7f, 0.5f, 0.8f, 1}; }
        else                               { period = "Night";   pc = {0.4f, 0.5f, 0.9f, 1}; }
        text.draw_text(batch, game.font_desc, period, {tx + 12*S, ty + 34*S}, pc, 0.65f * S);

        float ico = 20 * S;
        draw_ui_icon(batch, game, (hour >= 6 && hour < 18) ? "icon_star" : "icon_gem_blue",
                     tx + tw - ico - 8*S, ty + 10*S, ico);
    }

    // ── Inventory Quick Bar (bottom-left) ──
    if (H.show_inventory && !game.inventory.items.empty()) {
        float slot_w = H.inv_slot_size * S, slot_h = slot_w;
        float pad = H.inv_padding * S;
        float ix = 8, iy_base = screen_h - H.inv_y_offset * S;
        int max_slots = std::min(H.inv_max_slots, (int)game.inventory.items.size());
        bool sel_mode = game.hud.inv_open;

        float strip_w = max_slots * (slot_w + pad) + pad;
        draw_ui_region(batch, game, "panel_dark", ix - 3, iy_base - 3, strip_w + 6, slot_h + 8);

        // Hint text above the bar
        if (sel_mode) {
            const char* hint = "[Left/Right] Select  [Z] Use  [X] Close";
            float hint_scale = 0.5f * S;
            auto hsz = text.measure_text(hint, hint_scale);
            batch.set_texture(game.white_desc);
            batch.draw_quad({ix, iy_base - 22*S}, {hsz.x + 12, hsz.y + 6},
                            {0,0}, {1,1}, {0, 0, 0, 0.7f});
            text.draw_text(batch, game.font_desc, hint,
                           {ix + 6, iy_base - 20*S}, {0.7f, 0.8f, 1.0f, 0.9f}, hint_scale);
        } else if (game.inventory.items.size() > 0) {
            // Show "X: Items" hint
            const char* open_hint = "[X] Items";
            float oh_scale = 0.45f * S;
            text.draw_text(batch, game.font_desc, open_hint,
                           {ix, iy_base - 14*S}, {0.5f, 0.5f, 0.55f, 0.6f}, oh_scale);
        }

        for (int i = 0; i < max_slots; i++) {
            auto& item = game.inventory.items[i];
            float sx = ix + i * (slot_w + pad) + pad;
            bool selected = sel_mode && i == game.hud.inv_selected;

            // Slot background
            batch.set_texture(game.white_desc);
            if (selected) {
                // Bright highlight for selected slot
                batch.draw_quad({sx - 2, iy_base - 2}, {slot_w + 4, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.9f, 0.3f, 0.35f});
                batch.draw_quad({sx - 2, iy_base - 2}, {slot_w + 4, 2.5f},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx - 2, iy_base + slot_h}, {slot_w + 4, 2.5f},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx - 2, iy_base - 2}, {2.5f, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
                batch.draw_quad({sx + slot_w, iy_base - 2}, {2.5f, slot_h + 4},
                                {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.9f});
            }
            batch.draw_quad({sx, iy_base}, {slot_w, slot_h}, {0,0}, {1,1},
                            selected ? eb::Vec4{0.15f, 0.15f, 0.25f, 0.9f}
                                     : eb::Vec4{0.1f, 0.1f, 0.18f, 0.8f});
            batch.draw_quad({sx, iy_base}, {slot_w, 1.5f}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.6f});
            batch.draw_quad({sx, iy_base + slot_h - 1.5f}, {slot_w, 1.5f}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.6f});

            // Item icon
            const char* icon_name = "icon_gem_blue";
            if (item.damage > 0) icon_name = "icon_sword";
            else if (item.heal_hp > 0) icon_name = "icon_potion";
            else if (item.element == "fire") icon_name = "icon_heart_red";
            else if (item.element == "holy") icon_name = "icon_star";
            else if (item.element == "ice") icon_name = "icon_gem_green";
            else if (item.element == "lightning") icon_name = "icon_ring";
            float icon_sz = slot_w * 0.55f;
            draw_ui_icon(batch, game, icon_name, sx + (slot_w - icon_sz) * 0.5f, iy_base + 4*S, icon_sz);

            // Quantity badge
            if (item.quantity > 1) {
                char qty[8]; std::snprintf(qty, sizeof(qty), "%d", item.quantity);
                text.draw_text(batch, game.font_desc, qty,
                               {sx + slot_w - 14*S, iy_base + slot_h - 16*S},
                               {1, 1, 1, 0.9f}, 0.5f * S);
            }
        }

        // "..." if more items
        if ((int)game.inventory.items.size() > max_slots) {
            float dx = ix + max_slots * (slot_w + pad) + pad + 4;
            text.draw_text(batch, game.font_desc, "...", {dx, iy_base + slot_h * 0.3f},
                           {0.6f, 0.6f, 0.6f, 0.8f}, 0.6f * S);
        }

        // Show selected item name + description below the bar
        if (sel_mode && game.hud.inv_selected < (int)game.inventory.items.size()) {
            auto& sel_item = game.inventory.items[game.hud.inv_selected];
            float desc_y = iy_base + slot_h + 8*S;
            float desc_w = strip_w + 6;

            // Description panel
            draw_ui_region(batch, game, "panel_mini", ix - 3, desc_y, desc_w, 36*S);

            // Item name (bright)
            text.draw_text(batch, game.font_desc, sel_item.name,
                           {ix + 6, desc_y + 4*S}, {1, 1, 0.9f, 1}, 0.7f * S);

            // Description (dim)
            if (!sel_item.description.empty()) {
                text.draw_text(batch, game.font_desc, sel_item.description,
                               {ix + 6, desc_y + 20*S}, {0.7f, 0.7f, 0.65f, 0.9f}, 0.5f * S);
            }
        }
    }

    // ── Minimap (bottom-right corner) ──
    if (H.show_minimap && game.tile_map.width() > 0 && game.tile_map.height() > 0) {
        float mm_base = H.minimap_size * S;
        float mw = (float)game.tile_map.width();
        float mh = (float)game.tile_map.height();
        float aspect = mw / mh;
        float mm_w, mm_h;
        if (aspect >= 1.0f) { mm_w = mm_base; mm_h = mm_base / aspect; }
        else { mm_h = mm_base; mm_w = mm_base * aspect; }

        float mm_x = screen_w - mm_w - 10;
        float mm_y = screen_h - mm_h - 10;
        float px_w = mm_w / mw;  // Pixels per tile
        float px_h = mm_h / mh;

        // Background panel
        draw_ui_region(batch, game, "panel_hud_sq", mm_x - 4, mm_y - 4, mm_w + 8, mm_h + 8);

        // Draw tile colors (every Nth tile for performance on large maps)
        batch.set_texture(game.white_desc);
        int step = std::max(1, (int)mw / 80);  // Limit to ~80 samples across
        for (int y = 0; y < (int)mh; y += step) {
            for (int x = 0; x < (int)mw; x += step) {
                int t = game.tile_map.tile_at(0, x, y);
                eb::Vec4 col;
                if (t == 0)                          col = {0.08f, 0.08f, 0.12f, 1};
                else if (t >= 42 && t <= 50)         col = {0.12f, 0.24f, 0.47f, 1}; // Water
                else if (t >= 25 && t <= 36)         col = {0.31f, 0.31f, 0.31f, 1}; // Roads
                else if (t >= 5 && t <= 8)           col = {0.47f, 0.35f, 0.24f, 1}; // Dirt
                else if (t >= 19 && t <= 24)         col = {0.20f, 0.12f, 0.16f, 1}; // Dark
                else                                 col = {0.20f, 0.39f, 0.16f, 1}; // Grass
                if (game.tile_map.collision_at(x, y) == eb::CollisionType::Solid && t > 0)
                    col = {col.x * 0.5f, col.y * 0.5f, col.z * 0.5f, 1};

                batch.draw_quad({mm_x + x * px_w, mm_y + y * px_h},
                                {px_w * step + 0.5f, px_h * step + 0.5f}, {0,0}, {1,1}, col);
            }
        }

        // Player dot (yellow, pulsing)
        float ts = (float)game.tile_map.tile_size();
        float pp_x = mm_x + (game.player_pos.x / ts) * px_w;
        float pp_y = mm_y + (game.player_pos.y / ts) * px_h;
        float dot_sz = 4.0f * S;
        float pulse = 0.8f + 0.2f * std::sin(game.game_time * 5.0f);
        batch.draw_quad({pp_x - dot_sz*0.5f, pp_y - dot_sz*0.5f}, {dot_sz, dot_sz},
                        {0,0}, {1,1}, {1.0f, 1.0f, 0.3f, pulse});

        // NPC dots
        for (auto& npc : game.npcs) {
            if (!npc.schedule.currently_visible) continue;
            float np_x = mm_x + (npc.position.x / ts) * px_w;
            float np_y = mm_y + (npc.position.y / ts) * px_h;
            float nd = 2.5f * S;
            eb::Vec4 nc = npc.hostile ? eb::Vec4{1.0f, 0.2f, 0.2f, 0.9f}
                                      : eb::Vec4{0.2f, 0.8f, 1.0f, 0.7f};
            batch.draw_quad({np_x - nd*0.5f, np_y - nd*0.5f}, {nd, nd}, {0,0}, {1,1}, nc);
        }

        // Item drop dots (gold)
        for (auto& drop : game.world_drops) {
            float dp_x = mm_x + (drop.position.x / ts) * px_w;
            float dp_y = mm_y + (drop.position.y / ts) * px_h;
            batch.draw_quad({dp_x - 1.5f, dp_y - 1.5f}, {3, 3},
                            {0,0}, {1,1}, {1.0f, 0.85f, 0.2f, 0.8f});
        }

        // Border
        batch.draw_quad({mm_x, mm_y}, {mm_w, 1}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x, mm_y + mm_h - 1}, {mm_w, 1}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x, mm_y}, {1, mm_h}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
        batch.draw_quad({mm_x + mm_w - 1, mm_y}, {1, mm_h}, {0,0}, {1,1}, {0.4f, 0.4f, 0.55f, 0.7f});
    }
}

// ─── Render world ───

void render_game_world(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text) {
    // Apply camera shake offset
    eb::Mat4 proj = game.camera.projection_matrix();
    if (game.shake_timer > 0 && game.shake_intensity > 0) {
        float sx = ((game.rng() % 200) - 100) / 100.0f * game.shake_intensity;
        float sy = ((game.rng() % 200) - 100) / 100.0f * game.shake_intensity;
        proj[3][0] += sx;
        proj[3][1] += sy;
    }
    batch.set_projection(proj);

    // ── Parallax backgrounds (rendered behind everything) ──
    for (auto& layer : game.parallax_layers) {
        if (!layer.active || !layer.texture_desc) continue;
        VkDescriptorSet desc = (VkDescriptorSet)layer.texture_desc;
        batch.set_texture(desc);
        // Camera offset * scroll factor = parallax offset
        eb::Vec2 cam = game.camera.position();
        float px = -cam.x * layer.scroll_x + layer.offset_x;
        float py = -cam.y * layer.scroll_y + layer.offset_y;
        float tw = (float)layer.tex_width;
        float th = (float)layer.tex_height;
        if (tw <= 0 || th <= 0) continue;
        eb::Rect view = game.camera.visible_area();
        if (layer.repeat_x) {
            // Wrap horizontally
            float mod_x = std::fmod(px, tw);
            if (mod_x > 0) mod_x -= tw;
            for (float dx = mod_x; dx < view.w + tw; dx += tw) {
                batch.draw_quad({view.x + dx, view.y + py}, {tw, th}, {0,0}, {1,1});
            }
        } else {
            batch.draw_quad({view.x + px, view.y + py}, {tw, th}, {0,0}, {1,1});
        }
    }

    // Tile map with water animation
    batch.set_texture(game.tileset_desc);
    game.tile_map.render(batch, game.camera, game.game_time);

    // Grass overlay
    render_grass_overlay(game, batch, game.game_time);

    // Y-sorted objects (with per-object scale and tint)
    for (const auto& obj : game.world_objects) {
        if (obj.sprite_id < 0 || obj.sprite_id >= (int)game.object_defs.size() ||
            obj.sprite_id >= (int)game.object_regions.size()) continue;
        const auto& def = game.object_defs[obj.sprite_id];
        const auto& region = game.object_regions[obj.sprite_id];
        float sw = def.render_size.x * obj.scale;
        float sh = def.render_size.y * obj.scale;
        eb::Vec2 dp = {obj.position.x - sw*0.5f, obj.position.y - sh};
        batch.draw_sorted(dp, {sw, sh}, region.uv_min, region.uv_max,
                         obj.position.y, game.tileset_desc, obj.tint);
    }

    // Leaf overlay
    render_leaf_overlay(game, batch, game.game_time);

    // NPCs (skip scheduled-out NPCs)
    for (const auto& npc : game.npcs) {
        if (!npc.schedule.currently_visible) continue;
        // Prefer string-keyed atlas cache (with grid-size-aware key)
        eb::TextureAtlas* atlas_ptr = nullptr;
        VkDescriptorSet desc = VK_NULL_HANDLE;
        if (!npc.sprite_atlas_key.empty()) {
            // Build cache key with optional grid size suffix
            std::string cache_key = npc.sprite_atlas_key;
            if (npc.sprite_grid_w > 0 && npc.sprite_grid_h > 0) {
                char buf[32]; std::snprintf(buf, sizeof(buf), "@%dx%d", npc.sprite_grid_w, npc.sprite_grid_h);
                cache_key += buf;
            }
            auto it = game.atlas_cache.find(cache_key);
            if (it != game.atlas_cache.end()) {
                atlas_ptr = it->second.get();
                desc = game.atlas_descs[cache_key];
            } else {
                // Try without grid suffix (backward compat)
                auto it2 = game.atlas_cache.find(npc.sprite_atlas_key);
                if (it2 != game.atlas_cache.end()) {
                    atlas_ptr = it2->second.get();
                    desc = game.atlas_descs[npc.sprite_atlas_key];
                }
            }
        }
        // Fallback to legacy indexed lookup
        if (!atlas_ptr && npc.sprite_atlas_id >= 0 && npc.sprite_atlas_id < (int)game.npc_atlases.size()) {
            atlas_ptr = game.npc_atlases[npc.sprite_atlas_id].get();
            desc = game.npc_descs[npc.sprite_atlas_id];
        }
        if (atlas_ptr && desc) {
            auto sr = get_character_sprite(*atlas_ptr, npc.dir, npc.moving, npc.frame);
            // Use per-NPC cell size for render dimensions, with 1.5x scale
            float base_w = (npc.sprite_grid_w > 0) ? (float)npc.sprite_grid_w : 32.0f;
            float base_h = (npc.sprite_grid_h > 0) ? (float)npc.sprite_grid_h : 32.0f;
            float rw = base_w * 1.5f * npc.sprite_scale;
            float rh = base_h * 1.5f * npc.sprite_scale;
            eb::Vec2 uv0 = sr.uv_min, uv1 = sr.uv_max;
            if (npc.sprite_flip_h) std::swap(uv0.x, uv1.x);
            eb::Vec2 dp = {npc.position.x-rw*0.5f, npc.position.y-rh+4};
            batch.draw_sorted(dp, {rw,rh}, uv0, uv1, npc.position.y, desc, npc.sprite_tint);
        }
    }

    // Party followers (with ally sprite scale)
    if (game.sam_atlas) {
        for (int pi = 0; pi < (int)game.party.size(); pi++) {
            auto& pm = game.party[pi];
            auto sr = get_character_sprite(*game.sam_atlas, pm.dir, pm.moving, pm.frame);
            float rw = 48 * game.ally_sprite_scale, rh = 64 * game.ally_sprite_scale;
            float bob = pm.moving ? std::sin(pm.anim_timer*15.0f)*2.0f : 0.0f;
            eb::Vec2 dp = {pm.position.x-rw*0.5f, pm.position.y-rh+4+bob};
            batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, pm.position.y, game.sam_desc);
        }
    }

    // Player character (with per-player sprite scale)
    // Render at same size as NPCs (48x64) so all characters are consistent
    if (game.dean_atlas) {
        auto sr = get_character_sprite(*game.dean_atlas, game.player_dir, game.player_moving, game.player_frame);
        float rw = 48.0f * game.player_sprite_scale;
        float rh = 64.0f * game.player_sprite_scale;
        float bob = game.player_moving ? std::sin(game.anim_timer*15.0f)*2.0f : 0.0f;
        eb::Vec2 dp = {game.player_pos.x-rw*0.5f, game.player_pos.y-rh+4+bob};
        batch.draw_sorted(dp, {rw,rh}, sr.uv_min, sr.uv_max, game.player_pos.y, game.dean_desc);
    }

    // World item drops (bouncing icons on the ground)
    for (auto& drop : game.world_drops) {
        float bob = std::sin(drop.anim_timer * 4.0f) * 3.0f;
        float glow = 0.5f + 0.3f * std::sin(drop.anim_timer * 3.0f);
        float drop_sz = 20.0f;
        eb::Vec2 dp = {drop.position.x - drop_sz * 0.5f, drop.position.y - drop_sz - 4.0f + bob};

        // Glow circle underneath
        batch.set_texture(game.white_desc);
        batch.draw_sorted({drop.position.x - 10, drop.position.y - 4},
                         {20, 8}, {0,0}, {1,1}, drop.position.y + 1,
                         game.white_desc, {1.0f, 0.9f, 0.3f, glow * 0.3f});

        // Item icon
        const char* icon = "icon_gem_blue";
        if (drop.damage > 0) icon = "icon_sword";
        else if (drop.heal_hp > 0) icon = "icon_potion";
        else if (drop.element == "fire") icon = "icon_heart_red";
        else if (drop.element == "holy") icon = "icon_star";
        if (game.ui_atlas) {
            auto* r = game.ui_atlas->find_region(icon);
            if (r) {
                batch.draw_sorted(dp, {drop_sz, drop_sz}, r->uv_min, r->uv_max,
                                 drop.position.y, game.ui_desc);
            }
        }
    }

    batch.flush_sorted();

    // ── Water Reflections ──
    // Draw vertically flipped copies of player/NPCs below water with blue tint
    if (game.water_reflections) {
        float reflect_alpha = 0.35f;
        eb::Vec4 water_tint = {0.5f, 0.6f, 0.9f, reflect_alpha};
        int ts = game.tile_map.tile_size();

        // Player reflection — only on reflective tiles
        if (game.dean_atlas) {
            auto sr = get_character_sprite(*game.dean_atlas, game.player_dir, game.player_moving, game.player_frame);
            float rw = 48.0f * game.player_sprite_scale;
            float rh = 64.0f * game.player_sprite_scale;
            int check_y = (int)((game.player_pos.y + rh * 0.5f) / ts);
            int check_x = (int)(game.player_pos.x / ts);
            if (check_x >= 0 && check_x < game.tile_map.width() &&
                check_y >= 0 && check_y < game.tile_map.height() &&
                game.tile_map.is_reflective(check_x, check_y)) {
                eb::Vec2 dp = {game.player_pos.x - rw * 0.5f, game.player_pos.y + 4};
                batch.set_texture(game.dean_desc);
                batch.draw_quad(dp, {rw, rh * 0.7f},
                    {sr.uv_min.x, sr.uv_max.y}, {sr.uv_max.x, sr.uv_min.y}, water_tint);
            }
        }

        // NPC reflections — only on reflective tiles
        for (const auto& npc : game.npcs) {
            if (!npc.schedule.currently_visible) continue;
            eb::TextureAtlas* atlas_ptr = nullptr;
            VkDescriptorSet desc = VK_NULL_HANDLE;
            if (!npc.sprite_atlas_key.empty()) {
                std::string cache_key = npc.sprite_atlas_key;
                if (npc.sprite_grid_w > 0 && npc.sprite_grid_h > 0) {
                    char buf[32]; std::snprintf(buf, sizeof(buf), "@%dx%d", npc.sprite_grid_w, npc.sprite_grid_h);
                    cache_key += buf;
                }
                auto it = game.atlas_cache.find(cache_key);
                if (it != game.atlas_cache.end()) {
                    atlas_ptr = it->second.get();
                    auto dit = game.atlas_descs.find(cache_key);
                    if (dit != game.atlas_descs.end()) desc = dit->second;
                }
            }
            if (!atlas_ptr && npc.sprite_atlas_id >= 0 && npc.sprite_atlas_id < (int)game.npc_atlases.size()) {
                atlas_ptr = game.npc_atlases[npc.sprite_atlas_id].get();
                desc = game.npc_descs[npc.sprite_atlas_id];
            }
            if (atlas_ptr && desc) {
                int nx = (int)(npc.position.x / ts);
                int ny = (int)((npc.position.y + 16) / ts);
                if (nx >= 0 && nx < game.tile_map.width() &&
                    ny >= 0 && ny < game.tile_map.height() &&
                    game.tile_map.is_reflective(nx, ny)) {
                    auto sr = get_character_sprite(*atlas_ptr, npc.dir, npc.moving, npc.frame);
                    float base_w = (npc.sprite_grid_w > 0) ? (float)npc.sprite_grid_w : 32.0f;
                    float base_h = (npc.sprite_grid_h > 0) ? (float)npc.sprite_grid_h : 32.0f;
                    float rw = base_w * 1.5f * npc.sprite_scale;
                    float rh = base_h * 1.5f * npc.sprite_scale;
                    eb::Vec2 dp = {npc.position.x - rw * 0.5f, npc.position.y + 4};
                    batch.set_texture(desc);
                    batch.draw_quad(dp, {rw, rh * 0.7f},
                        {sr.uv_min.x, sr.uv_max.y}, {sr.uv_max.x, sr.uv_min.y}, water_tint);
                }
            }
        }
    }

    // ── Particles (rendered on top of world, before UI) ──
    // Use default white texture for colored quads
    batch.set_texture(game.white_desc);
    for (auto& emitter : game.emitters) {
        for (auto& p : emitter.particles) {
            if (!p.alive) continue;
            float t = p.progress();
            float sz = p.size + (p.size_end - p.size) * t;
            eb::Vec4 col = {
                p.color.x + (p.color_end.x - p.color.x) * t,
                p.color.y + (p.color_end.y - p.color.y) * t,
                p.color.z + (p.color_end.z - p.color.z) * t,
                p.color.w + (p.color_end.w - p.color.w) * t,
            };
            batch.draw_quad({p.pos.x - sz*0.5f, p.pos.y - sz*0.5f}, {sz, sz}, col);
        }
    }

    // ── Bloom / Glow (cheap: re-draw lights and bright particles larger + transparent) ──
    if (game.bloom_enabled) {
        batch.set_texture(game.white_desc);
        float bi = game.bloom_intensity;
        // Glow around point lights
        for (auto& light : game.lights) {
            if (!light.active) continue;
            float r = light.radius * 1.5f;
            batch.draw_quad(
                {light.x - r, light.y - r}, {r * 2, r * 2},
                {0,0}, {1,1},
                {light.color.x, light.color.y, light.color.z, bi * light.intensity * 0.15f}
            );
        }
        // Glow around bright particles (fire, magic, explosion)
        for (auto& emitter : game.emitters) {
            for (auto& p : emitter.particles) {
                if (!p.alive) continue;
                float t = p.progress();
                float brightness = p.color.x + p.color.y + p.color.z;
                if (brightness < game.bloom_threshold * 3.0f) continue;
                float sz = (p.size + (p.size_end - p.size) * t) * 2.5f;
                float alpha = (p.color.w + (p.color_end.w - p.color.w) * t) * bi * 0.2f;
                eb::Vec4 col = {
                    p.color.x + (p.color_end.x - p.color.x) * t,
                    p.color.y + (p.color_end.y - p.color.y) * t,
                    p.color.z + (p.color_end.z - p.color.z) * t,
                    alpha
                };
                batch.draw_quad({p.pos.x - sz*0.5f, p.pos.y - sz*0.5f}, {sz, sz}, col);
            }
        }
    }

    // ── 2D Lighting (darken scene then brighten around lights) ──
    if (game.lighting_enabled && game.ambient_light < 0.99f) {
        batch.set_texture(game.white_desc);
        auto vis = game.camera.visible_area();
        // Dark overlay covering entire visible area
        float darkness = 1.0f - game.ambient_light;
        batch.draw_quad({vis.x, vis.y}, {vis.w, vis.h}, {0,0}, {1,1},
                        {0, 0, 0, darkness});

        // Punch light circles as bright overlays (additive approximation)
        for (auto& light : game.lights) {
            if (!light.active) continue;
            float r = light.radius;
            // Draw concentric rings for gradient falloff
            int rings = 5;
            for (int i = 0; i < rings; i++) {
                float t = (float)i / rings;
                float ring_r = r * (1.0f - t);
                float alpha = light.intensity * darkness * (1.0f - t) * 0.25f;
                batch.draw_quad(
                    {light.x - ring_r, light.y - ring_r},
                    {ring_r * 2, ring_r * 2},
                    {0,0}, {1,1},
                    {light.color.x, light.color.y, light.color.z, alpha}
                );
            }
        }
    }

    batch.flush();
}

// ─── Sync game state → script UI components each frame ───
static void sync_hud_values(GameState& game) {
    auto find_label = [&](const std::string& id) -> ScriptUILabel* {
        for (auto& l : game.script_ui.labels) if (l.id == id) return &l;
        return nullptr;
    };
    auto find_bar = [&](const std::string& id) -> ScriptUIBar* {
        for (auto& b : game.script_ui.bars) if (b.id == id) return &b;
        return nullptr;
    };
    auto find_image = [&](const std::string& id) -> ScriptUIImage* {
        for (auto& img : game.script_ui.images) if (img.id == id) return &img;
        return nullptr;
    };

    // Player HP
    if (auto* b = find_bar("hud_hp")) { b->value = (float)game.player_hp; b->max_value = (float)game.player_hp_max; }
    if (auto* l = find_label("hud_hp_text")) {
        char s[32]; std::snprintf(s, sizeof(s), "%d/%d", game.player_hp, game.player_hp_max);
        l->text = s;
    }

    // Player name/level
    if (auto* l = find_label("hud_name")) {
        char s[64]; std::snprintf(s, sizeof(s), "Mage  Lv.%d", game.player_level);
        l->text = s;
    }

    // Gold
    if (auto* l = find_label("hud_gold")) { l->text = std::to_string(game.gold); }

    // Time
    if (auto* l = find_label("hud_time")) {
        int hour = (int)game.day_night.game_hours;
        int minute = (int)((game.day_night.game_hours - hour) * 60.0f);
        bool pm = hour >= 12;
        int dh = hour % 12; if (dh == 0) dh = 12;
        char s[16]; std::snprintf(s, sizeof(s), "%d:%02d %s", dh, minute, pm ? "PM" : "AM");
        l->text = s;
    }
    if (auto* l = find_label("hud_period")) {
        int hour = (int)game.day_night.game_hours;
        if (hour >= 6 && hour < 10)       { l->text = "Morning"; l->color = {1.0f, 0.85f, 0.4f, 1}; }
        else if (hour >= 10 && hour < 16)  { l->text = "Day";     l->color = {1.0f, 1.0f, 0.8f, 1}; }
        else if (hour >= 16 && hour < 19)  { l->text = "Evening"; l->color = {1.0f, 0.6f, 0.3f, 1}; }
        else if (hour >= 19 && hour < 21)  { l->text = "Dusk";    l->color = {0.7f, 0.5f, 0.8f, 1}; }
        else                               { l->text = "Night";   l->color = {0.4f, 0.5f, 0.9f, 1}; }
    }

    // Survival
    if (game.survival.enabled) {
        if (auto* b = find_bar("hud_hunger")) { b->value = game.survival.hunger; }
        if (auto* b = find_bar("hud_thirst")) { b->value = game.survival.thirst; }
        if (auto* b = find_bar("hud_energy")) { b->value = game.survival.energy; }
    }

    // Sun/moon icon swap (fantasy icons: 281=sun, 280=moon)
    if (auto* img = find_image("hud_sun")) {
        int hour = (int)game.day_night.game_hours;
        img->icon_name = (hour >= 6 && hour < 18) ? "fi_281" : "fi_280";
    }

    // ── Apply HUD scale to script-created elements ──
    // Uses HUDConfig values (editable via UI editor) instead of hardcoded sizes
    // SKIP position/size overrides when editor is active — lets direct edits take effect
    if (!game.editor_active)
    {
        float S = game.hud.scale;
        float sw = game.hud.screen_w;
        float sh = game.hud.screen_h;
        auto& h = game.hud;

        auto find_panel = [&](const std::string& id) -> ScriptUIPanel* {
            for (auto& p : game.script_ui.panels) if (p.id == id) return &p;
            return nullptr;
        };

        // Player panel — uses HUDConfig dimensions (editable in editor)
        if (auto* p = find_panel("hud_player_bg")) {
            p->position = {h.player_x * S, h.player_y * S};
            p->width = h.player_w * S; p->height = h.player_h * S;
            p->scale = 1.0f; // scale is already baked into dimensions
        }
        if (auto* l = find_label("hud_name")) { l->position = {(h.player_x + 10) * S, (h.player_y + 6) * S}; l->scale = h.text_scale * S; }
        if (auto* img = find_image("hud_heart")) { img->position = {(h.player_x + 8) * S, (h.player_y + 32) * S}; img->width = 16 * S; img->height = 16 * S; }
        if (auto* b = find_bar("hud_hp")) { b->position = {(h.player_x + 28) * S, (h.player_y + 34) * S}; b->width = h.hp_bar_w * S; b->height = h.hp_bar_h * S; }
        if (auto* l = find_label("hud_hp_text")) { l->position = {(h.player_x + 28 + h.hp_bar_w + 6) * S, (h.player_y + 32) * S}; l->scale = 0.6f * S; }
        if (auto* img = find_image("hud_coin")) { img->position = {(h.player_x + 28 + h.hp_bar_w + 6) * S, (h.player_y + 6) * S}; img->width = 14 * S; img->height = 14 * S; }
        if (auto* l = find_label("hud_gold")) { l->position = {(h.player_x + 28 + h.hp_bar_w + 24) * S, (h.player_y + 6) * S}; l->scale = 0.7f * S; }

        // Time panel — uses HUDConfig offset and dimensions
        float tx = sw - h.time_x_offset * S;
        if (auto* p = find_panel("hud_time_bg")) {
            p->position = {tx, h.player_y * S};
            p->width = h.time_w * S; p->height = h.time_h * S;
            p->scale = 1.0f;
        }
        if (auto* img = find_image("hud_sun")) { img->position = {tx + 8 * S, (h.player_y + 4) * S}; img->width = 20 * S; img->height = 20 * S; }
        if (auto* l = find_label("hud_time")) { l->position = {tx + 32 * S, (h.player_y + 6) * S}; l->scale = h.time_text_scale * S; }
        if (auto* l = find_label("hud_period")) { l->position = {tx + 32 * S, (h.player_y + 28) * S}; l->scale = 0.6f * S; }

        // Pause menu — centered, scales with S
        if (game.paused) {
            float pw = 260 * S, ph = 330 * S;
            float px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
            if (auto* p = find_panel("pause_bg")) { p->position = {px, py}; p->width = pw; p->height = ph; p->scale = 1.0f; }
            if (auto* img = find_image("pause_icon")) { img->position = {px + 16*S, py + 14*S}; img->width = 22*S; img->height = 22*S; }
            if (auto* l = find_label("pause_title")) { l->position = {px + 44*S, py + 14*S}; l->scale = 1.3f * S; }
            if (auto* p = find_panel("pause_div")) { p->position = {px + 16*S, py + 44*S}; p->width = 228*S; p->height = 2*S; }
            float ix = px + 56*S, icx = px + 24*S, iy = py + 58*S, gap = 38*S;
            for (int i = 0; i < 6; i++) {
                char iid[32], lid[32];
                std::snprintf(iid, sizeof(iid), "pause_icon_%d", i);
                std::snprintf(lid, sizeof(lid), "pause_item_%d", i);
                if (auto* img = find_image(iid)) { img->position = {icx, iy + i*gap}; img->width = 20*S; img->height = 20*S; }
                if (auto* l = find_label(lid)) { l->position = {ix, iy + i*gap + S}; l->scale = 1.0f * S; }
            }
            if (auto* img = find_image("pause_cursor")) {
                img->position = {icx - 4*S, iy + game.pause_selection * gap - 2*S};
                img->width = 24*S; img->height = 24*S;
            }
        }
    } // end if (!editor_active) block

    // HP bar color (green → yellow → red)
    if (auto* b = find_bar("hud_hp")) {
        float pct = b->max_value > 0 ? b->value / b->max_value : 0;
        if (pct > 0.5f) b->color = {0.2f, 0.8f, 0.2f, 1};
        else if (pct > 0.25f) b->color = {0.9f, 0.7f, 0.1f, 1};
        else b->color = {0.9f, 0.2f, 0.2f, 1};
    }

    // ── Pause menu sync ──
    auto set_vis = [&](const std::string& id, bool vis) {
        for (auto& l : game.script_ui.labels) if (l.id == id) { l.visible = vis; return; }
        for (auto& p : game.script_ui.panels) if (p.id == id) { p.visible = vis; return; }
        for (auto& img : game.script_ui.images) if (img.id == id) { img.visible = vis; return; }
    };

    bool paused = game.paused;
    bool show_pause = paused && !game.level_select_open;
    set_vis("pause_bg", paused);  // Keep bg visible during level select too
    set_vis("pause_icon", show_pause);
    set_vis("pause_title", show_pause);
    set_vis("pause_div", show_pause);
    set_vis("pause_cursor", show_pause);
    static const char* pause_item_ids[6] = {"pause_item_0","pause_item_1","pause_item_2","pause_item_3","pause_item_4","pause_item_5"};
    static const char* pause_icon_ids[6] = {"pause_icon_0","pause_icon_1","pause_icon_2","pause_icon_3","pause_icon_4","pause_icon_5"};
    for (int i = 0; i < 6; i++) {
        set_vis(pause_item_ids[i], show_pause);
        set_vis(pause_icon_ids[i], show_pause);

        // Highlight selected item
        if (auto* l = find_label(pause_item_ids[i])) {
            if (i == game.pause_selection && show_pause) {
                l->color = {1.0f, 1.0f, 0.9f, 1.0f};
            } else {
                l->color = {0.85f, 0.82f, 0.75f, 1.0f};
            }
        }
    }

    // Move pause cursor to selected item
    if (show_pause) {
        for (auto& img : game.script_ui.images) {
            if (img.id == "pause_cursor") {
                if (auto* l = find_label(pause_item_ids[game.pause_selection])) {
                    img.position.y = l->position.y;
                }
                break;
            }
        }
    }

    // ── Level selector sub-menu ──
    // Create/update level list labels dynamically
    if (paused && game.level_select_open) {
        // Show title
        static bool level_title_created = false;
        if (!level_title_created) {
            float cx = game.hud.screen_w / 2.0f;
            float cy = game.hud.screen_h / 2.0f;
            game.script_ui.labels.push_back({"lvl_sel_title", "SELECT LEVEL", {cx - 80, cy - 110}, {1, 0.9f, 0.5f, 1}, 1.2f, true});
            level_title_created = true;
        }
        set_vis("lvl_sel_title", true);

        // Remove old level list labels
        auto& labels = game.script_ui.labels;
        labels.erase(std::remove_if(labels.begin(), labels.end(),
            [](auto& l) { return l.id.size() > 8 && l.id.compare(0, 8, "lvl_sel_") == 0 && l.id != "lvl_sel_title"; }),
            labels.end());

        // Create labels for each loaded level
        float cx = game.hud.screen_w / 2.0f;
        float cy = game.hud.screen_h / 2.0f;
        for (int i = 0; i < (int)game.level_select_ids.size(); i++) {
            std::string lid = "lvl_sel_" + std::to_string(i);
            // Strip .json for display
            std::string display = game.level_select_ids[i];
            auto dot = display.rfind('.');
            if (dot != std::string::npos) display = display.substr(0, dot);
            // Capitalize first letter
            if (!display.empty()) display[0] = std::toupper(display[0]);
            // Mark active level
            if (game.level_manager && game.level_select_ids[i] == game.level_manager->active_level)
                display += "  (current)";

            eb::Vec4 color = (i == game.level_select_cursor)
                ? eb::Vec4{1.0f, 1.0f, 0.9f, 1.0f}
                : eb::Vec4{0.85f, 0.82f, 0.75f, 1.0f};
            labels.push_back({lid, display, {cx - 60, cy - 70 + i * 36.0f}, color, 1.0f, true});
        }
    } else {
        // Hide level selector labels
        set_vis("lvl_sel_title", false);
        auto& labels = game.script_ui.labels;
        labels.erase(std::remove_if(labels.begin(), labels.end(),
            [](auto& l) { return l.id.size() > 8 && l.id.compare(0, 8, "lvl_sel_") == 0 && l.id != "lvl_sel_title"; }),
            labels.end());
    }

    // ── Settings sub-menu ──
    {
        auto& labels = game.script_ui.labels;
        // Remove old settings labels each frame
        labels.erase(std::remove_if(labels.begin(), labels.end(),
            [](auto& l) { return l.id.size() > 5 && l.id.compare(0, 5, "sett_") == 0; }),
            labels.end());

        if (paused && game.settings_open) {
            float cx = game.hud.screen_w / 2.0f;
            float cy = game.hud.screen_h / 2.0f;
            labels.push_back({"sett_title", "SETTINGS", {cx - 60, cy - 110}, {1, 0.9f, 0.5f, 1}, 1.2f, true});

            static const char* setting_names[] = {"Music Volume", "SFX Volume", "Text Speed", "Back"};
            char val_buf[32];
            for (int i = 0; i < 4; i++) {
                std::string display = setting_names[i];
                if (i == 0) { std::snprintf(val_buf, sizeof(val_buf), ": %.0f%%", game.settings.music_volume * 100); display += val_buf; }
                else if (i == 1) { std::snprintf(val_buf, sizeof(val_buf), ": %.0f%%", game.settings.sfx_volume * 100); display += val_buf; }
                else if (i == 2) {
                    const char* speeds[] = {"", "Slow", "Normal", "Fast"};
                    display += std::string(": ") + speeds[game.settings.text_speed];
                }
                eb::Vec4 color = (i == game.settings_cursor)
                    ? eb::Vec4{1.0f, 1.0f, 0.9f, 1.0f}
                    : eb::Vec4{0.85f, 0.82f, 0.75f, 1.0f};
                std::string lid = "sett_" + std::to_string(i);
                labels.push_back({lid, display, {cx - 80, cy - 70 + i * 36.0f}, color, 1.0f, true});
            }
            // Hint
            labels.push_back({"sett_hint", "Left/Right to adjust", {cx - 70, cy + 80}, {0.6f, 0.6f, 0.7f, 0.8f}, 0.6f, true});
        }
    }
}

// ─── Render UI overlay ───

void render_game_ui(GameState& game, eb::SpriteBatch& batch, eb::TextRenderer& text,
                    eb::Mat4 screen_proj, float sw, float sh) {
    batch.set_projection(screen_proj);
    game.hud.screen_w = sw;
    game.hud.screen_h = sh;

    // Sync game values into script UI components
    sync_hud_values(game);

    // Day-night tint overlay (drawn first, under HUD)
    auto& tint = game.day_night.current_tint;
    if (tint.w > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, tint);
    }

    // NPC labels (skip hidden NPCs)
    // Render in world-space projection so labels track NPC position exactly
    batch.set_projection(game.camera.projection_matrix());
    for (const auto& npc : game.npcs) {
        if (!npc.schedule.currently_visible) continue;
        float dx = game.player_pos.x - npc.position.x;
        float dy = game.player_pos.y - npc.position.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < npc.interact_radius * 1.5f) {
            // Position label directly above the NPC sprite
            float label_x = npc.position.x;
            float label_y = npc.position.y - 72.0f; // Just above the 64px sprite

            float ns = 1.1f;
            auto name_size = text.measure_text(npc.name, ns);
            float lp = 8.0f;
            batch.set_texture(game.white_desc);
            batch.draw_quad({label_x - name_size.x*0.5f - lp, label_y - lp*0.5f},
                {name_size.x + lp*2, name_size.y + lp}, {0,0},{1,1}, {0.05f, 0.05f, 0.12f, 0.75f});
            // Border
            batch.draw_quad({label_x - name_size.x*0.5f - lp, label_y - lp*0.5f},
                {name_size.x + lp*2, 1.5f}, {0,0},{1,1}, {0.5f, 0.5f, 0.7f, 0.6f});
            text.draw_text(batch, game.font_desc, npc.name,
                {label_x - name_size.x*0.5f, label_y}, {1, 1, 0.5f, 1}, ns);

            if (dist < npc.interact_radius) {
                const char* hint_text =
#ifdef __ANDROID__
                    "[A] Talk";
#else
                    "[Z] Talk";
#endif
                float hs = 0.85f;
                auto hint_size = text.measure_text(hint_text, hs);
                float hy = label_y + name_size.y + 6.0f;
                batch.set_texture(game.white_desc);
                batch.draw_quad({label_x - hint_size.x*0.5f - 6, hy - 3},
                    {hint_size.x + 12, hint_size.y + 6}, {0,0},{1,1}, {0.05f, 0.05f, 0.12f, 0.65f});
                text.draw_text(batch, game.font_desc, hint_text,
                    {label_x - hint_size.x*0.5f, hy}, {0.8f, 0.9f, 1.0f, 0.95f}, hs);
            }
        }
    }
    // Switch back to screen projection for HUD
    batch.set_projection(screen_proj);

    // HUD
    render_hud(game, batch, text, sw, sh);

    // Dialogue
    if (game.dialogue.is_active()) {
        game.dialogue.render(batch, text, game.font_desc, game.white_desc, sw, sh);
    }

    // Merchant UI
    if (game.merchant_ui.is_open()) {
        game.merchant_ui.render(batch, text, game, sw, sh);
    }

    // ── Script-driven UI elements ──
    // Panels (rendered first as backgrounds)
    for (auto& panel : game.script_ui.panels) {
        if (!panel.visible) continue;
        draw_ui_region(batch, game, panel.sprite_region.c_str(),
                       panel.position.x, panel.position.y, panel.width, panel.height, panel.rotation);
    }
    // Images
    for (auto& img : game.script_ui.images) {
        if (!img.visible) continue;
        draw_ui_icon(batch, game, img.icon_name.c_str(),
                     img.position.x, img.position.y, img.width, img.rotation);
    }
    // Labels
    for (auto& label : game.script_ui.labels) {
        if (!label.visible) continue;
        // TODO: TextRenderer rotation support would need glyph-level transforms
        text.draw_text(batch, game.font_desc, label.text,
                       label.position, label.color, label.scale);
    }
    // Bars
    for (auto& bar : game.script_ui.bars) {
        if (!bar.visible) continue;
        batch.set_texture(game.white_desc);
        batch.draw_quad_rotated(bar.position, {bar.width, bar.height},
                        {0,0}, {1,1}, bar.rotation, bar.bg_color);
        float pct = bar.max_value > 0 ? bar.value / bar.max_value : 0;
        batch.draw_quad_rotated(bar.position, {bar.width * pct, bar.height},
                        {0,0}, {1,1}, bar.rotation, bar.color);
    }
    // Notifications (centered at top)
    for (auto& n : game.script_ui.notifications) {
        float alpha = std::min(1.0f, n.duration - n.timer);
        if (alpha <= 0) continue;
        float ns = 0.8f;
        auto sz = text.measure_text(n.text, ns);
        float nx = (sw - sz.x) * 0.5f;
        float ny = 60.0f;
        batch.set_texture(game.white_desc);
        batch.draw_quad({nx - 12, ny - 4}, {sz.x + 24, sz.y + 8},
                        {0,0}, {1,1}, {0, 0, 0, 0.7f * alpha});
        text.draw_text(batch, game.font_desc, n.text,
                       {nx, ny}, {1, 1, 1, alpha}, ns);
    }

    // ── Weather Rendering ──
    {
        auto& w = game.weather;
        batch.set_texture(game.white_desc);

        // Cloud shadows (drawn first, darkens the scene)
        if (w.clouds_active) {
            for (auto& c : w.cloud_shadows) {
                // Draw overlapping ellipses for soft cloud shadow
                float r = c.radius;
                eb::Vec4 shadow_col = {0, 0, 0, c.opacity};
                batch.draw_quad({c.x - r, c.y - r * 0.6f}, {r * 2, r * 1.2f}, {0,0}, {1,1}, shadow_col);
                // Secondary blob offset for organic shape
                batch.draw_quad({c.x - r * 0.7f + 20, c.y - r * 0.4f - 10}, {r * 1.4f, r * 0.8f}, {0,0}, {1,1},
                    {0, 0, 0, c.opacity * 0.6f});
            }
        }

        // God rays (additive light beams through cloud gaps)
        if (w.god_rays_active && w.clouds_active) {
            float ray_w = sw / w.god_ray_count;
            float angle_offset = w.god_ray_angle + w.god_ray_sway;
            for (int i = 0; i < w.god_ray_count; i++) {
                float base_x = (i + 0.5f) * ray_w;
                // Vary opacity per ray using sine
                float ray_alpha = w.god_ray_color.w * (0.5f + 0.5f * std::sin(game.game_time * 0.5f + i * 1.7f));
                if (ray_alpha < 0.02f) continue;
                float top_x = base_x + angle_offset;
                float beam_w = ray_w * 0.3f;
                // Draw as a tall narrow quad (approximation of a light beam)
                eb::Vec4 rc = {w.god_ray_color.x, w.god_ray_color.y, w.god_ray_color.z, ray_alpha};
                batch.draw_quad({top_x - beam_w * 0.5f, 0}, {beam_w, sh}, {0,0}, {1,1}, rc);
                // Wider, more transparent base
                batch.draw_quad({top_x - beam_w, sh * 0.3f}, {beam_w * 2, sh * 0.7f}, {0,0}, {1,1},
                    {rc.x, rc.y, rc.z, ray_alpha * 0.3f});
            }
        }

        // Rain particles
        if (w.rain_active) {
            float angle_rad = (w.rain_angle + w.wind_strength * 30.0f) * 3.14159f / 180.0f;
            float sin_a = std::sin(angle_rad);
            for (auto& d : w.rain_drops) {
                float x2 = d.x + sin_a * d.length;
                float y2 = d.y + d.length;
                // Draw rain drop as a thin vertical quad
                float rw = 1.0f;
                eb::Vec4 rc = {w.rain_color.x, w.rain_color.y, w.rain_color.z, d.opacity};
                batch.draw_quad({d.x, d.y}, {rw, d.length}, {0,0}, {1,1}, rc);
            }
        }

        // Snow particles
        if (w.snow_active) {
            for (auto& f : w.snow_flakes) {
                eb::Vec4 sc = {w.snow_color.x, w.snow_color.y, w.snow_color.z, f.opacity * w.snow_color.w};
                batch.draw_quad({f.x, f.y}, {f.size, f.size}, {0,0}, {1,1}, sc);
            }
        }

        // Lightning bolts
        for (auto& bolt : w.bolts) {
            float alpha = bolt.brightness * (bolt.timer / w.lightning_flash_duration);
            if (alpha < 0.01f) continue;
            eb::Vec4 lc = {0.9f, 0.9f, 1.0f, alpha};
            // Draw bolt segments
            float px = bolt.x1, py = bolt.y1;
            for (auto& [sx, sy] : bolt.segments) {
                // Draw each segment as a thin quad
                float dx = sx - px, dy = sy - py;
                float len = std::sqrt(dx * dx + dy * dy);
                batch.draw_quad({std::min(px, sx) - 1, std::min(py, sy)},
                    {std::abs(dx) + 2, std::abs(dy) + 1}, {0,0}, {1,1}, lc);
                // Glow around bolt
                batch.draw_quad({std::min(px, sx) - 4, std::min(py, sy) - 2},
                    {std::abs(dx) + 8, std::abs(dy) + 4}, {0,0}, {1,1},
                    {0.7f, 0.7f, 1.0f, alpha * 0.2f});
                px = sx; py = sy;
            }
        }

        // Fog overlay
        if (w.fog_active && w.fog_density > 0.01f) {
            eb::Vec4 fc = {w.fog_color.x, w.fog_color.y, w.fog_color.z, w.fog_density * w.fog_color.w};
            batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, fc);
        }
    }

    // ── Screen Effects ──
    // Flash overlay
    if (game.flash_a > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1},
                        {game.flash_r, game.flash_g, game.flash_b, game.flash_a});
    }
    // Fade overlay
    if (game.fade_a > 0.01f) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1},
                        {game.fade_r, game.fade_g, game.fade_b, game.fade_a});
    }

    // ── Screen Transitions ──
    if (game.transition.active || game.transition.progress > 0.01f) {
        batch.set_texture(game.white_desc);
        float p = game.transition.progress;
        switch (game.transition.type) {
            case eb::TransitionType::Fade:
                batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, p});
                break;
            case eb::TransitionType::Iris: {
                // Circle wipe: draw 4 quads around a circular hole
                float radius = (1.0f - p) * std::max(sw, sh) * 0.8f;
                float cx = sw * 0.5f, cy = sh * 0.5f;
                // Approximate with rectangle coverage leaving center open
                if (p > 0.01f) {
                    float top = std::max(0.0f, cy - radius);
                    float bot = std::min(sh, cy + radius);
                    float lft = std::max(0.0f, cx - radius);
                    float rgt = std::min(sw, cx + radius);
                    batch.draw_quad({0, 0}, {sw, top}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({0, bot}, {sw, sh - bot}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({0, top}, {lft, bot - top}, {0,0}, {1,1}, {0,0,0,1});
                    batch.draw_quad({rgt, top}, {sw - rgt, bot - top}, {0,0}, {1,1}, {0,0,0,1});
                }
                break;
            }
            case eb::TransitionType::Wipe: {
                float w_size = sw * p;
                float h_size = sh * p;
                if (game.transition.direction == 0) // left
                    batch.draw_quad({0, 0}, {w_size, sh}, {0,0}, {1,1}, {0,0,0,1});
                else if (game.transition.direction == 1) // right
                    batch.draw_quad({sw - w_size, 0}, {w_size, sh}, {0,0}, {1,1}, {0,0,0,1});
                else if (game.transition.direction == 2) // up
                    batch.draw_quad({0, 0}, {sw, h_size}, {0,0}, {1,1}, {0,0,0,1});
                else // down
                    batch.draw_quad({0, sh - h_size}, {sw, h_size}, {0,0}, {1,1}, {0,0,0,1});
                break;
            }
            case eb::TransitionType::Pixelate:
                // Approximate pixelation with a semi-transparent black overlay that increases
                batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, p * 0.9f});
                break;
            default: break;
        }
    }

    // ── Debug Overlay (F1) ──
    if (game.show_debug_overlay) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({sw - 200, 0}, {200, 80}, {0,0}, {1,1}, {0, 0, 0, 0.7f});
        char buf[128];
        std::snprintf(buf, sizeof(buf), "FPS: %.0f", game.debug_fps);
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 8}, {0.2f, 1, 0.2f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "Particles: %d", game.debug_particle_count);
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 28}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "NPCs: %d", (int)game.npcs.size());
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 48}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "Tweens: %d", (int)game.tween_system.tweens.size());
        text.draw_text(batch, game.font_desc, buf, {sw - 190, 68}, {0.8f, 0.8f, 0.8f, 1}, 0.6f);
    }

    // ── Pause Menu (dim overlay only — layout is script-driven) ──
    if (game.paused) {
        batch.set_texture(game.white_desc);
        batch.draw_quad({0, 0}, {sw, sh}, {0,0}, {1,1}, {0, 0, 0, 0.65f});
    }
}
