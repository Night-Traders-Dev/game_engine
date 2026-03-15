# Twilight Engine Guide

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Engine / Game Separation](#engine--game-separation)
3. [Build System](#build-system)
4. [Engine Core](#engine-core)
5. [Rendering Pipeline](#rendering-pipeline)
6. [Tile Map System](#tile-map-system)
7. [Sprite & Animation System](#sprite--animation-system)
8. [Battle System](#battle-system)
9. [Inventory System](#inventory-system)
10. [Character Stats System](#character-stats-system)
11. [Audio System](#audio-system)
12. [SageLang Battle Scripting](#sagelang-battle-scripting)
13. [Dialogue System](#dialogue-system)
14. [NPC & AI System](#npc--ai-system)
15. [Scripting with SageLang](#scripting-with-sagelang)
16. [Tile Editor](#tile-editor)
17. [Map File Format](#map-file-format)
18. [Asset Pipeline](#asset-pipeline)
19. [Android Platform](#android-platform)
20. [Day-Night Cycle](#day-night-cycle)
21. [NPC Pathfinding & Routes](#npc-pathfinding--routes)
22. [NPC Schedules & Interactions](#npc-schedules--interactions)
23. [Spawn System](#spawn-system)
24. [Survival System](#survival-system)
25. [Script-Driven UI](#script-driven-ui)
26. [Shop System](#shop-system)
27. [SageLang API Reference](#sagelang-api-reference)
28. [Adding New Content](#adding-new-content)

---

## Architecture Overview

Twilight Engine is a cross-platform 2D RPG engine built on Vulkan. The codebase is organized into layers:

```
Engine (src/engine/, standalone reusable library)
  ├── Core:      Engine loop, timer, types (Vec2, Vec4, Mat4)
  ├── Graphics:  VulkanContext, Renderer, SpriteBatch, Texture, TextureAtlas, Pipeline
  ├── Platform:  PlatformDesktop (GLFW), PlatformAndroid (NativeActivity)
  ├── Resource:  ResourceManager, FileIO, GameManifest (data-driven game loading)
  ├── Audio:     AudioEngine (miniaudio — BGM, SFX, crossfade)
  └── Scripting: ScriptEngine (embedded SageLang interpreter)

Game Framework (src/game/, generic RPG systems)
  ├── GameState:   All game data (player, NPCs, battle, map, party, inventory, skills)
  ├── game.h/cpp:  Shared game logic (init, update, render, save/load)
  ├── TileMap:     Tile storage, collision, portals, animated tiles
  ├── Camera:      Viewport, follow, bounds, offset
  └── DialogueBox: Typewriter text, portraits, choices

Editor (src/editor/, desktop only)
  ├── TileEditor:       Paint, erase, fill, line, rect, brush sizes, minimap, asset import
  ├── ImGuiIntegration: Vulkan+GLFW ImGui backend
  └── Dear ImGui:       Professional windowed UI with tabbed asset panels

Games (games/<game_name>/, separate from engine)
  └── game.json + assets/   — Game manifest + all game-specific content
```

**Key Design Principles**:
- **Engine is standalone** — no game-specific code in `src/engine/`
- Games live in `games/<name>/` with a `game.json` manifest defining characters, NPCs, scripts, and assets
- `twilight-build.sh` compiles the engine and links a game's assets against it
- Game logic is shared between desktop and Android via `game.h`/`game.cpp`
- Platform-specific code is conditionally compiled (`#ifndef EB_ANDROID`)
- Battle logic is scriptable via SageLang with C++ fallback
- The editor is desktop-only; the game runs identically on all platforms

---

## Engine / Game Separation

The engine and game content are fully separated. The engine (`src/`) is a reusable library; games live in `games/<name>/` with their own asset trees and a `game.json` manifest.

### Directory Structure

```
game_engine/
  src/
    engine/                    # Standalone engine (graphics, audio, scripting, editor)
    game/                      # Generic RPG framework (GameState, TileMap, battle, inventory)
    editor/                    # Tile editor (desktop only)
    third_party/               # miniaudio, stb, imgui, sagelang, tinyfiledialogs
  shaders/                     # GLSL vertex/fragment shaders
  assets/
    engine/fonts/              # Engine default font
    textures/                  # Sample sprite sheets
      earthbound/              # EarthBound sample sprites (nes_sprites.png)
      village/                 # Village tileset (village_tileset_32.png)
      sprite/                  # RPG object sprites (chests, doors, campfire, icons)
      rpgmaker/MZ/             # RPG Maker MZ tilesets and characters
  games/
    demo/                      # Built-in engine demo (village + RPG Maker MZ + EarthBound assets)
      game.json
      assets/
    Twilight_Engine_Games/     # External game submodule
      supernatural/            # Separate game in external submodule
        game.json
        assets/
          textures/            # Character sprites, tilesets, portraits
          scripts/
            battle/            # Modular battle scripts (battle_core, player, ally, vampire, demon)
            inventory/         # Modular inventory scripts (core, player, ally, party, battle)
          dialogue/            # NPC dialogue files
          audio/               # Music tracks
          maps/                # Map JSON files
```

### Game Manifest (`game.json`)

Each game defines a JSON manifest that declares all game-specific data:

```json
{
  "game": { "title": "My RPG", "version": "1.0", "window_width": 960, "window_height": 720 },
  "player": {
    "name": "Hero", "sprite": "assets/textures/hero.png", "sprite_grid": [32, 48],
    "hp": 100, "atk": 15, "def": 5,
    "skills": { "vitality": 5, "agility": 5, "tactics": 5, "spirit": 5, "strength": 5, "arcana": 3 }
  },
  "party": [ { "name": "Ally", "sprite": "assets/textures/ally.png", ... } ],
  "tileset": "assets/textures/tileset.png",
  "npcs": [ { "name": "Shopkeeper", "x": 100, "y": 200, "sprite": "...", ... } ],
  "scripts": [ "assets/scripts/battle_core.sage", "assets/scripts/items.sage" ],
  "init_scripts": [ "give_starter_items" ],
  "audio": { "overworld": "assets/audio/overworld.wav", "battle": "assets/audio/battle.wav" }
}
```

The engine's `GameManifest` loader (`src/engine/resource/game_manifest.h`) parses this file and provides structured data for initialization.

### Twilight Build Script

```bash
./twilight-build.sh <game> <platform> [build_type]

# Examples:
./twilight-build.sh supernatural linux Release
./twilight-build.sh demo linux
./twilight-build.sh supernatural win64
./twilight-build.sh supernatural android
./twilight-build.sh supernatural all

# List available games:
./twilight-build.sh nonexistent linux
# Output: demo, supernatural
```

The build script automatically searches `games/` recursively, including git submodules (e.g., `games/Twilight_Engine_Games/supernatural/`).

Steps performed:
1. Compiles the Twilight Engine as a static library
2. Symlinks the game's `assets/` into the build directory
3. Copies `game.json` alongside the binary
4. Outputs a ready-to-run executable with all game content

### Creating a New Game

1. Create `games/my_game/game.json` with your manifest (or add as a git submodule)
2. Create `games/my_game/assets/` with textures, scripts, audio, maps
3. Run `./twilight-build.sh my_game linux`
4. The engine loads your game's assets and scripts automatically

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
# Game build (recommended — builds engine + links game assets)
./twilight-build.sh supernatural linux Release
./twilight-build.sh demo linux
./twilight-build.sh supernatural win64
./twilight-build.sh supernatural android
./twilight-build.sh supernatural all

# Engine-only build (legacy, no game assets)
./build.sh linux [Debug|Release]
./build.sh win64 [Debug|Release]
./build.sh android [Debug|Release]
```

The build script searches `games/` recursively, finding games inside git submodules automatically.

### Build Outputs

- `build-linux/twilight_game_binary` — Linux executable
- `build-win64/twilight_game_binary.exe` — Windows executable (statically linked, no DLL dependencies)
- `android/app/build/outputs/apk/debug/app-debug.apk` — Android APK

### CMake Targets

- `tw_engine` — Static library containing all engine + game code
- `twilight_game_binary` — Desktop executable (links tw_engine)
- `sagelang` — Static library of embedded SageLang interpreter

### Dependencies (auto-fetched via FetchContent)

- GLFW 3.4 (windowing)
- GLM 1.0.1 (math)
- Vulkan Headers (Windows cross-compile only)

### Vendored Third-Party

- stb_image.h — Image loading
- stb_truetype.h — Font rasterization
- Dear ImGui — Editor UI (Vulkan + GLFW backends)
- SageLang — Scripting language (git submodule at `src/third_party/sagelang`)
- tinyfiledialogs — Native file dialogs (Linux: zenity via popen, Windows: native)

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
- Pre-transform IDENTITY for landscape mode (avoids rotation issues)
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
- `sprite.frag` — Texture sampling with vertex color tinting (alpha blending)

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

### Tile Types (54 ground tiles)

| Category | Count | Examples |
|----------|-------|---------|
| Grass | 4 | Pure, light, flowers, dark |
| Dirt | 4 | Brown, dark, mud, gravel |
| Edges/Hedges | 4 | Grass edges, hedge variants |
| Dirt Paths | 6 | Path, mixed, stone path |
| Special Ground | 6 | Pentagram, dark ground, blood dirt, dark stone |
| Roads | 12 | Horizontal, vertical, cross, corners, sidewalk, asphalt |
| Road Extras | 5 | Dirt road, blood patches |
| Water/Shore | 9 | Deep, mid, shore L/R, sand, wet sand, shallow, blood water |
| Water Objects | 4 | Bench, rocks |

### Object Stamps (46 objects)

| Category | Count | Examples |
|----------|-------|---------|
| Buildings | 5 | Gas Mart, Salvage Repair, Tall Building, Motel, House |
| Vehicles | 3 | Impala, Blue Car, Impala (side) |
| Trees | 17 | Large trees, dead trees, gnarly trees, bushes, stumps, night variants, hedge row |
| Misc | 21 | Tombstones, statues, urns, lamp posts, walls, pentagram, campfire, barrels |

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
2. **Hero's Turn** — Attack / Items / Defend / Run menu
3. **Ally's Turn** — Same menu (skipped if Ally is down)
4. **Enemy Turn** — SageLang AI selects target and calculates damage
5. Repeat until victory or defeat

### Rolling HP

EarthBound-style HP odometer — display rolls down toward actual HP at 40 HP/sec, giving time to heal before hitting zero.

### Attack Animations

- **Player attack**: Character lunges forward (sine wave), walk frames cycle
- **Enemy hit**: Sprite blinks (alternating visibility)
- **Damage numbers**: Shown in battle message box

### Battle Rendering

- Enemy sprite displayed at top (facing player, 1.5x size)
- Hero and Ally displayed bottom-center (backs to camera)
- HP bars for both Hero and Ally with active fighter highlighted
- Battle menu shows current fighter's name

### SageLang Integration

The battle system calls SageLang functions at key action points:
- `attack_normal()` — player attacks
- `defend()` — player heals
- `enemy_turn()` — generic enemy AI
- `vampire_attack()` — vampire-specific AI (HP drain)
- `on_victory()` / `on_defeat()` — battle end handlers

If no script function exists, the engine falls back to built-in C++ logic.

---

## Inventory System

### Overview

The inventory system manages items that players collect and use during gameplay. Items are defined and used via SageLang scripts, with the C++ engine managing storage, the battle menu UI, and native bridge functions.

### Item Types

| Type | Description | Battle Use |
|------|-------------|------------|
| Consumable | Healing items (food, medical supplies) | Heals active fighter, consumed on use |
| Weapon | Ammo and throwable weapons | Deals damage to enemy, consumed on use (except special weapons) |
| Key Item | Story/utility items (EMF Meter, Journal) | Not usable in battle |

### Item Struct (C++)

```cpp
struct Item {
    std::string id;          // "first_aid_kit"
    std::string name;        // "First Aid Kit"
    std::string description;
    ItemType type;           // Consumable, Weapon, KeyItem
    int quantity;
    int max_stack;           // Default 99
    int heal_hp;             // HP restored on use
    int damage;              // Base damage dealt
    std::string element;     // "holy", "silver", "salt", "demon", "divine"
    std::string sage_func;   // SageLang function called on use
};
```

### Inventory Management (C++)

```cpp
struct Inventory {
    static constexpr int MAX_SLOTS = 20;
    bool add(id, name, count, type, desc, heal, dmg, element, sage_func);
    bool remove(id, count);
    int count(id);
    Item* find(id);
    std::vector<const Item*> get_battle_items(); // Excludes key items
};
```

### Built-in Items (Default Items)

**Starter Items** (given at game start via `give_starter_items()`):

| Item | Type | Effect |
|------|------|--------|
| First Aid Kit | Consumable | Heals 30-40 HP |
| Burger | Consumable | Heals 50-65 HP |
| Beer | Consumable | Heals 15-20 HP |
| Salt Rounds | Weapon | 20-28 dmg, x2 vs Spirits/Ghosts |

**Elder's Supplies** (given via `elder_supplies()`):

| Item | Type | Effect |
|------|------|--------|
| Shotgun Shells | Weapon | 28-38 dmg |
| Holy Water | Weapon | 35-50 dmg, x2 vs Vampires/Demons |
| Silver Bullets | Weapon | 40-52 dmg, x2 vs Werewolves/Shapeshifters |

**Special Weapons** (quest rewards, not consumed):

| Item | Type | Effect |
|------|------|--------|
| Angel Blade | Weapon | 60-80 dmg |
| Ruby's Knife | Weapon | 55-73 dmg, x2 vs Demons |
| The Colt | Weapon | 99-149 dmg |

### Battle Menu Integration

The battle menu has 4 options: **Attack / Items / Defend / Run**

Selecting **Items** opens a scrollable submenu showing usable items with quantities. Up to 6 items visible at once with scroll support. Each item displays as `> Item Name x3` with the selected item highlighted in yellow.

### SageLang Native Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `add_item` | `add_item(id, name, qty, type, desc, heal, dmg, elem, sage_func)` | Add item to inventory |
| `remove_item` | `remove_item(id, qty)` | Remove item (qty defaults to 1) |
| `has_item` | `has_item(id)` | Returns true if item exists |
| `item_count` | `item_count(id)` | Returns quantity of item |

### SageLang Item Use Flow

1. Player selects an item in the battle submenu
2. C++ syncs battle state AND item info to SageLang globals
3. C++ calls the item's `sage_func` (e.g., `use_holy_water`)
4. SageLang script applies the effect (damage/heal, elemental bonus)
5. SageLang calls `remove_item()` to consume the item
6. C++ syncs modified battle state back

### Item Script Example

```sage
proc use_holy_water():
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Ally"
    let damage = 35 + random(0, 15)
    # Super effective against demons and vampires
    if enemy_name == "Vampire" or enemy_name == "Demon":
        damage = damage * 2
        battle_msg = fighter_name + " throws Holy Water! It burns! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " throws Holy Water! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
    remove_item("holy_water", 1)
```

### Adding New Items

1. Define the item grant function in `inventory.sage` using `add_item()`
2. Write a `use_*` function for battle use (read battle globals, apply effect, set `battle_msg`)
3. Call the grant function from an NPC script, event, or game init
4. The item automatically appears in the battle Items submenu

---

## Character Stats System

### Overview

The Character Stats system is a Fallout S.P.E.C.I.A.L.-inspired character stat system for RPG combat. Each stat ranges from 1-10 and provides derived combat bonuses.

### Stats

| Stat | Full Name | Effect Per Point |
|------|-----------|-----------------|
| **V** | Vitality | +10 max HP, damage resistance |
| **A** | Arcana | +8% spell power |
| **Ag** | Agility | +3% crit chance, +2% dodge chance |
| **T** | Tactics | +2 defense bonus |
| **S** | Spirit | +10% magic damage multiplier |
| **St** | Strength | +2 weapon damage bonus |

### Default Character Builds

**Hero** — Fighter (V5 A3 Ag6 T4 S3 St6):

- High Agility (6): 18% crit, 12% dodge — lucky and instinctive
- High Strength (6): +12 weapon damage — skilled fighter
- Low Spirit (3): relies on weapons over magic

**Ally** — Mage (V4 A5 Ag4 T6 S6 St3):

- High Spirit (6): 1.6x magic damage — trained in lore and rituals
- High Tactics (6): +12 defense — strategic thinker
- High Arcana (5): 1.4x spell power — innate magical talent
- Low Strength (3): prefers research over combat

### CharacterStats Struct (C++)

```cpp
struct CharacterStats {
    int vitality  = 5;  // 1-10
    int arcana    = 3;
    int agility   = 5;
    int tactics   = 5;
    int spirit    = 4;
    int strength  = 5;

    int hp_bonus() const;           // vitality * 10
    float crit_chance() const;      // agility * 0.03
    int defense_bonus() const;      // tactics * 2
    float magic_damage_mult() const; // 1.0 + spirit * 0.1
    int weapon_damage_bonus() const; // strength * 2
    float dodge_chance() const;     // agility * 0.02
    float spell_power_mult() const; // 1.0 + arcana * 0.08
};
```

### Battle Integration

Skills automatically apply when a battle starts:

- `player_atk` += Hero's `weapon_damage_bonus()` (Strength)
- `player_def` += Hero's `defense_bonus()` (Tactics)
- `ally_atk` += Ally's `weapon_damage_bonus()` (Strength)

During battle, skill globals are synced to SageLang (`skill_vitality`, `skill_agility`, etc.) so scripts can apply crit/dodge/magic bonuses.

### SageLang Native Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_skill` | `get_skill(character, skill)` | Get skill value (1-10) |
| `set_skill` | `set_skill(character, skill, value)` | Set skill value (clamped 1-10) |
| `get_skill_bonus` | `get_skill_bonus(character, type)` | Get derived bonus value |

**Bonus types:** `"hp"`, `"crit"`, `"defense"`, `"magic_mult"`, `"weapon_dmg"`, `"dodge"`, `"spell_mult"`

### Skill Script Example

```sage
proc skill_crit_check():
    let fighter = "player"
    if active_fighter == 1:
        fighter = "ally"
    let agility = get_skill(fighter, "agility")
    let roll = random(1, 100)
    if roll <= agility * 3:
        battle_damage = battle_damage * 2
        battle_msg = battle_msg + " CRITICAL!"
```

---

## Audio System

### Overview

The audio system uses **miniaudio** (single-header C library) for cross-platform audio playback. It supports background music with looping, sound effects, volume control, and smooth crossfading between tracks.

### AudioEngine Class

```cpp
eb::AudioEngine audio;

// Background music
audio.play_music("assets/audio/overworld.wav", true);  // loop
audio.stop_music();
audio.pause_music();
audio.resume_music();
audio.set_music_volume(0.5f);  // 0.0 - 1.0

// Crossfade to different track
audio.crossfade_music("assets/audio/battle.wav", 0.5f, true);

// Sound effects
audio.play_sfx("assets/audio/hit.wav", 1.0f);

// Master volume
audio.set_master_volume(0.8f);

// Call each frame (required for crossfade)
audio.update(dt);
```

### Automatic Music Switching

The engine automatically crossfades between overworld and battle music:

- **Entering battle** — crossfades to `battle.wav` over 0.5s
- **Exiting battle** — crossfades back to `overworld.wav` over 1.0s
- Same-track requests are ignored (no restart)

### Music Tracks

| Track | File | Duration | Style |
|-------|------|----------|-------|
| Overworld | `assets/audio/overworld.wav` | 44s | 8-bit overworld theme (A minor, 120 BPM) |
| Battle | `assets/audio/battle.wav` | 12s | Intense 8-bit battle theme (E minor, 160 BPM) |

Both tracks are procedurally generated WAVs using square wave melody and sine bass, designed to loop seamlessly.

### Platform Support

| Platform | Backend |
|----------|---------|
| Linux | PulseAudio / ALSA |
| Windows | WASAPI |
| Android | OpenSL ES / AAudio |

miniaudio auto-detects the best backend at runtime.

### Adding New Music

1. Place WAV/MP3/FLAC files in `assets/audio/`
2. Call `audio.play_music("assets/audio/my_track.wav", true)` to play
3. Use `crossfade_music()` for smooth transitions
4. For Android, `build.sh` automatically copies audio assets to the APK

---

## SageLang Battle Scripting

### How It Works

1. C++ syncs battle state variables TO SageLang globals
2. C++ calls the appropriate SageLang function
3. SageLang script reads/modifies the globals
4. C++ syncs the modified values BACK from SageLang

### Synced Variables

| Variable | Type | Description |
|----------|------|-------------|
| `enemy_hp` | number | Current enemy HP (read/write) |
| `enemy_max_hp` | number | Enemy max HP |
| `enemy_atk` | number | Enemy attack power |
| `enemy_name` | string | Enemy name |
| `player_hp` | number | Hero's current HP (read/write) |
| `player_max_hp` | number | Hero's max HP |
| `player_atk` | number | Hero's attack power |
| `player_def` | number | Hero's defense |
| `ally_hp` | number | Ally's current HP (read/write) |
| `ally_max_hp` | number | Ally's max HP |
| `ally_atk` | number | Ally's attack power |
| `active_fighter` | number | 0 = Player, 1 = Ally |
| `battle_damage` | number | Result: damage/heal amount |
| `battle_msg` | string | Result: message to display |
| `battle_target` | string | Result: who was hit |

### Battle Script Example

```sage
# battle_system.sage

proc attack_normal():
    let fighter_name = "Hero"
    let base_atk = player_atk
    if active_fighter == 1:
        fighter_name = "Ally"
        base_atk = ally_atk

    let damage = base_atk + random(-2, 2)
    if damage < 1:
        damage = 1

    # Critical hit chance (10%)
    let crit = random(1, 10)
    if crit == 10:
        damage = damage * 2
        battle_msg = fighter_name + " lands a CRITICAL HIT! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"

    enemy_hp = enemy_hp - damage
    battle_damage = damage

proc vampire_attack():
    # Vampires drain HP — damages target AND heals self
    let target = random(0, 1)
    if player_hp <= 0:
        target = 1
    if ally_hp <= 0:
        target = 0

    let damage = enemy_atk + random(0, 5)
    let drain = damage / 3

    if target == 0:
        player_hp = player_hp - damage
    else:
        ally_hp = ally_hp - damage

    enemy_hp = enemy_hp + drain
    if enemy_hp > enemy_max_hp:
        enemy_hp = enemy_max_hp

    battle_msg = enemy_name + " drains " + str(damage) + " HP!"
```

### Available Script Functions

| Function | Called When | Purpose |
|----------|-----------|---------|
| `attack_normal()` | Player selects Attack | Calculate and apply damage to enemy |
| `attack_iron_sword()` | Legacy (use inventory) | Iron sword attack (requires `has_iron_sword` flag) |
| `attack_holy_water()` | Legacy (use inventory) | Holy water attack (requires flag) |
| `defend()` | Player selects Defend | Heal the active fighter |
| `enemy_turn()` | Enemy's turn (generic) | AI selects target and attacks |
| `vampire_attack()` | Enemy's turn (vampire) | Vampire-specific drain attack |
| `on_victory()` | Battle won | Award XP, set flags |
| `on_defeat()` | Battle lost | Set flags |

### Adding New Attacks

1. Write a new `proc` in `battle_system.sage`
2. Read from synced globals, modify `enemy_hp`/`player_hp`/`ally_hp`
3. Set `battle_msg` and `battle_damage` for display
4. Call from C++ via `script_engine->call_function("my_attack")`

---

## Dialogue System

### DialogueBox (`game/dialogue/dialogue_box.h`)

Features:
- Typewriter effect (35 chars/sec)
- Word wrapping
- Character portraits (Hero, Ally, Elder — cropped from Profiles.png)
- Dialog.png background texture (Stardew Valley-style dark atmospheric box)
- Speaker name highlighting (cyan)
- Blinking advance indicator
- Choice menus

### Dialogue Files (`.dialogue`)

Legacy format with labeled sections:
```
@greeting
Elder: Adventurers, you better be prepared.
Elder: Watch your back.

@after_battle
Elder: Are you alright?
Hero: Nothing a good rest won't fix.
```

### SageLang Dialogue Scripts (`.sage`)

Modern format with full scripting and conditional logic:
```sage
proc greeting():
    say("Elder", "Adventurers, you better be prepared.")
    if get_flag("has_iron_sword"):
        say("Elder", "Good, you've got supplies.")
    else:
        say("Elder", "Talk to me about supplies.")

proc supplies():
    say("Elder", "Here, take this iron sword.")
    set_flag("has_iron_sword", true)
    set_flag("has_holy_water", true)
```

### Dialogue File Loading

Both formats are loaded at startup. `.dialogue` files are parsed by `load_dialogue_file()` using the cross-platform `FileIO::read_file()` (works on desktop and Android). `.sage` files are loaded into the SageLang interpreter.

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
    float aggro_range;      // Distance to start chasing (px)
    float attack_range;     // Distance to auto-trigger dialogue/battle (px)
    float move_speed;       // Movement speed (px/sec)
    float wander_interval;  // Time between wander target changes (sec)
    bool has_battle;
    int sprite_atlas_id;
    std::vector<DialogueLine> dialogue;
};
```

### AI Behaviors

- **Passive NPCs**: Wander randomly near home position, require manual interaction (Z/Enter or A button)
- **Hostile NPCs**: Chase player when within aggro range at increased speed, auto-trigger dialogue + battle at attack range
- **Post-encounter**: Hostile NPCs become passive after triggering (`has_triggered` flag)
- **Walk Animation**: NPCs animate while moving — direction from movement vector, frames toggle every 0.2s

---

## Scripting with SageLang

### Overview

SageLang is embedded as a C library via the `ScriptEngine` wrapper class. It's a Python-like language that compiles to C, is self-hosted, and runs as both a scripting and compiled language.

- **Repository**: https://github.com/Night-Traders-Dev/SageLang
- **Integration**: Git submodule at `src/third_party/sagelang/`
- **Build**: Compiled as a static library, linked into `tw_engine`

### Engine API Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `say` | `say(speaker, text)` | Display dialogue line |
| `log` | `log(message)` | Debug output to console |
| `set_flag` | `set_flag(name, value)` | Set a persistent game flag |
| `get_flag` | `get_flag(name)` | Check a game flag (returns bool) |
| `random` | `random(min, max)` | Random integer in [min, max] |
| `clamp` | `clamp(value, min, max)` | Clamp a number to range |
| `add_item` | `add_item(id, name, qty, type, ...)` | Add item to inventory |
| `remove_item` | `remove_item(id, qty)` | Remove item from inventory |
| `has_item` | `has_item(id)` | Check if item exists (returns bool) |
| `item_count` | `item_count(id)` | Get item quantity |
| `get_skill` | `get_skill(character, skill)` | Get Character Stats skill value (1-10) |
| `set_skill` | `set_skill(character, skill, value)` | Set skill value (clamped 1-10) |
| `get_skill_bonus` | `get_skill_bonus(character, type)` | Get derived bonus (hp, crit, etc.) |

### Script Loading

```cpp
eb::ScriptEngine script_engine;
script_engine.set_game_state(&game);
script_engine.load_file("assets/scripts/battle_system.sage");
script_engine.load_file("assets/scripts/inventory.sage");
script_engine.load_file("assets/scripts/elder.sage");
script_engine.call_function("give_starter_items");
script_engine.call_function("greeting");
```

### Game State Sync (Battle)

```cpp
// Before calling a battle script function:
script_engine.sync_battle_to_script();  // Push C++ state → SageLang globals
script_engine.call_function("attack_normal");
script_engine.sync_battle_from_script(); // Pull SageLang globals → C++ state
```

### Platform Notes

- **Linux**: Full SageLang support including FFI (`dlfcn.h`)
- **Windows**: Compiled with `SAGE_NO_FFI` (no dynamic library loading)
- **Android**: SageLang sources compiled directly into the APK native library

---

## Tile Editor

### Activation

Press **Tab** to toggle the editor. The game world renders underneath; editor UI overlays on top via Dear ImGui.

### Tools

| Tool | Key | Description |
|------|-----|-------------|
| Paint | P | Place selected tile or stamp object (supports 1x1, 2x2, 3x3 brush) |
| Erase | E | Remove tiles (right-click also erases, brush-size aware) |
| Fill | F | Flood fill area with selected tile |
| Eyedrop | I | Pick tile from map |
| Select | R | Rectangle selection (then Copy/Fill/Delete) |
| Collision | C | Cycle collision types (None → Solid → Portal) |
| Line | L | Draw straight lines of tiles (Bresenham algorithm) |
| Rectangle | B | Draw filled or outlined rectangles (toggle in UI) |

### Brush Sizes

Paint and Erase tools support three brush sizes selectable via radio buttons in the Tools panel:
- **1x1** — single tile (default)
- **2x2** — 4-tile square
- **3x3** — 9-tile square

### ImGui Panels

**Tools Window** (left):
- 8 tool buttons with active highlight
- Grid/Collision checkboxes
- Layer buttons with individual visibility toggles (Shift+1-9)
- Zoom display + reset
- Undo/Redo buttons with counts
- Save As... / Load... (native file dialogs) + Quick Save
- Brush size radio buttons (1x1, 2x2, 3x3)
- Filled Rectangle toggle (when Rect tool selected)
- Import Asset... button (opens file dialog for PNG/JPG/BMP)
- Selection tools (Copy, Fill, Delete, Flip H/V)
- Status messages

**Minimap Window** (bottom-left):
- Color-coded tile overview (green=grass, blue=water, grey=roads, brown=dirt)
- Solid collision tiles darkened
- Yellow dot: player position
- Blue/red dots: friendly/hostile NPCs
- Click to teleport camera to that location

**Assets Window** (right, tabbed):
- **Tiles** — all 54 ground tiles as image buttons with actual texture previews
- **Buildings** — Gas Mart, Salvage Repair, etc. with image previews and labels
- **Vehicles** — Impala, cars with previews
- **Trees** — all 17 tree/bush types with previews
- **Misc** — all 21 misc objects with previews
- Selected item preview at bottom of panel

### Ghost Cursor

When hovering over the map with a tile or stamp selected, a semi-transparent preview appears at the mouse cursor showing what will be placed.

### Object Placement & Deletion

- **Place**: Select an object from Buildings/Vehicles/Trees/Misc tab, click on map
- **Delete**: Right-click near a placed object (within 48px) to remove it
- Both placement and deletion are fully undoable (Ctrl+Z)

### File Dialogs

- **Save As...** / **Load...** open native OS file dialogs
- Linux: Uses `zenity` via `popen()` (isolated from Vulkan process to prevent GTK/Wayland crashes)
- Windows: Uses tinyfiledialogs (native Win32 dialogs)
- `vkDeviceWaitIdle()` called before dialog opens to prevent semaphore corruption

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Shift+Z / Ctrl+Y | Redo |
| Ctrl+S | Quick save |
| Ctrl+C | Copy selection |
| Ctrl+V | Paste |
| Ctrl+H | Flip clipboard horizontal |
| Ctrl+J | Flip clipboard vertical |
| Ctrl+0 | Reset zoom |
| G | Toggle grid |
| V | Toggle collision overlay |
| 1-9 | Switch active layer |
| Shift+1-9 | Toggle layer visibility |
| Delete | Clear selection |
| Middle mouse | Pan camera |
| Scroll wheel | Zoom (over map) / Scroll (over palette) |
| Right-click | Erase tile or delete nearest object |

### Undo/Redo

100-action history stack. All operations are recorded and fully reversible:
- Tile painting, erasing, filling
- Collision type changes
- Object placement and deletion
- Paste operations
- Selection clearing

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
    {"name": "ground", "data": [1,2,3,...]}
  ],
  "collision": [1,1,0,0,...],
  "portals": [
    {"x":5, "y":10, "target_map":"indoor.json", "target_x":3, "target_y":8, "label":"door"}
  ],
  "objects": [
    {"x":224, "y":256, "src_x":1007, "src_y":92, "src_w":157, "src_h":97,
     "render_w":140, "render_h":86}
  ],
  "npcs": [
    {"name":"Elder", "x":256, "y":224, "dir":0, "sprite_atlas_id":0,
     "interact_radius":40, "hostile":false, "aggro_range":150,
     "attack_range":32, "move_speed":30, "wander_interval":4,
     "has_battle":false,
     "dialogue":[{"speaker":"Elder", "text":"Hello."}]}
  ]
}
```

### What's Saved

- **Metadata**: Map name, dimensions, tile size, tileset path, player start position
- **Tile layers**: All layers with names and full tile data arrays
- **Collision**: Per-tile collision types (0=none, 1=solid, 2=portal)
- **Portals**: Position, target map file, target coordinates, label
- **World objects**: Position + full source region info for reconstruction
- **NPCs**: Full definition including AI parameters, battle info, and dialogue

---

## Asset Pipeline

### Asset Requirements

All textures must be:

- **PNG format** with **RGBA transparency** (no colored backgrounds)
- **32x32 pixel tiles** for tilesets (or 16x16 scaled 2x with nearest neighbor)
- **Named regions** for character sprites (direction + animation frame)

### Importing Assets

Use the **Import Asset...** button in the editor's Tools panel, or manually place files in the game's `assets/textures/` directory.

### Converting External Sprite Sheets

External sprite sheets often need conversion before the engine can use them:

1. **Background removal** — Remove solid or checkerboard backgrounds to RGBA transparency
2. **Scale to 32px** — If source is 16x16 tiles, scale 2x with nearest-neighbor interpolation
3. **Grid alignment** — Ensure sprites are on a regular grid for atlas region mapping

```python
# Example: scale 16x16 tileset to 32x32
from PIL import Image
img = Image.open("tileset_16.png").convert("RGBA")
scaled = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
scaled.save("tileset_32.png")
```

### Included Sample Assets

| Asset | Source | Format | Tiles/Sprites |
|-------|--------|--------|---------------|
| `earthbound/nes_sprites.png` | EarthBound NES | 996x1030, RGBA | Ness walk cycles, 4 rows |
| `earthbound/nes_sprites_2x.png` | Same, 2x scaled | 1992x2060, RGBA | Pixel-art upscale |
| `village/village_tileset_32.png` | Village tileset | 640x256, RGBA | 160 tiles (20x8 grid) |
| `sprite/free_campfire_32.png` | Campfire anim | 96x256, RGBA | 6 animation frames |
| `sprite/free_chests_32.png` | Chest variants | 432x512, RGBA | 208 chest sprites |
| `sprite/free_doors1_32.png` | Door variants | 384x256, RGBA | 96 door sprites |
| `sprite/free_icons1_32.png` | Heart icons | 96x128, RGBA | 12 heart sprites |
| `sprite/free_icons2_32.png` | Gem icons | 96x128, RGBA | 12 gem sprites |

### Character Sprite Sheets

Sprite sheets use named regions for each direction/animation:

- `idle_down`, `idle_up`, `idle_right` (left = UV-flipped right)
- `walk_down_0/1`, `walk_up_0/1`, `walk_right_0/1`

Two formats supported:

- **Grid-based**: Uniform cell size (e.g., 158x210), specified as `"sprite_grid": [158, 210]` in game.json
- **Custom regions**: Per-region pixel coordinates, specified as `"sprite_regions": { "idle_down": [x,y,w,h], ... }` in game.json

### Tileset Processing

Tilesets use transparent backgrounds. If source has a colored background:

1. Remove background via flood-fill from edges or color-key removal
2. Find sprite bounding boxes via connected component analysis
3. Define atlas regions with `atlas.add_region(x, y, width, height)`

### Fonts

TTF fonts baked into texture atlases at runtime via stb_truetype.

- Default: DejaVu Sans Mono Bold
- Game text: 7px with 6px letter spacing
- Editor text: Rendered via Dear ImGui's built-in font

---

## Android Platform

### Landscape Mode

Forced via `setRequestedOrientation(SCREEN_ORIENTATION_SENSOR_LANDSCAPE)` in Java + `preTransform = IDENTITY` in Vulkan swapchain.

### Virtual Resolution

Android uses 480p virtual height (width scaled by aspect ratio). The GPU stretches the rendered frame to fill the native screen. Touch controls render at native resolution for accurate finger hit detection.

### Touch Controls

Scaled dynamically based on screen DPI (scale = shorter_side / 720):
- D-pad (bottom-left): 120px base radius
- A button (confirm): 60px base radius
- B button (cancel): 60px base radius
- Menu button (top-right)

### Touch Input Timing

Touch events arrive via `ALooper` before `poll_events()`. The `apply_to()` call copies touch state to input BEFORE `begin_frame()` clears pressed flags, ensuring button presses are not lost.

---

## Adding New Content

### Adding a New NPC

1. Create sprite sheet (3x3 grid: idle/walk for down/up/right)
2. Process with Python to remove background and find bounding boxes
3. Add atlas region in `define_tileset_regions()` or load as separate texture
4. Add NPC in `setup_npcs()` with position, sprite ID, AI parameters
5. Create `.sage` script in `assets/scripts/` for dialogue
6. Create `.dialogue` file in `assets/dialogue/` as fallback

### Adding a New Tile Type

1. Add entry to `Tile` enum in `game.h`
2. Add `atlas.add_region()` call in `define_tileset_regions()` with precise pixel coordinates
3. Available immediately in the editor's Tiles palette

### Adding a New Object Stamp

1. Add `atlas.add_region()` for the sprite in `define_tileset_regions()`
2. Add `add()` call in `define_object_stamps()` with name, region index, placement size, category
3. Appears in the appropriate editor tab (Buildings/Vehicles/Trees/Misc)

### Adding a New Map

1. Use the editor to build the map (Tab to enter editor)
2. Place tiles, objects, set collision
3. Save via Save As... button (native file dialog)
4. Map file includes tiles, collision, objects, NPCs, portals

### Adding New Items

1. Define a grant function in `assets/scripts/inventory.sage`:

```sage
proc find_machete():
    add_item("machete", "Machete", 1, "weapon",
             "Good for decapitations", 0, 45, "", "use_machete")
```

2. Write a `use_*` battle function in `inventory.sage`:

```sage
proc use_machete():
    let fighter_name = "Hero"
    if active_fighter == 1:
        fighter_name = "Ally"
    let damage = 45 + random(0, 12)
    if enemy_name == "Vampire":
        damage = damage * 2
        battle_msg = fighter_name + " decapitates! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " slashes with machete! " + str(damage) + " damage!"
    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
```

3. Call the grant function from an NPC script, event trigger, or game init
4. The item automatically appears in the battle Items submenu with its quantity

### Adding New Battle Attacks

1. Write a new `proc` in `assets/scripts/battle_system.sage`
2. Read synced globals (`enemy_hp`, `player_atk`, etc.)
3. Modify HP values and set `battle_msg`, `battle_damage`
4. Call from C++ via `script_engine->call_function("my_attack")`

### Adding New Script Functions (C++ → SageLang)

1. Write a C function matching `Value fn(int argc, Value* args)`
2. Register in `ScriptEngine::register_engine_api()` via `env_define()`
3. Call from `.sage` scripts by name

---

## Day-Night Cycle

The engine includes a real-time day-night cycle that drives visual tinting and NPC schedules.

### How It Works

- `DayNightCycle` struct in `GameState` tracks `game_hours` (0.0–24.0) and `day_speed` (minutes per real second)
- Each frame, the clock advances: `game_hours += (dt * day_speed) / 60.0`
- A tint color is computed from the hour and rendered as a fullscreen overlay
- Default start: 8:00 AM, speed 1.0 (1 real second = 1 game minute)

### Visual Tint Schedule

| Time | Tint | Description |
|------|------|-------------|
| 5:00–6:00 | Dark blue → warm orange | Dawn |
| 6:00–7:30 | Warm → clear | Morning |
| 7:30–16:00 | No tint | Daytime |
| 16:00–18:00 | Clear → orange | Sunset |
| 18:00–20:00 | Orange → purple-blue | Dusk |
| 20:00–21:00 | Purple-blue → dark blue | Evening |
| 21:00–5:00 | Dark blue | Night |

### SageLang API

```sage
# Query time
let hour = get_hour()       # 0-23
let min = get_minute()      # 0-59

# Set time directly
set_time(12, 0)             # Noon
set_time(22, 30)            # 10:30 PM

# Control speed
set_day_speed(1)            # Default: 1 real second = 1 game minute
set_day_speed(60)           # Fast: full day in 24 seconds
set_day_speed(0)            # Freeze time

# Check day/night
if is_day():                # true between 6:00 and 18:00
    say("Elder", "Good day!")
if is_night():              # true between 18:00 and 6:00
    say("Elder", "Be careful at night.")
```

### Implementation Files

- `src/game/systems/day_night.h` / `.cpp` — `update_day_night()`, `compute_day_tint()`, `is_hour_in_range()`
- Tint rendered in `render_game_ui()` before HUD
- Clock updated in `update_game()` before all other systems

---

## NPC Pathfinding & Routes

### A* Pathfinding

NPCs can navigate around obstacles using A* pathfinding on the tile collision grid.

- 4-directional (no diagonals), max 200 search steps
- Uses `TileMap::is_solid()` for obstacle detection
- Path computed once per `npc_move_to()` call, then followed each frame
- NPCs also check collision during idle wander now

```sage
# Move an NPC to a specific tile (pathfinds around walls)
npc_move_to("Elder", 10, 8)

# Returns true if path found, false if blocked
let ok = npc_move_to("Merchant", 15, 12)
```

### Route / Patrol System

NPCs can follow waypoint-based routes with three modes:

| Mode | Behavior |
|------|----------|
| `"patrol"` | Loop: 1→2→3→1→2→3→... |
| `"once"` | Stop at last waypoint |
| `"pingpong"` | Reverse: 1→2→3→2→1→2→... |

Each waypoint-to-waypoint segment uses A* pathfinding automatically.

```sage
# Define a patrol route for the Elder
npc_add_waypoint("Elder", 320, 256)    # World coordinates
npc_add_waypoint("Elder", 480, 256)
npc_add_waypoint("Elder", 480, 384)
npc_add_waypoint("Elder", 320, 384)
npc_set_route("Elder", "patrol")       # Loop forever
npc_start_route("Elder")

# Stop or clear
npc_stop_route("Elder")                # Pause in place
npc_clear_route("Elder")               # Remove all waypoints
```

### NPC AI Priority Order

Each NPC's behavior is evaluated in this order each frame:

1. **Schedule** — If outside active hours, hide and skip
2. **Hostile chase** — If aggressive and player in aggro range
3. **Path following** — If `npc_move_to()` was called
4. **Route patrol** — If route is active with waypoints
5. **Idle wander** — Random movement near home position (now collision-aware)

### Implementation Files

- `src/game/ai/pathfinding.h` / `.cpp` — `find_path()` A* implementation
- NPC AI loop in `game.cpp` `update_game()`

---

## NPC Schedules & Interactions

### NPC Schedules

NPCs can be assigned time windows during which they appear. Outside their schedule, they become invisible and skip all AI updates.

```sage
# Merchant only appears during business hours
npc_set_schedule("Merchant", 8, 18)        # 8 AM to 6 PM
npc_set_spawn_point("Merchant", 512, 256)  # Where they appear

# Night guard appears at night
npc_set_schedule("Guard", 20, 6)           # 8 PM to 6 AM (wraps midnight)
npc_set_spawn_point("Guard", 300, 300)

# Remove schedule (always visible)
npc_clear_schedule("Merchant")
```

When a schedule activates:
- NPC teleports to their spawn point
- NPC resumes normal AI (wander, patrol, etc.)

When a schedule deactivates:
- NPC becomes invisible
- NPC stops all movement and AI

### NPC-to-NPC Interactions

Register callbacks that fire when two NPCs come within proximity of each other.

```sage
# When Elder and Merchant meet, they chat
npc_on_meet("Elder", "Merchant", "elder_merchant_chat")

proc elder_merchant_chat():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "How's business today?")
    say("Merchant", "Selling well, thanks!")

# Adjust trigger radius (default 40px)
npc_set_meet_radius("Elder", "Merchant", 60)

# Make two NPCs face each other immediately
npc_face_each_other("Elder", "Merchant")
```

Meet triggers fire once by default. They check visibility (scheduled-out NPCs won't trigger).

---

## Spawn System

Periodically spawn NPCs from a template with configurable intervals, limits, and spawn areas.

```sage
# Spawn slimes every 30 seconds, max 5 at a time
spawn_loop("Slime", 30, 5)

# Restrict spawn area to a region
set_spawn_area("Slime", 400, 300, 700, 500)

# Stop spawning
stop_spawn_loop("Slime")
```

### How It Works

- The first NPC matching the template name is used as a clone source
- Spawned NPCs get unique names: `"Slime_0"`, `"Slime_1"`, etc.
- Spawned NPCs inherit the template's sprite, stats, hostility, and behavior
- Position is randomized within the spawn area (or uses template position if no area set)
- `current_count` tracks how many have been spawned; stops at `max_count`

---

## Survival System

Optional hunger, thirst, and energy meters that deplete over game time.

### Enabling

```sage
# Turn on survival mechanics
enable_survival(true)

# Configure depletion rates (per game-minute)
set_survival_rate("hunger", 1.0)     # Default: 1.0
set_survival_rate("thirst", 1.5)     # Default: 1.5
set_survival_rate("energy", 0.8)     # Default: 0.8
```

### Reading & Setting Values

```sage
let h = get_hunger()     # 0-100
let t = get_thirst()     # 0-100
let e = get_energy()     # 0-100

# Restore via items
set_hunger(100)          # Full
set_thirst(get_thirst() + 30)
set_energy(100)
```

### Effects

| Stat | Threshold | Effect |
|------|-----------|--------|
| Hunger < 25 | Speed -20% | Player moves slower |
| Thirst < 25 | Speed -15% | Player moves slower |
| Energy < 20 | Speed -25% | Player moves slower |
| Hunger = 0 | HP drain | 1 HP per game-minute |
| Thirst = 0 | HP drain | 1 HP per game-minute |
| Minimum speed | 30% | Player never stops completely |

### Example: Food Item

```sage
proc use_bread():
    set_hunger(get_hunger() + 40)
    if get_hunger() > 100:
        set_hunger(100)
    battle_msg = "Ate bread. Hunger restored."
    remove_item("bread", 1)
```

---

## Script-Driven UI

Scripts can create overlay UI elements: labels, progress bars, and timed notifications.

### Labels

Persistent text displayed at a screen position. Create or update by ID.

```sage
# Show a label (id, text, x, y, r, g, b, a)
ui_label("clock", "12:00 PM", 850, 10, 1, 1, 1, 1)

# Update it each frame from a script
ui_label("clock", str(get_hour()) + ":" + str(get_minute()), 850, 10, 1, 1, 0.8, 1)

# Remove it
ui_remove("clock")
```

### Bars

Progress bars for displaying stats. Create or update by ID.

```sage
# Show a bar (id, value, max, x, y, width, height, r, g, b, a)
ui_bar("hp_bar", 80, 100, 10, 50, 150, 12, 0.2, 0.8, 0.2, 1)

# Survival bars example
ui_bar("hunger", get_hunger(), 100, 10, 680, 120, 8, 0.8, 0.6, 0.2, 1)
ui_bar("thirst", get_thirst(), 100, 140, 680, 120, 8, 0.2, 0.5, 0.9, 1)
ui_bar("energy", get_energy(), 100, 270, 680, 120, 8, 0.9, 0.8, 0.2, 1)
```

### Notifications

Timed popup messages displayed at the top center of the screen.

```sage
# Show a notification (text, duration_seconds)
ui_notify("Quest Updated!", 3)
ui_notify("You found a key!", 5)
ui_notify("Night is falling...", 4)
```

Notifications auto-fade and auto-remove when their duration expires.

---

## Shop System

The merchant store UI is driven entirely by SageLang scripts.

### Opening a Shop

```sage
proc merchant_shop_items():
    # add_shop_item(id, name, price, type, desc, heal, dmg, element, sage_func)
    add_shop_item("potion", "Potion", 25, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_shop_item("fire", "Fire Scroll", 60, "weapon", "Fire magic", 0, 30, "fire", "use_fire")
    open_shop("Merchant")
```

The function `{npc_name}_shop_items()` is auto-called when the player talks to that NPC.

### Shop Features

- **Buy tab**: Browse and purchase items (deducts gold)
- **Sell tab**: Sell inventory items for half buy price
- **Gold display**: Shows current gold with coin icon
- **Item descriptions**: Stats shown at bottom of panel
- **Affordable indicator**: Prices turn red when player can't afford

### Gold API

```sage
set_gold(500)              # Set gold directly
let g = get_gold()         # Query current gold
```

### Dynamic Inventory

Use `random()`, `get_flag()`, and `set_flag()` to create shops with rotating stock:

```sage
proc merchant_shop_items():
    let visits = get_flag("shop_visits")
    visits = visits + 1
    set_flag("shop_visits", visits)

    # Always available
    add_shop_item("potion", "Potion", 25, "consumable", "Heals 50 HP", 50, 0, "", "use_potion")

    # Random stock
    if random(1, 3) == 1:
        add_shop_item("rare_gem", "Rare Gem", 300, "key", "A valuable gem", 0, 0, "", "")

    # Unlock after 3 visits
    if visits >= 3:
        add_shop_item("steel_sword", "Steel Sword", 450, "weapon", "25 ATK", 0, 25, "", "")

    open_shop("Merchant")
```

---

## SageLang API Reference

Complete reference of all functions available in `.sage` scripts.

### Engine Core

| Function | Signature | Description |
|----------|-----------|-------------|
| `say` | `say(speaker, text)` | Queue a dialogue line |
| `log` | `log(message)` | Print to debug console |
| `set_flag` | `set_flag(name, value)` | Store a persistent value (any type) |
| `get_flag` | `get_flag(name) → value` | Retrieve a stored value (0 if unset) |
| `random` | `random(min, max) → number` | Random integer in [min, max] |
| `clamp` | `clamp(value, min, max) → number` | Constrain to range |
| `str` | `str(value) → string` | Convert number/bool to string |

### Inventory

| Function | Signature | Description |
|----------|-----------|-------------|
| `add_item` | `add_item(id, name, qty, type, desc, heal, dmg, element, sage_func)` | Add item to inventory |
| `remove_item` | `remove_item(id, qty)` | Remove from inventory |
| `has_item` | `has_item(id) → bool` | Check if player has item |
| `item_count` | `item_count(id) → number` | Get quantity |

### Shop

| Function | Signature | Description |
|----------|-----------|-------------|
| `add_shop_item` | `add_shop_item(id, name, price, type, desc, heal, dmg, element, sage_func)` | Queue shop item |
| `open_shop` | `open_shop(merchant_name)` | Open store UI |
| `set_gold` | `set_gold(amount)` | Set player gold |
| `get_gold` | `get_gold() → number` | Get player gold |

### Character Stats

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_skill` | `get_skill(character, stat) → number` | Get stat (1-10). Character: "player"/"ally" |
| `set_skill` | `set_skill(character, stat, value)` | Set stat. Stats: vitality, arcana, agility, tactics, spirit, strength |
| `get_skill_bonus` | `get_skill_bonus(character, bonus) → number` | Derived bonus: hp, crit, defense, magic_mult, weapon_dmg, dodge, spell_mult |

### Day-Night Cycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_hour` | `get_hour() → number` | Current game hour (0-23) |
| `get_minute` | `get_minute() → number` | Current game minute (0-59) |
| `set_time` | `set_time(hour, minute)` | Set game clock |
| `set_day_speed` | `set_day_speed(multiplier)` | Time speed (default 1.0, 0 = freeze) |
| `is_day` | `is_day() → bool` | True if 6:00–18:00 |
| `is_night` | `is_night() → bool` | True if 18:00–6:00 |

### NPC Pathfinding

| Function | Signature | Description |
|----------|-----------|-------------|
| `npc_move_to` | `npc_move_to(name, tile_x, tile_y) → bool` | A* pathfind NPC to tile |

### NPC Routes

| Function | Signature | Description |
|----------|-----------|-------------|
| `npc_add_waypoint` | `npc_add_waypoint(name, x, y)` | Add world-coord waypoint |
| `npc_set_route` | `npc_set_route(name, mode)` | Set mode: "patrol", "once", "pingpong" |
| `npc_start_route` | `npc_start_route(name)` | Activate route following |
| `npc_stop_route` | `npc_stop_route(name)` | Pause route |
| `npc_clear_route` | `npc_clear_route(name)` | Remove all waypoints |

### NPC Schedules

| Function | Signature | Description |
|----------|-----------|-------------|
| `npc_set_schedule` | `npc_set_schedule(name, start_hour, end_hour)` | Set active hours |
| `npc_set_spawn_point` | `npc_set_spawn_point(name, x, y)` | Where NPC appears |
| `npc_clear_schedule` | `npc_clear_schedule(name)` | Remove schedule |

### NPC Interactions

| Function | Signature | Description |
|----------|-----------|-------------|
| `npc_on_meet` | `npc_on_meet(npc1, npc2, callback)` | Register proximity trigger |
| `npc_set_meet_radius` | `npc_set_meet_radius(npc1, npc2, radius)` | Set trigger distance (default 40) |
| `npc_face_each_other` | `npc_face_each_other(npc1, npc2)` | Make NPCs face each other |

### Spawn System

| Function | Signature | Description |
|----------|-----------|-------------|
| `spawn_loop` | `spawn_loop(name, interval, max_count)` | Start periodic spawning |
| `stop_spawn_loop` | `stop_spawn_loop(name)` | Stop spawning |
| `set_spawn_area` | `set_spawn_area(name, x1, y1, x2, y2)` | Set random spawn area |

### Survival

| Function | Signature | Description |
|----------|-----------|-------------|
| `enable_survival` | `enable_survival(enabled)` | Turn survival on/off |
| `get_hunger` | `get_hunger() → number` | Current hunger (0-100) |
| `set_hunger` | `set_hunger(value)` | Set hunger |
| `get_thirst` | `get_thirst() → number` | Current thirst (0-100) |
| `set_thirst` | `set_thirst(value)` | Set thirst |
| `get_energy` | `get_energy() → number` | Current energy (0-100) |
| `set_energy` | `set_energy(value)` | Set energy |
| `set_survival_rate` | `set_survival_rate(stat, rate)` | Depletion per game-minute |

### Script UI

| Function | Signature | Description |
|----------|-----------|-------------|
| `ui_label` | `ui_label(id, text, x, y, r, g, b, a)` | Create/update text label |
| `ui_bar` | `ui_bar(id, value, max, x, y, w, h, r, g, b, a)` | Create/update progress bar |
| `ui_remove` | `ui_remove(id)` | Remove element by ID |
| `ui_notify` | `ui_notify(text, duration)` | Show timed notification |

### Debug

| Function | Signature | Description |
|----------|-----------|-------------|
| `debug` | `debug(msg)` | Log at debug level (grey) |
| `info` | `info(msg)` | Log at info level (green) |
| `warn` | `warn(msg)` | Log at warning level (yellow) |
| `error` | `error(msg)` | Log at error level (red) |
| `print` | `print(a, b, ...)` | Print multiple values (cyan) |
| `assert_true` | `assert_true(condition, msg)` | Assert, logs error if false |

### Battle Variables

These globals are automatically synced before/after battle script calls:

| Variable | Type | Description |
|----------|------|-------------|
| `enemy_hp` | number | Enemy current HP (read/write) |
| `enemy_max_hp` | number | Enemy max HP |
| `enemy_atk` | number | Enemy attack power |
| `enemy_name` | string | Enemy name |
| `player_hp` | number | Player current HP (read/write) |
| `player_max_hp` | number | Player max HP |
| `player_atk` | number | Player attack power |
| `player_def` | number | Player defense |
| `ally_hp` | number | Ally current HP (read/write) |
| `ally_max_hp` | number | Ally max HP |
| `ally_atk` | number | Ally attack power |
| `active_fighter` | number | 0 = Player, 1 = Ally |
| `battle_damage` | number | Set to damage/healing amount |
| `battle_msg` | string | Set to combat message |
| `battle_target` | string | Set to "enemy", player name, or ally name |
| `skill_vitality` | number | Current fighter's vitality stat |
| `skill_arcana` | number | Current fighter's arcana stat |
| `skill_agility` | number | Current fighter's agility stat |
| `skill_tactics` | number | Current fighter's tactics stat |
| `skill_spirit` | number | Current fighter's spirit stat |
| `skill_strength` | number | Current fighter's strength stat |

---

*Twilight Engine v0.6.0 — Built with Vulkan, SageLang, miniaudio, and Dear ImGui*
