# ═══════════════════════════════════════════════
# Sam Winchester — Battle Actions
# ═══════════════════════════════════════════════
#
# Sam's combat style: strategic, knowledge-based.
# High Exorcism and Tactics make him the anti-supernatural specialist.

# ── Sam's signature attack: Exorcism chant ──

proc sam_exorcism():
    let damage = sam_atk + skill_exorcism * 3
    if enemy_name == "Demon" or enemy_name == "Spirit" or enemy_name == "Ghost":
        damage = damage * 2
        battle_msg = "Sam chants an exorcism rite! Super effective! " + str(damage) + " damage!"
    else:
        battle_msg = "Sam chants a banishing ritual! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"

# ── Sam's research: reveal enemy weakness ──

proc sam_analyze():
    let intel = skill_tactics + skill_exorcism
    if intel >= 10:
        battle_msg = "Sam identifies a critical weakness! Enemy DEF lowered!"
        enemy_atk = enemy_atk - 3
    else:
        battle_msg = "Sam studies the enemy's patterns. Next attack will be stronger!"
    set_flag("sam_analyzed", true)
    battle_damage = 0
    battle_target = "enemy"

# ── Sam's psychic flash (Unholiness-based) ──

proc sam_psychic():
    let damage = sam_atk + skill_unholiness * 4
    let strain = 5 + random(0, 5)
    enemy_hp = enemy_hp - damage
    sam_hp = sam_hp - strain
    if sam_hp < 1:
        sam_hp = 1
    battle_msg = "Sam unleashes a psychic blast! " + str(damage) + " damage! (Took " + str(strain) + " strain)"
    battle_damage = damage
    battle_target = "enemy"

# ── Sam's protective ward ──

proc sam_ward():
    let ward_heal = 10 + skill_exorcism * 2
    dean_hp = dean_hp + ward_heal
    if dean_hp > dean_max_hp:
        dean_hp = dean_max_hp
    sam_hp = sam_hp + ward_heal
    if sam_hp > sam_max_hp:
        sam_hp = sam_max_hp
    battle_msg = "Sam draws a protection ward! Both healed " + str(ward_heal) + " HP!"
    battle_damage = ward_heal
    battle_target = "Sam"
