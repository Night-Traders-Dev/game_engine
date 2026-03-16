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

    # Helper: click at window-relative coords using X11 warp_pointer + XTest click
    from Xlib import X
    from Xlib.ext import xtest as _xtest

    def click(x, y, delay=0.2):
        """Move the real X11 cursor to window-relative (x,y) and click."""
        # warp_pointer moves the actual cursor, which triggers GLFW's cursor_pos_callback
        gw.window.warp_pointer(x, y)
        gw.display.sync()
        time.sleep(0.05)
        _xtest.fake_input(gw.display, X.ButtonPress, 1)
        gw.display.sync()
        time.sleep(0.05)
        _xtest.fake_input(gw.display, X.ButtonRelease, 1)
        gw.display.sync()
        time.sleep(delay)

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

    # Click each tab using window-relative XTest clicks
    # Tab positions based on ImGui layout (y ~= 68 for tab row)
    # Tabs: Tiles(~165), Buildings(~215), Furniture(~280), Characters(~345), Trees(~400), Vehicles(~448), Misc(~495)
    tab_y = 68

    click(215, tab_y)
    snap("13_editor_buildings", delay=0.3)

    click(280, tab_y)
    snap("14_editor_furniture", delay=0.3)

    click(345, tab_y)
    snap("15_editor_characters", delay=0.3)

    click(400, tab_y)
    snap("16_editor_trees", delay=0.3)

    click(448, tab_y)
    snap("17_editor_vehicles", delay=0.3)

    click(495, tab_y)
    snap("18_editor_misc", delay=0.3)

    # Back to Tiles
    click(165, tab_y)
    snap("19_editor_tiles_again", delay=0.3)

    # Scroll down in tile palette to see more tiles
    gw.window.warp_pointer(400, 150)
    gw.display.sync()
    time.sleep(0.1)
    for _ in range(10):
        _xtest.fake_input(gw.display, X.ButtonPress, 5)
        _xtest.fake_input(gw.display, X.ButtonRelease, 5)
        gw.display.sync()
        time.sleep(0.05)
    snap("20_tiles_scrolled", delay=0.3)

    # Show collision overlay
    click(120, 195)  # Collision checkbox area
    snap("21_collision_view", delay=0.3)

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
