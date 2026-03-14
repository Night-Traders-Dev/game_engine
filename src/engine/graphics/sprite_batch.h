#pragma once

#include "engine/core/types.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace eb {

class VulkanContext;
class Texture;

struct SpriteVertex {
    glm::vec2 position;
    glm::vec2 uv;
    glm::vec4 color;

    static VkVertexInputBindingDescription binding_description();
    static std::vector<VkVertexInputAttributeDescription> attribute_descriptions();
};

class SpriteBatch {
public:
    static constexpr uint32_t MAX_QUADS = 10000;
    static constexpr uint32_t MAX_VERTICES = MAX_QUADS * 4;
    static constexpr uint32_t MAX_INDICES = MAX_QUADS * 6;

    SpriteBatch(VulkanContext& ctx);
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    void begin();
    void draw_quad(Vec2 position, Vec2 size, Vec4 color = Vec4(1.0f));
    void draw_quad(Vec2 position, Vec2 size, Vec2 uv_min, Vec2 uv_max,
                   Vec4 color = Vec4(1.0f));
    void flush(VkCommandBuffer cmd, VkPipelineLayout layout, const Mat4& projection);
    void end();

    uint32_t quad_count() const { return quad_count_; }

private:
    void create_buffers();

    VulkanContext& ctx_;
    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory_ = VK_NULL_HANDLE;
    VkBuffer index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory index_memory_ = VK_NULL_HANDLE;

    SpriteVertex* vertex_data_ = nullptr; // Mapped pointer
    uint32_t quad_count_ = 0;
    bool in_batch_ = false;
};

} // namespace eb
