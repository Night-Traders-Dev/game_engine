# Cave Map Script — Crystal Caverns
# Dark dungeon with crystal veins, underground pool, and hostile mobs

proc cave_init():
    log("=== Crystal Caverns loaded ===")

    # ── Enemies ──
    spawn_npc("Skeleton", 480, 256, 0, true, "assets/textures/ai/skeleton_sheet.png", 8, 4, 50, 120, 64, 64)
    spawn_npc("Slime", 256, 480, 0, true, "assets/textures/ai/slime_sheet.png", 5, 2, 30, 60, 64, 64)
    spawn_npc("Goblin", 700, 300, 0, true, "assets/textures/ai/goblin_sheet.png", 5, 3, 60, 100, 64, 64)

    anim_define("Skeleton", "walk", 3, 0.18, true)
    anim_define("Slime", "walk", 3, 0.25, true)
    anim_define("Goblin", "walk", 3, 0.15, true)

    # Skeleton patrols the crystal corridor
    npc_add_waypoint("Skeleton", 320, 256)
    npc_add_waypoint("Skeleton", 640, 256)
    npc_add_waypoint("Skeleton", 640, 416)
    npc_add_waypoint("Skeleton", 320, 416)
    npc_set_route("Skeleton", "patrol")
    npc_start_route("Skeleton")
    npc_set_hostile("Skeleton", true)

    # Slime wanders near the underground pool
    npc_set_hostile("Slime", true)

    # Goblin guards the entrance
    npc_add_waypoint("Goblin", 700, 200)
    npc_add_waypoint("Goblin", 800, 300)
    npc_add_waypoint("Goblin", 700, 400)
    npc_set_route("Goblin", "patrol")
    npc_start_route("Goblin")
    npc_set_hostile("Goblin", true)

    # ── Props ──
    place_object(320, 160, "Torch")
    place_object(640, 160, "Torch")
    place_object(320, 512, "Torch")
    place_object(640, 512, "Torch")
    place_object(480, 384, "Chest")
    place_object(160, 320, "Rock")
    place_object(800, 480, "Rock")
    place_object(128, 128, "Barrel")

    log("Crystal Caverns ready")

proc cave_enter():
    set_clear_color(0.02, 0.02, 0.03)
    set_clouds(false, 0, 0, 0)
    set_god_rays(false, 0, 0)
    set_wind(0, 0)
