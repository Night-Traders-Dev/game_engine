# Twilight Engine — Architecture Chart

## System Overview

```text
+------------------------------------------------------------------+
|                        TWILIGHT ENGINE v3.2                       |
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
|  |  | Reflection grid  |  |                  |  |              | |  |
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
|  |  266 Native Functions across 48 Modules:                   |  |
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
|  |  Tile Map       : get/set tile, rotation, flip, collision,   |  |
|  |                    reflection                                |  |
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
| manifest()        |---->| (266 natives)     |<--->| (ImGui desktop)  |
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
| reflection,       |     |                   |
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

## File Counts (v3.2.0)

| Category | Count | Description |
|----------|-------|-------------|
| C++ Source | 28,038 lines (109 files) | Engine + game framework (excl. third-party) |
| With Third-Party | ~248,000 lines (230 files) | Including ImGui, SageLang, stb, miniaudio |
| Game Framework | 7 files, 6,107 lines | game_render.cpp (1,552), game.cpp (1,087), game.h (950), game_init.cpp (893), game_io.cpp (572), game_platformer.cpp (537), game_battle.cpp (516) |
| Script Engine | 9 files, 5,277 lines | script_api_map (1,034), script_api_new (814), script_api_systems (668), script_engine (655), script_api_ui (534), script_api_npc (452), script_api_platformer (397), script_api_physics (378), script_api_player (345) |
| Script API | 323 functions | 56 modules across 9 files |
| Editor | 11 files, 5,071 lines | tile_editor.cpp (2,157), tile_editor_ui.cpp (1,356), script_ide (410), tile_editor.h (377), systems (319), imgui (193), npc_spawner (147), debug (83), particle_editor (29) |
| New Systems | 22 headers, 2,725 lines | Collision, raycast, noise, triggers, FSM, pool, checkpoint, combo, replay, trails, rule tiles, dungeon gen, skeleton, coroutine, behavior tree, iso/hex, ECS, net, post-process, mod loader |
| Graphics | 14 files, 2,493 lines | Vulkan context, renderer, sprite batch, pipeline, text, texture, atlas, post-process stub |
| Platform | 8 files, 1,151 lines | Desktop (232), Android (246), Quest, input (171), touch (277) |
| Systems | 8+ headers, 965+ lines | Tween, particles, save, spawn, level_manager, day_night, survival, sprite_anim (+ 22 new system headers) |
| Overworld | 4 files, 880 lines | Camera (149), TileMap (731) with collision + reflection grids |
| Audio | 2 files, 258 lines | miniaudio backend, spatial audio, crossfade |
| AI | 2 files, 119 lines | A* pathfinding with grid-based navigation |
| Easing Types | 19 | Linear, Sine, Quad, Cubic, Back, Bounce, Elastic (each with In/Out/InOut) |
| Particle Presets | 9 | fire, smoke, sparkle, blood, dust, magic, explosion, heal, rain_splash |
| Maps | 6 | Forest, House Inside, Desert, Snow, Cave, Volcanic |
| Tilesets | 16 | cf_tileset (1,080 tiles) + procedural biome tilesets + legacy |
| Object Stamps | 88+ | 6 categories + auto-discovered biome stamps |
| Fantasy Icons | 432 | 16x27 grid at 32x32 |
| UI Regions | 113 | 57 (main atlas) + 56 (flat/theme packs) |
| UI Themes | 4 | Fantasy RPG, Dark, Medieval Stone, Cute Fantasy — 47 components each |
| Sage Scripts | 20 | Game logic, weather, tests, map scripts |
| Biome Presets | 10 | Grasslands, Forest, Desert, Snow, Swamp, Volcanic, Beach, Cave, Urban, Farmland |
| Python Tools | 7 + tw_test | Tileset generator, biome wiring, test automation (11 modules), asset scaler, tileset extractor, security fuzzer, UI pack generator |
| Fuzz Categories | 7 | boundaries, division, strings, types, exhaustion, conflicts, rapid |
| Test Assertions | 510 | 46 test sections across all API modules |
| Editor Panels | 26 | Tools, Assets (7 tabs), Minimap, NPC Spawner, Script IDE, Debug Console, Game Systems (15 sections), Object Inspector, Prefabs, UI/HUD Editor |
| Tile Properties | 2 grids | Collision (None/Solid/Portal) + Reflection (water/ice) |
| Screen Transitions | 5 types | Fade, Iris, Wipe, Pixelate, Slide |
| Platforms | 4 | Linux, Windows, Android, Meta Quest |

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
