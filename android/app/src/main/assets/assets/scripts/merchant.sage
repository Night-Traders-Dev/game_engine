# Item Shop — Merchant Store
# Uses the shop API: add_shop_item() + open_shop()
#
# merchant_shop_items() is auto-called when player talks to "Merchant" NPC
# It builds the shop inventory and opens the store UI

proc merchant_shop_items():
    # add_shop_item(id, name, price, type, description, heal, dmg, element, sage_func)
    add_shop_item("potion", "Potion", 25, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_shop_item("hi_potion", "Hi-Potion", 80, "consumable", "Restores 120 HP", 120, 0, "", "use_hi_potion")
    add_shop_item("phoenix_down", "Phoenix Down", 150, "consumable", "Revives fallen ally with 50% HP", 0, 0, "", "use_phoenix_down")
    add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
    add_shop_item("ice", "Ice Scroll", 60, "weapon", "Ice magic - 28 dmg, may slow", 0, 28, "ice", "use_ice")
    add_shop_item("cure", "Cure Scroll", 50, "consumable", "White magic - heal 40 HP", 40, 0, "holy", "use_cure")
    add_shop_item("iron_sword", "Iron Sword", 200, "weapon", "A sturdy iron blade - 15 ATK", 0, 15, "", "")
    add_shop_item("leather_armor", "Leather Armor", 180, "weapon", "Basic protection - 8 DEF", 0, 0, "", "")
    add_shop_item("antidote", "Antidote", 15, "consumable", "Cures poison status", 0, 0, "", "use_antidote")
    add_shop_item("smoke_bomb", "Smoke Bomb", 40, "consumable", "Guarantees escape from battle", 0, 0, "", "use_smoke_bomb")
    open_shop("Merchant")

# Greeting fallback (if shop doesn't trigger)
proc merchant_greeting():
    say("Merchant", "Welcome! Talk to me again to browse my wares.")
