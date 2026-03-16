# Forest Map Script — Enchanted Forest with house
# House footprint: tiles (13,7)-(15,10), door portal at (14,11)

proc forest_init():
    log("=== Enchanted Forest loaded ===")

    # ── House sprite — centered over collision footprint ──
    # Footprint center X = (13+1.5)*32 = 464, bottom Y = (7+4)*32 = 352
    place_object(464, 352, "House")

    # ── Dense tree ring around forest edges ──
    # Top edge
    place_object(64, 80, "Oak Tree")
    place_object(192, 64, "Oak Tree")
    place_object(352, 80, "Oak Tree")
    place_object(512, 64, "Oak Tree")
    place_object(672, 80, "Oak Tree")
    place_object(832, 64, "Oak Tree")
    place_object(128, 112, "Small Oak")
    place_object(288, 96, "Small Oak")
    place_object(448, 112, "Small Oak")
    place_object(608, 96, "Small Oak")
    place_object(768, 112, "Small Oak")
    place_object(896, 96, "Small Oak")

    # Bottom edge
    place_object(64, 656, "Oak Tree")
    place_object(224, 672, "Oak Tree")
    place_object(384, 656, "Oak Tree")
    place_object(576, 672, "Oak Tree")
    place_object(736, 656, "Oak Tree")
    place_object(896, 672, "Oak Tree")
    place_object(160, 640, "Small Oak")
    place_object(480, 640, "Small Oak")
    place_object(640, 640, "Small Oak")
    place_object(832, 640, "Small Oak")

    # Left edge
    place_object(48, 192, "Oak Tree")
    place_object(64, 336, "Oak Tree")
    place_object(48, 480, "Oak Tree")
    place_object(80, 256, "Small Oak")
    place_object(80, 416, "Small Oak")
    place_object(80, 560, "Small Oak")

    # Right edge
    place_object(912, 192, "Oak Tree")
    place_object(928, 336, "Oak Tree")
    place_object(912, 480, "Oak Tree")
    place_object(896, 256, "Small Oak")
    place_object(896, 416, "Small Oak")
    place_object(896, 560, "Small Oak")

    # ── Interior scattered trees ──
    place_object(224, 256, "Oak Tree")
    place_object(736, 240, "Oak Tree")
    place_object(224, 512, "Oak Tree")
    place_object(768, 512, "Oak Tree")
    place_object(320, 384, "Small Oak")
    place_object(640, 304, "Small Oak")
    place_object(544, 528, "Small Oak")
    place_object(704, 416, "Small Oak")

    # ── Bushes ──
    place_object(176, 192, "Bush Small")
    place_object(800, 192, "Bush Small")
    place_object(176, 560, "Bush Small")
    place_object(800, 560, "Bush Small")

    # ── NPCs ──
    spawn_npc("Elder", 352, 480, 0, false, "assets/textures/elder_sprites.png", 0, 0, 20, 0)
    spawn_npc("Merchant", 608, 384, 0, false, "assets/textures/merchant_sprites.png", 0, 0, 15, 0)
    spawn_npc("Woodsman", 672, 320, 0, false, "assets/textures/elder_sprites.png", 0, 0, 20, 0)
    spawn_npc("Fox", 288, 416, 3, false, "assets/textures/cf_chicken.png", 0, 0, 35, 0)

    npc_set_schedule("Merchant", 6, 20)

    npc_add_waypoint("Woodsman", 672, 320)
    npc_add_waypoint("Woodsman", 736, 448)
    npc_add_waypoint("Woodsman", 672, 544)
    npc_set_route("Woodsman", "patrol")
    npc_start_route("Woodsman")

    npc_add_waypoint("Fox", 288, 416)
    npc_add_waypoint("Fox", 384, 544)
    npc_add_waypoint("Fox", 192, 320)
    npc_set_route("Fox", "patrol")
    npc_start_route("Fox")

    # ── Night wolves ──
    spawn_npc("Wolf", 700, 200, 0, true, "assets/textures/cf_slime.png", 25, 8, 55, 120)
    npc_set_despawn_day("Wolf", true)
    spawn_loop("Wolf", 45, 2)
    set_spawn_area("Wolf", 192, 192, 768, 544)
    set_spawn_time("Wolf", 20, 5)
    add_loot("Wolf", "wolf_pelt", "Wolf Pelt", 0.7, "consumable", "A thick fur", 0, 0, "", "")

    # ── Forest atmosphere ──
    set_clear_color(0.02, 0.04, 0.02)
    set_day_speed(6)

    # ── Weather: cloudy forest with god rays by default ──
    set_clouds(true, 0.4, 15, 60)
    set_god_rays(true, 0.2, 4)
    set_wind(0.2, 30)

    log("Forest ready")
