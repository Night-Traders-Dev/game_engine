# Twilight Engine — Architecture Chart

## System Overview

```text
+------------------------------------------------------------------+
|                        TWILIGHT ENGINE v2.4                       |
+------------------------------------------------------------------+
|                                                                    |
|  +--------------------+    +--------------------+                  |
|  |   Platform Layer   |    |   Vulkan Renderer  |                  |
|  |--------------------|    |--------------------|                  |
|  | Desktop (GLFW)     |    | VulkanContext      |                  |
|  | Android (NDK)      |    | SpriteBatch        |                  |
|  | Meta Quest         |    | Pipeline           |                  |
|  | Input (actions)    |    | TextRenderer       |                  |
|  | Touch Controls     |    | Texture/Atlas      |                  |
|  +--------------------+    +--------------------+                  |
|           |                         |                              |
|  +--------------------+    +--------------------+                  |
|  |    Engine Core     |    |  Resource Manager  |                  |
|  |--------------------|    |--------------------|                  |
|  | Engine loop        |    | Texture loading    |                  |
|  | Timer / dt         |    | Atlas cache        |                  |
|  | Debug log          |    | File I/O (Desktop  |                  |
|  | Types (Vec2,Mat4)  |    |   + Android Asset) |                  |
|  +--------------------+    +--------------------+                  |
|           |                         |                              |
+-----------|-------------------------|------------------------------+
            |                         |
+-----------|-------------------------|------------------------------+
|           v                         v                              |
|  +-------------------------------------------------------------+  |
|  |                     GAME FRAMEWORK                           |  |
|  |-------------------------------------------------------------|  |
|  |                                                               |  |
|  |  +------------------+  +------------------+  +--------------+ |  |
|  |  |    TileMap       |  |   NPC / AI       |  |   Battle     | |  |
|  |  |------------------|  |------------------|  |--------------| |  |
|  |  | Multi-layer      |  | A* Pathfinding   |  | Turn-based   | |  |
|  |  | Collision grid   |  | Routes/Patrols   |  | Rolling HP   | |  |
|  |  | Portal system    |  | Schedules        |  | Party system | |  |
|  |  | Tile rotation    |  | Sprite scale     |  | XP/leveling  | |  |
|  |  | Animated tiles   |  | Tint/Flip        |  | Item use     | |  |
|  |  +------------------+  | Spatial grid sep |  +--------------+ |  |
|  |                         | Spawn system     |                   |  |
|  |  +------------------+  +------------------+  +--------------+ |  |
|  |  |  Level Manager   |                        |  Inventory   | |  |
|  |  |------------------|  +------------------+  |--------------| |  |
|  |  | Load/cache/switch|  |   Day-Night      |  | Items/stacks | |  |
|  |  | Portal auto-     |  |------------------|  | Merchant UI  | |  |
|  |  |   transition     |  | Visual tinting   |  | Gold system  | |  |
|  |  | Per-level zoom   |  | Configurable     |  | Loot tables  | |  |
|  |  | Background tick  |  | speed (0-100x)   |  +--------------+ |  |
|  |  | Atlas cache      |  +------------------+                   |  |
|  |  |   (shared_ptr)   |                        +--------------+ |  |
|  |  +------------------+  +------------------+  | Survival     | |  |
|  |                         |    Camera        |  |--------------| |  |
|  |  +------------------+  |------------------|  | Hunger       | |  |
|  |  |   Dialogue       |  | Follow player    |  | Thirst       | |  |
|  |  |------------------|  | Zoom / viewport  |  | Energy       | |  |
|  |  | Typewriter text  |  | Shake effect     |  | Rate config  | |  |
|  |  | Portraits        |  | Bounds clamping  |  +--------------+ |  |
|  |  | Choice menus     |  +------------------+                   |  |
|  |  +------------------+                                         |  |
|  |                                                               |  |
|  +-------------------------------------------------------------+  |
|                              |                                     |
+------------------------------|-------------------------------------+
                               |
+------------------------------|-------------------------------------+
|                              v                                     |
|  +-------------------------------------------------------------+  |
|  |                   SAGELANG SCRIPTING                          |  |
|  |-------------------------------------------------------------|  |
|  |                                                               |  |
|  |  185 Native Functions across 27 Modules:                   |  |
|  |                                                               |  |
|  |  Engine Core    : log, random, clamp, str, flags             |  |
|  |  Player         : pos, hp, atk, def, xp, dir, scale         |  |
|  |  NPC Runtime    : spawn, move, scale, tint, flip, remove     |  |
|  |  Inventory      : add/remove/has/count items                 |  |
|  |  Shop           : add_shop_item, open_shop, gold             |  |
|  |  Battle         : start_battle, xp_formula                   |  |
|  |  Stats          : get/set_skill, bonuses                     |  |
|  |  Day-Night      : time, speed, is_day/night                  |  |
|  |  Survival       : hunger, thirst, energy, rates              |  |
|  |  UI             : labels, bars, panels, images, ui_get/set   |  |
|  |  HUD            : hud_set/get config                         |  |
|  |  Camera         : pos, zoom, shake, follow                   |  |
|  |  Audio          : music, sfx, crossfade, volume              |  |
|  |  Map            : spawn_npc, place_object, portals, loot     |  |
|  |  Spawn          : loops, areas, callbacks, time-gating       |  |
|  |  Routes         : waypoints, patrol/once/pingpong            |  |
|  |  Schedules      : time-based NPC visibility                  |  |
|  |  Interactions   : meet triggers, face each other             |  |
|  |  Effects        : screen shake/flash/fade                    |  |
|  |  Tile Map       : get/set tile, rotation, flip, collision    |  |
|  |  Input          : key_held, key_pressed, mouse               |  |
|  |  Dialogue       : say, speed, scale                          |  |
|  |  Renderer       : clear_color                                |  |
|  |  Level          : load/switch/preload, zoom, level_selector  |  |
|  |  Platform       : PLATFORM, IS_ANDROID/DESKTOP/QUEST         |  |
|  |  Sprite         : npc/player/object scale, tint, flip        |  |
|  |  Weather        : rain, snow, lightning, clouds, god rays,   |  |
|  |                    fog, wind, presets                         |  |
|  |  Debug          : log, warn, error, assert_true              |  |
|  |                                                               |  |
|  +-------------------------------------------------------------+  |
|                                                                    |
+--------------------------------------------------------------------+
```

