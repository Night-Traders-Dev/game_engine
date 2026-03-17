proc map_init():
    set_game_type("platformer")
    set_gravity(980)
    set_jump_force(400)
    set_coyote_time(0.1)
    set_double_jump(true)

    # Spawn some coins
    spawn_coin(5 * 32, 15 * 32)
    spawn_coin(8 * 32, 13 * 32)
    spawn_coin(12 * 32, 11 * 32)
    spawn_coin(15 * 32, 14 * 32)
    spawn_coin(20 * 32, 10 * 32)

    # Spawn patrol enemy
    spawn_npc("Slime", 320, 480, 0, true, 0, 5, 2, 60, 100)
    npc_set_patrol("Slime", 256, 512, 60)
    npc_set_stompable("Slime", true)
    npc_on_stomp("Slime", "on_slime_stomp")

    # Moving platform
    let p = add_platform(25 * 32, 12 * 32, 96, 16, 5)
    platform_add_waypoint(p, 25 * 32, 12 * 32)
    platform_add_waypoint(p, 25 * 32, 6 * 32)
    platform_set_speed(p, 40)
    platform_start(p)

    # Portal back to forest
    set_portal(58, 17, "forest.json", 2, 10, "Back to Forest")

    log("Platformer demo loaded!")

proc on_slime_stomp():
    emit_preset("dust", 320, 480)
    ui_notify("Stomped!", 1.5)
