#!/usr/bin/env python3
"""CLI entry point for tw_test: python -m tw_test [command]"""

import argparse
import code
import sys
import time

from .harness import TestHarness
from .engine import GameEngine
from .window import GameWindow
from .input_sim import InputController
from .capture import ScreenCapture
from . import keys


def cmd_screenshot(args):
    """Launch game, take a screenshot, save it, exit."""
    harness = TestHarness(auto_launch=not args.no_launch, output_dir=args.output)
    try:
        harness.setup()
        time.sleep(args.delay)
        harness.screenshot(args.name or "capture")
        print("Done.")
    finally:
        harness.teardown()


def cmd_smoke(args):
    """Run the smoke test scenario."""
    from .scenarios import smoke_test
    harness = TestHarness(auto_launch=not args.no_launch, output_dir=args.output)
    try:
        harness.setup()
        smoke_test.run(harness)
        harness.print_summary()
    except Exception as e:
        print(f"\nSmoke test failed: {e}")
        harness.print_summary()
        sys.exit(1)
    finally:
        harness.teardown()


def cmd_interactive(args):
    """Launch game and drop into a Python REPL with all tools available."""
    inp = InputController()

    if args.no_launch:
        engine = None
        gw = GameWindow()
        gw.find()
        gw.focus()
    else:
        engine = GameEngine()
        gw = engine.launch()

    cap = ScreenCapture(gw)
    harness = TestHarness(auto_launch=False)
    harness._window = gw
    harness._input = inp
    harness._capture = cap
    harness._engine = engine

    banner = (
        "\n"
        "Twilight Engine Test Tool - Interactive Mode\n"
        "=============================================\n"
        "Available objects:\n"
        "  gw      - GameWindow (window management)\n"
        "  inp     - InputController (keyboard/mouse via uinput)\n"
        "  cap     - ScreenCapture (X11 window capture)\n"
        "  harness - TestHarness (high-level API)\n"
        "  engine  - GameEngine (process management)\n"
        "  keys    - Key constants (MOVE_UP, CONFIRM, etc.)\n"
        "\n"
        "Examples:\n"
        "  harness.walk('right', 1.0)\n"
        "  harness.interact()\n"
        "  harness.screenshot('test')\n"
        "  inp.key_tap(keys.CONFIRM)\n"
        "  inp.mouse_click(1, 480, 360)\n"
        "  cap.screenshot().show()\n"
        "\n"
    )

    local_vars = {
        "gw": gw,
        "inp": inp,
        "cap": cap,
        "harness": harness,
        "engine": engine,
        "keys": keys,
        "time": time,
    }

    try:
        code.interact(banner=banner, local=local_vars, exitmsg="Goodbye.")
    finally:
        inp.close()
        if engine:
            engine.stop()


def cmd_scenario(args):
    """Run a specific scenario by name."""
    import importlib
    try:
        mod = importlib.import_module(f".scenarios.{args.name}", package="tw_test")
    except ModuleNotFoundError:
        print(f"Unknown scenario: {args.name}")
        print("Available: smoke_test")
        sys.exit(1)

    harness = TestHarness(auto_launch=not args.no_launch, output_dir=args.output)
    try:
        harness.setup()
        mod.run(harness)
        harness.print_summary()
    except Exception as e:
        print(f"\nScenario '{args.name}' failed: {e}")
        harness.print_summary()
        sys.exit(1)
    finally:
        harness.teardown()


def main():
    parser = argparse.ArgumentParser(
        prog="tw_test",
        description="Twilight Engine test automation tool",
    )
    parser.add_argument(
        "--no-launch", action="store_true",
        help="Attach to an already-running game instead of launching one",
    )
    parser.add_argument(
        "--output", "-o", default="test_output",
        help="Output directory for screenshots (default: test_output)",
    )

    sub = parser.add_subparsers(dest="command")

    p_ss = sub.add_parser("screenshot", help="Launch and take a screenshot")
    p_ss.add_argument("--name", "-n", default=None, help="Screenshot filename")
    p_ss.add_argument("--delay", "-d", type=float, default=1.0,
                       help="Seconds to wait before capture (default: 1.0)")

    sub.add_parser("smoke", help="Run smoke test scenario")
    sub.add_parser("interactive", help="Launch with Python REPL")

    p_sc = sub.add_parser("scenario", help="Run a named test scenario")
    p_sc.add_argument("name", help="Scenario module name (e.g. smoke_test)")

    args = parser.parse_args()

    if args.command == "screenshot":
        cmd_screenshot(args)
    elif args.command == "smoke":
        cmd_smoke(args)
    elif args.command == "interactive":
        cmd_interactive(args)
    elif args.command == "scenario":
        cmd_scenario(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
