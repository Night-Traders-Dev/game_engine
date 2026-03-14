#include "engine/core/engine.h"
#include "engine/graphics/renderer.h"
#include "engine/platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <exception>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        eb::EngineConfig config;
        config.title = "EarthBound Engine";
        config.width = 960;
        config.height = 720;
        config.vsync = true;

        eb::Engine engine(config);

        // Point shader dir to where CMake compiles the .spv files
        engine.renderer().set_shader_dir("shaders/");

        // Set clear color to a dark EarthBound-esque blue
        engine.renderer().set_clear_color(0.05f, 0.05f, 0.12f);

        std::printf("[Main] Starting engine...\n");
        engine.run();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Fatal] %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
