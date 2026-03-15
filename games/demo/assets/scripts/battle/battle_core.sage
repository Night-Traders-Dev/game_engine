# ═══════════════════════════════════════════════
# Twilight Engine Demo — Final Fantasy Battle Core
# ═══════════════════════════════════════════════
#
# Classic FF turn-based battle system.
# Warrior uses Fight, Black Mage uses Fight (weaker).
# Magic and items are handled via inventory system.

# ── Fight Command ──

proc attack_normal():
    let fighter_name = "Warrior"
    let base_atk = dean_atk
    if active_fighter == 1:
        fighter_name = "Black Mage"
        base_atk = sam_atk
    let damage = base_atk + random(-1, 4)
    if damage < 1:
        damage = 1
    # Critical hit based on Luck (exorcism)
    let crit_roll = random(1, 100)
    if crit_roll <= skill_exorcism * 3:
        damage = damage * 2
        battle_msg = fighter_name + " scores a critical hit! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

# ── Defend Command ──

proc defend():
    let fighter_name = "Warrior"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    # Spirit (tactics) boosts healing from Defend
    let heal = 8 + random(0, 6) + skill_tactics
    if active_fighter == 0:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    else:
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " guards... Recovered " + str(heal) + " HP."
    battle_target = fighter_name

# ── Enemy Turn (generic) ──

proc enemy_turn():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0
    let target_name = "Warrior"
    if target == 1:
        target_name = "Black Mage"
    let damage = enemy_atk + random(-2, 3)
    if damage < 1:
        damage = 1
    # Evasion check based on Speed (nerve)
    let evade_roll = random(1, 100)
    let target_speed = skill_nerve
    if evade_roll <= target_speed * 2:
        battle_msg = target_name + " dodges the attack!"
        battle_damage = 0
        battle_target = target_name
    else:
        let special = random(1, 5)
        if special == 5:
            damage = damage + random(4, 10)
            battle_msg = enemy_name + " uses a fierce attack on " + target_name + "! " + str(damage) + " damage!"
        else:
            battle_msg = enemy_name + " attacks " + target_name + "! " + str(damage) + " damage!"
        if target == 0:
            dean_hp = dean_hp - damage
        else:
            sam_hp = sam_hp - damage
        battle_damage = damage
        battle_target = target_name

# ── Victory ──

proc on_victory():
    let xp = enemy_max_hp / 2 + enemy_atk * 2
    let gil = enemy_max_hp + random(10, 30)
    battle_msg = "Victory! Earned " + str(xp) + " EXP, " + str(gil) + " Gil!"
    battle_damage = xp
    set_flag("last_battle_won", true)

# ── Defeat ──

proc on_defeat():
    battle_msg = "The party has been defeated..."
    set_flag("last_battle_won", false)
