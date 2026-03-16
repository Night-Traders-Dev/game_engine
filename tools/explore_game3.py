#!/usr/bin/env python3
"""Exploration v3: Hybrid XTest+uinput input, full UI state capture, editor asset tabs."""

import sys
import time
sys.path.insert(0, ".")

from tw_test.engine import GameEngine
from tw_test.input_sim import InputController
from tw_test.capture import ScreenCapture
from tw_test import keys
from pathlib import Path

OUTPUT = Path("explore_output3")
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

    # ──── PART 1: Verify movement ────
    snap("01_initial", delay=2.0)

    # Use key_hold (uinput) for movement — hold for longer
    inp.key_hold(keys.MOVE_RIGHT, 2.0)
    snap("02_after_right")

    inp.key_hold(keys.MOVE_DOWN, 2.0)
    snap("03_after_down")

    # Also try XTest tap for movement comparison
    for _ in range(20):
        inp.key_tap(keys.MOVE_LEFT, duration=0.1)
        time.sleep(0.05)
    snap("04_after_left_taps")

    for _ in range(20):
        inp.key_tap(keys.MOVE_UP, duration=0.1)
        time.sleep(0.05)
    snap("05_after_up_taps")

    # ──── PART 2: Menu (Escape via XTest) ────
    inp.key_tap(keys.MENU, duration=0.1)
    snap("06_menu", delay=0.8)

    # Close menu
    inp.key_tap(keys.MENU, duration=0.1)
    snap("07_menu_closed", delay=0.5)

    # ──── PART 3: NPC dialogue (Z via XTest) ────
    # Walk toward the Elder NPC first
    inp.key_hold(keys.MOVE_UP, 1.0)
    time.sleep(0.1)
    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("08_interact", delay=0.8)

    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("09_dialogue_advance", delay=0.5)

    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("10_dialogue_advance2", delay=0.5)

    # Dismiss
    for _ in range(5):
        inp.key_tap(keys.CONFIRM, duration=0.1)
        time.sleep(0.3)

    snap("11_after_dialogue")

    # ──── PART 4: Editor + ALL asset tabs ────
    inp.key_tap(keys.EDITOR_TOGGLE, duration=0.1)
    snap("12_editor_tiles", delay=1.0)

    # The editor has tabs: Tiles, Buildings, Furniture, Characters, Trees, Vehicles, Misc
    # From the first run's editor screenshot, the tabs are in the Assets panel header
    # Approximate tab positions (adjusted for 1010x807 window with title bar ~47px):
    # Title bar offset is about 47px. Tab row is at about y=130 in screenshot coords.
    # Tiles ~245, Buildings ~310, Furniture ~390, Characters ~480, Trees ~550, Vehicles ~620, Misc ~675

    tab_positions = {
        "Buildings": 310,
        "Furniture": 390,
        "Characters": 480,
        "Trees": 550,
        "Vehicles": 620,
        "Misc": 675,
    }

    for tab_name, tab_x in tab_positions.items():
        inp.mouse_click(1, tab_x, 130)
        snap(f"13_editor_{tab_name.lower()}", delay=0.5)

    # Click back to Tiles
    inp.mouse_click(1, 245, 130)
    snap("14_editor_tiles_again", delay=0.3)

    # Scroll down in tile palette to see more tiles
    inp.mouse_move(400, 250)
    time.sleep(0.1)
    inp.mouse_scroll(-5, 400, 250)
    snap("15_tiles_scrolled", delay=0.5)

    # Try clicking on editor tool buttons (left panel)
    # Tools panel has: Paint, Erase, Fill, Eyedrop, Select, Collision, Line, Rectangle, Portal
    # Toggle collision view
    inp.mouse_click(1, 165, 197)  # Collision button
    snap("16_collision_view", delay=0.5)

    # Toggle grid off
    inp.mouse_click(1, 60, 295)  # Grid checkbox
    snap("17_grid_off", delay=0.3)

    # Toggle grid back on
    inp.mouse_click(1, 60, 295)
    time.sleep(0.2)

    # Click on collision toggle
    inp.mouse_click(1, 165, 295)  # Collision checkbox
    snap("18_collision_overlay", delay=0.5)

    # Close editor with ESC
    inp.key_tap(keys.MENU, duration=0.1)
    snap("19_back_to_game", delay=0.5)

    # ──── PART 5: Map exploration ────
    # Sprint to see different areas
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 3.0)
    inp.key_up(keys.RUN)
    snap("20_east")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 3.0)
    inp.key_up(keys.RUN)
    snap("21_southeast")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 6.0)
    inp.key_up(keys.RUN)
    snap("22_southwest")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_UP, 6.0)
    inp.key_up(keys.RUN)
    snap("23_northwest")

    # ──── PART 6: HUD detail ────
    # Back to center
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 3.0)
    inp.key_up(keys.RUN)
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 3.0)
    inp.key_up(keys.RUN)
    snap("24_center_hud")

    # Open menu for final inspection
    inp.key_tap(keys.MENU, duration=0.1)
    snap("25_final_menu", delay=0.8)

    count = len(list(OUTPUT.glob("*.png")))
    print(f"\nDone! {count} screenshots saved to {OUTPUT}/")

finally:
    inp.close()
    engine.stop()
