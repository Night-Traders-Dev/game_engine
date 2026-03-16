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

void TileEditor::render_imgui_script_ide(GameState& game) {
#ifndef EB_ANDROID
    if (show_script_ide_) {
    ImGui::SetNextWindowPos(ImVec2(250, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 550), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Script IDE##editor", &show_script_ide_, ImGuiWindowFlags_MenuBar)) {
        static int selected_script = -1;
        static char script_buffer[16384] = "";
        static bool script_dirty = false;
        static std::string current_file;
        static char new_script_name[128] = "new_script.sage";
        static bool map_script_selected = false;

        // Decay asset highlight timer
        if (highlight_timer_ > 0) highlight_timer_ -= 0.016f;
        if (highlight_timer_ <= 0) highlighted_asset_.clear();

        auto* se = game.script_engine;

        // ── Helper lambdas for menu actions ──
        auto do_save = [&]() {
            if (current_file.empty()) return;
            if (map_script_selected) {
                // Save map script — write buffer directly to file
                std::ofstream f(map_script_path_);
                if (f.is_open()) { f << script_buffer; f.close(); script_dirty = false;
                    // Re-parse body from the saved content
                    load_map_script(map_script_path_);
                    set_status("Map script saved: " + map_script_path_);
                }
                return;
            }
            if (selected_script < 0) return;
            std::ofstream f(current_file);
            if (f.is_open()) { f << script_buffer; f.close(); script_dirty = false; set_status("Saved: " + current_file); }
        };
        auto do_save_reload = [&]() {
            do_save();
            if (se) { se->reload_all(); set_status("Hot reloaded all scripts"); }
        };
        auto do_reload_all = [&]() {
            if (se) { se->reload_all(); set_status("Hot reloaded all scripts"); }
        };
        auto do_open_script = [&](int i) {
            if (!se) return;
            auto& files = se->loaded_files();
            if (i < 0 || i >= (int)files.size()) return;
            selected_script = i;
            current_file = files[i];
            auto data = eb::FileIO::read_file(current_file);
            if (!data.empty()) {
                int len = std::min((int)data.size(), (int)sizeof(script_buffer) - 1);
                std::memcpy(script_buffer, data.data(), len);
                script_buffer[len] = '\0';
                script_dirty = false;
            }
        };

        // ── Menu Bar ──
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Script...", "")) ImGui::OpenPopup("NewScriptPopup##menu");
                ImGui::Separator();
                if (se && ImGui::BeginMenu("Open Script")) {
                    auto& files = se->loaded_files();
                    for (int i = 0; i < (int)files.size(); i++) {
                        std::string label = files[i];
                        auto slash = label.rfind('/');
                        if (slash != std::string::npos) label = label.substr(slash + 1);
                        if (ImGui::MenuItem(label.c_str(), "", selected_script == i)) do_open_script(i);
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                bool can_save = (selected_script >= 0 || map_script_selected);
                if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save)) do_save();
                if (ImGui::MenuItem("Save & Reload", "", false, can_save)) do_save_reload();
                if (ImGui::MenuItem("Reload All Scripts", "")) do_reload_all();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("SageLang API Manual", "")) show_api_manual_ = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (se) {
            auto& files = se->loaded_files();

            // File list panel
            if (ImGui::BeginChild("ScriptList", ImVec2(180, 0), true)) {
                // Map Script (own section at top)
                if (!map_script_path_.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.4f, 1));
                    ImGui::Text("MAP SCRIPT");
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    std::string map_label = map_script_path_;
                    auto mslash = map_label.rfind('/');
                    if (mslash != std::string::npos) map_label = map_label.substr(mslash + 1);
                    if (map_label.empty()) map_label = "default.sage";
                    bool msel = map_script_selected;
                    // Unique ID to avoid conflict with same filename in scripts list
                    std::string map_selectable_id = map_label + "##mapscript";
                    if (ImGui::Selectable(map_selectable_id.c_str(), msel)) {
                        map_script_selected = true;
                        selected_script = -1;
                        // Load actual file content (not reconstructed)
                        auto data = eb::FileIO::read_file(map_script_path_);
                        if (!data.empty()) {
                            int len = std::min((int)data.size(), (int)sizeof(script_buffer) - 1);
                            std::memcpy(script_buffer, data.data(), len);
                            script_buffer[len] = '\0';
                        } else {
                            std::string fallback = "# Map script\n\nproc map_init():\n    log(\"Map loaded\")\n";
                            std::memcpy(script_buffer, fallback.data(), fallback.size());
                            script_buffer[fallback.size()] = '\0';
                        }
                        current_file = map_script_path_;
                        script_dirty = false;
                    }
                    ImGui::Spacing();
                }

                // Regular scripts (skip the map script file to avoid duplicate)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 1.0f, 1));
                ImGui::Text("SCRIPTS");
                ImGui::PopStyleColor();
                ImGui::Separator();
                for (int i = 0; i < (int)files.size(); i++) {
                    // Skip if this file is the map script (match by suffix)
                    if (!map_script_path_.empty()) {
                        // Check if file path ends with the map script path or vice versa
                        std::string fp = files[i];
                        std::string mp = map_script_path_;
                        // Normalize: strip leading ./
                        if (fp.size() > 2 && fp[0] == '.' && fp[1] == '/') fp = fp.substr(2);
                        if (mp.size() > 2 && mp[0] == '.' && mp[1] == '/') mp = mp.substr(2);
                        if (fp == mp) continue;
                        // Also check if one ends with the other
                        if (fp.size() >= mp.size() && fp.substr(fp.size()-mp.size()) == mp) continue;
                        if (mp.size() >= fp.size() && mp.substr(mp.size()-fp.size()) == fp) continue;
                    }

                    std::string label = files[i];
                    auto slash = label.rfind('/');
                    if (slash != std::string::npos) label = label.substr(slash + 1);
                    // Unique ID with index to prevent ImGui conflicts
                    char selid[256];
                    std::snprintf(selid, sizeof(selid), "%s##script_%d", label.c_str(), i);
                    bool sel = (selected_script == i && !map_script_selected);
                    if (ImGui::Selectable(selid, sel)) {
                        map_script_selected = false;
                        do_open_script(i);
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // ── Editor pane with syntax highlighting ──
            if (ImGui::BeginChild("ScriptEdit", ImVec2(0, 0))) {
                if (selected_script >= 0 || map_script_selected) {
                    static bool edit_mode = false;
                    ImGui::Text("%s%s", current_file.c_str(), script_dirty ? " *" : "");
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
                    if (ImGui::SmallButton(edit_mode ? "View" : "Edit")) edit_mode = !edit_mode;
                    ImGui::Separator();

                    // Syntax color helpers
                    auto is_keyword = [](const std::string& w) -> bool {
                        static const char* kws[] = {"proc","let","if","else","elif","while","for","return","in","and","or","not","break","continue",nullptr};
                        for (auto** k = kws; *k; k++) if (w == *k) return true; return false;
                    };
                    auto is_bool_nil = [](const std::string& w) -> bool { return w=="true"||w=="false"||w=="nil"; };
                    auto is_builtin = [](const std::string& w) -> bool {
                        static const char* fns[] = {"say","log","debug","info","warn","error","print","assert_true","add_item","remove_item","has_item","item_count","add_shop_item","open_shop","set_gold","get_gold","set_skill","get_skill","get_skill_bonus","set_flag","get_flag","random","clamp","str",nullptr};
                        for (auto** f = fns; *f; f++) if (w == *f) return true; return false;
                    };

                    ImVec4 c_keyword = {0.78f, 0.47f, 1.0f, 1.0f};
                    ImVec4 c_comment = {0.39f, 0.55f, 0.39f, 1.0f};
                    ImVec4 c_string  = {0.90f, 0.71f, 0.31f, 1.0f};
                    ImVec4 c_number  = {0.47f, 0.78f, 1.0f, 1.0f};
                    ImVec4 c_builtin = {0.39f, 0.78f, 0.63f, 1.0f};
                    ImVec4 c_bool    = {1.0f, 0.55f, 0.39f, 1.0f};
                    ImVec4 c_plain   = {0.78f, 0.78f, 0.73f, 1.0f};

                    if (edit_mode) {
                        // Plain text editor (no highlighting, but fully editable)
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.73f, 1.0f));
                        if (ImGui::InputTextMultiline("##code", script_buffer, sizeof(script_buffer),
                                avail, ImGuiInputTextFlags_AllowTabInput)) {
                            script_dirty = true;
                        }
                        ImGui::PopStyleColor(2);
                    } else {
                        // Syntax-highlighted read-only view in a scrollable child
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
                        if (ImGui::BeginChild("##codeview", ImVec2(0, 0), false,
                                ImGuiWindowFlags_HorizontalScrollbar)) {
                            // Render each line with colored spans
                            const char* p = script_buffer;
                            int line_num = 0;
                            while (*p) {
                                const char* ls = p;
                                while (*p && *p != '\n') p++;
                                int ll = (int)(p - ls);
                                if (*p == '\n') p++;

                                // Line number
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
                                ImGui::Text("%3d ", line_num + 1);
                                ImGui::PopStyleColor();
                                ImGui::SameLine(0, 0);

                                // Tokenize and render colored spans inline
                                int col = 0;
                                while (col < ll) {
                                    char ch = ls[col];

                                    // Comment
                                    if (ch == '#') {
                                        ImGui::PushStyleColor(ImGuiCol_Text, c_comment);
                                        ImGui::TextUnformatted(ls + col, ls + ll);
                                        ImGui::PopStyleColor();
                                        col = ll;
                                        break;
                                    }

                                    // String
                                    if (ch == '"') {
                                        int start = col; col++;
                                        while (col < ll && ls[col] != '"') col++;
                                        if (col < ll) col++;
                                        ImGui::PushStyleColor(ImGuiCol_Text, c_string);
                                        ImGui::TextUnformatted(ls + start, ls + col);
                                        ImGui::PopStyleColor();

                                        // Asset click detection
                                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && col - start > 2) {
                                            std::string asset(ls + start + 1, col - start - 2);
                                            if (!asset.empty()) {
                                                highlighted_asset_ = asset;
                                                highlight_timer_ = 5.0f;
                                                set_status("Highlighting: " + asset);
                                            }
                                        }
                                        ImGui::SameLine(0, 0);
                                        continue;
                                    }

                                    // Number
                                    if ((ch >= '0' && ch <= '9') ||
                                        (ch == '-' && col + 1 < ll && ls[col+1] >= '0' && ls[col+1] <= '9' &&
                                         (col == 0 || ls[col-1] == ' ' || ls[col-1] == '(' || ls[col-1] == ','))) {
                                        int start = col; if (ch == '-') col++;
                                        while (col < ll && ((ls[col] >= '0' && ls[col] <= '9') || ls[col] == '.')) col++;
                                        ImGui::PushStyleColor(ImGuiCol_Text, c_number);
                                        ImGui::TextUnformatted(ls + start, ls + col);
                                        ImGui::PopStyleColor();
                                        ImGui::SameLine(0, 0);
                                        continue;
                                    }

                                    // Identifier / keyword
                                    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
                                        int start = col;
                                        while (col < ll && ((ls[col] >= 'a' && ls[col] <= 'z') ||
                                               (ls[col] >= 'A' && ls[col] <= 'Z') ||
                                               (ls[col] >= '0' && ls[col] <= '9') || ls[col] == '_')) col++;
                                        std::string w(ls + start, col - start);
                                        ImVec4 tc = c_plain;
                                        if (is_keyword(w)) tc = c_keyword;
                                        else if (is_bool_nil(w)) tc = c_bool;
                                        else if (is_builtin(w)) tc = c_builtin;
                                        ImGui::PushStyleColor(ImGuiCol_Text, tc);
                                        ImGui::TextUnformatted(ls + start, ls + col);
                                        ImGui::PopStyleColor();
                                        ImGui::SameLine(0, 0);
                                        continue;
                                    }

                                    // Punctuation / whitespace — render character by character in runs
                                    {
                                        int start = col;
                                        while (col < ll && !((ls[col] >= 'a' && ls[col] <= 'z') ||
                                               (ls[col] >= 'A' && ls[col] <= 'Z') || ls[col] == '_' ||
                                               ls[col] == '#' || ls[col] == '"' ||
                                               (ls[col] >= '0' && ls[col] <= '9'))) col++;
                                        if (col > start) {
                                            ImGui::PushStyleColor(ImGuiCol_Text, c_plain);
                                            ImGui::TextUnformatted(ls + start, ls + col);
                                            ImGui::PopStyleColor();
                                            ImGui::SameLine(0, 0);
                                        }
                                    }
                                }

                                // End of line — need a newline. Use dummy text if line was empty.
                                if (ll == 0) ImGui::TextUnformatted("");
                                else ImGui::NewLine();

                                line_num++;
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                } else {
                    ImGui::TextDisabled("Select a script file or use File > New/Open");
                }

                // ── Error/Warning panel at bottom of editor ──
                auto& log_entries = eb::DebugLog::instance().entries();
                int error_count = 0;
                for (int ei = (int)log_entries.size() - 1; ei >= 0 && error_count < 5; ei--) {
                    if (log_entries[ei].level == eb::LogLevel::Error ||
                        log_entries[ei].level == eb::LogLevel::Warning)
                        error_count++;
                }
                if (error_count > 0) {
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.08f, 0.08f, 0.9f));
                    float err_h = std::min(80.0f, error_count * 18.0f + 4.0f);
                    if (ImGui::BeginChild("##errors", ImVec2(0, err_h), false)) {
                        int shown = 0;
                        for (int ei = (int)log_entries.size() - 1; ei >= 0 && shown < 5; ei--) {
                            auto& entry = log_entries[ei];
                            if (entry.level == eb::LogLevel::Error) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1));
                                ImGui::TextWrapped("[ERR] %s", entry.message.c_str());
                                ImGui::PopStyleColor();
                                shown++;
                            } else if (entry.level == eb::LogLevel::Warning) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1));
                                ImGui::TextWrapped("[WRN] %s", entry.message.c_str());
                                ImGui::PopStyleColor();
                                shown++;
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            // ── New Script popup (shared by menu and popup) ──
            if (ImGui::BeginPopup("NewScriptPopup##menu")) {
                ImGui::Text("Create New Script");
                ImGui::Separator();
                ImGui::InputText("Filename", new_script_name, sizeof(new_script_name));
                if (ImGui::Button("Create", ImVec2(120, 0))) {
                    std::string path = "assets/scripts/" + std::string(new_script_name);
                    std::ofstream f(path);
                    if (f.is_open()) {
                        std::string base = new_script_name;
                        auto dot = base.rfind('.'); if (dot != std::string::npos) base = base.substr(0, dot);
                        f << "# " << base << "\n\nproc " << base << "_init():\n    log(\"" << base << " loaded\")\n";
                        f.close();
                        se->load_file(path);
                        auto& uf = se->loaded_files();
                        selected_script = (int)uf.size()-1; current_file = path;
                        auto data = eb::FileIO::read_file(current_file);
                        if (!data.empty()) { int len=std::min((int)data.size(),(int)sizeof(script_buffer)-1); std::memcpy(script_buffer,data.data(),len); script_buffer[len]='\0'; script_dirty=false; }
                        set_status("Created: " + path);
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        } else {
            ImGui::TextDisabled("No script engine available");
        }
    }
    ImGui::End();
    } // show_script_ide_
#endif
}

} // namespace eb
