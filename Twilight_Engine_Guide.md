# Twilight Engine Guide

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Build System](#build-system)
3. [Engine Core](#engine-core)
4. [Rendering Pipeline](#rendering-pipeline)
5. [Tile Map System](#tile-map-system)
6. [Sprite & Animation System](#sprite--animation-system)
7. [Battle System](#battle-system)
8. [Dialogue System](#dialogue-system)
9. [NPC & AI System](#npc--ai-system)
10. [Scripting with SageLang](#scripting-with-sagelang)
11. [Tile Editor](#tile-editor)
12. [Map File Format](#map-file-format)
13. [Asset Pipeline](#asset-pipeline)
14. [Android Platform](#android-platform)
15. [Adding New Content](#adding-new-content)

---

## Architecture Overview

Twilight Engine is a cross-platform 2D RPG engine built on Vulkan. The codebase is organized into layers:

```
Engine Layer (eb:: namespace)
  ├── Core:     Engine loop, timer, types (Vec2, Vec4, Mat4)
  ├── Graphics: VulkanContext, Renderer, SpriteBatch, Texture, TextureAtlas, Pipeline
  ├── Platform: PlatformDesktop (GLFW), PlatformAndroid (NativeActivity)
  ├── Resource: ResourceManager, FileIO (cross-platform asset loading)
  └── Scripting: ScriptEngine (SageLang integration)

Game Layer (global structs)
  ├── GameState: All game data (player, NPCs, battle, map, party)
  ├── game.h/cpp: Shared game logic (init, update, render)
  ├── TileMap: Tile storage, collision, portals, rendering
  ├── Camera: Viewport, follow, bounds, offset
  └── DialogueBox: Typewriter text, portraits, choices

Editor Layer (desktop only)
  ├── TileEditor: Tools, undo/redo, zoom, selection, clipboard
  ├── ImGuiIntegration: Vulkan+GLFW ImGui backend
  └── Dear ImGui: Professional windowed UI
```

**Key Design Principle**: Game logic is shared between desktop and Android via `game.h`/`game.cpp`. Platform-specific code (GLFW, touch controls, editor) is conditionally compiled. The editor is desktop-only; the game runs identically on all platforms.

---

## Build System

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| Linux    | GCC/Clang with C++20, Vulkan SDK, CMake 3.20+ |
| Windows  | MinGW-w64 cross-compiler (`x86_64-w64-mingw32-g++`) |
| Android  | Android SDK, NDK 27+, Gradle, Java 17+ |

### Build Commands

```bash
./build.sh linux [Debug|Release]    # Native Linux build
./build.sh win64 [Debug|Release]    # Cross-compile for Windows
./build.sh android [Debug|Release]  # Android APK
./build.sh all                      # Build all platforms
./build.sh clean                    # Remove all build artifacts
```

### Build Outputs

- `build-linux/twilight_game_binary` — Linux executable
- `build-win64/twilight_game_binary.exe` — Windows executable
- `android/app/build/outputs/apk/debug/app-debug.apk` — Android APK

### CMake Targets

- `tw_engine` — Static library containing all engine + game code
- `twilight_game_binary` — Desktop executable (links tw_engine)
- `sagelang` — Static library of SageLang interpreter

### Dependencies (auto-fetched via FetchContent)

- GLFW 3.4 (windowing)
- GLM 1.0.1 (math)
- Vulkan Headers (Windows cross-compile only)

### Vendored Third-Party

- stb_image.h — Image loading
- stb_truetype.h — Font rasterization
- Dear ImGui — Editor UI
- SageLang — Scripting language (git submodule)
- tinyfiledialogs — Native file dialogs

---

## Engine Core

### Engine Class (`engine/core/engine.h`)

The main loop driver. Creates the platform, renderer, and resource manager.

```cpp
eb::EngineConfig config;
config.title = "My Game";
config.width = 960;
config.height = 720;
config.vsync = true;

eb::Engine engine(config);

engine.on_update = [](float dt) { /* game logic */ };
engine.on_render = [&]() { /* draw calls */ };

engine.run(); // Blocks until quit
```

### Timer (`engine/core/timer.h`)

High-resolution frame timer with 0.1s clamp to prevent spiral of death.

### Types (`engine/core/types.h`)

```cpp
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

struct Rect { float x, y, w, h; };
struct Color { float r, g, b, a; };
```

---

## Rendering Pipeline

### VulkanContext

Handles Vulkan initialization: instance, device, swapchain, command pools, buffers.

Key features:
- Automatic swapchain recreation on resize
- Composite alpha detection (Android compatibility)
- Pre-transform handling for landscape mode
- Validation layer support (debug builds)

### Renderer

Frame management layer on top of VulkanContext.

```cpp
if (renderer.begin_frame()) {
    auto& batch = renderer.sprite_batch();
    batch.set_projection(camera.projection_matrix());
    batch.set_texture(my_texture_desc);
    batch.draw_quad(position, size, uv_min, uv_max, color);
    batch.draw_sorted(position, size, uv_min, uv_max, sort_y, texture_desc);
    batch.flush_sorted(); // Y-sorts and draws all sorted sprites
    batch.flush();
    renderer.end_frame();
}
```

### SpriteBatch

Batched quad rendering with Y-sorting support. Key limits:

- `MAX_QUADS = 20000` per frame across all flushes
- Uses vertex_offset tracking to prevent GPU buffer overwrites
- Automatic flush on texture change

### Shader Pipeline

Simple 2D sprite shaders:
- `sprite.vert` — Transforms by push-constant projection matrix
- `sprite.frag` — Texture sampling with vertex color tinting

---

## Tile Map System

### TileMap (`game/overworld/tile_map.h`)

Stores tile layers, collision data, and portals.

```cpp
TileMap map;
map.create(30, 20, 32);           // 30x20 tiles, 32px each
map.set_tileset(atlas);
map.add_layer("ground", tile_data);
map.set_collision(collision_data);
map.set_animated_tiles(TILE_WATER_DEEP, TILE_WATER_SHORE_L);
map.render(batch, camera, game_time);
```

### Tile Types

54 ground tiles organized by category:
- Grass (4 variants), Dirt (4), Edges/Hedges (4)
- Dirt paths (6), Special ground (6, including pentagram)
- Roads (12 pieces), Road extras (5)
- Water/Shore (9 types), Water objects (4)

39 object stamps (buildings, vehicles, trees, misc) for editor placement.

### Collision

Per-tile collision types: `None`, `Solid`, `Portal`. Queried with:
```cpp
bool blocked = map.is_solid_world(world_x, world_y);
```

### Animated Tiles

Water tiles animate automatically with wave displacement, UV distortion, and color shimmer when `set_animated_tiles()` is called.

---

## Sprite & Animation System

### TextureAtlas

Grid-based or custom region atlases for sprite sheets.

```cpp
TextureAtlas atlas(texture, cell_width, cell_height);
atlas.define_region("idle_down", x, y, w, h);
auto region = atlas.find_region("walk_right_0");
```

### Character Sprites

Characters use named regions with direction/animation lookup:
```cpp
auto sr = get_character_sprite(atlas, direction, is_moving, frame);
// direction: 0=down, 1=up, 2=left(flipped right), 3=right
// Left-facing sprites are automatically UV-flipped
```

### Walking Animation

- 2-frame walk cycle (alternates every 0.2s)
- Foot bobbing via `sin(anim_timer * 15) * 2px`
- Grass overlay: procedural 1px blades on interior grass tiles
- Leaf overlay: particles on tree canopies with wind sway

---

## Battle System

### Turn Flow

1. **Intro** — "A [Enemy] appeared!" (1.5s or confirm)
2. **Dean's Turn** — Attack / Defend / Run menu
3. **Sam's Turn** — Same menu (skipped if Sam is down)
4. **Enemy Turn** — Attacks random party member
5. Repeat until victory or defeat

### Rolling HP

EarthBound-style HP odometer — display rolls down toward actual HP at 40 HP/sec, giving time to heal before hitting zero.

### Attack Animations

- **Player attack**: Character lunges forward (sine wave), walk frames cycle
- **Enemy hit**: Sprite blinks (alternating visibility)
- **Damage numbers**: Shown in battle message box

### Battle Data

```cpp
struct BattleState {
    BattlePhase phase;
    int enemy_hp_actual, enemy_hp_max, enemy_atk;
    int player_hp_actual, player_hp_max;
    float player_hp_display; // Rolling odometer
    int sam_hp_actual, sam_hp_max;
    float sam_hp_display;
    int active_fighter; // 0=Dean, 1=Sam
};
```

---

## Dialogue System

### DialogueBox (`game/dialogue/dialogue_box.h`)

Features:
- Typewriter effect (35 chars/sec)
- Word wrapping
- Character portraits (loaded from cropped images)
- Dialog.png background texture
- Speaker name highlighting
- Blinking advance indicator
- Choice menus

### Dialogue Files (`.dialogue`)

Legacy format with labeled sections:
```sage
@greeting
Bobby: You idjits better be prepared.
Bobby: Watch your back.

@after_battle
Bobby: You boys alright?
Dean: Nothing a cold beer won't fix.
```

### SageLang Scripts (`.sage`)

Modern format with full scripting:
```sage
proc greeting():
    say("Bobby", "You idjits better be prepared.")
    if get_flag("has_shotgun"):
        say("Bobby", "Good, you've got supplies.")
    else:
        say("Bobby", "Talk to me about supplies.")
```

---

## NPC & AI System

### NPC Struct

```cpp
struct NPC {
    std::string name;
    Vec2 position, home_pos;
    int dir, frame;
    float interact_radius;
    bool hostile;           // Will chase player
    float aggro_range;      // Distance to start chasing
    float attack_range;     // Distance to trigger dialogue/battle
    float move_speed;
    float wander_interval;
    bool has_battle;
    int sprite_atlas_id;
    std::vector<DialogueLine> dialogue;
};
```

### AI Behaviors

- **Passive NPCs**: Wander randomly near home position, require manual interaction (Z/Enter)
- **Hostile NPCs**: Chase player when within aggro range, auto-trigger dialogue + battle at attack range
- **Post-encounter**: Hostile NPCs become passive after triggering (`has_triggered` flag)

### Walk Animation

NPCs animate while moving — direction determined by movement vector, frames toggle every 0.2s.

---

## Scripting with SageLang

### Overview

SageLang is embedded as a C library via the `ScriptEngine` wrapper class. Scripts are `.sage` files loaded at runtime.

### Engine API Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `say` | `say(speaker, text)` | Display dialogue line |
| `dialogue` | `dialogue(s1, t1, s2, t2, ...)` | Multi-line dialogue |
| `start_battle` | `start_battle(name, hp, atk)` | Trigger battle |
| `spawn_npc` | `spawn_npc(name, x, y)` | Create NPC at position |
| `teleport` | `teleport(x, y)` | Move player |
| `log` | `log(message)` | Debug output |
| `set_flag` | `set_flag(name, value)` | Set game flag |
| `get_flag` | `get_flag(name)` | Check game flag |

### Script Example

```sage
# bobby.sage
proc greeting():
    say("Bobby", "You idjits better be prepared.")
    say("Bobby", "I've got some supplies if you need 'em.")

proc supplies():
    say("Bobby", "Here, take this shotgun.")
    set_flag("has_shotgun", true)
    set_flag("has_holy_water", true)

proc after_battle():
    say("Bobby", "You boys alright?")
    say("Dean", "Nothing a cold beer won't fix.")
    set_flag("bobby_checked_in", true)
```

### Using Scripts

```cpp
eb::ScriptEngine script;
script.load_file("assets/scripts/bobby.sage");
script.call_function("greeting");  // Executes the greeting proc
script.set_number("player_hp", 100);
```

---

## Tile Editor

### Activation

Press **Tab** to toggle the editor. The game world renders underneath; editor UI overlays on top via Dear ImGui.

### Tools

| Tool | Key | Description |
|------|-----|-------------|
| Paint | P | Place selected tile |
| Erase | E | Remove tiles |
| Fill | F | Flood fill area |
| Eyedrop | I | Pick tile from map |
| Select | R | Rectangle selection |
| Collision | C | Cycle collision types |
| Line | L | Draw straight lines |
| Rectangle | B | Draw rectangles |

### Panels

- **Tools** — Tool buttons, grid/collision toggles, layer management, zoom, undo/redo, save/load
- **Assets** — Tabbed panel with Tiles, Buildings, Vehicles, Trees, Misc categories
- All assets show image previews from the tileset
- Ghost cursor shows selected item at mouse position

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Shift+Z / Ctrl+Y | Redo |
| Ctrl+S | Quick save |
| Ctrl+C | Copy selection |
| Ctrl+V | Paste |
| Ctrl+H | Flip clipboard horizontal |
| Ctrl+0 | Reset zoom |
| G | Toggle grid |
| V | Toggle collision overlay |
| 1-9 | Switch layer |
| Shift+1-9 | Toggle layer visibility |
| Delete | Clear selection |
| Middle mouse | Pan camera |
| Scroll wheel | Zoom (over map) / Scroll (over palette) |

### Undo/Redo

100-action history stack. All tile/collision operations are recorded and fully reversible.

---

## Map File Format

Maps are saved as JSON (version 2):

```json
{
  "format": "twilight_map",
  "version": 2,
  "metadata": {
    "name": "town_center",
    "width": 30, "height": 20, "tile_size": 32,
    "tileset": "assets/textures/new_tileset.png",
    "player_start_x": 480, "player_start_y": 320
  },
  "layers": [
    {"name": "ground", "data": [1,2,3,...]},
    {"name": "details", "data": [0,0,5,...]}
  ],
  "collision": [1,1,0,0,...],
  "portals": [
    {"x":5, "y":10, "target_map":"indoor.json", "target_x":3, "target_y":8, "label":"door"}
  ],
  "objects": [
    {"x":224, "y":256, "src_x":1007, "src_y":86, "src_w":157, "src_h":103, "render_w":140, "render_h":96}
  ],
  "npcs": [
    {"name":"Bobby", "x":256, "y":224, "dir":0, "sprite_atlas_id":0,
     "hostile":false, "has_battle":false,
     "dialogue":[{"speaker":"Bobby", "text":"Hello."}]}
  ]
}
```

---

## Asset Pipeline

### Tilesets

PNG images with sprites arranged in any layout. Regions are defined programmatically:

```cpp
atlas.add_region(x, y, width, height);  // Adds a tile region
atlas.define_region("name", x, y, w, h); // Named region
```

### Character Sprites

Sprite sheets with named regions for each direction/animation:
- `idle_down`, `idle_up`, `idle_right` (left = flipped right)
- `walk_down_0/1`, `walk_up_0/1`, `walk_right_0/1`

### Processing New Assets

Character sprite sheets from source PNGs are processed with Python:
1. Remove background (flood fill from edges)
2. Find sprite bounding boxes
3. Arrange into engine grid format (3 cols x 3 rows)
4. Save as `*_sprites.png`

### Fonts

TTF fonts are baked into texture atlases at runtime via stb_truetype. Default: DejaVu Sans Mono Bold at 7px with 6px letter spacing.

---

## Android Platform

### Landscape Mode

Forced via `setRequestedOrientation(SCREEN_ORIENTATION_SENSOR_LANDSCAPE)` in Java + `preTransform = IDENTITY` in Vulkan swapchain.

### Virtual Resolution

Android uses 480p virtual height (width scaled by aspect ratio). The GPU stretches the rendered frame to fill the native screen. Touch controls render at native resolution.

### Touch Controls

Scaled dynamically based on screen DPI:
- D-pad (bottom-left): 120px base radius
- A button (confirm): 60px base radius
- B button (cancel): 60px
- Menu button (top-right)

---

## Adding New Content

### Adding a New NPC

1. Create sprite sheet (3x3 grid: idle/walk for down/up/right)
2. Add atlas region in `define_tileset_regions()` or load as separate texture
3. Add NPC in `setup_npcs()` with position, sprite ID, dialogue
4. Create `.sage` script in `assets/scripts/`

### Adding a New Tile Type

1. Add entry to `Tile` enum in `game.h`
2. Add `atlas.add_region()` call in `define_tileset_regions()`
3. Available immediately in the editor's Tiles palette

### Adding a New Map

1. Use the editor to build the map (Tab to enter editor)
2. Save via Save As... button (native file dialog)
3. Map file includes tiles, collision, objects, NPCs, portals

### Adding New Script Functions

1. Write a native C++ function matching `Value fn(int argc, Value* args)`
2. Register in `ScriptEngine::register_engine_api()`
3. Call from `.sage` scripts

---

*Twilight Engine v0.1.0 — Built with Vulkan, SageLang, and Dear ImGui*
