# Twilight Engine

A cross-platform Vulkan 2D RPG engine built in C++20, designed for creating pixel art games. Ships with an integrated tile editor, SageLang scripting, a data-driven game manifest, and a built-in debug console.

## Features

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles, fullscreen
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls with native-to-virtual coordinate mapping, editor overlay), Meta Quest (flat 2D mode)
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision, portals, animated water/grass overlays
- **Level System** — Multi-level manager with load/cache/switch, portal auto-transitions, background ticking (NPC schedules, spawn timers), per-level map scripts
- **Battle System** — Turn-based combat with rolling HP, party members, attack animations
- **Inventory & Shop System** — SageLang-driven items with battle submenu, elemental weaknesses, stacking; merchant store UI with buy/sell and scalable pixel art panels
- **Character Stats** — Fallout S.P.E.C.I.A.L.-style stats: Vitality, Arcana, Agility, Tactics, Spirit, Strength
- **Day-Night Cycle** — Real-time clock with visual tinting (dawn/day/sunset/dusk/night), configurable speed
- **NPC AI** — A* pathfinding, waypoint patrol routes, idle wandering with collision, hostile aggro/chase
- **NPC Schedules** — Time-based NPC visibility tied to the day-night cycle (e.g. merchant appears 8AM-6PM)
- **NPC Interactions** — Proximity-triggered callbacks between NPCs, face-each-other, meet events
- **Spawn System** — Periodic NPC spawning with configurable intervals, max counts, spawn areas, and callbacks via `set_spawn_callback()` for auto-configuring spawned NPCs
- **Survival System** — Hunger, thirst, energy meters with configurable depletion rates and gameplay effects
- **Script-Driven UI** — Labels, progress bars, panels, images, and timed notifications created from SageLang scripts; any component property modifiable via `ui_set()`; `reload_all()` clears all script UI before re-executing
- **Script-Driven Pause Menu** — Pause menu layout defined inline in `default.sage`; C++ handles dim overlay, show/hide on ESC, selection highlight, cursor movement, and mouse click support
- **Script-Driven HUD** — Built-in C++ HUD panels OFF by default; all HUD layout defined inline in `default.sage`. C++ auto-syncs values each frame by well-known component IDs (HP bar auto-colors green/yellow/red, sun/moon icon auto-swaps by time)
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends; 9 scripting API functions (`play_music`, `stop_music`, `crossfade_music`, `play_sfx`, etc.) plus an event library (`lib/audio.sage`) for context-based music management
- **Dialogue System** — SageLang-driven dialogue via `say()`, typewriter text, character portraits
- **SageLang Scripting** — 130+ API functions across 25+ modules driving all game systems with hot reload; all engine-loaded scripts share a global environment (functions callable across files without imports). Includes Player, Camera, Platform, NPC Runtime, Screen Effects, Tile Map Query, Input, Dialogue, Battle, and Renderer APIs
- **Test Suite** — `--test` CLI flag runs 90+ assertions across 24 modules (Engine Core, Flags, Inventory, Gold, Stats, Day-Night, Survival, UI, HUD, NPC, Spawn, Audio, Map, Player, Camera, Platform, NPC Runtime, Screen Effects, Tile Map, Input, Dialogue, Battle, Renderer, Level API); also callable from the F4 debug console via `run_all_tests()`
- **Map Scripting** — Visual Basic-style editor: every editor action (spawn NPC, place object, set portal) auto-generates SageLang in a companion map script
- **Party System** — EarthBound-style follower trail with smooth interpolation

### Editor

#### Desktop (Tab to toggle)

- **Menu Bar** — File (Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Line, Rectangle, Portal
- **Brush Sizes** — 1x1, 2x2, 3x3
- **Assets Panel** — Tabbed tileset browser (Tiles, Buildings, Trees, Misc) with image previews
- **Minimap** — Color-coded map overview with player/NPC markers, click to teleport
- **NPC Spawner** (F2) — Spawn NPCs with presets (animals, enemies, villagers), click-to-place on map
- **Script IDE** (F3) — Built-in SageLang editor with syntax highlighting, asset click-to-highlight, menu bar (File/Help), integrated API manual, and a **Map Script** panel (gold "MAP SCRIPT" header) for editing the current map's companion script
- **Debug Console** (F4) — Color-coded log stream, filter by level, live SageLang command input
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
| **demo** | "Crystal Quest" — FF-style demo with pixel art tileset, Mage/Black Mage party, merchant shop, day-night cycle |

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
| F2            | NPC Spawner                                     |
| F3            | Script IDE                                      |
| F4            | Debug Console                                   |
| ESC           | Pause Menu (in game) / Exit Editor (in editor)  |

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
      scripts/               # SageLang game logic (.sage)
        lib/                 # Reusable script libraries (imported via `import`)
        maps/                # Auto-generated map scripts (companion to .json maps)
        battle/              # Battle scripts
        inventory/           # Item usage scripts
      dialogue/              # NPC dialogue files
      textures/              # Sprite sheets, tilesets, UI elements
      fonts/                 # TTF fonts
      audio/                 # Music and sound effects
shaders/                     # GLSL vertex/fragment shaders (compiled to SPIR-V)
android/                     # Android build (Gradle, manifest, native glue)
```

## Tech Stack

- C++20, Vulkan, GLFW, GLM, stb_image, stb_truetype
- Dear ImGui (editor UI, desktop only)
- miniaudio (audio)
- SageLang (scripting — 130+ API functions)
- tinyfiledialogs (native file dialogs, desktop only)

## License

MIT

---

*Twilight Engine v1.1.0*
