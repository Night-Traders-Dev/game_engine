"""
Blender 4.x script: Generate game props as top-down sprites + ground tiles.
Run: blender -b -P tools/blender_generate_props.py -- --output tileset.png --tile-size 32
"""
import bpy, sys, os, math, tempfile, shutil

# Parse args after '--'
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
output_path = "tileset.png"
tile_size = 32
for i, a in enumerate(argv):
    if a == "--output" and i+1 < len(argv): output_path = argv[i+1]
    elif a == "--tile-size" and i+1 < len(argv): tile_size = int(argv[i+1])

tmpdir = tempfile.mkdtemp(prefix="tw_props_")
render_size = tile_size * 2  # 2x for AA

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)
    for mesh in list(bpy.data.meshes): bpy.data.meshes.remove(mesh)

def make_mat(name, r, g, b, a=1.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf: bsdf.inputs["Base Color"].default_value = (r, g, b, a)
    return mat

def add_cube(loc, scale, mat):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.active_object
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj

def add_cylinder(loc, r, h, mat, segs=16):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, location=loc, vertices=segs)
    obj = bpy.context.active_object
    obj.data.materials.append(mat)
    return obj

def add_sphere(loc, r, mat, segs=16):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=loc, segments=segs, ring_count=8)
    obj = bpy.context.active_object
    obj.data.materials.append(mat)
    return obj

def add_cone(loc, r, h, mat, verts=16):
    bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=0, depth=h, location=loc, vertices=verts)
    obj = bpy.context.active_object
    obj.data.materials.append(mat)
    return obj

def setup_render(w_px, h_px):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x = w_px
    scene.render.resolution_y = h_px
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.eevee.taa_render_samples = 32

def setup_camera(ortho_scale):
    bpy.ops.object.camera_add(location=(0, 0, 10))
    cam = bpy.context.active_object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = ortho_scale
    bpy.context.scene.camera = cam
    return cam

def setup_light():
    bpy.ops.object.light_add(type='SUN', location=(3, -3, 8))
    light = bpy.context.active_object
    light.data.energy = 3.0
    light.rotation_euler = (math.radians(45), 0, math.radians(45))

def render_to(path, w_tiles, h_tiles):
    setup_render(w_tiles * render_size, h_tiles * render_size)
    cam = setup_camera(max(w_tiles, h_tiles))
    setup_light()
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

# ═══════════════════════════════════════════════
# GROUND TILE DEFINITIONS
# ═══════════════════════════════════════════════
ground_tiles = [
    # (name, base_r, base_g, base_b, noise_scale)
    ("grass_normal", 0.15, 0.45, 0.12, 5),
    ("grass_variant", 0.18, 0.50, 0.14, 8),
    ("grass_flowers", 0.20, 0.48, 0.15, 12),
    ("grass_dark", 0.10, 0.35, 0.08, 5),
    ("dirt_normal", 0.35, 0.22, 0.10, 6),
    ("dirt_light", 0.42, 0.30, 0.15, 5),
    ("dirt_gravel", 0.30, 0.25, 0.18, 15),
    ("dirt_mud", 0.25, 0.18, 0.08, 4),
    ("stone_cobble", 0.40, 0.40, 0.42, 10),
    ("stone_light", 0.50, 0.50, 0.48, 8),
    ("stone_cracked", 0.35, 0.35, 0.38, 12),
    ("stone_mossy", 0.30, 0.40, 0.32, 9),
    ("water_normal", 0.10, 0.25, 0.55, 6),
    ("water_deep", 0.05, 0.15, 0.45, 5),
    ("water_shallow", 0.18, 0.35, 0.60, 7),
    ("water_shore", 0.40, 0.35, 0.25, 4),
    ("sand_normal", 0.70, 0.60, 0.35, 5),
    ("sand_dark", 0.55, 0.45, 0.25, 6),
    ("cave_dark", 0.08, 0.06, 0.10, 4),
    ("cave_rock", 0.15, 0.12, 0.14, 8),
    ("cave_crystal", 0.12, 0.10, 0.20, 10),
    ("cave_moss", 0.10, 0.15, 0.10, 7),
    ("road_h", 0.30, 0.30, 0.32, 3),
    ("road_v", 0.32, 0.32, 0.34, 3),
    ("road_cross", 0.28, 0.28, 0.30, 4),
    ("road_sidewalk", 0.45, 0.44, 0.42, 3),
]

