# ═══════════════════════════════════════════════
# Twilight Engine — Battle System (SageLang)
# ═══════════════════════════════════════════════
#
# This script defines all battle actions.
# The C++ engine calls these functions and syncs
# battle state variables before/after each call.
#
# Available globals (synced from C++):
#   enemy_hp, enemy_max_hp, enemy_atk, enemy_name
#   dean_hp, dean_max_hp, dean_atk, dean_def
#   sam_hp, sam_max_hp, sam_atk
#   active_fighter  (0 = Dean, 1 = Sam)
#
# Set these to communicate results back:
#   battle_damage   — damage/heal amount
#   battle_msg      — message to display
#   battle_target   — who was hit

# ── Player Attack Actions ──

proc attack_normal():
    let fighter_name = "Dean"
    let base_atk = dean_atk
    if active_fighter == 1:
        fighter_name = "Sam"
        base_atk = sam_atk

    let damage = base_atk + random(-2, 2)
    if damage < 1:
        damage = 1

    # Critical hit chance (10%)
    let crit = random(1, 10)
    if crit == 10:
        damage = damage * 2
        battle_msg = fighter_name + " lands a CRITICAL HIT! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"

    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

proc attack_shotgun():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"

    if get_flag("has_shotgun"):
        let damage = 25 + random(0, 10)
        enemy_hp = enemy_hp - damage
        battle_damage = damage
        battle_msg = fighter_name + " fires the shotgun! " + str(damage) + " damage!"
        battle_target = "enemy"
    else:
        battle_msg = fighter_name + " doesn't have a shotgun!"
        battle_damage = 0

proc attack_holy_water():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"

    if get_flag("has_holy_water"):
        let damage = 30 + random(0, 15)
        enemy_hp = enemy_hp - damage
        battle_damage = damage
        battle_msg = fighter_name + " throws holy water! " + str(damage) + " damage!"
        battle_target = "enemy"
    else:
        battle_msg = "No holy water!"
        battle_damage = 0

# ── Defend / Heal ──

proc defend():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"

    let heal = 8 + random(0, 8)
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

# ── Enemy AI ──

proc enemy_turn():
    # Pick target: random between Dean and Sam (if alive)
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

    # Calculate damage
    let damage = enemy_atk + random(-2, 2) - target_def / 3
    if damage < 1:
        damage = 1

    # Special attack chance (20%)
    let special = random(1, 5)
    if special == 5:
        damage = damage + random(5, 10)
        battle_msg = enemy_name + " unleashes a powerful attack on " + target_name + "! " + str(damage) + " damage!"
    else:
        battle_msg = enemy_name + " attacks " + target_name + "! " + str(damage) + " damage!"

    # Apply damage
    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    battle_damage = damage
    battle_target = target_name

# ── Victory / Defeat ──

proc on_victory():
    let xp = enemy_max_hp / 2 + enemy_atk
    battle_msg = "Victory! Gained " + str(xp) + " XP!"
    battle_damage = xp
    set_flag("last_battle_won", true)

proc on_defeat():
    battle_msg = "The Winchesters are down!"
    set_flag("last_battle_won", false)

# ── Vampire-specific battle ──

proc vampire_attack():
    # Vampires drain HP
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    let damage = enemy_atk + random(0, 5)
    let drain = damage / 3

    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    # Vampire heals from the drain
    enemy_hp = enemy_hp + drain
    if enemy_hp > enemy_max_hp:
        enemy_hp = enemy_max_hp

    battle_msg = enemy_name + " bites " + target_name + "! " + str(damage) + " damage! Drains " + str(drain) + " HP!"
    battle_damage = damage
    battle_target = target_name
