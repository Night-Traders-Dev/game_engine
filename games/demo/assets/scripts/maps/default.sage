# ═══════════════════════════════════════════════════════
# Default Map Script — Crystal Quest Demo
# ═══════════════════════════════════════════════════════
#
# Uses the HUD library for reusable UI builders.
# Edit this file to customize the map's HUD, NPCs, and game logic.
# Changes apply on Save & Reload in the Script IDE.

import hud

proc map_init():
    log("Map script loading...")

    # ═══════════════════════════════════════
    # HUD — Built using library functions
    # Change positions/sizes by editing the arguments
    # ═══════════════════════════════════════
    hud.setup_player_panel(8, 8, 340, 90)
    hud.setup_time_panel(780, 8, 170, 80)
    hud.setup_pause_menu(480, 300, 180, 40)

    # Uncomment to enable survival bars:
    # hud.setup_survival_bars(8, 104, 100, 10, 4)

    # Uncomment for quest tracker:
    # hud.setup_quest_tracker(380, 8, "Explore the village")

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

    log("Map setup complete")

# ═══════════════════════════════════════
# NPC Interaction Callbacks
# ═══════════════════════════════════════
proc elder_merchant_chat():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "Good morning! How is business?")
    say("Merchant", "Sales are good today, Elder!")
