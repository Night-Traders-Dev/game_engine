# ═══════════════════════════════════════════════
# Brothers' Inventory — Shared Dean + Sam items
# ═══════════════════════════════════════════════
#
# Items that both Sam and Dean can use.
# Consumables that either brother can grab.

# ── Shared consumable grants ──

proc restock_food():
    add_item("burger", "Burger", 2, "consumable", "Dean's favorite. Heals 50 HP", 50, 0, "", "use_burger")
    add_item("beer", "Beer", 3, "consumable", "Takes the edge off. Heals 15 HP", 15, 0, "", "use_beer")
    log("Restocked food and drinks")

proc find_pie():
    add_item("pie", "Pie", 1, "consumable", "Dean LOVES pie. Heals 80 HP", 80, 0, "", "use_pie")
    log("Found Pie!")

# ── Shared item grants ──

proc find_angel_blade():
    add_item("angel_blade", "Angel Blade", 1, "weapon", "Can kill angels and demons", 0, 60, "holy", "use_angel_blade")
    log("Found Angel Blade!")

# ── Shared consumable usage ──

proc use_burger():
    let heal = 50 + random(0, 15)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " eats a Burger! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("burger", 1)

proc use_beer():
    let heal = 15 + random(0, 5)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " drinks a Beer. Healed " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("beer", 1)

proc use_pie():
    let heal = 80 + random(0, 20)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " eats Pie! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("pie", 1)

proc use_first_aid():
    let heal = 30 + random(0, 10)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " uses First Aid Kit! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("first_aid_kit", 1)

proc use_angel_blade():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 60 + random(0, 20)
    battle_msg = fighter_name + " strikes with the Angel Blade! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # Angel blade doesn't get consumed
