#!/usr/bin/env python3
"""
Blender Python script: render a 3D model into 9 sprite frames for a 3x3 sprite grid.

Layout: 3 columns (idle, walk_0, walk_1) x 3 rows (down, up, right).
Left-facing is rendered by flipping right horizontally at runtime.

Usage:
    blender -b model.blend -P tools/blender_render_sprites.py -- [options]

Options:
    --mode topdown|side    Camera perspective (default: topdown)
    --size 64              Output pixel size per frame (default: 64)
    --output dir/          Output directory for frames (default: /tmp/blender_sprites/)
    --transparent          Transparent background (default: on)
    --no-transparent       Opaque background
    --action walk          Animation action name (default: "walk" or first action)
"""

import bpy
import sys
import os
import math
import mathutils

# ---------------------------------------------------------------------------
# Argument parsing (everything after "--" in sys.argv)
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


def has_flag(flag):
    return flag in argv


mode = get_arg("--mode", "topdown")
size = int(get_arg("--size", "64"))
output_dir = get_arg("--output", "/tmp/blender_sprites/")
transparent = not has_flag("--no-transparent")  # default True
action_name = get_arg("--action", None)

render_size = size * 2  # render at 2x for AA downscale

os.makedirs(output_dir, exist_ok=True)

# ---------------------------------------------------------------------------
# Scene setup
# ---------------------------------------------------------------------------
scene = bpy.context.scene

# Use EEVEE for speed (Cycles is slower); fall back to Cycles if unavailable
if hasattr(bpy.types, "EEVEE"):
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in dir(bpy.types) else "BLENDER_EEVEE"
else:
    scene.render.engine = "CYCLES"

# Try setting EEVEE Next if the old name fails
try:
    _ = scene.render.engine
except:
    scene.render.engine = "BLENDER_EEVEE"

scene.render.resolution_x = render_size
scene.render.resolution_y = render_size
scene.render.resolution_percentage = 100
scene.render.film_transparent = transparent
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"

# Disable denoising (may not be available in all Blender builds)
scene.view_layers[0].use_pass_combined = True
try:
    scene.view_layers[0].cycles.use_denoising = False
except:
    pass
try:
    # EEVEE Next denoising
    scene.eevee.use_raytracing = False
except:
    pass

# Remove default objects we don't need (lights, default cube, default camera)
# But keep any mesh objects that are the model
# We'll create our own camera and light

# ---------------------------------------------------------------------------
# Find the main model object(s) and compute bounding box
# ---------------------------------------------------------------------------
mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
if not mesh_objects:
    print("ERROR: No mesh objects found in the scene!")
    sys.exit(1)

# Compute combined bounding box in world space
min_co = mathutils.Vector((float("inf"),) * 3)
max_co = mathutils.Vector((float("-inf"),) * 3)
for obj in mesh_objects:
    for corner in obj.bound_box:
        world_co = obj.matrix_world @ mathutils.Vector(corner)
        min_co.x = min(min_co.x, world_co.x)
        min_co.y = min(min_co.y, world_co.y)
        min_co.z = min(min_co.z, world_co.z)
        max_co.x = max(max_co.x, world_co.x)
        max_co.y = max(max_co.y, world_co.y)
        max_co.z = max(max_co.z, world_co.z)

center = (min_co + max_co) / 2
bbox_size = max_co - min_co
max_dim = max(bbox_size.x, bbox_size.y, bbox_size.z, 0.001)

# ---------------------------------------------------------------------------
# Camera setup
# ---------------------------------------------------------------------------
cam_data = bpy.data.cameras.new("SpriteCamera")
cam_data.type = "ORTHO"
# Set ortho scale to fit the model with some padding
cam_data.ortho_scale = max_dim * 1.3

cam_obj = bpy.data.objects.new("SpriteCamera", cam_data)
scene.collection.objects.link(cam_obj)
scene.camera = cam_obj

# ---------------------------------------------------------------------------
# Light setup — add a sun light if none exists
# ---------------------------------------------------------------------------
has_light = any(obj.type == "LIGHT" for obj in bpy.data.objects)
if not has_light:
    light_data = bpy.data.lights.new("SpriteLight", "SUN")
    light_data.energy = 3.0
    light_obj = bpy.data.objects.new("SpriteLight", light_data)
    light_obj.rotation_euler = (math.radians(45), math.radians(15), math.radians(30))
    scene.collection.objects.link(light_obj)

# ---------------------------------------------------------------------------
# Direction definitions: camera positions for each direction
#
# "down"  = character facing toward the camera (front view)
# "up"    = character facing away from camera (back view)
# "right" = character facing right (right side view)
#
# In topdown mode, camera is above at ~45 deg elevation.
# In side mode, camera is horizontal (platformer side view).
# ---------------------------------------------------------------------------
CAM_DIST = max_dim * 4  # far enough to avoid clipping

