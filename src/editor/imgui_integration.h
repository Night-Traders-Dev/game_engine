#pragma once

#ifndef EB_ANDROID

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace eb {

class VulkanContext;
class Renderer;

class ImGuiIntegration {
public:
    ImGuiIntegration() = default;
    ~ImGuiIntegration();

    // Initialize ImGui with our Vulkan renderer
    bool init(GLFWwindow* window, Renderer& renderer);
    void shutdown();

    // Call at start of frame (after begin_frame)
    void new_frame();

    // Call at end of frame (before end_frame submits)
    void render(VkCommandBuffer cmd);

    bool is_initialized() const { return initialized_; }

    // Check if ImGui wants to capture input
    bool wants_keyboard() const;
    bool wants_mouse() const;

private:
    bool initialized_ = false;
    VkDescriptorPool imgui_pool_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
};

} // namespace eb

#endif // EB_ANDROID