def render_ground_tile(name, r, g, b, noise_scale, idx):
    clear_scene()
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0,0,0))
    obj = bpy.context.active_object
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    # Add noise texture for variation
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = noise_scale
    noise.inputs["Detail"].default_value = 4
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.inputs[0].default_value = 0.3  # Factor
    mix.inputs[6].default_value = (r, g, b, 1)  # A color
    mix.inputs[7].default_value = (r*0.7, g*0.7, b*0.7, 1)  # B color
    links.new(noise.outputs["Fac"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    obj.data.materials.append(mat)
    setup_render(render_size, render_size)
    setup_camera(1.0)
    setup_light()
    path = os.path.join(tmpdir, f"ground_{idx:03d}_{name}.png")
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return path

# ═══════════════════════════════════════════════
# PROP DEFINITIONS
# ═══════════════════════════════════════════════
# (name, w_tiles, h_tiles, category, build_function)

def build_house():
    m_wall = make_mat("wall", 0.45, 0.30, 0.15)
    m_roof = make_mat("roof", 0.55, 0.15, 0.10)
    m_door = make_mat("door", 0.30, 0.18, 0.08)
    add_cube((0,0,0.4), (0.8, 0.8, 0.4), m_wall)
    add_cone((0,0,1.0), 0.6, 0.5, m_roof)
    add_cube((0,-0.4,0.2), (0.15, 0.02, 0.2), m_door)

def build_cabin():
    m = make_mat("cabin", 0.40, 0.25, 0.12)
    m2 = make_mat("roof_c", 0.35, 0.20, 0.10)
    add_cube((0,0,0.3), (0.6, 0.5, 0.3), m)
    add_cube((0,0,0.65), (0.65, 0.55, 0.05), m2)

def build_stone_wall():
    add_cube((0,0,0.2), (0.9, 0.3, 0.2), make_mat("sw", 0.4, 0.4, 0.42))

def build_bridge():
    m = make_mat("br", 0.40, 0.28, 0.15)
    add_cube((0,0,0.05), (1.8, 0.4, 0.05), m)
    add_cube((-0.85,0,0.12), (0.05, 0.4, 0.07), m)
    add_cube((0.85,0,0.12), (0.05, 0.4, 0.07), m)

def build_fountain():
    m = make_mat("fn_stone", 0.45, 0.45, 0.48)
    m2 = make_mat("fn_water", 0.15, 0.30, 0.60)
    add_cylinder((0,0,0.15), 0.45, 0.3, m)
    add_cylinder((0,0,0.25), 0.38, 0.1, m2)
    add_cylinder((0,0,0.4), 0.08, 0.3, m)

def build_well():
    m = make_mat("well_s", 0.40, 0.40, 0.42)
    m2 = make_mat("well_w", 0.35, 0.22, 0.12)
    add_cylinder((0,0,0.15), 0.3, 0.3, m)
    add_cube((0,0,0.45), (0.05, 0.5, 0.05), m2)
    add_cube((0,0,0.50), (0.5, 0.05, 0.03), m2)

def build_door():
    add_cube((0,0,0.2), (0.3, 0.05, 0.2), make_mat("door", 0.35, 0.20, 0.10))

def build_oak():
    add_cylinder((0,0,0.3), 0.08, 0.6, make_mat("trunk", 0.35, 0.20, 0.10))
    add_sphere((0,0,0.75), 0.4, make_mat("leaves", 0.15, 0.50, 0.12))

def build_pine():
    add_cylinder((0,0,0.25), 0.05, 0.5, make_mat("pt", 0.35, 0.20, 0.10))
    add_cone((0,0,0.7), 0.25, 0.6, make_mat("pl", 0.10, 0.40, 0.10))

def build_big_tree():
    add_cylinder((0,0,0.4), 0.12, 0.8, make_mat("bt", 0.30, 0.18, 0.08))
    add_sphere((0,0,0.95), 0.55, make_mat("bl", 0.12, 0.45, 0.10))

def build_dead_tree():
    m = make_mat("dt", 0.30, 0.20, 0.12)
    add_cylinder((0,0,0.3), 0.05, 0.6, m)
    add_cube((0.1,0,0.5), (0.15, 0.02, 0.02), m)
    add_cube((-0.08,0.05,0.45), (0.12, 0.02, 0.02), m)

def build_bush_round():
    add_sphere((0,0,0.15), 0.25, make_mat("bush", 0.18, 0.48, 0.15))

def build_bush_small():
    add_sphere((0,0,0.1), 0.15, make_mat("bsm", 0.20, 0.45, 0.18))

def build_cactus_tall():
    m = make_mat("cac", 0.20, 0.50, 0.15)
    add_cylinder((0,0,0.35), 0.08, 0.7, m)
    add_cylinder((0.15,0,0.3), 0.05, 0.2, m)
    add_cylinder((-0.12,0,0.25), 0.05, 0.15, m)

def build_cactus_short():
    add_cylinder((0,0,0.15), 0.12, 0.3, make_mat("cs", 0.22, 0.52, 0.18))

def build_fir():
    add_cone((0,0,0.4), 0.25, 0.8, make_mat("fir", 0.08, 0.38, 0.08))

def build_snow_pine():
    add_cone((0,0,0.4), 0.25, 0.8, make_mat("sp", 0.60, 0.72, 0.65))

def build_palm():
    add_cylinder((0,0,0.4), 0.04, 0.8, make_mat("pm", 0.40, 0.25, 0.12))
    add_sphere((0.15,0,0.85), 0.25, make_mat("pmf", 0.15, 0.50, 0.12))

def build_flower_bed():
    add_cube((0,0,0.02), (0.4, 0.4, 0.02), make_mat("fb", 0.18, 0.45, 0.15))
    add_sphere((0.1,0.1,0.08), 0.04, make_mat("fr", 0.85, 0.15, 0.15))
    add_sphere((-0.1,0.05,0.08), 0.04, make_mat("fy", 0.85, 0.80, 0.15))
    add_sphere((0,-0.1,0.08), 0.04, make_mat("fp", 0.70, 0.15, 0.65))

def build_table():
    m = make_mat("tbl", 0.40, 0.25, 0.12)
    add_cube((0,0,0.2), (0.6, 0.35, 0.03), m)
    for dx, dy in [(-0.25,-0.13), (0.25,-0.13), (-0.25,0.13), (0.25,0.13)]:
        add_cube((dx,dy,0.1), (0.03, 0.03, 0.1), m)

def build_chair():
    m = make_mat("chr", 0.38, 0.22, 0.10)
    add_cube((0,0,0.12), (0.2, 0.2, 0.02), m)
    add_cube((0,0.1,0.2), (0.2, 0.02, 0.08), m)

def build_bed():
    m = make_mat("bed_f", 0.35, 0.20, 0.10)
    m2 = make_mat("bed_w", 0.85, 0.82, 0.78)
    add_cube((0,0,0.1), (0.6, 0.35, 0.08), m)
    add_cube((0,0,0.14), (0.55, 0.30, 0.02), m2)
    add_cube((-0.25,0,0.18), (0.08, 0.25, 0.04), m2)

def build_chest():
    m = make_mat("chst", 0.40, 0.28, 0.12)
    m2 = make_mat("chst_g", 0.75, 0.65, 0.15)
    add_cube((0,0,0.1), (0.25, 0.18, 0.1), m)
    add_cube((0,0,0.16), (0.20, 0.12, 0.02), m2)

def build_barrel():
    m = make_mat("brl", 0.40, 0.25, 0.12)
    m2 = make_mat("brl_b", 0.25, 0.15, 0.08)
    add_cylinder((0,0,0.18), 0.15, 0.36, m)
    add_cylinder((0,0,0.08), 0.16, 0.02, m2)
    add_cylinder((0,0,0.28), 0.16, 0.02, m2)

def build_fireplace():
    m = make_mat("fp_s", 0.35, 0.35, 0.38)
    m2 = make_mat("fp_f", 0.90, 0.45, 0.10)
    add_cube((0,0,0.1), (0.35, 0.30, 0.1), m)
    add_sphere((0,0,0.18), 0.08, m2)

def build_torch():
    add_cylinder((0,0,0.15), 0.02, 0.3, make_mat("tc", 0.35, 0.20, 0.10))
    add_sphere((0,0,0.32), 0.05, make_mat("tf", 0.95, 0.60, 0.10))

def build_cauldron():
    add_sphere((0,0,0.05), 0.2, make_mat("cld", 0.15, 0.15, 0.18))
    add_sphere((0,0,0.12), 0.12, make_mat("cld_l", 0.20, 0.50, 0.15))

def build_bookshelf():
    m = make_mat("bs", 0.35, 0.22, 0.10)
    add_cube((0,0,0.3), (0.6, 0.2, 0.3), m)
    for i, c in enumerate([(0.6,0.1,0.1), (0.1,0.1,0.6), (0.5,0.4,0.1), (0.1,0.5,0.1)]):
        add_cube((-0.2+i*0.12, 0, 0.25+i*0.05), (0.05, 0.15, 0.12), make_mat(f"bk{i}", *c))

def build_warrior():
    add_cylinder((0,0,0.2), 0.12, 0.35, make_mat("wr_b", 0.45, 0.45, 0.50))
    add_sphere((0,0,0.42), 0.1, make_mat("wr_h", 0.75, 0.60, 0.50))
    add_cube((0.2,0,0.2), (0.15, 0.02, 0.02), make_mat("wr_s", 0.55, 0.55, 0.58))

def build_mage():
    add_cylinder((0,0,0.2), 0.12, 0.35, make_mat("mg_b", 0.20, 0.15, 0.50))
    add_sphere((0,0,0.42), 0.1, make_mat("mg_h", 0.70, 0.55, 0.45))
    add_cone((0,0,0.55), 0.12, 0.15, make_mat("mg_hat", 0.15, 0.10, 0.45))
    add_cylinder((0.18,0,0.3), 0.015, 0.5, make_mat("mg_st", 0.40, 0.25, 0.12))

def build_skeleton():
    add_cylinder((0,0,0.18), 0.10, 0.30, make_mat("sk_b", 0.85, 0.82, 0.75))
    add_sphere((0,0,0.38), 0.09, make_mat("sk_h", 0.90, 0.88, 0.80))

def build_slime():
    add_sphere((0,0,0.1), 0.2, make_mat("sl", 0.20, 0.80, 0.15))

def build_golem():
    add_cube((0,0,0.22), (0.22, 0.18, 0.22), make_mat("gl", 0.40, 0.35, 0.30))
    add_sphere((0,0,0.42), 0.12, make_mat("gl_h", 0.45, 0.40, 0.35))

def build_chicken():
    add_sphere((0,0,0.08), 0.1, make_mat("ck", 0.90, 0.88, 0.82))
    add_cone((0,-0.1,0.08), 0.02, 0.06, make_mat("ck_b", 0.90, 0.55, 0.10))

def build_cow():
    m = make_mat("cow", 0.85, 0.82, 0.78)
    add_cube((0,0,0.15), (0.25, 0.15, 0.12), m)
    add_sphere((0,-0.15,0.18), 0.08, m)

def build_pig():
    add_sphere((0,0,0.1), 0.15, make_mat("pig", 0.90, 0.65, 0.60))

def build_sheep():
    add_sphere((0,0,0.12), 0.18, make_mat("shp", 0.92, 0.90, 0.88))

def build_crate():
    m = make_mat("crt", 0.42, 0.28, 0.14)
    add_cube((0,0,0.12), (0.22, 0.22, 0.12), m)
    add_cube((0,0,0.12), (0.22, 0.02, 0.12), make_mat("crt_l", 0.30, 0.18, 0.08))
    add_cube((0,0,0.12), (0.02, 0.22, 0.12), make_mat("crt_l2", 0.30, 0.18, 0.08))

def build_rock1():
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.18, location=(0,0,0.1), subdivisions=2)
    obj = bpy.context.active_object
    obj.scale = (1.2, 0.9, 0.7)
    obj.data.materials.append(make_mat("rk1", 0.42, 0.40, 0.38))

def build_rock2():
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.15, location=(0,0,0.08), subdivisions=2)
    obj = bpy.context.active_object
    obj.scale = (0.8, 1.1, 0.6)
    obj.data.materials.append(make_mat("rk2", 0.35, 0.33, 0.32))

