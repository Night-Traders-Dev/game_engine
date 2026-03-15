#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct ma_engine;
struct ma_sound;

namespace eb {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool is_initialized() const { return initialized_; }

    // Background music
    bool play_music(const std::string& path, bool loop = true);
    void stop_music();
    void pause_music();
    void resume_music();
    void set_music_volume(float volume); // 0.0 - 1.0
    bool is_music_playing() const;

    // Sound effects
    void play_sfx(const std::string& path, float volume = 1.0f);

    // Master volume
    void set_master_volume(float volume);

    // Crossfade to new music track
    void crossfade_music(const std::string& path, float duration_sec = 1.0f, bool loop = true);

    // Update (call each frame for crossfade support)
    void update(float dt);

private:
    ma_engine* engine_ = nullptr;
    ma_sound* music_ = nullptr;
    ma_sound* music_next_ = nullptr; // For crossfade
    bool initialized_ = false;

    float music_volume_ = 0.7f;
    float crossfade_timer_ = 0.0f;
    float crossfade_duration_ = 0.0f;
    bool crossfading_ = false;

    std::string current_music_path_;

    void cleanup_sound(ma_sound*& sound);
};

} // namespace eb
