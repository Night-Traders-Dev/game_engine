#!/usr/bin/env python3
"""Wire up procedurally generated biome tilesets as demo levels.

Generates tilesets, creates map JSON files with noise-based terrain,
creates .sage map scripts, and adds portals between the forest and new maps.

Usage: python tools/wire_biome_levels.py
"""

import json
import math
import os
import random
import sys
from pathlib import Path

# Add tools dir to path for generate_tileset
sys.path.insert(0, str(Path(__file__).parent))
from generate_tileset import BIOMES, generate_biome, SimplexNoise2D

GAME_DIR = Path(__file__).parent.parent / "games" / "demo"
ASSETS = GAME_DIR / "assets"
MAPS_DIR = ASSETS / "maps"
TEXTURES_DIR = ASSETS / "textures"
SCRIPTS_DIR = ASSETS / "scripts" / "maps"

# Biomes to generate as demo levels (subset for manageable demo)
DEMO_BIOMES = ["desert", "snow", "cave", "volcanic"]
MAP_WIDTH = 30
MAP_HEIGHT = 22
TILE_SIZE = 32
SEED = 42


def generate_map_tiles(biome_name, manifest, width, height, seed):
    """Generate a tile data array using noise for terrain placement."""
    noise = SimplexNoise2D(seed + hash(biome_name) % 10000)

    # Get tile IDs from the manifest
    terrains = manifest["terrains"]
    terrain_names = list(terrains.keys())
    terrain_a_ids = terrains[terrain_names[0]]  # primary terrain
    terrain_b_ids = terrains[terrain_names[1]] if len(terrain_names) > 1 else terrain_a_ids

    trans_key = list(manifest.get("transitions", {}).keys())
    trans_ids = manifest["transitions"][trans_key[0]] if trans_key else terrain_a_ids
    deco_ids = manifest.get("decorations", terrain_a_ids[:4])
    water_ids = manifest.get("water", {})
    water_first = water_ids.get("first", terrain_a_ids[0]) if isinstance(water_ids, dict) else terrain_a_ids[0]

    data = []
    collision = []

    # Generate terrain noise
    terrain_noise = noise.generate_grid(width, height, scale=8.0, octaves=3,
                                         offset_x=seed * 100, offset_y=seed * 77)

    rng = random.Random(seed)

    for y in range(height):
        for x in range(width):
            n = terrain_noise[y, x]

            # Border: trees/dense terrain (collision)
            is_border = (x < 2 or x >= width - 2 or y < 2 or y >= height - 2)

            if is_border:
                # Dark border tiles
                tile_id = terrain_b_ids[rng.randint(0, len(terrain_b_ids) - 1)]
                collision.append(1)
            elif n < 0.15:
                # Water
                tile_id = water_first
                collision.append(1)
            elif n < 0.3:
                # Secondary terrain
                tile_id = terrain_b_ids[rng.randint(0, len(terrain_b_ids) - 1)]
                collision.append(0)
            elif n < 0.4:
                # Transition zone — use transition tiles
                tidx = int((n - 0.3) / 0.1 * len(trans_ids)) % len(trans_ids)
                tile_id = trans_ids[tidx]
                collision.append(0)
            elif n > 0.85 and rng.random() < 0.3:
                # Scattered decorations
                tile_id = deco_ids[rng.randint(0, len(deco_ids) - 1)]
                collision.append(0)
            else:
                # Primary terrain
                tile_id = terrain_a_ids[rng.randint(0, len(terrain_a_ids) - 1)]
                collision.append(0)

            data.append(tile_id)

    # Clear a walkable area around the player start (center)
    cx, cy = width // 2, height // 2
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            tx, ty = cx + dx, cy + dy
            if 0 <= tx < width and 0 <= ty < height:
                idx = ty * width + tx
                data[idx] = terrain_a_ids[0]
                collision[idx] = 0

    return data, collision


