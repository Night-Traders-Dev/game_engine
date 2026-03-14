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
    create_descriptor_set_layout();
    create_descriptor_pool();
    create_pipeline_layout();
    create_default_texture();
    sprite_batch_ = std::make_unique<SpriteBatch>(*ctx_);
    std::printf("[Renderer] Initialized\n");
}

Renderer::~Renderer() {
    if (ctx_) {
        ctx_->wait_idle();

        sprite_pipeline_.reset();
        sprite_batch_.reset();
        default_texture_.reset();
        texture_descriptors_.clear();

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(ctx_->device(), image_available_[i], nullptr);
            vkDestroySemaphore(ctx_->device(), render_finished_[i], nullptr);
            vkDestroyFence(ctx_->device(), in_flight_[i], nullptr);
        }

        if (descriptor_pool_) vkDestroyDescriptorPool(ctx_->device(), descriptor_pool_, nullptr);
        if (descriptor_set_layout_) vkDestroyDescriptorSetLayout(ctx_->device(), descriptor_set_layout_, nullptr);
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

    // Per-swapchain-image fence tracking (initialized to null)
    images_in_flight_.resize(ctx_->swapchain_image_views().size(), VK_NULL_HANDLE);
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

void Renderer::create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding sampler_binding{};
    sampler_binding.binding = 0;
    sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &sampler_binding;

    if (vkCreateDescriptorSetLayout(ctx_->device(), &info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void Renderer::create_descriptor_pool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = MAX_TEXTURE_DESCRIPTORS;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 1;
    info.pPoolSizes = &pool_size;
    info.maxSets = MAX_TEXTURE_DESCRIPTORS;

    if (vkCreateDescriptorPool(ctx_->device(), &info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
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
    info.setLayoutCount = 1;
    info.pSetLayouts = &descriptor_set_layout_;

    if (vkCreatePipelineLayout(ctx_->device(), &info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void Renderer::create_default_texture() {
    // 1x1 white pixel — used when drawing colored quads without a texture
    uint8_t white_pixel[] = {255, 255, 255, 255};
    default_texture_ = std::make_unique<Texture>(*ctx_, white_pixel, 1, 1);
    default_tex_descriptor_ = get_texture_descriptor(*default_texture_);
    std::printf("[Renderer] Default white texture created\n");
}

VkDescriptorSet Renderer::get_texture_descriptor(const Texture& tex) {
    auto it = texture_descriptors_.find(tex.image_view());
    if (it != texture_descriptors_.end()) {
        return it->second;
    }

    // Allocate new descriptor set
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    VkDescriptorSet desc_set;
    if (vkAllocateDescriptorSets(ctx_->device(), &alloc_info, &desc_set) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture descriptor set");
    }

    // Update with texture image/sampler
    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = tex.image_view();
    image_info.sampler = tex.sampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = desc_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(ctx_->device(), 1, &write, 0, nullptr);

    texture_descriptors_[tex.image_view()] = desc_set;
    return desc_set;
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
    if (w == 0 || h == 0) return;

    ctx_->wait_idle();
    cleanup_framebuffers();

    vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr);
    ctx_->recreate_swapchain(w, h);
    create_render_pass();
    create_framebuffers();

    sprite_pipeline_.reset();
    create_sprite_pipeline();

    images_in_flight_.assign(ctx_->swapchain_image_views().size(), VK_NULL_HANDLE);
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

Mat4 Renderer::screen_projection() const {
    auto extent = ctx_->swapchain_extent();
    // Vulkan clip space: +Y is down, so bottom < top
    return glm::ortho(0.0f, static_cast<float>(extent.width),
                      0.0f, static_cast<float>(extent.height),
                      -1.0f, 1.0f);
}

bool Renderer::begin_frame() {
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

    // Wait if this swapchain image is still in use by a previous frame
    if (images_in_flight_[image_index_] != VK_NULL_HANDLE) {
        vkWaitForFences(ctx_->device(), 1, &images_in_flight_[image_index_], VK_TRUE, UINT64_MAX);
    }
    images_in_flight_[image_index_] = in_flight_[current_frame_];

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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sprite_pipeline_->handle());

    // Initialize sprite batch with default screen projection and white texture
    sprite_batch_->begin(cmd, pipeline_layout_);
    sprite_batch_->set_projection(screen_projection());
    sprite_batch_->set_texture(default_tex_descriptor_);

    return true;
}

void Renderer::end_frame() {
    auto cmd = command_buffers_[current_frame_];

    sprite_batch_->end();

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

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
