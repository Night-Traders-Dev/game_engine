#!/usr/bin/env python3
"""
Twilight Engine — Asset Scaling Pipeline

Generates multi-resolution versions of all game textures using nearest-neighbor
scaling (preserves pixel art crispness). Output goes to resolution-specific
subdirectories alongside the originals.

Usage:
    python3 tools/scale_assets.py [game_dir] [scales...]

    # Generate 2x and 3x versions of all demo assets
    python3 tools/scale_assets.py games/demo 2 3

    # Generate all standard scales (1x is original, 2x, 3x, 4x)
    python3 tools/scale_assets.py games/demo 2 3 4

    # Just 2x for a specific file
    python3 tools/scale_assets.py --file games/demo/assets/textures/mage_player.png 2

Output structure:
    assets/textures/mage_player.png          (original 1x)
    assets/textures/2x/mage_player.png       (2x scaled)
    assets/textures/3x/mage_player.png       (3x scaled)
"""

import sys
import os
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow required. Install with: pip install Pillow")
    sys.exit(1)


def scale_image(src_path: str, dst_path: str, scale: int):
    """Scale a PNG image by integer factor using nearest-neighbor."""
    try:
        img = Image.open(src_path)
    except Exception as e:
        print(f"  WARNING: Skipping {src_path}: {e}")
        return 0, 0
    w, h = img.size
    new_w, new_h = w * scale, h * scale
    scaled = img.resize((new_w, new_h), Image.NEAREST)

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    scaled.save(dst_path, "PNG")
    return new_w, new_h


def scale_directory(tex_dir: str, scales: list[int], verbose: bool = True):
    """Scale all PNG files in a texture directory."""
    tex_path = Path(tex_dir)
    if not tex_path.exists():
        print(f"ERROR: Directory not found: {tex_dir}")
        return

    png_files = sorted(tex_path.glob("*.png"))
    if not png_files:
        print(f"No PNG files found in {tex_dir}")
        return

    total = 0
    for scale in scales:
        scale_dir = tex_path / f"{scale}x"
        scale_dir.mkdir(exist_ok=True)

        for png in png_files:
            dst = scale_dir / png.name
            new_w, new_h = scale_image(str(png), str(dst), scale)
            if new_w == 0: continue
            if verbose:
                print(f"  {scale}x: {png.name} ({png.stat().st_size//1024}KB) "
                      f"-> {new_w}x{new_h} ({dst.stat().st_size//1024}KB)")
            total += 1

    print(f"\nScaled {total} files across {len(scales)} resolutions")


def scale_single_file(file_path: str, scales: list[int]):
    """Scale a single file to multiple resolutions."""
    src = Path(file_path)
    if not src.exists():
        print(f"ERROR: File not found: {file_path}")
        return

    for scale in scales:
        scale_dir = src.parent / f"{scale}x"
        dst = scale_dir / src.name
        new_w, new_h = scale_image(str(src), str(dst), scale)
        print(f"  {scale}x: {src.name} -> {new_w}x{new_h}")


def generate_stamps_file(tex_dir: str, scale: int):
    """Generate a scaled version of cf_stamps.txt with adjusted coordinates."""
    stamps_path = Path(tex_dir) / "cf_stamps.txt"
    if not stamps_path.exists():
        return

    out_path = Path(tex_dir) / f"{scale}x" / "cf_stamps.txt"
    with open(stamps_path) as f:
        lines = f.readlines()

    scaled_lines = []
    for line in lines:
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) >= 5:
            name = parts[0]
            px, py, pw, ph = int(parts[1])*scale, int(parts[2])*scale, int(parts[3])*scale, int(parts[4])*scale
            cat = parts[5] if len(parts) > 5 else "misc"
            scaled_lines.append(f"{name}|{px}|{py}|{pw}|{ph}|{cat}\n")

    os.makedirs(out_path.parent, exist_ok=True)
    with open(out_path, "w") as f:
        f.writelines(scaled_lines)
    print(f"  {scale}x: cf_stamps.txt ({len(scaled_lines)} stamps)")


def main():
    args = sys.argv[1:]

    if not args:
        print(__doc__)
        sys.exit(0)

    # Parse --file mode
    if args[0] == "--file":
        if len(args) < 3:
            print("Usage: scale_assets.py --file <path.png> <scale1> [scale2...]")
            sys.exit(1)
        file_path = args[1]
        scales = [int(s) for s in args[2:]]
        scale_single_file(file_path, scales)
        return

    # Directory mode
    game_dir = args[0]
    scales = [int(s) for s in args[1:]] if len(args) > 1 else [2, 3]

    tex_dir = os.path.join(game_dir, "assets", "textures")
    print(f"Scaling assets in {tex_dir}")
    print(f"Scales: {', '.join(f'{s}x' for s in scales)}")
    print()

    scale_directory(tex_dir, scales)

    # Also generate scaled stamps files
    for scale in scales:
        generate_stamps_file(tex_dir, scale)


if __name__ == "__main__":
    main()
