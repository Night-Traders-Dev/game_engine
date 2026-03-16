#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/vulkan_context.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace eb {

VkVertexInputBindingDescription SpriteVertex::binding_description() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(SpriteVertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::vector<VkVertexInputAttributeDescription> SpriteVertex::attribute_descriptions() {
    std::vector<VkVertexInputAttributeDescription> attrs(3);

    // position
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(SpriteVertex, position);

    // uv
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(SpriteVertex, uv);

    // color
    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(SpriteVertex, color);

    return attrs;
}

SpriteBatch::SpriteBatch(VulkanContext& ctx) : ctx_(ctx) {
    create_buffers();
}

SpriteBatch::~SpriteBatch() {
    if (vertex_data_) {
        vkUnmapMemory(ctx_.device(), vertex_memory_);
    }
    if (vertex_buffer_) {
        vkDestroyBuffer(ctx_.device(), vertex_buffer_, nullptr);
        vkFreeMemory(ctx_.device(), vertex_memory_, nullptr);
    }
    if (index_buffer_) {
        vkDestroyBuffer(ctx_.device(), index_buffer_, nullptr);
        vkFreeMemory(ctx_.device(), index_memory_, nullptr);
    }
}

void SpriteBatch::create_buffers() {
    // Vertex buffer (host visible for dynamic updates)
    VkDeviceSize vertex_size = sizeof(SpriteVertex) * MAX_VERTICES;
    vertex_buffer_ = ctx_.create_buffer(
        vertex_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertex_memory_
    );

    void* data;
    vkMapMemory(ctx_.device(), vertex_memory_, 0, vertex_size, 0, &data);
    vertex_data_ = static_cast<SpriteVertex*>(data);

    // Index buffer (static pattern: 0,1,2, 2,3,0 repeated)
    std::vector<uint32_t> indices(MAX_INDICES);
    for (uint32_t i = 0; i < MAX_QUADS; i++) {
        uint32_t base = i * 4;
        uint32_t idx = i * 6;
        indices[idx + 0] = base + 0;
        indices[idx + 1] = base + 1;
        indices[idx + 2] = base + 2;
        indices[idx + 3] = base + 2;
        indices[idx + 4] = base + 3;
        indices[idx + 5] = base + 0;
    }

    VkDeviceSize index_size = sizeof(uint32_t) * MAX_INDICES;

    VkDeviceMemory staging_memory;
    VkBuffer staging_buffer = ctx_.create_buffer(
        index_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_memory
    );

    void* staging_data;
    vkMapMemory(ctx_.device(), staging_memory, 0, index_size, 0, &staging_data);
    std::memcpy(staging_data, indices.data(), index_size);
    vkUnmapMemory(ctx_.device(), staging_memory);

    index_buffer_ = ctx_.create_buffer(
        index_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        index_memory_
    );

    ctx_.copy_buffer(staging_buffer, index_buffer_, index_size);

    vkDestroyBuffer(ctx_.device(), staging_buffer, nullptr);
    vkFreeMemory(ctx_.device(), staging_memory, nullptr);
}

void SpriteBatch::begin(VkCommandBuffer cmd, VkPipelineLayout layout) {
    cmd_ = cmd;
    layout_ = layout;
    quad_count_ = 0;
    vertex_offset_ = 0;
    current_texture_ = VK_NULL_HANDLE;
    in_batch_ = true;
}

void SpriteBatch::set_projection(const Mat4& projection) {
    if (projection != projection_) {
        flush();
        projection_ = projection;
    }
}

void SpriteBatch::set_texture(VkDescriptorSet descriptor_set) {
    if (descriptor_set != current_texture_) {
        flush();
        current_texture_ = descriptor_set;
    }
}

void SpriteBatch::draw_quad(Vec2 position, Vec2 size, Vec4 color) {
    draw_quad(position, size, {0.0f, 0.0f}, {1.0f, 1.0f}, color);
}

void SpriteBatch::draw_quad(Vec2 position, Vec2 size, Vec2 uv_min, Vec2 uv_max, Vec4 color) {
    if (quad_count_ >= MAX_QUADS || (vertex_offset_ + quad_count_ + 1) > MAX_QUADS) {
        flush();
    }

    uint32_t base = (vertex_offset_ + quad_count_) * 4;

    // Top-left
    vertex_data_[base + 0] = {{position.x, position.y}, {uv_min.x, uv_min.y}, color};
    // Top-right
    vertex_data_[base + 1] = {{position.x + size.x, position.y}, {uv_max.x, uv_min.y}, color};
    // Bottom-right
    vertex_data_[base + 2] = {{position.x + size.x, position.y + size.y}, {uv_max.x, uv_max.y}, color};
    // Bottom-left
    vertex_data_[base + 3] = {{position.x, position.y + size.y}, {uv_min.x, uv_max.y}, color};

    quad_count_++;
}

void SpriteBatch::draw_quad_uvs(Vec2 position, Vec2 size, Vec2 uv_tl, Vec2 uv_tr,
                                 Vec2 uv_br, Vec2 uv_bl, Vec4 color) {
    if (quad_count_ >= MAX_QUADS || (vertex_offset_ + quad_count_ + 1) > MAX_QUADS) {
        flush();
    }
    uint32_t base = (vertex_offset_ + quad_count_) * 4;
    vertex_data_[base + 0] = {{position.x, position.y}, {uv_tl.x, uv_tl.y}, color};
    vertex_data_[base + 1] = {{position.x + size.x, position.y}, {uv_tr.x, uv_tr.y}, color};
    vertex_data_[base + 2] = {{position.x + size.x, position.y + size.y}, {uv_br.x, uv_br.y}, color};
    vertex_data_[base + 3] = {{position.x, position.y + size.y}, {uv_bl.x, uv_bl.y}, color};
    quad_count_++;
}

void SpriteBatch::flush() {
    if (quad_count_ == 0 || !cmd_) return;

    // Push projection matrix
    vkCmdPushConstants(cmd_, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &projection_);

    // Bind texture descriptor set
    if (current_texture_) {
        vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_,
                                0, 1, &current_texture_, 0, nullptr);
    }

    VkBuffer buffers[] = {vertex_buffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmd_, index_buffer_, 0, VK_INDEX_TYPE_UINT32);

    // Draw from the correct offset in the index/vertex buffer
    uint32_t first_index = vertex_offset_ * 6;
    int32_t vertex_off = 0; // vertices are already at the right position in the buffer
    vkCmdDrawIndexed(cmd_, quad_count_ * 6, 1, first_index, vertex_off, 0);

    // Advance offset for next batch (don't reuse buffer region this frame)
    vertex_offset_ += quad_count_;
    quad_count_ = 0;
}

void SpriteBatch::draw_sorted(Vec2 position, Vec2 size, Vec2 uv_min, Vec2 uv_max,
                               float sort_y, VkDescriptorSet texture, Vec4 color) {
    sorted_sprites_.push_back({position, size, uv_min, uv_max, color, sort_y, texture});
}

void SpriteBatch::flush_sorted() {
    if (sorted_sprites_.empty()) return;

    // Sort by Y (back-to-front: smaller Y draws first, larger Y draws on top)
    std::sort(sorted_sprites_.begin(), sorted_sprites_.end(),
              [](const SortedSprite& a, const SortedSprite& b) {
                  return a.sort_y < b.sort_y;
              });

    for (const auto& s : sorted_sprites_) {
        set_texture(s.texture);
        draw_quad(s.position, s.size, s.uv_min, s.uv_max, s.color);
    }

    sorted_sprites_.clear();
}

void SpriteBatch::end() {
    flush_sorted();
    flush();
    in_batch_ = false;
    cmd_ = VK_NULL_HANDLE;
}

} // namespace eb
