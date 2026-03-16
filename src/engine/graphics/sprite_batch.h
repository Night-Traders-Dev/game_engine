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
    static constexpr uint32_t MAX_QUADS = 20000;
    static constexpr uint32_t MAX_VERTICES = MAX_QUADS * 4;
    static constexpr uint32_t MAX_INDICES = MAX_QUADS * 6;

    SpriteBatch(VulkanContext& ctx);
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Frame lifecycle
    void begin(VkCommandBuffer cmd, VkPipelineLayout layout);
    void end();

    // Set projection matrix (pushed to shader via push constants)
    void set_projection(const Mat4& projection);

    // Set active texture — auto-flushes if texture changes
    void set_texture(VkDescriptorSet descriptor_set);

    // Draw a colored quad (uses current texture, tinted by color)
    void draw_quad(Vec2 position, Vec2 size, Vec4 color = Vec4(1.0f));
    void draw_quad(Vec2 position, Vec2 size, Vec2 uv_min, Vec2 uv_max,
                   Vec4 color = Vec4(1.0f));
    // Draw with per-corner UVs (for rotation): TL, TR, BR, BL
    void draw_quad_uvs(Vec2 position, Vec2 size, Vec2 uv_tl, Vec2 uv_tr,
                       Vec2 uv_br, Vec2 uv_bl, Vec4 color = Vec4(1.0f));

    // Y-sorted sprite rendering (for proper depth ordering)
    // Collects sprites, then flush_sorted() draws them back-to-front by Y
    void draw_sorted(Vec2 position, Vec2 size, Vec2 uv_min, Vec2 uv_max,
                     float sort_y, VkDescriptorSet texture,
                     Vec4 color = Vec4(1.0f));
    void flush_sorted();

    // Manually flush the current batch
    void flush();

    uint32_t quad_count() const { return quad_count_; }

private:
    struct SortedSprite {
        Vec2 position, size;
        Vec2 uv_min, uv_max;
        Vec4 color;
        float sort_y;
        VkDescriptorSet texture;
    };
    std::vector<SortedSprite> sorted_sprites_;
    void create_buffers();

    VulkanContext& ctx_;
    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory_ = VK_NULL_HANDLE;
    VkBuffer index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory index_memory_ = VK_NULL_HANDLE;

    SpriteVertex* vertex_data_ = nullptr; // Mapped pointer
    uint32_t quad_count_ = 0;     // Quads in current batch (since last flush)
    uint32_t vertex_offset_ = 0;  // Running vertex offset across flushes within a frame
    bool in_batch_ = false;

    // Per-frame state (set during begin)
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    Mat4 projection_{1.0f};
    VkDescriptorSet current_texture_ = VK_NULL_HANDLE;
};

} // namespace eb
