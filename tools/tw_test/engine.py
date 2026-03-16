#!/usr/bin/env python3
"""Game process launcher and lifecycle management."""

import os
import signal
import subprocess
import threading
import time
from pathlib import Path

from .window import GameWindow

# Default paths relative to the engine repo root
_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_DEFAULT_BINARY = _REPO_ROOT / "build-linux" / "twilight_game_binary"
_DEFAULT_GAME_DIR = _REPO_ROOT / "build-linux"


class GameEngine:
    """Launch and manage the game binary process."""

    def __init__(self, binary_path=None, game_dir=None):
        self._binary = Path(binary_path) if binary_path else _DEFAULT_BINARY
        self._game_dir = Path(game_dir) if game_dir else _DEFAULT_GAME_DIR
        self._process = None
        self._stdout_lines = []
        self._stderr_lines = []
        self._reader_thread = None

    @property
    def process(self):
        return self._process

    @property
    def stdout_lines(self):
        return list(self._stdout_lines)

    @property
    def stderr_lines(self):
        return list(self._stderr_lines)

    def launch(self, extra_args=None, wait_for_window=15.0):
        """Launch the game binary in windowed X11 mode and wait for the window.

        Sets TW_FORCE_X11=1 so GLFW uses XWayland, and passes --windowed
        to disable fullscreen. This makes the window visible to python-xlib
        for screenshots and input via XTest.

        Returns a GameWindow instance.
        """
        if not self._binary.exists():
            raise FileNotFoundError(f"Game binary not found: {self._binary}")

        env = {**os.environ, "TW_FORCE_X11": "1"}
        cmd = [str(self._binary), "--windowed"]
        if extra_args:
            cmd.extend(extra_args)

        self._process = subprocess.Popen(
            cmd,
            cwd=str(self._game_dir),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # Background thread to read stdout/stderr without blocking
        self._reader_thread = threading.Thread(target=self._read_output, daemon=True)
        self._reader_thread.start()

        # Check that process didn't die immediately
        time.sleep(0.5)
        if self._process.poll() is not None:
            raise RuntimeError(
                f"Game process exited immediately with code {self._process.returncode}\n"
                f"stderr: {''.join(self._stderr_lines)}"
            )

        # Wait for the X11 window to appear
        gw = GameWindow()
        gw.find(timeout=wait_for_window)
        time.sleep(0.5)

        # Maximize the window for consistent coordinates
        gw.maximize()
        time.sleep(0.5)

        # Move mouse pointer onto the window center and click to grab focus.
        # XWayland requires the pointer to be over the window for keyboard input.
        geo = gw.geometry()
        cx, cy = geo[0] + geo[2] // 2, geo[1] + geo[3] // 2
        from Xlib import X
        from Xlib.ext import xtest
        xtest.fake_input(gw.display, X.MotionNotify, x=cx, y=cy)
        gw.display.sync()
        time.sleep(0.1)
        xtest.fake_input(gw.display, X.ButtonPress, 1)
        gw.display.sync()
        time.sleep(0.05)
        xtest.fake_input(gw.display, X.ButtonRelease, 1)
        gw.display.sync()
        time.sleep(0.5)

        gw.focus()
        geo = gw.geometry()
        print(f"[Engine] Game is ready (window {geo[2]}x{geo[3]} at {geo[0]},{geo[1]})")
        return gw

    def _read_output(self):
        """Read stdout and stderr in background."""
        def read_stream(stream, target):
            for line in iter(stream.readline, b""):
                target.append(line.decode("utf-8", errors="replace"))
            stream.close()

        stdout_thread = threading.Thread(
            target=read_stream,
            args=(self._process.stdout, self._stdout_lines),
            daemon=True,
        )
        stderr_thread = threading.Thread(
            target=read_stream,
            args=(self._process.stderr, self._stderr_lines),
            daemon=True,
        )
        stdout_thread.start()
        stderr_thread.start()
        stdout_thread.join()
        stderr_thread.join()

    def is_running(self):
        """Check if the game process is still alive."""
        return self._process is not None and self._process.poll() is None

    def stop(self, timeout=5.0):
        """Gracefully stop the game process."""
        if not self._process:
            return
        if self._process.poll() is not None:
            return

        self._process.send_signal(signal.SIGTERM)
        try:
            self._process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self._process.kill()
            self._process.wait(timeout=2.0)
        print("[Engine] Game stopped")
