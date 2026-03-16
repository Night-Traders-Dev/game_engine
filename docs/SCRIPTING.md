# SageLang Scripting Guide — Twilight Engine

Complete API reference for the Twilight Engine scripting system. All functions are available globally in any `.sage` file loaded by the engine.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Language Basics](#language-basics)
3. [Engine Core](#engine-core)
4. [Player API](#player-api)
5. [NPC API](#npc-api)
6. [Sprite Manipulation](#sprite-manipulation)
7. [Inventory & Items](#inventory--items)
8. [Shop / Merchant](#shop--merchant)
9. [Battle](#battle)
10. [Character Stats](#character-stats)
11. [Day-Night Cycle](#day-night-cycle)
12. [Survival System](#survival-system)
13. [UI Components](#ui-components)
14. [HUD Configuration](#hud-configuration)
15. [Camera](#camera)
16. [Audio](#audio)
17. [Map & World](#map--world)
18. [Spawn System](#spawn-system)
19. [NPC Routes & Schedules](#npc-routes--schedules)
20. [NPC Interactions](#npc-interactions)
21. [Screen Effects](#screen-effects)
22. [Tile Map & Rotation](#tile-map--rotation)
23. [Input](#input)
24. [Dialogue](#dialogue)
25. [Renderer](#renderer)
26. [Level System](#level-system)
27. [Platform](#platform)
28. [Asset Pipeline](#asset-pipeline)
29. [Debug & Testing](#debug--testing)
30. [Recipes & Techniques](#recipes--techniques)

---

## Getting Started

### File Structure

```text
games/demo/assets/scripts/
  maps/default.sage      # Map script — runs on map load, defines HUD & pause menu
  day.sage               # Day-night config, spawn loops, loot tables
  patrol.sage            # Patrol routes and spawn callbacks
  battle/                # Battle scripts
  inventory/             # Item use scripts
  tests/test_all.sage    # Test suite
```

### How Scripts Load

1. The engine loads all `.sage` files listed in `game.json`
2. It also auto-scans `assets/scripts/` for additional `.sage` files
3. All scripts share a **global environment** — functions defined in one file are callable from any other
4. After all files load, the engine calls `map_init()` if it exists

### Hello World

```sage
proc map_init():
    log("Hello from SageLang!")
    say("Elder", "Welcome to the village!")
    ui_notify("Game started!", 3)
```

---

## Language Basics

SageLang uses Python-like syntax with `proc` for functions, `let` for variables, and indentation-based blocks.

```sage
# Variables
let x = 42
let name = "Elder"
let alive = true

# Functions
proc greet(who):
    say(who, "Hello!")
    log("Greeted " + who)

# Conditionals
if is_night():
    log("It's dark out")
elif is_day():
    log("The sun is up")

# Loops
let i = 0
while i < 10:
    log(str(i))
    i = i + 1

# Built-in functions
let r = random(1, 100)
let c = clamp(x, 0, 50)
let s = str(42)          # "42"
```

---

## Engine Core

| Function | Description |
|----------|-------------|
| `log(msg)` | Print to debug log |
| `debug(msg)` | Debug-level log |
| `info(msg)` | Info-level log |
| `warn(msg)` | Warning-level log |
| `error(msg)` | Error-level log |
| `print(a, b, ...)` | Print multiple values on one line |
| `random(min, max)` | Random integer in [min, max] |
| `clamp(val, min, max)` | Clamp value to range |
| `str(value)` | Convert number to string |
| `set_flag(name, value)` | Set a persistent flag (number, string, or bool) |
| `get_flag(name)` | Get a flag value (returns 0 if unset) |
| `assert_true(cond, msg)` | Assert condition is true (logs error if false) |

### Flags

Flags are the primary way to store persistent game state across scripts.

```sage
# Track quest progress
set_flag("met_elder", true)
set_flag("quest_stage", 2)
set_flag("player_name", "Hero")

# Check flags
if get_flag("met_elder") == true:
    say("Elder", "Welcome back!")
    let stage = get_flag("quest_stage")
    if stage == 2:
        say("Elder", "Go find the crystal!")
```

---

## Player API

| Function | Description |
|----------|-------------|
| `get_player_x()` | Player X position (pixels) |
| `get_player_y()` | Player Y position (pixels) |
| `set_player_pos(x, y)` | Teleport player + center camera |
| `get_player_speed()` | Current movement speed |
| `set_player_speed(speed)` | Set movement speed (default: 120) |
| `get_player_hp()` | Current HP |
| `get_player_hp_max()` | Max HP |
| `set_player_hp(hp)` | Set current HP |
| `get_player_atk()` | Attack power |
| `set_player_atk(atk)` | Set attack power |
| `get_player_def()` | Defense |
| `set_player_def(def)` | Set defense |
| `get_player_level()` | Current level |
| `get_player_xp()` | Current XP |
| `add_player_xp(amount)` | Add XP |
| `get_player_dir()` | Facing direction (0=down, 1=up, 2=left, 3=right) |
| `set_player_dir(dir)` | Set facing direction |
| `get_ally_hp()` | Ally current HP |
| `set_ally_hp(hp)` | Set ally HP |
| `get_ally_atk()` | Ally attack power |

```sage
# Heal player to full
set_player_hp(get_player_hp_max())

# Speed boost
set_player_speed(200)

# Teleport to town square
set_player_pos(320, 256)
```

---

## NPC API

### Spawning

```sage
# spawn_npc(name, x, y, dir, hostile, sprite, hp, atk, speed, aggro_range)
spawn_npc("Guard", 400, 300, 0, false, "assets/textures/guard.png", 0, 0, 30, 0)
spawn_npc("Slime", 600, 400, 0, true, "assets/textures/cf_slime.png", 20, 5, 40, 120)
```

The 6th argument can be a **string** (texture path for atlas cache) or a **number** (legacy atlas index).

Optional args 11-12 specify grid cell dimensions:

```sage
# Spawn with explicit grid size (args 11, 12)
spawn_npc("Boss", 400, 300, 0, true, "assets/textures/boss_64x64.png", 500, 40, 10, 200, 64, 64)

# Same texture, different grids — cached separately
spawn_npc("SmallGuy", 100, 100, 0, false, "assets/textures/multi.png", 0, 0, 20, 0, 16, 16)
spawn_npc("BigGuy", 200, 200, 0, false, "assets/textures/multi.png", 0, 0, 20, 0, 48, 48)
```

### Grid Size Control

| Function | Description |
|----------|-------------|
| `npc_set_grid(name, grid_w, grid_h)` | Change sprite grid size at runtime |

The grid size determines how the sprite sheet is divided into cells (3 cols × 3 rows) and the render size (grid × 1.5). Grid size of 0 = auto-detect from texture.

| Grid | Render Size | Use Case |
|------|-------------|----------|
| 16×16 | 24×24 | Tiny sprites |
| 32×32 | 48×48 | Standard NPCs |
| 32×48 | 48×72 | Tall characters (mage) |
| 64×64 | 96×96 | Large creatures, bosses |

### Runtime Queries

| Function | Description |
|----------|-------------|
| `npc_count()` | Number of NPCs in current level |
| `npc_exists(name)` | Check if NPC exists |
| `npc_get_x(name)` | NPC X position |
| `npc_get_y(name)` | NPC Y position |
| `npc_set_pos(name, x, y)` | Set NPC position |
| `npc_get_speed(name)` | NPC movement speed |
| `npc_set_speed(name, speed)` | Set NPC speed |
| `npc_get_dir(name)` | NPC facing direction |
| `npc_set_dir(name, dir)` | Set NPC direction |
| `npc_set_hostile(name, bool)` | Set hostile flag |
| `npc_is_hostile(name)` | Check if hostile |
| `npc_remove(name)` | Remove NPC from the world |

```sage
# Move Elder to the town square
npc_set_pos("Elder", 320, 256)

# Make guard face right
npc_set_dir("Guard", 3)

# Check if enemy is alive
if npc_exists("Slime"):
    log("Slime is still alive!")
```

---

## Sprite Manipulation

Per-sprite scaling, tinting, and flipping — independent per NPC, object, player, and ally.

### NPC Sprite Control

| Function | Description |
|----------|-------------|
| `npc_set_scale(name, scale)` | Set NPC render scale (0.1-10.0, default 1.0) |
| `npc_get_scale(name)` | Get NPC scale |
| `npc_set_tint(name, r, g, b, a)` | Set NPC color tint/alpha |
| `npc_set_flip(name, flip_h)` | Flip NPC sprite horizontally |

### Player & Ally Scale

| Function | Description |
|----------|-------------|
| `set_player_scale(scale)` | Scale player sprite |
| `get_player_scale()` | Get player scale |
| `set_ally_scale(scale)` | Scale party follower sprites |

### Object Scale & Tint

| Function | Description |
|----------|-------------|
| `set_object_scale(x, y, scale)` | Scale nearest object to (x,y) |
| `set_object_tint(x, y, r, g, b, a)` | Tint nearest object |

```sage
# Giant boss fight — boss is 3x size, minions are half
spawn_npc("Dragon", 400, 300, 0, true, "assets/textures/cf_skeleton.png", 500, 40, 15, 200)
npc_set_scale("Dragon", 3.0)

spawn_npc("Imp", 500, 350, 0, true, "assets/textures/cf_slime.png", 15, 3, 60, 80)
npc_set_scale("Imp", 0.5)

# Shrink player to fit inside a house
set_player_scale(0.7)
set_ally_scale(0.7)

# Make a house object bigger
place_object(400, 300, "House")
set_object_scale(400, 300, 2.0)

# Tint an NPC red (damaged/angry)
npc_set_tint("Guard", 1, 0.3, 0.3, 1)

# Ghost effect — semi-transparent NPC
npc_set_tint("Ghost", 0.6, 0.6, 1.0, 0.5)
```

---

## Inventory & Items

| Function | Description |
|----------|-------------|
| `add_item(id, name, qty, type, desc, heal, dmg, element, sage_func)` | Add item to inventory |
| `remove_item(id, qty)` | Remove items |
| `has_item(id)` | Check if player has item |
| `item_count(id)` | Get quantity of item |

**Item Types:** `"consumable"`, `"weapon"`, `"key"`

```sage
# Add a healing potion
add_item("potion", "Health Potion", 3, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")

# Add a weapon
add_item("iron_sword", "Iron Sword", 1, "weapon", "A sturdy blade", 0, 15, "physical", "")

# Use callback — called automatically when item is used from inventory
proc use_potion():
    set_player_hp(get_player_hp() + 50)
    ui_notify("Healed 50 HP!", 2)
```

---

## Shop / Merchant

| Function | Description |
|----------|-------------|
| `add_shop_item(id, name, price, type, desc, heal, dmg, element, sage_func)` | Add item to pending shop list |
| `open_shop(merchant_name)` | Open the shop UI with pending items |
| `set_gold(amount)` | Set player gold |
| `get_gold()` | Get player gold |

```sage
proc open_merchant():
    add_shop_item("potion", "Health Potion", 50, "consumable", "Heals 50 HP", 50, 0, "", "")
    add_shop_item("bread", "Bread", 10, "consumable", "Restores hunger", 0, 0, "", "use_bread")
    add_shop_item("sword", "Steel Sword", 200, "weapon", "ATK +20", 0, 20, "", "")
    open_shop("Merchant")
```

---

## Battle

| Function | Description |
|----------|-------------|
| `start_battle(enemy_name, hp, atk, sprite)` | Start a battle encounter |
| `is_in_battle()` | Check if currently in battle |
| `set_xp_formula(multiplier)` | Set XP reward multiplier |

```sage
# Start a boss fight
start_battle("Dragon", 200, 30, "assets/textures/dragon.png")

# Double XP weekend
set_xp_formula(2.0)
```

---

## Character Stats

Stats follow a Fallout S.P.E.C.I.A.L.-style system: Vitality, Arcana, Agility, Tactics, Spirit, Strength (range 1-10).

| Function | Description |
|----------|-------------|
| `get_skill(character, stat)` | Get stat value ("player" or "ally") |
| `set_skill(character, stat, value)` | Set stat (clamped to 1-10) |
| `get_skill_bonus(character, bonus)` | Get derived bonus |

**Stats:** `"vitality"`, `"arcana"`, `"agility"`, `"tactics"`, `"spirit"`, `"strength"`

**Bonuses:** `"hp"`, `"crit"`, `"defense"`, `"magic_mult"`, `"weapon_dmg"`, `"dodge"`, `"spell_mult"`

```sage
set_skill("player", "strength", 8)
let bonus = get_skill_bonus("player", "weapon_dmg")
log("Weapon damage bonus: " + str(bonus))
```

---

## Day-Night Cycle

| Function | Description |
|----------|-------------|
| `get_hour()` | Current hour (0-23) |
| `get_minute()` | Current minute (0-59) |
| `set_time(hour, minute)` | Set game time |
| `set_day_speed(multiplier)` | Time speed (1 = 1 real sec per game minute, clamped 0-100) |
| `is_day()` | True if 6:00-18:00 |
| `is_night()` | True if 18:00-6:00 |

```sage
# Night starts at 6 PM, ends at 6 AM
set_day_speed(6)  # 6x speed: ~4 minutes per full day

if is_night():
    set_clear_color(0.02, 0.02, 0.08)  # Dark blue sky
    spawn_loop("Skeleton", 30, 3)
```

---

## Survival System

| Function | Description |
|----------|-------------|
| `enable_survival(bool)` | Enable/disable survival meters |
| `get_hunger()` / `set_hunger(val)` | Hunger (0-100) |
| `get_thirst()` / `set_thirst(val)` | Thirst (0-100) |
| `get_energy()` / `set_energy(val)` | Energy (0-100) |
| `set_survival_rate(stat, rate)` | Depletion rate per game-minute |

```sage
enable_survival(true)
set_survival_rate("hunger", 2.0)
set_survival_rate("thirst", 2.5)
set_hunger(100)
set_thirst(100)
set_energy(100)
```

---

## UI Components

Create UI elements from script. Every component has a unique string ID for later modification.

| Function | Description |
|----------|-------------|
| `ui_label(id, text, x, y, r, g, b, a)` | Create/update a text label |
| `ui_bar(id, value, max, x, y, w, h, r, g, b, a)` | Create/update a progress bar |
| `ui_panel(id, x, y, w, h, sprite_region)` | Create/update a panel |
| `ui_image(id, x, y, w, h, icon_name)` | Create/update an image |
| `ui_set(id, property, value)` | Modify any component property |
| `ui_remove(id)` | Remove a component |
| `ui_notify(text, duration)` | Show a timed notification |

### Properties for `ui_set()`

- Labels: `"text"`, `"visible"`, `"x"`, `"y"`, `"scale"`
- Bars: `"value"`, `"max"`, `"visible"`
- Panels/Images: `"visible"`, `"x"`, `"y"`

```sage
# Create an HP bar
ui_panel("hp_bg", 10, 10, 200, 30, "panel_mini")
ui_bar("hp_bar", 100, 100, 15, 18, 170, 14, 0.2, 0.8, 0.2, 1)
ui_label("hp_text", "HP: 100/100", 15, 12, 1, 1, 1, 1)

# Update HP display
proc update_hp():
    let hp = get_player_hp()
    let max = get_player_hp_max()
    ui_set("hp_bar", "value", hp)
    ui_set("hp_text", "text", "HP: " + str(hp) + "/" + str(max))

# Show a pickup notification
ui_notify("Found a Health Potion!", 3)
```

---

## HUD Configuration

| Function | Description |
|----------|-------------|
| `hud_set(property, value)` | Set HUD config property |
| `hud_get(property)` | Get HUD config property |

**Properties:** `"scale"`, `"show_player"`, `"show_time"`, `"show_inventory"`, `"show_survival"`, `"show_minimap"`, `"screen_w"`, `"screen_h"`, `"inv_max_slots"`, `"player_w"`, `"player_h"`, `"time_w"`, `"time_h"`

```sage
hud_set("scale", 2.0)
hud_set("show_survival", true)
let sw = hud_get("screen_w")
```

---

## Camera

| Function | Description |
|----------|-------------|
| `get_camera_x()` / `get_camera_y()` | Camera position |
| `set_camera_pos(x, y)` | Set camera position |
| `camera_center(x, y)` | Center camera on point |
| `camera_follow(x, y, speed)` | Smooth follow to point |
| `camera_shake(intensity, duration)` | Camera shake effect (intensity 0-50, duration 0-10) |
| `camera_set_zoom(zoom)` | Set zoom level (min 0.1) |
| `camera_get_zoom()` | Get current zoom |

```sage
# Cinematic zoom on boss
camera_set_zoom(2.0)
camera_center(500, 400)
camera_shake(8, 0.5)
```

---

## Audio

| Function | Description |
|----------|-------------|
| `play_music(path, loop)` | Play background music |
| `stop_music()` | Stop music |
| `pause_music()` | Pause music |
| `resume_music()` | Resume music |
| `set_music_volume(vol)` | Set music volume (0.0-2.0) |
| `set_master_volume(vol)` | Set master volume (0.0-2.0) |
| `play_sfx(path, vol)` | Play sound effect |
| `crossfade_music(path, duration, loop)` | Crossfade to new track |
| `is_music_playing()` | Check if music is playing |

**Security:** All paths are sanitized — absolute paths and `../` traversals are rejected.

```sage
play_music("assets/audio/village.ogg", true)
set_music_volume(0.7)

# Crossfade to battle music
crossfade_music("assets/audio/battle.ogg", 1.5, true)

# Play hit sound
play_sfx("assets/audio/hit.wav", 0.8)
```

---

## Map & World

| Function | Description |
|----------|-------------|
| `set_collision(tx, ty, type)` | Set tile collision (0=None, 1=Solid, 2=Portal) |
| `set_portal(tx, ty, target_map, tx2, ty2, label)` | Place a portal |
| `remove_portal(tx, ty)` | Remove a portal |
| `place_object(x, y, stamp_name)` | Place a world object |
| `remove_object(x, y)` | Remove nearest object |
| `set_tile(layer, tx, ty, tile_id)` | Set a tile |
| `drop_item(x, y, id, name, type, desc, heal, dmg, element, func)` | Drop item in world |
| `add_loot(enemy, id, name, chance, type, desc, heal, dmg, element, func)` | Add loot table entry |
| `clear_loot(enemy)` | Clear loot table for enemy |
| `npc_set_despawn_day(name, bool)` | Hostile NPCs disappear at dawn |
| `npc_set_loot(name, func)` | Set loot callback for NPC death |

```sage
# Create a portal to the cave
set_portal(10, 15, "cave.json", 2, 2, "Cave Entrance")

# Drop loot when skeleton dies
add_loot("Skeleton", "bone", "Bone", 0.8, "consumable", "A dusty bone", 0, 0, "", "")
add_loot("Skeleton", "gold_coin", "Gold Coin", 0.5, "consumable", "", 0, 0, "", "")

# Night-only enemies that drop loot and despawn at dawn
npc_set_despawn_day("Skeleton", true)
```

---

## Spawn System

| Function | Description |
|----------|-------------|
| `spawn_loop(npc_name, interval, max_count)` | Start periodic spawning |
| `stop_spawn_loop(npc_name)` | Stop spawning |
| `set_spawn_area(name, x1, y1, x2, y2)` | Set spawn area rectangle |
| `set_spawn_callback(name, func)` | Function called on each spawn |
| `set_spawn_time(name, start_hour, end_hour)` | Only spawn during these hours |

```sage
# Spawn skeletons at night in the graveyard
spawn_loop("Skeleton", 30, 5)
set_spawn_area("Skeleton", 400, 400, 700, 600)
set_spawn_time("Skeleton", 18, 6)  # 6 PM to 6 AM
set_spawn_callback("Skeleton", "on_skeleton_spawn")

proc on_skeleton_spawn():
    let name = get_flag("spawned_npc")  # Auto-set by engine
    npc_set_despawn_day(name, true)
    npc_add_waypoint(name, 500, 450)
    npc_add_waypoint(name, 600, 550)
    npc_set_route(name, "patrol")
    npc_start_route(name)
```

---

## NPC Routes & Schedules

### Routes

| Function | Description |
|----------|-------------|
| `npc_move_to(name, x, y)` | A* pathfind to position |
| `npc_add_waypoint(name, x, y)` | Add patrol waypoint |
| `npc_set_route(name, mode)` | Set route mode: `"patrol"`, `"once"`, `"pingpong"` |
| `npc_start_route(name)` | Start following route |
| `npc_stop_route(name)` | Stop route |
| `npc_clear_route(name)` | Clear all waypoints |

### Schedules

| Function | Description |
|----------|-------------|
| `npc_set_schedule(name, start_hour, end_hour)` | NPC visible only during these hours |
| `npc_clear_schedule(name)` | Remove schedule (always visible) |
| `npc_set_spawn_point(name, x, y)` | Where NPC appears when schedule activates |

```sage
# Merchant works 8 AM to 6 PM
npc_set_schedule("Merchant", 8, 18)

# Guard patrols the wall
npc_add_waypoint("Guard", 100, 200)
npc_add_waypoint("Guard", 300, 200)
npc_add_waypoint("Guard", 300, 400)
npc_add_waypoint("Guard", 100, 400)
npc_set_route("Guard", "patrol")
npc_start_route("Guard")
```

---

## NPC Interactions

| Function | Description |
|----------|-------------|
| `npc_on_meet(npc1, npc2, callback, radius, repeatable)` | Trigger when two NPCs meet |
| `npc_set_meet_radius(npc1, npc2, radius)` | Change trigger radius |
| `npc_face_each_other(npc1, npc2)` | Make two NPCs face each other |

```sage
# Elder and Merchant have a conversation when they meet
npc_on_meet("Elder", "Merchant", "elder_meets_merchant", 50, false)

proc elder_meets_merchant():
    npc_face_each_other("Elder", "Merchant")
    say("Elder", "Good morning!")
    say("Merchant", "Fine day for business!")
```

---

## Screen Effects

| Function | Description |
|----------|-------------|
| `screen_shake(intensity, duration)` | Shake the screen |
| `screen_flash(r, g, b, a, duration)` | Flash overlay color |
| `screen_fade(r, g, b, target_alpha, duration)` | Fade to/from color |

```sage
# Explosion effect
screen_shake(10, 0.5)
screen_flash(1, 0.8, 0.2, 0.8, 0.3)

# Fade to black
screen_fade(0, 0, 0, 1, 1.0)
```

---

## Tile Map & Rotation

### Queries

| Function | Description |
|----------|-------------|
| `get_map_width()` | Map width in tiles |
| `get_map_height()` | Map height in tiles |
| `get_tile_size()` | Tile size in pixels |
| `get_layer_count()` | Number of tile layers |
| `get_tile(layer, x, y)` | Get tile ID at position (without rotation) |
| `is_solid(tx, ty)` | Check if tile is solid |
| `is_solid_world(wx, wy)` | Check if world position is solid |

### Tile Rotation & Flip

| Function | Description |
|----------|-------------|
| `set_tile_rotation(layer, tx, ty, rot)` | Set rotation: 0=0°, 1=90°, 2=180°, 3=270° |
| `get_tile_rotation(layer, tx, ty)` | Get tile rotation (0-3) |
| `set_tile_flip(layer, tx, ty, flip_h, flip_v)` | Flip tile horizontally/vertically |
| `set_tile_ex(layer, tx, ty, id, rot, flip_h, flip_v)` | Set tile with full control |

Rotation is encoded in the upper bits of the tile value and persists in saved maps automatically.

```sage
let w = get_map_width()
let h = get_map_height()
log("Map size: " + str(w) + "x" + str(h))

# Check walkability
if is_solid_world(500, 300) == false:
    set_player_pos(500, 300)

# Rotate a wall tile 90 degrees
set_tile_rotation(0, 5, 3, 1)

# Place a tile with 180° rotation and horizontal flip
set_tile_ex(0, 10, 5, 73, 2, true, false)

# Build a rotated corner from the same tile
set_tile_ex(0, 0, 0, 21, 0, false, false)  # Top-left
set_tile_ex(0, 1, 0, 21, 1, false, false)  # Top-right (90°)
set_tile_ex(0, 0, 1, 21, 3, false, false)  # Bottom-left (270°)
set_tile_ex(0, 1, 1, 21, 2, false, false)  # Bottom-right (180°)
```

**Editor hotkeys:** Press **R** to cycle rotation (0°→90°→180°→270°), **Shift+R** to toggle horizontal flip. The eyedrop tool (I) picks up rotation from existing tiles.

---

## Input

| Function | Description |
|----------|-------------|
| `is_key_held(action)` | Check if action is held |
| `is_key_pressed(action)` | Check if action was just pressed |
| `get_mouse_x()` | Mouse X position |
| `get_mouse_y()` | Mouse Y position |

**Actions:** `"up"`, `"down"`, `"left"`, `"right"`, `"confirm"`, `"cancel"`, `"menu"`, `"run"`

```sage
if is_key_pressed("confirm"):
    log("Player pressed confirm!")
```

---

## Dialogue

| Function | Description |
|----------|-------------|
| `say(speaker, text)` | Show dialogue line |
| `set_dialogue_speed(chars_per_sec)` | Typewriter speed |
| `set_dialogue_scale(scale)` | Text scale |

```sage
say("Elder", "The crystal lies beyond the forest.")
say("Elder", "Take this sword — you'll need it.")
add_item("crystal_sword", "Crystal Sword", 1, "weapon", "Enchanted blade", 0, 25, "light", "")
```

---

## Renderer

| Function | Description |
|----------|-------------|
| `set_clear_color(r, g, b)` | Set background clear color |

```sage
# Dark dungeon atmosphere
set_clear_color(0.02, 0.02, 0.05)
```

---

## Level System

| Function | Description |
|----------|-------------|
| `load_level(id, map_path)` | Load a map file into cache |
| `switch_level(id)` | Switch to a loaded level |
| `switch_level_at(id, x, y)` | Switch level + set player position |
| `preload_level(id, map_path)` | Same as load_level (preload alias) |
| `unload_level(id)` | Free a cached level |
| `get_active_level()` | Get current level ID |
| `is_level_loaded(id)` | Check if level is in cache |
| `get_level_count()` | Number of loaded levels |
| `set_level_spawn(id, x, y)` | Set default spawn point for level |
| `level_get_npc_count(id)` | NPC count in a background level |

```sage
# Preload the cave level
preload_level("cave", "assets/maps/cave.json")

# Transition when player enters portal
proc enter_cave():
    switch_level_at("cave", 64, 64)
```

Portal tiles auto-trigger level switches — just set them up with `set_portal()`.

---

## Platform

| Constant/Function | Description |
|----------|-------------|
| `PLATFORM` | `"linux"`, `"windows"`, or `"android"` |
| `IS_ANDROID` | Boolean |
| `IS_DESKTOP` | Boolean |
| `IS_QUEST` | Boolean (Meta Quest detected at runtime) |
| `get_screen_w()` | Native screen width |
| `get_screen_h()` | Native screen height |

```sage
if IS_ANDROID:
    hud_set("scale", 2.5)
elif IS_DESKTOP:
    hud_set("scale", 1.5)
```

---

## Debug & Testing

| Function | Description |
|----------|-------------|
| `log(msg)` | Script-level log |
| `debug(msg)` | Debug log |
| `info(msg)` | Info log |
| `warn(msg)` | Warning log |
| `error(msg)` | Error log |
| `print(a, b, ...)` | Print multiple values |
| `assert_true(cond, label)` | Test assertion |

### Running Tests

```bash
# From command line
./twilight_game_binary --test

# From F4 debug console
run_all_tests()
```

### Writing Tests

```sage
proc test_my_feature():
    set_flag("test_val", 42)
    assert_true(get_flag("test_val") == 42, "flag roundtrip")

    add_item("test_item", "Test", 3, "consumable", "", 0, 0, "", "")
    assert_true(item_count("test_item") == 3, "item count")
    remove_item("test_item", 3)
    assert_true(has_item("test_item") == false, "item removed")
```

---

## Complete Example: Village Map Script

```sage
# assets/scripts/maps/village.sage

proc map_init():
    # ── NPCs ──
    spawn_npc("Elder", 320, 256, 0, false, "assets/textures/cf_player.png", 0, 0, 20, 0)
    spawn_npc("Merchant", 512, 256, 0, false, "assets/textures/cf_player.png", 0, 0, 15, 0)
    spawn_npc("Guard", 160, 128, 0, false, "assets/textures/cf_skeleton.png", 0, 0, 25, 0)

    # ── Schedules ──
    npc_set_schedule("Merchant", 8, 18)  # Open during day
    npc_set_schedule("Guard", 18, 6)     # Night watch

    # ── Patrol ──
    npc_add_waypoint("Guard", 160, 128)
    npc_add_waypoint("Guard", 480, 128)
    npc_add_waypoint("Guard", 480, 400)
    npc_add_waypoint("Guard", 160, 400)
    npc_set_route("Guard", "patrol")
    npc_start_route("Guard")

    # ── Portal to Forest ──
    set_portal(0, 10, "forest.json", 19, 10, "To Forest")

    # ── Night spawns ──
    spawn_loop("Skeleton", 45, 3)
    set_spawn_area("Skeleton", 300, 300, 600, 500)
    set_spawn_time("Skeleton", 20, 5)

    # ── Loot ──
    add_loot("Skeleton", "bone", "Bone", 0.6, "consumable", "A dusty bone", 5, 0, "", "")

    # ── World setup ──
    set_day_speed(6)
    set_clear_color(0.05, 0.08, 0.12)

    # ── HUD ──
    ui_panel("hp_bg", 8, 8, 200, 30, "panel_mini")
    ui_bar("hud_hp", 100, 100, 13, 16, 170, 14, 0.2, 0.8, 0.2, 1)
    ui_label("hp_label", "HP", 15, 10, 1, 1, 1, 1)

    log("Village initialized!")
```

---

## Weather System

Full weather control with particles, lighting effects, and atmospheric overlays.

### Quick Presets

| Function | Description |
|----------|-------------|
| `set_weather(type, intensity)` | Set weather preset (0.0-1.0 intensity) |
| `get_weather()` | Returns current type: `"clear"`, `"rain"`, `"storm"`, `"snow"`, `"fog"`, `"cloudy"` |

**Types:** `"clear"`, `"rain"`, `"storm"` (rain+lightning+wind), `"snow"`, `"fog"`, `"cloudy"` (clouds+god rays)

```sage
set_weather("storm", 0.9)    # Thunderstorm
set_weather("snow", 0.5)     # Gentle snowfall
set_weather("cloudy", 0.7)   # Overcast with god rays
set_weather("clear")         # Reset everything
```

### Individual Controls

| Function | Description |
|----------|-------------|
| `set_rain(active, intensity)` | Rain particles (0.0-1.0) |
| `set_snow(active, intensity)` | Snow flakes (0.0-1.0) |
| `set_lightning(active, interval, chance)` | Lightning bolts (seconds between, probability 0-1) |
| `set_clouds(active, density, speed, direction)` | Cloud shadows (density 0-1, speed px/s, direction degrees) |
| `set_god_rays(active, intensity, count)` | Light beams through clouds |
| `set_fog(active, density, r, g, b)` | Fog overlay with custom color |
| `set_wind(strength, direction)` | Wind affects rain angle, snow drift, clouds (0-1, degrees) |
| `set_rain_color(r, g, b, a)` | Custom rain color |
| `is_raining()` | Check if rain active |
| `is_snowing()` | Check if snow active |

```sage
# Custom storm with colored rain
set_rain(true, 0.7)
set_rain_color(0.5, 0.6, 1.0, 0.4)  # Blue-tinted rain
set_lightning(true, 5, 0.6)           # Every 5s, 60% chance
set_clouds(true, 0.6, 25, 45)         # Dense clouds moving NE
set_wind(0.5, 30)                     # Moderate wind
set_fog(true, 0.2, 0.3, 0.3, 0.4)   # Light dark fog

# Blood rain (horror)
set_rain(true, 0.6)
set_rain_color(0.8, 0.1, 0.1, 0.5)

# Blizzard
set_snow(true, 1.0)
set_wind(0.8, 60)
set_fog(true, 0.5, 0.9, 0.9, 0.95)

# Volcanic ash
set_snow(true, 0.4)
set_fog(true, 0.3, 0.4, 0.35, 0.3)

# Dynamic weather based on time
proc check_weather():
    let h = get_hour()
    if h >= 5 and h < 8:
        set_fog(true, 0.4)
        set_god_rays(true, 0.3, 4)
    elif h >= 19:
        if random(1, 10) > 7:
            set_weather("rain", 0.4)
```

See `weather.sage` for a complete library of presets (forest, desert, mountain, haunted, cave, etc.).

---

## Asset Pipeline

### Multi-Resolution Scaling

Generate 2x/3x versions of all textures for different detail levels:

```bash
# Generate 2x and 3x scaled assets (nearest-neighbor, pixel-perfect)
python3 tools/scale_assets.py games/demo 2 3

# Scale a single file
python3 tools/scale_assets.py --file games/demo/assets/textures/mage_player.png 2 3 4
```

Output: `assets/textures/2x/mage_player.png`, `assets/textures/3x/mage_player.png`

### Per-Level Zoom

Different levels can use different zoom levels for varying detail:

```sage
# Forest overworld — wide view
proc forest_init():
    set_level_zoom("forest.json", 1.0)

# House interior — close-up detail
proc house_inside_init():
    set_level_zoom("house_inside.json", 2.0)

# Dungeon — medium zoom
proc dungeon_init():
    set_level_zoom("dungeon.json", 1.5)
```

Zoom auto-applies when switching levels via portals.

---

## Recipes & Techniques

### Boss Fight with Mixed Scales

```sage
proc start_boss_fight():
    # Giant dragon boss
    spawn_npc("Dragon", 400, 250, 0, true, "assets/textures/cf_skeleton.png", 500, 40, 10, 250)
    npc_set_scale("Dragon", 3.5)
    npc_set_tint("Dragon", 1.0, 0.3, 0.1, 1.0)  # Fiery red

    # Tiny imp minions
    let i = 0
    while i < 4:
        let ix = 300 + i * 80
        spawn_npc("Imp_" + str(i), ix, 400, 0, true, "assets/textures/cf_slime.png", 15, 3, 70, 80)
        npc_set_scale("Imp_" + str(i), 0.5)
        i = i + 1

    camera_set_zoom(1.5)
    screen_shake(8, 0.5)
    ui_notify("A dragon appears!", 3)
```

### Interior Level with Scaled Characters

```sage
proc enter_house():
    set_level_zoom("house.json", 2.0)     # Zoom camera in
    set_player_scale(0.6)                   # Shrink player
    set_ally_scale(0.6)                     # Shrink party
    set_clear_color(0.12, 0.10, 0.06)      # Warm lighting

    # Place furniture
    place_object(128, 96, "Table")
    place_object(64, 64, "Cabinet")
    place_object(224, 64, "Bed")
    place_object(160, 32, "Fireplace")

proc exit_house():
    set_player_scale(1.0)
    set_ally_scale(1.0)
```

### Day-Night Event System

```sage
proc check_time_events():
    let h = get_hour()
    if h == 6:
        ui_notify("The sun rises...", 3)
        set_clear_color(0.05, 0.08, 0.12)
    elif h == 18:
        ui_notify("Night falls...", 3)
        set_clear_color(0.02, 0.02, 0.06)
        screen_fade(0, 0, 0, 0.3, 2.0)
    elif h == 20:
        spawn_loop("Ghost", 30, 3)
        set_spawn_time("Ghost", 20, 5)
```

### Quest System with Flags

```sage
proc talk_to_elder():
    let stage = get_flag("main_quest")
    if stage == 0:
        say("Elder", "The crystal has been stolen!")
        say("Elder", "Please, find it in the cave to the east.")
        set_flag("main_quest", 1)
        ui_notify("Quest: Find the Crystal", 4)
    elif stage == 1:
        say("Elder", "Have you found the crystal yet?")
        if has_item("crystal"):
            say("Elder", "You found it! Thank you!")
            remove_item("crystal", 1)
            add_player_xp(100)
            set_gold(get_gold() + 500)
            set_flag("main_quest", 2)
            ui_notify("Quest Complete! +500 gold", 4)
    elif stage == 2:
        say("Elder", "The village is safe, thanks to you.")
```

### Dynamic Shop with Inventory Check

```sage
proc open_potion_shop():
    add_shop_item("potion", "Health Potion", 50, "consumable", "Heals 50 HP", 50, 0, "", "use_potion")
    add_shop_item("ether", "Ether", 80, "consumable", "Restores magic", 20, 0, "", "use_ether")

    # Only sell sword if player doesn't have one
    if has_item("iron_sword") == false:
        add_shop_item("iron_sword", "Iron Sword", 300, "weapon", "ATK +15", 0, 15, "", "")

    # Night-only items
    if is_night():
        add_shop_item("torch", "Torch", 20, "consumable", "Light in darkness", 0, 0, "", "use_torch")

    open_shop("Potion Shop")
```

### Custom HUD with Script UI

```sage
proc setup_custom_hud():
    let sw = hud_get("screen_w")
    let sh = hud_get("screen_h")

    # HP bar with panel background
    ui_panel("hp_bg", 8, 8, 220, 35, "panel_mini")
    ui_image("hp_icon", 14, 14, 20, 20, "icon_heart_red")
    ui_bar("hp_bar", 100, 100, 40, 16, 180, 16, 0.2, 0.8, 0.2, 1)
    ui_label("hp_text", "100/100", 150, 12, 1, 1, 1, 1)
    ui_set("hp_text", "scale", 0.8)

    # XP bar at bottom
    ui_bar("xp_bar", 0, 100, 0, sh - 6, sw, 6, 0.3, 0.5, 0.9, 0.7)
    ui_set("xp_bar", "layer", 10)

    # Minimap-style location label
    ui_label("location", "Enchanted Forest", sw - 200, sh - 30, 0.8, 0.8, 0.8, 0.6)
    ui_set("location", "scale", 0.7)
```

### Animated UI Effects

```sage
# Pulse a warning label using opacity
proc pulse_warning():
    ui_label("warning", "LOW HP!", 400, 300, 1, 0.2, 0.2, 1)
    ui_set("warning", "scale", 1.5)
    ui_set("warning", "layer", 100)
    # Engine will render with current opacity — use a timer in update loop

# Screen transition effect
proc scene_transition():
    screen_fade(0, 0, 0, 1, 0.5)     # Fade to black
    # After fade completes, switch level and fade back
    # (In practice, call switch_level then screen_fade back)
```

### Rotated Tile Patterns

```sage
# Create a symmetrical floor pattern using rotation
proc build_floor_pattern(cx, cy):
    let tile = 73  # Stone tile
    # Four corners with rotations
    set_tile_ex(0, cx, cy, tile, 0, false, false)
    set_tile_ex(0, cx+1, cy, tile, 1, false, false)
    set_tile_ex(0, cx, cy+1, tile, 3, false, false)
    set_tile_ex(0, cx+1, cy+1, tile, 2, false, false)

# Create a border with flipped tiles
proc build_wall_border(x1, y1, x2, y2):
    let wall = 281  # Stone wall tile
    let i = x1
    while i <= x2:
        set_tile_ex(0, i, y1, wall, 0, false, false)   # Top
        set_tile_ex(0, i, y2, wall, 2, false, false)    # Bottom (180°)
        i = i + 1
    let j = y1
    while j <= y2:
        set_tile_ex(0, x1, j, wall, 3, false, false)    # Left (270°)
        set_tile_ex(0, x2, j, wall, 1, false, false)    # Right (90°)
        j = j + 1
```

### NPC Ghost Effect

```sage
proc make_ghost(name):
    npc_set_tint(name, 0.5, 0.5, 1.0, 0.4)   # Blue, semi-transparent
    npc_set_scale(name, 1.2)                    # Slightly larger than normal

proc make_poisoned(name):
    npc_set_tint(name, 0.3, 0.8, 0.3, 1.0)   # Green tint

proc make_enraged(name):
    npc_set_tint(name, 1.0, 0.2, 0.2, 1.0)   # Red tint
    npc_set_scale(name, 1.3)                    # Bigger when angry
```

### Multi-Level Dungeon

```sage
proc setup_dungeon():
    # Preload all dungeon floors
    preload_level("dungeon_f1", "assets/maps/dungeon_f1.json")
    preload_level("dungeon_f2", "assets/maps/dungeon_f2.json")
    preload_level("dungeon_f3", "assets/maps/dungeon_f3.json")

    # Set zoom for cramped dungeon feel
    set_level_zoom("dungeon_f1", 1.8)
    set_level_zoom("dungeon_f2", 1.8)
    set_level_zoom("dungeon_f3", 2.0)  # Boss floor is more zoomed

    # Each floor has portals to the next
    # Placed in the map JSON, auto-triggered on player step
```

---

Twilight Engine v2.1.0 — 20,227 lines C++, 231 API functions, 40 modules, 6 editor files, 6 maps, 8 tilesets, 4 platforms