directions = {}

if mode == "topdown":
    elev = math.radians(45)
    # Front (down): camera in front of model, looking at model from +Y towards -Y
    directions["down"] = {
        "pos": mathutils.Vector((
            center.x,
            center.y + CAM_DIST * math.cos(elev),
            center.z + CAM_DIST * math.sin(elev)
        )),
        "rot": (elev, 0, 0),  # pitch down
    }
    # Back (up): camera behind model
    directions["up"] = {
        "pos": mathutils.Vector((
            center.x,
            center.y - CAM_DIST * math.cos(elev),
            center.z + CAM_DIST * math.sin(elev)
        )),
        "rot": (elev, 0, math.radians(180)),
    }
    # Right: camera to the right of model
    directions["right"] = {
        "pos": mathutils.Vector((
            center.x + CAM_DIST * math.cos(elev),
            center.y,
            center.z + CAM_DIST * math.sin(elev)
        )),
        "rot": (elev, 0, math.radians(90)),
    }
else:  # side mode (platformer)
    # Front (down): camera in front, horizontal
    directions["down"] = {
        "pos": mathutils.Vector((center.x, center.y + CAM_DIST, center.z)),
        "rot": (math.radians(90), 0, 0),
    }
    # Back (up): camera behind
    directions["up"] = {
        "pos": mathutils.Vector((center.x, center.y - CAM_DIST, center.z)),
        "rot": (math.radians(90), 0, math.radians(180)),
    }
    # Right: camera to the right
    directions["right"] = {
        "pos": mathutils.Vector((center.x + CAM_DIST, center.y, center.z)),
        "rot": (math.radians(90), 0, math.radians(90)),
    }

# ---------------------------------------------------------------------------
# Animation setup
# ---------------------------------------------------------------------------
# Find the animation action
anim_action = None
total_frames = 1

# Try to find the requested action or any action
all_actions = list(bpy.data.actions)
if action_name:
    for act in all_actions:
        if act.name.lower() == action_name.lower():
            anim_action = act
            break
if not anim_action and all_actions:
    # Try "walk" first, then first action
    for act in all_actions:
        if "walk" in act.name.lower():
            anim_action = act
            break
    if not anim_action:
        anim_action = all_actions[0]

if anim_action:
    # Apply the action to all armatures and mesh objects
    for obj in bpy.data.objects:
        if obj.type == "ARMATURE":
            if not obj.animation_data:
                obj.animation_data_create()
            obj.animation_data.action = anim_action
        elif obj.type == "MESH" and obj.parent and obj.parent.type == "ARMATURE":
            pass  # animated via armature
        elif obj.type == "MESH":
            if not obj.animation_data:
                obj.animation_data_create()
            obj.animation_data.action = anim_action

    frame_start = int(anim_action.frame_range[0])
    frame_end = int(anim_action.frame_range[1])
    total_frames = max(frame_end - frame_start, 1)
    print(f"Using action '{anim_action.name}' with {total_frames} frames "
          f"(range {frame_start}-{frame_end})")
else:
    frame_start = 0
    print("No animation found — rendering static poses")

# Compute the 3 frame indices: idle, walk_0, walk_1
if total_frames > 1:
    frame_indices = [
        frame_start,                            # idle (frame 0)
        frame_start + total_frames // 3,        # walk_0 (1/3 through)
        frame_start + 2 * total_frames // 3,    # walk_1 (2/3 through)
    ]
else:
    frame_indices = [frame_start, frame_start, frame_start]

print(f"Frame indices: {frame_indices}")

# ---------------------------------------------------------------------------
# Render loop
# ---------------------------------------------------------------------------
for dir_name, cam_setup in directions.items():
    cam_obj.location = cam_setup["pos"]
    # Use track-to constraint for more reliable aiming
    cam_obj.rotation_euler = mathutils.Euler((0, 0, 0))

    # Point camera at center
    direction_vec = center - cam_setup["pos"]
    rot_quat = direction_vec.to_track_quat("-Z", "Y")
    cam_obj.rotation_euler = rot_quat.to_euler()

    for frame_idx, anim_frame in enumerate(frame_indices):
        scene.frame_set(anim_frame)
        # Force dependency graph update
        bpy.context.view_layer.update()

        filename = f"frame_{dir_name}_{frame_idx}.png"
        filepath = os.path.join(output_dir, filename)
        scene.render.filepath = filepath

        print(f"Rendering {filename} (anim frame {anim_frame})...")
        bpy.ops.render.render(write_still=True)
        print(f"  Saved: {filepath}")

print(f"\nAll 9 frames rendered to: {output_dir}")
print("Directions: down, up, right")
print("Frames per direction: 3 (idle, walk_0, walk_1)")
