#include "game/dialogue/dialogue_box.h"
#include "engine/graphics/sprite_batch.h"
#include "engine/graphics/text_renderer.h"

#include <algorithm>
#include <cmath>

namespace eb {

void DialogueBox::set_portrait(const std::string& speaker, VkDescriptorSet portrait_desc) {
    portraits_[speaker] = portrait_desc;
}

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

    if (!line_complete_) {
        char_timer_ += dt;
        int new_chars = (int)(char_timer_ * CHARS_PER_SEC);
        if (new_chars > visible_chars_) visible_chars_ = new_chars;
        const auto& t = lines_[current_line_].text;
        if (visible_chars_ >= (int)t.size()) {
            line_complete_ = true;
            visible_chars_ = (int)t.size();
        }
    }

    if (confirm_pressed) {
        if (!line_complete_) {
            visible_chars_ = (int)lines_[current_line_].text.size();
            line_complete_ = true;
        } else {
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

    float box_x = BOX_MARGIN;
    float box_y = screen_height - BOX_HEIGHT - BOX_MARGIN;
    float box_w = screen_width - BOX_MARGIN * 2.0f;

    // Draw Dialog.png background (or fallback solid color)
    if (bg_desc_ != VK_NULL_HANDLE) {
        batch.set_texture(bg_desc_);
        batch.draw_quad({box_x, box_y}, {box_w, BOX_HEIGHT},
                        {0.0f, 0.0f}, {1.0f, 1.0f},
                        {1.0f, 1.0f, 1.0f, 0.95f});
    } else {
        batch.set_texture(white_desc);
        batch.draw_quad({box_x, box_y}, {box_w, BOX_HEIGHT},
                        {0.0f, 0.0f}, {1.0f, 1.0f},
                        {0.05f, 0.05f, 0.15f, 0.9f});
        float border = 2.0f;
        batch.draw_quad({box_x, box_y}, {box_w, border}, {0,0},{1,1}, {0.6f,0.6f,0.8f,1});
        batch.draw_quad({box_x, box_y+BOX_HEIGHT-border}, {box_w, border}, {0,0},{1,1}, {0.6f,0.6f,0.8f,1});
        batch.draw_quad({box_x, box_y}, {border, BOX_HEIGHT}, {0,0},{1,1}, {0.6f,0.6f,0.8f,1});
        batch.draw_quad({box_x+box_w-border, box_y}, {border, BOX_HEIGHT}, {0,0},{1,1}, {0.6f,0.6f,0.8f,1});
    }

    // Dialog.png layout (measured from 1536x1024 source):
    //   Chalkboard text area: ~(55,250) to (900,770)  = u 0.036-0.586, v 0.244-0.752
    //   Portrait frame inner: ~(1016,319) to (1351,679) = u 0.661-0.880, v 0.312-0.663

    // Text area in screen coords
    float text_x = box_x + box_w * 0.045f;
    float text_y = box_y + BOX_HEIGHT * 0.28f;
    float text_area_w = box_w * 0.53f;

    // Portrait area in screen coords (matching the frame in Dialog.png)
    float port_x = box_x + box_w * 0.661f;
    float port_y = box_y + BOX_HEIGHT * 0.312f;
    float port_w = box_w * 0.218f;
    float port_h = BOX_HEIGHT * 0.351f;

    // Find current speaker's portrait
    std::string current_speaker;
    if (!is_choice_ && current_line_ < (int)lines_.size()) {
        current_speaker = lines_[current_line_].speaker;
    }

    bool has_portrait = false;
    if (!current_speaker.empty()) {
        auto it = portraits_.find(current_speaker);
        if (it != portraits_.end()) {
            has_portrait = true;
            batch.set_texture(it->second);
            batch.draw_quad({port_x, port_y}, {port_w, port_h},
                            {0.0f, 0.0f}, {1.0f, 1.0f});
        }
    }

    // If no portrait, use more width for text
    if (!has_portrait) {
        text_area_w = box_w * 0.85f;
    }

    if (is_choice_) {
        text.draw_text(batch, font_desc, choice_prompt_,
                       {text_x, text_y}, {1.0f, 1.0f, 1.0f, 1.0f}, TEXT_SCALE);
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
        const auto& line = lines_[current_line_];

        // Speaker name
        if (!line.speaker.empty()) {
            text.draw_text(batch, font_desc, line.speaker,
                           {text_x, text_y}, {0.4f, 0.8f, 1.0f, 1.0f}, TEXT_SCALE);
            text_y += text.line_height() * TEXT_SCALE + 8.0f;
        }

        // Dialogue text with typewriter
        std::string visible = line.text.substr(0, visible_chars_);
        text.draw_text_wrapped(batch, font_desc, visible,
                               {text_x, text_y}, text_area_w,
                               {1.0f, 1.0f, 1.0f, 1.0f}, TEXT_SCALE);

        // Blinking advance indicator
        if (line_complete_) {
            float ind_x = text_x + text_area_w - 12.0f;
            float ind_y = box_y + BOX_HEIGHT * 0.75f;
            batch.set_texture(white_desc);
            float blink = std::fmod(char_timer_ + visible_chars_ * 0.03f, 0.8f) < 0.5f ? 1.0f : 0.0f;
            batch.draw_quad({ind_x, ind_y}, {8.0f, 8.0f},
                            {0.0f, 0.0f}, {1.0f, 1.0f},
                            {1.0f, 1.0f, 1.0f, blink});
        }
    }
}

} // namespace eb
