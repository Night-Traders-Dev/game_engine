#include "editor/tile_editor.h"
#include "game/game.h"
#ifndef EB_ANDROID
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#endif
#include "engine/graphics/texture_atlas.h"
#include "engine/graphics/text_renderer.h"
#include "engine/graphics/renderer.h"
#include "engine/platform/input.h"
#include "engine/core/debug_log.h"
#include "engine/resource/file_io.h"
#include "engine/scripting/script_engine.h"
#include "game/overworld/camera.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>

namespace eb {

void TileEditor::render_imgui_debug_console(GameState& game) {
#ifndef EB_ANDROID
    if (show_debug_console_) {
    ImGui::SetNextWindowPos(ImVec2(8, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(940, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug Console##editor", &show_debug_console_)) {
        auto& dlog = eb::DebugLog::instance();

        // Filter buttons
        ImGui::Checkbox("Debug", &dlog.show_debug); ImGui::SameLine();
        ImGui::Checkbox("Info", &dlog.show_info); ImGui::SameLine();
        ImGui::Checkbox("Warn", &dlog.show_warning); ImGui::SameLine();
        ImGui::Checkbox("Error", &dlog.show_error); ImGui::SameLine();
        ImGui::Checkbox("Script", &dlog.show_script); ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) dlog.clear();
        ImGui::SameLine();
        ImGui::Text("(%d entries)", (int)dlog.entries().size());

        ImGui::Separator();

        // Scrollable log
        if (ImGui::BeginChild("LogScroll", ImVec2(0, -30), true)) {
            for (auto& entry : dlog.entries()) {
                bool show = false;
                ImVec4 color;
                const char* prefix;
                switch (entry.level) {
                    case eb::LogLevel::Debug:   show = dlog.show_debug;   color = ImVec4(0.5f,0.5f,0.5f,1); prefix = "[DBG]"; break;
                    case eb::LogLevel::Info:    show = dlog.show_info;    color = ImVec4(0.7f,0.9f,0.7f,1); prefix = "[INF]"; break;
                    case eb::LogLevel::Warning: show = dlog.show_warning; color = ImVec4(1,0.9f,0.3f,1);    prefix = "[WRN]"; break;
                    case eb::LogLevel::Error:   show = dlog.show_error;   color = ImVec4(1,0.3f,0.3f,1);    prefix = "[ERR]"; break;
                    case eb::LogLevel::Script:  show = dlog.show_script;  color = ImVec4(0.4f,0.8f,1,1);    prefix = "[SCR]"; break;
                }
                if (show) {
                    ImGui::TextColored(color, "%s %.1fs %s", prefix, entry.timestamp, entry.message.c_str());
                }
            }
            // Auto-scroll to bottom
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // Command input
        static char cmd_buf[256] = "";
        if (ImGui::InputText("##cmd", cmd_buf, sizeof(cmd_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (game.script_engine && cmd_buf[0] != '\0') {
                DLOG_SCRIPT("> %s", cmd_buf);
                game.script_engine->execute(cmd_buf);
                cmd_buf[0] = '\0';
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Enter SageLang expression");
    }
    ImGui::End();
    } // show_debug_console_
#endif
}

} // namespace eb
