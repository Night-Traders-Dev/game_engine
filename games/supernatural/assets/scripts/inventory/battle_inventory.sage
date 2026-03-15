# ═══════════════════════════════════════════════
# Battle Inventory — Weapons usable only in combat
# ═══════════════════════════════════════════════
#
# Ammunition and throwable weapons consumed during battle.
# Elemental damage with weakness multipliers.

# ── Salt Rounds (vs Spirits/Ghosts) ──

proc use_salt_round():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 20 + random(0, 8) + skill_riflery * 2
    if enemy_name == "Spirit" or enemy_name == "Ghost":
        damage = damage * 2
        battle_msg = fighter_name + " fires salt rounds! Super effective! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " fires salt rounds! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("salt_round", 1)

# ── Shotgun Shells ──

proc use_shotgun_shells():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 28 + random(0, 10) + skill_riflery * 2
    battle_msg = fighter_name + " blasts with the shotgun! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("shotgun_shells", 1)

# ── Holy Water (vs Vampires/Demons) ──

proc use_holy_water():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 35 + random(0, 15) + skill_exorcism * 2
    if enemy_name == "Vampire" or enemy_name == "Demon":
        damage = damage * 2
        battle_msg = fighter_name + " throws Holy Water! It burns! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " throws Holy Water! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("holy_water", 1)

# ── Silver Bullets (vs Werewolves/Shapeshifters) ──

proc use_silver_bullet():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 40 + random(0, 12) + skill_riflery * 2
    if enemy_name == "Werewolf" or enemy_name == "Shapeshifter":
        damage = damage * 2
        battle_msg = fighter_name + " fires a silver bullet! Critical weakness! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " fires a silver bullet! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("silver_bullet", 1)
