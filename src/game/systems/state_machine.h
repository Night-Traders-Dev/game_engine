#pragma once

#include <string>
#include <unordered_map>

namespace eb {

// ─── Finite State Machine ───

struct FSMState {
    std::string name;
    std::string on_enter;
    std::string on_update;
    std::string on_exit;
};

class StateMachine {
    std::unordered_map<std::string, FSMState> states;
    std::string current;
    std::string previous;
    bool needs_enter = true;
    bool needs_exit = false;

public:
    void add_state(const std::string& name,
                   const std::string& on_enter,
                   const std::string& on_update,
                   const std::string& on_exit) {
        states[name] = {name, on_enter, on_update, on_exit};
    }

    void transition(const std::string& name) {
        if (name == current) return;
        if (states.find(name) == states.end()) return;
        previous = current;
        needs_exit = !current.empty();
        current = name;
        needs_enter = true;
    }

    std::string update() {
        if (current.empty()) return "";
        auto it = states.find(current);
        if (it == states.end()) return "";
        return it->second.on_update;
    }

    std::string check_enter() {
        if (!needs_enter || current.empty()) return "";
        auto it = states.find(current);
        if (it == states.end()) return "";
        needs_enter = false;
        return it->second.on_enter;
    }

    std::string check_exit() {
        if (!needs_exit || previous.empty()) return "";
        auto it = states.find(previous);
        if (it == states.end()) {
            needs_exit = false;
            return "";
        }
        needs_exit = false;
        return it->second.on_exit;
    }

    const std::string& current_state() const { return current; }
    const std::string& previous_state() const { return previous; }
};

} // namespace eb
