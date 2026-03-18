#!/usr/bin/env python3
"""
Blender Python script: generate procedural tilesets by rendering 3D scenes.

Creates 3D geometry with procedural materials for each tile type, renders
from orthographic camera, and assembles into a single tileset PNG.

Usage:
    blender -b -P tools/blender_generate_tileset.py -- --style topdown \
        --output games/demo/assets/textures/blender_topdown_tileset.png --tile-size 32

    blender -b -P tools/blender_generate_tileset.py -- --style platformer \
        --output games/demo/assets/textures/blender_platformer_tileset.png --tile-size 32

Options:
    --style topdown|platformer   Tile style (default: topdown)
    --tile-size 32               Output pixel size per tile (default: 32)
    --output PATH                Output tileset PNG path
    --columns 8                  Number of columns in tileset grid (default: 8)
    --seed 42                    Random seed for procedural variation (default: 42)
"""

import bpy
import bmesh
import sys
import os
import json
import math
import tempfile
import shutil

# ---------------------------------------------------------------------------
# Argument parsing (everything after "--")
# ---------------------------------------------------------------------------
argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []


def get_arg(flag, default=None):
    if flag in argv:
        idx = argv.index(flag)
        if idx + 1 < len(argv):
            return argv[idx + 1]
    return default


style = get_arg("--style", "topdown")
tile_size = int(get_arg("--tile-size", "32"))
output_path = get_arg("--output", "/tmp/blender_tileset.png")
columns = int(get_arg("--columns", "8"))
seed = int(get_arg("--seed", "42"))

render_size = tile_size * 2  # Render at 2x for AA, downscale later

tmp_dir = tempfile.mkdtemp(prefix="blender_tileset_")
print(f"[tileset] Style: {style}, tile_size: {tile_size}, output: {output_path}")
print(f"[tileset] Temp dir: {tmp_dir}")

# ---------------------------------------------------------------------------
# Utility: clear scene
# ---------------------------------------------------------------------------

def clear_scene():
    """Remove all objects, materials, etc."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    # Clean orphan data
    for block in bpy.data.meshes:
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in bpy.data.materials:
        if block.users == 0:
            bpy.data.materials.remove(block)
    for block in bpy.data.textures:
        if block.users == 0:
            bpy.data.textures.remove(block)
    for block in bpy.data.images:
        if block.users == 0:
            bpy.data.images.remove(block)


# ---------------------------------------------------------------------------
# Utility: setup scene for rendering
# ---------------------------------------------------------------------------

def setup_scene(transparent=False):
    """Configure render settings."""
    scene = bpy.context.scene

    # Use EEVEE for speed
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except:
        try:
            scene.render.engine = "BLENDER_EEVEE"
        except:
            scene.render.engine = "CYCLES"

    scene.render.resolution_x = render_size
    scene.render.resolution_y = render_size
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = transparent
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    scene.render.image_settings.compression = 15

    # Disable unnecessary features for speed (attribute names vary by Blender version)
    if hasattr(scene, 'eevee'):
        for attr in ('use_ssr', 'use_bloom', 'use_gtao',
                      'use_raytracing', 'use_screen_space_refraction'):
            if hasattr(scene.eevee, attr):
                try:
                    setattr(scene.eevee, attr, False)
                except:
                    pass

    # World background
    if not bpy.data.worlds:
        bpy.ops.world.new()
    world = bpy.data.worlds[0]
    scene.world = world
    world.use_nodes = True
    bg_node = world.node_tree.nodes.get("Background")
    if bg_node:
        bg_node.inputs[0].default_value = (0.0, 0.0, 0.0, 1.0)


def setup_camera_topdown():
    """Orthographic camera looking straight down."""
    bpy.ops.object.camera_add(location=(0, 0, 2))
    cam = bpy.context.object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 1.0
    cam.rotation_euler = (0, 0, 0)
    bpy.context.scene.camera = cam
    return cam


def setup_camera_platformer():
    """Orthographic camera looking from the front (side view)."""
    bpy.ops.object.camera_add(location=(0, -2, 0))
    cam = bpy.context.object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 1.0
    cam.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.scene.camera = cam
    return cam


def setup_light_topdown():
    """Sun light at 45 degrees for top-down view."""
    bpy.ops.object.light_add(type='SUN', location=(1, -1, 3))
    light = bpy.context.object
    light.rotation_euler = (math.radians(45), math.radians(15), math.radians(30))
    light.data.energy = 3.0
    return light


def setup_light_platformer():
    """Sun light for side view."""
    bpy.ops.object.light_add(type='SUN', location=(1, -2, 2))
    light = bpy.context.object
    light.rotation_euler = (math.radians(50), math.radians(10), math.radians(20))
    light.data.energy = 3.0
    return light


# ---------------------------------------------------------------------------
# Material creation helpers
# ---------------------------------------------------------------------------

def create_material(name, base_color, roughness=0.8):
    """Create a simple Principled BSDF material."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # Get Principled BSDF
    bsdf = nodes.get("Principled BSDF")
    if not bsdf:
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")

    bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    return mat


