# ═══════════════════════════════════════════════
# Battle Core — Global battle functions
# ═══════════════════════════════════════════════
#
# Shared battle utilities, victory/defeat handlers,
# and the generic defend action.
#
# Available globals (synced from C++):
#   enemy_hp, enemy_max_hp, enemy_atk, enemy_name
#   dean_hp, dean_max_hp, dean_atk, dean_def
#   sam_hp, sam_max_hp, sam_atk
#   active_fighter  (0 = Dean, 1 = Sam)
#   skill_hardiness, skill_unholiness, skill_nerve
#   skill_tactics, skill_exorcism, skill_riflery
#
# Set these to communicate results back:
#   battle_damage, battle_msg, battle_target

# ── Helper: get active fighter name ──

proc get_fighter_name():
    if active_fighter == 1:
        return "Sam"
    return "Dean"

# ── Defend / Heal (shared by all characters) ──

proc defend():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"

    let heal = 8 + random(0, 8) + skill_tactics
    if active_fighter == 0:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    else:
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp

    battle_damage = heal
    battle_msg = fighter_name + " braces! Recovered " + str(heal) + " HP."
    battle_target = fighter_name

# ── Victory ──

proc on_victory():
    let xp = enemy_max_hp / 2 + enemy_atk
    battle_msg = "Victory! Gained " + str(xp) + " XP!"
    battle_damage = xp
    set_flag("last_battle_won", true)

# ── Defeat ──

proc on_defeat():
    battle_msg = "The Winchesters are down!"
    set_flag("last_battle_won", false)

# ── Generic attack (fallback for any character) ──

proc attack_normal():
    let fighter_name = "Dean"
    let base_atk = dean_atk
    if active_fighter == 1:
        fighter_name = "Sam"
        base_atk = sam_atk

    let damage = base_atk + random(-2, 2)
    if damage < 1:
        damage = 1

    # Crit check using Nerve skill
    let crit_roll = random(1, 100)
    if crit_roll <= skill_nerve * 3:
        damage = damage * 2
        battle_msg = fighter_name + " lands a CRITICAL HIT! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"

    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

# ── Generic enemy turn (fallback for unnamed enemies) ──

proc enemy_turn():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    let target_def = dean_def
    if target == 1:
        target_name = "Sam"
        target_def = 2

    let damage = enemy_atk + random(-2, 2) - target_def / 3
    if damage < 1:
        damage = 1

    let special = random(1, 5)
    if special == 5:
        damage = damage + random(5, 10)
        battle_msg = enemy_name + " unleashes a powerful attack on " + target_name + "! " + str(damage) + " damage!"
    else:
        battle_msg = enemy_name + " attacks " + target_name + "! " + str(damage) + " damage!"

    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    battle_damage = damage
    battle_target = target_name