def build_rock3():
    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.10, location=(0,0,0.06), subdivisions=1)
    obj = bpy.context.active_object
    obj.data.materials.append(make_mat("rk3", 0.48, 0.46, 0.44))

def build_sign():
    m = make_mat("sgn", 0.38, 0.24, 0.12)
    add_cylinder((0,0,0.2), 0.02, 0.4, m)
    add_cube((0,0,0.38), (0.18, 0.02, 0.10), m)

def build_tombstone():
    add_cube((0,0,0.15), (0.15, 0.05, 0.15), make_mat("ts", 0.42, 0.42, 0.45))

def build_hay():
    add_cylinder((0,0,0.12), 0.15, 0.18, make_mat("hay", 0.80, 0.70, 0.30))

def build_lamp():
    add_cylinder((0,0,0.25), 0.02, 0.5, make_mat("lmp", 0.15, 0.15, 0.18))
    add_sphere((0,0,0.52), 0.06, make_mat("lmp_l", 0.95, 0.85, 0.30))

def build_log():
    add_cylinder((0,0,0.08), 0.08, 0.6, make_mat("log", 0.38, 0.22, 0.10))
    bpy.context.active_object.rotation_euler = (0, math.pi/2, 0)

def build_pumpkin():
    add_sphere((0,0,0.1), 0.15, make_mat("pmp", 0.90, 0.55, 0.10))

