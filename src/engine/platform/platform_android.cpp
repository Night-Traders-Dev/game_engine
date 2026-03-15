#ifdef __ANDROID__

#include "engine/platform/platform_android.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#define LOG_TAG "TWEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace eb {

PlatformAndroid::PlatformAndroid(ANativeWindow* window) : window_(window) {
    if (window_) {
        width_ = ANativeWindow_getWidth(window_);
        height_ = ANativeWindow_getHeight(window_);
    }
    LOGI("PlatformAndroid created (%dx%d)", width_.load(), height_.load());
}

PlatformAndroid::~PlatformAndroid() = default;

void PlatformAndroid::poll_events() {
    input_.clear_frame();
    // Apply touch control actions (A/B/DPad)
    touch_controls_.apply_to(input_);
    // Apply touch → mouse button mapping (survives clear_frame)
    input_.mouse.buttons[0] = touch_is_down_;
    if (touch_just_pressed_) {
        input_.mouse.buttons_pressed[0] = true;
        touch_just_pressed_ = false;
    }
    // Reset touch pressed flags for next frame
    touch_controls_.begin_frame(width_, height_);
}

bool PlatformAndroid::should_close() const {
    return should_close_;
}

VkSurfaceKHR PlatformAndroid::create_surface(VkInstance instance) const {
    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = window_;

    VkSurfaceKHR surface;
    if (vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Android Vulkan surface");
    }
    return surface;
}

std::vector<const char*> PlatformAndroid::get_required_extensions() const {
    return {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };
}

int PlatformAndroid::get_width() const {
    return width_;
}

int PlatformAndroid::get_height() const {
    return height_;
}

bool PlatformAndroid::was_resized() {
    return resized_.exchange(false);
}

void PlatformAndroid::set_window(ANativeWindow* window) {
    window_ = window;
    if (window_) {
        int new_w = ANativeWindow_getWidth(window_);
        int new_h = ANativeWindow_getHeight(window_);
        if (new_w != width_ || new_h != height_) {
            width_ = new_w;
            height_ = new_h;
            resized_ = true;
        }
    }
}

void PlatformAndroid::handle_resize(int width, int height) {
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        resized_ = true;
    }
}

int32_t PlatformAndroid::handle_input(AInputEvent* event) {
    int32_t type = AInputEvent_getType(event);
    int32_t source = AInputEvent_getSource(event);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        // Gamepad/joystick axis events (Quest controllers, generic gamepads)
        if ((source & AINPUT_SOURCE_JOYSTICK) || (source & AINPUT_SOURCE_GAMEPAD)) {
            float lx = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
            float ly = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
            float deadzone = 0.25f;

            // Left stick → movement
            int up = static_cast<int>(InputAction::MoveUp);
            int dn = static_cast<int>(InputAction::MoveDown);
            int lt = static_cast<int>(InputAction::MoveLeft);
            int rt = static_cast<int>(InputAction::MoveRight);
            input_.actions[up] = (ly < -deadzone);
            input_.actions[dn] = (ly > deadzone);
            input_.actions[lt] = (lx < -deadzone);
            input_.actions[rt] = (lx > deadzone);

            // Right trigger → Run
            float rtrigger = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RTRIGGER, 0);
            input_.actions[static_cast<int>(InputAction::Run)] = (rtrigger > 0.3f);

            return 1;
        }
        // Touch events
        process_touch(event);
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t key = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);

        auto set_action = [&](InputAction ia) {
            int idx = static_cast<int>(ia);
            if (action == AKEY_EVENT_ACTION_DOWN) {
                input_.actions[idx] = true;
                input_.actions_pressed[idx] = true;
            } else if (action == AKEY_EVENT_ACTION_UP) {
                input_.actions[idx] = false;
                input_.actions_released[idx] = true;
            }
        };

        switch (key) {
            // Back button → Menu (pause)
            case AKEYCODE_BACK:
                set_action(InputAction::Menu);
                return 1;

            // Gamepad buttons (Quest controllers + generic gamepads)
            case AKEYCODE_BUTTON_A:     // Quest A / Gamepad A
            case AKEYCODE_BUTTON_SELECT:
                set_action(InputAction::Confirm);
                return 1;
            case AKEYCODE_BUTTON_B:     // Quest B / Gamepad B
                set_action(InputAction::Cancel);
                return 1;
            case AKEYCODE_BUTTON_X:     // Quest X (left controller)
                set_action(InputAction::Confirm);
                return 1;
            case AKEYCODE_BUTTON_Y:     // Quest Y (left controller)
                set_action(InputAction::Cancel);
                return 1;
            case AKEYCODE_BUTTON_START: // Menu/Start
            case AKEYCODE_MENU:
                set_action(InputAction::Menu);
                return 1;
            case AKEYCODE_BUTTON_R1:    // Right bumper → Run
            case AKEYCODE_BUTTON_R2:
                set_action(InputAction::Run);
                return 1;

            // D-pad (some controllers)
            case AKEYCODE_DPAD_UP:    set_action(InputAction::MoveUp); return 1;
            case AKEYCODE_DPAD_DOWN:  set_action(InputAction::MoveDown); return 1;
            case AKEYCODE_DPAD_LEFT:  set_action(InputAction::MoveLeft); return 1;
            case AKEYCODE_DPAD_RIGHT: set_action(InputAction::MoveRight); return 1;

            default: break;
        }
    }
    return 0;
}

void PlatformAndroid::process_touch(AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t action_masked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    switch (action_masked) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int32_t id = AMotionEvent_getPointerId(event, pointer_index);
            float x = AMotionEvent_getX(event, pointer_index);
            float y = AMotionEvent_getY(event, pointer_index);
            touch_controls_.touch_down(id, x, y);
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            // Update all active pointers
            int32_t count = AMotionEvent_getPointerCount(event);
            for (int32_t i = 0; i < count; i++) {
                int32_t id = AMotionEvent_getPointerId(event, i);
                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                touch_controls_.touch_move(id, x, y);
            }
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            int32_t id = AMotionEvent_getPointerId(event, pointer_index);
            touch_controls_.touch_up(id);
            break;
        }
        case AMOTION_EVENT_ACTION_CANCEL: {
            // Release all fingers
            for (int i = 0; i < TouchControls::MAX_FINGERS; i++) {
                touch_controls_.touch_up(i);
            }
            break;
        }
    }

    // Apply touch controls to input state
    touch_controls_.apply_to(input_);

    // Map primary touch to mouse state for UI compatibility
    if (AMotionEvent_getPointerCount(event) > 0) {
        input_.mouse.x = AMotionEvent_getX(event, 0);
        input_.mouse.y = AMotionEvent_getY(event, 0);
    }
    // Track touch state for mouse button emulation
    if (action_masked == AMOTION_EVENT_ACTION_DOWN) {
        touch_is_down_ = true;
        touch_just_pressed_ = true;
    } else if (action_masked == AMOTION_EVENT_ACTION_UP ||
               action_masked == AMOTION_EVENT_ACTION_CANCEL) {
        touch_is_down_ = false;
    }
}

} // namespace eb

#endif // __ANDROID__
