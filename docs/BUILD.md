# Build Instructions

## Prerequisites

- Vulkan SDK
- CMake 3.20+
- C++20 compiler (GCC 11+, Clang 14+)
- For Windows: mingw-w64
- For Android: Android SDK/NDK 27

## Commands

```bash
# Build for Linux (launches fullscreen)
./build.sh linux Debug
./build.sh linux Release

# Cross-compile for Windows (requires mingw-w64)
./build.sh win64

# Build for Android (requires Android SDK/NDK 27)
./build.sh android

# Build for Meta Quest (flat 2D mode)
./build.sh quest

# Build all platforms
./build.sh all

# Run test suite (exit 0 = pass, exit 1 = fail)
./build-linux/twilight_game_binary --test

# Clean
./build.sh clean
```

## Project Structure

```text
src/
  engine/                    # Standalone engine
    graphics/                # Vulkan renderer, sprite batch, atlas, pipeline, text
    scripting/               # SageLang bindings (9 files, 5,278 lines, 323 API functions)
    platform/                # Desktop (GLFW), Android (NDK), Meta Quest, input, touch
    audio/                   # miniaudio wrapper (crossfade, spatial, bus mixer)
    physics/                 # Collision (AABB/circle/polygon SAT), raycast (DDA)
    core/                    # Engine loop, timer, types, debug log, Perlin noise
    resource/                # File I/O, manifest loader, textures
    ecs/                     # Lightweight Entity-Component-System (SparseSet)
    net/                     # Networking stubs (UDP packets)
  game/                      # RPG/platformer framework (6,235 lines)
    game_render.cpp          #   World, parallax, HUD, UI overlay, 9-slice (1,680 lines)
    game.cpp                 #   Core update loop + all systems (1,087 lines)
    game.h                   #   GameState + all data structs (950 lines)
    game_init.cpp            #   Init, tilesets, NPCs (893 lines)
    game_io.cpp              #   Map I/O, auto-reflect (572 lines)
    game_platformer.cpp      #   Platformer physics + enemies (537 lines)
    game_battle.cpp          #   Turn-based battle (516 lines)
    ai/                      # A* pathfinding, behavior trees
    systems/                 # 22 system headers: triggers, FSM, pool, checkpoint,
                             #   combo, replay, trails, rule tiles, dungeon gen,
                             #   skeleton, coroutine, tween, particles, save, etc.
    overworld/               # Camera (smooth zoom, Perlin shake), tile map, iso/hex
    ui/                      # Merchant store UI
    dialogue/                # Typewriter text, portraits
  editor/                    # Tile editor (5,209 lines, 11 files)
    tile_editor.cpp          #   Core + map script management (2,269 lines)
    tile_editor_ui.cpp       #   UI/HUD editor, 18 templates (1,364 lines)
  third_party/               # miniaudio, stb_image, stb_truetype, imgui, sagelang
games/
  demo/                      # "Crystal Quest" demo
    game.json                # Game manifest
    assets/                  # Maps, scripts, textures, fonts, audio, parallax
tools/                       # 13 Python tools + tw_test package
docs/                        # Engine documentation (8 files)
shaders/                     # GLSL → SPIR-V
android/                     # Gradle build
```
