# Twilight Engine

A cross-platform Vulkan 2D RPG engine built in C++20, designed for creating pixel art games. Ships with an integrated tile editor, SageLang scripting, a data-driven game manifest, and a built-in debug console.

## Features

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls)
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision, portals, animated water/grass overlays
- **Battle System** — Turn-based combat with rolling HP, party members, attack animations
- **Inventory System** — SageLang-driven items with battle submenu, elemental weaknesses, stacking
- **H.U.N.T.E.R. Skills** — Fallout S.P.E.C.I.A.L.-style character stats (Strength, Vitality, Magic, Spirit, Speed, Luck)
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends
- **Dialogue System** — SageLang-driven dialogue via `say()`, typewriter text, character portraits
- **NPC AI** — Idle wandering, hostile aggro/chase, auto-trigger encounters
- **SageLang Scripting** — All dialogue, battle, inventory, and events driven by `.sage` scripts with hot reload
- **Party System** — EarthBound-style follower trail with smooth interpolation

### Editor (Tab to toggle)

- **Menu Bar** — File (Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Line, Rectangle, Portal
- **Brush Sizes** — 1x1, 2x2, 3x3
- **Assets Panel** — Tabbed tileset browser (Tiles, Buildings, Trees, Misc) with image previews
- **Minimap** — Color-coded map overview with player/NPC markers, click to teleport
- **NPC Spawner** (F2) — Spawn NPCs with presets (animals, enemies, villagers), click-to-place on map
- **Script IDE** (F3) — Built-in code editor for `.sage` files with Save & Hot Reload
- **Debug Console** (F4) — Color-coded log stream, filter by level, live SageLang command input

### Debug API (SageLang)

```sage
debug("variable value: " + str(x))     # Grey - debug level
info("system initialized")             # Green - info level
warn("low HP")                          # Yellow - warning
error("something broke")               # Red - error
print("damage:", dmg, "to", target)     # Cyan - multi-arg script output
assert_true(hp > 0, "HP went negative") # Red if assertion fails
```

## Games

| Game | Description |
|------|-------------|
| **demo** | "Crystal Quest" — FF-style demo with Cute Fantasy tileset, Warrior/Black Mage party |
| **supernatural** | Supernatural TV show fan RPG (git submodule: [Twilight_Engine_Games](games/Twilight_Engine_Games)) |

## Build

```bash
# Prerequisites: Vulkan SDK, CMake 3.20+, C++20 compiler

# Build a game against the engine
./twilight-build.sh demo linux Release
./twilight-build.sh supernatural linux Release
./twilight-build.sh supernatural win64
./twilight-build.sh supernatural android
./twilight-build.sh supernatural all

# List available games
./twilight-build.sh nonexistent linux
```

## Controls

| Input | Action |
|-------|--------|
| WASD / Arrows | Move |
| Shift | Run |
| Z / Enter | Talk / Confirm |
| Tab | Toggle Editor |
| F2 | NPC Spawner |
| F3 | Script IDE |
| F4 | Debug Console |
| ESC | Quit |

## Project Structure

```
src/
  engine/                    # Standalone engine (graphics, audio, scripting, debug, platform)
  game/                      # Generic RPG framework (battle, inventory, skills, dialogue)
  editor/                    # Tile editor (ImGui, menu bar, tools, NPC spawner, script IDE, debug)
  third_party/               # miniaudio, stb_image, stb_truetype, imgui, sagelang
games/
  demo/                      # "Crystal Quest" FF-style demo
    game.json
    assets/
  Twilight_Engine_Games/     # External games (git submodule)
    supernatural/
      game.json
      assets/
assets/
  textures/                  # Sample sprite sheets (Cute_Fantasy_Free, earthbound, village, rpgmaker)
  engine/                    # Engine defaults (fonts)
shaders/                     # GLSL vertex/fragment shaders
android/                     # Android build (Gradle, manifest, native glue)
```

## Tech Stack

- C++20, Vulkan, GLFW, GLM, stb_image, stb_truetype
- Dear ImGui (editor UI)
- miniaudio (audio)
- SageLang (scripting)
- tinyfiledialogs (native file dialogs)

![Language Stats](stats/leaderboard_by_lines.png)

## License

MIT
