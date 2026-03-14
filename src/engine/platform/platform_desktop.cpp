#include "engine/platform/platform_desktop.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdio>

namespace eb {

PlatformDesktop::PlatformDesktop(const std::string& title, int width, int height)
    : width_(width), height_(height) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, key_callback);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

    std::printf("[Platform] Desktop window created (%dx%d)\n", width, height);
}

PlatformDesktop::~PlatformDesktop() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void PlatformDesktop::poll_events() {
    input_.clear_frame();
    glfwPollEvents();
}

bool PlatformDesktop::should_close() const {
    return glfwWindowShouldClose(window_);
}

VkSurfaceKHR PlatformDesktop::create_surface(VkInstance instance) const {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    return surface;
}

std::vector<const char*> PlatformDesktop::get_required_extensions() const {
    uint32_t count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(glfw_extensions, glfw_extensions + count);
}

bool PlatformDesktop::was_resized() {
    bool r = resized_;
    resized_ = false;
    return r;
}

void PlatformDesktop::key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->update_key(key, action);
    }
}

void PlatformDesktop::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->width_ = width;
        platform->height_ = height;
        platform->resized_ = true;
    }
}

void PlatformDesktop::update_key(int key, int action) {
    auto set_action = [&](InputAction ia) {
        int idx = static_cast<int>(ia);
        if (action == GLFW_PRESS) {
            input_.actions[idx] = true;
            input_.actions_pressed[idx] = true;
        } else if (action == GLFW_RELEASE) {
            input_.actions[idx] = false;
            input_.actions_released[idx] = true;
        }
    };

    switch (key) {
        case GLFW_KEY_W: case GLFW_KEY_UP:    set_action(InputAction::MoveUp);    break;
        case GLFW_KEY_S: case GLFW_KEY_DOWN:   set_action(InputAction::MoveDown);  break;
        case GLFW_KEY_A: case GLFW_KEY_LEFT:   set_action(InputAction::MoveLeft);  break;
        case GLFW_KEY_D: case GLFW_KEY_RIGHT:  set_action(InputAction::MoveRight); break;
        case GLFW_KEY_Z: case GLFW_KEY_ENTER:  set_action(InputAction::Confirm);   break;
        case GLFW_KEY_X: case GLFW_KEY_BACKSPACE: set_action(InputAction::Cancel); break;
        case GLFW_KEY_ESCAPE:                  set_action(InputAction::Menu);      break;
        case GLFW_KEY_LEFT_SHIFT:              set_action(InputAction::Run);       break;
        default: break;
    }

}

} // namespace eb
