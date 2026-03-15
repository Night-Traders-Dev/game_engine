#pragma once

#ifdef __ANDROID__

#include "engine/platform/platform.h"
#include "engine/platform/touch_controls.h"

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

    // Access touch controls for rendering
    TouchControls& touch_controls() { return touch_controls_; }
    const TouchControls& touch_controls() const { return touch_controls_; }

private:
    void process_touch(AInputEvent* event);

    ANativeWindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool resized_ = false;
    bool should_close_ = false;
    InputState input_;

    TouchControls touch_controls_;
    bool touch_is_down_ = false;
    bool touch_just_pressed_ = false;
};

} // namespace eb

#endif // __ANDROID__
