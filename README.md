# Twilight Engine

A cross-platform Vulkan 2D game engine built in C++20, supporting both **top-down RPG** and **2D platformer** modes. Ships with AABB/circle/polygon collision, raycasting, trigger zones, behavior trees, coroutines, procedural dungeon generation, platformer physics (gravity, jumping, coyote time, wall slide, dash), moving platforms, stomping enemies, parallax backgrounds (8 biome presets + procedural generator), trail rendering, skeleton animation, a tween engine, particle system, save/load, quests, equipment, 2D lighting, an integrated tile editor with visual UI/HUD builder (18 templates, 9-slice panels), SageLang scripting (323 API functions across 56 modules), and procedural tileset generation for 10 biome types.

## Features

- **Vulkan Renderer** — Sprite batching with rotated quad support, Y-sorted rendering, texture atlases with half-texel UV correction, animated tiles, fullscreen, per-sprite tint/alpha
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls, editor overlay), Meta Quest (flat 2D mode)
- **Dual Game Modes** — Top-down RPG and 2D platformer modes per-level. Switch with `set_game_type("platformer")` in scripts or the editor dropdown
- **Platformer Physics** — Gravity, velocity, variable-height jumping, coyote time (0.08s), jump buffering (0.12s), wall slide/jump, double jump, air dash. Configurable per-level
- **Platformer Collision** — Solid, one-way platforms (jump through from below), 45-degree slopes (up/down), ladders, hazard tiles. All paintable in the editor
- **Platformer Enemies** — Patrol (walk back/forth with edge detection), jump, fly patterns. Stomping kills enemies and bounces the player. Contact damage with knockback. All script-driven
- **Moving Platforms** — Waypoint-based platforms with configurable speed, ping-pong/loop modes. Carries the player when riding. Script API: `add_platform()`, `platform_add_waypoint()`
- **Platformer Camera** — Horizontal follow with facing-direction lookahead, vertical deadzone (only tracks when player exits band), faster snap when grounded
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision grid, reflection grid (per-tile water/ice reflections), portals, animated water/grass overlays, per-tile rotation (0°/90°/180°/270°) and flip
- **Level System** — Multi-level manager with load/cache/switch, portal auto-transitions, background ticking, per-level zoom, per-level map scripts, level selector in pause menu
- **Tween/Easing Engine** — 19 easing types (sine, quad, cubic, back, bounce, elastic), tween any UI/camera/player/NPC property, loop/yoyo, delayed callbacks, script-driven cutscene sequencing
- **Particle System** — Emitters with color/size interpolation, gravity, spread angles, shapes (point/circle/rect). 9 presets: fire, smoke, sparkle, blood, dust, magic, explosion, heal, rain_splash
- **Save/Load System** — JSON save slots, persistent key-value flags, playtime tracking, full player state serialization
- **2D Lighting** — Ambient light level + point lights with radius/intensity/color, multiplicative darkening with light circles
- **Screen Transitions** — Fade, iris (circle wipe), directional wipe, pixelate, slide. Script-driven with callbacks
- **Quest System** — Start/complete/fail quests with objectives, HUD tracker, saved via flag system
- **Equipment System** — Weapon/armor/accessory/shield slots with stat bonuses (ATK/DEF/HP)
- **Gamepad Support** — Full GLFW gamepad with deadzone, stick/dpad/buttons mapped to actions, real-time state in editor
- **Rebindable Controls** — `KeyBindings` struct with primary/secondary keys per action, runtime rebinding
- **Input Buffering** — 150ms input buffer for lenient timing on dialogue confirm, NPC interaction, and inventory use
- **Event System** — Register listeners and emit named events from scripts, decoupled game logic
- **Achievement System** — Unlock/check achievements persisted via save flag system
- **Localization** — String key lookup with locale fallback (`loc("key")` returns translated text)
- **Battle System** — Turn-based combat with rolling HP, party members, attack animations
- **Inventory & Shop System** — SageLang-driven items with battle submenu, elemental weaknesses, stacking; merchant store UI with buy/sell and scalable pixel art panels
- **Character Stats** — Fallout S.P.E.C.I.A.L.-style stats: Vitality, Arcana, Agility, Tactics, Spirit, Strength
- **Day-Night Cycle** — Real-time clock with visual tinting (dawn/day/sunset/dusk/night), configurable speed
- **NPC AI** — A* pathfinding, waypoint patrol routes, idle wandering with collision, hostile aggro/chase, spatial grid separation
- **NPC Schedules** — Time-based NPC visibility tied to the day-night cycle (e.g. merchant appears 8AM-6PM)
- **NPC Interactions** — Proximity-triggered callbacks between NPCs, face-each-other, meet events
- **Spawn System** — Periodic NPC spawning with configurable intervals, max counts, spawn areas, time-gating, and callbacks
- **Survival System** — Hunger, thirst, energy meters with configurable depletion rates and gameplay effects
- **Per-Sprite Scaling** — Independent scale, tint, and flip per NPC, per object, player, and ally. Mix different sizes on screen (giant bosses + tiny minions)
- **Script-Driven UI** — Labels, bars, panels, images with per-component opacity, layer, rotation (all types), flip, scale, `on_click` callbacks, and `ui_get()`/`ui_set()` for reading/writing any property. **9-slice panel rendering** (corners fixed, edges/center stretch) with configurable border. **Layer-sorted rendering** (0-20, panels→images→labels→bars). **Bar text overlay** (`show_text`). Editor provides 18 templates, style picker dropdowns for all panel and icon assets
- **Script-Driven Pause Menu** — 6-item pause menu (Resume, Editor, Levels, Reset, Settings, Quit) with built-in level selector sub-menu
- **Script-Driven HUD** — All HUD layout defined in `default.sage`. C++ auto-syncs values each frame (HP bar auto-colors, sun/moon icon auto-swaps)
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends; path-sanitized file operations
- **Dialogue System** — SageLang-driven dialogue via `say()`, typewriter text, character portraits
- **Weather System** — Rain, snow, lightning, procedural cloud shadows, god rays, fog, wind. All script-controllable with presets (`set_weather("storm")`) and per-parameter control. Dynamic time-based weather changes
- **Multi-Grid Sprites** — Per-NPC sprite grid dimensions (16x16, 32x32, 32x48, 64x64, etc.). Same texture can be loaded with different grids. No conversion needed — just specify the grid size
- **Procedural Tileset Generator** — `tools/generate_tileset.py` creates complete pixel-art tilesets for 10 biome types with noise-based terrain, autotile transitions, decorations, water animations, and object stamps
- **Sprite Animation** — Multi-frame animation player per NPC with define/play/stop, loop and one-shot modes
- **Dialogue History** — Track conversations, remember who player has talked to, branch on past choices
- **Parallax Backgrounds** — Multi-layer scrolling with per-layer scroll speed, tint, auto-scroll, scale, pin-bottom, fill-viewport. Z-order sorted rendering. 8 biome presets. Script API: `add_parallax()`, `set_parallax()`, `load_parallax_preset()`, `clear_parallax()`
- **Spatial Audio** — Distance-based SFX volume falloff from camera center
- **Settings Menu** — In-game settings accessible from pause menu: music volume, SFX volume, text speed. Left/Right to adjust, changes apply immediately
- **Debug Overlay** — F1 toggle shows FPS, particle count, NPC count, tween count
- **SageLang Scripting** — 323 API functions across 56 modules driving all game systems with hot reload. See [docs/SCRIPTING.md](docs/SCRIPTING.md) for the full API reference
- **Asset Pipeline** — Multi-resolution asset generator; procedural tileset generator (10 biomes); auto-discovery of biome stamps; 1,080 base tiles, 88+ stamps, 432 fantasy icons, 3 UI spritesheets
- **Test Automation Tool** — `tools/tw_test/` Python package for automated game testing via XTest keyboard injection and X11 screenshot capture
- **String-Keyed Atlas Cache** — Shared texture atlas cache keyed by path+grid-size; runtime sprite loading from scripts
- **Test Suite** — `--test` CLI flag runs 101 assertions across 33 test categories; also callable from the F4 debug console via `run_all_tests()`
- **Security Hardened** — Path traversal protection, input clamping, file size validation, descriptor pool exhaustion protection, Vulkan resource cleanup, thread-safe Android platform, division-by-zero guards, bounds-checked array access, fuzzer-verified API safety
- **Map Scripting** — Visual Basic-style editor: every editor action auto-generates SageLang in a companion map script
- **Party System** — EarthBound-style follower trail with smooth interpolation
- **9-Slice Panel Rendering** — Corners stay fixed-size, edges stretch in one axis, center stretches both. Configurable border inset per panel. Works with any atlas region
- **AABB / Shape Collision** — Rect vs rect, rect vs circle, circle vs circle, convex polygon (SAT). `CollisionResult` with overlap normal + depth. Per-entity `Collider` on player and NPCs
- **Raycasting** — Ray vs rect, circle, tilemap (DDA grid traversal). `line_of_sight()` for LOS checks between two points
- **Trigger Zones** — Scriptable rectangular/circular areas with `on_enter`/`on_exit`/`on_stay` callbacks. One-shot triggers, per-entity tracking
- **Coroutines** — Step-based coroutine manager for cutscenes and boss patterns: define sequences of (function, delay) pairs, start/stop/loop
- **Generic State Machine** — Reusable FSM per entity with script-driven `on_enter`/`on_update`/`on_exit` callbacks per state
- **Behavior Trees** — Sequence/Selector/Parallel composites, Inverter/Repeater/Cooldown decorators, Action/Condition leaves for NPC AI
- **Trail / Ribbon Rendering** — Polyline trails with age-based width/alpha interpolation for sword swings, dash effects, bullet paths
- **Skeleton / Bone Animation** — 2D forward kinematics with parent-child bone transforms, keyframe interpolation, sprite attachments per bone
- **Procedural Dungeon Generation** — BSP tree + cellular automata generators output tile + collision data directly into TileMap
- **8-Bit Blob Auto-Tiling** — 256→47 LUT for corner-aware tile transitions (upgrade from 4-bit)
- **Platformer Backgrounds** — 8 biome parallax presets (45 PNGs), per-layer tint/auto-scroll/scale/pin-bottom/fill-viewport, z-order sorting. Procedural generator tool. Editor panel with preset loader
- **Camera Smooth Zoom** — Lerped zoom transitions with `zoom_to(target, speed)`, zoom-aware orthographic projection
- **Camera Perlin Shake** — Smooth noise-based camera shake with configurable frequency and fade-out
- **Combo / Input Sequence Detection** — Ring buffer of timestamped inputs with configurable timing windows for fighting game inputs
- **Checkpoint / Respawn** — Named checkpoints per map, activate/respawn system for platformer death
- **Object Pooling** — Generic `ObjectPool<T>` template with freelist for O(1) acquire/release
- **Input Recording / Replay** — Binary record/playback of all input actions with timestamps
- **Audio Bus / Mixer** — Separate volume channels for music, SFX, ambience, voice
- **Post-Processing** — Data model + API for CRT, bloom, vignette, blur, color grading effects (Vulkan framebuffer backend ready)
- **ECS** — Lightweight Entity-Component-System with `SparseSet<T>` storage, opt-in alongside existing struct-based entities
- **Networking** — Packet protocol + UDP socket architecture defined (client/server stubs)
- **Plugin / Mod System** — `ModLoader` with manifest-driven .sage module loading from game directories
- **Isometric Tiles** — Diamond-grid coordinate conversion + draw-order sorting utilities
- **Hex Tiles** — Pointy-top offset coordinates, 6-neighbor lookup, hex distance calculation
- **New Map Creation** — Editor File > New Map dialog + `new_map()` script API. Top-Down or Platformer presets with auto-configured ground/collision