def build_flower_r():
    add_cylinder((0,0,0.1), 0.01, 0.2, make_mat("frs", 0.15, 0.45, 0.12))
    add_sphere((0,0,0.22), 0.04, make_mat("frr", 0.85, 0.15, 0.12))

def build_flower_y():
    add_cylinder((0,0,0.1), 0.01, 0.2, make_mat("fys", 0.15, 0.45, 0.12))
    add_sphere((0,0,0.22), 0.04, make_mat("fyy", 0.90, 0.82, 0.15))

# All props: (name, w_tiles, h_tiles, category, builder)
props = [
    ("House", 3, 3, "building", build_house),
    ("Cabin", 2, 2, "building", build_cabin),
    ("Stone Wall", 2, 1, "building", build_stone_wall),
    ("Bridge", 4, 1, "building", build_bridge),
    ("Fountain", 2, 2, "building", build_fountain),
    ("Well", 1, 1, "building", build_well),
    ("Door", 1, 1, "building", build_door),
    ("Oak Tree", 2, 2, "tree", build_oak),
    ("Pine Tree", 1, 2, "tree", build_pine),
    ("Big Tree", 3, 3, "tree", build_big_tree),
    ("Dead Tree", 1, 2, "tree", build_dead_tree),
    ("Bush Round", 1, 1, "tree", build_bush_round),
    ("Bush Small", 1, 1, "tree", build_bush_small),
    ("Cactus Tall", 1, 2, "tree", build_cactus_tall),
    ("Cactus Short", 1, 1, "tree", build_cactus_short),
    ("Fir Tree", 1, 2, "tree", build_fir),
    ("Snow Pine", 1, 2, "tree", build_snow_pine),
    ("Palm Tree", 1, 2, "tree", build_palm),
    ("Flower Bed", 1, 1, "tree", build_flower_bed),
    ("Table", 2, 1, "furniture", build_table),
    ("Chair", 1, 1, "furniture", build_chair),
    ("Bed", 2, 1, "furniture", build_bed),
    ("Chest", 1, 1, "furniture", build_chest),
    ("Barrel", 1, 1, "furniture", build_barrel),
    ("Fireplace", 1, 1, "furniture", build_fireplace),
    ("Torch", 1, 1, "furniture", build_torch),
    ("Cauldron", 1, 1, "furniture", build_cauldron),
    ("Bookshelf", 2, 1, "furniture", build_bookshelf),
    ("Warrior", 1, 1, "character", build_warrior),
    ("Mage", 1, 1, "character", build_mage),
    ("Skeleton", 1, 1, "character", build_skeleton),
    ("Slime", 1, 1, "character", build_slime),
    ("Golem", 1, 1, "character", build_golem),
    ("Chicken", 1, 1, "character", build_chicken),
    ("Cow", 1, 1, "character", build_cow),
    ("Pig", 1, 1, "character", build_pig),
    ("Sheep", 1, 1, "character", build_sheep),
    ("Crate", 1, 1, "misc", build_crate),
    ("Rock 1", 1, 1, "misc", build_rock1),
    ("Rock 2", 1, 1, "misc", build_rock2),
    ("Rock 3", 1, 1, "misc", build_rock3),
    ("Sign Post", 1, 1, "misc", build_sign),
    ("Tombstone", 1, 1, "misc", build_tombstone),
    ("Hay Bale", 1, 1, "misc", build_hay),
    ("Lamp Post", 1, 1, "misc", build_lamp),
    ("Log", 2, 1, "misc", build_log),
    ("Pumpkin", 1, 1, "misc", build_pumpkin),
    ("Flower Red", 1, 1, "misc", build_flower_r),
    ("Flower Yellow", 1, 1, "misc", build_flower_y),
]

