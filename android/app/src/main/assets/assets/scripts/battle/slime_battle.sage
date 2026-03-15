# ═══════════════════════════════════════════════
# Final Fantasy Demo — Monster AI
# ═══════════════════════════════════════════════
#
# Classic FF enemy patterns:
# - Slime: weak, can split, vulnerable to Fire
# - Skeleton: undead, resistant to physical, weak to Fire/Cure
# - Goblin: balanced fighter (future)
# - Bomb: self-destructs when low HP (future)

# ── Green Slime ──

proc slime_attack():
    let target = random(0, 1)
    if player_hp <= 0:
        target = 1
    if ally_hp <= 0:
        target = 0
    let target_name = "Mage"
    if target == 1:
        target_name = "Black Mage"
    let pattern = random(1, 10)
    if pattern <= 5:
        # Normal attack
        let damage = enemy_atk + random(0, 3)
        if target == 0:
            player_hp = player_hp - damage
        else:
            ally_hp = ally_hp - damage
        battle_msg = "Slime oozes onto " + target_name + "! " + str(damage) + " damage!"
        battle_damage = damage
        battle_target = target_name
    else:
        if pattern <= 7:
            # Acid Spit — hits one target, may reduce defense
            let damage = enemy_atk + random(2, 6)
            if target == 0:
                player_hp = player_hp - damage
            else:
                ally_hp = ally_hp - damage
            battle_msg = "Slime spits acid at " + target_name + "! " + str(damage) + " damage!"
            battle_damage = damage
            battle_target = target_name
        else:
            if pattern <= 9:
                # Divide — heals self
                let heal = random(5, 12)
                enemy_hp = enemy_hp + heal
                if enemy_hp > enemy_max_hp:
                    enemy_hp = enemy_max_hp
                battle_msg = "Slime divides and regenerates " + str(heal) + " HP!"
                battle_damage = 0
                battle_target = "enemy"
            else:
                # Do nothing
                battle_msg = "Slime jiggles menacingly..."
                battle_damage = 0
                battle_target = "enemy"

# ── Skeleton ──

proc skeleton_attack():
    let target = random(0, 1)
    if player_hp <= 0:
        target = 1
    if ally_hp <= 0:
        target = 0
    let target_name = "Mage"
    if target == 1:
        target_name = "Black Mage"
    let pattern = random(1, 10)
    if pattern <= 4:
        # Bone Strike
        let damage = enemy_atk + random(1, 5)
        if target == 0:
            player_hp = player_hp - damage
        else:
            ally_hp = ally_hp - damage
        battle_msg = "Skeleton strikes with a bone! " + str(damage) + " damage to " + target_name + "!"
        battle_damage = damage
        battle_target = target_name
    else:
        if pattern <= 7:
            # Bone Throw — ranged attack, targets Black Mage preferentially
            if ally_hp > 0:
                target = 1
                target_name = "Black Mage"
            let damage = enemy_atk + random(3, 8)
            if target == 0:
                player_hp = player_hp - damage
            else:
                ally_hp = ally_hp - damage
            battle_msg = "Skeleton hurls a bone at " + target_name + "! " + str(damage) + " damage!"
            battle_damage = damage
            battle_target = target_name
        else:
            if pattern <= 9:
                # Shadow Claw — dark damage
                let damage = enemy_atk + random(4, 10)
                if target == 0:
                    player_hp = player_hp - damage
                else:
                    ally_hp = ally_hp - damage
                battle_msg = "Skeleton slashes with shadow claws! " + str(damage) + " dark damage to " + target_name + "!"
                battle_damage = damage
                battle_target = target_name
            else:
                # Rattle — intimidate, does nothing
                battle_msg = "Skeleton rattles its bones ominously..."
                battle_damage = 0
                battle_target = "enemy"
