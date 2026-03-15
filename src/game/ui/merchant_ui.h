#pragma once

#include "engine/core/types.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/text_renderer.h"
#include "engine/graphics/texture_atlas.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct GameState;
struct Item;

namespace eb {

// Item type enum (mirrors ItemType in game.h)
enum class ShopItemType { Consumable = 0, Weapon = 1, KeyItem = 2 };

// A shop item available for purchase
struct ShopItem {
    std::string id;
    std::string name;
    std::string description;
    int price = 0;
    int icon_region = -1;  // Index into icons atlas (-1 = no icon)
    // Item stats (used when buying)
    ShopItemType type = ShopItemType::Consumable;
    int heal_hp = 0;
    int damage = 0;
    std::string element;
    std::string sage_func;
};

// UI sprite sheet region IDs (indices into ui_atlas)
namespace UIRegion {
    enum : int {
        // Panels
        PanelLarge = 0,     // 183x280 main store panel
        PanelScroll,        // 159x251 item list scroll area
        PanelDialogue,      // 410x84 dialogue/description box
        PanelMini,          // 109x34 mini dialogue
        PanelHUD_Wide,      // 157x65 HUD panel (wide)
        PanelHUD_Square,    // 81x80 HUD panel (square)
        // Buttons (different sizes)
        BtnTiny,            // 25x22
        BtnSmall,           // 39x22
        BtnMedium,          // 49x22
        BtnLarge,           // 55x22
        BtnXLarge,          // 57x22
        // Small square buttons
        BtnSquare,          // 52x28
        // Arrows
        ArrowLeft,          // 22x42
        ArrowRight,         // 22x42
        // Portrait frame
        Portrait,           // 65x82
        // Cursor/selection
        CursorBox,          // 24x27
        // Scrollbar track pieces (vertical line decorators)
        ScrollDot,          // 7x7
        // Progress/HP bar background
        BarBackground,      // 166x36
        Count
    };
}

class MerchantUI {
public:
    MerchantUI() = default;

    // Open the shop with a list of items for sale
    void open(const std::vector<ShopItem>& items, const std::string& merchant_name = "Merchant");
    void close();
    bool is_open() const { return open_; }

    // Input: returns true if UI consumed the input
    bool update(GameState& game, bool up, bool down, bool left, bool right,
                bool confirm, bool cancel, float dt);

    // Render the shop UI
    void render(SpriteBatch& batch, TextRenderer& text, GameState& game,
                float screen_w, float screen_h);

private:
    void render_panel(SpriteBatch& batch, GameState& game,
                      float x, float y, float w, float h);
    void render_button(SpriteBatch& batch, TextRenderer& text, GameState& game,
                       const std::string& label, float x, float y, float w, float h,
                       bool selected);

    bool open_ = false;
    std::string merchant_name_;
    std::vector<ShopItem> shop_items_;

    int selected_item_ = 0;
    int scroll_offset_ = 0;
    int tab_ = 0;           // 0 = Buy, 1 = Sell
    int tab_selection_ = 0;  // Which tab button is highlighted

    float anim_timer_ = 0.0f;
    std::string message_;
    float message_timer_ = 0.0f;

    static constexpr int VISIBLE_ITEMS = 6;
    static constexpr float PANEL_SCALE = 3.0f;  // Scale up pixel art
};

// Atlas region setup (called during game init)
void define_ui_atlas_regions(TextureAtlas& atlas);
void define_icons_atlas_regions(TextureAtlas& atlas);

} // namespace eb
