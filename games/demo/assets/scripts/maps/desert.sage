# Desert Map Script — Sunscorch Desert
# Sandy expanse with oasis, cactus, and wandering traders

proc desert_init():
    log("=== Sunscorch Desert loaded ===")

    # ── NPCs ──
    spawn_npc("Nomad", 800, 480, 0, false, "assets/textures/ai/merchant_sheet.png", 0, 0, 10, 0, 64, 64)
    spawn_npc("Scorpion", 300, 600, 0, true, "assets/textures/ai/slime_sheet.png", 5, 2, 40, 80, 64, 64)
    spawn_npc("Bandit", 1000, 300, 0, true, "assets/textures/ai/goblin_sheet.png", 5, 3, 70, 150, 64, 64)

    anim_define("Nomad", "walk", 3, 0.2, true)
    anim_define("Scorpion", "walk", 3, 0.2, true)
    anim_define("Bandit", "walk", 3, 0.15, true)

    # Nomad wanders between oasis and edge
    npc_add_waypoint("Nomad", 800, 480)
    npc_add_waypoint("Nomad", 544, 448)
    npc_add_waypoint("Nomad", 300, 448)
    npc_add_waypoint("Nomad", 544, 448)
    npc_set_route("Nomad", "patrol")
    npc_start_route("Nomad")

    npc_set_hostile("Scorpion", true)

    # Bandit patrols the east road
    npc_add_waypoint("Bandit", 1000, 300)
    npc_add_waypoint("Bandit", 1100, 500)
    npc_add_waypoint("Bandit", 1000, 600)
    npc_set_route("Bandit", "patrol")
    npc_start_route("Bandit")
    npc_set_hostile("Bandit", true)

    # ── Vegetation ──
    place_object(160, 320, "Cactus")
    place_object(400, 200, "Cactus")
    place_object(900, 700, "Cactus")
    place_object(700, 350, "Palm Tree")
    place_object(580, 300, "Palm Tree")
    place_object(650, 550, "Palm Tree")

    # ── Props ──
    place_object(544, 416, "Sign Post")
    place_object(200, 800, "Rock")
    place_object(1100, 200, "Rock")
    place_object(1050, 700, "Barrel")
    place_object(950, 150, "Chest")

    log("Sunscorch Desert ready")

proc desert_enter():
    set_clear_color(0.15, 0.12, 0.08)
    set_clouds(true, 0.15, 8, 40)
    set_wind(0.25, 45)
    set_god_rays(true, 0.25, 6)
