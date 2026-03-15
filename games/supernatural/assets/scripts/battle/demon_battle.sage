# ═══════════════════════════════════════════════
# Demon — Enemy Battle AI
# ═══════════════════════════════════════════════
#
# Demons use telekinesis, possession threats,
# and hellfire. Weak to holy water and exorcism.

proc demon_attack():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    let attack_type = random(1, 4)

    if attack_type <= 2:
        # Telekinesis slam
        let damage = enemy_atk + random(2, 8)
        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage
        battle_msg = enemy_name + " hurls " + target_name + " across the room! " + str(damage) + " damage!"
        battle_damage = damage
        battle_target = target_name
    else:
        if attack_type == 3:
            # Hellfire burst — hits both
            let damage = enemy_atk / 2 + random(3, 7)
            dean_hp = dean_hp - damage
            sam_hp = sam_hp - damage
            battle_msg = enemy_name + " unleashes hellfire! Both take " + str(damage) + " damage!"
            battle_damage = damage
            battle_target = "Dean"
        else:
            # Dark whisper — psychic damage
            let damage = enemy_atk + random(0, 5)
            if target == 0:
                dean_hp = dean_hp - damage
            else:
                sam_hp = sam_hp - damage
            battle_msg = enemy_name + " whispers dark truths to " + target_name + "! " + str(damage) + " psychic damage!"
            battle_damage = damage
            battle_target = target_name

proc azazel_attack():
    # Yellow-Eyes special: more powerful, psychic focus
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    let attack_type = random(1, 3)

    if attack_type == 1:
        # Ceiling pin — massive single target
        let damage = enemy_atk + random(10, 20)
        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage
        battle_msg = "Azazel pins " + target_name + " to the ceiling! " + str(damage) + " damage!"
        battle_damage = damage
        battle_target = target_name
    else:
        if attack_type == 2:
            # Psychic assault on Sam specifically
            let damage = enemy_atk + random(5, 12)
            sam_hp = sam_hp - damage
            battle_msg = "Azazel reaches into Sam's mind! " + str(damage) + " psychic damage!"
            battle_damage = damage
            battle_target = "Sam"
        else:
            # Telekinetic barrage — hits both
            let damage = enemy_atk / 2 + random(5, 10)
            dean_hp = dean_hp - damage
            sam_hp = sam_hp - damage
            battle_msg = "Azazel waves his hand. Everything flies! Both take " + str(damage) + " damage!"
            battle_damage = damage
            battle_target = "Dean"
