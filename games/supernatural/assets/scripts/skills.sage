# ═══════════════════════════════════════════════
# Twilight Engine — H.U.N.T.E.R. Skills System
# ═══════════════════════════════════════════════
#
# H - Hardiness   (HP, damage resistance)
# U - Unholiness  (dark knowledge, demon deals)
# N - Nerve       (courage, crits, dodge)
# T - Tactics     (combat strategy, defense)
# E - Exorcism    (holy power, vs supernatural)
# R - Riflery     (weapon damage, accuracy)
#
# Native functions from C++:
#   get_skill(character, skill)     — returns skill value (1-10)
#   set_skill(character, skill, val)
#   get_skill_bonus(character, bonus_type) — returns derived bonus
#
# Bonus types: "hp", "crit", "defense", "holy_mult", "weapon_dmg", "dodge", "dark_mult"

# ── Initialize default skill sets ──

proc init_dean_skills():
    set_skill("dean", "hardiness", 5)
    set_skill("dean", "unholiness", 3)
    set_skill("dean", "nerve", 6)
    set_skill("dean", "tactics", 4)
    set_skill("dean", "exorcism", 3)
    set_skill("dean", "riflery", 6)
    log("Dean skills initialized (H5 U3 N6 T4 E3 R6)")

proc init_sam_skills():
    set_skill("sam", "hardiness", 4)
    set_skill("sam", "unholiness", 5)
    set_skill("sam", "nerve", 4)
    set_skill("sam", "tactics", 6)
    set_skill("sam", "exorcism", 6)
    set_skill("sam", "riflery", 3)
    log("Sam skills initialized (H4 U5 N4 T6 E6 R3)")

# ── Skill check functions ──

proc skill_attack_bonus():
    let fighter = "dean"
    if active_fighter == 1:
        fighter = "sam"
    let riflery = get_skill(fighter, "riflery")
    let bonus = riflery * 2
    battle_damage = battle_damage + bonus

proc skill_crit_check():
    let fighter = "dean"
    if active_fighter == 1:
        fighter = "sam"
    let nerve = get_skill(fighter, "nerve")
    let roll = random(1, 100)
    if roll <= nerve * 3:
        battle_damage = battle_damage * 2
        battle_msg = battle_msg + " CRITICAL!"

proc skill_defense_check():
    let fighter = "dean"
    if active_fighter == 1:
        fighter = "sam"
    let tactics = get_skill(fighter, "tactics")
    let reduction = tactics * 2
    battle_damage = battle_damage - reduction
    if battle_damage < 1:
        battle_damage = 1

proc skill_dodge_check():
    let fighter = "dean"
    if active_fighter == 1:
        fighter = "sam"
    let nerve = get_skill(fighter, "nerve")
    let roll = random(1, 100)
    if roll <= nerve * 2:
        battle_damage = 0
        battle_msg = fighter + " dodges the attack!"

proc skill_holy_bonus():
    let fighter = "dean"
    if active_fighter == 1:
        fighter = "sam"
    let exorcism = get_skill(fighter, "exorcism")
    if enemy_name == "Vampire" or enemy_name == "Demon" or enemy_name == "Spirit":
        let bonus = exorcism * 3
        battle_damage = battle_damage + bonus
        if exorcism >= 7:
            battle_msg = battle_msg + " Holy power surges!"

# ── Level up skill point allocation ──

proc on_level_up():
    log("Level up! Allocate 2 skill points.")
    set_flag("pending_skill_points", true)
    # Engine handles UI for allocation
