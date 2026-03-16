# Forest Map Script — Enchanted Forest
# House center, dense tree border, dirt paths, small pond

proc forest_init():
    log("=== Enchanted Forest loaded ===")

    # ── House sprite centered over door portal (14, 12) ──
    place_object(464, 320, "House")

    # ── Dense tree ring (edge trees match dark tile border) ──
    # Top edge
    place_object(96, 96, "Oak Tree")
    place_object(224, 80, "Oak Tree")
    place_object(384, 96, "Oak Tree")
    place_object(544, 80, "Oak Tree")
    place_object(704, 96, "Oak Tree")
    place_object(864, 80, "Oak Tree")
    place_object(160, 128, "Small Oak")
    place_object(320, 112, "Small Oak")
    place_object(480, 128, "Small Oak")
    place_object(640, 112, "Small Oak")
    place_object(800, 128, "Small Oak")

    # Bottom edge
    place_object(96, 640, "Oak Tree")
    place_object(288, 656, "Oak Tree")
    place_object(480, 640, "Oak Tree")
    place_object(672, 656, "Oak Tree")
    place_object(864, 640, "Oak Tree")

    # Left edge
    place_object(64, 224, "Oak Tree")
    place_object(80, 384, "Oak Tree")
    place_object(64, 544, "Oak Tree")

    # Right edge
    place_object(896, 224, "Oak Tree")
    place_object(880, 384, "Oak Tree")
    place_object(896, 544, "Oak Tree")

    # Interior scattered trees (not near house or paths)
    place_object(240, 256, "Oak Tree")
    place_object(720, 224, "Oak Tree")
    place_object(240, 480, "Oak Tree")
    place_object(768, 480, "Oak Tree")
    place_object(352, 384, "Small Oak")
    place_object(640, 288, "Small Oak")
    place_object(576, 512, "Small Oak")

    # Bushes for detail
    place_object(192, 192, "Bush Small")
    place_object(800, 192, "Bush Small")
    place_object(192, 560, "Bush Small")
    place_object(800, 560, "Bush Small")

    # ── NPCs ──
    spawn_npc("Elder", 384, 448, 0, false, "assets/textures/elder_sprites.png", 0, 0, 20, 0, 32, 32)
    spawn_npc("Merchant", 576, 352, 0, false, "assets/textures/merchant_sprites.png", 0, 0, 15, 0, 32, 32)
    spawn_npc("Woodsman", 640, 320, 0, false, "assets/textures/elder_sprites.png", 0, 0, 20, 0, 32, 32)
    spawn_npc("Fox", 288, 400, 3, false, "assets/textures/cf_chicken.png", 0, 0, 35, 0, 32, 32)

    npc_set_schedule("Merchant", 6, 20)

    npc_add_waypoint("Woodsman", 640, 320)
    npc_add_waypoint("Woodsman", 736, 448)
    npc_add_waypoint("Woodsman", 640, 544)
    npc_set_route("Woodsman", "patrol")
    npc_start_route("Woodsman")

    npc_add_waypoint("Fox", 288, 400)
    npc_add_waypoint("Fox", 384, 528)
    npc_add_waypoint("Fox", 192, 320)
    npc_set_route("Fox", "patrol")
    npc_start_route("Fox")

    # ── Night wolves ──
    spawn_npc("Wolf", 700, 200, 0, true, "assets/textures/cf_wolf.png", 25, 8, 55, 120, 32, 32)
    npc_set_despawn_day("Wolf", true)
    spawn_loop("Wolf", 45, 2)
    set_spawn_area("Wolf", 192, 192, 768, 544)
    set_spawn_time("Wolf", 20, 5)
    add_loot("Wolf", "wolf_pelt", "Wolf Pelt", 0.7, "consumable", "A thick fur", 0, 0, "", "")

    # ── Weather: cloudy forest with god rays ──
    set_clouds(true, 0.35, 12, 60)
    set_god_rays(true, 0.18, 4)
    set_wind(0.15, 30)

    # ── Atmosphere ──
    set_clear_color(0.02, 0.04, 0.02)
    set_day_speed(6)

    log("Forest ready")
