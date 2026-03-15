# Default Map Script — Crystal Quest Demo

proc map_init():
    log("Map script loading...")

    # Player panel
    ui_panel("hud_player_bg", 8, 8, 340, 90, "panel_hud_wide")
    ui_label("hud_name", "Mage  Lv.1", 22, 16, 1, 1, 1, 1)
    ui_set("hud_name", "scale", 1.1)
    ui_image("hud_heart", 20, 48, 20, 20, "icon_heart_red")
    ui_bar("hud_hp", 100, 100, 46, 52, 200, 16, 0.2, 0.8, 0.2, 1)
    ui_label("hud_hp_text", "100/100", 255, 50, 1, 1, 1, 1)
    ui_set("hud_hp_text", "scale", 0.8)
    ui_image("hud_coin", 250, 16, 20, 20, "icon_coin")
    ui_label("hud_gold", "200", 275, 18, 1, 0.95, 0.3, 1)
    ui_set("hud_gold", "scale", 0.9)

    # Time panel
    ui_panel("hud_time_bg", 780, 8, 170, 80, "panel_hud_sq")
    ui_label("hud_time", "8:00 AM", 794, 18, 1, 1, 0.9, 1)
    ui_set("hud_time", "scale", 1.1)
    ui_label("hud_period", "Morning", 794, 46, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.8)
    ui_image("hud_sun", 910, 18, 26, 26, "icon_star")

    # Pause menu (hidden by default)
    ui_panel("pause_bg", 370, 260, 220, 280, "panel_large")
    ui_label("pause_title", "PAUSED", 428, 278, 1, 0.9, 0.5, 1)
    ui_set("pause_title", "scale", 1.3)
    ui_label("pause_item_0", "Resume Game", 408, 320, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_1", "Editor Mode", 408, 360, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_2", "Reset", 432, 400, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_3", "Settings", 422, 440, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_4", "Quit", 440, 480, 0.85, 0.82, 0.75, 1)
    ui_image("pause_cursor", 380, 320, 20, 20, "cursor_box")
    ui_set("pause_bg", "visible", false)
    ui_set("pause_title", "visible", false)
    ui_set("pause_item_0", "visible", false)
    ui_set("pause_item_1", "visible", false)
    ui_set("pause_item_2", "visible", false)
    ui_set("pause_item_3", "visible", false)
    ui_set("pause_item_4", "visible", false)
    ui_set("pause_cursor", "visible", false)

    # NPC setup
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
