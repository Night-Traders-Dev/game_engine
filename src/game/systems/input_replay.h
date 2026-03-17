#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace eb {

// ─── Input Replay System ───

struct InputFrame {
    float timestamp;
    uint32_t action_bits;
    float mouse_x;
    float mouse_y;
};

// Binary format: "TWIR" magic (4 bytes) + frame count (uint32_t) + raw InputFrame array

class InputRecorder {
    std::vector<InputFrame> frames;
    bool recording = false;

public:
    void start() {
        frames.clear();
        recording = true;
    }

    void stop() {
        recording = false;
    }

    void record(float time, uint32_t action_bits, float mouse_x, float mouse_y) {
        if (!recording) return;
        frames.push_back({time, action_bits, mouse_x, mouse_y});
    }

    bool save(const std::string& path) const {
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        // Magic header
        const char magic[4] = {'T', 'W', 'I', 'R'};
        std::fwrite(magic, 1, 4, f);

        // Frame count
        uint32_t count = static_cast<uint32_t>(frames.size());
        std::fwrite(&count, sizeof(uint32_t), 1, f);

        // Raw frame data
        if (!frames.empty()) {
            std::fwrite(frames.data(), sizeof(InputFrame), count, f);
        }

        std::fclose(f);
        return true;
    }

    bool is_recording() const { return recording; }
    int frame_count() const { return static_cast<int>(frames.size()); }
    const std::vector<InputFrame>& get_frames() const { return frames; }
};

class InputReplayer {
    std::vector<InputFrame> frames;
    int playhead = 0;
    bool playing = false;

public:
    bool load(const std::string& path) {
        frames.clear();
        playhead = 0;
        playing = false;

        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;

        // Verify magic
        char magic[4];
        if (std::fread(magic, 1, 4, f) != 4) { std::fclose(f); return false; }
        if (std::memcmp(magic, "TWIR", 4) != 0) { std::fclose(f); return false; }

        // Read frame count
        uint32_t count = 0;
        if (std::fread(&count, sizeof(uint32_t), 1, f) != 1) { std::fclose(f); return false; }

        // Read frames
        frames.resize(count);
        if (count > 0) {
            size_t read = std::fread(frames.data(), sizeof(InputFrame), count, f);
            if (read != count) {
                frames.clear();
                std::fclose(f);
                return false;
            }
        }

        std::fclose(f);
        return true;
    }

    void start() {
        playhead = 0;
        playing = true;
    }

    void stop() {
        playing = false;
    }

    InputFrame get_frame(float time) {
        if (frames.empty() || !playing) {
            return {0, 0, 0, 0};
        }

        // Advance playhead to the latest frame at or before the given time
        while (playhead < static_cast<int>(frames.size()) - 1 &&
               frames[playhead + 1].timestamp <= time) {
            playhead++;
        }

        return frames[playhead];
    }

    bool is_done() const {
        if (!playing || frames.empty()) return true;
        return playhead >= static_cast<int>(frames.size()) - 1;
    }

    bool is_playing() const { return playing; }
    int frame_count() const { return static_cast<int>(frames.size()); }
};

} // namespace eb