def create_noisy_material(name, base_color, noise_scale=8.0, noise_strength=0.3,
                          roughness=0.8, noise_seed=0, noise_detail=4.0):
    """Create a material with noise texture mixed into base color."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # Clear default nodes
    for n in nodes:
        nodes.remove(n)

    # Create nodes
    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (400, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (200, 0)
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (0, 0)
    mix.inputs[0].default_value = noise_strength  # Factor
    mix.inputs[6].default_value = (*base_color, 1.0)  # Color A
    # Color B: slightly varied version
    varied = tuple(min(1.0, max(0.0, c + 0.15)) for c in base_color)
    mix.inputs[7].default_value = (*varied, 1.0)

    noise = nodes.new("ShaderNodeTexNoise")
    noise.location = (-200, 0)
    noise.inputs["Scale"].default_value = noise_scale
    noise.inputs["Detail"].default_value = noise_detail
    if hasattr(noise, "noise_dimensions"):
        noise.noise_dimensions = '3D'

    # Texture coordinate
    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-400, 0)

    # Links
    links.new(texcoord.outputs["Generated"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


def create_voronoi_material(name, base_color, voronoi_scale=12.0, roughness=0.85,
                            color_variation=0.15):
    """Create a material with voronoi texture for cobblestone look."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    for n in nodes:
        nodes.remove(n)

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (600, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (400, 0)
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (200, 0)
    mix.inputs[0].default_value = 0.5
    mix.inputs[6].default_value = (*base_color, 1.0)
    varied = tuple(min(1.0, max(0.0, c - color_variation)) for c in base_color)
    mix.inputs[7].default_value = (*varied, 1.0)

    voronoi = nodes.new("ShaderNodeTexVoronoi")
    voronoi.location = (-50, 0)
    voronoi.inputs["Scale"].default_value = voronoi_scale
    if hasattr(voronoi, "voronoi_dimensions"):
        voronoi.voronoi_dimensions = '3D'

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-250, 0)

    links.new(texcoord.outputs["Generated"], voronoi.inputs["Vector"])
    links.new(voronoi.outputs["Distance"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


def create_wave_material(name, base_color, wave_scale=6.0, roughness=0.3):
    """Create a material with wave-like noise for water."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    for n in nodes:
        nodes.remove(n)

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (600, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (400, 0)
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.2
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.5

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (200, 0)
    mix.inputs[0].default_value = 0.4
    mix.inputs[6].default_value = (*base_color, 1.0)
    lighter = tuple(min(1.0, c + 0.15) for c in base_color)
    mix.inputs[7].default_value = (*lighter, 1.0)

    wave = nodes.new("ShaderNodeTexWave")
    wave.location = (-50, 0)
    wave.inputs["Scale"].default_value = wave_scale
    wave.inputs["Distortion"].default_value = 3.0
    wave.inputs["Detail"].default_value = 3.0
    wave.wave_type = 'RINGS'

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-250, 0)

    links.new(texcoord.outputs["Generated"], wave.inputs["Vector"])
    links.new(wave.outputs["Fac"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


def create_gradient_material(name, top_color, bottom_color):
    """Create a vertical gradient material (for platformer sky, grass-top blocks, etc.)."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    for n in nodes:
        nodes.remove(n)

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (600, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (400, 0)
    bsdf.inputs["Roughness"].default_value = 0.9
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (200, 0)
    mix.inputs[6].default_value = (*bottom_color, 1.0)
    mix.inputs[7].default_value = (*top_color, 1.0)

    # Separate XYZ to get Z component for gradient
    sep = nodes.new("ShaderNodeSeparateXYZ")
    sep.location = (-50, 0)

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-250, 0)

    links.new(texcoord.outputs["Generated"], sep.inputs["Vector"])
    links.new(sep.outputs["Z"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


def create_split_material(name, top_color, bottom_color, split=0.7):
    """Create a material that's one color on top, another on bottom (sharp split).
    Used for grass-top blocks in platformer style."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    for n in nodes:
        nodes.remove(n)

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (800, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (600, 0)
    bsdf.inputs["Roughness"].default_value = 0.9
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (400, 0)
    mix.inputs[6].default_value = (*bottom_color, 1.0)
    mix.inputs[7].default_value = (*top_color, 1.0)

    # Use math node to create sharp split
    greater = nodes.new("ShaderNodeMath")
    greater.operation = 'GREATER_THAN'
    greater.location = (200, 0)
    greater.inputs[1].default_value = split

    sep = nodes.new("ShaderNodeSeparateXYZ")
    sep.location = (0, 0)

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-200, 0)

    # Add noise to the body
    noise = nodes.new("ShaderNodeTexNoise")
    noise.location = (-200, -200)
    noise.inputs["Scale"].default_value = 10.0
    noise.inputs["Detail"].default_value = 3.0

    mix2 = nodes.new("ShaderNodeMix")
    mix2.data_type = 'RGBA'
    mix2.location = (200, -200)
    mix2.inputs[0].default_value = 0.15
    mix2.inputs[6].default_value = (*bottom_color, 1.0)
    darker = tuple(max(0.0, c - 0.08) for c in bottom_color)
    mix2.inputs[7].default_value = (*darker, 1.0)

    # Final mix between textured body and top color
    mix_final = nodes.new("ShaderNodeMix")
    mix_final.data_type = 'RGBA'
    mix_final.location = (400, -100)
    mix_final.inputs[7].default_value = (*top_color, 1.0)

    links.new(texcoord.outputs["Generated"], sep.inputs["Vector"])
    links.new(texcoord.outputs["Generated"], noise.inputs["Vector"])
    links.new(sep.outputs["Z"], greater.inputs[0])
    links.new(noise.outputs["Fac"], mix2.inputs[0])
    links.new(greater.outputs["Value"], mix_final.inputs[0])
    links.new(mix2.outputs[2], mix_final.inputs[6])
    links.new(mix_final.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


def create_lined_material(name, base_color, line_color, direction='horizontal',
                          line_count=6, line_thickness=0.3):
    """Create a material with parallel lines (for road markings, wood grain)."""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    for n in nodes:
        nodes.remove(n)

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (600, 0)

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (400, 0)
    bsdf.inputs["Roughness"].default_value = 0.8
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.0

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.location = (200, 0)
    mix.inputs[6].default_value = (*base_color, 1.0)
    mix.inputs[7].default_value = (*line_color, 1.0)

    wave = nodes.new("ShaderNodeTexWave")
    wave.location = (-50, 0)
    wave.inputs["Scale"].default_value = line_count
    wave.inputs["Distortion"].default_value = 0.0
    wave.inputs["Detail"].default_value = 0.0
    wave.wave_type = 'BANDS'
    if direction == 'horizontal':
        wave.bands_direction = 'Z'
    else:
        wave.bands_direction = 'X'

    # Threshold to make sharp lines
    threshold = nodes.new("ShaderNodeMath")
    threshold.operation = 'GREATER_THAN'
    threshold.location = (50, -100)
    threshold.inputs[1].default_value = 1.0 - line_thickness

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-250, 0)

    links.new(texcoord.outputs["Generated"], wave.inputs["Vector"])
    links.new(wave.outputs["Fac"], threshold.inputs[0])
    links.new(threshold.outputs["Value"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    return mat


# ---------------------------------------------------------------------------
# Geometry creation
# ---------------------------------------------------------------------------

def create_plane(material, location=(0, 0, 0)):
    """Create a 1x1 plane with the given material."""
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.data.materials.append(material)
    return obj


def create_cube(material_top, material_sides, location=(0, 0, 0)):
    """Create a unit cube with different material on top vs sides.
    For platformer grass-block style tiles."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object

    # Assign materials
    obj.data.materials.append(material_sides)  # index 0
    obj.data.materials.append(material_top)     # index 1

    # Enter edit mode to assign top face to material_top
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bm = bmesh.from_edit_mesh(obj.data)
    bm.faces.ensure_lookup_table()

    # Find the top face (highest Z normal)
    for face in bm.faces:
        if face.normal.z > 0.5:
            face.material_index = 1  # top material
        else:
            face.material_index = 0  # sides material

    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode='OBJECT')

    return obj


def create_thin_platform(material, height=0.15, location=(0, 0, 0)):
    """Create a thin platform (wide and short)."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.scale = (1.0, 1.0, height)
    bpy.ops.object.transform_apply(scale=True)
    obj.data.materials.append(material)
    return obj


def create_bush(material, location=(0, 0, 0)):
    """Create a bush-like shape (sphere squished) for platformer decoration."""
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.35, segments=12, ring_count=8,
                                          location=(location[0], location[1], location[2] - 0.15))
    obj = bpy.context.object
    obj.scale = (1.2, 1.0, 0.8)
    bpy.ops.object.transform_apply(scale=True)
    obj.data.materials.append(material)
    return obj


def create_tree_trunk(material, location=(0, 0, 0)):
    """Create a tree trunk rectangle for platformer."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.scale = (0.25, 1.0, 0.8)
    bpy.ops.object.transform_apply(scale=True)
    obj.data.materials.append(material)
    return obj


def create_cloud(material, location=(0, 0, 0)):
    """Create a cloud wisp from merged spheres."""
    # Main body
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, segments=10, ring_count=6,
                                          location=location)
    main = bpy.context.object
    main.data.materials.append(material)

    # Left bump
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.18, segments=10, ring_count=6,
                                          location=(location[0] - 0.2, location[1], location[2] - 0.05))
    left = bpy.context.object
    left.data.materials.append(material)

    # Right bump
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.2, segments=10, ring_count=6,
                                          location=(location[0] + 0.2, location[1], location[2] + 0.02))
    right = bpy.context.object
    right.data.materials.append(material)

    # Join them
    bpy.ops.object.select_all(action='DESELECT')
    main.select_set(True)
    left.select_set(True)
    right.select_set(True)
    bpy.context.view_layer.objects.active = main
    bpy.ops.object.join()

    return main


