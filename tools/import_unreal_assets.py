#!/usr/bin/env python3
"""
Import Unreal Engine assets into Twilight Engine.

Supports:
  - .glb/.gltf files (exported from UE5 via glTF Exporter plugin)
  - .fbx files (via Blender conversion to sprite sheets)
  - .png/.jpg textures (direct copy + resize)
  - Directories of assets (batch import)

Pipeline:
  1. glTF/FBX → Blender (headless) → orthographic render → sprite sheet
  2. Textures → resize to tile size → add to tileset + stamps
  3. Generates engine-compatible sprite sheets and stamps

Usage:
    python3 tools/import_unreal_assets.py --input model.glb --output assets/textures/imported.png --size 64
    python3 tools/import_unreal_assets.py --input exports/ --output assets/textures/ue_imports/ --mode topdown
    python3 tools/import_unreal_assets.py --input texture.png --output assets/textures/tile.png --resize 32

Requirements: Blender 4.x (for 3D model conversion), Pillow
"""

import argparse
import os
import sys
import subprocess
import tempfile
import shutil

def convert_3d_to_spritesheet(input_path, output_path, size=64, mode="topdown"):
    """Convert a 3D model (glTF/FBX) to a sprite sheet via Blender."""
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    render_script = os.path.join(tools_dir, "blender_render_sprites.py")
    assemble_script = os.path.join(tools_dir, "blender_to_spritesheet.py")

    ext = os.path.splitext(input_path)[1].lower()

    if ext in (".glb", ".gltf", ".fbx", ".obj", ".blend"):
        # For glTF/FBX: import into Blender, then render sprite sheet
        tmpdir = tempfile.mkdtemp(prefix="tw_ue_import_")

        # Create a Blender script that imports the model then renders
        import_script = os.path.join(tmpdir, "import_and_render.py")
        with open(import_script, "w") as f:
            f.write(f"""
import bpy, sys, os
# Clear default scene
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Import the model
path = r"{os.path.abspath(input_path)}"
ext = os.path.splitext(path)[1].lower()
if ext in ('.glb', '.gltf'):
    bpy.ops.import_scene.gltf(filepath=path)
elif ext == '.fbx':
    bpy.ops.import_scene.fbx(filepath=path)
elif ext == '.obj':
    bpy.ops.wm.obj_import(filepath=path)
elif ext == '.blend':
    # Already a blend file, load objects from it
    with bpy.data.libraries.load(path) as (data_from, data_to):
        data_to.objects = data_from.objects
    for obj in data_to.objects:
        bpy.context.collection.objects.link(obj)

# Auto-center and scale to fit in unit cube
bpy.ops.object.select_all(action='SELECT')
if bpy.context.selected_objects:
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
    # Calculate bounding box
    import mathutils
    min_co = mathutils.Vector((float('inf'),) * 3)
    max_co = mathutils.Vector((float('-inf'),) * 3)
    for obj in bpy.context.selected_objects:
        if hasattr(obj, 'bound_box'):
            for v in obj.bound_box:
                world_v = obj.matrix_world @ mathutils.Vector(v)
                min_co = mathutils.Vector((min(min_co[i], world_v[i]) for i in range(3)))
                max_co = mathutils.Vector((max(max_co[i], world_v[i]) for i in range(3)))
    size_vec = max_co - min_co
    max_dim = max(size_vec)
    if max_dim > 0:
        scale = 1.0 / max_dim
        for obj in bpy.context.selected_objects:
            obj.scale *= scale

# Save as temp blend
tmp = r"{os.path.join(tmpdir, 'imported.blend')}"
bpy.ops.wm.save_as_mainfile(filepath=tmp)
print(f"[UE Import] Saved imported model: {{tmp}}")
""")

        # Run import
        print(f"[UE Import] Importing {input_path} into Blender...")
        result = subprocess.run(
            ["blender", "-b", "-P", import_script],
            capture_output=True, text=True, timeout=120
        )
        if result.returncode != 0:
            print(f"[UE Import] Blender import failed:\n{result.stderr[-500:]}")
            shutil.rmtree(tmpdir, ignore_errors=True)
            return False

        imported_blend = os.path.join(tmpdir, "imported.blend")
        if not os.path.exists(imported_blend):
            print("[UE Import] Failed to create imported.blend")
            shutil.rmtree(tmpdir, ignore_errors=True)
            return False

        # Render sprite sheet using existing pipeline
        print(f"[UE Import] Rendering sprite sheet ({size}px, {mode})...")
        result = subprocess.run(
            ["python3", assemble_script,
             "--blend", imported_blend,
             "--output", output_path,
             "--size", str(size),
             "--mode", mode],
            capture_output=True, text=True, timeout=300
        )

        shutil.rmtree(tmpdir, ignore_errors=True)

        if result.returncode == 0 and os.path.exists(output_path):
            print(f"[UE Import] Sprite sheet saved: {output_path}")
            return True
        else:
            print(f"[UE Import] Render failed:\n{result.stderr[-500:]}")
            return False
    else:
        print(f"[UE Import] Unsupported 3D format: {ext}")
        return False


