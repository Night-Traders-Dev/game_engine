# Map Design Guide — Twilight Engine

Reference for creating professional-looking maps. Based on RPG Maker mapping theory, autotiling principles, and pixel art tileset design.

## Core Principles

### 1. Never Use Perfect Rectangles
Square rooms, perfectly aligned tree rows, and grid-matching buildings look artificial. Break up edges with:
- Irregular tree placement along borders
- Winding paths instead of straight lines
- Elevation changes and terrain variation

### 2. Start Smaller Than You Think
Empty space is harder to fill than adding more. A 20x15 map with good detail beats a 40x30 map with sparse placement.

### 3. Every Tile Has a Purpose
- **Solid fills** (grass, water, dirt) go in the CENTER of terrain areas
- **Transition tiles** go at EDGES between two terrain types
- **Never scatter transition tiles randomly** — they have specific directional meaning

### 4. The Three-Layer Rule
Professional maps use at least 3 visual layers:
1. **Ground** — Base terrain (grass, stone, sand)
2. **Detail** — Paths, water edges, terrain variation
3. **Objects** — Trees, buildings, rocks, decorations

## Terrain Transition System

### How Transitions Work
When grass meets water, you don't just place grass tiles next to water tiles. You use transition tiles that blend the two:

```text
G = pure grass    W = pure water    T = grass-water transition

    G G G G G
    G G T T G        ← transitions at the edge
    G T W W T
    G T W W T        ← pure water in center
    G G T T G
    G G G G G
```

### Our Tileset's Transition Tiles (cf_tileset.png)

| Type | Center | Edges | Description |
|------|--------|-------|-------------|
| Grass | 1, 59 | 19, 37-40, 55-65 | Pure green → dark hedge border |
| Water | 3, 4, 41 | 5-6, 9, 11, 14-16, 44-50 | Deep blue → shore curves |
| Sand/Dirt | 2, 23 | 7-8, 12-13, 20, 22, 24, 26 | Tan fill → grass-sand edge |
| Path | 73-80 | 75 (lighter edges) | Brown dirt → grass edge |

### Transition Placement Rules
- Place **solid fill** tiles first (the interiors)
- Then place **edge transitions** around each fill area
- Edge tiles have directionality — a top-edge is different from a left-edge
- Corner tiles go at the intersections of two edges
- Use the **tile catalog** (TILE_CATALOG.png) to identify which tiles connect to which

## Forest Design

### Key Rules (from RPG Maker mapping theory)
1. **Never line trees uniformly** — stagger placement, vary spacing
2. **Create density gradients** — dense border → medium middle → open clearings
3. **Use 3+ tree sizes** — large oaks, small oaks, bushes
4. **Add bald spots** — small clearings break up monotony
5. **Winding paths** — paths in forests should curve, not run straight
6. **Points of interest** — a pond, camp, cabin, or ruin gives purpose to areas
7. **Overlap trees** — place some trees partially behind others for depth

### Forest Map Pattern
```text
D D D D D D D D D D D D D
D D M M M M M M M M M D D
D M . . . T . . . . M D D    D = Dense dark border
D M . . . T . . . . M D D    M = Medium forest
D M . . [HOUSE] . . M D D    T = Trees (scattered)
D M . . . P . . . . M D D    P = Dirt path
D M T . . P . . T . M D D    . = Open grass
D M . . . P . . . . M D D    [  ] = Clearing
D D M M M P M M M M D D D
D D D D D P D D D D D D D
```

## Town/Village Design

### Key Rules
1. **Face entrances toward the center** — doors should point at the main path/plaza
2. **Vary building sizes** — important NPCs get larger homes
3. **Organic paths** — curves where people would walk, hard corners around buildings
4. **Contextual details** — crops near farms, crates near shops, wells in the center
5. **2-3 focal points** — market, town square, dungeon entrance
6. **No empty space** — if you can't fill it, the map is too big

## Interior Design

