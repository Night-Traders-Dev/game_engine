#!/usr/bin/env python3
"""Linux input event key codes matching the engine's GLFW key mappings.

Uses linux/input-event-codes.h values for uinput virtual device injection.
Mirrors platform_desktop.cpp key_callback (lines 162-170).
"""

# From linux/input-event-codes.h
# Movement keys (WASD + arrows)
KEY_Q = 16
KEY_W = 17
KEY_E = 18
KEY_A = 30
KEY_S = 31
KEY_D = 32
KEY_UP = 103
KEY_DOWN = 108
KEY_LEFT = 105
KEY_RIGHT = 106

# Action keys
KEY_Z = 44
KEY_ENTER = 28
KEY_X = 45
KEY_BACKSPACE = 14
KEY_ESCAPE = 1
KEY_LEFT_SHIFT = 42
KEY_TAB = 15
KEY_LEFT_CTRL = 29
KEY_LEFT_ALT = 56
KEY_LEFT_BRACKET = 26   # [
KEY_RIGHT_BRACKET = 27  # ]
KEY_F5 = 63
KEY_F6 = 64
KEY_F7 = 65
KEY_F8 = 66
KEY_F9 = 67
KEY_F10 = 68
KEY_F11 = 87
KEY_1 = 2
KEY_2 = 3
KEY_3 = 4
KEY_4 = 5
KEY_5 = 6
KEY_6 = 7
KEY_7 = 8

# Game action aliases (primary key for each action)
MOVE_UP = KEY_W
MOVE_DOWN = KEY_S
MOVE_LEFT = KEY_A
MOVE_RIGHT = KEY_D
CONFIRM = KEY_Z
CANCEL = KEY_X
MENU = KEY_ESCAPE
RUN = KEY_LEFT_SHIFT
EDITOR_TOGGLE = KEY_TAB

# Direction name -> key code mapping
DIRECTIONS = {
    "up": MOVE_UP,
    "down": MOVE_DOWN,
    "left": MOVE_LEFT,
    "right": MOVE_RIGHT,
}

# All key codes the tool might use (for uinput device registration)
ASSET_TAB_PREV = KEY_Q
ASSET_TAB_NEXT = KEY_E

ALL_KEYS = [
    KEY_Q, KEY_W, KEY_E, KEY_A, KEY_S, KEY_D,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_Z, KEY_ENTER, KEY_X, KEY_BACKSPACE,
    KEY_ESCAPE, KEY_LEFT_SHIFT, KEY_TAB,
    KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7,
    KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11,
]

# Mouse button codes (from linux/input-event-codes.h)
BTN_LEFT = 0x110
BTN_RIGHT = 0x111
BTN_MIDDLE = 0x112

# Relative axis codes
REL_X = 0x00
REL_Y = 0x01
REL_WHEEL = 0x08

# Absolute axis codes (for tablet-style absolute positioning)
ABS_X = 0x00
ABS_Y = 0x01
