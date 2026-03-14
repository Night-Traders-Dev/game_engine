#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <string>

namespace eb {

class Platform;

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class VulkanContext {
public:
    VulkanContext(Platform& platform, bool vsync);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void wait_idle();
    void recreate_swapchain(int width, int height);

    // Buffer helpers
    VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkDeviceMemory& memory);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    // Image helpers
    VkImage create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageTiling tiling, VkImageUsageFlags usage,
                         VkMemoryPropertyFlags properties, VkDeviceMemory& memory);
    VkImageView create_image_view(VkImage image, VkFormat format,
                                  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void transition_image_layout(VkImage image, VkFormat format,
                                 VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // Command helpers
    VkCommandBuffer begin_single_command();
    void end_single_command(VkCommandBuffer cmd);

    // Shader
    VkShaderModule create_shader_module(const std::vector<char>& code);

    // Accessors
    VkInstance instance() const { return instance_; }
    VkDevice device() const { return device_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    VkQueue present_queue() const { return present_queue_; }
    VkCommandPool command_pool() const { return command_pool_; }
    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkFormat swapchain_format() const { return swapchain_format_; }
    VkExtent2D swapchain_extent() const { return swapchain_extent_; }
    const std::vector<VkImageView>& swapchain_image_views() const { return swapchain_image_views_; }
    uint32_t graphics_family() const { return graphics_family_; }

private:
    void create_instance(Platform& platform);
    void create_surface(Platform& platform);
    void pick_physical_device();
    void create_logical_device();
    void create_command_pool();
    void create_swapchain(int width, int height);
    void cleanup_swapchain();

    SwapchainSupport query_swapchain_support(VkPhysicalDevice device);
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_family_ = 0;
    uint32_t present_family_ = 0;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_;
    VkExtent2D swapchain_extent_;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;

    bool vsync_;

#ifdef NDEBUG
    static constexpr bool enable_validation_ = false;
#else
    static constexpr bool enable_validation_ = true;
#endif
    bool validation_active_ = false; // Set at runtime if layers were found
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    void setup_debug_messenger();
    void destroy_debug_messenger();
};

} // namespace eb