### Key Rules
1. **Use bright tiles** — interior tiles need brightness > 120 to be visible
2. **Wall tiles on edges** — walls go on the perimeter, floor tiles in the center
3. **Furniture as objects** — use stamps (Table, Bed, Chest) not tiles for furniture
4. **Door tile at exit** — the brightest/most distinct tile marks the door
5. **Rug/carpet variation** — break up floor monotony with a center rug
6. **Match the atmosphere** — warm clear color for homes, cool for dungeons

### Interior Tile Brightness Guide
| Brightness | Tiles | Visibility |
|-----------|-------|-----------|
| 150+ | 2, 7, 8, 12, 13, 23, 277 | Excellent (use for floors, walls) |
| 120-150 | 28, 73-80, 301 | Good (use for paths, rugs) |
| 80-120 | 1, 19-27 | Fair (use for accents) |
| < 80 | 261-272, 281-290 | Poor (nearly invisible indoors) |

## Autotile / Bitmask Theory (for future implementation)

### 4-Bit Bitmask (16 tiles)
Check 4 cardinal neighbors (N/W/E/S), assign bit values 1/2/4/8. Sum gives tile index 0-15.

### 8-Bit Bitmask with Diagonal Gating (47 tiles)
Check all 8 neighbors but diagonal corners only count if both adjacent cardinals are present. Reduces 256 combinations to 47 unique tiles.

### Implementation Notes
- Our tileset has enough transition tiles for manual placement
- Future: implement auto-tiling in the editor paint tool
- The editor could auto-select transition tiles when painting near terrain borders

## Interior Design Theory

### Core Principle: Rooms Tell Stories
Every room should feel like someone LIVES there. Ask: who owns this room? What do they do? Where do they sleep, eat, work?

### Avoid "Square House Syndrome"
- Don't build a big rectangle and throw furniture in randomly
- Use inner walls to divide space into functional zones
- Irregular shapes feel more natural than perfect squares
- L-shaped rooms, alcoves, and nooks add character

### Wall Rules
- **Consistent wall height** — 1 tile high walls throughout (for top-down)
- **Walls on the perimeter ONLY** — top and side edges
- **Bottom wall = door wall** — player enters from the south
- **Windows on exterior walls only** — don't put windows on interior dividers

### Floor Pattern
- **Single base tile** for most of the floor (wood planks, stone)
- **Rug/carpet** in the center to break monotony
- **Threshold tiles** at doorways (different from main floor)
- Avoid checkerboard patterns — they look artificial

### Furniture Placement
- **Against walls, not ON walls** — furniture sits IN FRONT of wall tiles
- **Functional zones**: bed corner, dining area, fireplace nook
- **Leave walkable paths** — player needs to move through the room
- **Corner details** — put something in every corner (barrel, plant, shelf)
- **Central focus** — table, fireplace, or workbench as the room's purpose

### Room Type Recipes

**Cottage/Home (10x10)**:
```text
W W B B F F S S W W     W=wall, B=bookshelf, F=fireplace
W . . . . . . . . W     S=shelf/window, D=door, R=rug
W . . . . . . . . W     T=table, C=chair, b=bed
W . . R R R . . . W     .=floor
W . . R T R . . . W
W . . R R R . b . W
W . . . . . . b . W
W . . . . . . . . W
W . . C . . . . . W
W W W W D W W W W W
```

**Shop/Merchant (12x8)**:
```text
W W S S S S W W W W W W    S=shelf/goods, C=counter
W . . . . . . . . . . W    $=register/chest
W . . . . . C C C $ . W
W . . . . . . . . . . W
W . . . . . . . . . . W
W B . . . . . B . B . W    B=barrel/crate
W . . . . . . . . . . W
W W W W W D W W W W W W
```

