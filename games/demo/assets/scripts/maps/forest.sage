# Forest Map Script — Enchanted Forest
# House center, dense tree border, dirt paths, small pond

proc forest_init():
    ui_set("hud_player_bg", "x", 12)
    ui_set("hud_player_bg", "y", 12)
    ui_set("hud_player_bg", "sprite", "flat_blue")
    ui_set("hud_time_bg", "x", 1698)
    ui_set("hud_time_bg", "y", 12)
    ui_set("hud_time_bg", "sprite", "flat_blue")
    ui_set("pause_bg", "sprite", "flat_orange")
    ui_remove("pause_div")
    ui_set("log_bg", "x", 736)
    ui_set("log_bg", "y", 901)
    ui_set("log_bg", "x", 730)
    ui_set("log_bg", "y", 896)
    ui_set("log_bg", "w", 630)
    ui_set("log_bg", "h", 128)
    place_object(86, 707, "Oak Tree")
    place_object(278, 707, "Oak Tree")
    place_object(566, 739, "Oak Tree")
    place_object(598, 707, "Oak Tree")
    place_object(854, 707, "Oak Tree")
    place_object(726, 707, "Oak Tree")
    place_object(406, 707, "Oak Tree")
# Called EVERY time the player enters this level
proc forest_enter():
    set_clouds(true, 0.35, 12, 60)
    set_god_rays(true, 0.18, 4)
    set_wind(0.15, 30)
    set_clear_color(0.02, 0.04, 0.02)
