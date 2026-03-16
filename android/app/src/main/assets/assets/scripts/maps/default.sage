# Default Map Script — Crystal Quest Demo
# HUD + Pause Menu

proc map_init():
    log("Map script loading...")

    # ══════════════════════════════════════
    # HUD — Player info (top-left)
    # ══════════════════════════════════════
    ui_panel("hud_player_bg", 8, 8, 300, 80, "flat_blue")

    # Name + Level
    ui_label("hud_name", "Mage  Lv.1", 18, 14, 1, 1, 1, 1)
    ui_set("hud_name", "scale", 1.0)

    # HP heart + bar + text
    ui_image("hud_heart", 16, 44, 18, 18, "fi_6")
    ui_bar("hud_hp", 100, 100, 38, 46, 180, 14, 0.2, 0.8, 0.2, 1)
    ui_label("hud_hp_text", "100/100", 224, 44, 1, 1, 1, 1)
    ui_set("hud_hp_text", "scale", 0.7)

    # Gold
    ui_image("hud_coin", 224, 14, 16, 16, "fi_132")
    ui_label("hud_gold", "200", 244, 14, 1, 0.95, 0.3, 1)
    ui_set("hud_gold", "scale", 0.8)

    # ══════════════════════════════════════
    # HUD — Time (top-right)
    # ══════════════════════════════════════
    let tw = hud_get("screen_w")
    if tw < 100:
        tw = 960
    let tx = tw - 160
    ui_panel("hud_time_bg", tx, 8, 150, 70, "flat_orange")
    ui_label("hud_time", "8:00 AM", tx + 12, 14, 1, 1, 0.9, 1)
    ui_set("hud_time", "scale", 1.0)
    ui_label("hud_period", "Morning", tx + 12, 38, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.7)
    ui_image("hud_sun", tx + 112, 14, 24, 24, "fi_281")

    # ══════════════════════════════════════
    # PAUSE MENU
    # ══════════════════════════════════════
    let sw = hud_get("screen_w")
    let sh = hud_get("screen_h")
    if sw < 100:
        sw = 960
    if sh < 100:
        sh = 720

    let px = sw / 2 - 140
    let py = sh / 2 - 170

    # Window panel background
    ui_panel("pause_bg", px, py, 280, 360, "panel_window")

    # Title
    ui_image("pause_icon", px + 24, py + 20, 24, 24, "fi_65")
    ui_label("pause_title", "PAUSED", px + 56, py + 20, 1, 0.9, 0.5, 1)
    ui_set("pause_title", "scale", 1.4)

    # Menu items
    let ix = px + 64
    let icx = px + 32
    let iy = py + 64
    let gap = 40

    ui_image("pause_icon_0", icx, iy, 22, 22, "fi_16")
    ui_label("pause_item_0", "Resume", ix, iy + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_0", "scale", 1.0)

    ui_image("pause_icon_1", icx, iy + gap, 22, 22, "fi_37")
    ui_label("pause_item_1", "Editor", ix, iy + gap + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_1", "scale", 1.0)

    ui_image("pause_icon_2", icx, iy + gap * 2, 22, 22, "fi_119")
    ui_label("pause_item_2", "Levels", ix, iy + gap * 2 + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_2", "scale", 1.0)

    ui_image("pause_icon_3", icx, iy + gap * 3, 22, 22, "fi_20")
    ui_label("pause_item_3", "Reset", ix, iy + gap * 3 + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_3", "scale", 1.0)

    ui_image("pause_icon_4", icx, iy + gap * 4, 22, 22, "fi_36")
    ui_label("pause_item_4", "Settings", ix, iy + gap * 4 + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_4", "scale", 1.0)

    ui_image("pause_icon_5", icx, iy + gap * 5, 22, 22, "fi_0")
    ui_label("pause_item_5", "Quit", ix, iy + gap * 5 + 1, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_5", "scale", 1.0)

    # Cursor
    ui_image("pause_cursor", icx - 4, iy - 2, 26, 26, "fi_66")

    # Hide all pause elements
    ui_set("pause_bg", "visible", false)
    ui_set("pause_icon", "visible", false)
    ui_set("pause_title", "visible", false)
    ui_set("pause_cursor", "visible", false)
    let i = 0
    while i < 6:
        ui_set("pause_item_" + str(i), "visible", false)
        ui_set("pause_icon_" + str(i), "visible", false)
        i = i + 1

    # ══════════════════════════════════════
    # NPC setup
    # ══════════════════════════════════════
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

proc elder_merchant_chat():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "Good morning! How is business?")
    say("Merchant", "Sales are good today, Elder!")
