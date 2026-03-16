#include "game/ui/merchant_ui.h"
#include "game/game.h"
#include "engine/core/debug_log.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace eb {

// ─── UI Atlas Setup ───
// Called once during init to define named regions from the sprite sheet (704x2160)

void define_ui_atlas_regions(TextureAtlas& atlas) {
    // Panels
    atlas.define_region("panel_large",    32, 1120, 183, 280);  // Main store panel
    atlas.define_region("panel_window",  32, 1120, 237, 271);  // Warm window (full width)
    atlas.define_region("panel_scroll",  268, 1133, 159, 251);  // Scroll/list panel
    atlas.define_region("panel_window_lg", 38, 1666, 248, 280); // Large settings window
    atlas.define_region("panel_dialogue", 32,  678, 410,  84);  // Dialogue box
    atlas.define_region("panel_mini",     38,  983, 109,  34);  // Mini dialogue
    atlas.define_region("panel_hud_wide", 32,   32, 157,  65);  // HUD panel (wide)
    atlas.define_region("panel_hud_sq",  240,   32,  81,  80);  // HUD panel (square)
    atlas.define_region("panel_hud_sq2", 368,   32,  81,  80);  // HUD panel (square, tab)
    atlas.define_region("panel_settings",38, 1666, 248, 280);   // Settings panel

    // Wide panels (row 2-3)
    atlas.define_region("panel_wide",     32,  128, 157, 161);  // Tall panel
    atlas.define_region("panel_wide2",   240,  144,  81,  80);  // Square panel (row 2)

    // Buttons (stacked on right side of large panel area)
    atlas.define_region("btn_tiny",      507, 1131,  25,  22);
    atlas.define_region("btn_small",     500, 1195,  39,  22);
    atlas.define_region("btn_medium",    495, 1259,  49,  22);
    atlas.define_region("btn_large",     492, 1323,  55,  22);
    atlas.define_region("btn_xlarge",    491, 1387,  57,  22);

    // Square button + arrows
    atlas.define_region("btn_square",     32,  432,  52,  28);
    atlas.define_region("arrow_left",    116,  420,  22,  42);
    atlas.define_region("arrow_right",   164,  420,  22,  42);

    // Dark panel area (row 3 left side)
    atlas.define_region("panel_dark",     32,  320, 348,  68);

    // Mini buttons bar
    atlas.define_region("btn_bar",        32,  496, 150,  32);

    // Portrait frame
    atlas.define_region("portrait",       36,  855,  65,  82);

    // Cursor/selection box
    atlas.define_region("cursor_box",    500,  227,  24,  27);

    // Selection highlight frames
    atlas.define_region("select_sq1",    552,  232,  16,  16);
    atlas.define_region("select_sq2",    584,  232,  16,  16);

    // Progress bar area
    atlas.define_region("bar_bg",        286, 1549, 166,  36);

    // Media/action buttons
    atlas.define_region("btn_action",    282, 1463, 173,  49);

    // Character portrait (with hat)
    atlas.define_region("char_portrait", 499, 1506,  24,  27);

    // Scrollbar pieces (tiny)
    atlas.define_region("scroll_dot",    580,  340,   7,   7);

    // Item icons from the sprite sheet (row 0: y=144-159, row 1: y=176-191)
    // Row 0: sword, book, potion(blue), shield, hearts(red/orange/gray/black)
    atlas.define_region("icon_sword",    496,  144,  16,  16);
    atlas.define_region("icon_book",     530,  144,  12,  16);
    atlas.define_region("icon_scroll",   562,  144,  12,  16);
    atlas.define_region("icon_shield",   592,  144,  16,  16);
    atlas.define_region("icon_heart_red",624,  144,  16,  16);
    atlas.define_region("icon_heart_orange",656,144,  16,  16);

    // Row 1: potions, gems, food items
    atlas.define_region("icon_potion",   496,  176,  16,  16);
    atlas.define_region("icon_gem_blue", 528,  176,  16,  16);
    atlas.define_region("icon_gem_green",560,  176,  16,  16);
    atlas.define_region("icon_ring",     592,  176,  16,  16);
    atlas.define_region("icon_star",     624,  176,  16,  16);
    atlas.define_region("icon_coin",     657,  176,  15,  16);

    // Checkboxes and arrows (bottom)
    atlas.define_region("arrow_up",       84, 2050,   7,  12);
    atlas.define_region("arrow_sm_right",117, 2052,   5,   8);
    atlas.define_region("checkbox_empty",146, 2049,  11,  12);
    atlas.define_region("checkbox_check",178, 2050,  12,  12);
}

