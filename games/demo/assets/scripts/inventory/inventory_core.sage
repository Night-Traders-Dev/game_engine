# ═══════════════════════════════════════════════
# Final Fantasy Demo — Items & Magic
# ═══════════════════════════════════════════════
#
# Classic FF item set:
#   Potion     — Heals 50 HP
#   Hi-Potion  — Heals 150 HP
#   Ether      — (future: restores MP)
#   Phoenix Down — Revives KO'd ally
#   Antidote   — (future: cures Poison)
#
# Black Magic (used as consumable scrolls for now):
#   Fire    — Fire elemental damage
#   Blizzard — Ice elemental damage
#   Thunder  — Lightning elemental damage
#   Cure     — Heals one ally (White Magic)

proc give_starter_items():
    add_item("potion", "Potion", 10, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_item("ether", "Ether", 3, "consumable", "Restores spirit", 20, 0, "", "use_ether")
    add_item("phoenix_down", "Phoenix Down", 2, "consumable", "Life to fallen ally", 0, 0, "", "use_phoenix_down")
    add_item("fire", "Fire", 5, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
    add_item("blizzard", "Blizzard", 3, "weapon", "Ice magic - 30 dmg", 0, 30, "ice", "use_blizzard")
    add_item("thunder", "Thunder", 3, "weapon", "Lightning magic - 30 dmg", 0, 30, "lightning", "use_thunder")
    add_item("cure", "Cure", 5, "consumable", "White magic - heal 40 HP", 40, 0, "holy", "use_cure")
    log("Starter items: Potions, Ether, Phoenix Down, Fire, Blizzard, Thunder, Cure")

# ═══════════════════════════════════════════════
# Items
# ═══════════════════════════════════════════════

proc use_potion():
    let heal = 50 + random(0, 10)
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
        ally_hp = ally_hp + heal
        if ally_hp > ally_max_hp:
            ally_hp = ally_max_hp
    else:
        player_hp = player_hp + heal
        if player_hp > player_max_hp:
            player_hp = player_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " uses Potion! Recovered " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("potion", 1)

proc use_ether():
    let heal = 20 + random(0, 10)
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
        ally_hp = ally_hp + heal
        if ally_hp > ally_max_hp:
            ally_hp = ally_max_hp
    else:
        player_hp = player_hp + heal
        if player_hp > player_max_hp:
            player_hp = player_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " uses Ether! Recovered " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("ether", 1)

proc use_phoenix_down():
    # Revive the other party member (heal to 25% HP)
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    if active_fighter == 0:
        if ally_hp <= 0:
            ally_hp = ally_max_hp / 4
            if ally_hp < 1:
                ally_hp = 1
            battle_msg = fighter_name + " uses Phoenix Down! Black Mage is revived!"
            battle_damage = ally_hp
            battle_target = "Black Mage"
            remove_item("phoenix_down", 1)
        else:
            battle_msg = "Black Mage is not KO'd!"
            battle_damage = 0
    else:
        if player_hp <= 0:
            player_hp = player_max_hp / 4
            if player_hp < 1:
                player_hp = 1
            battle_msg = fighter_name + " uses Phoenix Down! Warrior is revived!"
            battle_damage = player_hp
            battle_target = "Mage"
            remove_item("phoenix_down", 1)
        else:
            battle_msg = "Warrior is not KO'd!"
            battle_damage = 0

# ═══════════════════════════════════════════════
# Black Magic (spell scrolls)
# ═══════════════════════════════════════════════

proc use_fire():
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    # Magic power scales with MAG stat (unholiness)
    let damage = 30 + random(0, 12) + skill_arcana * 3
    # Fire is super effective against undead and slimes
    if enemy_name == "Skeleton" or enemy_name == "Slime":
        damage = damage + damage / 2
        battle_msg = fighter_name + " casts Fire! It's super effective! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " casts Fire! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("fire", 1)

proc use_blizzard():
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    let damage = 30 + random(0, 12) + skill_arcana * 3
    battle_msg = fighter_name + " casts Blizzard! " + str(damage) + " ice damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("blizzard", 1)

proc use_thunder():
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    let damage = 30 + random(0, 12) + skill_arcana * 3
    # Thunder has a chance to stun (skip enemy turn — simulated as extra damage)
    let stun = random(1, 4)
    if stun == 4:
        damage = damage + 15
        battle_msg = fighter_name + " casts Thunder! Stun! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " casts Thunder! " + str(damage) + " lightning damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("thunder", 1)

# ═══════════════════════════════════════════════
# White Magic
# ═══════════════════════════════════════════════

proc use_cure():
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
    # Cure scales with Spirit (tactics)
    let heal = 40 + random(0, 15) + skill_tactics * 3
    if active_fighter == 0:
        player_hp = player_hp + heal
        if player_hp > player_max_hp:
            player_hp = player_max_hp
    else:
        ally_hp = ally_hp + heal
        if ally_hp > ally_max_hp:
            ally_hp = ally_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " casts Cure! Recovered " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("cure", 1)
