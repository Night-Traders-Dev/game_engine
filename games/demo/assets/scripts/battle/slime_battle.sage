# ═══════════════════════════════════════════════
# Demo — Slime Enemy AI
# ═══════════════════════════════════════════════

proc slime_attack():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0
    let target_name = "Hero"
    if target == 1:
        target_name = "Mage"
    let attack_type = random(1, 3)
    if attack_type == 1:
        let damage = enemy_atk + random(0, 3)
        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage
        battle_msg = "Slime bounces on " + target_name + "! " + str(damage) + " damage!"
        battle_damage = damage
        battle_target = target_name
    else:
        if attack_type == 2:
            let damage = enemy_atk / 2 + random(0, 2)
            dean_hp = dean_hp - damage
            sam_hp = sam_hp - damage
            battle_msg = "Slime splits and attacks both! " + str(damage) + " damage each!"
            battle_damage = damage
            battle_target = "Hero"
        else:
            let heal = random(3, 8)
            enemy_hp = enemy_hp + heal
            if enemy_hp > enemy_max_hp:
                enemy_hp = enemy_max_hp
            battle_msg = "Slime regenerates " + str(heal) + " HP!"
            battle_damage = 0
            battle_target = "enemy"
