#!/usr/bin/env python3
"""Screenshot capture of the game window via X11 GetImage.

Works because the game is launched with TW_FORCE_X11=1 which puts
it on XWayland, making it visible to python-xlib.
"""

import time
import numpy as np
from PIL import Image
from Xlib import X

from .window import GameWindow


class ScreenCapture:
    """Capture screenshots of the game window via X11."""

    def __init__(self, game_window: GameWindow):
        self._gw = game_window

    def screenshot(self):
        """Capture the game window contents. Returns a PIL Image (RGB)."""
        win = self._gw.window
        if not win:
            raise RuntimeError("No game window found")

        geo = win.get_geometry()
        w, h = geo.width, geo.height

        raw = win.get_image(0, 0, w, h, X.ZPixmap, 0xFFFFFFFF)
        data = raw.data
        if isinstance(data, str):
            data = data.encode("latin-1")

        # X11 ZPixmap with 24/32 bit depth returns BGRX format
        img = Image.frombytes("RGBX", (w, h), bytes(data), "raw", "BGRX")
        return img.convert("RGB")

    def save(self, path):
        """Take a screenshot and save to file. Returns the path."""
        img = self.screenshot()
        img.save(str(path))
        return str(path)

    def region(self, x, y, w, h):
        """Capture a sub-region of the game window. Returns a PIL Image."""
        img = self.screenshot()
        return img.crop((x, y, x + w, y + h))

    def compare(self, img1, img2):
        """Return similarity score (0.0 to 1.0) between two images.

        1.0 means identical, 0.0 means completely different.
        """
        if img1.size != img2.size:
            img2 = img2.resize(img1.size, Image.NEAREST)

        arr1 = np.array(img1, dtype=np.float32)
        arr2 = np.array(img2, dtype=np.float32)

        diff = np.abs(arr1 - arr2)
        max_diff = 255.0 * arr1.size
        similarity = 1.0 - (diff.sum() / max_diff)
        return float(similarity)

    def wait_for_change(self, reference=None, timeout=5.0, threshold=0.95):
        """Poll screenshots until content changes from reference.

        If reference is None, takes one immediately as baseline.
        Returns True if change detected, False on timeout.
        """
        if reference is None:
            reference = self.screenshot()
            time.sleep(0.1)

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            current = self.screenshot()
            sim = self.compare(reference, current)
            if sim < threshold:
                return True
            time.sleep(0.1)
        return False
