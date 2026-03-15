# ═══════════════════════════════════════════════
# Demo — Inventory Core
# ═══════════════════════════════════════════════

proc give_starter_items():
    add_item("potion", "Potion", 5, "consumable", "Heals 30 HP", 30, 0, "", "use_potion")
    add_item("bread", "Bread", 3, "consumable", "Heals 15 HP", 15, 0, "", "use_bread")
    add_item("fire_scroll", "Fire Scroll", 2, "weapon", "Deals fire damage", 0, 25, "fire", "use_fire_scroll")
    log("Demo starter items given")

proc use_potion():
    let heal = 30 + random(0, 10)
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Mage"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " drinks a Potion! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("potion", 1)

proc use_bread():
    let heal = 15 + random(0, 5)
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Mage"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " eats Bread. Healed " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("bread", 1)

proc use_fire_scroll():
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Mage"
    let damage = 25 + random(0, 10)
    if enemy_name == "Slime":
        damage = damage * 2
        battle_msg = fighter_name + " casts Fire! Super effective! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " casts Fire! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("fire_scroll", 1)
