#pragma once

#include "engine/platform/platform.h"
#include <string>

struct GLFWwindow;

namespace eb {

class PlatformDesktop : public Platform {
public:
    PlatformDesktop(const std::string& title, int width, int height);
    ~PlatformDesktop() override;

    void poll_events() override;
    bool should_close() const override;

    VkSurfaceKHR create_surface(VkInstance instance) const override;
    std::vector<const char*> get_required_extensions() const override;

    int get_width() const override { return width_; }
    int get_height() const override { return height_; }
    bool was_resized() override;

    const InputState& input() const override { return input_; }

    GLFWwindow* glfw_window() const { return window_; }

private:
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void update_key(int key, int action);

    GLFWwindow* window_ = nullptr;
    int width_;
    int height_;
    bool resized_ = false;
    InputState input_;
};

} // namespace eb
