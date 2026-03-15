# Default map script — auto-executed on game start
# This file configures the starting map's UI, NPCs, objects, and game logic.
# The editor auto-appends lines when you spawn NPCs or place objects.

proc map_init():
    log("Default map loaded")

    # ── Custom UI Components ──
    # These use the script UI API to create HUD elements from .sage

    # Example: Quest tracker panel (top-center)
    ui_panel("quest_bg", 380, 8, 200, 40, "panel_mini")
    ui_image("quest_icon", 388, 14, 24, 24, "icon_book")
    ui_label("quest_text", "Explore the village", 418, 16, 0.9, 0.85, 0.7, 1)
    ui_set("quest_text", "scale", 0.65)

    # Example: compass indicator (below time panel)
    # ui_panel("compass_bg", 820, 70, 60, 24, "panel_mini")
    # ui_label("compass", "N", 842, 74, 0.8, 0.8, 0.8, 1)
