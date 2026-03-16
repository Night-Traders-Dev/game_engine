#!/usr/bin/env python3
"""Explore the game visually - capture screenshots of all major UI states."""

import sys
import time
sys.path.insert(0, ".")

from tw_test.engine import GameEngine
from tw_test.input_sim import InputController
from tw_test.capture import ScreenCapture
from tw_test import keys
from pathlib import Path

OUTPUT = Path("explore_output")
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

    # 1. Initial state - default map, HUD
    snap("01_initial_state", delay=1.0)

    # 2. Walk around to see the map
    inp.key_hold(keys.MOVE_RIGHT, 1.5)
    snap("02_walk_right")

    inp.key_hold(keys.MOVE_DOWN, 1.5)
    snap("03_walk_down")

    inp.key_hold(keys.MOVE_LEFT, 2.0)
    snap("04_walk_left")

    inp.key_hold(keys.MOVE_UP, 2.0)
    snap("05_walk_up")

    # 3. Try confirm interaction
    inp.key_tap(keys.CONFIRM)
    snap("06_confirm_press")
    inp.key_tap(keys.CONFIRM)
    time.sleep(0.3)
    inp.key_tap(keys.CONFIRM)
    snap("07_after_dismiss")

    # 4. Open the pause menu
    inp.key_tap(keys.MENU)
    snap("08_menu_open", delay=0.5)
    inp.key_tap(keys.MENU)
    snap("09_menu_closed", delay=0.3)

    # 5. Toggle editor
    inp.key_tap(keys.EDITOR_TOGGLE)
    snap("10_editor_open", delay=0.8)

    inp.key_hold(keys.MOVE_RIGHT, 1.0)
    snap("11_editor_move_right")

    inp.key_hold(keys.MOVE_DOWN, 1.0)
    snap("12_editor_move_down")

    # Click in editor
    inp.mouse_click(1, 480, 360)
    snap("13_editor_center_click")

    # Close editor
    inp.key_tap(keys.EDITOR_TOGGLE)
    snap("14_editor_closed", delay=0.5)

    # 6. Sprint to explore map edges
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 4.0)
    inp.key_up(keys.RUN)
    snap("15_sprint_right")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 4.0)
    inp.key_up(keys.RUN)
    snap("16_sprint_down")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 8.0)
    inp.key_up(keys.RUN)
    snap("17_sprint_left")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_UP, 8.0)
    inp.key_up(keys.RUN)
    snap("18_sprint_up")

    # Open editor at edge
    inp.key_tap(keys.EDITOR_TOGGLE)
    snap("19_editor_at_edge", delay=0.8)
    inp.key_tap(keys.EDITOR_TOGGLE)
    time.sleep(0.3)

    # 7. Back to center and interact in each direction
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 4.0)
    inp.key_up(keys.RUN)
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 4.0)
    inp.key_up(keys.RUN)
    snap("20_back_center")

    for d in ["up", "right", "down", "left"]:
        inp.key_hold(keys.DIRECTIONS[d], 0.8)
        time.sleep(0.1)
        inp.key_tap(keys.CONFIRM)
        snap(f"21_interact_{d}", delay=0.5)
        inp.key_tap(keys.CONFIRM)
        time.sleep(0.2)
        inp.key_tap(keys.CONFIRM)
        time.sleep(0.2)

    count = len(list(OUTPUT.glob("*.png")))
    print(f"\nDone! {count} screenshots saved to {OUTPUT}/")

finally:
    inp.close()
    engine.stop()