def create_map_json(biome_name, manifest, portal_back, portal_forward=None):
    """Create a map JSON file for a biome level."""
    tileset_path = f"assets/textures/{biome_name}_tileset.png"

    data, collision = generate_map_tiles(biome_name, manifest, MAP_WIDTH, MAP_HEIGHT, SEED)

    # Player start at center
    start_x = MAP_WIDTH // 2 * TILE_SIZE
    start_y = MAP_HEIGHT // 2 * TILE_SIZE

    # Portals
    portals = []

    # Portal back to forest (bottom-center)
    portals.append({
        "x": MAP_WIDTH // 2,
        "y": MAP_HEIGHT - 3,
        "target_map": portal_back["target_map"],
        "target_x": portal_back["target_x"],
        "target_y": portal_back["target_y"],
        "label": portal_back.get("label", "Return")
    })
    # Mark portal tile as walkable
    pidx = (MAP_HEIGHT - 3) * MAP_WIDTH + MAP_WIDTH // 2
    collision[pidx] = 0

    # Portal forward to next biome (top-center)
    if portal_forward:
        portals.append({
            "x": MAP_WIDTH // 2,
            "y": 2,
            "target_map": portal_forward["target_map"],
            "target_x": portal_forward["target_x"],
            "target_y": portal_forward["target_y"],
            "label": portal_forward.get("label", "Continue")
        })
        pidx2 = 2 * MAP_WIDTH + MAP_WIDTH // 2
        collision[pidx2] = 0

    biome_display = biome_name.replace("_", " ").title()
    map_data = {
        "format": "twilight_map",
        "version": 2,
        "metadata": {
            "name": biome_display,
            "width": MAP_WIDTH,
            "height": MAP_HEIGHT,
            "tile_size": TILE_SIZE,
            "tileset": tileset_path,
            "player_start_x": start_x,
            "player_start_y": start_y,
        },
        "layers": [
            {"name": "ground", "data": data}
        ],
        "collision": collision,
        "portals": portals,
        "objects": [],
        "npcs": [],
    }

    map_path = MAPS_DIR / f"{biome_name}.json"
    map_path.write_text(json.dumps(map_data, indent=2) + "\n")
    print(f"  Map: {map_path}")
    return map_path


def create_map_script(biome_name, manifest):
    """Create a .sage map init script for the biome."""
    biome = BIOMES[biome_name]
    biome_display = biome_name.replace("_", " ").title()

    # Get stamp names for object placement
    stamps = manifest.get("stamps", [])
    tree_stamps = [s["name"] for s in stamps if s["category"] == "tree"]
    misc_stamps = [s["name"] for s in stamps if s["category"] == "misc"]

    lines = [
        f"# {biome_display} Map Script",
        f"# Auto-generated biome level",
        "",
        f"proc {biome_name}_init():",
        f'    log("=== {biome_display} loaded ===")',
        "",
    ]

    # Place trees/vegetation around the edges
    rng = random.Random(SEED + hash(biome_name))
    if tree_stamps:
        lines.append(f"    # Trees and vegetation")
        for i in range(8):
            stamp = tree_stamps[rng.randint(0, len(tree_stamps) - 1)]
            x = rng.randint(3, MAP_WIDTH - 4) * TILE_SIZE
            y = rng.randint(3, MAP_HEIGHT - 4) * TILE_SIZE
            # Avoid center where player spawns
            cx, cy = MAP_WIDTH // 2 * TILE_SIZE, MAP_HEIGHT // 2 * TILE_SIZE
            if abs(x - cx) < 128 and abs(y - cy) < 128:
                x += 160
            lines.append(f'    place_object({x}, {y}, "{stamp}")')
        lines.append("")

    # Place misc objects
    if misc_stamps:
        lines.append(f"    # Props and details")
        for i in range(4):
            stamp = misc_stamps[rng.randint(0, len(misc_stamps) - 1)]
            x = rng.randint(4, MAP_WIDTH - 5) * TILE_SIZE
            y = rng.randint(4, MAP_HEIGHT - 5) * TILE_SIZE
            cx, cy = MAP_WIDTH // 2 * TILE_SIZE, MAP_HEIGHT // 2 * TILE_SIZE
            if abs(x - cx) < 128 and abs(y - cy) < 128:
                x += 160
            lines.append(f'    place_object({x}, {y}, "{stamp}")')
        lines.append("")

    # Atmosphere settings per biome
    atmos = {
        "desert": ('set_clear_color(0.15, 0.12, 0.08)', 'set_clouds(true, 0.15, 8, 40)', 'set_wind(0.25, 45)'),
        "snow": ('set_clear_color(0.08, 0.08, 0.12)', 'set_clouds(true, 0.5, 15, 50)', 'set_wind(0.3, 60)'),
        "cave": ('set_clear_color(0.02, 0.02, 0.03)', 'set_clouds(false, 0, 0, 0)', 'set_god_rays(false, 0, 0)'),
        "volcanic": ('set_clear_color(0.08, 0.02, 0.01)', 'set_clouds(true, 0.6, 10, 30)', 'set_wind(0.1, 15)'),
    }

    if biome_name in atmos:
        lines.append(f"    # Atmosphere")
        for cmd in atmos[biome_name]:
            lines.append(f"    {cmd}")
        lines.append("")

    lines.append(f'    log("{biome_display} ready")')
    lines.append("")

    # Enter proc (called every time player enters)
    lines.append(f"proc {biome_name}_enter():")
    if biome_name in atmos:
        for cmd in atmos[biome_name]:
            lines.append(f"    {cmd}")
    else:
        lines.append(f"    pass")
    lines.append("")

    script_path = SCRIPTS_DIR / f"{biome_name}.sage"
    script_path.write_text("\n".join(lines))
    print(f"  Script: {script_path}")
    return script_path


