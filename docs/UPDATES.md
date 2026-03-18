# Twilight Engine Updates

## v3.3.0 — March 17, 2026

### Intelligent Map Script Management
- **Upsert system** — Property edits (move, resize, color, scale) now **replace existing lines** instead of appending duplicates. Moving a panel 50 times produces exactly 1 x-line and 1 y-line
- **Component deletion** — Deleting a UI element removes ALL its lines (creation + every `ui_set`) instead of appending `ui_remove()` while leaving dead code
- **Structured line storage** — `ScriptLine` struct tracks `component_id` + `property` per line. `upsert_map_script()` finds and replaces; `remove_component_script()` erases all lines for an ID
- **Load-time parsing** — Existing `.sage` scripts are parsed on load to extract component IDs from `ui_label/panel/bar/image/set/remove` calls, enabling in-place editing of previously saved scripts

### Material Design UI Overhaul
- **Unified color palette** across all platforms (desktop + Android):
  - Amber 300 (#FFD54F) for selection/highlights/gold
  - Blue-Grey 200/900 for text hierarchy and panel backgrounds
  - Green 700 / Amber 800 / Red 700 for HP bar states
  - Red 400 for destructive actions (Quit stays red even when selected)
  - Light Blue 400 for accents and interaction hints
- **Touch controls** (Android): Proper disc rendering (9-quad approximation), drop shadows, ring outlines, Material-colored buttons (green confirm, red cancel, orange menu), proper A/B letter shapes
- **HUD** (all platforms): Bar shine effect, survival bar labels (H/T/E), dark bar backgrounds, quantity badges with backgrounds, amber player dot glow on minimap, NPC dot colors match palette
- **Pause menu**: Amber selected items, Blue-Grey unselected, dim Red quit, cinematic vignette overlay
- **NPC labels**: Drop shadows, top-edge highlights, left accent bar on interaction hints
- **Debug overlay**: Shadow panel, Light Blue accent bars, FPS color-coded (green/amber/red), shows trigger + trail counts
- **default.sage + hud.sage**: Fully rewritten with Material palette, fantasy icon references (fi_*), proper bar backgrounds

### Modernized Icon System
- All C++ render code migrated from `icon_sword/icon_coin/icon_potion` atlas names to `fi_*` fantasy icon indices
- Editor icon presets updated to fi_* range (23 icons)
- Forest map script cleaned: removed old `flat_blue`/`flat_orange` panel overrides

### Test Suite Expansion
- **40 new assertions** (101 → 141 total) across 17 new test sections
- Tests for: collision (10), raycast (2), triggers, coroutines, FSM (2), checkpoints, combos, trails, skeletons, input replay, post-processing, audio buses, parallax extended (3), camera zoom, 9-slice (2), layer sorting, bar show_text, colliders

### README Modularization
- README reduced from 509 to ~110 lines — concise overview with feature table
- Detailed sections extracted to dedicated docs/:
  - [docs/EDITOR.md](EDITOR.md) — Editor features, new map dialog, parallax panel
  - [docs/CONTROLS.md](CONTROLS.md) — All platform control schemes
  - [docs/BUILD.md](BUILD.md) — Build instructions + project structure
  - [docs/TOOLS.md](TOOLS.md) — Python tool descriptions

### Stats
- 28,474 lines C++ (109 files, excluding third-party)
- 323 API functions across 56 modules (9 script files)
- 5,278 lines scripting engine, 6,235 lines game framework
- 5,209 lines editor (11 files)
- 141 test assertions across 50 categories
- 45 parallax backgrounds, 13 Python tools

---

## v3.2.0 — March 17, 2026

### 27 New Engine Systems (Tier 1–4 Feature Sweep)
- **AABB / Shape Collision** — Rect, circle, convex polygon with SAT. `CollisionResult` with normal+depth for push-out. `Collider` tagged union per entity. 8 script functions
- **Raycasting** — Ray vs rect/circle/tilemap (DDA grid traversal). `line_of_sight()`. 4 script functions
- **Trigger Zones** — Rect/circle areas with `on_enter`/`on_exit`/`on_stay` callbacks. One-shot support. Updated each frame for player + all NPCs. 4 script functions
- **Coroutines** — Step-based coroutine manager: define sequences of (function, delay) pairs, start/stop/loop. Powers cutscenes and boss patterns. 6 script functions
- **Post-Processing Pipeline** — Data model + API for CRT, bloom, vignette, blur, color grading. Vulkan framebuffer backend stubbed for render-to-texture. 2 script functions
- **Sprite Animation Events** — `event` field on `AnimFrame`, `last_event` on `AnimPlayer`. Fire script callbacks on specific animation frames (e.g. frame 3 = spawn hitbox)
- **Generic State Machine** — Reusable FSM with script-driven `on_enter`/`on_update`/`on_exit` per state. 5 script functions
- **Object Pooling** — `ObjectPool<T>` template with freelist for O(1) acquire/release
- **Checkpoint / Respawn** — Named checkpoints with map ID, activate/respawn. 4 script functions
- **Combo / Input Sequence Detection** — Ring buffer of timestamped inputs, configurable timing windows. 1 script function
- **Camera Smooth Zoom** — Lerped `zoom_to(target, speed)`. Zoom-aware orthographic projection
- **Camera Perlin Shake** — Smooth noise-based shake replacing random offset. Configurable frequency and fade-out
- **Audio Bus / Mixer** — Separate volume for music, SFX, ambience, voice channels. 1 script function
- **Audio Effects** — Reverb, echo, low-pass stubs (miniaudio node graph ready). 4 script functions
- **Procedural Dungeon Generation** — BSP tree + cellular automata generators. Output feeds directly into TileMap. 4 script functions
- **Trail / Ribbon Rendering** — Polyline trail mesh with age-based width/alpha interpolation. 5 script functions
- **8-Bit Blob Auto-Tiling** — 256→47 LUT for proper corner-aware transitions (replaces 4-bit)
- **Skeleton / Bone Animation** — 2D forward kinematics: bones with parent transforms, keyframe interpolation. 5 script functions
- **Behavior Trees** — Sequence/Selector/Parallel composites, Inverter/Repeater/Cooldown decorators, Action/Condition leaves
- **Input Recording / Replay** — Binary record/playback with timestamps. 4 script functions
- **Editor Multi-Select** — Data model for multi-tile selection (Shift+click additive)
- **Visual Particle Editor** — Preview emitter + preset management stub
- **Networking** — Packet types, UDP socket stub (client/server architecture defined)
- **ECS** — Lightweight `SparseSet<T>` storage + `World` class with create/destroy/add/get/each
- **Plugin / Mod System** — `ModLoader` with directory scanning + manifest format
- **Isometric Tiles** — `iso_to_screen`/`screen_to_iso` coordinate conversion + sort key
- **Hex Tiles** — Pointy-top offset coordinates, 6-neighbor lookup, hex distance

### Platformer Backgrounds
- **Enhanced ParallaxLayer**: Per-layer tint (Vec4), auto-scroll (px/s drift), scale, pin-bottom (Y-anchor to viewport), fill-viewport (stretch sky), z-order sorting
- **8 biome presets**: forest, cave, night, sunset, snow, desert + forest_sunset, forest_trees (processed from Glitch CC0 HD assets)
- **45 parallax PNG assets** (1.4 MB) across 8 biome folders
- **Procedural generator tool** (`tools/generate_parallax_bg.py`): 6 biomes, 5 layers each, horizontally tileable, value noise mountains/trees, biome-specific features (stalactites, stars, snow caps, cacti)
- **Editor: Systems > Parallax Backgrounds**: Preset dropdown with one-click load, per-layer controls (scroll, auto-scroll, scale, tint, pin/fill/repeat), add custom layer, clear all
- **Script API**: `load_parallax_preset(biome)`, `clear_parallax()`, `parallax_count()` + 10 new `set_parallax` properties (tint, auto_scroll, scale, pin_bottom, fill_viewport, z_order)
- **Rendering rewrite**: Z-order sorted draw, pin-bottom Y calculation, fill-viewport stretching, auto-scroll accumulation, per-layer tint

### New Map Creation
- **Editor: File > New Map...** — Modal dialog with width/height/tile size + game mode dropdown (Top-Down RPG / Platformer)
- **Platformer mode**: Bottom 2 rows pre-filled with solid ground, player spawns above ground, GameType set to Platformer
- **Top-Down mode**: Completely empty map, player at center
- **Script API**: `new_map(width, height, tile_size, mode)` — mode 0=topdown, 1=platformer

### Water Reflection Fix
- **Auto-detect water tiles** (IDs 42-54) and mark reflective on every map load
- Called from `load_map_file()`, `init_game()`, and `init_game_from_manifest()`
- Previously: maps had no reflection data, so `is_reflective()` always returned false

### Stats
- 28,038 lines C++ (109 source files, excluding third-party)
- 323 API functions across 56 modules (9 script files)
- 5,277 lines scripting engine, 6,107 lines game framework
- 5,071 lines editor (11 files), 2,725 lines new system headers
- 22 new header files, 2 new script API files
- 45 parallax background PNGs across 8 biomes
- 13 Python tools

---

## v3.1.0 — March 17, 2026

### 9-Slice Panel Rendering
- **9-slice system** for ScriptUI panels: corners stay fixed-size, edges stretch in one axis, center stretches both axes
- Works with any atlas region — UV splits computed from pixel dimensions automatically
- Configurable border inset (default 16px), scales with panel scale
- Degenerate quads skipped when panel is smaller than 2x border
- Script API: `ui_set(id, "nine_slice", true)`, `ui_set(id, "border", 24)`
- Editor: 9-Slice checkbox + Border slider in panel properties
- Convenience helper `draw_ui_region_9s()` available for C++ HUD code

### Layer-Sorted UI Rendering
- All ScriptUI elements (panels, images, labels, bars) now sort by `layer` then type before drawing
- Panels render behind images, images behind labels, labels behind bars within the same layer
- Previously everything drew in vector insertion order with no layer support

### Full Property Rendering
- **Panels**: Now honor `color` (tint), `opacity`, and `scale` (multiplied into dimensions)
- **Images**: Render with separate width/height (was square-only), `tint`, `opacity`, `scale`, `flip_h`/`flip_v` (UV swap)
- **Labels**: Apply `opacity` multiplied into color alpha
- **Bars**: Apply `opacity` to both fill and background; render `show_text` overlay ("75/100" centered on bar)

### 18 Editor UI Templates (fully wired with script generation)
- **Dialogs & Windows**: Dialog Box (speaker + text), Confirm Dialog (Yes/No buttons), Toast (notification banner)
- **HUD Elements**: Quest Tracker, Status Bar, Boss HP (full-width with show_text), XP Bar (bottom-of-screen), Buff Row (5 icon slots), Location Banner (layer 10)
- **Character & Stats**: Character Stats (portrait, HP/MP/XP bars, 4 stat rows), Party HUD (3-member display), Equipment Slots (6-slot grid)
- **Menus & Overlays**: Inventory Grid (4x5 slots), Pause Menu (4 buttons), Title Screen (title + 3 options), Settings Panel (4 slider rows), Tooltip (item info, layer 15), Save Indicator (layer 18), Message Log (6 fading lines), Minimap Frame, Shop Window (4 item rows with Buy buttons), Damage Numbers (5 floating labels, layer 19), Interact Prompt (keybind hint)

### Bug Fixes
- `ui_remove()` now removes panels and images (was labels/bars only)
- Quest Tracker template now generates complete script for all sub-components (was panel only)
- Status Bar template now generates complete script for label + bar (was panel only)
- Label creation in editor now emits `ui_set("scale", 0.8)` so script matches editor visual
- Images now support viewport right-drag resize (was panels/bars only)

### Stats
- 23,486 lines C++ (85 source files, excluding third-party)
- 266 API functions across 48 modules (7 script files)
- 4,071 lines scripting engine, 4,967 lines game framework
- 4,832 lines editor (9 files), 1,356 lines UI/HUD editor alone
- 18 editor templates, 9 draw quads per 9-slice panel

---

## v3.0.0 — March 17, 2026

### 2D Platformer Mode
- **Dual game modes**: Per-level `GameType` enum (TopDown / Platformer). Switch with `set_game_type("platformer")` or the editor dropdown
- **Platformer physics**: Gravity (980 px/s^2), velocity-based movement, variable-height jumping, coyote time (0.08s), jump buffering (0.12s), max fall speed capping
- **Advanced movement**: Wall slide/jump, double jump, air dash — all toggleable via script API
- **New collision types**: OneWayUp (jump-through platforms), Slope45Up/Down (45-degree ramps), Ladder (climbable), Hazard (damage on contact)
- **Platformer camera**: Horizontal follow with facing-direction lookahead, vertical deadzone, faster ground snap
- **Moving platforms**: Waypoint-based with ping-pong/loop, velocity output for player carrying
- **Platformer enemies**: Patrol (edge detection), Jump, Fly AI patterns. Stomping detection with bounce. Contact damage with knockback. Script callbacks for stomp/contact events
- **Collectibles**: `spawn_coin()`, `spawn_collectible()` convenience functions using existing WorldDrop system
- **30 new script API functions**: Physics control, enemy AI, collectibles, moving platforms, mode switching, animation bindings
- **Editor tools**: Cycle through all 8 collision types with color-coded overlays (OneWayUp=cyan, Slope=yellow diagonal, Ladder=green stripes, Hazard=red spikes). Game Type dropdown in Systems panel
- **Demo level**: `platformer_demo.sage` with coins, patrol enemy, moving platform, portal back to RPG

### Script Engine Modularization
- **Split script_engine.cpp** (2,841 lines) into 7 focused files (4,063 lines total with new API):
  - `script_engine.cpp` (641) — Core, battle, inventory, shop, debug
  - `script_api_map.cpp` (984) — Map, camera, weather, level, audio, day-night
  - `script_api_new.cpp` (717) — Tween, particle, save, quest, equipment, lighting, parallax
  - `script_api_ui.cpp` (527) — UI, HUD, effects, renderer, survival, dialogue ext
  - `script_api_npc.cpp` (452) — NPC runtime, routes, schedules, spawn, pathfinding
  - `script_api_platformer.cpp` (397) — All platformer-specific functions
  - `script_api_player.cpp` (345) — Player, skills, input, platform detection
- Shared helpers (`find_npc_by_name`, `is_safe_path`) declared in header for cross-file access

### Stats
- 22,629 lines C++ (85 source files, excluding third-party)
- 266 API functions across 48 modules (7 script files)
- 4,063 lines scripting engine, 4,570 lines game framework
- 8 collision types, 8 platformer player states, 4 NPC AI patterns

---

## v2.6.0 — March 17, 2026

### Parallax Background System
- Multi-layer scrolling backgrounds rendered behind the tile map
- Camera-relative scrolling with configurable speed per layer (lower = further away)
- Horizontal tiling support for seamless scrolling
- Runtime texture loading via resource manager
- Script API: `add_parallax(path, scroll_x, scroll_y)`, `remove_parallax(index)`, `set_parallax(index, property, value)`

### Auto-Tiling
- 4-bit corner bitmask auto-selects from 16 transition tiles when painting terrain boundaries
- Computes which corners touch terrain B and picks the correct transition tile
- Integrates with undo/redo system
- Configure terrain pairs and transition ranges in Systems panel (F5)

### In-Game Settings Menu
- Accessible from pause menu "Settings" item (was previously a placeholder)
- Sub-menu: Music Volume, SFX Volume, Text Speed, Back
- Left/Right arrows adjust values (volume in 10% steps, text speed Slow/Normal/Fast)
- Music volume changes apply immediately

### Input Buffering (Wired)
- 150ms input buffer now actively consumed during dialogue confirm and NPC interaction
- If player presses confirm within 150ms before a prompt appears, it auto-processes
- Buffer fields marked mutable for const-correct access through InputState

### SageLang Update
- Pulled latest SageLang (6 commits): thread-safe GC, recursion depth guard, parser fix, dict deletion fix, identifier length cap, str_repeat overflow guard, VM bounds checks, shell injection prevention in asm paths

### Stats
- 21,249 lines C++ (76 source files, excluding third-party)
- 236 API functions across 41 modules
- 3,558 lines scripting engine (2,841 + 717)
- 4,220 lines game framework (5 files)
- 3,699 lines editor (7 files)

---

## v2.5.0 — March 17, 2026

### Per-Tile Reflection Grid
- New `reflection_grid_` parallel to collision grid in TileMap
- `set_reflective_at(x, y, bool)`, `is_reflective(x, y)` methods
- Serialized as `"reflection": [...]` in map JSON; defaults to all-false for existing maps
- Water reflections now only render on tiles marked as reflective
- Editor: Reflection tool (N key), reflection overlay (M key), undo/redo/clipboard support
- Script API: `set_reflective(tx, ty, bool)`, `is_reflective(tx, ty)`

### UI/Component Rotation
- `draw_quad_rotated()` added to SpriteBatch — rotates vertices around quad center
- Rotation field added to ScriptUIPanel and ScriptUIBar (labels and images already had it)
- All UI element types support rotation in rendering, script API (`ui_set(id, "rotation", deg)`), and editor
- Editor: rotation sliders for all component types

### Texture Atlas UV Fix
- Half-texel UV inset in `make_region()` prevents texture bleeding at atlas region boundaries
- Fixes black lines around HUD panels and pause menu assets

### UI Editor Asset Style Pickers
- Panel properties: dropdown combo listing all panel/button regions from UI atlas and flat UI pack
- Image properties: searchable dropdown with all icon regions + 432 fantasy icons with text filter
- Create Component: panel style combo (22 styles) and icon preset combo for new elements

### HUD Editor Persistence Fix
- `sync_to_hud_config()` writes editor changes back to HUDConfig
- `apply_hud_drag()` moves entire HUD groups (children follow parent panel)
- Edits persist when closing editor

---

## v2.4.0 — March 16, 2026

### Initial Release
- Vulkan 2D RPG engine with sprite batching, Y-sorted rendering, texture atlases
- Cross-platform: Linux, Windows, Android, Meta Quest
- SageLang scripting with 229+ API functions
- Tween engine (19 easing types), particle system (9 presets), save/load
- Quest system, equipment, 2D lighting, screen transitions
- Tile editor with 7 modularized files, 26 panels
- Procedural tileset generator (10 biomes), security fuzzer (7 categories)
- Weather system, day-night cycle, NPC AI with A* pathfinding
- 6 maps across 5 biomes with portal connections
