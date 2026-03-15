# ═══════════════════════════════════════════════════════
# Default Map Script — Crystal Quest Demo
# ═══════════════════════════════════════════════════════
#
# This file is the source of truth for all game logic on the default map.
# It is auto-executed on game start and also editable from the Script IDE.
# The editor auto-appends lines when you spawn NPCs or place objects.
#
# JSON handles tile/collision data. This script handles everything else.

proc map_init():
    log("Default map script loaded")

    # ── NPCs ──
    # These mirror game.json NPCs — edit here to modify at runtime
    # (game.json NPCs load first, these can add/modify on top)

    # spawn_npc(name, x, y, dir, hostile, sprite_id, hp, atk, speed, aggro)
    # Elder: sprite_id=0, friendly, slow wanderer
    # spawn_npc("Elder", 320, 256, 0, false, 0, 0, 0, 20, 40)
    # Merchant: sprite_id=1, friendly, shop keeper
    # spawn_npc("Merchant", 512, 256, 0, false, 1, 0, 0, 15, 40)
    # Skeleton: sprite_id=2, hostile, battle enemy
    # spawn_npc("Skeleton", 640, 384, 0, true, 2, 50, 12, 45, 140)
    # Slime: sprite_id=3, hostile, battle enemy
    # spawn_npc("Slime", 400, 450, 0, true, 3, 30, 6, 30, 100)

    # ── NPC Schedules ──
    npc_set_schedule("Merchant", 6, 20)
    npc_set_spawn_point("Merchant", 512, 256)

    # ── NPC Routes ──
    # Elder patrols the village center
    npc_add_waypoint("Elder", 320, 256)
    npc_add_waypoint("Elder", 400, 256)
    npc_add_waypoint("Elder", 400, 320)
    npc_add_waypoint("Elder", 320, 320)
    npc_set_route("Elder", "patrol")
    npc_start_route("Elder")

    # ── Portals ──
    # set_portal(tile_x, tile_y, target_map, target_x, target_y, label)

    # ── Collision Overrides ──
    # set_collision(tile_x, tile_y, type)  # 0=None, 1=Solid, 2=Portal

    # ── World Objects ──
    # place_object(world_x, world_y, stamp_name)

    # ── Custom HUD / UI Components ──
    # Quest tracker (top-center)
    #ui_panel("quest_bg", 380, 8, 200, 40, "panel_mini")
    #ui_image("quest_icon", 388, 14, 24, 24, "icon_book")
    #ui_label("quest_text", "Explore the village", 418, 16, 0.9, 0.85, 0.7, 1)
    #ui_set("quest_text", "scale", 0.65)

    # ── Day-Night Driven Spawning ──
    # (Configured in day.sage via init_day_night)

    # ── NPC Interactions ──
    npc_on_meet("Elder", "Merchant", "elder_merchant_chat")

    log("Map setup complete")

# ── NPC Interaction Callbacks ──
proc elder_merchant_chat():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "Good morning! How is business?")
    say("Merchant", "Sales are good today, Elder!")
