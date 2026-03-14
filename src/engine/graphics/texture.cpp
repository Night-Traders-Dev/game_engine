#include "engine/graphics/texture.h"
#include "engine/graphics/vulkan_context.h"
#include "engine/resource/file_io.h"

#include <stb_image.h>
#include <stdexcept>
#include <cstring>

namespace eb {

Texture::Texture(VulkanContext& ctx, const std::string& path) : ctx_(ctx) {
    int w, h, channels;
    stbi_uc* pixels = nullptr;

#ifdef __ANDROID__
    // On Android, read from AAssetManager then decode from memory
    auto data = FileIO::read_file(path);
    if (data.empty()) {
        throw std::runtime_error("Failed to load texture: " + path);
    }
    pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()),
                                    &w, &h, &channels, STBI_rgb_alpha);
#else
    pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
#endif

    if (!pixels) {
        throw std::runtime_error("Failed to decode texture: " + path);
    }

    create_from_pixels(pixels, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    stbi_image_free(pixels);
}

Texture::Texture(VulkanContext& ctx, const uint8_t* pixels, uint32_t width, uint32_t height)
    : ctx_(ctx) {
    create_from_pixels(pixels, width, height);
}

Texture::~Texture() {
    if (sampler_) vkDestroySampler(ctx_.device(), sampler_, nullptr);
    if (view_) vkDestroyImageView(ctx_.device(), view_, nullptr);
    if (image_) vkDestroyImage(ctx_.device(), image_, nullptr);
    if (memory_) vkFreeMemory(ctx_.device(), memory_, nullptr);
}

void Texture::create_from_pixels(const uint8_t* pixels, uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;

    VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * height * 4;

    // Staging buffer
    VkDeviceMemory staging_memory;
    VkBuffer staging_buffer = ctx_.create_buffer(
        image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_memory
    );

    void* data;
    vkMapMemory(ctx_.device(), staging_memory, 0, image_size, 0, &data);
    std::memcpy(data, pixels, image_size);
    vkUnmapMemory(ctx_.device(), staging_memory);

    // Create image
    image_ = ctx_.create_image(
        width, height, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        memory_
    );

    // Transition and copy
    ctx_.transition_image_layout(image_, VK_FORMAT_R8G8B8A8_SRGB,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx_.copy_buffer_to_image(staging_buffer, image_, width, height);
    ctx_.transition_image_layout(image_, VK_FORMAT_R8G8B8A8_SRGB,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging
    vkDestroyBuffer(ctx_.device(), staging_buffer, nullptr);
    vkFreeMemory(ctx_.device(), staging_memory, nullptr);

    // Create view and sampler
    view_ = ctx_.create_image_view(image_, VK_FORMAT_R8G8B8A8_SRGB);
    create_sampler();
}

void Texture::create_sampler() {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_NEAREST; // Pixel art!
    info.minFilter = VK_FILTER_NEAREST;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.anisotropyEnable = VK_FALSE;
    info.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(ctx_.device(), &info, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
}

} // namespace eb
