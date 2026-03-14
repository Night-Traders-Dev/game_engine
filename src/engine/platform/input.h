#pragma once

#include <cstdint>

namespace eb {

enum class InputAction : uint8_t {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Confirm,      // A / Enter / Z
    Cancel,       // B / Escape / X
    Menu,         // Start / Escape
    Run,          // Hold to run
    Count
};

enum class MouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    Count
};

struct MouseState {
    static constexpr int BUTTON_COUNT = static_cast<int>(MouseButton::Count);

    float x = 0.0f;
    float y = 0.0f;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;

    bool buttons[BUTTON_COUNT] = {};          // Currently held
    bool buttons_pressed[BUTTON_COUNT] = {};  // Just pressed this frame
    bool buttons_released[BUTTON_COUNT] = {}; // Just released this frame

    bool is_held(MouseButton b) const { return buttons[static_cast<int>(b)]; }
    bool is_pressed(MouseButton b) const { return buttons_pressed[static_cast<int>(b)]; }
    bool is_released(MouseButton b) const { return buttons_released[static_cast<int>(b)]; }

    void clear_frame() {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            buttons_pressed[i] = false;
            buttons_released[i] = false;
        }
        scroll_x = 0.0f;
        scroll_y = 0.0f;
    }
};

struct KeyMods {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct InputState {
    static constexpr int ACTION_COUNT = static_cast<int>(InputAction::Count);

    bool actions[ACTION_COUNT] = {};         // Currently held
    bool actions_pressed[ACTION_COUNT] = {}; // Just pressed this frame
    bool actions_released[ACTION_COUNT] = {};// Just released this frame

    // Raw key tracking for editor shortcuts (indexed by GLFW key codes)
    static constexpr int MAX_KEYS = 512;
    bool keys[MAX_KEYS] = {};
    bool keys_pressed[MAX_KEYS] = {};

    MouseState mouse;
    KeyMods mods;

    bool is_held(InputAction a) const { return actions[static_cast<int>(a)]; }
    bool is_pressed(InputAction a) const { return actions_pressed[static_cast<int>(a)]; }
    bool is_released(InputAction a) const { return actions_released[static_cast<int>(a)]; }

    // Raw key access for editor (use GLFW_KEY_* constants)
    bool key_held(int key) const { return key >= 0 && key < MAX_KEYS && keys[key]; }
    bool key_pressed(int key) const { return key >= 0 && key < MAX_KEYS && keys_pressed[key]; }

    void clear_frame() {
        for (int i = 0; i < ACTION_COUNT; i++) {
            actions_pressed[i] = false;
            actions_released[i] = false;
        }
        for (int i = 0; i < MAX_KEYS; i++) {
            keys_pressed[i] = false;
        }
        mouse.clear_frame();
    }
};

} // namespace eb
