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
    LOGI("PlatformAndroid created (%dx%d)", width_, height_);
}

PlatformAndroid::~PlatformAndroid() = default;

void PlatformAndroid::poll_events() {
    input_.clear_frame();
    // Apply touch state BEFORE begin_frame clears the pressed flags
    touch_controls_.apply_to(input_);
    // Now reset pressed flags for next frame
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

    // Also update mouse state from primary touch for compatibility
    if (AMotionEvent_getPointerCount(event) > 0) {
        input_.mouse.x = AMotionEvent_getX(event, 0);
        input_.mouse.y = AMotionEvent_getY(event, 0);
    }
}

} // namespace eb

#endif // __ANDROID__
