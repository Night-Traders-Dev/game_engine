#!/usr/bin/env python3
"""Hybrid input injection: XTest for key taps, uinput for held keys & mouse.

XTest sends events directly to the focused X11 window — reliable for
single key presses (Tab, Escape, Z, etc.) that GLFW checks with key_pressed().

uinput creates a kernel-level virtual device — reliable for held keys
(movement) that GLFW checks with is_pressed()/actions[].

The game must be running on XWayland (TW_FORCE_X11=1) for XTest to work.
"""

import ctypes
import fcntl
import os
import struct
import time

from Xlib.display import Display
from Xlib import X, XK
from Xlib.ext import xtest

from . import keys


# ── XTest key mapping (Linux evdev code -> X11 keysym) ──

_EVDEV_TO_KEYSYM = {
    keys.KEY_W: XK.XK_w,
    keys.KEY_A: XK.XK_a,
    keys.KEY_S: XK.XK_s,
    keys.KEY_D: XK.XK_d,
    keys.KEY_UP: XK.XK_Up,
    keys.KEY_DOWN: XK.XK_Down,
    keys.KEY_LEFT: XK.XK_Left,
    keys.KEY_RIGHT: XK.XK_Right,
    keys.KEY_Z: XK.XK_z,
    keys.KEY_ENTER: XK.XK_Return,
    keys.KEY_X: XK.XK_x,
    keys.KEY_BACKSPACE: XK.XK_BackSpace,
    keys.KEY_ESCAPE: XK.XK_Escape,
    keys.KEY_LEFT_SHIFT: XK.XK_Shift_L,
    keys.KEY_TAB: XK.XK_Tab,
    keys.KEY_LEFT_CTRL: XK.XK_Control_L,
    keys.KEY_LEFT_ALT: XK.XK_Alt_L,
    keys.KEY_LEFT_BRACKET: XK.XK_bracketleft,
    keys.KEY_RIGHT_BRACKET: XK.XK_bracketright,
    keys.KEY_1: XK.XK_1,
    keys.KEY_2: XK.XK_2,
    keys.KEY_3: XK.XK_3,
    keys.KEY_4: XK.XK_4,
    keys.KEY_5: XK.XK_5,
    keys.KEY_6: XK.XK_6,
    keys.KEY_7: XK.XK_7,
    keys.KEY_F5: XK.XK_F5,
    keys.KEY_F6: XK.XK_F6,
    keys.KEY_F7: XK.XK_F7,
    keys.KEY_F8: XK.XK_F8,
    keys.KEY_F9: XK.XK_F9,
    keys.KEY_F10: XK.XK_F10,
    keys.KEY_F11: XK.XK_F11,
}


# ── uinput constants ──

_UINPUT_MAX_NAME_SIZE = 80
_UI_SET_EVBIT = 0x40045564
_UI_SET_KEYBIT = 0x40045565
_UI_SET_RELBIT = 0x40045566
_UI_SET_ABSBIT = 0x40045567
_UI_DEV_CREATE = 0x5501
_UI_DEV_DESTROY = 0x5502
_UI_DEV_SETUP = 0x405c5503
_UINPUT_ABS_SETUP = 0x401c5504

_EV_SYN = 0x00
_EV_KEY = 0x01
_EV_REL = 0x02
_EV_ABS = 0x03
_SYN_REPORT = 0x00

_ABSINFO_FMT = "iiiiii"
_UINPUT_SETUP_FMT = f"{_UINPUT_MAX_NAME_SIZE}sHHHHI"
_INPUT_EVENT_FMT = "llHHi"


