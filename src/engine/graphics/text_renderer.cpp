#include "engine/graphics/text_renderer.h"
#include "engine/graphics/vulkan_context.h"
#include "engine/graphics/texture.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/resource/file_io.h"

#include <stb_truetype.h>
using eb::FileIO;
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace eb {

TextRenderer::TextRenderer(VulkanContext& ctx, const std::string& font_path, float font_size)
    : ctx_(ctx), font_size_(font_size) {
    std::memset(glyphs_, 0, sizeof(glyphs_));
    bake_atlas(font_path, font_size);
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::bake_atlas(const std::string& font_path, float font_size) {
    // Read font file
    auto font_data = FileIO::read_file(font_path);
    if (font_data.empty()) {
        std::fprintf(stderr, "[TextRenderer] Failed to load font: %s\n", font_path.c_str());
        return;
    }

    // Initialize stb_truetype
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_data.data(), 0)) {
        std::fprintf(stderr, "[TextRenderer] Failed to init font\n");
        return;
    }

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent_i, descent_i, line_gap_i;
    stbtt_GetFontVMetrics(&font, &ascent_i, &descent_i, &line_gap_i);
    ascent_ = ascent_i * scale;
    line_height_ = (ascent_i - descent_i + line_gap_i) * scale;

    // Determine atlas size — pack ASCII 32-126
    const int ATLAS_W = 512;
    const int ATLAS_H = 512;
    std::vector<uint8_t> bitmap(ATLAS_W * ATLAS_H, 0);

    // Pack glyphs
    stbtt_pack_context pack_ctx;
    stbtt_PackBegin(&pack_ctx, bitmap.data(), ATLAS_W, ATLAS_H, 0, 1, nullptr);
    stbtt_PackSetOversampling(&pack_ctx, 2, 2);

    stbtt_packedchar packed_chars[96]; // 32-127
    stbtt_PackFontRange(&pack_ctx, font_data.data(), 0, font_size, 32, 96, packed_chars);
    stbtt_PackEnd(&pack_ctx);

    // Extract glyph info
    for (int c = 32; c < 128; c++) {
        const auto& pc = packed_chars[c - 32];
        auto& g = glyphs_[c];
        g.uv_x0 = pc.x0 / (float)ATLAS_W;
        g.uv_y0 = pc.y0 / (float)ATLAS_H;
        g.uv_x1 = pc.x1 / (float)ATLAS_W;
        g.uv_y1 = pc.y1 / (float)ATLAS_H;
        g.x_offset = pc.xoff;
        g.y_offset = pc.yoff;
        g.width = (float)(pc.x1 - pc.x0);
        g.height = (float)(pc.y1 - pc.y0);
        g.advance = pc.xadvance;
    }

    // Convert single-channel bitmap to RGBA
    std::vector<uint8_t> rgba(ATLAS_W * ATLAS_H * 4);
    for (int i = 0; i < ATLAS_W * ATLAS_H; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    atlas_texture_ = std::make_unique<Texture>(ctx_, rgba.data(), ATLAS_W, ATLAS_H);
    std::printf("[TextRenderer] Font atlas baked: %dx%d, size=%.0f\n", ATLAS_W, ATLAS_H, font_size);
}

void TextRenderer::draw_text(SpriteBatch& batch, VkDescriptorSet font_desc,
                              const std::string& text, Vec2 position,
                              Vec4 color, float scale) {
    batch.set_texture(font_desc);

    float cursor_x = position.x;
    float cursor_y = position.y;

    for (char c : text) {
        if (c == '\n') {
            cursor_x = position.x;
            cursor_y += line_height_ * scale;
            continue;
        }

        if (c < 32 || c >= 128) continue;
        const auto& g = glyphs_[(int)c];
        if (g.width == 0) {
            cursor_x += g.advance * scale;
            continue;
        }

        Vec2 pos = {cursor_x + g.x_offset * scale,
                    cursor_y + (ascent_ + g.y_offset) * scale};
        Vec2 size = {g.width * scale, g.height * scale};
        Vec2 uv_min = {g.uv_x0, g.uv_y0};
        Vec2 uv_max = {g.uv_x1, g.uv_y1};

        batch.draw_quad(pos, size, uv_min, uv_max, color);
        cursor_x += g.advance * scale;
    }
}

Vec2 TextRenderer::measure_text(const std::string& text, float scale) const {
    float max_w = 0.0f;
    float cursor_x = 0.0f;
    int lines = 1;

    for (char c : text) {
        if (c == '\n') {
            max_w = std::max(max_w, cursor_x);
            cursor_x = 0.0f;
            lines++;
            continue;
        }
        if (c < 32 || c >= 128) continue;
        cursor_x += glyphs_[(int)c].advance * scale;
    }
    max_w = std::max(max_w, cursor_x);
    return {max_w, lines * line_height_ * scale};
}

void TextRenderer::draw_text_wrapped(SpriteBatch& batch, VkDescriptorSet font_desc,
                                      const std::string& text, Vec2 position,
                                      float max_width, Vec4 color, float scale) {
    batch.set_texture(font_desc);

    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    size_t line_start = 0;
    size_t last_space = std::string::npos;

    for (size_t i = 0; i <= text.size(); i++) {
        bool end_of_text = (i == text.size());
        char c = end_of_text ? '\0' : text[i];

        if (c == ' ') last_space = i;

        if (c == '\n' || end_of_text) {
            // Draw line from line_start to i
            float lx = position.x;
            for (size_t j = line_start; j < i; j++) {
                char ch = text[j];
                if (ch < 32 || ch >= 128) continue;
                const auto& g = glyphs_[(int)ch];
                if (g.width > 0) {
                    Vec2 pos = {lx + g.x_offset * scale,
                                position.y + cursor_y + (ascent_ + g.y_offset) * scale};
                    Vec2 sz = {g.width * scale, g.height * scale};
                    batch.draw_quad(pos, sz, {g.uv_x0, g.uv_y0}, {g.uv_x1, g.uv_y1}, color);
                }
                lx += g.advance * scale;
            }
            cursor_y += line_height_ * scale;
            cursor_x = 0.0f;
            line_start = i + 1;
            last_space = std::string::npos;
            continue;
        }

        float char_advance = glyphs_[(int)c].advance * scale;
        if (cursor_x + char_advance > max_width && cursor_x > 0) {
            // Word wrap
            size_t wrap_at = (last_space != std::string::npos && last_space > line_start)
                             ? last_space : i;

            float lx = position.x;
            for (size_t j = line_start; j < wrap_at; j++) {
                char ch = text[j];
                if (ch < 32 || ch >= 128) continue;
                const auto& g = glyphs_[(int)ch];
                if (g.width > 0) {
                    Vec2 pos = {lx + g.x_offset * scale,
                                position.y + cursor_y + (ascent_ + g.y_offset) * scale};
                    Vec2 sz = {g.width * scale, g.height * scale};
                    batch.draw_quad(pos, sz, {g.uv_x0, g.uv_y0}, {g.uv_x1, g.uv_y1}, color);
                }
                lx += g.advance * scale;
            }

            cursor_y += line_height_ * scale;
            cursor_x = 0.0f;
            line_start = (wrap_at == last_space) ? wrap_at + 1 : wrap_at;
            last_space = std::string::npos;

            // Recalculate cursor_x for chars already past line_start
            for (size_t j = line_start; j <= i; j++) {
                if (j < text.size()) {
                    cursor_x += glyphs_[(int)text[j]].advance * scale;
                }
            }
        } else {
            cursor_x += char_advance;
        }
    }
}

int TextRenderer::chars_that_fit(const std::string& text, float max_width, float scale) const {
    float w = 0.0f;
    for (int i = 0; i < (int)text.size(); i++) {
        char c = text[i];
        if (c < 32 || c >= 128) continue;
        w += glyphs_[(int)c].advance * scale;
        if (w > max_width) return i;
    }
    return (int)text.size();
}

} // namespace eb
