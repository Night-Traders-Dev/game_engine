#pragma once
#include "game/systems/particles.h"
#include <string>

namespace eb {

class ParticleEditor {
public:
    bool is_open() const { return open_; }
    void set_open(bool o) { open_ = o; }

    // Preview emitter for live editing
    ParticleEmitter preview;

    // Preset management
    std::string current_preset_name;

    void reset_preview() {
        preview = ParticleEmitter{};
        preview.active = true;
        preview.rate = 20;
        preview.pos = {480, 360};
    }

private:
    bool open_ = false;
};

} // namespace eb
