#!/usr/bin/env python3
"""Full exploration: maximized window, all UI states, editor asset tabs, map edges."""

import sys, time
sys.path.insert(0, ".")

from tw_test.engine import GameEngine
from tw_test.input_sim import InputController
from tw_test.capture import ScreenCapture
from tw_test import keys
from pathlib import Path

OUTPUT = Path("explore_full")
# Clean previous run
for f in Path("explore_full").glob("*.png"):
    f.unlink()
OUTPUT.mkdir(exist_ok=True)

engine = GameEngine()
inp = InputController()

try:
    gw = engine.launch()
    cap = ScreenCapture(gw)
    geo = gw.geometry()
    W, H = geo[2], geo[3]
    print(f"  Window: {W}x{H}")

    def snap(name, delay=0.5):
        time.sleep(delay)
        img = cap.screenshot()
        path = OUTPUT / f"{name}.png"
        img.save(str(path))
        print(f"  [{name}] {img.size[0]}x{img.size[1]}")
        return img

    # Helper: click at window-relative coords using ydotool (Wayland-native mouse)
    import subprocess
    _last_mouse = [0, 0]  # track cursor position for relative moves

    def click(x, y, delay=0.2):
        """Move the Wayland cursor to window-relative (x,y) and left-click."""
        ax, ay = geo[0] + x, geo[1] + y
        # ydotool 0.1.x only supports relative moves, so calculate delta
        dx = ax - _last_mouse[0]
        dy = ay - _last_mouse[1]
        subprocess.run(["ydotool", "mousemove", "--delay", "0", str(dx), str(dy)],
                       capture_output=True)
        _last_mouse[0] = ax
        _last_mouse[1] = ay
        time.sleep(0.05)
        subprocess.run(["ydotool", "click", "--delay", "0", "1"],
                       capture_output=True)
        time.sleep(delay)

    # Initialize cursor to window center (where launch click placed it)
    _last_mouse[0] = geo[0] + W // 2
    _last_mouse[1] = geo[1] + H // 2

    # ──── PART 1: Game world & HUD ────
    snap("01_initial_state", delay=2.0)

    inp.key_hold(keys.MOVE_RIGHT, 2.0)
    snap("02_moved_right")

    inp.key_hold(keys.MOVE_DOWN, 2.0)
    snap("03_moved_down")

    inp.key_hold(keys.MOVE_LEFT, 3.0)
    snap("04_moved_left")

    inp.key_hold(keys.MOVE_UP, 3.0)
    snap("05_moved_up")

    # ──── PART 2: Pause menu ────
    inp.key_tap(keys.MENU, duration=0.1)
    snap("06_pause_menu", delay=0.8)

    # Close menu
    inp.key_tap(keys.MENU, duration=0.1)
    snap("07_menu_closed", delay=0.5)

    # ──── PART 3: NPC interaction ────
    # Walk toward Elder NPC and interact
    inp.key_hold(keys.MOVE_UP, 1.5)
    inp.key_hold(keys.MOVE_RIGHT, 0.5)
    time.sleep(0.1)
    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("08_npc_interact", delay=0.8)

    # Advance dialogue
    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("09_dialogue_2", delay=0.5)

    inp.key_tap(keys.CONFIRM, duration=0.1)
    snap("10_dialogue_3", delay=0.5)

    # Dismiss
    for _ in range(5):
        inp.key_tap(keys.CONFIRM, duration=0.1)
        time.sleep(0.3)
    snap("11_after_dialogue")

    # ──── PART 4: Editor - all tabs ────
    inp.key_tap(keys.EDITOR_TOGGLE, duration=0.1)
    time.sleep(1.0)

    # Re-read geometry in case it changed
    geo = gw.geometry()
    W, H = geo[2], geo[3]
    print(f"  Editor window: {W}x{H}")

    snap("12_editor_tiles")

    # Cycle through all asset tabs using E key (next tab) via uinput hold
    tab_names = ["buildings", "furniture", "characters", "trees", "vehicles", "misc"]
    for name in tab_names:
        inp.key_hold(keys.ASSET_TAB_NEXT, 0.3)
        time.sleep(0.3)
        snap(f"13_editor_{name}", delay=0.3)

    # Back to Tiles with Q key (prev tab)
    for _ in range(6):
        inp.key_hold(keys.ASSET_TAB_PREV, 0.3)
        time.sleep(0.3)
    snap("19_editor_tiles_again", delay=0.3)

    # Close editor with ESC
    inp.key_tap(keys.MENU, duration=0.1)
    time.sleep(0.5)

    # If pause menu opened, close it too
    inp.key_tap(keys.MENU, duration=0.1)
    snap("22_back_to_game", delay=0.5)

    # ──── PART 5: Sprint to map edges ────
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 5.0)
    inp.key_up(keys.RUN)
    snap("23_east_edge")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 5.0)
    inp.key_up(keys.RUN)
    snap("24_SE_corner")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 10.0)
    inp.key_up(keys.RUN)
    snap("25_SW_corner")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_UP, 10.0)
    inp.key_up(keys.RUN)
    snap("26_NW_corner")

    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_RIGHT, 10.0)
    inp.key_up(keys.RUN)
    snap("27_NE_corner")

    # Back to center
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_LEFT, 5.0)
    inp.key_up(keys.RUN)
    inp.key_down(keys.RUN)
    inp.key_hold(keys.MOVE_DOWN, 5.0)
    inp.key_up(keys.RUN)
    snap("28_center_final")

    count = len(list(OUTPUT.glob("*.png")))
    print(f"\nDone! {count} screenshots in {OUTPUT}/")

finally:
    inp.close()
    engine.stop()