# ---------------------------------------------------------------------------
# Render a single tile
# ---------------------------------------------------------------------------

def render_tile(index, name, transparent=False):
    """Render the current scene and save as tile_NNN.png. Returns filepath."""
    scene = bpy.context.scene
    scene.render.film_transparent = transparent
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"

    filepath = os.path.join(tmp_dir, f"tile_{index:03d}.png")
    scene.render.filepath = filepath
    bpy.ops.render.render(write_still=True)
    print(f"  [tile {index:03d}] Rendered: {name}")
    return filepath


# ---------------------------------------------------------------------------
# Top-Down tile definitions
# ---------------------------------------------------------------------------

def generate_topdown_tiles():
    """Generate all top-down tile types. Returns list of (name, filepath)."""
    tiles = []
    idx = 0

    tile_defs = [
        # Grass tiles (4 variants)
        ("grass_normal", lambda: create_plane(
            create_noisy_material("grass_normal", (0.15, 0.45, 0.12), noise_scale=8.0,
                                   noise_strength=0.3, noise_seed=0))),
        ("grass_variant", lambda: create_plane(
            create_noisy_material("grass_variant", (0.18, 0.50, 0.14), noise_scale=12.0,
                                   noise_strength=0.4, noise_seed=7))),
        ("grass_flowers", lambda: create_plane(
            create_noisy_material("grass_flowers", (0.17, 0.48, 0.13), noise_scale=18.0,
                                   noise_strength=0.5, noise_detail=8.0, noise_seed=42))),
        ("grass_dark", lambda: create_plane(
            create_noisy_material("grass_dark", (0.08, 0.30, 0.06), noise_scale=10.0,
                                   noise_strength=0.25, noise_seed=13))),

        # Dirt tiles (4 variants)
        ("dirt_normal", lambda: create_plane(
            create_noisy_material("dirt_normal", (0.35, 0.22, 0.10), noise_scale=10.0,
                                   noise_strength=0.3))),
        ("dirt_light", lambda: create_plane(
            create_noisy_material("dirt_light", (0.45, 0.32, 0.18), noise_scale=8.0,
                                   noise_strength=0.25))),
        ("dirt_gravel", lambda: create_plane(
            create_noisy_material("dirt_gravel", (0.38, 0.25, 0.14), noise_scale=20.0,
                                   noise_strength=0.5, noise_detail=8.0))),
        ("dirt_mud", lambda: create_plane(
            create_noisy_material("dirt_mud", (0.22, 0.14, 0.06), noise_scale=6.0,
                                   noise_strength=0.2, roughness=0.5))),

        # Stone/Path tiles (4 variants)
        ("stone_cobble", lambda: create_plane(
            create_voronoi_material("stone_cobble", (0.40, 0.40, 0.42), voronoi_scale=12.0))),
        ("stone_light", lambda: create_plane(
            create_voronoi_material("stone_light", (0.55, 0.55, 0.57), voronoi_scale=10.0))),
        ("stone_cracked", lambda: create_plane(
            create_voronoi_material("stone_cracked", (0.35, 0.35, 0.37), voronoi_scale=18.0,
                                     color_variation=0.25))),
        ("stone_mossy", lambda: create_plane(
            create_voronoi_material("stone_mossy", (0.30, 0.42, 0.28), voronoi_scale=14.0,
                                     color_variation=0.12))),

        # Water tiles (4 variants)
        ("water_normal", lambda: create_plane(
            create_wave_material("water_normal", (0.10, 0.25, 0.55), wave_scale=6.0))),
        ("water_deep", lambda: create_plane(
            create_wave_material("water_deep", (0.05, 0.12, 0.40), wave_scale=5.0,
                                  roughness=0.2))),
        ("water_shallow", lambda: create_plane(
            create_wave_material("water_shallow", (0.18, 0.40, 0.65), wave_scale=8.0,
                                  roughness=0.4))),
        ("water_shore", lambda: create_plane(
            create_gradient_material("water_shore", (0.10, 0.25, 0.55), (0.60, 0.52, 0.30)))),

        # Sand tiles (2 variants)
        ("sand_normal", lambda: create_plane(
            create_noisy_material("sand_normal", (0.70, 0.60, 0.35), noise_scale=12.0,
                                   noise_strength=0.2, roughness=0.9))),
        ("sand_dark", lambda: create_plane(
            create_noisy_material("sand_dark", (0.58, 0.48, 0.28), noise_scale=10.0,
                                   noise_strength=0.25, roughness=0.9))),

        # Dark/Cave tiles (4 variants)
        ("cave_dark", lambda: create_plane(
            create_noisy_material("cave_dark", (0.08, 0.06, 0.10), noise_scale=6.0,
                                   noise_strength=0.15))),
        ("cave_rock", lambda: create_plane(
            create_voronoi_material("cave_rock", (0.12, 0.10, 0.14), voronoi_scale=10.0,
                                     color_variation=0.06))),
        ("cave_crystal", lambda: create_plane(
            create_noisy_material("cave_crystal", (0.10, 0.08, 0.18), noise_scale=15.0,
                                   noise_strength=0.4, roughness=0.3))),
        ("cave_moss", lambda: create_plane(
            create_noisy_material("cave_moss", (0.06, 0.12, 0.08), noise_scale=8.0,
                                   noise_strength=0.3))),

        # Road tiles (4 variants)
        ("road_horizontal", lambda: create_plane(
            create_lined_material("road_h", (0.30, 0.30, 0.32), (0.40, 0.40, 0.38),
                                   direction='horizontal', line_count=4, line_thickness=0.15))),
        ("road_vertical", lambda: create_plane(
            create_lined_material("road_v", (0.30, 0.30, 0.32), (0.40, 0.40, 0.38),
                                   direction='vertical', line_count=4, line_thickness=0.15))),
        ("road_crossroad", lambda: create_plane(
            create_voronoi_material("road_cross", (0.32, 0.32, 0.34), voronoi_scale=6.0,
                                     color_variation=0.05))),
        ("road_sidewalk", lambda: create_plane(
            create_voronoi_material("road_side", (0.55, 0.53, 0.50), voronoi_scale=8.0,
                                     color_variation=0.08))),
    ]

    for name, create_fn in tile_defs:
        clear_scene()
        setup_scene(transparent=False)
        setup_camera_topdown()
        setup_light_topdown()
        create_fn()
        filepath = render_tile(idx, name, transparent=False)
        tiles.append((name, filepath))
        idx += 1

    return tiles


