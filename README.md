# Twilight Engine

A cross-platform Vulkan 2D RPG engine built in C++20, designed for creating pixel art games. Ships with a tween engine, particle system, save/load, quests, equipment, 2D lighting, screen transitions, gamepad support, an integrated tile editor, SageLang scripting (229 API functions across 39 modules), and procedural tileset generation for 10 biome types.

## Features

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles, fullscreen, per-sprite tint/alpha
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls, editor overlay), Meta Quest (flat 2D mode)
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision, portals, animated water/grass overlays, per-tile rotation (0°/90°/180°/270°) and flip
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
- **Input Buffering** — 150ms input buffer for lenient timing on confirmations and interactions
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
- **Script-Driven UI** — Labels, bars, panels, images with per-component opacity, layer, rotation, flip, scale, `on_click` callbacks, and `ui_get()`/`ui_set()` for reading/writing any property
- **Script-Driven Pause Menu** — 6-item pause menu (Resume, Editor, Levels, Reset, Settings, Quit) with built-in level selector sub-menu
- **Script-Driven HUD** — All HUD layout defined in `default.sage`. C++ auto-syncs values each frame (HP bar auto-colors, sun/moon icon auto-swaps)
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends; path-sanitized file operations
- **Dialogue System** — SageLang-driven dialogue via `say()`, typewriter text, character portraits
- **Weather System** — Rain, snow, lightning, procedural cloud shadows, god rays, fog, wind. All script-controllable with presets (`set_weather("storm")`) and per-parameter control. Dynamic time-based weather changes
- **Multi-Grid Sprites** — Per-NPC sprite grid dimensions (16x16, 32x32, 32x48, 64x64, etc.). Same texture can be loaded with different grids. No conversion needed — just specify the grid size
- **Procedural Tileset Generator** — `tools/generate_tileset.py` creates complete pixel-art tilesets for 10 biome types with noise-based terrain, autotile transitions, decorations, water animations, and object stamps
- **Sprite Animation** — Multi-frame animation player per NPC with define/play/stop, loop and one-shot modes
- **Dialogue History** — Track conversations, remember who player has talked to, branch on past choices
- **Parallax Backgrounds** — Multi-layer scrolling with configurable scroll speed per layer
- **Spatial Audio** — Distance-based SFX volume falloff from camera center
- **Settings System** — Master/music/SFX volume, text speed, fullscreen; ready for UI menu
- **Debug Overlay** — F1 toggle shows FPS, particle count, NPC count, tween count
- **SageLang Scripting** — 229 API functions across 39 modules driving all game systems with hot reload. See [docs/SCRIPTING.md](docs/SCRIPTING.md) for the full API reference
- **Asset Pipeline** — Multi-resolution asset generator; procedural tileset generator (10 biomes); auto-discovery of biome stamps; 1,080 base tiles, 88+ stamps, 432 fantasy icons, 3 UI spritesheets
- **Test Automation Tool** — `tools/tw_test/` Python package for automated game testing via XTest keyboard injection and X11 screenshot capture
- **String-Keyed Atlas Cache** — Shared texture atlas cache keyed by path+grid-size; runtime sprite loading from scripts
- **Test Suite** — `--test` CLI flag runs 101 assertions across 33 test categories; also callable from the F4 debug console via `run_all_tests()`
- **Security Hardened** — Path traversal protection, input clamping, file size validation, descriptor pool exhaustion protection, Vulkan resource cleanup, thread-safe Android platform, division-by-zero guards, bounds-checked array access, fuzzer-verified API safety
- **Map Scripting** — Visual Basic-style editor: every editor action auto-generates SageLang in a companion map script
- **Party System** — EarthBound-style follower trail with smooth interpolation

### Editor

#### Desktop (Tab to toggle)

- **Menu Bar** — File (Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Line, Rectangle, Portal
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
- **Auto-Tile Config** — Configure terrain pairs and transition tile ranges for auto-placement
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
  game/                      # Generic RPG framework (battle, inventory, shop, stats, dialogue)
    ai/                      # A* pathfinding
    systems/                 # Day-night cycle, survival stats, spawn system
    ui/                      # Game UI systems (merchant store)
  editor/                    # Tile editor (ImGui, menu bar, tools, NPC spawner, script IDE, debug)
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

## Documentation

| Document | Description |
|----------|-------------|
| [Engine Guide](docs/Twilight_Engine_Guide.md) | Comprehensive engine guide with all systems |
| [Scripting API](docs/SCRIPTING.md) | Full SageLang API reference (185 functions, 28 modules) |
| [Architecture](docs/ARCHITECTURE.md) | Engine architecture and module breakdown |
| [Map Design Guide](docs/MAP_DESIGN_GUIDE.md) | Map creation with biome portals and scripting |
| [Tile Reference](docs/TILE_REFERENCE.md) | Tileset format, procedural generator, stamp system |
| [Battle System](docs/Battle_System_Comparison.md) | Battle system design comparison |

## Tech Stack

- C++20, Vulkan, GLFW, GLM, stb_image, stb_truetype
- Dear ImGui (editor UI, desktop only)
- miniaudio (audio)
- SageLang (scripting — 229 API functions, 39 modules, multi-grid atlas cache)
- tinyfiledialogs (native file dialogs, desktop only)
- Python 3 + Pillow + numpy (tooling: tileset generator, test automation, asset pipeline)

## License

MIT

---

Twilight Engine v2.0.0 — 19,787 lines C++, 229 API functions, 39 script modules, 6 maps, 8 tilesets, 10 biome presets, 88 stamps, 432 icons, 19 easing types, 9 particle presets, 502 test assertions, 7 fuzz categories, 4 platforms