// ─── Icon Atlas Setup ───
// For the separate icons sprite sheet (demo_srw_free_icons1.png = 64x96)

void define_icons_atlas_regions(TextureAtlas& atlas) {
    // 4 columns x 3 rows of 16x16 icons
    // Row 0: sword, book1, book2, potion
    // Row 1: shield, heart_red, heart_orange, heart_gray
    // Row 2: char_face, star, coin, heart_dark
    int icon_size = 16;
    const char* names[] = {
        "sword", "book", "potion_blue", "shield_gold",
        "heart_red", "heart_orange", "heart_gray", "heart_black",
        "face", "star", "coin", "cursor"
    };
    // The actual icon sheet has irregular spacing, define precisely
    // Row 0 (y=0): 4 icons
    atlas.define_region("sword",       0,  0, 16, 16);
    atlas.define_region("book",       20,  0, 12, 16);
    atlas.define_region("potion_blue",36,  0, 12, 16);
    atlas.define_region("shield",     52,  0, 16, 16);
    // Row 1 (y=20): 4 hearts
    atlas.define_region("heart_red",   0, 20, 16, 16);
    atlas.define_region("heart_orange",20,20, 16, 16);
    atlas.define_region("heart_gray", 40, 20, 16, 16);
    atlas.define_region("heart_black",56, 20, 16, 16);
    // Row 2 (y=40): face, star, coin
    atlas.define_region("face",        0, 40, 16, 16);
    atlas.define_region("star",       20, 40, 16, 16);
    atlas.define_region("coin_icon",  40, 40, 16, 16);
}

// ─── MerchantUI ───

void MerchantUI::open(const std::vector<ShopItem>& items, const std::string& merchant_name) {
    shop_items_ = items;
    merchant_name_ = merchant_name;
    open_ = true;
    selected_item_ = 0;
    scroll_offset_ = 0;
    tab_ = 0;
    anim_timer_ = 0.0f;
    message_.clear();
    message_timer_ = 0.0f;
    DLOG_INFO("Shop opened: %s (%d items)", merchant_name.c_str(), (int)items.size());
}

void MerchantUI::close() {
    open_ = false;
    DLOG_INFO("Shop closed");
}

bool MerchantUI::update(GameState& game, bool up, bool down, bool left, bool right,
                         bool confirm, bool cancel, float dt) {
    if (!open_) return false;

    anim_timer_ += dt;
    if (message_timer_ > 0.0f) message_timer_ -= dt;

    // Tab switching with left/right
    if (left && tab_ > 0) { tab_ = 0; selected_item_ = 0; scroll_offset_ = 0; }
    if (right && tab_ < 1) { tab_ = 1; selected_item_ = 0; scroll_offset_ = 0; }

    // Get current item list
    int item_count = 0;
    if (tab_ == 0) {
        item_count = (int)shop_items_.size();
    } else {
        // Sell tab: count player items that have sell value
        for (auto& it : game.inventory.items)
            if (it.sell_price > 0) item_count++;
    }

    // Navigate items
    if (up && selected_item_ > 0) {
        selected_item_--;
        if (selected_item_ < scroll_offset_) scroll_offset_ = selected_item_;
    }
    if (down && selected_item_ < item_count - 1) {
        selected_item_++;
        if (selected_item_ >= scroll_offset_ + VISIBLE_ITEMS)
            scroll_offset_ = selected_item_ - VISIBLE_ITEMS + 1;
    }

    // Confirm = buy/sell
    if (confirm && item_count > 0) {
        if (tab_ == 0) {
            // Buy
            auto& shop = shop_items_[selected_item_];
            if (game.gold >= shop.price) {
                game.gold -= shop.price;
                auto item_type = static_cast<ItemType>(static_cast<int>(shop.type));
                game.inventory.add(shop.id, shop.name, 1, item_type, shop.description,
                                   shop.heal_hp, shop.damage, shop.element, shop.sage_func);
                // Set prices on the inventory item
                if (auto* inv_item = game.inventory.find(shop.id)) {
                    inv_item->buy_price = shop.price;
                    inv_item->sell_price = shop.price / 2;
                }
                message_ = "Bought " + shop.name + "!";
                message_timer_ = 2.0f;
                DLOG_SCRIPT("Bought %s for %d gold", shop.name.c_str(), shop.price);
            } else {
                message_ = "Not enough gold!";
                message_timer_ = 2.0f;
            }
        } else {
            // Sell
            int sell_idx = 0;
            for (auto& it : game.inventory.items) {
                if (it.sell_price > 0) {
                    if (sell_idx == selected_item_) {
                        game.gold += it.sell_price;
                        message_ = "Sold " + it.name + " for " + std::to_string(it.sell_price) + "G!";
                        message_timer_ = 2.0f;
                        DLOG_SCRIPT("Sold %s for %d gold", it.name.c_str(), it.sell_price);
                        game.inventory.remove(it.id, 1);
                        if (selected_item_ >= item_count - 1 && selected_item_ > 0)
                            selected_item_--;
                        break;
                    }
                    sell_idx++;
                }
            }
        }
    }

    // Cancel = close shop
    if (cancel) {
        close();
    }

    return true;  // Always consume input when open
}