### Editor

#### Desktop (Tab to toggle)

- **Menu Bar** — File (Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Reflection, Line, Rectangle, Portal
- **Brush Sizes** — 1x1, 2x2, 3x3
- **Assets Panel** — Tabbed tileset browser (Tiles, Buildings, Furniture, Characters, Trees, Vehicles, Misc) with image previews and keyboard shortcuts (Q/E to cycle, [ ] brackets, F5-F11 direct jump)
- **Minimap** — Color-coded map overview with player/NPC markers, click to teleport
- **NPC Spawner** (F2) — Spawn NPCs with presets (animals, enemies, villagers), click-to-place on map
- **Script IDE** (F3) — Built-in SageLang editor with syntax highlighting, asset click-to-highlight, menu bar (File/Help), integrated API manual, and a **Map Script** panel (gold "MAP SCRIPT" header) for editing the current map's companion script
- **Debug Console** (F4) — Color-coded log stream, filter by level, live SageLang command input
- **Game Systems Panel** (F5) — Tweens, particles, lighting, quests, equipment, save/load, settings, achievements, transitions, map resize, auto-tiling config, gamepad state, dialogue history, events — all with live controls
- **Object Inspector** (View menu) — Edit world object position, scale per-object
- **Prefab System** (View menu) — Save tile selections as reusable prefabs, click-to-paste
- **Map Resize** — Resize maps from the Systems panel (4x4 to 200x200)
- **Auto-Tiling** — 4-bit corner bitmask auto-selects from 16 transition tiles when painting terrain boundaries. Configure terrain pairs in Systems panel (F5)
- **UI/HUD Editor** (F6) — Visual drag-and-drop: click to select, drag to move (HUD groups move together), right-drag edges to resize (panels, bars, images). Style picker dropdowns (22 panel styles, 432+ icons). 18 templates (dialog box, confirm dialog, toast, quest tracker, status bar, boss HP, XP bar, buff row, location banner, character stats, party HUD, equipment slots, inventory grid, pause menu, title screen, settings panel, tooltip, shop window, + more). 9-slice checkbox + border slider. Live property editing with color pickers, opacity, scale, layer, rotation. Edits write back to HUD config for persistence
- **Map Script Generation** — Every editor action auto-appends SageLang to the map's companion `.sage` script

#### Android (Menu button toggles editor)

- Tap the **hamburger menu button** (top-right) to open the full-screen editor menu
- **Tools** — Paint, Erase, Fill, Collision with brush size selection
- **Layers** — Select active tile layer
- **Tile Select** — Grid of tile IDs for touch selection
- **NPC Spawner** — Preset NPCs (Villager, Chicken, Cow, Slime, Skeleton) spawned at player position
- **Map Info** — Map dimensions, NPC count, player position, gold, inventory stats

### Script IDE

- **File Menu** — New Script, Open Script, Save, Save & Reload, Reload All
- **Help Menu** — SageLang API Manual (8 tabbed sections: Overview, Dialogue, Inventory, Shop, Battle, Stats, Debug, Utilities)
- **Syntax Highlighting** — Keywords (purple), strings (gold), numbers (cyan), built-in functions (teal), booleans (orange), comments (green)
- **Asset Highlighting** — Click any string literal in code to highlight matching NPCs/items in the game world for 5 seconds
- **View/Edit Toggle** — Syntax-highlighted read-only view by default; click "Edit" for full text editing
- **Map Script Panel** — The current map's companion `.sage` script appears at the top of the file list under a gold "MAP SCRIPT" header; regular scripts are listed below under a blue "SCRIPTS" header. The map script is fully editable and saveable from the IDE

### Map Scripting (Visual Basic Style)

Each map has a companion `.sage` script that is the source of truth for game logic. JSON stores tile data; the script stores everything else.

```sage
# assets/scripts/maps/village.sage (auto-generated by editor)
proc map_init():
    spawn_npc("Elder", 320, 256, 0, false, 0, 0, 0, 20, 40)
    spawn_npc("Merchant", 512, 256, 0, false, 1, 0, 0, 15, 40)
    place_object(160, 320, "Oak Tree")
    set_portal(5, 11, "forest.json", 10, 20, "To Forest")
    npc_set_schedule("Merchant", 8, 18)
    npc_add_waypoint("Elder", 320, 256)
    npc_add_waypoint("Elder", 480, 256)
    npc_set_route("Elder", "patrol")
    npc_start_route("Elder")
```

Editor actions automatically append lines. Edit the script manually for advanced logic.

### Day-Night Cycle

```sage
set_day_speed(60)           # Full day in 24 seconds
set_time(22, 0)             # Set to 10 PM
if is_night():
    spawn_loop("Skeleton", 30, 3)
```

### HUD System

```sage
# Configure HUD from script
hud_set("scale", 2.0)
hud_set("show_survival", true)
hud_set("inv_max_slots", 10)
```

### Import System

```sage
# All scripts share a global environment.
# Functions from lib/hud.sage are available directly:
setup_player_panel(8, 8, 340, 90)    # Defined in lib/hud.sage
setup_time_panel(780, 8, 170, 80)    # No import needed

# The `import` keyword is supported for SageLang's native module system,
# but parametric cross-file calls (e.g. hud.func(x, y)) may crash due to
# AST source-buffer limitations. Use direct calls instead.

# Search paths: scripts/, scripts/lib/, scripts/battle/,
#               scripts/inventory/, scripts/maps/
```

### UI Components

```sage
# Panels and images from UI sprite sheet
ui_panel("quest_bg", 380, 8, 200, 40, "panel_mini")
ui_image("quest_icon", 388, 14, 24, 24, "icon_book")
ui_label("quest_text", "Explore the village", 418, 16, 0.9, 0.85, 0.7, 1)

# Modify any component property by ID
ui_set("quest_text", "text", "Find the Crystal")
ui_set("quest_bg", "visible", false)
```

### Inventory Quick-Use

```sage
# Items with sage_func are auto-called when used from inventory
add_item("bread", "Bread", 5, "consumable", "Restores hunger", 0, 0, "", "use_bread")

proc use_bread():
    set_hunger(get_hunger() + 40)
    ui_notify("Ate bread!", 2)
```

### Survival System

```sage
enable_survival(true)
set_survival_rate("hunger", 1.5)
# Items restore stats
set_hunger(get_hunger() + 40)
```

## Games

| Game     | Description |
|----------|-------------|
| **demo** | "Crystal Quest" — FF-style RPG with 6 maps across 5 biomes, Mage/Black Mage party, merchant shop, day-night cycle, procedural tilesets |

### Demo Levels

| Map | Biome | Tileset | Description |
|-----|-------|---------|-------------|
| Enchanted Forest | Forest | cf_tileset.png | Starting area with house, Elder NPC, merchant, wolves at night |
| House Inside | Interior | cf_tileset.png | Interior of the forest house |
| Desert | Desert | desert_tileset.png | Sand dunes, cacti, palm trees, ruins, tent |
| Snow | Tundra | snow_tileset.png | Snow fields, ice, pine trees, igloo, snowman |
| Cave | Dungeon | cave_tileset.png | Dark stone, crystals, stalagmites, skeleton |
| Volcanic | Volcanic | volcanic_tileset.png | Obsidian, lava pools, charred trees, ruined altar |

All biome maps are connected via portals in the forest (corners of the map).

## Build

```bash
# Prerequisites: Vulkan SDK, CMake 3.20+, C++20 compiler

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

## Controls

### Desktop

| Input         | Action                                          |
|---------------|-------------------------------------------------|
| WASD / Arrows | Move                                            |
| Shift         | Run                                             |
| Z / Enter     | Talk / Confirm / Buy                            |
| X / Backspace | Open Inventory / Cancel / Close Shop            |
| Left/Right    | Browse inventory items                          |
| Z             | Use selected item                               |
| Tab           | Toggle Editor                                   |
| F1            | Debug Overlay (FPS, particles, NPCs, tweens)    |
| F2            | NPC Spawner                                     |
| F3            | Script IDE                                      |
| F4            | Debug Console                                   |
| F5            | Game Systems Panel (editor only)                |
| F6            | UI/HUD Editor (editor only)                     |
| N             | Reflection tool (editor only)                   |
| M             | Toggle reflection overlay (editor only)         |
| Q / E         | Cycle asset tabs (editor only)                  |
| ESC           | Pause Menu (in game) / Exit Editor (in editor)  |

### Gamepad (Desktop — any standard gamepad)

| Input            | Action                                |
|------------------|---------------------------------------|
| Left stick       | Move (with deadzone)                  |
| D-pad            | Move                                  |
| A                | Confirm                               |
| B                | Cancel                                |
| Start            | Pause Menu                            |
| RB               | Run                                   |

### Android

| Input            | Action                                |
|------------------|---------------------------------------|
| Left stick       | Move                                  |
| A button         | Talk / Confirm / Buy                  |
| B button         | Open Inventory / Cancel / Close       |
| Tap              | Left click (menus, shop, inventory)   |
| Menu button      | Toggle Editor                         |
| Back button      | Quit                                  |

### Meta Quest

| Input            | Action                                |
|------------------|---------------------------------------|
| Left stick       | Move                                  |
| A / X button     | Confirm                               |
| B / Y button     | Cancel                                |
| Right trigger    | Run                                   |
| Start            | Pause                                 |

## Project Structure

```text
src/
  engine/                    # Standalone engine (graphics, audio, scripting, debug, platform)
    scripting/
      script_engine.cpp      #   Core + original API modules (2,841 lines)
      script_api_new.cpp     #   Phase 1-4 APIs: tween, particle, save, parallax, etc. (717 lines)
  game/                      # Generic RPG framework — modularized across 5 files:
    game.cpp                 #   Core update loop + settings menu (1,000 lines)
    game_io.cpp              #   Map/dialogue file I/O, JSON parser (537 lines)
    game_init.cpp            #   Game init, tileset setup, NPC setup (891 lines)
    game_battle.cpp          #   Battle logic + battle rendering (516 lines)
    game_render.cpp          #   World rendering, parallax, HUD, UI overlay, 9-slice, sync (1,475 lines)
    ai/                      # A* pathfinding
    systems/                 # Day-night cycle, survival stats, spawn system
    ui/                      # Game UI systems (merchant store)
  editor/                    # Tile editor — modularized across 6 files:
    tile_editor.cpp          #   Core: update, shortcuts, tools, undo/redo, clipboard, menu bar, assets
    tile_editor_render.cpp   #   World rendering: grid, collision overlay, cursor, panels (in core)
    tile_editor_npc_spawner.cpp  # NPC Spawner panel (F2)
    tile_editor_script_ide.cpp   # Script IDE with syntax highlighting (F3)
    tile_editor_debug.cpp    #   Debug Console panel (F4)
    tile_editor_systems.cpp  #   Game Systems panel: tweens, particles, lighting, quests, etc. (F5)
    tile_editor_ui.cpp       #   UI/HUD/Window Editor panel, 18 templates, 9-slice (F6) (1,356 lines)
  third_party/               # miniaudio, stb_image, stb_truetype, imgui, sagelang
games/
  demo/                      # "Crystal Quest" FF-style demo
    game.json                # Game manifest (player, party, NPCs, scripts, audio)
    assets/
      maps/                  # Map JSON files (forest, desert, snow, cave, volcanic, house_inside)
      scripts/               # SageLang game logic (.sage)
        maps/                # Per-map init/enter scripts
        battle/              # Battle scripts
        inventory/           # Item usage scripts
      dialogue/              # NPC dialogue files
      textures/              # Sprite sheets, tilesets (cf_tileset + 4 procedural biome tilesets)
      fonts/                 # TTF fonts
      audio/                 # Music and sound effects
tools/
  generate_tileset.py        # Procedural tileset generator (10 biomes)
  wire_biome_levels.py       # Generate + wire biome maps into a game
  tw_test/                   # Automated test tool (screenshot, input injection, smoke tests)
  scale_assets.py            # Multi-resolution asset scaler
  extract_tileset.py         # Tileset background removal
  fuzz_engine.py             # Security fuzzer (7 categories, boundary/type/injection/exhaustion)
  generate_ui_pack.py        # Procedural UI sprite sheet generator (4 themes, 47 components)
docs/                        # Engine documentation
shaders/                     # GLSL vertex/fragment shaders (compiled to SPIR-V)
android/                     # Android build (Gradle, manifest, native glue)
```

## Tools

| Tool | Description |
|------|-------------|
| `tools/generate_tileset.py` | Procedural pixel-art tileset generator for 10 biome types (grasslands, forest, desert, snow, swamp, volcanic, beach, cave, urban, farmland). Outputs engine-compatible PNG + stamps.txt |
| `tools/wire_biome_levels.py` | Generates tilesets + maps + scripts and wires them into the demo with portals |
| `tools/tw_test/` | Automated test tool: launches game, sends keyboard input via XTest, captures screenshots via X11, runs smoke tests |
| `tools/scale_assets.py` | Multi-resolution asset scaler (2x, 3x, 4x) with nearest-neighbor for pixel art |
| `tools/extract_tileset.py` | Removes background color from tileset PNGs |
| `tools/fuzz_engine.py` | Security fuzzer: boundary values, type confusion, resource exhaustion, division edge cases, string injection. 7 fuzz categories |
| `tools/generate_ui_pack.py` | Procedural UI/HUD sprite sheet generator. 4 themes (fantasy, dark, medieval, cute), 47 components each: panels, buttons, bars, checkboxes, sliders, 9-slice tiles, arrows |
| `tools/generate_parallax_bg.py` | Procedural parallax background generator. 6 biomes (forest, cave, night, sunset, snow, desert), 5 layers each, horizontally tileable, pixel-art style |

## Documentation

| Document | Description |
|----------|-------------|
| [Engine Guide](docs/Twilight_Engine_Guide.md) | Comprehensive engine guide with all systems |
| [Scripting API](docs/SCRIPTING.md) | Full SageLang API reference (323 functions, 56 modules) |
| [Architecture](docs/ARCHITECTURE.md) | Engine architecture and module breakdown |
| [Map Design Guide](docs/MAP_DESIGN_GUIDE.md) | Map creation with biome portals and scripting |
| [Tile Reference](docs/TILE_REFERENCE.md) | Tileset format, procedural generator, stamp system |
| [Battle System](docs/Battle_System_Comparison.md) | Battle system design comparison |
| [Updates](docs/UPDATES.md) | Version changelog with feature details |

## Tech Stack

- C++20, Vulkan, GLFW, GLM, stb_image, stb_truetype
- Dear ImGui (editor UI, desktop only)
- miniaudio (audio)
- SageLang (scripting — 323 API functions, 56 modules, multi-grid atlas cache, tracks latest main branch)
- tinyfiledialogs (native file dialogs, desktop only)
- Python 3 + Pillow + numpy (tooling: tileset generator, test automation, asset pipeline)

## License

MIT

---

## Stats

```text
                    ┌─────────────────────────────────────┐
                    │     TWILIGHT ENGINE v3.2.0           │
                    └─────────────────────────────────────┘

  C++ Source Code          28,038 lines across 109 files (excl. third-party)
  Total with Third-Party   ~248,000 lines across 230 files

  ┌─ Game Framework (7 files) ──────────────────────────────────────┐
  │  game_render.cpp  1,552 lines   World, parallax, HUD, UI, 9s  │
  │  game.cpp         1,087 lines   Core update + phase 4 systems │
  │  game.h             950 lines   All data structs + GameState   │
  │  game_init.cpp      893 lines   Init, tilesets, NPCs           │
  │  game_io.cpp        572 lines   Map I/O + auto-reflect         │
  │  game_platformer.cpp 537 lines  Platformer physics + enemies   │
  │  game_battle.cpp    516 lines   Turn-based battle system       │
  │  Total            6,107 lines                                  │
  └────────────────────────────────────────────────────────────────┘

  ┌─ Script Engine (9 files) ────────────────────────────────────────┐
  │  script_api_map.cpp  1,034 lines  Map, camera, weather, levels  │
  │  script_api_new.cpp    814 lines  Tween, particle, parallax     │
  │  script_api_systems.cpp 668 lines FSM, checkpoint, trail, etc.  │
  │  script_engine.cpp     655 lines  Core + battle + inventory     │
  │  script_api_ui.cpp     534 lines  UI, HUD, effects, renderer   │
  │  script_api_npc.cpp    452 lines  NPC runtime, routes, spawn    │
  │  script_api_platformer 397 lines  Platformer physics + enemies  │
  │  script_api_physics.cpp 378 lines Collision, raycast, triggers  │
  │  script_api_player.cpp 345 lines  Player, skills, input         │
  │  Total               5,277 lines  323 API functions, 56 modules │
  └─────────────────────────────────────────────────────────────────┘

  ┌─ Editor (11 files) ────────────────────────────────────────────┐
  │  tile_editor.cpp          2,157 lines  Core, tools, new map   │
  │  tile_editor_ui.cpp       1,356 lines  UI/HUD editor, 18 tmpls│
  │  tile_editor_script_ide.cpp 410 lines  Script IDE + highlight  │
  │  tile_editor.h              377 lines  Editor class definition │
  │  tile_editor_systems.cpp    319 lines  Systems + parallax mgr  │
  │  imgui_integration.cpp/h    193 lines  Vulkan ImGui bridge     │
  │  tile_editor_npc_spawner.cpp 147 lines NPC spawner             │
  │  tile_editor_debug.cpp       83 lines  Debug console           │
  │  particle_editor.h           29 lines  Particle editor stub    │
  │  Total                    5,071 lines  11 ImGui panels         │
  └────────────────────────────────────────────────────────────────┘

  ┌─ New Systems (22 headers, 2,725 lines) ────────────────────────┐
  │  Physics: collision.h, raycast.h           (AABB/circle/SAT)  │
  │  Core: noise.h, post_process.h, ecs.h      (Perlin, ECS)     │
  │  Systems: trigger_zone, state_machine, object_pool, checkpoint│
  │    combo_detector, input_replay, trail_renderer, rule_tiles   │
  │    dungeon_gen, skeleton_anim, coroutine                      │
  │  AI: behavior_tree.h                       (BT composites)   │
  │  Overworld: iso_utils.h, hex_utils.h       (Iso/hex grids)   │
  │  Net: net_common.h  Scripting: mod_loader.h                   │
  └────────────────────────────────────────────────────────────────┘

  ┌─ Engine Subsystems ────────────────────────────────────────────┐
  │  Graphics (renderer, batch, atlas, pipeline, text)  2,493 lines│
  │  Platform (desktop, android, quest, input, touch)   1,151 lines│
  │  Systems (tween, particles, save, spawn, etc.)        965 lines│
  │  Overworld (camera, tile_map + reflection grid)       880 lines│
  │  UI (merchant store + atlas regions)                  673 lines│
  │  Resource (file I/O, manifest, textures)              581 lines│
  │  Audio (miniaudio, spatial, crossfade, buses)         258 lines│
  │  Dialogue (typewriter, portraits, choices)            277 lines│
  │  AI (A* pathfinding + behavior trees)                 119 lines│
  │  Core (engine loop, timer, types, debug log)          274 lines│
  └────────────────────────────────────────────────────────────────┘

  ┌─ Content ──────────────────────────────────────────────────────┐
  │  Maps              6   Forest, House, Desert, Snow, Cave, Lava │
  │  Tilesets         16   cf_tileset (1,080) + biome + legacy     │
  │  Object Stamps    88   Trees, buildings, props, biome-specific │
  │  Fantasy Icons   432   16x27 grid at 32x32                    │
  │  Parallax BGs     45   8 biome presets (6 procedural + 2 CC0) │
  │  UI Regions      113   57 (main) + 56 (flat/theme packs)      │
  │  UI Themes         4   Fantasy, Dark, Medieval, Cute (47 each)│
  │  UI Templates     18   Dialogs, HUD, stats, menus, tooltips   │
  │  Sage Scripts     20   Game logic, weather, tests, map scripts │
  │  Biome Presets    10   Grasslands → Farmland                   │
  │  Python Tools     13   + tw_test package (11 modules)          │
  │  Fuzz Categories   7   boundary, division, string, type, etc.  │
  └────────────────────────────────────────────────────────────────┘

  ┌─ Engine Features ──────────────────────────────────────────────┐
  │  Easing Types      19   Linear, Sine, Quad, Cubic, Back, etc.  │
  │  Particle Presets   9   fire, smoke, sparkle, blood, dust, etc.│
  │  Collision Shapes   3   AABB, Circle, Convex Polygon (SAT)    │
  │  Tile Properties    2   Collision grid + Reflection grid       │
  │  Tile Grid Types    3   Orthogonal + Isometric + Hex           │
  │  Screen Transitions 5   Fade, Iris, Wipe, Pixelate, Slide     │
  │  Panel Rendering    2   Stretch + 9-Slice (configurable border)│
  │  Dungeon Generators 2   BSP Tree + Cellular Automata           │
  │  Parallax Biomes    8   forest, cave, night, sunset, snow, etc.│
  │  Platforms          4   Linux, Windows, Android, Meta Quest    │
  │  Input Modes        4   Keyboard, Gamepad, Touch, Quest Ctrl   │
  └────────────────────────────────────────────────────────────────┘
```
