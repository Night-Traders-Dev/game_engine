#pragma once

#include "engine/core/types.h"
#include "engine/graphics/vulkan_context.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/pipeline.h"
#include "engine/graphics/texture.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace eb {

class Platform;

class Renderer {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr int MAX_TEXTURE_DESCRIPTORS = 256;

    Renderer(Platform& platform, bool vsync);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool begin_frame();
    void end_frame();

    SpriteBatch& sprite_batch() { return *sprite_batch_; }
    VulkanContext& vulkan_context() { return *ctx_; }

    void set_clear_color(float r, float g, float b, float a = 1.0f);

    int width() const;
    int height() const;

    void set_shader_dir(const std::string& dir) { shader_dir_ = dir; }

    // Texture descriptor management
    VkDescriptorSet get_texture_descriptor(const Texture& tex);
    VkDescriptorSet default_texture_descriptor() const { return default_tex_descriptor_; }

    // Screen-space orthographic projection (origin top-left)
    Mat4 screen_projection() const;

    VkPipelineLayout pipeline_layout() const { return pipeline_layout_; }
    VkCommandBuffer current_command_buffer() const { return command_buffers_[current_frame_]; }
    VkRenderPass render_pass() const { return render_pass_; }

private:
    void create_render_pass();
    void create_framebuffers();
    void create_sync_objects();
    void create_command_buffers();
    void create_descriptor_set_layout();
    void create_descriptor_pool();
    void create_pipeline_layout();
    void create_sprite_pipeline();
    void create_default_texture();
    void cleanup_framebuffers();
    void handle_resize();

    Platform& platform_;
    std::unique_ptr<VulkanContext> ctx_;
    std::unique_ptr<SpriteBatch> sprite_batch_;
    std::unique_ptr<Pipeline> sprite_pipeline_;
    std::unique_ptr<Texture> default_texture_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    // Cached descriptor sets per texture image view
    std::unordered_map<VkImageView, VkDescriptorSet> texture_descriptors_;
    VkDescriptorSet default_tex_descriptor_ = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    std::vector<VkFence> in_flight_;
    std::vector<VkFence> images_in_flight_; // Per swapchain image

    uint32_t current_frame_ = 0;
    uint32_t image_index_ = 0;

    float clear_r_ = 0.1f, clear_g_ = 0.1f, clear_b_ = 0.15f, clear_a_ = 1.0f;
    std::string shader_dir_ = "shaders/";
};

} // namespace eb
