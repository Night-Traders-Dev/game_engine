# House Interior Map Script
# Cozy room with furniture objects

proc house_inside_init():
    log("=== House Interior loaded ===")

    # ── Warm interior ──
    set_clear_color(0.12, 0.10, 0.06)
    set_level_zoom("house_inside.json", 2.0)

    # ── Furniture ──
    place_object(64, 128, "Barrel")
    place_object(256, 128, "Crate")
    place_object(160, 96, "Table")

    # ── House owner ──
    spawn_npc("Hermit", 160, 192, 0, false, "assets/textures/merchant_sprites.png", 0, 0, 10, 0, 32, 32)
    npc_add_waypoint("Hermit", 160, 192)
    npc_add_waypoint("Hermit", 192, 224)
    npc_add_waypoint("Hermit", 128, 192)
    npc_set_route("Hermit", "patrol")
    npc_start_route("Hermit")

    # No weather indoors
    set_weather("clear")

    log("House interior ready")
