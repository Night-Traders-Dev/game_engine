# Volcanic Map Script — Molten Depths
# Lava rivers, obsidian platforms, dangerous enemies

proc volcanic_init():
    log("=== Molten Depths loaded ===")

    # ── Enemies ──
    spawn_npc("FireDragon", 480, 128, 0, true, "assets/textures/ai/dragon_sheet.png", 10, 5, 120, 300, 64, 64)
    spawn_npc("LavaSlime", 200, 400, 0, true, "assets/textures/ai/slime_sheet.png", 5, 2, 40, 80, 64, 64)
    spawn_npc("DarkKnight", 700, 480, 0, true, "assets/textures/ai/skeleton_sheet.png", 8, 4, 90, 200, 64, 64)

    anim_define("FireDragon", "walk", 3, 0.2, true)
    anim_define("LavaSlime", "walk", 3, 0.25, true)
    anim_define("DarkKnight", "walk", 3, 0.18, true)

    # Dragon patrols the upper cavern
    npc_add_waypoint("FireDragon", 480, 128)
    npc_add_waypoint("FireDragon", 700, 128)
    npc_add_waypoint("FireDragon", 700, 256)
    npc_add_waypoint("FireDragon", 480, 256)
    npc_set_route("FireDragon", "patrol")
    npc_start_route("FireDragon")
    npc_set_hostile("FireDragon", true)

    npc_set_hostile("LavaSlime", true)

    # Dark knight patrols the lower section
    npc_add_waypoint("DarkKnight", 700, 400)
    npc_add_waypoint("DarkKnight", 800, 500)
    npc_add_waypoint("DarkKnight", 600, 550)
    npc_set_route("DarkKnight", "patrol")
    npc_start_route("DarkKnight")
    npc_set_hostile("DarkKnight", true)

    # ── Props ──
    place_object(384, 288, "Torch")
    place_object(576, 288, "Torch")
    place_object(384, 416, "Torch")
    place_object(576, 416, "Torch")
    place_object(480, 480, "Chest")
    place_object(160, 160, "Rock")
    place_object(800, 160, "Rock")
    place_object(256, 544, "Cauldron")

    log("Molten Depths ready")

proc volcanic_enter():
    set_clear_color(0.08, 0.02, 0.01)
    set_clouds(true, 0.6, 10, 30)
    set_wind(0.1, 15)
    set_god_rays(false, 0, 0)
