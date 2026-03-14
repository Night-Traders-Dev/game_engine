#pragma once

#ifdef __ANDROID__

#include "engine/platform/platform.h"

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/log.h>

namespace eb {

class PlatformAndroid : public Platform {
public:
    PlatformAndroid(ANativeWindow* window);
    ~PlatformAndroid() override;

    void poll_events() override;
    bool should_close() const override;

    VkSurfaceKHR create_surface(VkInstance instance) const override;
    std::vector<const char*> get_required_extensions() const override;

    int get_width() const override;
    int get_height() const override;
    bool was_resized() override;

    const InputState& input() const override { return input_; }

    void set_window(ANativeWindow* window);
    void set_should_close(bool close) { should_close_ = close; }

    // Touch input processing
    int32_t handle_input(AInputEvent* event);
    void handle_resize(int width, int height);

private:
    void process_touch(AInputEvent* event);

    ANativeWindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool resized_ = false;
    bool should_close_ = false;
    InputState input_;

    // Virtual d-pad touch tracking
    struct TouchState {
        bool active = false;
        float x = 0.0f;
        float y = 0.0f;
    };
    TouchState primary_touch_;
};

} // namespace eb

#endif // __ANDROID__