## Data Flow

```text
game.json (manifest)
    |
    v
+-------------------+     +-------------------+     +------------------+
| init_game_from_   |     | ScriptEngine      |     | TileEditor       |
| manifest()        |---->| (184 natives)     |<--->| (ImGui desktop)  |
| Load tileset,     |     | Load .sage files  |     | Paint/erase/fill |
| NPCs, party,      |     | Execute map_init  |     | Tile rotation    |
| audio config      |     | Hot reload        |     | NPC spawner      |
+-------------------+     +-------------------+     | Script IDE       |
    |                           |                    +------------------+
    v                           v
+-------------------+     +-------------------+
| load_map_file()   |     | update_game()     |
| Parse JSON map    |     | Per-frame loop:   |
| Tiles, collision, |     |  Screen effects   |
| portals, objects, |     |  Day-night        |
| NPCs              |     |  Survival         |
+-------------------+     |  Spawn loops      |
    |                      |  Weather (rain,   |
    v                      |   snow, lightning) |
+-------------------+     |  Pause menu       |
| LevelManager      |     |  Battle/dialogue  |
+-------------------+     |  Player movement  |
| LevelManager      |     |  NPC AI (spatial) |
| Load/cache levels |     |  Portal detection |
| Portal auto-switch|     |  Level background |
| Per-level zoom    |     |  Camera follow    |
| Background tick   |     +-------------------+
+-------------------+           |
                                v
                    +-------------------+
                    | render_game_ui()  |
                    | World sprites     |
                    | Objects (scaled)  |
                    | NPCs (scaled)     |
                    | Player (scaled)   |
                    | Script UI (layered|
                    |   with opacity)   |
                    | Weather (rain,    |
                    |  snow, lightning,  |
                    |  clouds, god rays) |
                    | Battle screen     |
                    | Screen effects    |
                    +-------------------+
```

