# Editor Guide

## Desktop (Tab to toggle)

- **Menu Bar** — File (New Map/Save/Load/Import), Edit (Undo/Redo), View (toggle windows), Tools
- **Tools** — Paint, Erase, Fill, Eyedrop, Select, Collision, Reflection, Line, Rectangle, Portal
- **Brush Sizes** — 1x1, 2x2, 3x3
- **Assets Panel** — Tabbed tileset browser (Tiles, Buildings, Furniture, Characters, Trees, Vehicles, Misc) with image previews and keyboard shortcuts (Q/E to cycle, [ ] brackets, F5-F11 direct jump)
- **Minimap** — Color-coded map overview with player/NPC markers, click to teleport
- **NPC Spawner** (F2) — Spawn NPCs with presets (animals, enemies, villagers), click-to-place on map
- **Script IDE** (F3) — Built-in SageLang editor with syntax highlighting, asset click-to-highlight, menu bar (File/Help), integrated API manual, and a **Map Script** panel for editing the current map's companion script
- **Debug Console** (F4) — Color-coded log stream, filter by level, live SageLang command input
- **Game Systems Panel** (F5) — Tweens, particles, lighting, quests, equipment, save/load, settings, achievements, transitions, map resize, auto-tiling config, parallax backgrounds, gamepad state, dialogue history, events — all with live controls
- **Object Inspector** (View menu) — Edit world object position, scale per-object
- **Prefab System** (View menu) — Save tile selections as reusable prefabs, click-to-paste
- **Map Resize** — Resize maps from the Systems panel (4x4 to 500x500)
- **Auto-Tiling** — 4-bit corner bitmask (plus 8-bit blob) auto-selects transition tiles when painting terrain boundaries
- **UI/HUD Editor** (F6) — Visual drag-and-drop: click to select, drag to move (HUD groups move together), right-drag edges to resize. 18 templates. 9-slice. Live property editing with color pickers, opacity, scale, layer, rotation. Edits update in-place (upsert) rather than appending duplicates
- **Map Script Generation** — Every editor action auto-generates SageLang. Updates/deletes modify existing lines in-place rather than appending

### New Map Dialog (File > New Map)

- Width/Height in tiles, Tile Size in pixels
- **Top-Down RPG** mode: empty map, player at center (default 40x30)
- **Platformer** mode: ground at bottom row, player near left edge (default 60x15)
- Shows pixel dimensions and screen count

## Android (Menu button toggles editor)

- Tap the **hamburger menu button** (top-right) to open the full-screen editor menu
- **Tools** — Paint, Erase, Fill, Collision with brush size selection
- **Layers** — Select active tile layer
- **Tile Select** — Grid of tile IDs for touch selection
- **NPC Spawner** — Preset NPCs spawned at player position
- **Map Info** — Map dimensions, NPC count, player position, gold, inventory stats

## Script IDE

- **File Menu** — New Script, Open Script, Save, Save & Reload, Reload All
- **Help Menu** — SageLang API Manual (8 tabbed sections)
- **Syntax Highlighting** — Keywords (purple), strings (gold), numbers (cyan), built-in functions (teal), booleans (orange), comments (green)
- **Asset Highlighting** — Click any string literal in code to highlight matching NPCs/items in the game world
- **View/Edit Toggle** — Syntax-highlighted read-only view by default; click "Edit" for full text editing
- **Map Script Panel** — Current map's companion `.sage` script at top of file list under gold header

## Parallax Backgrounds Panel (F5 > Parallax Backgrounds)

- **Preset dropdown** — 8 biomes (forest, cave, night, sunset, snow, desert, forest_sunset, forest_trees)
- **One-click Load** — Loads all layers with auto-configured scroll speeds
- **Per-layer controls** — Scroll X/Y, auto-scroll, scale, tint color, repeat/pin/fill, z-order
- **Add custom layer** — Texture path input + Add button
- **Clear All** — Remove all background layers
