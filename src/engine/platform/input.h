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

struct InputState {
    static constexpr int ACTION_COUNT = static_cast<int>(InputAction::Count);

    bool actions[ACTION_COUNT] = {};         // Currently held
    bool actions_pressed[ACTION_COUNT] = {}; // Just pressed this frame
    bool actions_released[ACTION_COUNT] = {};// Just released this frame

    bool is_held(InputAction a) const { return actions[static_cast<int>(a)]; }
    bool is_pressed(InputAction a) const { return actions_pressed[static_cast<int>(a)]; }
    bool is_released(InputAction a) const { return actions_released[static_cast<int>(a)]; }

    void clear_frame() {
        for (int i = 0; i < ACTION_COUNT; i++) {
            actions_pressed[i] = false;
            actions_released[i] = false;
        }
    }
};

} // namespace eb
