# Twilight Engine

A cross-platform Vulkan 2D RPG engine built in C++20, designed for creating pixel art games. Ships with an integrated tile editor, SageLang scripting, a data-driven game manifest, and a built-in debug console.

## Features

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles, fullscreen
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls, editor overlay)
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision, portals, animated water/grass overlays
- **Battle System** — Turn-based combat with rolling HP, party members, attack animations
- **Inventory & Shop System** — SageLang-driven items with battle submenu, elemental weaknesses, stacking; merchant store UI with buy/sell and scalable pixel art panels
- **Character Stats** — Fallout S.P.E.C.I.A.L.-style stats: Vitality, Arcana, Agility, Tactics, Spirit, Strength
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends
- **Dialogue System** — SageLang-driven dialogue via `say()`, typewriter text, character portraits
- **NPC AI** — Idle wandering, hostile aggro/chase, auto-trigger encounters
- **SageLang Scripting** — All dialogue, battle, inventory, shop, and events driven by `.sage` scripts with hot reload
- **Party System** — EarthBound-style follower trail with smooth interpolation

### Editor

#### Desktop (Tab to toggle)

- **Menu Bar** — File (Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Line, Rectangle, Portal
- **Brush Sizes** — 1x1, 2x2, 3x3
- **Assets Panel** — Tabbed tileset browser (Tiles, Buildings, Trees, Misc) with image previews
- **Minimap** — Color-coded map overview with player/NPC markers, click to teleport
- **NPC Spawner** (F2) — Spawn NPCs with presets (animals, enemies, villagers), click-to-place on map
- **Script IDE** (F3) — Built-in SageLang editor with syntax highlighting, asset click-to-highlight, menu bar (File/Help), and integrated API manual
- **Debug Console** (F4) — Color-coded log stream, filter by level, live SageLang command input

#### Android (EDIT button overlay)

- Tap **EDIT/PLAY** button (top-right) to toggle editor mode
- Touch-friendly toolbar with Paint, Erase, Fill, and Collision tools
- Camera info bar with position and map dimensions
- Touch controls hidden in editor mode to prevent interference

### Script IDE

- **File Menu** — New Script, Open Script, Save, Save & Reload, Reload All
- **Help Menu** — SageLang API Manual (8 tabbed sections: Overview, Dialogue, Inventory, Shop, Battle, Stats, Debug, Utilities)
- **Syntax Highlighting** — Keywords (purple), strings (gold), numbers (cyan), built-in functions (teal), booleans (orange), comments (green)
- **Asset Highlighting** — Click any string literal in code to highlight matching NPCs/items in the game world for 5 seconds
- **View/Edit Toggle** — Syntax-highlighted read-only view by default; click "Edit" for full text editing

### Debug API (SageLang)

```sage
debug("variable value: " + str(x))     # Grey - debug level
info("system initialized")             # Green - info level
warn("low HP")                          # Yellow - warning
error("something broke")               # Red - error
print("damage:", dmg, "to", target)     # Cyan - multi-arg script output
assert_true(hp > 0, "HP went negative") # Red if assertion fails
```

### Shop API (SageLang)

```sage
# Define items and open a merchant store UI
add_shop_item("potion", "Potion", 25, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic", 0, 30, "fire", "use_fire")
open_shop("Merchant")

# Gold management
set_gold(500)
let g = get_gold()
```

## Games

| Game     | Description |
|----------|-------------|
| **demo** | "Crystal Quest" — FF-style demo with pixel art tileset, Mage/Black Mage party, merchant shop |

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

# Build all platforms
./build.sh all

# Clean
./build.sh clean
```

## Controls

### Desktop

| Input         | Action                    |
|---------------|---------------------------|
| WASD / Arrows | Move                      |
| Shift         | Run                       |
| Z / Enter     | Talk / Confirm / Buy      |
| X / Backspace | Cancel / Close Shop       |
| Tab           | Toggle Editor             |
| F2            | NPC Spawner               |
| F3            | Script IDE                |
| F4            | Debug Console             |
| ESC           | Quit                      |

### Android

| Input            | Action               |
|------------------|-----------------------|
| Left stick       | Move                 |
| A button         | Talk / Confirm / Buy |
| B button         | Cancel / Close       |
| EDIT button      | Toggle Editor        |

## Project Structure

```text
src/
  engine/                    # Standalone engine (graphics, audio, scripting, debug, platform)
  game/                      # Generic RPG framework (battle, inventory, shop, stats, dialogue)
    ui/                      # Game UI systems (merchant store)
  editor/                    # Tile editor (ImGui, menu bar, tools, NPC spawner, script IDE, debug)
  third_party/               # miniaudio, stb_image, stb_truetype, imgui, sagelang
games/
  demo/                      # "Crystal Quest" FF-style demo
    game.json                # Game manifest (player, party, NPCs, scripts, audio)
    assets/
      scripts/               # SageLang game logic (.sage)
      dialogue/              # NPC dialogue files
      textures/              # Sprite sheets, tilesets, UI elements
      fonts/                 # TTF fonts
      audio/                 # Music and sound effects
assets/
  textures/                  # Sample sprite sheets (source art, not used at runtime)
shaders/                     # GLSL vertex/fragment shaders (compiled to SPIR-V)
android/                     # Android build (Gradle, manifest, native glue)
```

## Tech Stack

- C++20, Vulkan, GLFW, GLM, stb_image, stb_truetype
- Dear ImGui (editor UI, desktop only)
- miniaudio (audio)
- SageLang (scripting)
- tinyfiledialogs (native file dialogs, desktop only)

## License

MIT
