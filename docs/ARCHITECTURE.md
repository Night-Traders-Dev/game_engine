# Twilight Engine — Architecture Chart

## System Overview

```text
+------------------------------------------------------------------+
|                        TWILIGHT ENGINE v2.6                       |
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
|  |  236 Native Functions across 41 Modules:                   |  |
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
| manifest()        |---->| (236 natives)     |<--->| (ImGui desktop)  |
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

## File Counts (v2.6.0)

| Category | Count | Description |
|----------|-------|-------------|
| C++ Source | 21,249 lines (76 files) | Engine + game framework (excl. third-party) |
| With Third-Party | 272,354 lines (206 files) | Including ImGui, SageLang, stb, miniaudio |
| Game Logic | 5 files, 4,220 lines | game.cpp (1,000), game_io.cpp (537), game_init.cpp (891), game_battle.cpp (516), game_render.cpp (1,276) |
| Script Engine | 2 files, 3,558 lines | script_engine.cpp (2,841) + script_api_new.cpp (717) |
| Script API | 236 functions | 41 modules across 2 files (190 + 46 env_define) |
| Editor | 7 files, 3,699 lines | tile_editor.cpp (1,946), ui (727), script_ide (410), systems (236), imgui (150), npc_spawner (147), debug (83) |
| Graphics | 14 files, 2,493 lines | Vulkan context, renderer, sprite batch, pipeline, text, texture, atlas |
| Platform | 8 files, 1,151 lines | Desktop (232), Android (246), Quest, input (171), touch (277) |
| Systems | 8 headers, 965 lines | Tween (213), particles (244), save (257), spawn, level_manager, day_night, survival, sprite_anim |
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
