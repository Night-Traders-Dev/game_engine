#!/usr/bin/env python3
"""Targeted exploration: capture menu, dialogue, editor properly, map edges."""

import sys
import time
sys.path.insert(0, ".")

from tw_test.engine import GameEngine
from tw_test.input_sim import InputController
from tw_test.capture import ScreenCapture
from tw_test import keys
from pathlib import Path

OUTPUT = Path("explore_output2")
OUTPUT.mkdir(exist_ok=True)

engine = GameEngine()
inp = InputController()

try:
    gw = engine.launch()
    cap = ScreenCapture(gw)

    def snap(name, delay=0.5):
        time.sleep(delay)
        img = cap.screenshot()
        path = OUTPUT / f"{name}.png"
        img.save(str(path))
        print(f"  [{name}] {img.size[0]}x{img.size[1]}")
        return img

    # --- 1. HUD close-up: Initial state with all elements visible ---
    snap("01_hud_initial", delay=1.5)

    # --- 2. Walk to find the Elder NPC and interact ---
    # From the initial screenshots, Elder was visible near center-right
    # Walk up and right to find Elder
    inp.key_hold(keys.MOVE_UP, 1.0)
    inp.key_hold(keys.MOVE_RIGHT, 0.5)
    snap("02_near_elder")

    # Face up toward Elder and interact
    inp.key_hold(keys.MOVE_UP, 0.3)
    time.sleep(0.1)
    inp.key_tap(keys.CONFIRM)
    snap("03_elder_dialogue", delay=0.8)

    # Advance dialogue
    inp.key_tap(keys.CONFIRM)
    snap("04_elder_dialogue_2", delay=0.5)

    inp.key_tap(keys.CONFIRM)
    snap("05_elder_dialogue_3", delay=0.5)

    inp.key_tap(keys.CONFIRM)
    snap("06_after_elder", delay=0.5)

    # Dismiss any remaining dialogue
    inp.key_tap(keys.CONFIRM)
    time.sleep(0.3)
    inp.key_tap(keys.CONFIRM)
    time.sleep(0.3)

    # --- 3. Open pause menu properly ---
    inp.key_tap(keys.MENU)
    snap("07_pause_menu", delay=0.8)

    # Navigate menu (try down arrow)
    inp.key_tap(keys.MOVE_DOWN)
    snap("08_menu_nav_down", delay=0.3)

    inp.key_tap(keys.MOVE_DOWN)
    snap("09_menu_nav_down2", delay=0.3)

    # Close menu with Escape
    inp.key_tap(keys.MENU)
    snap("10_menu_closed", delay=0.5)

    # --- 4. Open editor and explore it properly ---
    inp.key_tap(keys.EDITOR_TOGGLE)
    snap("11_editor_tools", delay=1.0)

    # Click on different editor tabs
    # Asset palette tabs are at top: Tiles, Buildings, Furniture, Characters, Trees, Vehicles, Misc
    # Based on screenshots they're around y=130, starting at x=245
    inp.mouse_click(1, 310, 130)  # Buildings tab
    snap("12_editor_buildings", delay=0.5)

    inp.mouse_click(1, 390, 130)  # Furniture tab
    snap("13_editor_furniture", delay=0.5)

    inp.mouse_click(1, 480, 130)  # Characters tab
    snap("14_editor_characters", delay=0.5)

    inp.mouse_click(1, 550, 130)  # Trees tab
    snap("15_editor_trees", delay=0.5)

    # Click back to Tiles
    inp.mouse_click(1, 245, 130)
    snap("16_editor_tiles", delay=0.3)

    # Close editor with ESC (not Tab - Tab doesn't work when ImGui has focus)
    inp.key_tap(keys.MENU)
    snap("17_editor_closed", delay=0.5)

    # --- 5. Try to find the merchant NPC ---
    # Walk around the map more thoroughly
    inp.key_hold(keys.MOVE_DOWN, 2.0)
    inp.key_hold(keys.MOVE_RIGHT, 2.0)
    snap("18_exploring_SE")

    # Try interacting wherever we are
    inp.key_tap(keys.CONFIRM)
    snap("19_interact_SE", delay=0.5)
    inp.key_tap(keys.CONFIRM)
    time.sleep(0.2)

    # --- 6. Sprint to all 4 map edges (without editor) to see map boundaries ---
    # Go to far right
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 6.0)
    inp.key_up(keys.RUN)
    snap("20_east_edge")

    # Go to far bottom-right corner
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 6.0)
    inp.key_up(keys.RUN)
    snap("21_SE_corner")

    # Go to far left
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 12.0)
    inp.key_up(keys.RUN)
    snap("22_west_edge")

    # Go to far top
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_UP, 12.0)
    inp.key_up(keys.RUN)
    snap("23_NW_corner")

    # Go to NE corner
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 12.0)
    inp.key_up(keys.RUN)
    snap("24_NE_corner")

    # --- 7. Walk toward center, find NPCs, try interactions ---
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 5.0)
    inp.key_up(keys.RUN)
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 5.0)
    inp.key_up(keys.RUN)
    snap("25_back_center")

    # Walk slowly in each direction and try to interact with NPCs
    for d in ["up", "down", "left", "right"]:
        for dist in [0.5, 1.0, 1.5]:
            inp.key_hold(keys.DIRECTIONS[d], dist)
            time.sleep(0.1)
            inp.key_tap(keys.CONFIRM)
            time.sleep(0.4)
        snap(f"26_search_{d}")
        # Dismiss any dialogue
        for _ in range(3):
            inp.key_tap(keys.CONFIRM)
            time.sleep(0.2)

    # --- 8. Check the bottom HUD bar (inventory/items) ---
    snap("27_final_hud")

    # --- 9. Open editor one more time to show the map at center ---
    inp.key_tap(keys.EDITOR_TOGGLE)
    snap("28_editor_center_map", delay=1.0)

    # Close with ESC
    inp.key_tap(keys.MENU)
    snap("29_game_restored", delay=0.5)

    count = len(list(OUTPUT.glob("*.png")))
    print(f"\nDone! {count} screenshots saved to {OUTPUT}/")

finally:
    inp.close()
    engine.stop()
