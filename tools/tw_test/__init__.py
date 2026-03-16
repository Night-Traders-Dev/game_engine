"""tw_test - Twilight Engine test automation tool.

External tool for automated testing and analysis of the game engine.
Uses TW_FORCE_X11=1 + --windowed to run the game on XWayland,
uinput for input injection, and X11 GetImage for screenshots.

Usage as library:
    from tw_test import GameEngine, TestHarness
    harness = TestHarness().setup()
    harness.walk("right", 1.0)
    harness.screenshot("after_walk")
    harness.teardown()

Usage as CLI:
    python -m tw_test screenshot
    python -m tw_test smoke
    python -m tw_test interactive
"""

from .keys import (
    MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT,
    CONFIRM, CANCEL, MENU, RUN, EDITOR_TOGGLE,
    DIRECTIONS,
)
from .window import GameWindow
from .input_sim import InputController
from .capture import ScreenCapture
from .engine import GameEngine
from .harness import TestHarness

__all__ = [
    "GameWindow", "InputController", "ScreenCapture", "GameEngine", "TestHarness",
    "MOVE_UP", "MOVE_DOWN", "MOVE_LEFT", "MOVE_RIGHT",
    "CONFIRM", "CANCEL", "MENU", "RUN", "EDITOR_TOGGLE",
    "DIRECTIONS",
]
