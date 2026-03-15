# ═══════════════════════════════════════════════
# Twilight Engine Demo — Battle Core
# ═══════════════════════════════════════════════

proc attack_normal():
    let fighter_name = "Hero"
    let base_atk = dean_atk
    if active_fighter == 1:
        fighter_name = "Mage"
        base_atk = sam_atk
    let damage = base_atk + random(-2, 3)
    if damage < 1:
        damage = 1
    let crit = random(1, 10)
    if crit == 10:
        damage = damage * 2
        battle_msg = fighter_name + " lands a CRITICAL HIT! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

proc defend():
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Mage"
    let heal = 10 + random(0, 8)
    if active_fighter == 0:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    else:
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " defends! Recovered " + str(heal) + " HP."
    battle_target = fighter_name

proc enemy_turn():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0
    let target_name = "Hero"
    if target == 1:
        target_name = "Mage"
    let damage = enemy_atk + random(-2, 3)
    if damage < 1:
        damage = 1
    let special = random(1, 5)
    if special == 5:
        damage = damage + random(3, 8)
        battle_msg = enemy_name + " unleashes a powerful attack on " + target_name + "! " + str(damage) + " damage!"
    else:
        battle_msg = enemy_name + " attacks " + target_name + "! " + str(damage) + " damage!"
    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage
    battle_damage = damage
    battle_target = target_name

proc on_victory():
    let xp = enemy_max_hp / 2 + enemy_atk
    battle_msg = "Victory! Gained " + str(xp) + " XP!"
    battle_damage = xp
    set_flag("last_battle_won", true)

proc on_defeat():
    battle_msg = "Your party has fallen!"
    set_flag("last_battle_won", false)
