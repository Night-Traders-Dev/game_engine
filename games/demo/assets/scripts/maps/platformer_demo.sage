proc platformer_demo_init():
    set_game_type("platformer")
    set_gravity(980)
    set_jump_force(400)
    set_coyote_time(0.1)
    set_double_jump(true)

    # Spawn coins on platforms and along the ground
    spawn_coin(7 * 32, 9 * 32)
    spawn_coin(8 * 32, 9 * 32)
    spawn_coin(16 * 32, 7 * 32)
    spawn_coin(17 * 32, 7 * 32)
    spawn_coin(23 * 32, 5 * 32)
    spawn_coin(31 * 32, 8 * 32)
    spawn_coin(32 * 32, 8 * 32)
    spawn_coin(33 * 32, 8 * 32)

    # Patrol enemy on the ground
    spawn_npc("Slime", 10 * 32, 12 * 32, 0, true, 0, 5, 2, 60, 100)
    npc_set_patrol("Slime", 3 * 32, 11 * 32, 60)
    npc_set_stompable("Slime", true)
    npc_on_stomp("Slime", "on_slime_stomp")

    # Another enemy on the right side
    spawn_npc("Skeleton", 32 * 32, 12 * 32, 0, true, 0, 8, 3, 50, 120)
    npc_set_patrol("Skeleton", 28 * 32, 37 * 32, 50)
    npc_set_stompable("Skeleton", true)
    npc_on_stomp("Skeleton", "on_skeleton_stomp")

    # Moving platform over the first gap
    let p1 = add_platform(12 * 32, 11 * 32, 96, 16, 5)
    platform_add_waypoint(p1, 12 * 32, 11 * 32)
    platform_add_waypoint(p1, 12 * 32, 7 * 32)
    platform_set_speed(p1, 40)
    platform_start(p1)

    log("Platformer demo loaded!")

proc on_slime_stomp():
    emit_preset("dust", 320, 384)
    ui_notify("Stomped the Slime!", 1.5)

proc on_skeleton_stomp():
    emit_preset("dust", 1024, 384)
    ui_notify("Stomped the Skeleton!", 1.5)
