# Twilight Engine

A cross-platform Vulkan 2D RPG engine built in C++20, designed for creating EarthBound-style pixel art games. Ships with an integrated tile editor, SageLang scripting, and a data-driven game manifest system.

## Features

- **Vulkan Renderer** — Sprite batching, Y-sorted rendering, texture atlases, animated tiles
- **Cross-Platform** — Linux, Windows (cross-compile), Android (landscape, touch controls)
- **Engine / Game Separation** — Games live in `games/<name>/` with a `game.json` manifest; engine is standalone
- **Tile Map System** — Multi-layer maps, collision, portals, animated water/grass overlays
- **Battle System** — Turn-based combat with rolling HP, party members, attack animations
- **Inventory System** — SageLang-driven items with battle submenu, elemental weaknesses, stacking
- **H.U.N.T.E.R. Skills** — Fallout S.P.E.C.I.A.L.-style character stats (Hardiness, Unholiness, Nerve, Tactics, Exorcism, Riflery)
- **Audio System** — miniaudio-powered BGM with crossfade, SFX, per-platform backends
- **Dialogue System** — Typewriter text, character portraits, branching conversations
- **NPC AI** — Idle wandering, hostile aggro/chase, auto-trigger encounters
- **Tile Editor** — Dear ImGui editor with brush sizes, line/rect tools, minimap, asset import, undo/redo
- **SageLang Scripting** — Modular scripting for battle, inventory, dialogue, and events
- **Party System** — EarthBound-style follower trail with smooth interpolation

## Games

Games are separate from the engine and live in `games/`. The engine ships with a built-in demo and supports external game submodules.

| Game | Description |
|------|-------------|
| **demo** | Engine tech demo with village tileset, RPG Maker MZ assets, EarthBound sprites |
| **supernatural** | Supernatural TV show fan RPG (git submodule: [Twilight_Engine_Games](games/Twilight_Engine_Games)) |

## Build

```bash
# Prerequisites: Vulkan SDK, CMake 3.20+, C++20 compiler

# Build a game against the engine
./twilight-build.sh supernatural linux Release
./twilight-build.sh demo linux
./twilight-build.sh supernatural win64
./twilight-build.sh supernatural android
./twilight-build.sh supernatural all

# List available games
./twilight-build.sh nonexistent linux

# Engine-only build (legacy)
./build.sh linux
```

## Controls

| Input | Action |
|-------|--------|
| WASD / Arrows | Move |
| Shift | Run |
| Z / Enter | Talk / Confirm |
| Tab | Toggle Editor |
| ESC | Quit |

## Project Structure

```
src/
  engine/                    # Standalone engine (graphics, audio, scripting, platform)
  game/                      # Generic RPG framework (battle, inventory, skills, dialogue)
  editor/                    # Tile editor (ImGui UI, tools, minimap, asset import)
  third_party/               # miniaudio, stb_image, stb_truetype, imgui, sagelang
games/
  demo/                      # Built-in engine demo
    game.json
    assets/
  Twilight_Engine_Games/     # External games (git submodule)
    supernatural/
      game.json
      assets/
assets/
  textures/                  # Sample sprite sheets (earthbound, village, rpgmaker, sprite)
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
