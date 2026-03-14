#pragma once

#include "engine/core/types.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

namespace eb {

class VulkanContext;
class Texture;
class SpriteBatch;

struct GlyphInfo {
    float uv_x0, uv_y0, uv_x1, uv_y1; // UV coordinates in atlas
    float x_offset, y_offset;            // Offset from cursor position
    float width, height;                  // Glyph size in pixels
    float advance;                        // Horizontal advance
};

class TextRenderer {
public:
    TextRenderer(VulkanContext& ctx, const std::string& font_path, float font_size);
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    // Draw text at a position (screen or world coordinates depending on projection)
    void draw_text(SpriteBatch& batch, VkDescriptorSet font_desc,
                   const std::string& text, Vec2 position,
                   Vec4 color = Vec4(1.0f), float scale = 1.0f);

    // Measure text dimensions without drawing
    Vec2 measure_text(const std::string& text, float scale = 1.0f) const;

    // Word-wrapped text
    void draw_text_wrapped(SpriteBatch& batch, VkDescriptorSet font_desc,
                           const std::string& text, Vec2 position,
                           float max_width, Vec4 color = Vec4(1.0f),
                           float scale = 1.0f);

    // Get the number of characters that fit in max_width
    int chars_that_fit(const std::string& text, float max_width, float scale = 1.0f) const;

    Texture* texture() const { return atlas_texture_.get(); }
    float line_height() const { return line_height_; }
    float font_size() const { return font_size_; }

    // Extra pixels between each character (default 0)
    void set_letter_spacing(float spacing) { letter_spacing_ = spacing; }
    float letter_spacing() const { return letter_spacing_; }

private:
    void bake_atlas(const std::string& font_path, float font_size);

    VulkanContext& ctx_;
    std::unique_ptr<Texture> atlas_texture_;
    GlyphInfo glyphs_[128]; // ASCII range
    float font_size_;
    float line_height_;
    float ascent_;
    float letter_spacing_ = 0.0f;
};

} // namespace eb
