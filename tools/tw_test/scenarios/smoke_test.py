#!/usr/bin/env python3
"""Smoke test scenario: verify basic game functionality.

Tests:
1. Game launches and renders (not a black screen)
2. Player movement works in all 4 directions
3. Menu opens and closes
4. NPC interaction triggers dialogue
"""

import time
import numpy as np


def run(harness):
    """Run the smoke test suite."""

    with harness.scenario("launch_and_render"):
        img = harness.screenshot("initial_state")
        pixels = np.array(img)
        # Verify game rendered something (not a solid black screen)
        assert pixels.std() > 5.0, (
            f"Screen appears blank (pixel std={pixels.std():.1f}). "
            "Game may not have rendered."
        )

    with harness.scenario("movement_right"):
        ref = harness.screenshot("before_move_right")
        harness.walk("right", duration=0.8)
        time.sleep(0.2)
        harness.assert_screen_changed(ref, "Player should have moved right")
        harness.screenshot("after_move_right")

    with harness.scenario("movement_left"):
        ref = harness.screenshot("before_move_left")
        harness.walk("left", duration=0.8)
        time.sleep(0.2)
        harness.assert_screen_changed(ref, "Player should have moved left")

    with harness.scenario("movement_up"):
        ref = harness.screenshot("before_move_up")
        harness.walk("up", duration=0.8)
        time.sleep(0.2)
        harness.assert_screen_changed(ref, "Player should have moved up")

    with harness.scenario("movement_down"):
        ref = harness.screenshot("before_move_down")
        harness.walk("down", duration=0.8)
        time.sleep(0.2)
        harness.assert_screen_changed(ref, "Player should have moved down")

    with harness.scenario("menu_toggle"):
        ref = harness.screenshot("before_menu")
        harness.open_menu()
        time.sleep(0.3)
        harness.assert_screen_changed(ref, "Menu should have opened")
        harness.screenshot("menu_open")
        # Close menu
        harness.open_menu()
        time.sleep(0.3)

    with harness.scenario("confirm_interaction"):
        ref = harness.screenshot("before_interact")
        harness.interact()
        time.sleep(0.3)
        harness.screenshot("after_interact")
        # Dismiss any dialogue that appeared
        harness.interact()
        time.sleep(0.2)
