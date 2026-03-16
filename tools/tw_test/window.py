#!/usr/bin/env python3
"""X11 window finding and management for the game window."""

import time
from Xlib.display import Display
from Xlib import X, Xatom


class GameWindow:
    """Find, focus, and query the game window via X11."""

    def __init__(self, display=None):
        self._display = display or Display()
        self._root = self._display.screen().root
        self._window = None
        self._net_wm_name = self._display.intern_atom("_NET_WM_NAME")
        self._utf8_string = self._display.intern_atom("UTF8_STRING")

    @property
    def display(self):
        return self._display

    @property
    def window(self):
        return self._window

    @property
    def window_id(self):
        return self._window.id if self._window else None

    def find(self, title_substring="Crystal Quest", timeout=10.0):
        """Search X11 window tree for game window by title substring.

        Returns True if found, raises TimeoutError if not found within timeout.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            win = self._search_tree(self._root, title_substring)
            if win:
                self._window = win
                return True
            time.sleep(0.2)
        raise TimeoutError(
            f"Game window containing '{title_substring}' not found within {timeout}s"
        )

    def _search_tree(self, window, title_substring):
        """Recursively search the window tree for a matching title."""
        title = self._get_window_name(window)
        if title and title_substring in title:
            return window
        try:
            children = window.query_tree().children
        except Exception:
            return None
        for child in children:
            result = self._search_tree(child, title_substring)
            if result:
                return result
        return None

    def _get_window_name(self, window):
        """Get window name, preferring _NET_WM_NAME (UTF-8) over WM_NAME."""
        try:
            prop = window.get_full_property(self._net_wm_name, self._utf8_string)
            if prop:
                return prop.value.decode("utf-8", errors="replace")
            prop = window.get_full_property(Xatom.WM_NAME, Xatom.STRING)
            if prop:
                val = prop.value
                return val.decode("latin-1") if isinstance(val, bytes) else val
        except Exception:
            pass
        return None

    def maximize(self):
        """Maximize the game window using _NET_WM_STATE."""
        if not self._window:
            raise RuntimeError("No game window found. Call find() first.")
        wm_state = self._display.intern_atom("_NET_WM_STATE")
        max_h = self._display.intern_atom("_NET_WM_STATE_MAXIMIZED_HORZ")
        max_v = self._display.intern_atom("_NET_WM_STATE_MAXIMIZED_VERT")
        from Xlib.protocol.event import ClientMessage
        # _NET_WM_STATE: action=1 (add), prop1=max_h, prop2=max_v
        event = ClientMessage(
            window=self._window,
            client_type=wm_state,
            data=(32, [1, max_h, max_v, 0, 0]),
        )
        self._root.send_event(event,
                              event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
        self._display.sync()

    def focus(self):
        """Raise and focus the game window."""
        if not self._window:
            raise RuntimeError("No game window found. Call find() first.")
        self._window.raise_window()
        self._window.set_input_focus(X.RevertToParent, X.CurrentTime)
        self._display.sync()

    def geometry(self):
        """Return (x, y, width, height) of game window in root coordinates."""
        if not self._window:
            raise RuntimeError("No game window found. Call find() first.")
        geo = self._window.get_geometry()
        coords = self._window.translate_coords(self._root, 0, 0)
        return (-coords.x, -coords.y, geo.width, geo.height)

    def is_alive(self):
        """Check if the game window still exists."""
        if not self._window:
            return False
        try:
            self._window.get_geometry()
            return True
        except Exception:
            self._window = None
            return False
