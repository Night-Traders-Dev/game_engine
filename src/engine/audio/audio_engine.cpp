#include "engine/audio/audio_engine.h"
#include "engine/resource/file_io.h"
#include "miniaudio.h"

#include <cstdio>
#include <cstring>
#include <new>

namespace eb {

AudioEngine::AudioEngine() {
    engine_ = new(std::nothrow) ma_engine();
    if (!engine_) {
        std::fprintf(stderr, "[Audio] Failed to allocate audio engine\n");
        return;
    }
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    config.sampleRate = 44100;

    ma_result result = ma_engine_init(&config, engine_);
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "[Audio] Failed to initialize audio engine (error %d)\n", result);
        delete engine_;
        engine_ = nullptr;
        return;
    }

    initialized_ = true;
    std::printf("[Audio] Engine initialized (44100 Hz, stereo)\n");
}

AudioEngine::~AudioEngine() {
    cleanup_sound(music_);
    cleanup_sound(music_next_);
    if (engine_) {
        ma_engine_uninit(engine_);
        delete engine_;
    }
}

void AudioEngine::cleanup_sound(ma_sound*& sound) {
    if (sound) {
        ma_sound_uninit(sound);
        delete sound;
        sound = nullptr;
    }
}

// ─── Background Music ───

bool AudioEngine::play_music(const std::string& path, bool loop) {
    if (!initialized_) return false;

    stop_music();

    music_ = new ma_sound();
    ma_result result = ma_sound_init_from_file(engine_, path.c_str(),
        MA_SOUND_FLAG_STREAM, nullptr, nullptr, music_);

    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "[Audio] Failed to load music: %s (error %d)\n", path.c_str(), result);
        delete music_;
        music_ = nullptr;
        return false;
    }

    ma_sound_set_looping(music_, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(music_, music_volume_);
    ma_sound_start(music_);
    current_music_path_ = path;

    std::printf("[Audio] Playing music: %s%s\n", path.c_str(), loop ? " (loop)" : "");
    return true;
}

void AudioEngine::stop_music() {
    if (music_) {
        ma_sound_stop(music_);
        cleanup_sound(music_);
        current_music_path_.clear();
    }
    if (music_next_) {
        ma_sound_stop(music_next_);
        cleanup_sound(music_next_);
    }
    crossfading_ = false;
}

void AudioEngine::pause_music() {
    if (music_) ma_sound_stop(music_);
}

void AudioEngine::resume_music() {
    if (music_) ma_sound_start(music_);
}

void AudioEngine::set_music_volume(float volume) {
    music_volume_ = volume;
    if (music_) ma_sound_set_volume(music_, music_volume_);
}

bool AudioEngine::is_music_playing() const {
    return music_ && ma_sound_is_playing(music_);
}

// ─── Sound Effects ───

void AudioEngine::play_sfx(const std::string& path, float volume) {
    if (!initialized_) return;
    ma_engine_play_sound(engine_, path.c_str(), nullptr);
}

// ─── Master Volume ───

void AudioEngine::set_master_volume(float volume) {
    if (engine_) ma_engine_set_volume(engine_, volume);
}

// ─── Crossfade ───

void AudioEngine::crossfade_music(const std::string& path, float duration_sec, bool loop) {
    if (!initialized_) return;

    if (path == current_music_path_) return; // Already playing this track

    // If nothing playing, just start directly
    if (!music_ || !ma_sound_is_playing(music_)) {
        play_music(path, loop);
        return;
    }

    // Start new track at volume 0
    cleanup_sound(music_next_);
    music_next_ = new ma_sound();
    ma_result result = ma_sound_init_from_file(engine_, path.c_str(),
        MA_SOUND_FLAG_STREAM, nullptr, nullptr, music_next_);

    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "[Audio] Failed to load crossfade music: %s\n", path.c_str());
        delete music_next_;
        music_next_ = nullptr;
        return;
    }

    ma_sound_set_looping(music_next_, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(music_next_, 0.0f);
    ma_sound_start(music_next_);

    crossfading_ = true;
    crossfade_timer_ = 0.0f;
    crossfade_duration_ = duration_sec;
    current_music_path_ = path;

    std::printf("[Audio] Crossfading to: %s (%.1fs)\n", path.c_str(), duration_sec);
}

// ─── Update ───

void AudioEngine::update(float dt) {
    if (!crossfading_) return;

    crossfade_timer_ += dt;
    float t = crossfade_timer_ / crossfade_duration_;

    if (t >= 1.0f) {
        // Crossfade complete: swap tracks
        cleanup_sound(music_);
        music_ = music_next_;
        music_next_ = nullptr;
        ma_sound_set_volume(music_, music_volume_);
        crossfading_ = false;
    } else {
        // Interpolate volumes
        if (music_) ma_sound_set_volume(music_, music_volume_ * (1.0f - t));
        if (music_next_) ma_sound_set_volume(music_next_, music_volume_ * t);
    }
}

} // namespace eb
