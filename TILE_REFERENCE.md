# Tile Reference — cf_tileset.png

Visual catalog saved as `games/demo/assets/textures/TILE_CATALOG.png`

## Terrain Tiles (Rows 0-3, Tiles 1-80)

### Solid Fills
| ID | Content | Use |
|----|---------|-----|
| 1 | Pure green grass | Forest/field base |
| 2 | Tan/sand | Desert/beach/path base |
| 3-4 | Deep blue water | Lake/ocean center |
| 81 | Dark brown dirt | Path base |

### Grass-Sand Transitions (edges where grass meets sand)
| ID | Content |
|----|---------|
| 7-8 | Tan with green edges (grass on top/sides) |
| 12-13 | Tan with green edges (opposite corners) |
| 20 | Tan with dark green overlay |
| 22-23 | Grass-sand blend variants |
| 24, 26 | Green with tan bleed |

### Water-Shore Transitions
| ID | Content |
|----|---------|
| 5-6 | Water with curved sandy shore (island shapes) |
| 9 | Water meeting sand (corner) |
| 11 | Water with sand shore (side) |
| 14-16 | Water with various shore curves |
| 41 | Blue water (deep) |
| 44, 46-47 | Water-grass transitions |
| 49-50 | Water with dark shore |
| 52-54 | Water with shadows/rocks |

### Forest/Hedge Edges
| ID | Content |
|----|---------|
| 9-10 | Green with vine/hedge overlays |
| 19 | Dark green forest edge |
| 37-40 | Dense forest/hedge transition pieces |
| 55-60 | Forest floor with path/branch overlays |
| 61-65 | Dark forest variants |
| 67-68 | Dense tree canopy edge |
| 70-72 | Vine/moss overlays |

### Dirt/Path
| ID | Content |
|----|---------|
| 28-29 | Dark dirt/soil |
| 31-32 | Brown earth |
| 34-36 | Sandy brown variants |
| 73-80 | Brown dirt path tiles (good for roads) |

## Object Sprites (Rows 4-8, Tiles 81-160)

Rows 4-8 contain sprite objects (houses, fences, trees, rocks, animals) with transparency. These are NOT ground tiles — they're rendered as stamps/objects.

| ID | Content |
|----|---------|
| 81 | Brown square (dirt) |
| 82 | Crate/barrel scene |
| 83-86 | Fence pieces (semi-transparent) |
| 101-103 | House roof pieces |
| 104-108 | Fence posts, benches |
| 109 | Tree canopy (large green) |
| 111-115 | Trees, bushes, leaves |
| 116-120 | Flowers, mushrooms, flora |
| 121-122 | House wall/window |
| 129 | Full tree with trunk |
| 141-143 | House front with door |
| 142 | Complete house front (detailed) |

## Interior Tiles (Row 13-14, Tiles 261-300)

### Wood/Furniture
| ID | Content |
|----|---------|
| 261 | Shelf/window wall piece |
| 262 | Wood plank floor |
| 263-264 | Bookshelves (colorful books) |
| 267-270 | Dark wood panels/doors |
| 271-272 | Wood with stone base |
| 283 | Wood beam/stair landing |

### Walls
| ID | Content |
|----|---------|
| 265-266 | Light grey-blue walls |
| 273-276 | Light mint/cream walls |
| 277 | Door frame (light with handle) |
| 278 | Warm brown brick |
| 281-282 | Brown brick wall |
| 285-286 | Light blue-mint wall (kitchen) |

### Stone/Floor
| ID | Content |
|----|---------|
| 284 | Dark stone diagonal |
| 287-290 | Herringbone stone floor patterns |
| 291-296 | Grey stone wall/floor variants |
| 297 | Stairs (diagonal) |
| 298-300 | More stone/moss patterns |

## Dungeon Tiles (Row 15, Tiles 301-315)

| ID | Content |
|----|---------|
| 301-302 | Warm tan floor (sandy) |
| 303-305 | Dark purple dungeon wall |
| 306-308 | Brick patterns (brown/red) |
| 309-310 | Dark wall corner pieces |
| 311-312 | Dark with stone border |
| 313-315 | Stone frame/border pieces (L-shapes, corners) |

## Map Building Rules

### Forest Map
- **Base**: Tile 1 (pure grass) for the main ground
- **Forest edges**: Tiles 19, 37-40 around the border (dark green hedges)
- **Paths**: Tiles 73-80 (brown dirt) for walkways
- **Water**: Tile 3-4 for pond center, tiles 5-6, 11, 14-16 for shores
- **Trees**: Use stamp objects (Oak Tree, Small Oak, etc.)

### House Interior
- **Floor**: Tile 262 (wood planks) or 287-290 (stone herringbone)
- **Walls**: Tiles 265-266 (grey) or 281-282 (brick) or 285-286 (mint)
- **Door**: Tile 277 (door frame)
- **Shelves**: Tiles 263-264 (bookshelves)
- **Dark corners**: Tiles 268-270 (dark wood)
- **Stairs**: Tile 297

### Dungeon/Cave
- **Floor**: Tiles 293-296 (mossy stone) or 301-302 (sandy)
- **Walls**: Tiles 303-305 (dark purple)
- **Brick**: Tiles 306-308
- **Borders**: Tiles 313-315 (stone frame corners)

### Key Principle
Transition tiles (grass-to-water, grass-to-sand, etc.) go at BORDERS between two terrain types. Never place them randomly in the middle of a terrain area. Solid fills go in the center, transitions go at edges.
