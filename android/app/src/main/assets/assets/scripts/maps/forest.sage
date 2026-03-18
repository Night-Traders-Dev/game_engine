# Forest Map Script — Enchanted Forest
# House center, dense tree border, dirt paths, small pond

proc forest_init():
    ui_set("hud_player_bg", "sprite", "flat_blue")
    ui_set("hud_time_bg", "sprite", "flat_blue")
    ui_set("pause_bg", "sprite", "flat_orange")
    ui_set("hud_period", "scale", 1.782)
    ui_set("hud_time_bg", "x", 1708)
    ui_set("hud_time_bg", "y", 12)
# Called EVERY time the player enters this level
proc forest_enter():
    set_clouds(true, 0.35, 12, 60)
    set_god_rays(true, 0.18, 4)
    set_wind(0.15, 30)
    set_clear_color(0.02, 0.04, 0.02)
