#!/usr/bin/env python3
"""
Assemble 9 sprite frames into a 3x3 sprite sheet, or run the full Blender-to-spritesheet pipeline.

Sprite sheet layout (matches define_npc_atlas_regions):
    [idle_down]   [walk_down_0]   [walk_down_1]
    [idle_up]     [walk_up_0]     [walk_up_1]
    [idle_right]  [walk_right_0]  [walk_right_1]

Usage (full pipeline):
    python3 tools/blender_to_spritesheet.py --blend model.blend --output character.png --size 64 --mode topdown

Usage (assemble only):
    python3 tools/blender_to_spritesheet.py --frames /tmp/frames/ --output character.png --size 64
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image


def parse_args():
    parser = argparse.ArgumentParser(
        description="Blender-to-2D-engine sprite sheet pipeline"
    )
    parser.add_argument(
        "--blend", type=str, default=None,
        help="Path to .blend file (triggers full pipeline with Blender rendering)"
    )
    parser.add_argument(
        "--frames", type=str, default=None,
        help="Directory containing pre-rendered frame PNGs (assemble-only mode)"
    )
    parser.add_argument(
        "--output", type=str, required=True,
        help="Output sprite sheet PNG path"
    )
    parser.add_argument(
        "--size", type=int, default=64,
        help="Pixel size per frame cell (default: 64)"
    )
    parser.add_argument(
        "--mode", type=str, default="topdown", choices=["topdown", "side"],
        help="Camera mode for Blender rendering (default: topdown)"
    )
    parser.add_argument(
        "--action", type=str, default=None,
        help="Animation action name for Blender rendering"
    )
    parser.add_argument(
        "--blender", type=str, default="blender",
        help="Path to Blender executable (default: blender)"
    )
    parser.add_argument(
        "--no-transparent", action="store_true",
        help="Disable transparent background"
    )
    parser.add_argument(
        "--manifest", type=str, default=None,
        help="Output manifest JSON path (default: <output>.json)"
    )
    return parser.parse_args()


def render_frames(args, frames_dir):
    """Run blender_render_sprites.py to produce 9 frame PNGs."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    render_script = os.path.join(script_dir, "blender_render_sprites.py")

    if not os.path.isfile(render_script):
        print(f"ERROR: Render script not found: {render_script}")
        sys.exit(1)

    if not os.path.isfile(args.blend):
        print(f"ERROR: Blend file not found: {args.blend}")
        sys.exit(1)

    cmd = [
        args.blender, "-b", args.blend, "-P", render_script, "--"
    ]
    cmd += ["--mode", args.mode]
    cmd += ["--size", str(args.size)]
    cmd += ["--output", frames_dir]
    if args.no_transparent:
        cmd += ["--no-transparent"]
    if args.action:
        cmd += ["--action", args.action]

    print(f"Running Blender: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=False)
    if result.returncode != 0:
        print(f"ERROR: Blender exited with code {result.returncode}")
        sys.exit(1)


def assemble_spritesheet(frames_dir, output_path, size):
    """
    Assemble 9 frame PNGs into a 3x3 sprite sheet.

    Expected files in frames_dir:
        frame_down_0.png  frame_down_1.png  frame_down_2.png
        frame_up_0.png    frame_up_1.png    frame_up_2.png
        frame_right_0.png frame_right_1.png frame_right_2.png
    """
    directions = ["down", "up", "right"]
    num_frames = 3

    sheet = Image.new("RGBA", (size * num_frames, size * len(directions)), (0, 0, 0, 0))

    for row, direction in enumerate(directions):
        for col in range(num_frames):
            filename = f"frame_{direction}_{col}.png"
            filepath = os.path.join(frames_dir, filename)

            if not os.path.isfile(filepath):
                print(f"WARNING: Missing frame: {filepath} — using blank")
                frame = Image.new("RGBA", (size, size), (0, 0, 0, 0))
            else:
                frame = Image.open(filepath).convert("RGBA")
                # Downscale from 2x render size to target size (AA downscale)
                if frame.size != (size, size):
                    frame = frame.resize((size, size), Image.LANCZOS)

            sheet.paste(frame, (col * size, row * size))

    # Ensure output directory exists
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    sheet.save(output_path, "PNG")
    print(f"Sprite sheet saved: {output_path} ({size * num_frames}x{size * len(directions)})")
    return sheet


def write_manifest(output_path, manifest_path, size, blend_path):
    """Write companion JSON manifest."""
    if manifest_path is None:
        base, _ = os.path.splitext(output_path)
        manifest_path = base + ".json"

    manifest = {
        "sprite_sheet": os.path.basename(output_path),
        "grid_w": size,
        "grid_h": size,
        "directions": ["down", "up", "right"],
        "frames_per_direction": 3,
        "source_blend": os.path.basename(blend_path) if blend_path else None,
    }

    os.makedirs(os.path.dirname(os.path.abspath(manifest_path)), exist_ok=True)
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"Manifest saved: {manifest_path}")


def main():
    args = parse_args()

    if not args.blend and not args.frames:
        print("ERROR: Provide either --blend (full pipeline) or --frames (assemble only)")
        sys.exit(1)

    if args.blend:
        # Full pipeline: render then assemble
        frames_dir = tempfile.mkdtemp(prefix="blender_sprites_")
        print(f"Rendering frames to: {frames_dir}")
        render_frames(args, frames_dir)
    else:
        frames_dir = args.frames

    # Assemble
    assemble_spritesheet(frames_dir, args.output, args.size)
    write_manifest(args.output, args.manifest, args.size, args.blend)

    print("\nDone!")


if __name__ == "__main__":
    main()
