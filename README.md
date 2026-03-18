# Twilight Engine

A cross-platform Vulkan 2D game engine built in C++20 for **top-down RPG** and **2D platformer** games. 28,474 lines of C++ across 109 files, 323 script API functions, 141 automated tests.

## Highlights

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles, 9-slice panels, fullscreen
- **Dual Game Modes** — Top-down RPG + 2D platformer per-level with one-click switching
- **Physics** — AABB/circle/polygon collision (SAT), raycasting (DDA), trigger zones, tilemap collision (8 types)
- **Platformer** — Gravity, coyote time, wall slide, double jump, air dash, moving platforms, stomping enemies, parallax backgrounds (8 biomes)
- **AI** — A* pathfinding, behavior trees, generic state machines, patrol routes, schedules, aggro/chase
- **Scripting** — 323 SageLang API functions across 56 modules with hot reload. Coroutines, trigger callbacks, combo detection
- **Editor** — Tile painting, NPC spawner, Script IDE, game systems panel, visual UI/HUD builder (18 templates), parallax manager, new map dialog
- **UI System** — Labels, bars, panels, images with 9-slice, layer sorting, opacity, tint, rotation, scale. Material Design color palette
- **Cross-Platform** — Linux, Windows, Android (touch + gamepad), Meta Quest

## Features

| Category | Features |
|----------|----------|
| **Rendering** | Sprite batching, Y-sort, 9-slice panels, trail/ribbon rendering, skeleton/bone animation, parallax backgrounds, water reflections, weather (rain/snow/lightning/fog/clouds/god rays), screen transitions (5 types), post-processing pipeline (stub) |
| **Physics & Collision** | AABB, circle, convex polygon (SAT), raycasting (DDA), trigger zones (enter/exit/stay), tilemap collision (solid/one-way/slope/ladder/hazard) |
| **Platformer** | Gravity, variable-height jump, coyote time, jump buffer, wall slide/jump, double jump, air dash, moving platforms, patrol/jump/fly enemies, stomp detection |
| **AI & Logic** | A* pathfinding, behavior trees, state machines, coroutines, combo detection, NPC routes/schedules, spawn system, trigger zones |
| **Game Systems** | Battle (turn-based), inventory/shop, equipment, quests, achievements, save/load, survival (hunger/thirst/energy), day-night cycle, character stats, localization, events |
| **UI/HUD** | Script-driven labels/bars/panels/images, 9-slice rendering, layer sorting (0-20), 18 editor templates, Material Design palette, bar text overlay |
| **Audio** | BGM crossfade, spatial SFX, bus mixer (music/SFX/ambience/voice), audio effects (stubs) |
| **Editor** | Paint/erase/fill/line/rect, undo/redo, prefabs, NPC spawner, Script IDE, auto-tiling (4-bit + 8-bit blob), parallax manager, new map dialog, UI/HUD visual editor |
| **Scripting** | 323 API functions, hot reload, coroutines, input replay, map script generation (upsert, not append) |
| **Generation** | Procedural tilesets (10 biomes), procedural parallax backgrounds (6 biomes), dungeon generation (BSP + cellular automata), 8-bit blob auto-tiling |
| **Architecture** | ECS (opt-in), object pooling, checkpoint/respawn, input recording, plugin/mod system (stub), networking (stub), isometric + hex tile utilities |
| **Platforms** | Linux, Windows (cross-compile), Android (touch + gamepad), Meta Quest |

## Quick Start

```bash
./build.sh linux Debug
./build-linux/twilight_game_binary          # Run game
./build-linux/twilight_game_binary --test   # Run 141 tests
```

See [Build Instructions](docs/BUILD.md) for all platforms.

## Demo Game

**Crystal Quest** — 6 maps across 5 biomes (forest, desert, snow, cave, volcanic), Mage/Black Mage party, merchant shop, day-night cycle, procedural tilesets. All maps connected via portals.

## Documentation

| Document | Description |
|----------|-------------|
| [Scripting API](docs/SCRIPTING.md) | Full SageLang reference (323 functions, 56 modules) |
| [Engine Guide](docs/Twilight_Engine_Guide.md) | Comprehensive guide to all systems |
| [Architecture](docs/ARCHITECTURE.md) | Module breakdown and data flow |
| [Editor Guide](docs/EDITOR.md) | Desktop + Android editor features |
| [Controls](docs/CONTROLS.md) | Desktop, gamepad, Android, Meta Quest |
| [Build](docs/BUILD.md) | Build instructions + project structure |
| [Tools](docs/TOOLS.md) | Python tools (tileset gen, parallax gen, fuzzer, etc.) |
| [Map Design](docs/MAP_DESIGN_GUIDE.md) | Map creation with biome portals |
| [Tile Reference](docs/TILE_REFERENCE.md) | Tileset format, stamps, procedural generator |
| [Battle System](docs/Battle_System_Comparison.md) | Battle system design |
| [Updates](docs/UPDATES.md) | Version changelog |

## Tech Stack

C++20, Vulkan, GLFW, GLM, Dear ImGui, miniaudio, SageLang, stb_image, stb_truetype, tinyfiledialogs, Python 3 + Pillow

## Stats

```text
                    ┌─────────────────────────────────────┐
                    │     TWILIGHT ENGINE v3.2.0           │
                    └─────────────────────────────────────┘

  C++ Source           28,474 lines across 109 files (excl. third-party)
  Script Engine         5,278 lines   323 API functions   56 modules
  Game Framework        6,235 lines   7 files
  Editor                5,209 lines   11 files   18 UI templates
  New Systems           2,725 lines   22 headers (collision, raycast, triggers, FSM, etc.)
  Test Suite              141 assertions across 50 test categories

  ┌─ Content ──────────────────────────────────────────────────────┐
  │  Maps              6   Forest, House, Desert, Snow, Cave, Lava │
  │  Tilesets         16   cf_tileset (1,080) + biome + legacy     │
  │  Parallax BGs     45   8 biome presets (6 procedural + 2 CC0) │
  │  Fantasy Icons   432   16x27 grid at 32x32                    │
  │  UI Templates     18   Dialogs, HUD, stats, menus, tooltips   │
  │  Python Tools     13   + tw_test package (11 modules)          │
  └────────────────────────────────────────────────────────────────┘

  ┌─ Engine Features ──────────────────────────────────────────────┐
  │  Collision Shapes   3   AABB, Circle, Convex Polygon (SAT)    │
  │  Tile Grid Types    3   Orthogonal + Isometric + Hex           │
  │  Dungeon Generators 2   BSP Tree + Cellular Automata           │
  │  Parallax Biomes    8   forest, cave, night, sunset, snow, etc.│
  │  Easing Types      19   Linear, Sine, Quad, Cubic, Back, etc.  │
  │  Particle Presets   9   fire, smoke, sparkle, blood, dust, etc.│
  │  Screen Transitions 5   Fade, Iris, Wipe, Pixelate, Slide     │
  │  Platforms          4   Linux, Windows, Android, Meta Quest    │
  └────────────────────────────────────────────────────────────────┘
```

## License

MIT