**Tavern/Inn (14x10)**:
```text
W W W W S S W W W W W W W W    S=bar shelf
W . . . C C C . . . . . . W    C=bar counter
W . . . . . . . . . . . . W    T=table, c=chair
W . T c . . . c T . . . . W
W . . . . . . . . . . . . W
W . . . c T c . . c T c . W
W . . . . . . . . . . . . W
W . B . . . . . . . . B . W    B=barrel
W . . . . . . . . . . . . W
W W W W W W D W W W W W W W
```

### Lighting & Atmosphere
- **Warm clear color** for homes: `(0.25, 0.20, 0.15)`
- **Cool clear color** for dungeons: `(0.10, 0.10, 0.15)`
- **Bright clear color** for shops: `(0.30, 0.25, 0.20)`
- Use fireplace/torch stamps near light sources
- Darker corners feel more cozy (but don't go invisible)

### The "Lived-In" Test
Before finalizing an interior, check:
- [ ] Could you describe who lives here?
- [ ] Does the furniture arrangement make sense for daily life?
- [ ] Is there a clear purpose for each area of the room?
- [ ] Are there small details (flowers, books, tools) that add personality?
- [ ] Can the player navigate without getting stuck?

## HUD & UI Design Theory

### The Z-Pattern
Players' eyes naturally follow a Z across the screen:
1. **Top-left** → Most important persistent info (HP, name, level)
2. **Top-right** → Secondary persistent info (time, gold, minimap)
3. **Bottom-left** → Contextual info (inventory bar, item use)
4. **Bottom-right** → Minimap or secondary display
5. **Center** → NEVER put persistent UI here (blocks gameplay)

### 7 Beginner HUD Mistakes (from UI/UX Art Directors)

1. **Too much on screen** → Use information hierarchy, progressive disclosure
2. **Not testing in-context** → Test at actual play distance (5ft PC, 10ft console)
3. **Poor typography** → Max 2 fonts, prefer sans-serif, use outlines for contrast
4. **Thoughtless color** → One "hot-action" color for critical elements, desaturate secondary
5. **Plain number presentation** → Use bars/gauges instead of raw numbers
6. **Poor spacing** → Group similar elements (HP+mana together, NOT scattered)
7. **Ignoring platform** → Different sizes for desktop vs mobile vs TV

### HUD Layout Rules

- **Minimal footprint** — show only what affects current decisions
- **Group by importance** — HP/MP together, gold/XP together
- **Consistent position** — never move elements between screens
- **Dynamic visibility** — hide when not needed (inventory bar during cutscenes)
- **High contrast** — UI must be readable over any background tile
- **No raw numbers alone** — always pair with a visual (bar, icon, gauge)

### Pause Menu Structure

- **Center screen** — modal overlay, dim the game behind it
- **6-8 items max** — Resume, Save, Settings, Controls, Quit (not 20 options)
- **Visual hierarchy** — Resume is largest/brightest, Quit is smallest/dimmest
- **Icon + text** — each item has an icon for quick scanning
- **Cursor feedback** — highlighted item must be obvious (color change + cursor)

### Window/Panel Best Practices

- **Dark semi-transparent background** with a visible border
- **Title bar** at the top with icon + text
- **Content area** with padding (don't touch the edges)
- **Close button** or ESC hint visible
- **Consistent style** across all windows (same border, same font)

## References

- [Tile Bitmasking Auto-Tile Tutorial](https://code.tutsplus.com/how-to-use-tile-bitmasking-to-auto-tile-your-level-layouts--cms-25673t)
- [Pixel Art Tileset Complete Guide](https://www.sandromaglione.com/articles/how-to-create-a-pixel-art-tileset-complete-guide)
- [Procedural Pixel Art Tilemaps](https://dev.to/jhmciberman/procedural-pixel-art-tilemaps-57e2)
- [RPG Maker Mapping Forests](https://www.rpgmakerweb.com/blog/mapping-forests)
- [RPG Maker Mapping Towns](https://www.rpgmakerweb.com/blog/mapping-towns)
- [Terrain Transitions on OpenGameArt](https://opengameart.org/content/terrain-transitions)
