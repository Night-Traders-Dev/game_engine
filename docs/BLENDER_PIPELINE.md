# Blender to Twilight Engine Pipeline

Import 3D models from Blender as 2D sprite sheets for use in the engine. The pipeline renders a .blend file from multiple angles and assembles the frames into a 3x3 grid matching the engine's walk cycle format.

## Quick Start

```bash
# Create a sample character model
blender -b -P tools/blender_create_sample.py -- --output games/demo/assets/blender/my_character.blend

# Convert to sprite sheet
python3 tools/blender_to_spritesheet.py \
  --blend games/demo/assets/blender/my_character.blend \
  --output assets/textures/my_character.png \
  --size 64 --mode topdown
```

Or use the editor: **F5 > Blender Import > Import to Sprite Sheet**

## Pipeline Overview

```text
.blend file (3D model + animation)
    |
    v
blender_render_sprites.py (Blender headless)
    |-- Renders 3 directions: down, up, right
    |-- Renders 3 frames per direction: idle, walk_0, walk_1
    |-- Orthographic camera, transparent background
    |-- 2x render + downscale for anti-aliasing
    v
9 individual frame PNGs
    |
    v
blender_to_spritesheet.py (Pillow)
    |-- Assembles 3x3 grid
    |-- Outputs sprite sheet + JSON manifest
    v
sprite_sheet.png (3 cols x 3 rows)
```

## Sprite Sheet Layout

The output matches `define_npc_atlas_regions(atlas, cw, ch)`:

```text
Column:     0 (idle)      1 (walk_0)    2 (walk_1)
Row 0:    idle_down     walk_down_0   walk_down_1
Row 1:    idle_up       walk_up_0     walk_up_1
Row 2:    idle_right    walk_right_0  walk_right_1
```

Left-facing sprites are rendered at runtime by flipping right horizontally.

## Tools

### blender_render_sprites.py

Blender Python script. Run via `blender -b model.blend -P tools/blender_render_sprites.py -- [args]`.

| Flag | Default | Description |
|------|---------|-------------|
| `--mode` | `topdown` | Camera angle: `topdown` (45-degree above) or `side` (horizontal, for platformer) |
| `--size` | `64` | Output frame size in pixels |
| `--output` | `/tmp/tw_sprites/` | Directory for individual frame PNGs |
| `--transparent` | enabled | Transparent background (RGBA) |
| `--action` | first action | Animation action name to render |

### blender_to_spritesheet.py

Standalone Python script (requires Pillow).

| Flag | Description |
|------|-------------|
| `--blend path.blend` | Full pipeline: render + assemble |
| `--frames dir/` | Assemble-only from existing frame PNGs |
| `--output path.png` | Output sprite sheet path |
| `--size 64` | Frame size in pixels |
| `--mode topdown\|side` | Camera mode (passed to render script) |

### blender_create_sample.py

Creates a sample humanoid character from Blender primitives with a walk animation.

```bash
blender -b -P tools/blender_create_sample.py -- --output path.blend
```

## Editor Integration

**F5 > Blender Import** panel provides:
- .blend file path input
- Output PNG path input
- Frame size slider (16-256 px)
- Camera mode dropdown (Top-Down RPG / Side Platformer)
- **Import to Sprite Sheet** button (runs full pipeline, auto-loads into atlas cache)
- **Create Sample .blend** button (generates test model)
- Status display (success/failure)

After import, the sprite sheet is automatically registered in the atlas cache and can be used immediately for NPCs or player sprites.

## Using Imported Sprites

### In SageLang

```sage
# Spawn an NPC using the imported sprite
spawn_npc("Guard", 200, 300, 0, false, "assets/textures/my_character.png", 0, 0, 50, 0, 64, 64)
```

### In the Editor

1. Import via F5 > Blender Import
2. Go to Player & Spawn > Player Sprite
3. Enter the output path and grid dimensions
4. Click "Load Player Sprite"

---

## Blender Modeling Guide for Game Sprites

### Character Modeling Best Practices

**Start with primitives.** Build characters from simple shapes: cubes for torso, spheres for heads, cylinders for limbs. Use the Mirror modifier for symmetry.

