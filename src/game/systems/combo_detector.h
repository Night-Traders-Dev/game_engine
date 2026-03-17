#pragma once

#include <string>
#include <vector>

namespace eb {

// ─── Combo Detection ───

struct ComboStep {
    int action;       // InputAction enum cast to int
    float max_delay;  // max seconds between this step and the next
};

struct Combo {
    std::string name;
    std::vector<ComboStep> sequence;
    std::string on_complete;
};

class ComboDetector {
    std::vector<Combo> combos;

    struct InputRecord {
        int action;
        float time;
    };

    std::vector<InputRecord> history;
    int max_history = 32;

public:
    void add_combo(const std::string& name,
                   const std::vector<ComboStep>& steps,
                   const std::string& callback) {
        combos.push_back({name, steps, callback});
    }

    void record_input(int action, float game_time) {
        history.push_back({action, game_time});
        if (static_cast<int>(history.size()) > max_history) {
            history.erase(history.begin());
        }
    }

    // Check all combos against input history, return callbacks for completed combos
    std::vector<std::string> check(float game_time) {
        std::vector<std::string> completed;

        for (const auto& combo : combos) {
            if (combo.sequence.empty()) continue;

            int seq_len = static_cast<int>(combo.sequence.size());
            int hist_len = static_cast<int>(history.size());
            if (hist_len < seq_len) continue;

            // Slide window: try matching ending at each position in history
            for (int end = hist_len - 1; end >= seq_len - 1; end--) {
                bool matched = true;
                int h = end;

                // Match sequence in reverse from the last step
                for (int s = seq_len - 1; s >= 0; s--) {
                    if (h < 0) { matched = false; break; }
                    if (history[h].action != combo.sequence[s].action) {
                        matched = false;
                        break;
                    }

                    // Check timing constraint between consecutive steps
                    if (s < seq_len - 1) {
                        float delta = history[h + 1].time - history[h].time;
                        if (delta > combo.sequence[s].max_delay || delta < 0) {
                            matched = false;
                            break;
                        }
                    }

                    // Ensure the last input is recent enough
                    if (s == seq_len - 1) {
                        float age = game_time - history[h].time;
                        if (age > combo.sequence[s].max_delay) {
                            matched = false;
                            break;
                        }
                    }

                    h--;
                }

                if (matched) {
                    if (!combo.on_complete.empty()) {
                        completed.push_back(combo.on_complete);
                    }
                    break; // Only trigger once per combo per check
                }
            }
        }

        return completed;
    }

    void clear_history() { history.clear(); }
    void set_max_history(int max) { max_history = max; }
};

} // namespace eb