# ═══════════════════════════════════════════════
# RENDER ALL
# ═══════════════════════════════════════════════
print(f"[Props] Rendering {len(ground_tiles)} ground tiles + {len(props)} props...")

# Render ground tiles
ground_paths = []
for i, (name, r, g, b, ns) in enumerate(ground_tiles):
    print(f"  Ground [{i+1}/{len(ground_tiles)}] {name}")
    p = render_ground_tile(name, r, g, b, ns, i)
    ground_paths.append(p)

# Render props
prop_paths = []
for i, (name, wt, ht, cat, builder) in enumerate(props):
    print(f"  Prop [{i+1}/{len(props)}] {name} ({wt}x{ht})")
    clear_scene()
    builder()
    px_w = wt * render_size
    px_h = ht * render_size
    setup_render(px_w, px_h)
    cam = setup_camera(max(wt, ht))
    setup_light()
    path = os.path.join(tmpdir, f"prop_{i:03d}_{name.replace(' ','_')}.png")
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    prop_paths.append((path, wt, ht, name, cat))

# ═══════════════════════════════════════════════
# ASSEMBLE TILESET
# ═══════════════════════════════════════════════
print("[Props] Assembling tileset...")
from PIL import Image

ts = tile_size
cols = 8
sheet_w = cols * ts

