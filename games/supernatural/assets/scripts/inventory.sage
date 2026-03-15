# ═══════════════════════════════════════════════
# Twilight Engine — Inventory System (SageLang)
# ═══════════════════════════════════════════════
#
# Native functions available from C++:
#   add_item(id, name, qty, type, desc, heal, dmg, element, sage_func)
#   remove_item(id, qty)
#   has_item(id)          — returns true/false
#   item_count(id)        — returns quantity
#
# Battle globals (synced from C++):
#   item_id     — ID of item being used
#   item_heal   — heal_hp value
#   item_damage — damage value
#   item_element — element string

# ── Setup starting inventory ──

proc give_starter_items():
    add_item("first_aid_kit", "First Aid Kit", 3, "consumable", "Heals 30 HP", 30, 0, "", "use_first_aid")
    add_item("burger", "Burger", 2, "consumable", "Dean's favorite. Heals 50 HP", 50, 0, "", "use_burger")
    add_item("beer", "Beer", 3, "consumable", "Takes the edge off. Heals 15 HP", 15, 0, "", "use_beer")
    add_item("salt_round", "Salt Rounds", 5, "weapon", "Rock salt shells", 0, 20, "salt", "use_salt_round")
    log("Starter items given")

# ── Bobby's supply drop ──

proc bobby_supplies():
    add_item("shotgun_shells", "Shotgun Shells", 8, "weapon", "Standard buckshot", 0, 28, "", "use_shotgun_shells")
    add_item("holy_water", "Holy Water", 4, "weapon", "Burns demons and vampires", 0, 35, "holy", "use_holy_water")
    add_item("silver_bullet", "Silver Bullets", 3, "weapon", "For werewolves and shapeshifters", 0, 40, "silver", "use_silver_bullet")
    add_item("first_aid_kit", "First Aid Kit", 2, "consumable", "Heals 30 HP", 30, 0, "", "use_first_aid")
    set_flag("has_shotgun", true)
    set_flag("has_holy_water", true)
    log("Bobby gave supplies")

# ── Special item rewards ──

proc find_angel_blade():
    add_item("angel_blade", "Angel Blade", 1, "weapon", "Can kill angels and demons", 0, 60, "holy", "use_angel_blade")
    log("Found Angel Blade!")

proc find_rubys_knife():
    add_item("rubys_knife", "Ruby's Knife", 1, "weapon", "Kills demons on contact", 0, 55, "demon", "use_rubys_knife")
    log("Found Ruby's Knife!")

proc find_the_colt():
    add_item("the_colt", "The Colt", 1, "weapon", "Can kill anything", 0, 99, "divine", "use_the_colt")
    set_flag("has_colt", true)
    log("Found The Colt!")

# ── Key items ──

proc find_emf_meter():
    add_item("emf_meter", "EMF Meter", 1, "key", "Detects supernatural activity", 0, 0, "", "")
    set_flag("has_emf", true)
    log("Found EMF Meter")

proc find_journal():
    add_item("johns_journal", "John's Journal", 1, "key", "Dad's hunting journal", 0, 0, "", "")
    set_flag("has_journal", true)
    log("Found John's Journal")

# ═══════════════════════════════════════════════
# Battle Item Usage Functions
# ═══════════════════════════════════════════════
#
# Each function is called when a player uses that item.
# The engine syncs: active_fighter, dean_hp/max, sam_hp/max,
#                   enemy_hp, item_heal, item_damage, item_element

proc use_first_aid():
    let heal = 30 + random(0, 10)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " uses First Aid Kit! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("first_aid_kit", 1)

proc use_burger():
    let heal = 50 + random(0, 15)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " eats a Burger! Healed " + str(heal) + " HP!"
    battle_target = fighter_name
    remove_item("burger", 1)

proc use_beer():
    let heal = 15 + random(0, 5)
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp
    else:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    battle_damage = heal
    battle_msg = fighter_name + " drinks a Beer. Healed " + str(heal) + " HP."
    battle_target = fighter_name
    remove_item("beer", 1)

proc use_salt_round():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 20 + random(0, 8)
    # Salt is super effective against spirits
    if enemy_name == "Spirit" or enemy_name == "Ghost":
        damage = damage * 2
        battle_msg = fighter_name + " fires salt rounds! Super effective! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " fires salt rounds! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("salt_round", 1)

proc use_shotgun_shells():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 28 + random(0, 10)
    battle_msg = fighter_name + " blasts with the shotgun! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("shotgun_shells", 1)

proc use_holy_water():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 35 + random(0, 15)
    # Holy water is super effective against demons and vampires
    if enemy_name == "Vampire" or enemy_name == "Demon":
        damage = damage * 2
        battle_msg = fighter_name + " throws Holy Water! It burns! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " throws Holy Water! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("holy_water", 1)

proc use_silver_bullet():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 40 + random(0, 12)
    # Silver is super effective against werewolves and shapeshifters
    if enemy_name == "Werewolf" or enemy_name == "Shapeshifter":
        damage = damage * 2
        battle_msg = fighter_name + " fires a silver bullet! Critical weakness! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " fires a silver bullet! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("silver_bullet", 1)

proc use_angel_blade():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 60 + random(0, 20)
    battle_msg = fighter_name + " strikes with the Angel Blade! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # Angel blade doesn't get consumed

proc use_rubys_knife():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 55 + random(0, 18)
    if enemy_name == "Demon":
        damage = damage * 2
        battle_msg = fighter_name + " stabs with Ruby's Knife! Lethal to demons! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " slashes with Ruby's Knife! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # Ruby's knife doesn't get consumed

proc use_the_colt():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"
    let damage = 99 + random(0, 50)
    battle_msg = fighter_name + " fires The Colt! " + str(damage) + " massive damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    # The Colt doesn't get consumed but has limited ammo lore-wise
