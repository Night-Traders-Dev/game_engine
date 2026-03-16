# House Interior Map Script
# 9x9 room, exit portal at (4, 8)

proc house_inside_init():
    log("=== House Interior loaded ===")

    set_clear_color(0.12, 0.10, 0.06)

    # Zoom in for interior detail (2x = close-up view)
    set_level_zoom("house_inside.json", 2.0)

    # ── Furniture ──
    place_object(64, 96, "Barrel")
    place_object(96, 96, "Crate")
    place_object(224, 96, "Barrel")

    # ── House owner ──
    spawn_npc("Hermit", 160, 192, 0, false, "assets/textures/merchant_sprites.png", 0, 0, 10, 0)
    npc_add_waypoint("Hermit", 160, 192)
    npc_add_waypoint("Hermit", 192, 224)
    npc_add_waypoint("Hermit", 128, 192)
    npc_set_route("Hermit", "patrol")
    npc_start_route("Hermit")

    log("House interior ready")
