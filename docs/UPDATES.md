# Twilight Engine Updates

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
