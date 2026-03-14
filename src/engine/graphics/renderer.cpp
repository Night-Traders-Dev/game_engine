#include "engine/graphics/renderer.h"
#include "engine/platform/platform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cstdio>
#include <array>

namespace eb {

Renderer::Renderer(Platform& platform, bool vsync) : platform_(platform) {
    ctx_ = std::make_unique<VulkanContext>(platform, vsync);
    create_render_pass();
    create_framebuffers();
    create_sync_objects();
    create_command_buffers();
    create_pipeline_layout();
    sprite_batch_ = std::make_unique<SpriteBatch>(*ctx_);
    // Pipeline creation is deferred until shaders are available
    std::printf("[Renderer] Initialized\n");
}

Renderer::~Renderer() {
    if (ctx_) {
        ctx_->wait_idle();

        sprite_pipeline_.reset();
        sprite_batch_.reset();

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(ctx_->device(), image_available_[i], nullptr);
            vkDestroySemaphore(ctx_->device(), render_finished_[i], nullptr);
            vkDestroyFence(ctx_->device(), in_flight_[i], nullptr);
        }

        if (pipeline_layout_) vkDestroyPipelineLayout(ctx_->device(), pipeline_layout_, nullptr);
        cleanup_framebuffers();
        if (render_pass_) vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr);
    }
}

void Renderer::create_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = ctx_->swapchain_format();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color_attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(ctx_->device(), &info, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void Renderer::create_framebuffers() {
    auto& views = ctx_->swapchain_image_views();
    auto extent = ctx_->swapchain_extent();
    framebuffers_.resize(views.size());

    for (size_t i = 0; i < views.size(); i++) {
        VkImageView attachments[] = {views[i]};

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(ctx_->device(), &info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void Renderer::cleanup_framebuffers() {
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    }
    framebuffers_.clear();
}

void Renderer::create_sync_objects() {
    image_available_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx_->device(), &sem_info, nullptr, &image_available_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(ctx_->device(), &sem_info, nullptr, &render_finished_[i]) != VK_SUCCESS ||
            vkCreateFence(ctx_->device(), &fence_info, nullptr, &in_flight_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}

void Renderer::create_command_buffers() {
    command_buffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = ctx_->command_pool();
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

    if (vkAllocateCommandBuffers(ctx_->device(), &info, command_buffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void Renderer::create_pipeline_layout() {
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(Mat4);

    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push_range;
    info.setLayoutCount = 0;

    if (vkCreatePipelineLayout(ctx_->device(), &info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void Renderer::create_sprite_pipeline() {
    PipelineConfig config;
    config.vert_shader_path = shader_dir_ + "sprite.vert.spv";
    config.frag_shader_path = shader_dir_ + "sprite.frag.spv";
    config.binding_descriptions = {SpriteVertex::binding_description()};
    config.attribute_descriptions = SpriteVertex::attribute_descriptions();
    config.pipeline_layout = pipeline_layout_;
    config.render_pass = render_pass_;
    config.alpha_blending = true;

    sprite_pipeline_ = std::make_unique<Pipeline>(*ctx_, config);
    std::printf("[Renderer] Sprite pipeline created\n");
}

void Renderer::handle_resize() {
    int w = platform_.get_width();
    int h = platform_.get_height();
    if (w == 0 || h == 0) return; // Minimized

    ctx_->wait_idle();
    cleanup_framebuffers();

    // Recreate render pass (format may change)
    vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr);
    ctx_->recreate_swapchain(w, h);
    create_render_pass();
    create_framebuffers();

    // Recreate pipeline with new render pass
    sprite_pipeline_.reset();
    create_sprite_pipeline();
}

void Renderer::set_clear_color(float r, float g, float b, float a) {
    clear_r_ = r; clear_g_ = g; clear_b_ = b; clear_a_ = a;
}

int Renderer::width() const {
    return static_cast<int>(ctx_->swapchain_extent().width);
}

int Renderer::height() const {
    return static_cast<int>(ctx_->swapchain_extent().height);
}

bool Renderer::begin_frame() {
    // Lazy-init the sprite pipeline on first frame
    if (!sprite_pipeline_) {
        try {
            create_sprite_pipeline();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Renderer] Cannot create pipeline: %s\n", e.what());
            return false;
        }
    }

    vkWaitForFences(ctx_->device(), 1, &in_flight_[current_frame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        ctx_->device(), ctx_->swapchain(), UINT64_MAX,
        image_available_[current_frame_], VK_NULL_HANDLE, &image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handle_resize();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(ctx_->device(), 1, &in_flight_[current_frame_]);

    auto cmd = command_buffers_[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkClearValue clear_value{};
    clear_value.color = {{clear_r_, clear_g_, clear_b_, clear_a_}};

    VkRenderPassBeginInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = render_pass_;
    rp_info.framebuffer = framebuffers_[image_index_];
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = ctx_->swapchain_extent();
    rp_info.clearValueCount = 1;
    rp_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic viewport and scissor
    auto extent = ctx_->swapchain_extent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind sprite pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sprite_pipeline_->handle());

    // Begin sprite batch
    sprite_batch_->begin();

    return true;
}

void Renderer::end_frame() {
    auto cmd = command_buffers_[current_frame_];
    auto extent = ctx_->swapchain_extent();

    // Flush sprite batch with orthographic projection
    Mat4 projection = glm::ortho(0.0f, static_cast<float>(extent.width),
                                  static_cast<float>(extent.height), 0.0f,
                                  -1.0f, 1.0f);
    sprite_batch_->flush(cmd, pipeline_layout_, projection);
    sprite_batch_->end();

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // Submit
    VkSemaphore wait_semaphores[] = {image_available_[current_frame_]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {render_finished_[current_frame_]};

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = wait_semaphores;
    submit.pWaitDstStageMask = wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(ctx_->graphics_queue(), 1, &submit, in_flight_[current_frame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    // Present
    VkSwapchainKHR swapchains[] = {ctx_->swapchain()};
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signal_semaphores;
    present.swapchainCount = 1;
    present.pSwapchains = swapchains;
    present.pImageIndices = &image_index_;

    VkResult result = vkQueuePresentKHR(ctx_->present_queue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || platform_.was_resized()) {
        handle_resize();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace eb
