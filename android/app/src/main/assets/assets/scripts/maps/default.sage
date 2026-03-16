# Default Map Script — Crystal Quest Demo
# HUD + Pause Menu using Fantasy Icons + Flat UI panels

proc map_init():
    log("Map script loading...")

    # ══════════════════════════════════════
    # HUD — Top-left player panel (blue flat theme)
    # ══════════════════════════════════════
    ui_panel("hud_player_bg", 8, 8, 340, 90, "flat_blue")

    # Player name + level
    ui_label("hud_name", "Mage  Lv.1", 22, 16, 1, 1, 1, 1)
    ui_set("hud_name", "scale", 1.1)

    # HP: fi_6 = pink heart
    ui_image("hud_heart", 20, 48, 22, 22, "fi_6")
    ui_bar("hud_hp", 100, 100, 46, 52, 200, 16, 0.2, 0.8, 0.2, 1)
    ui_label("hud_hp_text", "100/100", 255, 50, 1, 1, 1, 1)
    ui_set("hud_hp_text", "scale", 0.8)

    # Gold: fi_132 = gold item
    ui_image("hud_coin", 250, 16, 20, 20, "fi_132")
    ui_label("hud_gold", "200", 275, 18, 1, 0.95, 0.3, 1)
    ui_set("hud_gold", "scale", 0.9)

    # ══════════════════════════════════════
    # HUD — Top-right time panel (orange flat theme)
    # ══════════════════════════════════════
    let tw = hud_get("screen_w")
    if tw < 100:
        tw = 960
    let tx = tw - 180
    ui_panel("hud_time_bg", tx, 8, 170, 80, "flat_orange")
    ui_label("hud_time", "8:00 AM", tx + 14, 18, 1, 1, 0.9, 1)
    ui_set("hud_time", "scale", 1.1)
    ui_label("hud_period", "Morning", tx + 14, 46, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.8)

    # Sun/moon: fi_281 = sun, fi_280 = moon
    ui_image("hud_sun", tx + 130, 18, 28, 28, "fi_281")

    # ══════════════════════════════════════
    # PAUSE MENU — Dark flat panel, fantasy icons
    # ══════════════════════════════════════
    let sw = hud_get("screen_w")
    let sh = hud_get("screen_h")
    if sw < 100:
        sw = 960
    if sh < 100:
        sh = 720
    let px = sw / 2 - 150
    let py = sh / 2 - 180

    # Warm window panel with dark border
    ui_panel("pause_bg", px, py, 300, 380, "panel_window")

    # Title: fi_65 = sword icon
    ui_image("pause_icon", px + 20, py + 16, 28, 28, "fi_65")
    ui_label("pause_title", "PAUSED", px + 56, py + 18, 1, 0.9, 0.5, 1)
    ui_set("pause_title", "scale", 1.5)

    # Menu items with icons
    let item_x = px + 70
    let icon_x = px + 32
    let item_start = py + 70
    let item_gap = 42

    # 0: Resume — fi_16 = green up arrow
    ui_image("pause_icon_0", icon_x, item_start, 24, 24, "fi_16")
    ui_label("pause_item_0", "Resume", item_x, item_start + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_0", "scale", 1.1)

    # 1: Editor — fi_37 = crossed hammers/anvil
    ui_image("pause_icon_1", icon_x, item_start + item_gap, 24, 24, "fi_37")
    ui_label("pause_item_1", "Editor", item_x, item_start + item_gap + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_1", "scale", 1.1)

    # 2: Levels — fi_119 = map/document
    ui_image("pause_icon_2", icon_x, item_start + item_gap * 2, 24, 24, "fi_119")
    ui_label("pause_item_2", "Levels", item_x, item_start + item_gap * 2 + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_2", "scale", 1.1)

    # 3: Reset — fi_20 = refresh/cycle arrows
    ui_image("pause_icon_3", icon_x, item_start + item_gap * 3, 24, 24, "fi_20")
    ui_label("pause_item_3", "Reset", item_x, item_start + item_gap * 3 + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_3", "scale", 1.1)

    # 4: Settings — fi_36 = gear/pickaxe
    ui_image("pause_icon_4", icon_x, item_start + item_gap * 4, 24, 24, "fi_36")
    ui_label("pause_item_4", "Settings", item_x, item_start + item_gap * 4 + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_4", "scale", 1.1)

    # 5: Quit — fi_0 = skull
    ui_image("pause_icon_5", icon_x, item_start + item_gap * 5, 24, 24, "fi_0")
    ui_label("pause_item_5", "Quit", item_x, item_start + item_gap * 5 + 2, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_5", "scale", 1.1)

    # Cursor: fi_66 = sword icon for selection
    ui_image("pause_cursor", icon_x - 6, item_start - 2, 28, 28, "fi_66")

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
