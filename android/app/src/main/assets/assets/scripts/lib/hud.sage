# ═══════════════════════════════════════
# HUD Library — Reusable UI component builders
# ═══════════════════════════════════════
# Material Design color palette:
#   Panel:     "panel_window" (Blue-Grey 900 via atlas)
#   Primary:   1, 1, 1, 0.92 (white 92%)
#   Secondary: 0.78, 0.82, 0.86, 0.85 (Blue-Grey 200)
#   Accent:    1, 0.84, 0.31, 1 (Amber 300)
#   HP Green:  0.26, 0.63, 0.28, 1 (Green 700)
#   Danger:    0.94, 0.33, 0.31 (Red 400)
#   Bar BG:    0.10, 0.10, 0.14, 0.90

proc setup_player_panel(x, y, w, h):
    ui_panel("hud_player_bg", x, y, w, h, "panel_window")
    ui_label("hud_name", "Mage  Lv.1", x + 14, y + 8, 1, 1, 1, 0.92)
    ui_set("hud_name", "scale", 0.9)
    ui_image("hud_heart", x + 12, y + 38, 16, 16, "fi_6")
    ui_bar("hud_hp", 100, 100, x + 34, y + 40, w - 100, 12, 0.26, 0.63, 0.28, 1)
    ui_set("hud_hp", "bg_r", 0.10)
    ui_set("hud_hp", "bg_g", 0.10)
    ui_set("hud_hp", "bg_b", 0.14)
    ui_set("hud_hp", "bg_a", 0.90)
    ui_label("hud_hp_text", "100/100", x + w - 55, y + 38, 0.75, 0.78, 0.82, 0.80)
    ui_set("hud_hp_text", "scale", 0.6)
    ui_image("hud_coin", x + w - 70, y + 8, 14, 14, "fi_132")
    ui_label("hud_gold", "200", x + w - 52, y + 10, 1, 0.84, 0.31, 1)
    ui_set("hud_gold", "scale", 0.7)
    log("HUD: Player panel created")

proc setup_time_panel(x, y, w, h):
    ui_panel("hud_time_bg", x, y, w, h, "panel_window")
    ui_image("hud_sun", x + 8, y + 6, 20, 20, "fi_281")
    ui_label("hud_time", "8:00 AM", x + 32, y + 8, 1, 1, 0.95, 0.92)
    ui_set("hud_time", "scale", 0.85)
    ui_label("hud_period", "Morning", x + 32, y + 30, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.6)
    log("HUD: Time panel created")

proc setup_survival_bars(x, y, w, h, gap):
    ui_bar("hud_hunger", 100, 100, x, y, w, h, 0.85, 0.55, 0.15, 0.9)
    ui_set("hud_hunger", "bg_r", 0.10)
    ui_set("hud_hunger", "bg_g", 0.10)
    ui_set("hud_hunger", "bg_b", 0.14)
    ui_bar("hud_thirst", 100, 100, x, y + h + gap, w, h, 0.26, 0.54, 0.90, 0.9)
    ui_set("hud_thirst", "bg_r", 0.10)
    ui_set("hud_thirst", "bg_g", 0.10)
    ui_set("hud_thirst", "bg_b", 0.14)
    ui_bar("hud_energy", 100, 100, x, y + (h + gap) * 2, w, h, 0.90, 0.80, 0.20, 0.9)
    ui_set("hud_energy", "bg_r", 0.10)
    ui_set("hud_energy", "bg_g", 0.10)
    ui_set("hud_energy", "bg_b", 0.14)
    log("HUD: Survival bars created")

proc setup_pause_menu(cx, cy, w, spacing):
    ui_panel("pause_bg", cx - w / 2 - 10, cy - 40, w + 20, spacing * 5 + 80, "panel_window")
    ui_label("pause_title", "PAUSED", cx - 30, cy - 22, 1, 0.84, 0.31, 1)
    ui_set("pause_title", "scale", 1.3)

    # Secondary blue-grey for items; Quit in dim red
    ui_label("pause_item_0", "Resume Game", cx - 45, cy + 30, 0.78, 0.82, 0.86, 0.85)
    ui_label("pause_item_1", "Editor Mode", cx - 45, cy + 30 + spacing, 0.78, 0.82, 0.86, 0.85)
    ui_label("pause_item_2", "Reset", cx - 20, cy + 30 + spacing * 2, 0.78, 0.82, 0.86, 0.85)
    ui_label("pause_item_3", "Settings", cx - 30, cy + 30 + spacing * 3, 0.78, 0.82, 0.86, 0.85)
    ui_label("pause_item_4", "Quit", cx - 15, cy + 30 + spacing * 4, 0.94, 0.33, 0.31, 0.70)
    ui_set("pause_item_0", "scale", 1.0)
    ui_set("pause_item_1", "scale", 1.0)
    ui_set("pause_item_2", "scale", 1.0)
    ui_set("pause_item_3", "scale", 1.0)
    ui_set("pause_item_4", "scale", 1.0)

    ui_image("pause_cursor", cx - 70, cy + 30, 20, 20, "fi_66")

    ui_set("pause_bg", "visible", false)
    ui_set("pause_title", "visible", false)
    ui_set("pause_item_0", "visible", false)
    ui_set("pause_item_1", "visible", false)
    ui_set("pause_item_2", "visible", false)
    ui_set("pause_item_3", "visible", false)
    ui_set("pause_item_4", "visible", false)
    ui_set("pause_cursor", "visible", false)
    log("HUD: Pause menu created")

proc setup_quest_tracker(x, y, text):
    ui_panel("quest_bg", x, y, 200, 40, "panel_window")
    ui_image("quest_icon", x + 8, y + 6, 24, 24, "fi_119")
    ui_label("quest_text", text, x + 38, y + 8, 0.78, 0.82, 0.86, 0.90)
    ui_set("quest_text", "scale", 0.65)
    log("HUD: Quest tracker created")
