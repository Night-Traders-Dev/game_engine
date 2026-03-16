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

    // Input buffering: stores recent presses with timestamps for lenient timing
    struct BufferedInput {
        InputAction action;
        float timestamp = 0;
    };
    static constexpr int INPUT_BUFFER_SIZE = 8;
    static constexpr float INPUT_BUFFER_WINDOW = 0.15f; // 150ms buffer
    BufferedInput input_buffer[INPUT_BUFFER_SIZE] = {};
    int buffer_head = 0;
    float input_time = 0;

    void buffer_press(InputAction action) {
        input_buffer[buffer_head] = {action, input_time};
        buffer_head = (buffer_head + 1) % INPUT_BUFFER_SIZE;
    }
    bool consume_buffered(InputAction action) {
        for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
            auto& b = input_buffer[i];
            if (b.action == action && (input_time - b.timestamp) < INPUT_BUFFER_WINDOW) {
                b.timestamp = -1; // consumed
                return true;
            }
        }
        return false;
    }

    // Gamepad state (updated from GLFW gamepad API)
    struct GamepadState {
        bool connected = false;
        float axes[6] = {};        // 0=LX, 1=LY, 2=RX, 3=RY, 4=LT, 5=RT
        bool buttons[16] = {};     // A,B,X,Y,LB,RB,Back,Start,Guide,LS,RS,DPadU,DPadR,DPadD,DPadL
        bool buttons_pressed[16] = {};
        static constexpr float DEADZONE = 0.2f;
        void clear_frame() { for (int i = 0; i < 16; i++) buttons_pressed[i] = false; }
    };
    GamepadState gamepad;

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
        gamepad.clear_frame();
    }
};

// ─── Rebindable Key Bindings ───

struct KeyBinding {
    InputAction action;
    int primary_key;      // GLFW key code
    int secondary_key;    // Alternative key (-1 = none)
    int gamepad_button;   // GLFW gamepad button (-1 = none)
};

struct KeyBindings {
    static constexpr int ACTION_COUNT = static_cast<int>(InputAction::Count);
    KeyBinding bindings[ACTION_COUNT];

    KeyBindings() { reset_defaults(); }

    void reset_defaults() {
        // Default WASD + arrows + game controls
        bindings[0] = {InputAction::MoveUp,    87,  265, 11};  // W, Up, DPadUp
        bindings[1] = {InputAction::MoveDown,   83,  264, 13};  // S, Down, DPadDown
        bindings[2] = {InputAction::MoveLeft,   65,  263, 14};  // A, Left, DPadLeft
        bindings[3] = {InputAction::MoveRight,  68,  262, 12};  // D, Right, DPadRight
        bindings[4] = {InputAction::Confirm,    90,   257, 0};  // Z, Enter, A
        bindings[5] = {InputAction::Cancel,     88,   259, 1};  // X, Backspace, B
        bindings[6] = {InputAction::Menu,       256,  -1,  6};  // Escape, -, Start
        bindings[7] = {InputAction::Run,        340,  -1,  5};  // LShift, -, RB
    }

    void rebind(InputAction action, int new_key) {
        int idx = static_cast<int>(action);
        if (idx >= 0 && idx < ACTION_COUNT) bindings[idx].primary_key = new_key;
    }

    // Check if a GLFW key matches any binding for the given action
    bool matches(InputAction action, int glfw_key) const {
        int idx = static_cast<int>(action);
        if (idx < 0 || idx >= ACTION_COUNT) return false;
        return bindings[idx].primary_key == glfw_key || bindings[idx].secondary_key == glfw_key;
    }
};

} // namespace eb