# ---------------------------------------------------------------------------
# Platformer tile definitions
# ---------------------------------------------------------------------------

def generate_platformer_tiles():
    """Generate all platformer tile types. Returns list of (name, filepath)."""
    tiles = []
    idx = 0

    # --- Opaque ground tiles ---
    opaque_defs = [
        # Ground tiles
        ("ground_grass_top", lambda: create_plane(
            create_split_material("ground_grass_top",
                                   top_color=(0.15, 0.50, 0.12),
                                   bottom_color=(0.35, 0.22, 0.10),
                                   split=0.7))),
        ("ground_dirt", lambda: create_plane(
            create_noisy_material("ground_dirt", (0.35, 0.22, 0.10), noise_scale=10.0,
                                   noise_strength=0.3))),
        ("ground_stone", lambda: create_plane(
            create_voronoi_material("ground_stone", (0.40, 0.40, 0.42), voronoi_scale=14.0,
                                     color_variation=0.15))),
        ("ground_underground", lambda: create_plane(
            create_noisy_material("ground_underground", (0.15, 0.12, 0.10), noise_scale=8.0,
                                   noise_strength=0.2))),

        # Platform tiles
        ("platform_grass", lambda: create_plane(
            create_split_material("platform_grass",
                                   top_color=(0.18, 0.55, 0.15),
                                   bottom_color=(0.30, 0.20, 0.08),
                                   split=0.5))),
        ("platform_wood", lambda: create_plane(
            create_lined_material("platform_wood", (0.40, 0.25, 0.12), (0.32, 0.18, 0.08),
                                   direction='horizontal', line_count=8, line_thickness=0.2))),

        # Background tiles
        ("bg_sky", lambda: create_plane(
            create_gradient_material("bg_sky", (0.35, 0.60, 0.90), (0.55, 0.75, 0.95)))),
        ("bg_sky_dark", lambda: create_plane(
            create_gradient_material("bg_sky_dark", (0.08, 0.10, 0.25), (0.15, 0.18, 0.35)))),

        # Additional ground types
        ("ground_sand", lambda: create_plane(
            create_noisy_material("ground_sand", (0.70, 0.60, 0.35), noise_scale=12.0,
                                   noise_strength=0.2, roughness=0.9))),
        ("ground_ice", lambda: create_plane(
            create_noisy_material("ground_ice", (0.65, 0.80, 0.90), noise_scale=6.0,
                                   noise_strength=0.15, roughness=0.15))),
        ("ground_lava_rock", lambda: create_plane(
            create_voronoi_material("ground_lava_rock", (0.18, 0.08, 0.05), voronoi_scale=10.0,
                                     color_variation=0.12))),
        ("ground_metal", lambda: create_plane(
            create_lined_material("ground_metal", (0.45, 0.45, 0.48), (0.38, 0.38, 0.40),
                                   direction='horizontal', line_count=6, line_thickness=0.1))),
    ]

    # --- Transparent decoration tiles ---
    transparent_defs = [
        ("deco_cloud", lambda: create_cloud(
            create_material("cloud_mat", (0.90, 0.92, 0.95), roughness=0.9))),
        ("deco_bush", lambda: create_bush(
            create_noisy_material("bush_mat", (0.12, 0.42, 0.10), noise_scale=15.0,
                                   noise_strength=0.4))),
        ("deco_tree_trunk", lambda: create_tree_trunk(
            create_lined_material("trunk_mat", (0.35, 0.22, 0.10), (0.28, 0.16, 0.06),
                                   direction='vertical', line_count=10, line_thickness=0.15))),
        ("deco_leaves", lambda: create_bush(
            create_noisy_material("leaves_mat", (0.10, 0.38, 0.08), noise_scale=20.0,
                                   noise_strength=0.5))),
    ]

    # Render opaque tiles
    for name, create_fn in opaque_defs:
        clear_scene()
        setup_scene(transparent=False)
        setup_camera_platformer()
        setup_light_platformer()
        create_fn()
        filepath = render_tile(idx, name, transparent=False)
        tiles.append((name, filepath))
        idx += 1

    # Render transparent tiles
    for name, create_fn in transparent_defs:
        clear_scene()
        setup_scene(transparent=True)
        setup_camera_platformer()
        setup_light_platformer()
        create_fn()
        filepath = render_tile(idx, name, transparent=True)
        tiles.append((name, filepath))
        idx += 1

    return tiles


