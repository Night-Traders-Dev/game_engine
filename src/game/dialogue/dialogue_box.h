#pragma once

#include "engine/core/types.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace eb {

class SpriteBatch;
class TextRenderer;

struct DialogueLine {
    std::string speaker;
    std::string text;
};

struct DialogueChoice {
    std::string text;
    int result;
};

class DialogueBox {
public:
    DialogueBox() = default;

    // Set the dialog background texture (Dialog.png)
    void set_background(VkDescriptorSet bg_desc) { bg_desc_ = bg_desc; }

    // Register a portrait for a speaker name
    void set_portrait(const std::string& speaker, VkDescriptorSet portrait_desc);

    void start(const std::vector<DialogueLine>& lines);
    void queue_line(const DialogueLine& line);
    void start_choice(const std::string& prompt, const std::vector<DialogueChoice>& choices);

    // Returns: -1 if active, 0+ if ended
    int update(float dt, bool confirm_pressed, bool up_pressed, bool down_pressed);

    void render(SpriteBatch& batch, TextRenderer& text,
                VkDescriptorSet font_desc, VkDescriptorSet white_desc,
                float screen_width, float screen_height);

    bool is_active() const { return active_; }

private:
    bool active_ = false;
    bool is_choice_ = false;

    std::vector<DialogueLine> lines_;
    int current_line_ = 0;

    float char_timer_ = 0.0f;
    int visible_chars_ = 0;
    bool line_complete_ = false;
    static constexpr float CHARS_PER_SEC = 35.0f;

    std::string choice_prompt_;
    std::vector<DialogueChoice> choices_;
    int selected_choice_ = 0;

    static constexpr float BOX_MARGIN = 12.0f;
    static constexpr float BOX_HEIGHT_FRAC = 0.28f; // Fraction of screen height
    static constexpr float BOX_PADDING = 16.0f;
    static constexpr float TEXT_SCALE = 1.0f;

    // Textures
    VkDescriptorSet bg_desc_ = VK_NULL_HANDLE;
    std::unordered_map<std::string, VkDescriptorSet> portraits_;
};

} // namespace eb
