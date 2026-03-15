# ═══════════════════════════════════════════════
# Dean's Inventory — Dean-only items
# ═══════════════════════════════════════════════
#
# Items that only Dean can find or use.

# ── Dean-only item grants ──

proc find_the_colt():
    add_item("the_colt", "The Colt", 1, "weapon", "Can kill anything", 0, 99, "divine", "use_the_colt")
    set_flag("has_colt", true)
    log("Found The Colt!")

proc dean_find_amulet():
    add_item("deans_amulet", "Dean's Amulet", 1, "key", "Burns hot near God", 0, 0, "", "")
    set_flag("has_amulet", true)
    log("Found Dean's Amulet")

# ── Dean-only item usage ──

proc use_the_colt():
    let damage = 99 + random(0, 50)
    battle_msg = "Dean fires The Colt! " + str(damage) + " massive damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # The Colt doesn't get consumed
