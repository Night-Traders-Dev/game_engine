#pragma once

#include "engine/platform/platform.h"
#include <string>

struct GLFWwindow;

namespace eb {

class PlatformDesktop : public Platform {
public:
    PlatformDesktop(const std::string& title, int width, int height, bool fullscreen = false);
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
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void update_key(int key, int action, int mods);

    GLFWwindow* window_ = nullptr;
    int width_;
    int height_;
    bool resized_ = false;
    InputState input_;
    KeyBindings key_bindings_;
public:
    KeyBindings& key_bindings() { return key_bindings_; }
    const KeyBindings& key_bindings() const { return key_bindings_; }
};

} // namespace eb
