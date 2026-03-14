#pragma once

#include "engine/core/types.h"
#include "engine/graphics/vulkan_context.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/pipeline.h"

#include <memory>
#include <string>

namespace eb {

class Platform;

class Renderer {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

private:
    void create_render_pass();
    void create_framebuffers();
    void create_sync_objects();
    void create_command_buffers();
    void create_pipeline_layout();
    void create_sprite_pipeline();
    void cleanup_framebuffers();
    void handle_resize();

    Platform& platform_;
    std::unique_ptr<VulkanContext> ctx_;
    std::unique_ptr<SpriteBatch> sprite_batch_;
    std::unique_ptr<Pipeline> sprite_pipeline_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    std::vector<VkFence> in_flight_;

    uint32_t current_frame_ = 0;
    uint32_t image_index_ = 0;

    float clear_r_ = 0.1f, clear_g_ = 0.1f, clear_b_ = 0.15f, clear_a_ = 1.0f;
    std::string shader_dir_ = "shaders/";
};

} // namespace eb
