#!/usr/bin/env python3
"""High-level test harness with convenience API for game testing."""

import time
from contextlib import contextmanager
from pathlib import Path

from . import keys
from .engine import GameEngine
from .window import GameWindow
from .input_sim import InputController
from .capture import ScreenCapture


class TestHarness:
    """Convenience API for writing game test scenarios.

    Manages the game lifecycle and provides high-level methods
    for movement, interaction, screenshots, and assertions.
    """

    def __init__(self, auto_launch=True, output_dir=None):
        self._auto_launch = auto_launch
        self._output_dir = Path(output_dir) if output_dir else Path("test_output")
        self._engine = None
        self._window = None
        self._input = None
        self._capture = None
        self._screenshot_count = 0
        self._scenario_results = []

    @property
    def engine(self):
        return self._engine

    @property
    def window(self):
        return self._window

    @property
    def input(self):
        return self._input

    @property
    def capture(self):
        return self._capture

    def setup(self):
        """Launch the game and initialize all controllers. Returns self."""
        self._output_dir.mkdir(parents=True, exist_ok=True)

        if self._auto_launch:
            self._engine = GameEngine()
            self._window = self._engine.launch()
        else:
            self._window = GameWindow()
            self._window.find()
            self._window.focus()

        self._input = InputController()
        self._capture = ScreenCapture(self._window)
        return self

    def teardown(self):
        """Stop the game and clean up."""
        if self._input:
            self._input.close()
        if self._engine:
            self._engine.stop()

    # --- Movement ---

    def walk(self, direction, duration=1.0):
        """Walk in a direction for the given duration.

        direction: 'up', 'down', 'left', 'right'
        """
        key = keys.DIRECTIONS.get(direction)
        if key is None:
            raise ValueError(f"Invalid direction: {direction}")
        self._input.key_hold(key, duration)

    def run(self, direction, duration=1.0):
        """Run (shift + direction) for the given duration."""
        key = keys.DIRECTIONS.get(direction)
        if key is None:
            raise ValueError(f"Invalid direction: {direction}")
        self._input.key_down(keys.RUN)
        time.sleep(0.02)
        self._input.key_hold(key, duration)
        self._input.key_up(keys.RUN)

    # --- Actions ---

    def interact(self):
        """Press confirm (Z) and wait briefly for response."""
        self._input.key_tap(keys.CONFIRM)
        time.sleep(0.3)

    def cancel(self):
        """Press cancel (X)."""
        self._input.key_tap(keys.CANCEL)
        time.sleep(0.2)

    def open_menu(self):
        """Press Escape to open/close menu."""
        self._input.key_tap(keys.MENU)
        time.sleep(0.3)

    def toggle_editor(self):
        """Press Tab to toggle the editor."""
        self._input.key_tap(keys.EDITOR_TOGGLE)
        time.sleep(0.3)

    # --- Waiting ---

    def wait(self, seconds):
        """Simple time-based wait."""
        time.sleep(seconds)

    def wait_for_screen_change(self, timeout=5.0):
        """Wait until the screen content changes. Returns True if changed."""
        return self._capture.wait_for_change(timeout=timeout)

    # --- Screenshots ---

    def screenshot(self, name=None):
        """Take a screenshot and save it. Returns the PIL Image."""
        img = self._capture.screenshot()
        self._screenshot_count += 1
        if name is None:
            name = f"screenshot_{self._screenshot_count:03d}"
        path = self._output_dir / f"{name}.png"
        img.save(str(path))
        print(f"  Screenshot saved: {path} ({img.size[0]}x{img.size[1]})")
        return img

    # --- Assertions ---

    def assert_screen_changed(self, reference, msg=""):
        """Assert that the current screen differs from a reference image."""
        current = self._capture.screenshot()
        similarity = self._capture.compare(reference, current)
        if similarity > 0.95:
            self.screenshot("assertion_failed")
            raise AssertionError(
                f"Screen did not change (similarity={similarity:.3f}). {msg}"
            )

    def assert_screen_region_color(self, x, y, w, h, expected_rgb, tolerance=30):
        """Assert that a screen region has approximately the expected color."""
        import numpy as np
        region = self._capture.region(x, y, w, h)
        pixels = np.array(region, dtype=np.float32)
        mean_color = pixels.mean(axis=(0, 1))
        expected = np.array(expected_rgb, dtype=np.float32)
        diff = np.abs(mean_color - expected).max()
        if diff > tolerance:
            self.screenshot("color_assertion_failed")
            raise AssertionError(
                f"Region ({x},{y},{w},{h}) mean color {mean_color.tolist()} "
                f"differs from expected {expected_rgb} by {diff:.1f} (tolerance={tolerance})"
            )

    # --- Scenarios ---

    @contextmanager
    def scenario(self, name):
        """Context manager for a named test scenario.

        Logs start/end, captures a failure screenshot on exception.
        """
        print(f"[SCENARIO] {name} ...")
        start = time.monotonic()
        try:
            yield
            elapsed = time.monotonic() - start
            print(f"[SCENARIO] {name} PASSED ({elapsed:.1f}s)")
            self._scenario_results.append((name, "PASSED", elapsed))
        except Exception as e:
            elapsed = time.monotonic() - start
            print(f"[SCENARIO] {name} FAILED ({elapsed:.1f}s): {e}")
            try:
                self.screenshot(f"FAIL_{name}")
            except Exception:
                pass
            self._scenario_results.append((name, "FAILED", elapsed))
            raise

    def print_summary(self):
        """Print a summary of all scenario results."""
        print("\n" + "=" * 50)
        print("TEST SUMMARY")
        print("=" * 50)
        passed = sum(1 for _, status, _ in self._scenario_results if status == "PASSED")
        total = len(self._scenario_results)
        for name, status, elapsed in self._scenario_results:
            marker = "PASS" if status == "PASSED" else "FAIL"
            print(f"  [{marker}] {name} ({elapsed:.1f}s)")
        print(f"\n  {passed}/{total} passed")
        print("=" * 50)
