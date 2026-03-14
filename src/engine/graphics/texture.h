#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>

namespace eb {

class VulkanContext;

class Texture {
public:
    Texture(VulkanContext& ctx, const std::string& path);
    Texture(VulkanContext& ctx, const uint8_t* pixels, uint32_t width, uint32_t height);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    VkImage image() const { return image_; }
    VkImageView image_view() const { return view_; }
    VkSampler sampler() const { return sampler_; }

private:
    void create_from_pixels(const uint8_t* pixels, uint32_t width, uint32_t height);
    void create_sampler();

    VulkanContext& ctx_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

} // namespace eb