# Ground tiles: rows 0-3 (26 tiles in 8-col grid)
ground_rows = (len(ground_tiles) + cols - 1) // cols
ground_h = ground_rows * ts

# Calculate prop area: pack props sequentially
# Each prop at its rendered size, packed left-to-right wrapping at sheet_w
prop_placements = []
cur_x, cur_y = 0, ground_h
row_h = 0
for path, wt, ht, name, cat in prop_paths:
    pw, ph = wt * ts, ht * ts
    if cur_x + pw > sheet_w:
        cur_y += row_h
        cur_x = 0
        row_h = 0
    prop_placements.append((cur_x, cur_y, pw, ph, name, cat, path))
    row_h = max(row_h, ph)
    cur_x += pw
# Final row
total_h = cur_y + row_h

# Create sheet
sheet = Image.new("RGBA", (sheet_w, total_h), (0, 0, 0, 0))

# Place ground tiles
for i, gp in enumerate(ground_paths):
    img = Image.open(gp).resize((ts, ts), Image.LANCZOS)
    col, row = i % cols, i // cols
    sheet.paste(img, (col * ts, row * ts))

# Place props
for px, py, pw, ph, name, cat, path in prop_placements:
    img = Image.open(path).resize((pw, ph), Image.LANCZOS)
    sheet.paste(img, (px, py), img)

# Save
os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
sheet.save(output_path)
print(f"[Props] Saved tileset: {output_path} ({sheet_w}x{total_h})")

# Generate stamps file
stamps_path = output_path.replace(".png", "_stamps.txt")
with open(stamps_path, "w") as f:
    # Ground tile stamps (first 26)
    for i, (name, r, g, b, ns) in enumerate(ground_tiles):
        col, row = i % cols, i // cols
        cat = "misc"
        if "grass" in name or "flower" in name: cat = "tree"
        elif "stone" in name or "road" in name: cat = "building"
        f.write(f"{name}|{col*ts}|{row*ts}|{ts}|{ts}|{cat}\n")
    # Prop stamps
    for px, py, pw, ph, name, cat, path in prop_placements:
        f.write(f"{name}|{px}|{py}|{pw}|{ph}|{cat}\n")
print(f"[Props] Saved stamps: {stamps_path} ({len(ground_tiles) + len(props)} entries)")

# Cleanup
shutil.rmtree(tmpdir, ignore_errors=True)
print("[Props] Done!")