**Keep it low-poly.** For sprites rendered at 32-128px, high detail is wasted. 500-2000 triangles is plenty. Focus on clear silhouettes rather than fine detail.

**Use quads.** Quad topology deforms predictably under animation. Avoid ngons and minimize triangles, especially around joints.

### Topology for Animation

**Edge loops around joints.** Place extra edge loops at elbows, knees, shoulders, and hips. These allow clean bending without collapsing geometry.

```text
Bad (collapses):     Good (clean bend):
    |                    |  |
    |                    |  |
    |                    |--|  <- extra loops
    |                    |  |
    |                    |  |
```

**Follow muscle flow.** Edges should follow the natural curves of the body — around eye sockets, along the jawline, following the deltoid around the shoulder.

**Consistent edge density.** Don't have 20 loops on the face and 2 on the legs. Keep density proportional to importance and deformation needs.

### Beveling

**Bevel modifier** adds chamfered edges that catch light better than sharp corners. For game sprites:
- 1-2 segments, small width (0.02-0.05)
- Apply before rigging
- Use **Ctrl+B** for manual edge bevels on specific edges

**Subdivision Surface** smooths the entire mesh. Workflow:
1. Model low-poly base
2. Apply Subdivision Surface (level 1-2) to preview smooth result
3. Either apply the modifier for final mesh, or keep it for rendering only

### UV Unwrapping

**Smart UV Project** (U > Smart UV Project) works for most sprite work since the render is low-res. For higher quality:

1. Mark seams (Ctrl+E > Mark Seam) along hidden edges (inner legs, back of head, underarms)
2. Unwrap (U > Unwrap)
3. Check with a checkerboard texture — squares should be roughly equal size
4. Minimize stretching in the UV editor

**Texture painting** in Blender (Texture Paint mode) lets you paint directly on the model. Use a 256x256 or 512x512 texture for sprite work.

### Rigging

**Armature setup:**
1. Add Armature (Shift+A > Armature)
2. Enter Edit Mode, extrude bones (E key) to create skeleton
3. Minimum bones for a walk cycle: root, spine, head, upper_arm.L/R, forearm.L/R, thigh.L/R, shin.L/R

**Parenting:**
1. Select mesh, then Shift-select armature
2. Ctrl+P > Armature Deform > With Automatic Weights
3. Test by posing in Pose Mode

**Weight painting fixes:**
- Switch to Weight Paint mode
- Select each bone and check influence
- Red = full influence, blue = none
- Fix bleeding between bones (e.g., head moving with shoulder) by painting blue on wrong areas
- Enable Auto-Normalize to keep total weights = 1.0

**IK Constraints** (for advanced rigs):
- Add IK constraint to foot/hand bones
- Set chain length (2 for arm: upper+forearm, 2 for leg: thigh+shin)
- Add pole targets for knee/elbow direction control

### Walk Cycle Animation

For the engine's 3-frame walk cycle (idle, walk_0, walk_1):

```text
Frame 1 (idle):     Frame 5 (walk_0):     Frame 9 (walk_1):
  Standing          Left foot forward     Right foot forward
  Arms at sides     Right arm forward     Left arm forward
                    Slight body dip       Slight body dip
```

1. Create an Action in the Action Editor
2. Set keyframes at frames 1, 5, 9 (or 1, 4, 8 for 12-frame cycle)
3. Key the leg bones forward/back, arms opposite
4. Add slight up/down on the root bone (body bob)
5. Set action to cyclic (Graph Editor > Channel > Extrapolation Mode > Cyclic)

### Materials for Sprites

Keep materials simple — sprites are small and detail is lost:
- **Principled BSDF** with flat base colors
- Low roughness (0.3-0.5) for slight shininess
- No complex textures needed unless rendering at 128px+
- Use distinct colors per body part for readability at small sizes

### Lighting for Sprite Rendering

The pipeline tool auto-creates a sun light, but for custom scenes:
- **Single sun light** at 45 degrees from above-front
- Energy: 3-5 for EEVEE, 1-2 for Cycles
- Transparent background (Film > Transparent in Render Properties)
- Orthographic camera (the tool handles this automatically)
