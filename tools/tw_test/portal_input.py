#!/usr/bin/env python3
"""Wayland Remote Desktop Portal input injection.

Uses org.freedesktop.portal.RemoteDesktop D-Bus API to send mouse and keyboard
events through the GNOME compositor. This is the only reliable way to inject
mouse input on GNOME Wayland — events go through Mutter's input pipeline
so GLFW receives proper cursor position updates.

Requires a one-time user permission prompt (GNOME shows a dialog).
"""

import time
import dbus
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

# Linux evdev button codes
BTN_LEFT = 0x110
BTN_RIGHT = 0x111
BTN_MIDDLE = 0x112


class PortalInput:
    """Mouse and keyboard input via the XDG Remote Desktop portal."""

    def __init__(self):
        DBusGMainLoop(set_as_default=True)
        self._bus = dbus.SessionBus()
        self._portal = self._bus.get_object(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
        )
        self._rd = dbus.Interface(self._portal, "org.freedesktop.portal.RemoteDesktop")
        self._session_path = None
        self._stream = 0
        self._loop = GLib.MainLoop()

    def start(self):
        """Create a Remote Desktop session and get user permission.

        This will show a GNOME permission dialog the first time.
        """
        # Step 1: Create session
        self._pending_response = None
        token = "tw_test_%d" % int(time.time())
        request_path = self._rd.CreateSession({
            "handle_token": token,
            "session_handle_token": "tw_test_session",
        })
        self._wait_for_response(request_path)
        self._session_path = self._pending_response.get("session_handle", "")
        if not self._session_path:
            # Try to extract from response
            self._session_path = str(request_path).replace("/request/", "/session/").rsplit("/", 1)[0] + "/tw_test_session"

        print(f"[Portal] Session: {self._session_path}")

        # Step 2: Select devices (keyboard + pointer)
        token2 = "tw_test_dev_%d" % int(time.time())
        request_path2 = self._rd.SelectDevices(
            self._session_path,
            {
                "handle_token": token2,
                "types": dbus.UInt32(3),  # 1=keyboard, 2=pointer, 3=both
            },
        )
        self._wait_for_response(request_path2)

        # Step 3: Start (this shows the permission dialog — user must click Allow)
        print("[Portal] Waiting for user to approve Remote Desktop access...")
        token3 = "tw_test_start_%d" % int(time.time())
        request_path3 = self._rd.Start(
            self._session_path,
            "",  # parent_window
            {"handle_token": token3},
        )
        self._wait_for_response(request_path3, timeout=60000)  # 60s for user approval

        # Extract stream ID if available
        streams = self._pending_response.get("streams", [])
        if streams:
            self._stream = streams[0][0]  # first stream's node_id

        print("[Portal] Remote Desktop session active")
        return self

    def _wait_for_response(self, request_path, timeout=30000):
        """Wait for the portal Response signal."""
        self._pending_response = None
        request_obj = self._bus.get_object(
            "org.freedesktop.portal.Desktop",
            request_path,
        )

        def on_response(response_code, results):
            self._pending_response = dict(results) if results else {}
            self._pending_response["_response_code"] = int(response_code)
            self._loop.quit()

        request_obj.connect_to_signal("Response", on_response)

        GLib.timeout_add(timeout, self._loop.quit)
        self._loop.run()

        if self._pending_response is None:
            raise TimeoutError("Portal response timed out")
        if self._pending_response.get("_response_code", 0) != 0:
            raise RuntimeError(f"Portal request denied (code {self._pending_response['_response_code']})")

    # --- Mouse ---

    def mouse_move_absolute(self, x, y):
        """Move mouse to absolute screen coordinates (x, y)."""
        self._rd.NotifyPointerMotionAbsolute(
            self._session_path, {},
            dbus.UInt32(self._stream),
            dbus.Double(x), dbus.Double(y),
        )

    def mouse_move_relative(self, dx, dy):
        """Move mouse by relative offset."""
        self._rd.NotifyPointerMotion(
            self._session_path, {},
            dbus.Double(dx), dbus.Double(dy),
        )

    def mouse_button(self, button, pressed):
        """Press or release a mouse button.

        button: BTN_LEFT (0x110), BTN_RIGHT (0x111), BTN_MIDDLE (0x112)
        pressed: True for press, False for release
        """
        self._rd.NotifyPointerButton(
            self._session_path, {},
            dbus.Int32(button),
            dbus.UInt32(1 if pressed else 0),
        )

    def mouse_move_to(self, x, y):
        """Move mouse to approximate absolute screen position using relative moves.

        First moves to (0,0) via a large negative relative move, then to (x,y).
        Not pixel-perfect but reliable for clicking UI elements.
        """
        # Move far top-left to reset to (0,0)
        self.mouse_move_relative(-3000.0, -3000.0)
        time.sleep(0.02)
        # Now move to target
        self.mouse_move_relative(float(x), float(y))
        time.sleep(0.02)

    def mouse_click(self, x=None, y=None, button=BTN_LEFT):
        """Move to (x, y) and click."""
        if x is not None and y is not None:
            self.mouse_move_to(x, y)
            time.sleep(0.05)
        self.mouse_button(button, True)
        time.sleep(0.05)
        self.mouse_button(button, False)

    def mouse_scroll(self, steps, axis=0):
        """Scroll. axis: 0=vertical, 1=horizontal. steps: positive=up/right."""
        self._rd.NotifyPointerAxisDiscrete(
            self._session_path, {},
            dbus.UInt32(axis),
            dbus.Int32(steps),
        )

    # --- Keyboard ---

    def key_press(self, keycode):
        """Press a key (Linux evdev keycode)."""
        self._rd.NotifyKeyboardKeycode(
            self._session_path, {},
            dbus.Int32(keycode),
            dbus.UInt32(1),  # pressed
        )

    def key_release(self, keycode):
        """Release a key."""
        self._rd.NotifyKeyboardKeycode(
            self._session_path, {},
            dbus.Int32(keycode),
            dbus.UInt32(0),  # released
        )

    def key_tap(self, keycode, duration=0.05):
        """Press and release a key."""
        self.key_press(keycode)
        time.sleep(duration)
        self.key_release(keycode)

    def close(self):
        """Close the Remote Desktop session."""
        if self._session_path:
            try:
                session_obj = self._bus.get_object(
                    "org.freedesktop.portal.Desktop",
                    self._session_path,
                )
                session_iface = dbus.Interface(session_obj, "org.freedesktop.portal.Session")
                session_iface.Close()
            except Exception:
                pass
            self._session_path = None