# ---------------------------------------------------------------------------
# Assembly: combine tiles into tileset PNG
# ---------------------------------------------------------------------------

def assemble_tileset(tiles, output_path, tile_size, columns):
    """Load individual tile PNGs, downscale, and arrange into a grid tileset."""
    from PIL import Image

    num_tiles = len(tiles)
    rows = math.ceil(num_tiles / columns)

    tileset = Image.new("RGBA", (columns * tile_size, rows * tile_size), (0, 0, 0, 0))

    for i, (name, filepath) in enumerate(tiles):
        img = Image.open(filepath).convert("RGBA")
        # Downscale from render_size to tile_size with high quality
        img = img.resize((tile_size, tile_size), Image.LANCZOS)

        col = i % columns
        row = i // columns
        tileset.paste(img, (col * tile_size, row * tile_size))

    # Ensure output directory exists
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    tileset.save(output_path, "PNG")
    print(f"[tileset] Saved tileset: {output_path} ({tileset.size[0]}x{tileset.size[1]})")
    print(f"[tileset] {num_tiles} tiles in {columns}x{rows} grid")

    return tileset.size


def save_manifest(tiles, output_path, tile_size, columns):
    """Save a JSON manifest mapping tile index to name."""
    manifest_path = output_path.replace(".png", "_manifest.json")

    manifest = {
        "tile_size": tile_size,
        "columns": columns,
        "rows": math.ceil(len(tiles) / columns),
        "total_tiles": len(tiles),
        "tiles": {}
    }

    for i, (name, _) in enumerate(tiles):
        col = i % columns
        row = i // columns
        manifest["tiles"][str(i)] = {
            "name": name,
            "x": col * tile_size,
            "y": row * tile_size,
            "col": col,
            "row": row
        }

    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f"[tileset] Saved manifest: {manifest_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print(f"\n{'='*60}")
    print(f"  Blender Tileset Generator")
    print(f"  Style: {style} | Tile size: {tile_size}px | Columns: {columns}")
    print(f"{'='*60}\n")

    if style == "topdown":
        tiles = generate_topdown_tiles()
    elif style == "platformer":
        tiles = generate_platformer_tiles()
    else:
        print(f"[ERROR] Unknown style: {style}. Use 'topdown' or 'platformer'.")
        sys.exit(1)

    size = assemble_tileset(tiles, output_path, tile_size, columns)
    save_manifest(tiles, output_path, tile_size, columns)

    # Clean up temp files
    shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"\n{'='*60}")
    print(f"  Done! Output: {output_path}")
    print(f"  Tileset dimensions: {size[0]}x{size[1]} pixels")
    print(f"  Total tiles: {len(tiles)}")
    print(f"{'='*60}\n")


main()
