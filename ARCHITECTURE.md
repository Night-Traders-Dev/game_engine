# Twilight Engine — Architecture Chart

## System Overview

```text
+------------------------------------------------------------------+
|                        TWILIGHT ENGINE v1.3                       |
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
|  |  150+ Native Functions across 30+ Modules:                   |  |
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

## File Counts (v1.4.0)

| Category | Count | Description |
|----------|-------|-------------|
| C++ Source | 17,211 lines | Engine + game framework |
| Script API | 184 functions | 27 modules |
| Test Assertions | 101 | 33 test categories |
| Tileset Tiles | 580 | 20x29 grid at 32x32 |
| Object Stamps | 62 | 6 categories (buildings, furniture, characters, trees, vehicles, misc) |
| Sage Scripts | 16 | Game logic, weather, tests, map scripts |
| Platforms | 4 | Linux, Windows, Android, Quest |
| Asset Scale Levels | 3 | 1x, 2x, 3x (auto-generated) |

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
Source Assets (1x)              Build Tool                 Output
+------------------+     +-------------------+     +------------------+
| mage_player.png  |     | scale_assets.py   |     | 2x/mage_player   |
| (48x60)          |---->| nearest-neighbor  |---->| (96x120)         |
| cf_tileset.png   |     | + stamps scaling  |     | 3x/mage_player   |
| (640x608)        |     +-------------------+     | (144x180)        |
+------------------+                                +------------------+
                         +-------------------+
                         | cf_stamps.txt     |
                         | 40 object stamps  |
                         | Auto-scaled per   |
                         |   resolution      |
                         +-------------------+
```