## File Counts (v2.4.0)

| Category | Count | Description |
|----------|-------|-------------|
| C++ Source | 20,313 lines (72 files) | Engine + game framework + 4 system headers |
| Script Engine | 2 files | script_engine.cpp (core + original APIs, 2,815 lines) + script_api_new.cpp (Phase 1-4 APIs, 644 lines) |
| Script API | 231 functions | 40 modules across 2 files |
| Easing Types | 19 | Linear, Sine, Quad, Cubic, Back, Bounce, Elastic (each with In/Out/InOut) |
| Particle Presets | 9 | fire, smoke, sparkle, blood, dust, magic, explosion, heal, rain_splash |
| Maps | 6 | Forest, House Inside, Desert, Snow, Cave, Volcanic |
| Tilesets | 8 | cf_tileset (1,080 tiles) + 4 procedural biome tilesets + 3 legacy |
| Object Stamps | 88+ | 6 categories + auto-discovered biome stamps (29 additional) |
| Fantasy Icons | 432 | 16x27 grid at 32x32 |
| UI Sheets | 3 + 4 generated | Original (704x2160), Fantasy icons (512x867), Flat UI (736x288) + 4 procedural UI theme packs (47 components each) |
| UI Themes | 4 | Fantasy RPG, Dark, Medieval Stone, Cute Fantasy — panels, buttons, bars, checkboxes, sliders, 9-slice tiles, arrows |
| Sage Scripts | 20 | Game logic, weather, tests, map scripts |
| Biome Presets | 10 | Grasslands, Forest, Desert, Snow, Swamp, Volcanic, Beach, Cave, Urban, Farmland |
| Python Tools | 7 | Tileset generator, biome wiring, test automation, asset scaler, tileset extractor, security fuzzer, UI pack generator |
| Fuzz Categories | 7 | boundaries, division, strings, types, exhaustion, conflicts, rapid |
| Test Assertions | 510 | 46 test sections across all API modules |
| Editor Source | 7 files | tile_editor.cpp (core), npc_spawner, script_ide, debug, systems, ui, imgui_integration |
| Editor Panels | 25 | Tools, Assets (7 tabs), Minimap, NPC Spawner, Script IDE, Debug Console, Game Systems (15 sections), Object Inspector, Prefabs, UI/HUD Editor (with templates) |
| Platforms | 4 | Linux, Windows, Android, Quest |

## Platform Support

```text
+------------------+------------------+------------------+
|     DESKTOP      |     ANDROID      |    META QUEST    |
|------------------|------------------|------------------|
| GLFW window      | NativeActivity   | Flat 2D mode     |
| Keyboard+Mouse   | Touch controls   | Controller input |
| ImGui editor     | Touch editor     | Gamepad mapping  |
| Fullscreen       | Landscape lock   | Quest 2/3/Pro    |
| Vulkan 1.x       | Vulkan 1.x      | Vulkan 1.x       |
+------------------+------------------+------------------+
```

## Asset Pipeline

```text
Source Assets (1x)              Build Tools                Output
+------------------+     +-------------------+     +------------------+
| mage_player.png  |     | scale_assets.py   |     | 2x/mage_player   |
| (48x60)          |---->| nearest-neighbor  |---->| (96x120)         |
| cf_tileset.png   |     | + stamps scaling  |     | 3x/mage_player   |
| (640x1728)       |     +-------------------+     | (144x180)        |
+------------------+                                +------------------+
                         +-------------------+
                         | cf_stamps.txt     |
                         | 88 object stamps  |
                         | Auto-scaled per   |
                         |   resolution      |
                         +-------------------+

Procedural Pipeline:
+-------------------+     +-------------------+     +------------------+
| generate_tileset  |     | wire_biome_levels |     | desert_tileset   |
|   --biome desert  |---->|   Generates maps  |---->| desert_stamps    |
|   --seed 42       |     |   + scripts +     |     | desert.json      |
| 10 biome presets  |     |   portals         |     | desert.sage      |
+-------------------+     +-------------------+     +------------------+
```
