# Day-Night cycle configuration, night spawning, and loot tables

proc init_day_night():
    set_day_speed(6)
    set_time(22, 0)
    log("Day-Night: speed=6, starting at 10 PM")

    # Spawn skeletons only at night (18:00 - 06:00)
    spawn_loop("Skeleton", 15, 5)
    set_spawn_area("Skeleton", 300, 200, 800, 600)
    set_spawn_time("Skeleton", 18, 6)
    log("Night spawning configured: Skeletons 6PM-6AM, every 15s, max 5")

    # ── Default loot tables ──
    # All enemies drop gold pouch (high chance)
    add_loot("*", "gold_pouch", "Gold Pouch", 0.8, "consumable", "Contains 10-30 gold", 0, 0, "", "use_gold_pouch")

    # Skeleton drops
    add_loot("Skeleton", "bone", "Bone", 0.6, "key", "A skeletal bone", 0, 0, "", "")
    add_loot("Skeleton", "potion", "Potion", 0.4, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_loot("Skeleton", "iron_sword", "Iron Sword", 0.1, "weapon", "A battered iron blade - 15 ATK", 0, 15, "", "")

    # Slime drops
    add_loot("Slime", "slime_gel", "Slime Gel", 0.7, "consumable", "Sticky gel, restores 20 HP", 20, 0, "", "use_slime_gel")
    add_loot("Slime", "antidote", "Antidote", 0.3, "consumable", "Cures poison", 0, 0, "", "use_antidote")

    log("Loot tables configured")

# Gold pouch gives random gold
proc use_gold_pouch():
    let amount = 10 + random(0, 20)
    set_gold(get_gold() + amount)
    ui_notify("Found " + str(amount) + " gold!", 2)

proc use_slime_gel():
    let fighter_name = "Mage"
    if active_fighter == 1:
        fighter_name = "Black Mage"
        ally_hp = ally_hp + 20
        if ally_hp > ally_max_hp:
            ally_hp = ally_max_hp
    else:
        player_hp = player_hp + 20
        if player_hp > player_max_hp:
            player_hp = player_max_hp
    battle_damage = 20
    battle_msg = fighter_name + " uses Slime Gel! Recovered 20 HP."
    battle_target = fighter_name
    remove_item("slime_gel", 1)

proc log_size():
    log("Map Width: " + str(get_map_width()) + ", Map Height: " + str(get_map_height()))
    log("Screen Width: " + str(hud_get("screen_w")) + ", Screen Height: " + str(hud_get("screen_h")))
