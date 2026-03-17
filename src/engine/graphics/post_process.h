#pragma once
#include "engine/core/types.h"
#include <cstdio>

namespace eb {

// Post-processing effect configuration
// The actual Vulkan framebuffer/shader pipeline will be implemented
// when render-to-texture support is added. For now, this provides
// the data model and API surface so scripts can configure effects.
struct PostProcessConfig {
    bool enabled = false;
    bool crt = false;
    bool bloom = false;
    bool vignette = false;
    bool blur = false;
    bool color_grade = false;
    float bloom_threshold = 0.7f;
    float bloom_intensity = 0.5f;
    float vignette_strength = 0.5f;
    float blur_strength = 0.0f;
    Vec4 color_tint = {1, 1, 1, 1};
    float contrast = 1.0f;
    float brightness = 0.0f;
    float saturation = 1.0f;
    float crt_curvature = 0.03f;
    float crt_scanline_intensity = 0.15f;
};

// Placeholder — will manage offscreen framebuffer + fullscreen quad pipeline
class PostProcessPipeline {
public:
    PostProcessConfig config;

    bool is_available() const { return available_; }

    void log_status() {
        if (!available_)
            std::printf("[PostProcess] Pipeline not yet initialized (render-to-texture required)\n");
    }

private:
    bool available_ = false; // Set to true when Vulkan offscreen FB is implemented
};

} // namespace eb
