#!/usr/bin/env python3
"""
Blender Python script: create a simple humanoid character from primitives with a walk animation.

Usage:
    blender -b -P tools/blender_create_sample.py -- --output path/to/character.blend
"""

import bpy
import sys
import os
import math

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
argv = sys.argv
if "--" in argv:
    argv = argv[argv.index("--") + 1:]
else:
    argv = []

output_path = None
if "--output" in argv:
    idx = argv.index("--output")
    if idx + 1 < len(argv):
        output_path = argv[idx + 1]

if not output_path:
    output_path = "/tmp/sample_character.blend"

# Ensure output directory exists
os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

# ---------------------------------------------------------------------------
# Clean scene
# ---------------------------------------------------------------------------
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

# Also clean orphan data
for block in bpy.data.meshes:
    if block.users == 0:
        bpy.data.meshes.remove(block)
for block in bpy.data.materials:
    if block.users == 0:
        bpy.data.materials.remove(block)

# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------
def make_material(name, color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = color
    return mat

mat_skin = make_material("Skin", (0.87, 0.72, 0.53, 1.0))
mat_body = make_material("Body_Blue", (0.2, 0.3, 0.7, 1.0))
mat_legs = make_material("Legs_Brown", (0.4, 0.25, 0.1, 1.0))
mat_arms = make_material("Arms_Skin", (0.82, 0.67, 0.48, 1.0))

# ---------------------------------------------------------------------------
# Create armature
# ---------------------------------------------------------------------------
bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
armature_obj = bpy.context.active_object
armature_obj.name = "CharacterArmature"
armature = armature_obj.data
armature.name = "CharacterArmature"

# Remove default bone
for bone in armature.edit_bones:
    armature.edit_bones.remove(bone)

# Create bones
def add_bone(name, head, tail, parent_name=None):
    bone = armature.edit_bones.new(name)
    bone.head = head
    bone.tail = tail
    if parent_name:
        bone.parent = armature.edit_bones[parent_name]
    return bone

# Skeleton (character faces -Y by default in Blender, standing along Z)
# Root/hips at origin
add_bone("Root",       (0, 0, 0.5),    (0, 0, 0.55))
add_bone("Spine",      (0, 0, 0.55),   (0, 0, 0.9),   "Root")
add_bone("Head",       (0, 0, 0.9),    (0, 0, 1.15),   "Spine")

add_bone("Arm_L",      (0.15, 0, 0.85), (0.15, 0, 0.55), "Spine")
add_bone("Arm_R",      (-0.15, 0, 0.85), (-0.15, 0, 0.55), "Spine")

add_bone("Leg_L",      (0.07, 0, 0.5),  (0.07, 0, 0.0),  "Root")
add_bone("Leg_R",      (-0.07, 0, 0.5), (-0.07, 0, 0.0), "Root")

bpy.ops.object.mode_set(mode="OBJECT")

# ---------------------------------------------------------------------------
# Create mesh parts and parent to armature
# ---------------------------------------------------------------------------
def create_mesh(name, primitive, location, scale, material, bone_name):
    """Create a mesh primitive, assign material, parent to armature with bone vertex group."""
    bpy.ops.object.select_all(action="DESELECT")

    if primitive == "cube":
        bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    elif primitive == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, segments=16, ring_count=8, location=location)
    elif primitive == "cylinder":
        bpy.ops.mesh.primitive_cylinder_add(radius=0.5, depth=1, vertices=12, location=location)

    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale

    # Apply scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    # Assign material
    obj.data.materials.append(material)

    # Create vertex group for the bone
    vg = obj.vertex_groups.new(name=bone_name)
    vg.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")

    # Parent to armature with armature modifier
    obj.parent = armature_obj
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = armature_obj

    return obj


# Body (torso)
create_mesh("Body", "cube", (0, 0, 0.7), (0.25, 0.15, 0.35), mat_body, "Spine")

# Head
create_mesh("Head", "sphere", (0, 0, 1.0), (0.2, 0.2, 0.22), mat_skin, "Head")