// Helper: draw a scaled sprite region
static void draw_region(SpriteBatch& batch, const AtlasRegion& r,
                         float x, float y, float scale) {
    batch.draw_quad({x, y}, {r.pixel_w * scale, r.pixel_h * scale},
                    r.uv_min, r.uv_max);
}

static void draw_region_sized(SpriteBatch& batch, const AtlasRegion& r,
                               float x, float y, float w, float h) {
    batch.draw_quad({x, y}, {w, h}, r.uv_min, r.uv_max);
}

void MerchantUI::render_panel(SpriteBatch& batch, GameState& game,
                               float x, float y, float w, float h) {
    if (!game.ui_atlas) return;
    auto* r = game.ui_atlas->find_region("panel_large");
    if (r) {
        batch.set_texture(game.ui_desc);
        draw_region_sized(batch, *r, x, y, w, h);
    } else {
        // Fallback: solid color panel
        batch.set_texture(game.white_desc);
        batch.draw_quad({x, y}, {w, h}, {0,0}, {1,1}, {0.12f, 0.08f, 0.06f, 0.95f});
        // Border
        batch.draw_quad({x, y}, {w, 3}, {0,0}, {1,1}, {0.35f, 0.2f, 0.15f, 1.0f});
        batch.draw_quad({x, y+h-3}, {w, 3}, {0,0}, {1,1}, {0.35f, 0.2f, 0.15f, 1.0f});
        batch.draw_quad({x, y}, {3, h}, {0,0}, {1,1}, {0.35f, 0.2f, 0.15f, 1.0f});
        batch.draw_quad({x+w-3, y}, {3, h}, {0,0}, {1,1}, {0.35f, 0.2f, 0.15f, 1.0f});
    }
}

void MerchantUI::render_button(SpriteBatch& batch, TextRenderer& text, GameState& game,
                                const std::string& label, float x, float y, float w, float h,
                                bool selected) {
    auto* r = game.ui_atlas ? game.ui_atlas->find_region("btn_xlarge") : nullptr;
    if (r) {
        batch.set_texture(game.ui_desc);
        draw_region_sized(batch, *r, x, y, w, h);
    } else {
        batch.set_texture(game.white_desc);
        Vec4 col = selected ? Vec4{0.6f, 0.35f, 0.2f, 1.0f} : Vec4{0.4f, 0.25f, 0.15f, 0.9f};
        batch.draw_quad({x, y}, {w, h}, {0,0}, {1,1}, col);
    }

    // Selection highlight
    if (selected) {
        batch.set_texture(game.white_desc);
        float pulse = 0.15f + 0.1f * std::sin(anim_timer_ * 4.0f);
        batch.draw_quad({x-2, y-2}, {w+4, h+4}, {0,0}, {1,1},
                       {1.0f, 0.9f, 0.5f, pulse});
    }

    // Text
    float text_scale = 0.85f;
    auto sz = text.measure_text(label, text_scale);
    float tx = x + (w - sz.x) * 0.5f;
    float ty = y + (h - sz.y) * 0.5f;
    Vec4 text_col = selected ? Vec4{1.0f, 1.0f, 0.9f, 1.0f} : Vec4{0.9f, 0.85f, 0.75f, 1.0f};
    text.draw_text(batch, game.font_desc, label, {tx, ty}, text_col, text_scale);
}