def import_texture(input_path, output_path, resize=None):
    """Import a texture file (PNG/JPG) with optional resize."""
    from PIL import Image

    img = Image.open(input_path).convert("RGBA")
    if resize:
        img = img.resize((resize, resize), Image.LANCZOS)
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    img.save(output_path)
    print(f"[UE Import] Texture saved: {output_path} ({img.size[0]}x{img.size[1]})")
    return True


def batch_import(input_dir, output_dir, size=64, mode="topdown"):
    """Import all supported files from a directory."""
    os.makedirs(output_dir, exist_ok=True)
    supported_3d = {".glb", ".gltf", ".fbx", ".obj", ".blend"}
    supported_tex = {".png", ".jpg", ".jpeg", ".bmp", ".tga"}

    count = 0
    for f in sorted(os.listdir(input_dir)):
        ext = os.path.splitext(f)[1].lower()
        path = os.path.join(input_dir, f)
        name = os.path.splitext(f)[0]

        if ext in supported_3d:
            out = os.path.join(output_dir, f"{name}_sheet.png")
            if convert_3d_to_spritesheet(path, out, size, mode):
                count += 1
        elif ext in supported_tex:
            out = os.path.join(output_dir, f"{name}.png")
            if import_texture(path, out, resize=size):
                count += 1

    print(f"[UE Import] Batch complete: {count} assets imported to {output_dir}")
    return count


def generate_stamps(output_dir, stamps_path):
    """Generate a stamps file for all imported assets in a directory."""
    from PIL import Image

    stamps = []
    for f in sorted(os.listdir(output_dir)):
        if not f.endswith(".png"): continue
        path = os.path.join(output_dir, f)
        img = Image.open(path)
        name = os.path.splitext(f)[0].replace("_", " ").title()
        w, h = img.size

        # Guess category from name
        cat = "misc"
        name_lower = f.lower()
        if any(k in name_lower for k in ["tree", "bush", "plant", "flower", "grass"]):
            cat = "tree"
        elif any(k in name_lower for k in ["house", "wall", "door", "bridge", "castle", "tower"]):
            cat = "building"
        elif any(k in name_lower for k in ["chair", "table", "bed", "chest", "shelf", "torch"]):
            cat = "furniture"
        elif any(k in name_lower for k in ["knight", "warrior", "mage", "skeleton", "slime", "character"]):
            cat = "character"

        stamps.append(f"{name}|0|0|{w}|{h}|{cat}")

    with open(stamps_path, "w") as f:
        f.write("\n".join(stamps) + "\n")
    print(f"[UE Import] Stamps file: {stamps_path} ({len(stamps)} entries)")


def main():
    parser = argparse.ArgumentParser(description="Import Unreal Engine assets into Twilight Engine")
    parser.add_argument("--input", "-i", required=True, help="Input file or directory")
    parser.add_argument("--output", "-o", required=True, help="Output file or directory")
    parser.add_argument("--size", type=int, default=64, help="Sprite size in pixels (default 64)")
    parser.add_argument("--mode", default="topdown", choices=["topdown", "side"],
                       help="Camera mode: topdown (RPG) or side (platformer)")
    parser.add_argument("--resize", type=int, default=None, help="Resize texture to NxN pixels")
    parser.add_argument("--stamps", default=None, help="Generate stamps file at this path")
    args = parser.parse_args()

    if os.path.isdir(args.input):
        batch_import(args.input, args.output, args.size, args.mode)
        if args.stamps:
            generate_stamps(args.output, args.stamps)
    elif os.path.isfile(args.input):
        ext = os.path.splitext(args.input)[1].lower()
        if ext in (".glb", ".gltf", ".fbx", ".obj", ".blend"):
            convert_3d_to_spritesheet(args.input, args.output, args.size, args.mode)
        elif ext in (".png", ".jpg", ".jpeg", ".bmp", ".tga"):
            import_texture(args.input, args.output, args.resize)
        else:
            print(f"Unsupported format: {ext}")
            print("Supported: .glb, .gltf, .fbx, .obj, .blend, .png, .jpg")
            sys.exit(1)
    else:
        print(f"Input not found: {args.input}")
        sys.exit(1)


if __name__ == "__main__":
    main()
