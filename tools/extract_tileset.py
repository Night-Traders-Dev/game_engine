#!/usr/bin/env python3
"""
Process Tilemap.png for the game engine.
1. Removes the specific green background color (preserving grass tile greens)
2. Saves cleaned full-resolution atlas as tileset.png
"""

import numpy as np
from PIL import Image

def main():
    src_path = '/home/kraken/Devel/game_engine/assets/textures/Tilemap.png'
    out_path = '/home/kraken/Devel/game_engine/assets/textures/tileset.png'

    img = Image.open(src_path).convert('RGBA')
    pixels = np.array(img)
    W, H = img.size
    print(f"Source: {W}x{H}")

    # Background color: ~R=123 G=166 B=100
    # Use Euclidean color distance with tight threshold
    bg_r, bg_g, bg_b = 123.0, 166.0, 100.0
    r = pixels[:,:,0].astype(float)
    g = pixels[:,:,1].astype(float)
    b = pixels[:,:,2].astype(float)

    dist = np.sqrt((r - bg_r)**2 + (g - bg_g)**2 + (b - bg_b)**2)

    # Tight threshold to only catch the exact background green
    threshold = 15.0
    is_bg = dist < threshold

    pixels[:,:,3] = np.where(is_bg, 0, 255)

    # Count removed vs kept
    total = W * H
    removed = np.sum(is_bg)
    print(f"Background pixels removed: {removed}/{total} ({100*removed/total:.1f}%)")

    out = Image.fromarray(pixels)
    out.save(out_path)
    print(f"Saved: {out_path}")

    # Debug version with dark background
    debug = pixels.copy()
    debug[:,:,0] = np.where(is_bg, 32, pixels[:,:,0])
    debug[:,:,1] = np.where(is_bg, 32, pixels[:,:,1])
    debug[:,:,2] = np.where(is_bg, 32, pixels[:,:,2])
    debug[:,:,3] = 255
    Image.fromarray(debug).save(out_path.replace('.png', '_debug.png'))
    print("Saved debug version")

    # Also save key tile crops with the improved bg removal for verification
    clean = Image.fromarray(pixels)
    crops = {
        'grass_block': (126, 96, 253, 202),
        'dirt_block': (275, 96, 375, 202),
        'hedge_block': (385, 96, 528, 202),
        'road_section': (562, 95, 960, 332),
        'water_section': (126, 504, 480, 621),
        'tree_large': (978, 496, 1081, 607),
    }
    import os
    crop_dir = '/home/kraken/Devel/game_engine/assets/textures/tile_crops'
    os.makedirs(crop_dir, exist_ok=True)
    for name, (x1, y1, x2, y2) in crops.items():
        clean.crop((x1, y1, x2, y2)).save(f'{crop_dir}/{name}_v2.png')
    print("Saved verification crops")


if __name__ == '__main__':
    main()
