#ifdef __ANDROID__

#include "engine/platform/platform_android.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <stdexcept>
#include <cmath>

#define LOG_TAG "EBEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace eb {

PlatformAndroid::PlatformAndroid(ANativeWindow* window) : window_(window) {
    if (window_) {
        width_ = ANativeWindow_getWidth(window_);
        height_ = ANativeWindow_getHeight(window_);
    }
    LOGI("PlatformAndroid created (%dx%d)", width_, height_);
}

PlatformAndroid::~PlatformAndroid() {
    // We don't own the window, so don't release it
}

void PlatformAndroid::poll_events() {
    input_.clear_frame();
    // Events are pushed from android_main's event loop via handle_input()
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
    bool r = resized_;
    resized_ = false;
    return r;
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
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        process_touch(event);
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t key = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);
        // Handle back button as Menu/Cancel
        if (key == AKEYCODE_BACK) {
            int idx = static_cast<int>(InputAction::Menu);
            if (action == AKEY_EVENT_ACTION_DOWN) {
                input_.actions[idx] = true;
                input_.actions_pressed[idx] = true;
            } else if (action == AKEY_EVENT_ACTION_UP) {
                input_.actions[idx] = false;
                input_.actions_released[idx] = true;
            }
            return 1;
        }
    }
    return 0;
}

void PlatformAndroid::process_touch(AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t action_masked = action & AMOTION_EVENT_ACTION_MASK;

    float x = AMotionEvent_getX(event, 0);
    float y = AMotionEvent_getY(event, 0);

    float screen_w = static_cast<float>(width_);
    float screen_h = static_cast<float>(height_);

    switch (action_masked) {
        case AMOTION_EVENT_ACTION_DOWN: {
            primary_touch_.active = true;
            primary_touch_.x = x;
            primary_touch_.y = y;

            // Left side of screen = virtual d-pad area
            // Right side = action buttons
            if (x > screen_w * 0.6f) {
                // Right side: bottom = confirm, top = cancel
                if (y > screen_h * 0.5f) {
                    int idx = static_cast<int>(InputAction::Confirm);
                    input_.actions[idx] = true;
                    input_.actions_pressed[idx] = true;
                } else {
                    int idx = static_cast<int>(InputAction::Cancel);
                    input_.actions[idx] = true;
                    input_.actions_pressed[idx] = true;
                }
            }
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            if (primary_touch_.active && x < screen_w * 0.5f) {
                // Virtual d-pad: compute direction from initial touch
                float dx = x - primary_touch_.x;
                float dy = y - primary_touch_.y;
                float dead_zone = screen_w * 0.03f;

                // Reset directional actions
                for (int i = static_cast<int>(InputAction::MoveUp);
                     i <= static_cast<int>(InputAction::MoveRight); i++) {
                    input_.actions[i] = false;
                }

                if (std::abs(dx) > dead_zone || std::abs(dy) > dead_zone) {
                    if (std::abs(dx) > std::abs(dy)) {
                        int idx = static_cast<int>(dx > 0 ? InputAction::MoveRight : InputAction::MoveLeft);
                        input_.actions[idx] = true;
                    } else {
                        int idx = static_cast<int>(dy > 0 ? InputAction::MoveDown : InputAction::MoveUp);
                        input_.actions[idx] = true;
                    }
                }
            }
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL: {
            primary_touch_.active = false;
            // Release all held actions
            for (int i = 0; i < InputState::ACTION_COUNT; i++) {
                if (input_.actions[i]) {
                    input_.actions[i] = false;
                    input_.actions_released[i] = true;
                }
            }
            break;
        }
    }
}

} // namespace eb

#endif // __ANDROID__
