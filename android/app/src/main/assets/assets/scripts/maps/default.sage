# ═══════════════════════════════════════════════════════
# Default Map Script — Crystal Quest Demo
# ═══════════════════════════════════════════════════════
#
# Uses the HUD library for reusable UI builders.
# Positions are screen-relative using hud_get("screen_w"/"screen_h").
# Works on any resolution (desktop, Android, etc.)

import hud

proc map_init():
    log("Map script loading...")

    # Get screen dimensions for relative positioning
    let sw = hud_get("screen_w")
    let sh = hud_get("screen_h")

    # Fallback if screen not yet measured (first frame)
    if sw < 100:
        sw = 960
    if sh < 100:
        sh = 720

    # ═══════════════════════════════════════
    # HUD — Positioned relative to screen edges
    # ═══════════════════════════════════════
    hud.setup_player_panel(8, 8, 340, 90)
    hud.setup_time_panel(sw - 180, 8, 170, 80)
    hud.setup_pause_menu(sw / 2, sh / 2, 180, 40)

    # Uncomment to enable survival bars:
    # hud.setup_survival_bars(8, 104, 100, 10, 4)

    # Uncomment for quest tracker (top-center):
    # hud.setup_quest_tracker(sw / 2 - 100, 8, "Explore the village")

    # ═══════════════════════════════════════
    # NPC Setup
    # ═══════════════════════════════════════
    npc_set_schedule("Merchant", 6, 20)
    npc_set_spawn_point("Merchant", 512, 256)

    npc_add_waypoint("Elder", 320, 256)
    npc_add_waypoint("Elder", 400, 256)
    npc_add_waypoint("Elder", 400, 320)
    npc_add_waypoint("Elder", 320, 320)
    npc_set_route("Elder", "patrol")
    npc_start_route("Elder")

    npc_on_meet("Elder", "Merchant", "elder_merchant_chat")

    log("Map setup complete (screen: " + str(sw) + "x" + str(sh) + ")")

# ═══════════════════════════════════════
# NPC Interaction Callbacks
# ═══════════════════════════════════════
proc elder_merchant_chat():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "Good morning! How is business?")
    say("Merchant", "Sales are good today, Elder!")