# Left arm
create_mesh("Arm_L", "cylinder", (0.15, 0, 0.7), (0.05, 0.05, 0.3), mat_arms, "Arm_L")

# Right arm
create_mesh("Arm_R", "cylinder", (-0.15, 0, 0.7), (0.05, 0.05, 0.3), mat_arms, "Arm_R")

# Left leg
create_mesh("Leg_L", "cylinder", (0.07, 0, 0.25), (0.06, 0.06, 0.25), mat_legs, "Leg_L")

# Right leg
create_mesh("Leg_R", "cylinder", (-0.07, 0, 0.25), (0.06, 0.06, 0.25), mat_legs, "Leg_R")

# ---------------------------------------------------------------------------
# Create walk animation
# ---------------------------------------------------------------------------
scene = bpy.context.scene
scene.frame_start = 1
scene.frame_end = 12

# Disable denoising to avoid issues on builds without OpenImageDenoise
try:
    scene.view_layers[0].cycles.use_denoising = False
except:
    pass

# Select armature and enter pose mode
bpy.context.view_layer.objects.active = armature_obj
bpy.ops.object.mode_set(mode="POSE")

# Create action
action = bpy.data.actions.new("walk")
armature_obj.animation_data_create()
armature_obj.animation_data.action = action

pose_bones = armature_obj.pose.bones

# Animation parameters
SWING_ANGLE = math.radians(30)  # leg/arm swing amplitude
BOB_HEIGHT = 0.02  # body bob amplitude
TOTAL_FRAMES = 12

# Keyframe helper
def set_bone_rotation_keyframe(bone_name, frame, angle_x):
    """Set rotation keyframe on a pose bone (rotation around local X axis)."""
    bone = pose_bones[bone_name]
    bone.rotation_mode = "XYZ"
    bone.rotation_euler = (angle_x, 0, 0)
    bone.keyframe_insert(data_path="rotation_euler", frame=frame)


def set_bone_location_keyframe(bone_name, frame, offset_z):
    """Set location keyframe for body bob."""
    bone = pose_bones[bone_name]
    bone.location = (0, 0, offset_z)
    bone.keyframe_insert(data_path="location", frame=frame)


# Walk cycle keyframes
# Full cycle: frame 1 to 12, loops back
# Legs: L forward, R back -> both center -> L back, R forward -> both center
for frame in range(1, TOTAL_FRAMES + 1):
    t = (frame - 1) / TOTAL_FRAMES  # 0 to ~1
    angle = math.sin(t * 2 * math.pi)

    # Legs swing opposite to each other
    set_bone_rotation_keyframe("Leg_L", frame, SWING_ANGLE * angle)
    set_bone_rotation_keyframe("Leg_R", frame, -SWING_ANGLE * angle)

    # Arms swing opposite to legs (natural walking motion)
    set_bone_rotation_keyframe("Arm_L", frame, -SWING_ANGLE * 0.7 * angle)
    set_bone_rotation_keyframe("Arm_R", frame, SWING_ANGLE * 0.7 * angle)

    # Body bob: up when legs are together (2x frequency)
    bob = BOB_HEIGHT * abs(math.sin(t * 2 * math.pi))
    set_bone_location_keyframe("Root", frame, bob)

# Make the animation cyclic by duplicating frame 1 at frame 13
# (Blender will interpolate between 12 and 1 for looping)

# Set interpolation to linear for smooth looping
for fcurve in action.fcurves:
    for keyframe in fcurve.keyframe_points:
        keyframe.interpolation = "LINEAR"

bpy.ops.object.mode_set(mode="OBJECT")

# ---------------------------------------------------------------------------
# Save
# ---------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(output_path))
print(f"\nSample character saved to: {os.path.abspath(output_path)}")
print("  - 6 mesh parts (body, head, 2 arms, 2 legs)")
print("  - Armature with 7 bones")
print("  - 12-frame 'walk' animation")
