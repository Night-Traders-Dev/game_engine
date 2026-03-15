# ═══════════════════════════════════════════════
# Dean Winchester — Battle Actions
# ═══════════════════════════════════════════════
#
# Dean's combat style: aggressive, gun-focused.
# High Riflery and Nerve make him a crit-heavy fighter.

# ── Dean's signature attack: Brawler punch ──

proc dean_brawl():
    let damage = dean_atk + random(2, 6) + skill_nerve
    let crit_roll = random(1, 100)
    if crit_roll <= skill_nerve * 3:
        damage = damage * 2
        battle_msg = "Dean throws a devastating haymaker! CRIT! " + str(damage) + " damage!"
    else:
        battle_msg = "Dean swings hard! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

# ── Dean's taunt: lower enemy atk ──

proc dean_taunt():
    let reduction = 2 + skill_nerve / 2
    enemy_atk = enemy_atk - reduction
    if enemy_atk < 1:
        enemy_atk = 1
    battle_msg = "Dean taunts the enemy! Come get some! Enemy ATK -" + str(reduction)
    battle_damage = 0
    battle_target = "enemy"

# ── Dean's last stand: more damage when low HP ──

proc dean_last_stand():
    let hp_pct = dean_hp * 100 / dean_max_hp
    let damage = dean_atk + skill_riflery * 2
    if hp_pct <= 25:
        damage = damage * 3
        battle_msg = "Dean fights with everything he's got! " + str(damage) + " massive damage!"
    else:
        if hp_pct <= 50:
            damage = damage * 2
            battle_msg = "Dean digs deep! " + str(damage) + " damage!"
        else:
            battle_msg = "Dean attacks! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
