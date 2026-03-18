# Default Map Script — Crystal Quest Demo
# Modern Material Design HUD + Pause Menu
#
# Color Theory:
#   Panel BG:  Blue-Grey 900 (#1F262E) via "panel_window"
#   Primary:   White 92% for key text
#   Secondary: Blue-Grey 200 (#C7D1DB) for inactive items
#   Accent:    Amber 300 (#FFD54F) for gold, highlights, selection
#   HP Green:  Green 700 (#43A047) — set dynamically by C++
#   Danger:    Red 400 (#EF5350) for quit/destructive
#   Info:      Light Blue 400 (#4FC3F7) for hints, time accent
#
# Font Rules:
#   Title:     scale 1.3 (large, bold feel)
#   Body:      scale 0.85-1.0 (readable)
#   Secondary: scale 0.6-0.7 (subdued info)
#   Tiny:      scale 0.48-0.55 (badges, hints)
#
# Layout: Z-pattern eye flow
#   Top-left  = Player vitals (most critical)
#   Top-right = Time/environment (glanceable)
#   Center    = Pause menu (modal)

proc map_init():
    log("Map script loading...")

    let sw = hud_get("screen_w")
    let sh = hud_get("screen_h")
    if sw < 100:
        sw = 960
    if sh < 100:
        sh = 720

    # ══════════════════════════════════════
    # TOP-LEFT — Player Vitals
    # ══════════════════════════════════════
    ui_panel("hud_player_bg", 6, 6, 260, 68, "flat_blue")

    # Name + Level — primary white
    ui_label("hud_name", "Mage  Lv.1", 16, 12, 1, 1, 1, 0.92)
    ui_set("hud_name", "scale", 0.9)

    # Heart icon + HP bar
    ui_image("hud_heart", 14, 38, 16, 16, "fi_6")
    ui_bar("hud_hp", 100, 100, 34, 40, 160, 12, 0.26, 0.63, 0.28, 1)
    ui_set("hud_hp", "bg_r", 0.10)
    ui_set("hud_hp", "bg_g", 0.10)
    ui_set("hud_hp", "bg_b", 0.14)
    ui_set("hud_hp", "bg_a", 0.90)

    # HP text — secondary
    ui_label("hud_hp_text", "100/100", 200, 38, 0.75, 0.78, 0.82, 0.80)
    ui_set("hud_hp_text", "scale", 0.6)

    # Coin icon + gold — amber accent
    ui_image("hud_coin", 200, 12, 14, 14, "fi_132")
    ui_label("hud_gold", "200", 218, 12, 1, 0.84, 0.31, 1)
    ui_set("hud_gold", "scale", 0.7)

    # ══════════════════════════════════════
    # TOP-RIGHT — Time & Environment
    # ══════════════════════════════════════
    let tx = sw - 140
    ui_panel("hud_time_bg", tx, 6, 132, 56, "flat_blue")
    ui_image("hud_sun", tx + 8, 10, 20, 20, "fi_281")

    # Time — primary white
    ui_label("hud_time", "8:00 AM", tx + 32, 12, 1, 1, 0.95, 0.92)
    ui_set("hud_time", "scale", 0.85)

    # Period — color set dynamically by C++ (morning=warm, night=blue)
    ui_label("hud_period", "Morning", tx + 32, 34, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.6)

    # ══════════════════════════════════════
    # PAUSE MENU — Center Modal
    # ══════════════════════════════════════
    let px = sw / 2 - 130
    let py = sh / 2 - 155

    ui_panel("pause_bg", px, py, 260, 330, "flat_orange")

    # Title — amber accent
    ui_image("pause_icon", px + 16, py + 14, 22, 22, "fi_65")
    ui_label("pause_title", "PAUSED", px + 44, py + 14, 1, 0.84, 0.31, 1)
    ui_set("pause_title", "scale", 1.3)

    # Menu items — icon + text, consistent gap
    let ix = px + 56
    let icx = px + 24
    let iy = py + 58
    let gap = 38

    # Resume — primary white (brightest = primary action)
    ui_image("pause_icon_0", icx, iy, 20, 20, "fi_16")
    ui_label("pause_item_0", "Resume", ix, iy + 1, 1, 1, 1, 0.92)
    ui_set("pause_item_0", "scale", 1.0)

    # Editor — secondary blue-grey
    ui_image("pause_icon_1", icx, iy + gap, 20, 20, "fi_37")
    ui_label("pause_item_1", "Editor", ix, iy + gap + 1, 0.78, 0.82, 0.86, 0.85)
    ui_set("pause_item_1", "scale", 1.0)

    # Levels — secondary
    ui_image("pause_icon_2", icx, iy + gap * 2, 20, 20, "fi_119")
    ui_label("pause_item_2", "Levels", ix, iy + gap * 2 + 1, 0.78, 0.82, 0.86, 0.85)
    ui_set("pause_item_2", "scale", 1.0)

    # Reset — secondary
    ui_image("pause_icon_3", icx, iy + gap * 3, 20, 20, "fi_20")
    ui_label("pause_item_3", "Reset", ix, iy + gap * 3 + 1, 0.78, 0.82, 0.86, 0.85)
    ui_set("pause_item_3", "scale", 1.0)

    # Settings — secondary
    ui_image("pause_icon_4", icx, iy + gap * 4, 20, 20, "fi_36")
    ui_label("pause_item_4", "Settings", ix, iy + gap * 4 + 1, 0.78, 0.82, 0.86, 0.85)
    ui_set("pause_item_4", "scale", 1.0)

    # Quit — dim red (destructive action, visually de-emphasized)
    ui_image("pause_icon_5", icx, iy + gap * 5, 20, 20, "fi_0")
    ui_label("pause_item_5", "Quit", ix, iy + gap * 5 + 1, 0.94, 0.33, 0.31, 0.70)
    ui_set("pause_item_5", "scale", 1.0)

    # Selection cursor
    ui_image("pause_cursor", icx - 4, iy - 2, 24, 24, "fi_66")

    # Hide all pause elements on load
    ui_set("pause_bg", "visible", false)
    ui_set("pause_icon", "visible", false)
    ui_set("pause_title", "visible", false)
    ui_set("pause_div", "visible", false)
    ui_set("pause_cursor", "visible", false)
    let i = 0
    while i < 6:
        ui_set("pause_item_" + str(i), "visible", false)
        ui_set("pause_icon_" + str(i), "visible", false)
        i = i + 1

    # ══════════════════════════════════════
    # NPC Setup
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