class InputController:
    """Hybrid input: XTest for taps, uinput for holds and mouse."""

    def __init__(self, screen_width=1920, screen_height=1080):
        # XTest setup
        self._display = Display()
        self._xtest_keycodes = {}
        for evdev_code, keysym in _EVDEV_TO_KEYSYM.items():
            kc = self._display.keysym_to_keycode(keysym)
            if kc:
                self._xtest_keycodes[evdev_code] = kc

        # uinput setup
        self._fd = None
        self._screen_w = screen_width
        self._screen_h = screen_height
        self._setup_uinput()

    def _setup_uinput(self):
        """Create the virtual uinput device for held keys and mouse."""
        self._fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)

        fcntl.ioctl(self._fd, _UI_SET_EVBIT, _EV_KEY)
        fcntl.ioctl(self._fd, _UI_SET_EVBIT, _EV_REL)
        fcntl.ioctl(self._fd, _UI_SET_EVBIT, _EV_ABS)

        for key_code in keys.ALL_KEYS:
            fcntl.ioctl(self._fd, _UI_SET_KEYBIT, key_code)

        fcntl.ioctl(self._fd, _UI_SET_KEYBIT, keys.BTN_LEFT)
        fcntl.ioctl(self._fd, _UI_SET_KEYBIT, keys.BTN_RIGHT)
        fcntl.ioctl(self._fd, _UI_SET_KEYBIT, keys.BTN_MIDDLE)

        fcntl.ioctl(self._fd, _UI_SET_RELBIT, keys.REL_X)
        fcntl.ioctl(self._fd, _UI_SET_RELBIT, keys.REL_Y)
        fcntl.ioctl(self._fd, _UI_SET_RELBIT, keys.REL_WHEEL)

        fcntl.ioctl(self._fd, _UI_SET_ABSBIT, keys.ABS_X)
        fcntl.ioctl(self._fd, _UI_SET_ABSBIT, keys.ABS_Y)

        # Configure absolute axes
        for axis in [keys.ABS_X, keys.ABS_Y]:
            max_val = self._screen_w if axis == keys.ABS_X else self._screen_h
            data = struct.pack("=Hxx" + _ABSINFO_FMT, axis, 0, 0, max_val, 0, 0, 0)
            fcntl.ioctl(self._fd, _UINPUT_ABS_SETUP, data)

        name = b"tw_test virtual input"
        name_padded = name.ljust(_UINPUT_MAX_NAME_SIZE, b"\x00")
        setup_data = struct.pack(_UINPUT_SETUP_FMT, name_padded,
                                 0x03, 0x1234, 0x5678, 1, 0)
        fcntl.ioctl(self._fd, _UI_DEV_SETUP, setup_data)
        fcntl.ioctl(self._fd, _UI_DEV_CREATE)
        time.sleep(0.3)

    def _uinput_event(self, ev_type, code, value):
        t = time.time()
        event = struct.pack(_INPUT_EVENT_FMT, int(t), int((t % 1) * 1_000_000),
                            ev_type, code, value)
        os.write(self._fd, event)

    def _uinput_syn(self):
        self._uinput_event(_EV_SYN, _SYN_REPORT, 0)

    def close(self):
        if self._fd is not None:
            try:
                fcntl.ioctl(self._fd, _UI_DEV_DESTROY)
            except Exception:
                pass
            os.close(self._fd)
            self._fd = None

    def __del__(self):
        self.close()

    # ── Keyboard (XTest for taps, uinput for holds) ──

    def _xtest_key_down(self, evdev_code):
        kc = self._xtest_keycodes.get(evdev_code)
        if kc:
            xtest.fake_input(self._display, X.KeyPress, kc)
            self._display.sync()

    def _xtest_key_up(self, evdev_code):
        kc = self._xtest_keycodes.get(evdev_code)
        if kc:
            xtest.fake_input(self._display, X.KeyRelease, kc)
            self._display.sync()

    def key_down(self, key_code):
        """Send a key press via uinput (for held keys)."""
        self._uinput_event(_EV_KEY, key_code, 1)
        self._uinput_syn()

    def key_up(self, key_code):
        """Send a key release via uinput."""
        self._uinput_event(_EV_KEY, key_code, 0)
        self._uinput_syn()

    def key_tap(self, key_code, duration=0.08):
        """Press and release a key via XTest (reliable for single presses).

        XTest sends events directly to the focused X11 window,
        ensuring GLFW's key_pressed() callback fires.
        """
        self._xtest_key_down(key_code)
        time.sleep(duration)
        self._xtest_key_up(key_code)

    def key_hold(self, key_code, duration):
        """Hold a key via uinput for the specified duration, then release.

        Uses uinput because held keys need to persist across multiple
        GLFW poll cycles, and uinput's kernel-level events do this reliably.
        """
        self.key_down(key_code)
        time.sleep(duration)
        self.key_up(key_code)

    def key_combo(self, *key_codes, duration=0.08):
        """Press multiple keys simultaneously via XTest."""
        for kc in key_codes:
            self._xtest_key_down(kc)
            time.sleep(0.01)
        time.sleep(duration)
        for kc in reversed(key_codes):
            self._xtest_key_up(kc)
            time.sleep(0.01)

    # ── Mouse (uinput) ──

    def mouse_move(self, x, y):
        """Move mouse to absolute position (x, y) on screen."""
        self._uinput_event(_EV_ABS, keys.ABS_X, x)
        self._uinput_event(_EV_ABS, keys.ABS_Y, y)
        self._uinput_syn()

    def mouse_move_relative(self, dx, dy):
        """Move mouse by relative offset."""
        self._uinput_event(_EV_REL, keys.REL_X, dx)
        self._uinput_event(_EV_REL, keys.REL_Y, dy)
        self._uinput_syn()

    def mouse_click(self, button=1, x=None, y=None):
        """Click a mouse button. button: 1=left, 2=middle, 3=right."""
        if x is not None and y is not None:
            self.mouse_move(x, y)
            time.sleep(0.02)
        btn = {1: keys.BTN_LEFT, 2: keys.BTN_MIDDLE, 3: keys.BTN_RIGHT}.get(button, keys.BTN_LEFT)
        self._uinput_event(_EV_KEY, btn, 1)
        self._uinput_syn()
        time.sleep(0.05)
        self._uinput_event(_EV_KEY, btn, 0)
        self._uinput_syn()

    def mouse_drag(self, x1, y1, x2, y2, duration=0.5, button=1):
        """Drag from (x1,y1) to (x2,y2)."""
        btn = {1: keys.BTN_LEFT, 2: keys.BTN_MIDDLE, 3: keys.BTN_RIGHT}.get(button, keys.BTN_LEFT)
        self.mouse_move(x1, y1)
        time.sleep(0.02)
        self._uinput_event(_EV_KEY, btn, 1)
        self._uinput_syn()
        steps = max(int(duration / 0.016), 2)
        for i in range(1, steps + 1):
            t = i / steps
            self.mouse_move(int(x1 + (x2 - x1) * t), int(y1 + (y2 - y1) * t))
            time.sleep(duration / steps)
        self._uinput_event(_EV_KEY, btn, 0)
        self._uinput_syn()

    def mouse_scroll(self, amount, x=None, y=None):
        """Scroll wheel. Positive=up, negative=down."""
        if x is not None and y is not None:
            self.mouse_move(x, y)
            time.sleep(0.02)
        for _ in range(abs(amount)):
            self._uinput_event(_EV_REL, keys.REL_WHEEL, 1 if amount > 0 else -1)
            self._uinput_syn()
            time.sleep(0.02)