void MerchantUI::render(SpriteBatch& batch, TextRenderer& text, GameState& game,
                         float screen_w, float screen_h) {
    if (!open_) return;

    // ── Dimmed background overlay ──
    batch.set_texture(game.white_desc);
    batch.draw_quad({0, 0}, {screen_w, screen_h}, {0,0}, {1,1}, {0, 0, 0, 0.6f});

    // ── Layout — scale to screen size ──
    float ui_scale = std::min(screen_w / 960.0f, screen_h / 720.0f);
    if (ui_scale < 1.0f) ui_scale = 1.0f;

    // Main panel dimensions (centered, scaled)
    float panel_w = 600.0f * ui_scale;
    float panel_h = 520.0f * ui_scale;
    float panel_x = (screen_w - panel_w) * 0.5f;
    float panel_y = (screen_h - panel_h) * 0.5f - 10.0f * ui_scale;

    // ── Main panel background ──
    render_panel(batch, game, panel_x, panel_y, panel_w, panel_h);

    // ── Title bar ──
    float title_y = panel_y + 16.0f * ui_scale;
    float title_scale = 1.1f * ui_scale;
    std::string title = merchant_name_ + "'s Shop";
    auto title_sz = text.measure_text(title, title_scale);
    float title_x = panel_x + (panel_w - title_sz.x) * 0.5f;
    text.draw_text(batch, game.font_desc, title, {title_x, title_y},
                   {1.0f, 0.9f, 0.6f, 1.0f}, title_scale);

    // ── Gold display ──
    float gold_scale = 0.8f * ui_scale;
    std::string gold_str = std::to_string(game.gold) + " G";
    auto gold_sz = text.measure_text(gold_str, gold_scale);
    float gold_x = panel_x + panel_w - gold_sz.x - 24.0f * ui_scale;
    float gold_y = title_y + 2.0f;

    // Gold coin icon from UI sprite sheet
    float icon_scale = 20.0f * ui_scale;
    if (game.ui_atlas) {
        auto* coin = game.ui_atlas->find_region("icon_coin");
        if (coin) {
            batch.set_texture(game.ui_desc);
            draw_region_sized(batch, *coin, gold_x - icon_scale - 4.0f, gold_y - 2.0f, icon_scale, icon_scale);
        }
    }
    text.draw_text(batch, game.font_desc, gold_str, {gold_x, gold_y},
                   {1.0f, 0.95f, 0.3f, 1.0f}, gold_scale);

    // ── Tab buttons (Buy / Sell) ──
    float tab_y = title_y + title_sz.y + 16.0f * ui_scale;
    float tab_w = 100.0f * ui_scale;
    float tab_h = 34.0f * ui_scale;
    float tab_x = panel_x + 24.0f * ui_scale;

    render_button(batch, text, game, "BUY", tab_x, tab_y, tab_w, tab_h, tab_ == 0);
    render_button(batch, text, game, "SELL", tab_x + tab_w + 10.0f * ui_scale, tab_y, tab_w, tab_h, tab_ == 1);

    // ── Separator line ──
    float sep_y = tab_y + tab_h + 10.0f * ui_scale;
    batch.set_texture(game.white_desc);
    batch.draw_quad({panel_x + 20.0f * ui_scale, sep_y}, {panel_w - 40.0f * ui_scale, 2.0f * ui_scale},
                    {0,0}, {1,1}, {0.5f, 0.35f, 0.25f, 0.8f});

    // ── Item list area ──
    float list_y = sep_y + 10.0f * ui_scale;
    float list_x = panel_x + 24.0f * ui_scale;
    float list_w = panel_w - 48.0f * ui_scale;
    float item_h = 48.0f * ui_scale;

    // Inner scroll panel background
    if (game.ui_atlas) {
        auto* scroll_r = game.ui_atlas->find_region("panel_scroll");
        if (scroll_r) {
            batch.set_texture(game.ui_desc);
            draw_region_sized(batch, *scroll_r, list_x - 4.0f, list_y - 4.0f,
                            list_w + 8.0f, VISIBLE_ITEMS * item_h + 8.0f);
        }
    }

    // Build display list
    struct DisplayItem {
        std::string name;
        std::string desc;
        int price = 0;
        int quantity = 0;
        std::string icon_name;
    };
    std::vector<DisplayItem> display;

    if (tab_ == 0) {
        // Buy tab: show shop items
        for (auto& si : shop_items_) {
            DisplayItem di;
            di.name = si.name;
            di.desc = si.description;
            di.price = si.price;
            di.quantity = -1;  // Not relevant for buying
            // Pick icon based on item type/element
            if (si.damage > 0) di.icon_name = "icon_sword";
            else if (si.heal_hp > 0) di.icon_name = "icon_potion";
            else if (si.element == "fire") di.icon_name = "icon_heart_red";
            else if (si.element == "holy") di.icon_name = "icon_star";
            else di.icon_name = "icon_gem_blue";
            display.push_back(di);
        }
    } else {
        // Sell tab: show player inventory items with sell value
        for (auto& it : game.inventory.items) {
            if (it.sell_price <= 0) continue;
            DisplayItem di;
            di.name = it.name;
            di.desc = it.description;
            di.price = it.sell_price;
            di.quantity = it.quantity;
            if (it.damage > 0) di.icon_name = "icon_sword";
            else if (it.heal_hp > 0) di.icon_name = "icon_potion";
            else if (it.element == "fire") di.icon_name = "icon_heart_red";
            else if (it.element == "holy") di.icon_name = "icon_star";
            else di.icon_name = "icon_gem_blue";
            display.push_back(di);
        }
    }

    // Render visible items
    for (int i = 0; i < VISIBLE_ITEMS && i + scroll_offset_ < (int)display.size(); i++) {
        int idx = i + scroll_offset_;
        auto& di = display[idx];
        bool selected = (idx == selected_item_);
        float iy = list_y + i * item_h;

        // Selection highlight — solid background + bright border
        if (selected) {
            batch.set_texture(game.white_desc);
            // Solid highlight background
            batch.draw_quad({list_x, iy}, {list_w, item_h - 2.0f * ui_scale},
                           {0,0}, {1,1}, {1.0f, 0.85f, 0.3f, 0.25f});
            // Bright border (top + bottom)
            float bdr = 2.0f * ui_scale;
            batch.draw_quad({list_x, iy}, {list_w, bdr},
                           {0,0}, {1,1}, {1.0f, 0.9f, 0.3f, 0.9f});
            batch.draw_quad({list_x, iy + item_h - 2.0f * ui_scale - bdr}, {list_w, bdr},
                           {0,0}, {1,1}, {1.0f, 0.9f, 0.3f, 0.9f});
            // Left edge accent
            batch.draw_quad({list_x, iy}, {4.0f * ui_scale, item_h - 2.0f * ui_scale},
                           {0,0}, {1,1}, {1.0f, 0.8f, 0.2f, 1.0f});

            // Selection cursor arrow from sprite sheet
            if (game.ui_atlas) {
                auto* cursor = game.ui_atlas->find_region("cursor_box");
                if (cursor) {
                    batch.set_texture(game.ui_desc);
                    float csz = 24.0f * ui_scale;
                    draw_region_sized(batch, *cursor, list_x - csz - 4.0f * ui_scale,
                                    iy + (item_h - csz) * 0.5f, csz, csz);
                }
            }
        }

        // Item icon
        float icon_x = list_x + 8.0f * ui_scale;
        float icon_y = iy + 8.0f * ui_scale;
        float icon_sz = 30.0f * ui_scale;
        if (game.ui_atlas && !di.icon_name.empty()) {
            auto* icon_r = game.ui_atlas->find_region(di.icon_name);
            if (icon_r) {
                batch.set_texture(game.ui_desc);
                draw_region_sized(batch, *icon_r, icon_x, icon_y, icon_sz, icon_sz);
            }
        }

        // Item name
        float name_x = icon_x + icon_sz + 10.0f * ui_scale;
        float name_scale = 0.8f * ui_scale;
        Vec4 name_col = selected ? Vec4{1.0f, 1.0f, 0.9f, 1.0f} : Vec4{0.85f, 0.8f, 0.7f, 1.0f};
        text.draw_text(batch, game.font_desc, di.name, {name_x, iy + 6.0f * ui_scale},
                       name_col, name_scale);

        // Quantity (for sell tab)
        if (di.quantity > 0) {
            std::string qty = "x" + std::to_string(di.quantity);
            text.draw_text(batch, game.font_desc, qty, {name_x, iy + 24.0f * ui_scale},
                           {0.7f, 0.7f, 0.65f, 0.9f}, 0.55f * ui_scale);
        }

        // Price (right-aligned)
        std::string price_str = std::to_string(di.price) + "G";
        float price_scale = 0.75f * ui_scale;
        auto price_sz = text.measure_text(price_str, price_scale);
        float price_x = list_x + list_w - price_sz.x - 10.0f * ui_scale;
        bool can_afford = (tab_ == 0) ? (game.gold >= di.price) : true;
        Vec4 price_col = can_afford ? Vec4{1.0f, 0.95f, 0.3f, 1.0f}
                                     : Vec4{0.8f, 0.3f, 0.3f, 1.0f};
        text.draw_text(batch, game.font_desc, price_str, {price_x, iy + 12.0f * ui_scale},
                       price_col, price_scale);
    }

    // ── Scroll indicators ──
    float arrow_sz = 14.0f * ui_scale;
    if (scroll_offset_ > 0) {
        if (game.ui_atlas) {
            auto* arr = game.ui_atlas->find_region("arrow_up");
            if (arr) {
                batch.set_texture(game.ui_desc);
                draw_region_sized(batch, *arr, panel_x + panel_w - 36.0f * ui_scale,
                                list_y - 4.0f * ui_scale, arrow_sz, arrow_sz);
            }
        }
    }
    if (scroll_offset_ + VISIBLE_ITEMS < (int)display.size()) {
        batch.set_texture(game.white_desc);
        float ax = panel_x + panel_w - 34.0f * ui_scale;
        float ay = list_y + VISIBLE_ITEMS * item_h - arrow_sz;
        batch.draw_quad({ax, ay}, {arrow_sz, arrow_sz}, {0,0}, {1,1}, {0.9f, 0.8f, 0.5f, 0.8f});
    }

    // ── Item description area ──
    float desc_y = list_y + VISIBLE_ITEMS * item_h + 14.0f * ui_scale;
    float desc_h = 64.0f * ui_scale;

    // Description panel background
    if (game.ui_atlas) {
        auto* desc_r = game.ui_atlas->find_region("panel_dialogue");
        if (desc_r) {
            batch.set_texture(game.ui_desc);
            draw_region_sized(batch, *desc_r, list_x - 4.0f, desc_y, list_w + 8.0f, desc_h);
        }
    }

    // Description text
    if (selected_item_ >= 0 && selected_item_ < (int)display.size()) {
        auto& di = display[selected_item_];
        std::string desc = di.desc;
        if (desc.empty()) desc = di.name;

        if (tab_ == 0 && selected_item_ < (int)shop_items_.size()) {
            auto& si = shop_items_[selected_item_];
            if (si.heal_hp > 0) desc += "  [Heal: " + std::to_string(si.heal_hp) + " HP]";
            if (si.damage > 0) desc += "  [DMG: " + std::to_string(si.damage) + "]";
            if (!si.element.empty()) desc += "  (" + si.element + ")";
        }

        text.draw_text_wrapped(batch, game.font_desc, desc,
                               {list_x + 10.0f * ui_scale, desc_y + 10.0f * ui_scale},
                               list_w - 20.0f * ui_scale,
                               {0.9f, 0.9f, 0.85f, 1.0f}, 0.65f * ui_scale);
    }

    // ── Status message (bought/sold feedback) ──
    if (message_timer_ > 0.0f && !message_.empty()) {
        float msg_scale = 0.7f * ui_scale;
        auto msg_sz = text.measure_text(message_, msg_scale);
        float msg_x = panel_x + (panel_w - msg_sz.x) * 0.5f;
        float msg_y = panel_y + panel_h - 28.0f * ui_scale;
        float alpha = std::min(1.0f, message_timer_);
        text.draw_text(batch, game.font_desc, message_, {msg_x, msg_y},
                       {0.3f, 1.0f, 0.3f, alpha}, msg_scale);
    }

    // ── Controls hint ──
    float hint_y = panel_y + panel_h + 8.0f;
    float hint_scale = 0.55f * ui_scale;
    std::string hint = "[Up/Down] Select   [Z] ";
    hint += (tab_ == 0) ? "Buy" : "Sell";
    hint += "   [Left/Right] Tab   [X] Close";
    auto hint_sz = text.measure_text(hint, hint_scale);
    float hint_x = panel_x + (panel_w - hint_sz.x) * 0.5f;
    text.draw_text(batch, game.font_desc, hint, {hint_x, hint_y},
                   {0.6f, 0.6f, 0.55f, 0.8f}, hint_scale);
}

} // namespace eb
