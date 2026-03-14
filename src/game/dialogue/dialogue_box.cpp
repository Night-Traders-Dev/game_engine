#include "game/dialogue/dialogue_box.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/text_renderer.h"

#include <algorithm>

namespace eb {

void DialogueBox::start(const std::vector<DialogueLine>& lines) {
    lines_ = lines;
    current_line_ = 0;
    visible_chars_ = 0;
    char_timer_ = 0.0f;
    line_complete_ = false;
    is_choice_ = false;
    active_ = true;
}

void DialogueBox::start_choice(const std::string& prompt,
                                const std::vector<DialogueChoice>& choices) {
    choice_prompt_ = prompt;
    choices_ = choices;
    selected_choice_ = 0;
    is_choice_ = true;
    line_complete_ = true;
    active_ = true;
}

int DialogueBox::update(float dt, bool confirm_pressed, bool up_pressed, bool down_pressed) {
    if (!active_) return -1;

    if (is_choice_) {
        if (up_pressed && selected_choice_ > 0) selected_choice_--;
        if (down_pressed && selected_choice_ < (int)choices_.size() - 1) selected_choice_++;
        if (confirm_pressed) {
            int result = choices_[selected_choice_].result;
            active_ = false;
            return result;
        }
        return -1;
    }

    // Typewriter effect
    if (!line_complete_) {
        char_timer_ += dt;
        int new_chars = (int)(char_timer_ * CHARS_PER_SEC);
        if (new_chars > visible_chars_) {
            visible_chars_ = new_chars;
        }
        const auto& text = lines_[current_line_].text;
        if (visible_chars_ >= (int)text.size()) {
            line_complete_ = true;
            visible_chars_ = (int)text.size();
        }
    }

    if (confirm_pressed) {
        if (!line_complete_) {
            // Skip typewriter — show full line
            visible_chars_ = (int)lines_[current_line_].text.size();
            line_complete_ = true;
        } else {
            // Advance to next line
            current_line_++;
            if (current_line_ >= (int)lines_.size()) {
                active_ = false;
                return 0;
            }
            visible_chars_ = 0;
            char_timer_ = 0.0f;
            line_complete_ = false;
        }
    }

    return -1;
}

void DialogueBox::render(SpriteBatch& batch, TextRenderer& text,
                          VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                          float screen_width, float screen_height) {
    if (!active_) return;

    // Box dimensions
    float box_x = BOX_MARGIN;
    float box_y = screen_height - BOX_HEIGHT - BOX_MARGIN;
    float box_w = screen_width - BOX_MARGIN * 2.0f;

    // Draw box background (dark semi-transparent)
    batch.set_texture(white_desc);
    batch.draw_quad({box_x, box_y}, {box_w, BOX_HEIGHT},
                    {0.0f, 0.0f}, {1.0f, 1.0f},
                    {0.05f, 0.05f, 0.15f, 0.9f});

    // Draw box border
    float border = 2.0f;
    batch.draw_quad({box_x, box_y}, {box_w, border}, {0.0f, 0.0f}, {1.0f, 1.0f},
                    {0.6f, 0.6f, 0.8f, 1.0f});
    batch.draw_quad({box_x, box_y + BOX_HEIGHT - border}, {box_w, border}, {0.0f, 0.0f}, {1.0f, 1.0f},
                    {0.6f, 0.6f, 0.8f, 1.0f});
    batch.draw_quad({box_x, box_y}, {border, BOX_HEIGHT}, {0.0f, 0.0f}, {1.0f, 1.0f},
                    {0.6f, 0.6f, 0.8f, 1.0f});
    batch.draw_quad({box_x + box_w - border, box_y}, {border, BOX_HEIGHT}, {0.0f, 0.0f}, {1.0f, 1.0f},
                    {0.6f, 0.6f, 0.8f, 1.0f});

    float text_x = box_x + BOX_PADDING;
    float text_y = box_y + BOX_PADDING;
    float text_area_w = box_w - BOX_PADDING * 2.0f;

    if (is_choice_) {
        // Draw prompt
        text.draw_text(batch, font_desc, choice_prompt_,
                       {text_x, text_y}, {1.0f, 1.0f, 1.0f, 1.0f}, TEXT_SCALE);

        // Draw choices
        float choice_y = text_y + text.line_height() * TEXT_SCALE + 4.0f;
        for (int i = 0; i < (int)choices_.size(); i++) {
            Vec4 color = (i == selected_choice_)
                ? Vec4{1.0f, 1.0f, 0.3f, 1.0f}
                : Vec4{0.8f, 0.8f, 0.8f, 1.0f};
            std::string prefix = (i == selected_choice_) ? "> " : "  ";
            text.draw_text(batch, font_desc, prefix + choices_[i].text,
                           {text_x, choice_y}, color, TEXT_SCALE);
            choice_y += text.line_height() * TEXT_SCALE;
        }
    } else {
        // Speaker name
        const auto& line = lines_[current_line_];
        if (!line.speaker.empty()) {
            text.draw_text(batch, font_desc, line.speaker,
                           {text_x, text_y}, {0.4f, 0.8f, 1.0f, 1.0f}, TEXT_SCALE);
            text_y += text.line_height() * TEXT_SCALE + 10.0f;
        }

        // Dialogue text with typewriter
        std::string visible = line.text.substr(0, visible_chars_);
        text.draw_text_wrapped(batch, font_desc, visible,
                               {text_x, text_y}, text_area_w,
                               {1.0f, 1.0f, 1.0f, 1.0f}, TEXT_SCALE);

        // Blinking advance indicator
        if (line_complete_) {
            float indicator_x = box_x + box_w - BOX_PADDING - 12.0f;
            float indicator_y = box_y + BOX_HEIGHT - BOX_PADDING - 12.0f;
            // Simple triangle indicator (drawn as small quad)
            batch.set_texture(white_desc);
            float blink = std::fmod(char_timer_ + visible_chars_ * 0.03f, 0.8f) < 0.5f ? 1.0f : 0.0f;
            batch.draw_quad({indicator_x, indicator_y}, {8.0f, 8.0f},
                            {0.0f, 0.0f}, {1.0f, 1.0f},
                            {1.0f, 1.0f, 1.0f, blink});
        }
    }
}

} // namespace eb
