# ═══════════════════════════════════════════════
# Inventory Core — Starter items and key items
# ═══════════════════════════════════════════════
#
# Items shared across the game: starter gear,
# key/story items, and NPC supply drops.

# ── Starter loadout (given at game start) ──

proc give_starter_items():
    add_item("first_aid_kit", "First Aid Kit", 3, "consumable", "Heals 30 HP", 30, 0, "", "use_first_aid")
    add_item("salt_round", "Salt Rounds", 5, "weapon", "Rock salt shells", 0, 20, "salt", "use_salt_round")
    log("Starter items given")

# ── Bobby's supply drop ──

proc bobby_supplies():
    add_item("shotgun_shells", "Shotgun Shells", 8, "weapon", "Standard buckshot", 0, 28, "", "use_shotgun_shells")
    add_item("holy_water", "Holy Water", 4, "weapon", "Burns demons and vampires", 0, 35, "holy", "use_holy_water")
    add_item("silver_bullet", "Silver Bullets", 3, "weapon", "For werewolves and shapeshifters", 0, 40, "silver", "use_silver_bullet")
    add_item("first_aid_kit", "First Aid Kit", 2, "consumable", "Heals 30 HP", 30, 0, "", "use_first_aid")
    set_flag("has_shotgun", true)
    set_flag("has_holy_water", true)
    log("Bobby gave supplies")

# ── Key items (non-consumable, story progression) ──

proc find_emf_meter():
    add_item("emf_meter", "EMF Meter", 1, "key", "Detects supernatural activity", 0, 0, "", "")
    set_flag("has_emf", true)
    log("Found EMF Meter")

proc find_journal():
    add_item("johns_journal", "John's Journal", 1, "key", "Dad's hunting journal", 0, 0, "", "")
    set_flag("has_journal", true)
    log("Found John's Journal")
