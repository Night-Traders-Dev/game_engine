# Item Shop — Merchant Store
# Dynamic inventory that changes based on visit count and random stock.
#
# merchant_shop_items() is auto-called when player talks to "Merchant" NPC.
# Each visit increments a counter, and the shop stocks rotate accordingly.

proc merchant_shop_items():
    # Track how many times the player has visited
    let visits = get_flag("merchant_visits")
    visits = visits + 1
    set_flag("merchant_visits", visits)

    # Roll daily stock variation (changes each visit)
    let stock_roll = random(1, 6)

    # ── Always in stock ──
    add_shop_item("potion", "Potion", 25, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_shop_item("antidote", "Antidote", 15, "consumable", "Cures poison status", 0, 0, "", "use_antidote")

    # ── Rotating consumables (based on stock roll) ──
    if stock_roll <= 3:
        add_shop_item("hi_potion", "Hi-Potion", 80, "consumable", "Restores 120 HP", 120, 0, "", "use_hi_potion")
    if stock_roll >= 3:
        add_shop_item("ether", "Ether", 60, "consumable", "Restores spirit energy", 20, 0, "", "use_ether")
    if stock_roll == 1 or stock_roll == 6:
        # Rare stock
        add_shop_item("phoenix_down", "Phoenix Down", 150, "consumable", "Revives fallen ally with 50% HP", 0, 0, "", "use_phoenix_down")
        add_shop_item("elixir", "Elixir", 500, "consumable", "Fully restores HP", 999, 0, "", "use_elixir")

    # ── Magic scrolls (rotate selection) ──
    let magic_roll = random(1, 4)
    if magic_roll == 1:
        add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
        add_shop_item("cure", "Cure Scroll", 50, "consumable", "White magic - heal 40 HP", 40, 0, "holy", "use_cure")
    if magic_roll == 2:
        add_shop_item("blizzard", "Blizzard Scroll", 60, "weapon", "Ice magic - 30 dmg", 0, 30, "ice", "use_blizzard")
        add_shop_item("cure", "Cure Scroll", 50, "consumable", "White magic - heal 40 HP", 40, 0, "holy", "use_cure")
    if magic_roll == 3:
        add_shop_item("thunder", "Thunder Scroll", 60, "weapon", "Lightning - 30 dmg, may stun", 0, 30, "lightning", "use_thunder")
        add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
    if magic_roll == 4:
        add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
        add_shop_item("blizzard", "Blizzard Scroll", 60, "weapon", "Ice magic - 30 dmg", 0, 30, "ice", "use_blizzard")
        add_shop_item("thunder", "Thunder Scroll", 60, "weapon", "Lightning - 30 dmg, may stun", 0, 30, "lightning", "use_thunder")

    # ── Equipment (unlocks after repeated visits) ──
    if visits >= 2:
        add_shop_item("iron_sword", "Iron Sword", 200, "weapon", "A sturdy iron blade - 15 ATK", 0, 15, "", "")
    if visits >= 3:
        add_shop_item("leather_armor", "Leather Armor", 180, "weapon", "Basic protection - 8 DEF", 0, 0, "", "")
    if visits >= 4:
        add_shop_item("steel_sword", "Steel Sword", 450, "weapon", "A keen steel blade - 25 ATK", 0, 25, "", "")
    if visits >= 5:
        add_shop_item("chain_mail", "Chain Mail", 400, "weapon", "Linked steel rings - 15 DEF", 0, 0, "", "")
        add_shop_item("silver_blade", "Silver Blade", 800, "weapon", "Blessed silver - 35 ATK, holy", 0, 35, "holy", "")

    # ── Utility items (random availability) ──
    if random(1, 3) == 1:
        add_shop_item("smoke_bomb", "Smoke Bomb", 40, "consumable", "Guarantees escape from battle", 0, 0, "", "use_smoke_bomb")
    if random(1, 4) == 1:
        add_shop_item("tent", "Tent", 120, "consumable", "Full rest - restores all HP", 999, 0, "", "use_tent")

    # ── Sale event (every 5th visit) ──
    let sale_mod = visits / 5
    let sale_check = visits - sale_mod * 5
    if sale_check == 0:
        say("Merchant", "Special sale today! Everything at a discount!")
        add_shop_item("hi_potion", "Hi-Potion (SALE)", 50, "consumable", "Restores 120 HP - SALE PRICE", 120, 0, "", "use_hi_potion")
        add_shop_item("phoenix_down", "Phoenix Down (SALE)", 100, "consumable", "Revives fallen ally - SALE PRICE", 0, 0, "", "use_phoenix_down")

    open_shop("Merchant")

# Greeting fallback (if shop doesn't trigger)
proc merchant_greeting():
    say("Merchant", "Welcome! Talk to me again to browse my wares.")
