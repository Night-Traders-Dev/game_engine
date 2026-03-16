#!/usr/bin/env python3
"""Quick test: verify movement + focus works, then capture key UI states."""

import sys, time
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
        print(f"  [{name}]")
        return img

    # 1. Initial (should show player in forest)
    snap("01_initial", delay=1.0)

    # 2. Test movement — walk right for 2 seconds
    inp.key_hold(keys.MOVE_RIGHT, 2.0)
    snap("02_walked_right")

    # 3. Walk down
    inp.key_hold(keys.MOVE_DOWN, 2.0)
    snap("03_walked_down")

    # 4. Walk left
    inp.key_hold(keys.MOVE_LEFT, 2.0)
    snap("04_walked_left")

    # 5. Walk up
    inp.key_hold(keys.MOVE_UP, 2.0)
    snap("05_walked_up")

    # 6. Menu (ESC via XTest tap)
    inp.key_tap(keys.MENU, duration=0.1)
    snap("06_menu", delay=0.8)

    # Close menu
    inp.key_tap(keys.MENU, duration=0.1)
    snap("07_menu_closed", delay=0.5)

    # 7. Try dialogue — walk up toward Elder and press Z
    inp.key_hold(keys.MOVE_UP, 1.5)
    time.sleep(0.1)
    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("08_dialogue", delay=0.8)

    # Advance/dismiss dialogue
    for i in range(5):
        inp.key_tap(keys.CONFIRM, duration=0.1)
        time.sleep(0.3)
    snap("09_after_dialogue")

    # 8. Editor
    inp.key_tap(keys.EDITOR_TOGGLE, duration=0.1)
    snap("10_editor", delay=1.0)

    # Close with ESC
    inp.key_tap(keys.MENU, duration=0.1)
    snap("11_editor_closed", delay=0.5)

    print("\nDone!")

finally:
    inp.close()
    engine.stop()
