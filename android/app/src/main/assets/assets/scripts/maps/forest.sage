# Forest Map Script — Enchanted Forest (Village Hub)
# Starting area with cabin, shop, pond, and connecting paths

proc forest_init():
    log("=== Enchanted Forest loaded ===")

    # ── Village NPCs ──
    spawn_npc("Knight", 400, 300, 0, false, "assets/textures/ai/knight_sheet.png", 0, 0, 40, 0, 64, 64)
    spawn_npc("Merchant", 1056, 128, 0, false, "assets/textures/ai/merchant_sheet.png", 0, 0, 10, 0, 64, 64)
    spawn_npc("Princess", 640, 400, 0, false, "assets/textures/ai/princess_sheet.png", 0, 0, 10, 0, 64, 64)
    spawn_npc("Wizard", 300, 500, 0, false, "assets/textures/ai/wizard_sheet.png", 0, 0, 10, 0, 64, 64)

    # Walk animations
    anim_define("Knight", "walk", 3, 0.2, true)
    anim_define("Merchant", "walk", 3, 0.2, true)
    anim_define("Princess", "walk", 3, 0.2, true)
    anim_define("Wizard", "walk", 3, 0.2, true)

    # Knight patrols the village crossroads
    npc_add_waypoint("Knight", 400, 300)
    npc_add_waypoint("Knight", 640, 300)
    npc_add_waypoint("Knight", 640, 500)
    npc_add_waypoint("Knight", 400, 500)
    npc_set_route("Knight", "patrol")
    npc_start_route("Knight")

    # Merchant stays near shop
    npc_set_schedule("Merchant", 6, 20)
    npc_set_spawn_point("Merchant", 1056, 128)

    # Princess wanders near the pond
    npc_add_waypoint("Princess", 640, 400)
    npc_add_waypoint("Princess", 900, 700)
    npc_add_waypoint("Princess", 900, 750)
    npc_add_waypoint("Princess", 640, 400)
    npc_set_route("Princess", "patrol")
    npc_start_route("Princess")

    # Wizard tends his garden
    npc_add_waypoint("Wizard", 300, 500)
    npc_add_waypoint("Wizard", 400, 600)
    npc_add_waypoint("Wizard", 300, 600)
    npc_set_route("Wizard", "patrol")
    npc_start_route("Wizard")

    # Knight greets merchant
    npc_on_meet("Knight", "Merchant", "knight_merchant_chat")

    # ── World Objects ──
    place_object(96, 96, "Sign Post")
    place_object(608, 32, "Sign Post")
    place_object(200, 700, "Barrel")
    place_object(232, 700, "Barrel")
    place_object(500, 600, "Bush")
    place_object(800, 300, "Bush")
    place_object(100, 400, "Oak Tree")
    place_object(1100, 200, "Oak Tree")
    place_object(1200, 600, "Pine Tree")
    place_object(50, 800, "Pine Tree")

    log("Enchanted Forest ready")

proc forest_enter():
    set_clouds(true, 0.35, 12, 60)
    set_god_rays(true, 0.18, 4)
    set_wind(0.15, 30)
    set_clear_color(0.02, 0.04, 0.02)

proc knight_merchant_chat():
    npc_face_each_other("Knight", "Merchant")
    say("Knight", "All quiet in the village today.")
    say("Merchant", "Good for business! Come see my wares.")
