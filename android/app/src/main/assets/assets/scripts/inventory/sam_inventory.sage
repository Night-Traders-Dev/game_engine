# ═══════════════════════════════════════════════
# Sam's Inventory — Sam-only items
# ═══════════════════════════════════════════════
#
# Items that only Sam can find or use.

# ── Sam-only item grants ──

proc find_rubys_knife():
    add_item("rubys_knife", "Ruby's Knife", 1, "weapon", "Kills demons on contact", 0, 55, "demon", "use_rubys_knife")
    log("Found Ruby's Knife!")

proc sam_find_laptop():
    add_item("sams_laptop", "Sam's Laptop", 1, "key", "Research tool for lore", 0, 0, "", "")
    set_flag("has_laptop", true)
    log("Found Sam's Laptop")

# ── Sam-only item usage ──

proc use_rubys_knife():
    let fighter_name = "Sam"
    let damage = 55 + random(0, 18)
    if enemy_name == "Demon":
        damage = damage * 2
        battle_msg = fighter_name + " stabs with Ruby's Knife! Lethal to demons! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " slashes with Ruby's Knife! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # Ruby's knife doesn't get consumed
