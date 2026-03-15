# ═══════════════════════════════════════
# HUD Library — Reusable UI component builders
# ═══════════════════════════════════════
# All positions use screen-relative coordinates.
# Usage: import hud
#   or:  from hud import setup_player_panel, setup_time_panel

proc setup_player_panel(x, y, w, h):
    ui_panel("hud_player_bg", x, y, w, h, "panel_hud_wide")
    ui_label("hud_name", "Mage  Lv.1", x + 14, y + 8, 1, 1, 1, 1)
    ui_set("hud_name", "scale", 1.1)
    ui_image("hud_heart", x + 12, y + 40, 20, 20, "icon_heart_red")
    ui_bar("hud_hp", 100, 100, x + 38, y + 44, w - 100, 16, 0.2, 0.8, 0.2, 1)
    ui_label("hud_hp_text", "100/100", x + w - 55, y + 42, 1, 1, 1, 1)
    ui_set("hud_hp_text", "scale", 0.8)
    ui_image("hud_coin", x + w - 90, y + 8, 20, 20, "icon_coin")
    ui_label("hud_gold", "200", x + w - 65, y + 10, 1, 0.95, 0.3, 1)
    ui_set("hud_gold", "scale", 0.9)
    log("HUD: Player panel created")

proc setup_time_panel(x, y, w, h):
    ui_panel("hud_time_bg", x, y, w, h, "panel_hud_sq")
    ui_label("hud_time", "8:00 AM", x + 14, y + 10, 1, 1, 0.9, 1)
    ui_set("hud_time", "scale", 1.1)
    ui_label("hud_period", "Morning", x + 14, y + 38, 1, 0.85, 0.4, 1)
    ui_set("hud_period", "scale", 0.8)
    ui_image("hud_sun", x + w - 36, y + 10, 26, 26, "icon_star")
    log("HUD: Time panel created")

proc setup_survival_bars(x, y, w, h, gap):
    ui_bar("hud_hunger", 100, 100, x, y, w, h, 0.85, 0.55, 0.15, 1)
    ui_bar("hud_thirst", 100, 100, x, y + h + gap, w, h, 0.2, 0.5, 0.9, 1)
    ui_bar("hud_energy", 100, 100, x, y + (h + gap) * 2, w, h, 0.9, 0.8, 0.2, 1)
    log("HUD: Survival bars created")

proc setup_pause_menu(cx, cy, w, spacing):
    ui_panel("pause_bg", cx - w / 2 - 10, cy - 40, w + 20, spacing * 5 + 80, "panel_large")
    ui_label("pause_title", "PAUSED", cx - 30, cy - 22, 1, 0.9, 0.5, 1)
    ui_set("pause_title", "scale", 1.3)

    ui_label("pause_item_0", "Resume Game", cx - 45, cy + 30, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_1", "Editor Mode", cx - 45, cy + 30 + spacing, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_2", "Reset", cx - 20, cy + 30 + spacing * 2, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_3", "Settings", cx - 30, cy + 30 + spacing * 3, 0.85, 0.82, 0.75, 1)
    ui_label("pause_item_4", "Quit", cx - 15, cy + 30 + spacing * 4, 0.85, 0.82, 0.75, 1)
    ui_set("pause_item_0", "scale", 1.0)
    ui_set("pause_item_1", "scale", 1.0)
    ui_set("pause_item_2", "scale", 1.0)
    ui_set("pause_item_3", "scale", 1.0)
    ui_set("pause_item_4", "scale", 1.0)

    ui_image("pause_cursor", cx - 70, cy + 30, 20, 20, "cursor_box")

    # Hide initially
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
    ui_panel("quest_bg", x, y, 200, 40, "panel_mini")
    ui_image("quest_icon", x + 8, y + 6, 24, 24, "icon_book")
    ui_label("quest_text", text, x + 38, y + 8, 0.9, 0.85, 0.7, 1)
    ui_set("quest_text", "scale", 0.65)
    log("HUD: Quest tracker created")
