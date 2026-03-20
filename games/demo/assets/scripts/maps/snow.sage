# Snow Map Script — Frostpeak Summit
# Frozen peaks with ice lake, bare trees, and hardy NPCs

proc snow_init():
    log("=== Frostpeak Summit loaded ===")

    # ── NPCs ──
    spawn_npc("Ranger", 400, 400, 0, false, "assets/textures/ai/knight_sheet.png", 0, 0, 30, 0, 64, 64)
    spawn_npc("IceWolf", 200, 600, 0, true, "assets/textures/ai/slime_sheet.png", 8, 4, 60, 100, 64, 64)
    spawn_npc("FrostMage", 900, 300, 0, true, "assets/textures/ai/wizard_sheet.png", 5, 3, 80, 200, 64, 64)

    anim_define("Ranger", "walk", 3, 0.2, true)
    anim_define("IceWolf", "walk", 3, 0.15, true)
    anim_define("FrostMage", "walk", 3, 0.18, true)

    # Ranger patrols the southern path
    npc_add_waypoint("Ranger", 400, 400)
    npc_add_waypoint("Ranger", 608, 700)
    npc_add_waypoint("Ranger", 608, 800)
    npc_add_waypoint("Ranger", 400, 600)
    npc_set_route("Ranger", "patrol")
    npc_start_route("Ranger")

    npc_set_hostile("IceWolf", true)

    # Frost mage guards the frozen lake
    npc_add_waypoint("FrostMage", 900, 300)
    npc_add_waypoint("FrostMage", 750, 300)
    npc_add_waypoint("FrostMage", 750, 500)
    npc_add_waypoint("FrostMage", 900, 500)
    npc_set_route("FrostMage", "patrol")
    npc_start_route("FrostMage")
    npc_set_hostile("FrostMage", true)

    # ── Vegetation ──
    place_object(100, 200, "Dead Tree")
    place_object(500, 150, "Dead Tree")
    place_object(300, 700, "Dead Tree")
    place_object(1100, 400, "Dead Tree")
    place_object(150, 500, "Pine Tree")
    place_object(1000, 700, "Pine Tree")
    place_object(700, 100, "Pine Tree")
    place_object(450, 300, "Bush")

    # ── Props ──
    place_object(608, 860, "Sign Post")
    place_object(200, 350, "Rock")
    place_object(1000, 200, "Rock")
    place_object(600, 500, "Chest")
    place_object(350, 800, "Barrel")

    log("Frostpeak Summit ready")

proc snow_enter():
    set_clear_color(0.08, 0.08, 0.12)
    set_clouds(true, 0.5, 15, 50)
    set_wind(0.3, 60)
    set_god_rays(false, 0, 0)
