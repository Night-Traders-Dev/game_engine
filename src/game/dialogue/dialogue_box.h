#pragma once

#include "engine/core/types.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>

namespace eb {

class SpriteBatch;
class TextRenderer;

struct DialogueLine {
    std::string speaker;
    std::string text;
};

struct DialogueChoice {
    std::string text;
    int result; // Returned when selected
};

class DialogueBox {
public:
    DialogueBox() = default;

    // Start a dialogue sequence
    void start(const std::vector<DialogueLine>& lines);

    // Start a choice menu
    void start_choice(const std::string& prompt, const std::vector<DialogueChoice>& choices);

    // Update typewriter effect and handle input
    // Returns: -1 if still active, 0+ if dialogue ended (choice result for choices, 0 for normal)
    int update(float dt, bool confirm_pressed, bool up_pressed, bool down_pressed);

    // Render the dialogue box
    void render(SpriteBatch& batch, TextRenderer& text,
                VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                float screen_width, float screen_height);

    bool is_active() const { return active_; }

private:
    bool active_ = false;
    bool is_choice_ = false;

    // Dialogue state
    std::vector<DialogueLine> lines_;
    int current_line_ = 0;

    // Typewriter
    float char_timer_ = 0.0f;
    int visible_chars_ = 0;
    bool line_complete_ = false;
    static constexpr float CHARS_PER_SEC = 35.0f;

    // Choice state
    std::string choice_prompt_;
    std::vector<DialogueChoice> choices_;
    int selected_choice_ = 0;

    // Visual
    static constexpr float BOX_MARGIN = 16.0f;
    static constexpr float BOX_HEIGHT = 120.0f;
    static constexpr float BOX_PADDING = 12.0f;
    static constexpr float TEXT_SCALE = 1.0f;
};

} // namespace eb
