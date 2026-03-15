# ═══════════════════════════════════════════════
# Vampire — Enemy Battle AI
# ═══════════════════════════════════════════════
#
# Vampires drain HP, get stronger at night,
# and are weak to holy water and dead man's blood.

proc vampire_attack():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    # Choose attack type
    let attack_type = random(1, 4)

    if attack_type <= 2:
        # Bite attack — drains HP
        let damage = enemy_atk + random(0, 5)
        let drain = damage / 3

        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage

        enemy_hp = enemy_hp + drain
        if enemy_hp > enemy_max_hp:
            enemy_hp = enemy_max_hp

        battle_msg = enemy_name + " bites " + target_name + "! " + str(damage) + " damage! Drains " + str(drain) + " HP!"
        battle_damage = damage
        battle_target = target_name

    else:
        if attack_type == 3:
            # Claw swipe — raw damage
            let damage = enemy_atk + random(3, 8)
            if target == 0:
                dean_hp = dean_hp - damage
            else:
                sam_hp = sam_hp - damage
            battle_msg = enemy_name + " slashes at " + target_name + "! " + str(damage) + " damage!"
            battle_damage = damage
            battle_target = target_name
        else:
            # Intimidate — reduces both fighters' next attack
            battle_msg = enemy_name + " hisses menacingly! The Winchesters flinch!"
            battle_damage = 0
            battle_target = target_name

proc vampire_frenzy():
    # When HP is low, vampire goes berserk
    if enemy_hp <= enemy_max_hp / 3:
        let damage = enemy_atk * 2 + random(0, 8)
        let target = random(0, 1)
        if dean_hp <= 0:
            target = 1
        if sam_hp <= 0:
            target = 0
        let target_name = "Dean"
        if target == 1:
            target_name = "Sam"
        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage
        let drain = damage / 2
        enemy_hp = enemy_hp + drain
        if enemy_hp > enemy_max_hp:
            enemy_hp = enemy_max_hp
        battle_msg = enemy_name + " enters a blood frenzy! " + str(damage) + " damage! Drains " + str(drain) + " HP!"
        battle_damage = damage
        battle_target = target_name
    else:
        vampire_attack()