def add_portal_to_forest(biome_name, portal_x, portal_y):
    """Add a portal in the forest map leading to the new biome."""
    forest_path = MAPS_DIR / "forest.json"
    with open(forest_path) as f:
        forest = json.load(f)

    biome_display = biome_name.replace("_", " ").title()

    # Check if portal already exists
    for p in forest.get("portals", []):
        if p.get("target_map", "").startswith(f"{biome_name}.json"):
            print(f"  Portal to {biome_name} already exists in forest")
            return

    forest.setdefault("portals", []).append({
        "x": portal_x,
        "y": portal_y,
        "target_map": f"{biome_name}.json",
        "target_x": MAP_WIDTH // 2,
        "target_y": MAP_HEIGHT // 2,
        "label": f"Enter {biome_display}",
    })

    # Make portal tile walkable
    idx = portal_y * forest["metadata"]["width"] + portal_x
    if idx < len(forest.get("collision", [])):
        forest["collision"][idx] = 0

    with open(forest_path, "w") as f:
        json.dump(forest, f, indent=2)
        f.write("\n")

    print(f"  Portal added to forest at ({portal_x}, {portal_y}) -> {biome_name}")


def update_game_json(new_scripts):
    """Add new map scripts to game.json."""
    game_json_path = GAME_DIR / "game.json"
    with open(game_json_path) as f:
        game = json.load(f)

    scripts = game.get("scripts", [])
    added = []
    for script_path in new_scripts:
        # Make path relative to game dir
        rel_path = str(script_path.relative_to(GAME_DIR))
        if rel_path not in scripts:
            scripts.append(rel_path)
            added.append(rel_path)

    if added:
        game["scripts"] = scripts
        with open(game_json_path, "w") as f:
            json.dump(game, f, indent=2)
            f.write("\n")
        print(f"  Added {len(added)} scripts to game.json: {added}")
    else:
        print(f"  All scripts already in game.json")


def main():
    print("=" * 50)
    print("Wiring biome levels to demo")
    print("=" * 50)

    # Portal positions in the forest map for each new biome
    # Placed near the edges of the walkable area
    portal_positions = {
        "desert": (25, 5),      # top-right area
        "snow": (5, 5),         # top-left area
        "cave": (25, 18),       # bottom-right area
        "volcanic": (5, 18),    # bottom-left area
    }

    new_scripts = []

    for i, biome_name in enumerate(DEMO_BIOMES):
        print(f"\n── {biome_name.upper()} ──")

        # 1. Generate tileset
        generate_biome(biome_name, str(TEXTURES_DIR), seed=SEED)

        # 2. Load manifest
        manifest_path = TEXTURES_DIR / f"{biome_name}_manifest.json"
        with open(manifest_path) as f:
            manifest = json.load(f)

        # 3. Portal info: back to forest
        px, py = portal_positions[biome_name]
        portal_back = {
            "target_map": "forest.json",
            "target_x": px,
            "target_y": py,
            "label": "Return to Forest",
        }

        # 4. Create map JSON
        create_map_json(biome_name, manifest, portal_back)

        # 5. Create map script
        script_path = create_map_script(biome_name, manifest)
        new_scripts.append(script_path)

        # 6. Add portal from forest to this biome
        add_portal_to_forest(biome_name, px, py)

    # 7. Update game.json with new scripts
    print(f"\n── GAME.JSON ──")
    update_game_json(new_scripts)

    # 8. Update forest script to mention the new portals
    print(f"\n── SUMMARY ──")
    print(f"Generated {len(DEMO_BIOMES)} biome levels:")
    for biome in DEMO_BIOMES:
        pos = portal_positions[biome]
        print(f"  {biome:12s} - portal at forest ({pos[0]}, {pos[1]})")
    print(f"\nForest map now has portals to: {', '.join(DEMO_BIOMES)}")
    print(f"Each biome has a return portal back to the forest.")
    print(f"\nBuild and run to test: ./build.sh linux && cd build-linux && ./twilight_game_binary")


if __name__ == "__main__":
    os.chdir(Path(__file__).parent)
    main()
