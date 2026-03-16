#include "engine/platform/platform_desktop.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdio>

namespace eb {

PlatformDesktop::PlatformDesktop(const std::string& title, int width, int height, bool fullscreen)
    : width_(width), height_(height) {
    // Allow forcing X11 via environment variable (for test tooling on Wayland)
    if (std::getenv("TW_FORCE_X11")) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
    // Disable libdecor on Wayland to avoid fontconfig crash in some distros
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWmonitor* monitor = nullptr;
    if (fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            width_ = mode->width;
            height_ = mode->height;
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        }
    }

    window_ = glfwCreateWindow(width_, height_, title.c_str(), monitor, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, key_callback);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window_, mouse_button_callback);
    glfwSetCursorPosCallback(window_, cursor_pos_callback);
    glfwSetScrollCallback(window_, scroll_callback);

    std::printf("[Platform] Desktop window created (%dx%d%s)\n",
                width_, height_, fullscreen ? ", fullscreen" : "");
}

PlatformDesktop::~PlatformDesktop() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void PlatformDesktop::poll_events() {
    input_.clear_frame();
    input_.input_time += 0.016f; // Approximate frame time for buffer timestamps
    glfwPollEvents();

    // ── Gamepad input ──
    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) {
        GLFWgamepadstate gp;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &gp)) {
            auto& pad = input_.gamepad;
            pad.connected = true;

            // Axes
            for (int i = 0; i < 6 && i < 6; i++) pad.axes[i] = gp.axes[i];

            // Buttons (detect press transitions)
            for (int i = 0; i < 15; i++) {
                bool was = pad.buttons[i];
                pad.buttons[i] = gp.buttons[i] == GLFW_PRESS;
                if (pad.buttons[i] && !was) pad.buttons_pressed[i] = true;
            }

            // Map gamepad to game actions
            float lx = pad.axes[0], ly = pad.axes[1];
            float dz = InputState::GamepadState::DEADZONE;
            if (ly < -dz) { input_.actions[static_cast<int>(InputAction::MoveUp)] = true; }
            if (ly > dz)  { input_.actions[static_cast<int>(InputAction::MoveDown)] = true; }
            if (lx < -dz) { input_.actions[static_cast<int>(InputAction::MoveLeft)] = true; }
            if (lx > dz)  { input_.actions[static_cast<int>(InputAction::MoveRight)] = true; }

            // D-pad
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP])    input_.actions[static_cast<int>(InputAction::MoveUp)] = true;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN])  input_.actions[static_cast<int>(InputAction::MoveDown)] = true;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT])  input_.actions[static_cast<int>(InputAction::MoveLeft)] = true;
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) input_.actions[static_cast<int>(InputAction::MoveRight)] = true;

            // Face buttons
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_A]) {
                input_.actions[static_cast<int>(InputAction::Confirm)] = true;
                if (pad.buttons_pressed[GLFW_GAMEPAD_BUTTON_A])
                    input_.actions_pressed[static_cast<int>(InputAction::Confirm)] = true;
            }
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_B]) {
                input_.actions[static_cast<int>(InputAction::Cancel)] = true;
                if (pad.buttons_pressed[GLFW_GAMEPAD_BUTTON_B])
                    input_.actions_pressed[static_cast<int>(InputAction::Cancel)] = true;
            }
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_START]) {
                input_.actions[static_cast<int>(InputAction::Menu)] = true;
                if (pad.buttons_pressed[GLFW_GAMEPAD_BUTTON_START])
                    input_.actions_pressed[static_cast<int>(InputAction::Menu)] = true;
            }
            // RB = Run
            if (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER])
                input_.actions[static_cast<int>(InputAction::Run)] = true;
        }
    } else {
        input_.gamepad.connected = false;
    }
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

void PlatformDesktop::key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->update_key(key, action, mods);
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

void PlatformDesktop::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (!platform || button < 0 || button >= MouseState::BUTTON_COUNT) return;

    if (action == GLFW_PRESS) {
        platform->input_.mouse.buttons[button] = true;
        platform->input_.mouse.buttons_pressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        platform->input_.mouse.buttons[button] = false;
        platform->input_.mouse.buttons_released[button] = true;
    }
}

void PlatformDesktop::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->input_.mouse.x = static_cast<float>(xpos);
        platform->input_.mouse.y = static_cast<float>(ypos);
    }
}

void PlatformDesktop::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* platform = static_cast<PlatformDesktop*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->input_.mouse.scroll_x += static_cast<float>(xoffset);
        platform->input_.mouse.scroll_y += static_cast<float>(yoffset);
    }
}

void PlatformDesktop::update_key(int key, int action, int mods) {
    // Update modifier state
    input_.mods.ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    input_.mods.shift = (mods & GLFW_MOD_SHIFT) != 0;
    input_.mods.alt = (mods & GLFW_MOD_ALT) != 0;

    // Raw key tracking
    if (key >= 0 && key < InputState::MAX_KEYS) {
        if (action == GLFW_PRESS) {
            input_.keys[key] = true;
            input_.keys_pressed[key] = true;
        } else if (action == GLFW_RELEASE) {
            input_.keys[key] = false;
        }
    }

    // Game action mapping (rebindable via key_bindings_)
    auto set_action = [&](InputAction ia) {
        int idx = static_cast<int>(ia);
        if (action == GLFW_PRESS) {
            input_.actions[idx] = true;
            input_.actions_pressed[idx] = true;
            input_.buffer_press(ia);
        } else if (action == GLFW_RELEASE) {
            input_.actions[idx] = false;
            input_.actions_released[idx] = true;
        }
    };

    // Check rebindable bindings
    for (int i = 0; i < KeyBindings::ACTION_COUNT; i++) {
        if (key_bindings_.matches(static_cast<InputAction>(i), key)) {
            set_action(static_cast<InputAction>(i));
            break;
        }
    }
}

} // namespace eb
