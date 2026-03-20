# House Interior Map Script — Cabin Interior
# Cozy room with furniture and a hermit NPC

proc house_inside_init():
    log("=== Cabin Interior loaded ===")

    set_clear_color(0.25, 0.20, 0.15)
    set_level_zoom("house_inside.json", 2.0)

    # ── Furniture ──
    place_object(64, 64, "Bookshelf")
    place_object(256, 64, "Bed")
    place_object(160, 96, "Table")
    place_object(128, 160, "Chair")
    place_object(64, 224, "Barrel")
    place_object(288, 224, "Chest")
    place_object(224, 160, "Fireplace")

    # ── Cabin owner ──
    spawn_npc("Hermit", 192, 192, 0, false, "assets/textures/ai/merchant_sheet.png", 0, 0, 10, 0, 64, 64)
    anim_define("Hermit", "walk", 3, 0.2, true)
    npc_add_waypoint("Hermit", 192, 192)
    npc_add_waypoint("Hermit", 224, 224)
    npc_add_waypoint("Hermit", 160, 192)
    npc_set_route("Hermit", "patrol")
    npc_start_route("Hermit")

    # No weather indoors
    set_weather("clear")

    log("Cabin Interior ready")

proc house_inside_enter():
    set_weather("clear")
    set_clear_color(0.25, 0.20, 0.15)
    set_clouds(false, 0, 0, 0)
    set_god_rays(false, 0, 0)
