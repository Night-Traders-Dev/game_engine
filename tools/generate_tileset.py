#!/usr/bin/env python3
"""Procedural tileset generator for Twilight Engine.

Generates pixel-art tilesets with terrain tiles, autotile transitions,
decorations, water animations, and object stamps for 10 biome types.

Output is engine-compatible: {biome}_tileset.png + {biome}_stamps.txt

Usage:
    python generate_tileset.py --biome grasslands --output /tmp/tiles/
    python generate_tileset.py --biome all --seed 42
    python generate_tileset.py --list-biomes
"""

import argparse
import json
import math
import os
import random
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from PIL import Image

# ═══════════════════════════════════════════════════════════════════════
# Section 1: Pure-Numpy Simplex Noise
# ═══════════════════════════════════════════════════════════════════════

class SimplexNoise2D:
    """2D simplex noise using only numpy. No external packages needed."""

    # Gradients for 2D simplex noise
    _GRAD3 = np.array([
        [1,1],[-1,1],[1,-1],[-1,-1],
        [1,0],[-1,0],[0,1],[0,-1],
        [1,1],[-1,1],[1,-1],[-1,-1],
    ], dtype=np.float64)

    _F2 = 0.5 * (math.sqrt(3.0) - 1.0)  # Skew factor
    _G2 = (3.0 - math.sqrt(3.0)) / 6.0  # Unskew factor

    def __init__(self, seed=0):
        rng = np.random.RandomState(seed)
        self._perm = np.arange(256, dtype=np.int32)
        rng.shuffle(self._perm)
        self._perm = np.tile(self._perm, 2)  # Double for wrapping

    def _noise2(self, x, y):
        """Single-point 2D simplex noise, returns [-1, 1]."""
        F2, G2 = self._F2, self._G2
        s = (x + y) * F2
        i, j = int(math.floor(x + s)), int(math.floor(y + s))
        t = (i + j) * G2
        x0, y0 = x - (i - t), y - (j - t)

        if x0 > y0:
            i1, j1 = 1, 0
        else:
            i1, j1 = 0, 1

        x1, y1 = x0 - i1 + G2, y0 - j1 + G2
        x2, y2 = x0 - 1.0 + 2.0 * G2, y0 - 1.0 + 2.0 * G2

        ii, jj = i & 255, j & 255
        perm = self._perm
        grad = self._GRAD3

        def contrib(tx, ty, gi):
            t_val = 0.5 - tx*tx - ty*ty
            if t_val < 0:
                return 0.0
            t_val *= t_val
            g = grad[gi % 12]
            return t_val * t_val * (g[0]*tx + g[1]*ty)

        gi0 = perm[ii + perm[jj]]
        gi1 = perm[ii + i1 + perm[jj + j1]]
        gi2 = perm[ii + 1 + perm[jj + 1]]

        n = contrib(x0, y0, gi0) + contrib(x1, y1, gi1) + contrib(x2, y2, gi2)
        return 70.0 * n

    def generate_grid(self, width, height, scale=1.0, octaves=4,
                      persistence=0.5, lacunarity=2.0, offset_x=0.0, offset_y=0.0):
        """Generate a 2D noise grid with fBm, returns [0, 1] array of shape (height, width)."""
        result = np.zeros((height, width), dtype=np.float64)
        amp_sum = 0.0
        amp = 1.0
        freq = 1.0
        for _ in range(octaves):
            for y in range(height):
                for x in range(width):
                    nx = (x + offset_x) / scale * freq
                    ny = (y + offset_y) / scale * freq
                    result[y, x] += self._noise2(nx, ny) * amp
            amp_sum += amp
            amp *= persistence
            freq *= lacunarity
        result /= amp_sum
        return (result + 1.0) * 0.5  # Normalize to [0, 1]


# ═══════════════════════════════════════════════════════════════════════
# Section 2: Pixel Art Utilities
# ═══════════════════════════════════════════════════════════════════════

# 4x4 Bayer dithering matrix (normalized to [0, 1])
_BAYER4 = np.array([
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5],
], dtype=np.float64) / 16.0


def noise_to_palette(noise_grid, palette, thresholds=None):
    """Map a [0,1] noise grid to RGBA palette colors.

    palette: list of (R, G, B, A) tuples
    thresholds: list of floats [0-1] defining breakpoints (len = len(palette)-1)
    Returns (H, W, 4) uint8 array.
    """
    h, w = noise_grid.shape
    if thresholds is None:
        n = len(palette)
        thresholds = [i / n for i in range(1, n)]

    result = np.zeros((h, w, 4), dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            v = noise_grid[y, x]
            # Apply Bayer dithering
            v += (_BAYER4[y % 4, x % 4] - 0.5) * 0.08
            v = max(0.0, min(1.0, v))
            idx = 0
            for t in thresholds:
                if v > t:
                    idx += 1
                else:
                    break
            idx = min(idx, len(palette) - 1)
            result[y, x] = palette[idx]
    return result


def add_outline(tile, color=(20, 15, 10, 255)):
    """Add 1px dark outline around non-transparent pixels."""
    h, w = tile.shape[:2]
    result = tile.copy()
    alpha = tile[:, :, 3]
    for y in range(h):
        for x in range(w):
            if alpha[y, x] < 128:
                # Check if any neighbor is opaque
                for dy, dx in [(-1,0),(1,0),(0,-1),(0,1)]:
                    ny, nx = y+dy, x+dx
                    if 0 <= ny < h and 0 <= nx < w and alpha[ny, nx] >= 128:
                        result[y, x] = color
                        break
    return result


def add_highlight(tile, strength=0.25):
    """Brighten top-left pixels for directional light."""
    h, w = tile.shape[:2]
    result = tile.astype(np.float64)
    for y in range(h):
        for x in range(w):
            if tile[y, x, 3] < 128:
                continue
            # Distance from top-left corner
            d = (x / w + y / h) / 2.0
            factor = 1.0 + strength * (1.0 - d)
            result[y, x, :3] = np.clip(result[y, x, :3] * factor, 0, 255)
    return result.astype(np.uint8)


# ═══════════════════════════════════════════════════════════════════════
# Section 3: Biome Presets
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class StampDef:
    name: str
    category: str
    width: int     # pixels
    height: int    # pixels
    generator: str
    params: dict = field(default_factory=dict)

@dataclass
class TerrainType:
    name: str
    palette: list  # list of (R,G,B,A) tuples
    thresholds: list = None
    noise_scale: float = 8.0

@dataclass
class BiomePreset:
    name: str
    terrains: list           # list of TerrainType
    transition_pairs: list   # list of (terrain_idx_a, terrain_idx_b)
    water_palette: list      # 4 RGBA colors for water animation
    decoration_colors: list  # colors for detail sprites
    stamps: list             # list of StampDef
    noise_scale: float = 10.0
    noise_octaves: int = 4


def _rgba(r, g, b, a=255):
    return (r, g, b, a)


BIOMES = {}

# ── Grasslands ──
BIOMES["grasslands"] = BiomePreset(
    name="grasslands",
    terrains=[
        TerrainType("grass", [
            _rgba(74, 140, 46), _rgba(93, 160, 58), _rgba(82, 150, 52),
            _rgba(65, 128, 40), _rgba(55, 115, 35),
        ], noise_scale=10.0),
        TerrainType("dirt", [
            _rgba(139, 107, 58), _rgba(160, 125, 70), _rgba(120, 90, 48),
            _rgba(100, 75, 40),
        ], noise_scale=8.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(60, 120, 180), _rgba(70, 135, 195), _rgba(55, 110, 170), _rgba(65, 125, 185)],
    decoration_colors=[_rgba(212, 232, 88), _rgba(232, 88, 128), _rgba(180, 200, 70), _rgba(255, 220, 80)],
    stamps=[
        StampDef("Oak Tree", "tree", 96, 128, "fractal_tree", {"trunk": _rgba(100, 70, 35), "leaves": [_rgba(60, 130, 40), _rgba(75, 145, 50), _rgba(50, 115, 35)]}),
        StampDef("Bush", "tree", 40, 32, "noise_blob", {"colors": [_rgba(55, 125, 38), _rgba(70, 140, 48), _rgba(45, 110, 30)]}),
        StampDef("Round Bush", "tree", 32, 28, "noise_blob", {"colors": [_rgba(65, 135, 42), _rgba(80, 150, 55)]}),
        StampDef("Rock 1", "misc", 48, 40, "noise_blob", {"colors": [_rgba(130, 130, 130), _rgba(150, 150, 145), _rgba(110, 108, 105)]}),
        StampDef("Rock 2", "misc", 32, 28, "noise_blob", {"colors": [_rgba(120, 118, 115), _rgba(140, 138, 135)]}),
        StampDef("Farmhouse", "building", 96, 112, "geometric_building", {"wall": _rgba(180, 160, 120), "roof": _rgba(160, 60, 50), "door": _rgba(100, 65, 30)}),
        StampDef("Fence Post", "misc", 32, 48, "prop_simple", {"shape": "post", "color": _rgba(130, 95, 50)}),
        StampDef("Sign Post", "misc", 32, 56, "prop_simple", {"shape": "sign", "color": _rgba(140, 100, 55)}),
        StampDef("Flower Bed", "misc", 64, 28, "prop_simple", {"shape": "flowerbed", "color": _rgba(220, 80, 120)}),
        StampDef("Hay Bale", "misc", 40, 36, "noise_blob", {"colors": [_rgba(200, 175, 80), _rgba(220, 195, 100), _rgba(180, 155, 65)]}),
    ],
)

# ── Forest ──
BIOMES["forest"] = BiomePreset(
    name="forest",
    terrains=[
        TerrainType("dark_grass", [
            _rgba(45, 92, 26), _rgba(55, 105, 35), _rgba(38, 80, 22),
            _rgba(50, 98, 30), _rgba(35, 72, 20),
        ], noise_scale=8.0),
        TerrainType("moss", [
            _rgba(59, 94, 40), _rgba(72, 110, 50), _rgba(48, 82, 32),
            _rgba(65, 100, 45),
        ], noise_scale=6.0),
        TerrainType("leaf_litter", [
            _rgba(107, 74, 42), _rgba(125, 90, 55), _rgba(90, 62, 35),
            _rgba(140, 105, 60),
        ], noise_scale=7.0),
    ],
    transition_pairs=[(0, 1), (0, 2)],
    water_palette=[_rgba(40, 85, 60), _rgba(48, 95, 68), _rgba(35, 78, 55), _rgba(42, 88, 62)],
    decoration_colors=[_rgba(180, 140, 50), _rgba(140, 60, 30), _rgba(80, 130, 50), _rgba(160, 120, 40)],
    stamps=[
        StampDef("Big Oak", "tree", 112, 128, "fractal_tree", {"trunk": _rgba(85, 55, 25), "leaves": [_rgba(40, 85, 28), _rgba(50, 100, 35), _rgba(35, 75, 22)]}),
        StampDef("Pine Tree", "tree", 64, 128, "fractal_tree", {"trunk": _rgba(75, 50, 25), "leaves": [_rgba(30, 72, 25), _rgba(38, 82, 30)], "shape": "conical"}),
        StampDef("Dead Tree", "tree", 40, 96, "fractal_tree", {"trunk": _rgba(90, 65, 35), "leaves": [], "shape": "dead"}),
        StampDef("Mushroom", "misc", 28, 24, "prop_simple", {"shape": "mushroom", "color": _rgba(200, 50, 40)}),
        StampDef("Boulder", "misc", 64, 56, "noise_blob", {"colors": [_rgba(100, 98, 95), _rgba(120, 118, 115), _rgba(85, 82, 80)]}),
        StampDef("Fallen Log", "misc", 64, 28, "prop_simple", {"shape": "log", "color": _rgba(95, 65, 30)}),
        StampDef("Log Cabin", "building", 96, 112, "geometric_building", {"wall": _rgba(110, 75, 35), "roof": _rgba(80, 55, 25), "door": _rgba(70, 45, 20)}),
        StampDef("Stump", "misc", 32, 28, "prop_simple", {"shape": "stump", "color": _rgba(100, 70, 35)}),
        StampDef("Fern", "tree", 32, 28, "noise_blob", {"colors": [_rgba(50, 100, 35), _rgba(60, 115, 42)]}),
    ],
)

# ── Desert ──
BIOMES["desert"] = BiomePreset(
    name="desert",
    terrains=[
        TerrainType("sand", [
            _rgba(212, 184, 122), _rgba(228, 200, 140), _rgba(195, 168, 108),
            _rgba(220, 192, 130), _rgba(205, 178, 115),
        ], noise_scale=12.0),
        TerrainType("sandstone", [
            _rgba(180, 140, 80), _rgba(195, 155, 95), _rgba(165, 125, 68),
            _rgba(150, 110, 55),
        ], noise_scale=8.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(90, 160, 140), _rgba(100, 172, 152), _rgba(82, 150, 130), _rgba(95, 165, 145)],
    decoration_colors=[_rgba(180, 155, 90), _rgba(160, 135, 75), _rgba(200, 175, 105)],
    stamps=[
        StampDef("Cactus Tall", "tree", 32, 96, "fractal_tree", {"trunk": _rgba(60, 120, 50), "leaves": [_rgba(70, 135, 58)], "shape": "cactus"}),
        StampDef("Cactus Short", "tree", 36, 64, "fractal_tree", {"trunk": _rgba(65, 125, 55), "leaves": [_rgba(75, 140, 62)], "shape": "cactus"}),
        StampDef("Sand Rock", "misc", 64, 48, "noise_blob", {"colors": [_rgba(175, 145, 95), _rgba(190, 160, 110), _rgba(160, 130, 82)]}),
        StampDef("Desert Ruins", "building", 96, 80, "geometric_building", {"wall": _rgba(185, 155, 100), "roof": _rgba(170, 140, 88), "door": _rgba(130, 100, 55), "ruined": True}),
        StampDef("Palm Tree", "tree", 64, 96, "fractal_tree", {"trunk": _rgba(140, 100, 50), "leaves": [_rgba(55, 130, 45), _rgba(65, 145, 55)], "shape": "palm"}),
        StampDef("Skull", "misc", 24, 20, "prop_simple", {"shape": "skull", "color": _rgba(220, 210, 195)}),
        StampDef("Tent", "building", 80, 64, "geometric_building", {"wall": _rgba(200, 180, 140), "roof": _rgba(180, 150, 100), "door": _rgba(120, 90, 50), "tent": True}),
        StampDef("Barrel", "misc", 28, 32, "prop_simple", {"shape": "barrel", "color": _rgba(140, 100, 55)}),
    ],
)

# ── Snow ──
BIOMES["snow"] = BiomePreset(
    name="snow",
    terrains=[
        TerrainType("snow", [
            _rgba(232, 232, 240), _rgba(220, 225, 235), _rgba(240, 240, 248),
            _rgba(210, 215, 228), _rgba(225, 228, 238),
        ], noise_scale=12.0),
        TerrainType("ice", [
            _rgba(160, 185, 210), _rgba(175, 198, 220), _rgba(145, 170, 198),
            _rgba(130, 155, 185),
        ], noise_scale=8.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(100, 140, 175), _rgba(110, 152, 188), _rgba(92, 130, 165), _rgba(105, 145, 180)],
    decoration_colors=[_rgba(190, 200, 215), _rgba(170, 180, 200), _rgba(200, 208, 220)],
    stamps=[
        StampDef("Snow Pine", "tree", 64, 116, "fractal_tree", {"trunk": _rgba(85, 60, 30), "leaves": [_rgba(35, 78, 40), _rgba(45, 90, 48)], "shape": "conical", "snow": True}),
        StampDef("Bare Tree", "tree", 48, 96, "fractal_tree", {"trunk": _rgba(95, 70, 40), "leaves": [], "shape": "dead"}),
        StampDef("Ice Rock", "misc", 72, 64, "noise_blob", {"colors": [_rgba(150, 175, 200), _rgba(170, 192, 215), _rgba(130, 158, 185)]}),
        StampDef("Snowman", "misc", 32, 48, "prop_simple", {"shape": "snowman", "color": _rgba(235, 235, 242)}),
        StampDef("Igloo", "building", 80, 64, "geometric_building", {"wall": _rgba(225, 230, 240), "roof": _rgba(210, 218, 230), "door": _rgba(80, 65, 45)}),
        StampDef("Frozen Bush", "tree", 40, 32, "noise_blob", {"colors": [_rgba(120, 155, 140), _rgba(140, 170, 158), _rgba(100, 135, 122)]}),
        StampDef("Log Pile", "misc", 48, 28, "prop_simple", {"shape": "log", "color": _rgba(105, 75, 40)}),
    ],
)

# ── Swamp ──
BIOMES["swamp"] = BiomePreset(
    name="swamp",
    terrains=[
        TerrainType("bog", [
            _rgba(60, 80, 42), _rgba(72, 95, 52), _rgba(50, 70, 35),
            _rgba(55, 75, 38), _rgba(65, 85, 45),
        ], noise_scale=8.0),
        TerrainType("mud", [
            _rgba(95, 75, 45), _rgba(110, 88, 55), _rgba(80, 62, 38),
            _rgba(100, 80, 48),
        ], noise_scale=6.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(50, 72, 48), _rgba(58, 82, 55), _rgba(42, 65, 42), _rgba(55, 78, 52)],
    decoration_colors=[_rgba(80, 110, 55), _rgba(65, 90, 42), _rgba(95, 125, 65)],
    stamps=[
        StampDef("Swamp Tree", "tree", 80, 112, "fractal_tree", {"trunk": _rgba(65, 50, 25), "leaves": [_rgba(48, 78, 35), _rgba(55, 88, 40)], "shape": "spreading"}),
        StampDef("Willow", "tree", 96, 120, "fractal_tree", {"trunk": _rgba(70, 55, 28), "leaves": [_rgba(55, 95, 42), _rgba(65, 108, 50)], "shape": "weeping"}),
        StampDef("Mud Pile", "misc", 48, 32, "noise_blob", {"colors": [_rgba(90, 72, 42), _rgba(105, 85, 52), _rgba(78, 60, 35)]}),
        StampDef("Lily Pad", "misc", 28, 20, "prop_simple", {"shape": "lilypad", "color": _rgba(60, 110, 45)}),
        StampDef("Wooden Hut", "building", 80, 96, "geometric_building", {"wall": _rgba(95, 70, 35), "roof": _rgba(65, 85, 40), "door": _rgba(70, 50, 25)}),
        StampDef("Mushroom Cluster", "misc", 36, 32, "prop_simple", {"shape": "mushroom", "color": _rgba(180, 55, 40)}),
        StampDef("Root Tangle", "misc", 56, 40, "noise_blob", {"colors": [_rgba(75, 55, 28), _rgba(90, 68, 35), _rgba(60, 42, 22)]}),
    ],
)

# ── Volcanic ──
BIOMES["volcanic"] = BiomePreset(
    name="volcanic",
    terrains=[
        TerrainType("obsidian", [
            _rgba(42, 26, 26), _rgba(55, 35, 35), _rgba(35, 20, 20),
            _rgba(48, 30, 30), _rgba(30, 18, 18),
        ], noise_scale=8.0),
        TerrainType("lava", [
            _rgba(200, 48, 32), _rgba(232, 104, 48), _rgba(180, 40, 25),
            _rgba(255, 140, 50), _rgba(160, 32, 20),
        ], noise_scale=6.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(200, 60, 30), _rgba(230, 90, 40), _rgba(180, 45, 25), _rgba(215, 75, 35)],
    decoration_colors=[_rgba(120, 50, 30), _rgba(80, 35, 20), _rgba(150, 65, 35)],
    stamps=[
        StampDef("Charred Tree", "tree", 40, 80, "fractal_tree", {"trunk": _rgba(45, 30, 18), "leaves": [], "shape": "dead"}),
        StampDef("Lava Rock", "misc", 56, 48, "noise_blob", {"colors": [_rgba(55, 35, 30), _rgba(70, 45, 38), _rgba(42, 28, 22)]}),
        StampDef("Obsidian Shard", "misc", 28, 48, "prop_simple", {"shape": "crystal", "color": _rgba(35, 25, 40)}),
        StampDef("Fire Pit", "misc", 40, 36, "noise_blob", {"colors": [_rgba(200, 80, 30), _rgba(230, 120, 40), _rgba(160, 50, 20)]}),
        StampDef("Ruined Altar", "building", 80, 64, "geometric_building", {"wall": _rgba(60, 40, 35), "roof": _rgba(50, 32, 28), "door": _rgba(180, 60, 30), "ruined": True}),
        StampDef("Skull Pile", "misc", 36, 28, "prop_simple", {"shape": "skull", "color": _rgba(200, 190, 175)}),
    ],
)

# ── Beach ──
BIOMES["beach"] = BiomePreset(
    name="beach",
    terrains=[
        TerrainType("sand", [
            _rgba(228, 210, 160), _rgba(240, 222, 175), _rgba(215, 198, 148),
            _rgba(235, 218, 168), _rgba(220, 205, 155),
        ], noise_scale=12.0),
        TerrainType("wet_sand", [
            _rgba(180, 168, 130), _rgba(195, 182, 142), _rgba(165, 152, 118),
            _rgba(172, 160, 125),
        ], noise_scale=10.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(50, 140, 190), _rgba(60, 155, 205), _rgba(42, 128, 178), _rgba(55, 148, 198)],
    decoration_colors=[_rgba(210, 195, 155), _rgba(190, 175, 138), _rgba(230, 215, 175)],
    stamps=[
        StampDef("Palm Tree", "tree", 64, 112, "fractal_tree", {"trunk": _rgba(145, 105, 55), "leaves": [_rgba(50, 135, 45), _rgba(60, 148, 55)], "shape": "palm"}),
        StampDef("Coconut Palm", "tree", 56, 96, "fractal_tree", {"trunk": _rgba(155, 115, 60), "leaves": [_rgba(55, 140, 48)], "shape": "palm"}),
        StampDef("Beach Rock", "misc", 48, 36, "noise_blob", {"colors": [_rgba(155, 148, 138), _rgba(170, 162, 152), _rgba(140, 132, 125)]}),
        StampDef("Shell", "misc", 20, 16, "prop_simple", {"shape": "shell", "color": _rgba(225, 200, 170)}),
        StampDef("Beach Hut", "building", 80, 80, "geometric_building", {"wall": _rgba(190, 170, 130), "roof": _rgba(130, 95, 50), "door": _rgba(100, 70, 35)}),
        StampDef("Driftwood", "misc", 56, 24, "prop_simple", {"shape": "log", "color": _rgba(160, 135, 95)}),
        StampDef("Crab", "misc", 20, 16, "prop_simple", {"shape": "crab", "color": _rgba(200, 80, 50)}),
        StampDef("Umbrella", "misc", 40, 48, "prop_simple", {"shape": "umbrella", "color": _rgba(220, 60, 60)}),
    ],
)

# ── Cave ──
BIOMES["cave"] = BiomePreset(
    name="cave",
    terrains=[
        TerrainType("stone", [
            _rgba(85, 82, 78), _rgba(100, 96, 92), _rgba(72, 68, 65),
            _rgba(92, 88, 84), _rgba(65, 62, 58),
        ], noise_scale=7.0),
        TerrainType("dark_floor", [
            _rgba(48, 45, 42), _rgba(58, 55, 50), _rgba(38, 36, 32),
            _rgba(52, 48, 44),
        ], noise_scale=6.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(40, 60, 90), _rgba(48, 70, 100), _rgba(35, 52, 82), _rgba(44, 65, 95)],
    decoration_colors=[_rgba(80, 120, 160), _rgba(100, 140, 180), _rgba(60, 90, 130)],
    stamps=[
        StampDef("Stalagmite", "misc", 28, 56, "prop_simple", {"shape": "crystal", "color": _rgba(95, 90, 85)}),
        StampDef("Crystal", "misc", 32, 48, "prop_simple", {"shape": "crystal", "color": _rgba(80, 130, 200)}),
        StampDef("Boulder", "misc", 56, 48, "noise_blob", {"colors": [_rgba(75, 72, 68), _rgba(90, 86, 82), _rgba(62, 58, 55)]}),
        StampDef("Chest", "furniture", 40, 32, "prop_simple", {"shape": "chest", "color": _rgba(140, 100, 45)}),
        StampDef("Torch", "furniture", 20, 44, "prop_simple", {"shape": "torch", "color": _rgba(200, 150, 50)}),
        StampDef("Cobweb", "misc", 32, 32, "prop_simple", {"shape": "cobweb", "color": _rgba(200, 200, 200)}),
        StampDef("Stone Pillar", "building", 32, 64, "geometric_building", {"wall": _rgba(90, 85, 80), "roof": _rgba(100, 95, 90), "door": _rgba(70, 65, 60)}),
        StampDef("Skeleton", "character", 24, 32, "prop_simple", {"shape": "skull", "color": _rgba(215, 205, 190)}),
    ],
)

# ── Urban ──
BIOMES["urban"] = BiomePreset(
    name="urban",
    terrains=[
        TerrainType("cobblestone", [
            _rgba(140, 135, 128), _rgba(155, 150, 142), _rgba(125, 120, 112),
            _rgba(148, 142, 135), _rgba(132, 128, 120),
        ], noise_scale=5.0),
        TerrainType("brick", [
            _rgba(165, 85, 55), _rgba(180, 95, 62), _rgba(150, 75, 48),
            _rgba(172, 90, 58),
        ], noise_scale=4.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(55, 100, 135), _rgba(65, 112, 148), _rgba(48, 90, 125), _rgba(60, 105, 140)],
    decoration_colors=[_rgba(120, 115, 108), _rgba(100, 95, 88), _rgba(140, 135, 128)],
    stamps=[
        StampDef("Town House", "building", 96, 112, "geometric_building", {"wall": _rgba(195, 180, 155), "roof": _rgba(140, 60, 45), "door": _rgba(100, 65, 30)}),
        StampDef("Shop", "building", 80, 96, "geometric_building", {"wall": _rgba(175, 165, 140), "roof": _rgba(55, 90, 130), "door": _rgba(90, 60, 28)}),
        StampDef("Lamp Post", "misc", 24, 72, "prop_simple", {"shape": "post", "color": _rgba(60, 58, 55)}),
        StampDef("Barrel", "misc", 28, 32, "prop_simple", {"shape": "barrel", "color": _rgba(130, 90, 45)}),
        StampDef("Crate", "misc", 32, 28, "prop_simple", {"shape": "crate", "color": _rgba(155, 120, 70)}),
        StampDef("Bench", "furniture", 48, 28, "prop_simple", {"shape": "bench", "color": _rgba(120, 85, 42)}),
        StampDef("Well", "building", 48, 56, "geometric_building", {"wall": _rgba(135, 130, 125), "roof": _rgba(120, 85, 42), "door": _rgba(50, 65, 90)}),
        StampDef("Cart", "vehicle", 64, 48, "prop_simple", {"shape": "cart", "color": _rgba(140, 100, 50)}),
        StampDef("Market Stall", "building", 80, 64, "geometric_building", {"wall": _rgba(175, 135, 80), "roof": _rgba(200, 50, 40), "door": _rgba(130, 95, 50), "tent": True}),
    ],
)

# ── Farmland ──
BIOMES["farmland"] = BiomePreset(
    name="farmland",
    terrains=[
        TerrainType("soil", [
            _rgba(115, 85, 48), _rgba(130, 98, 58), _rgba(100, 72, 40),
            _rgba(120, 90, 52), _rgba(108, 78, 44),
        ], noise_scale=6.0),
        TerrainType("crop_green", [
            _rgba(82, 145, 52), _rgba(95, 160, 62), _rgba(70, 130, 42),
            _rgba(88, 152, 55),
        ], noise_scale=5.0),
    ],
    transition_pairs=[(0, 1)],
    water_palette=[_rgba(65, 125, 160), _rgba(75, 138, 175), _rgba(58, 115, 148), _rgba(70, 130, 165)],
    decoration_colors=[_rgba(200, 180, 80), _rgba(220, 200, 95), _rgba(180, 160, 68)],
    stamps=[
        StampDef("Windmill", "building", 96, 128, "geometric_building", {"wall": _rgba(200, 190, 170), "roof": _rgba(160, 70, 50), "door": _rgba(100, 70, 35)}),
        StampDef("Barn", "building", 96, 96, "geometric_building", {"wall": _rgba(170, 60, 40), "roof": _rgba(140, 50, 35), "door": _rgba(95, 65, 30)}),
        StampDef("Scarecrow", "misc", 32, 56, "prop_simple", {"shape": "post", "color": _rgba(140, 105, 55)}),
        StampDef("Hay Stack", "misc", 48, 40, "noise_blob", {"colors": [_rgba(210, 185, 90), _rgba(228, 202, 108), _rgba(192, 168, 75)]}),
        StampDef("Apple Tree", "tree", 80, 96, "fractal_tree", {"trunk": _rgba(100, 70, 35), "leaves": [_rgba(60, 125, 40), _rgba(72, 140, 50)], "fruit": _rgba(210, 45, 35)}),
        StampDef("Fence Segment", "misc", 64, 32, "fence_line", {"color": _rgba(145, 105, 55), "posts": 3}),
        StampDef("Water Trough", "misc", 48, 28, "prop_simple", {"shape": "trough", "color": _rgba(120, 110, 100)}),
        StampDef("Chicken", "character", 20, 20, "prop_simple", {"shape": "chicken", "color": _rgba(230, 220, 200)}),
        StampDef("Pumpkin", "misc", 24, 20, "prop_simple", {"shape": "pumpkin", "color": _rgba(220, 130, 40)}),
    ],
)


# ═══════════════════════════════════════════════════════════════════════
# Section 4: Tile Generators
# ═══════════════════════════════════════════════════════════════════════

TILE_SIZE = 32


def generate_base_tiles(terrain, noise, count=4, seed_offset=0):
    """Generate base terrain tile variants. Returns list of (32,32,4) uint8 arrays."""
    tiles = []
    for i in range(count):
        grid = noise.generate_grid(
            TILE_SIZE, TILE_SIZE, scale=terrain.noise_scale,
            octaves=4, offset_x=(seed_offset + i) * 100, offset_y=(seed_offset + i) * 77,
        )
        tile = noise_to_palette(grid, terrain.palette, terrain.thresholds)
        tiles.append(tile)
    return tiles


def generate_transition_tiles(terrain_a, terrain_b, noise, seed_offset=0):
    """Generate 16 autotile transition variants using 4-bit corner bitmask.

    Bit layout: 0b(TL)(TR)(BL)(BR)
    Bit=1 means that corner uses terrain_a, bit=0 means terrain_b.
    """
    # Generate base textures for both terrains
    grid_a = noise.generate_grid(TILE_SIZE, TILE_SIZE, scale=terrain_a.noise_scale,
                                  octaves=3, offset_x=seed_offset*200, offset_y=seed_offset*150)
    grid_b = noise.generate_grid(TILE_SIZE, TILE_SIZE, scale=terrain_b.noise_scale,
                                  octaves=3, offset_x=seed_offset*200+50, offset_y=seed_offset*150+50)

    tex_a = noise_to_palette(grid_a, terrain_a.palette, terrain_a.thresholds)
    tex_b = noise_to_palette(grid_b, terrain_b.palette, terrain_b.thresholds)

    half = TILE_SIZE // 2
    tiles = []

    # Generate noise for feathering edges
    feather_grid = noise.generate_grid(TILE_SIZE, TILE_SIZE, scale=4.0, octaves=2,
                                        offset_x=seed_offset*300, offset_y=seed_offset*250)

    for mask in range(16):
        tl = (mask >> 3) & 1  # bit 3
        tr = (mask >> 2) & 1  # bit 2
        bl = (mask >> 1) & 1  # bit 1
        br = (mask >> 0) & 1  # bit 0

        tile = np.zeros((TILE_SIZE, TILE_SIZE, 4), dtype=np.uint8)
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                # Determine which quadrant
                if y < half:
                    corner = tl if x < half else tr
                else:
                    corner = bl if x < half else br

                # Feather the edge (2-3 pixels at boundaries)
                edge_dist = min(abs(x - half), abs(y - half))
                if edge_dist < 3:
                    feather = feather_grid[y, x]
                    if feather > 0.5 + edge_dist * 0.1:
                        corner = 1 - corner

                if corner:
                    tile[y, x] = tex_a[y, x]
                else:
                    tile[y, x] = tex_b[y, x]

        tiles.append(tile)
    return tiles


def generate_decoration_tiles(biome, noise, count=8, seed_offset=0):
    """Generate decoration tiles (transparent bg with scattered details)."""
    tiles = []
    rng = random.Random(seed_offset)

    for i in range(count):
        tile = np.zeros((TILE_SIZE, TILE_SIZE, 4), dtype=np.uint8)
        colors = biome.decoration_colors

        # Scatter small detail pixels
        n_details = rng.randint(3, 8)
        for _ in range(n_details):
            cx = rng.randint(4, TILE_SIZE - 5)
            cy = rng.randint(4, TILE_SIZE - 5)
            color = colors[rng.randint(0, len(colors) - 1)]
            # Draw small 2-4 pixel cluster
            size = rng.randint(1, 3)
            for dy in range(-size, size + 1):
                for dx in range(-size, size + 1):
                    if abs(dy) + abs(dx) <= size:
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < TILE_SIZE and 0 <= nx < TILE_SIZE:
                            tile[ny, nx] = color

        tiles.append(tile)
    return tiles


def generate_water_tiles(biome, noise, frames=4, seed_offset=0):
    """Generate water animation frame tiles."""
    tiles = []
    for f in range(frames):
        grid = noise.generate_grid(
            TILE_SIZE, TILE_SIZE, scale=6.0, octaves=3,
            offset_x=seed_offset * 400 + f * 8, offset_y=seed_offset * 350 + f * 6,
        )
        tile = noise_to_palette(grid, biome.water_palette)
        tiles.append(tile)
    return tiles


# ═══════════════════════════════════════════════════════════════════════
# Section 5: Stamp Generators
# ═══════════════════════════════════════════════════════════════════════

def generate_fractal_tree(stamp, noise, seed_offset=0):
    """Generate a tree stamp using trunk + noise-shaped canopy."""
    w, h = stamp.width, stamp.height
    img = np.zeros((h, w, 4), dtype=np.uint8)
    trunk_color = stamp.params.get("trunk", _rgba(100, 70, 35))
    leaf_colors = stamp.params.get("leaves", [_rgba(60, 130, 40)])
    shape = stamp.params.get("shape", "round")

    # Draw trunk (bottom third, centered)
    trunk_w = max(4, w // 8)
    trunk_h = h // 3
    tx = w // 2 - trunk_w // 2
    for y in range(h - trunk_h, h):
        for x in range(tx, tx + trunk_w):
            if 0 <= x < w:
                # Add slight variation
                c = list(trunk_color)
                c[0] = max(0, min(255, c[0] + random.randint(-10, 10)))
                img[y, x] = c

    # Draw canopy (upper two-thirds)
    if leaf_colors:
        canopy_h = h - trunk_h
        cx, cy = w // 2, canopy_h // 2

        canopy_noise = noise.generate_grid(w, canopy_h, scale=4.0, octaves=2,
                                            offset_x=seed_offset*500, offset_y=seed_offset*450)

        for y in range(canopy_h):
            for x in range(w):
                # Elliptical shape
                dx = (x - cx) / (w * 0.45)
                if shape == "conical":
                    # Triangle shape wider at bottom
                    progress = y / canopy_h
                    max_width = progress * 0.9 + 0.1
                    dx = (x - cx) / (w * 0.45 * max_width) if max_width > 0 else 10
                    dy = (y - cy) / (canopy_h * 0.48)
                elif shape == "palm":
                    # Palm fronds: drooping shape
                    dy = (y - canopy_h * 0.3) / (canopy_h * 0.55)
                    dx = (x - cx) / (w * 0.4)
                else:
                    dy = (y - cy) / (canopy_h * 0.48)

                dist = dx*dx + dy*dy
                noise_val = canopy_noise[y, x] * 0.3

                threshold = 1.0 + noise_val
                if shape == "dead" or not leaf_colors:
                    continue

                if dist < threshold:
                    color_idx = int(canopy_noise[y, x] * len(leaf_colors)) % len(leaf_colors)
                    img[y, x] = leaf_colors[color_idx]

        # Add fruit if specified
        fruit_color = stamp.params.get("fruit")
        if fruit_color and leaf_colors:
            rng = random.Random(seed_offset)
            for _ in range(3):
                fx = rng.randint(w // 4, 3 * w // 4)
                fy = rng.randint(canopy_h // 3, canopy_h - 5)
                if img[fy, fx, 3] > 0:
                    for dy in range(-1, 2):
                        for dx in range(-1, 2):
                            ny, nx = fy + dy, fx + dx
                            if 0 <= ny < h and 0 <= nx < w:
                                img[ny, nx] = fruit_color

        # Snow on top
        if stamp.params.get("snow"):
            for y in range(min(8, canopy_h)):
                for x in range(w):
                    if img[y, x, 3] > 0:
                        img[y, x] = _rgba(230, 235, 242)

    return add_outline(add_highlight(img))


def generate_noise_blob(stamp, noise, seed_offset=0):
    """Generate a blob-shaped stamp (rocks, bushes, piles)."""
    w, h = stamp.width, stamp.height
    img = np.zeros((h, w, 4), dtype=np.uint8)
    colors = stamp.params.get("colors", [_rgba(128, 128, 128)])

    blob_noise = noise.generate_grid(w, h, scale=3.0, octaves=3,
                                      offset_x=seed_offset*600, offset_y=seed_offset*550)
    cx, cy = w / 2, h / 2

    for y in range(h):
        for x in range(w):
            dx = (x - cx) / (w * 0.42)
            dy = (y - cy) / (h * 0.42)
            dist = dx*dx + dy*dy
            threshold = 0.8 + blob_noise[y, x] * 0.4
            if dist < threshold:
                color_idx = int(blob_noise[y, x] * len(colors)) % len(colors)
                img[y, x] = colors[color_idx]

    return add_outline(add_highlight(img))


def generate_geometric_building(stamp, noise, seed_offset=0):
    """Generate a simple building stamp with walls, roof, and door."""
    w, h = stamp.width, stamp.height
    img = np.zeros((h, w, 4), dtype=np.uint8)
    wall_color = stamp.params.get("wall", _rgba(180, 160, 120))
    roof_color = stamp.params.get("roof", _rgba(160, 60, 50))
    door_color = stamp.params.get("door", _rgba(100, 65, 30))
    is_tent = stamp.params.get("tent", False)
    is_ruined = stamp.params.get("ruined", False)

    roof_h = h // 3
    wall_h = h - roof_h
    margin = max(2, w // 16)

    # Draw walls
    for y in range(roof_h, h):
        for x in range(margin, w - margin):
            c = list(wall_color)
            # Add brick texture
            if (y % 6 < 1) or (x % 8 < 1 and (y // 6) % 2 == 0) or ((x + 4) % 8 < 1 and (y // 6) % 2 == 1):
                c[0] = max(0, c[0] - 20)
                c[1] = max(0, c[1] - 20)
                c[2] = max(0, c[2] - 20)
            else:
                c[0] = max(0, min(255, c[0] + random.randint(-8, 8)))
            if is_ruined and random.random() < 0.15:
                continue  # Skip pixels for ruined look
            img[y, x] = c

    # Draw roof (triangle or tent)
    for y in range(roof_h):
        progress = y / roof_h
        if is_tent:
            roof_left = int(margin + (w // 2 - margin) * (1 - progress))
            roof_right = int(w - margin - (w // 2 - margin) * (1 - progress))
        else:
            roof_left = int(w // 2 - (w // 2 - margin + 4) * progress)
            roof_right = int(w // 2 + (w // 2 - margin + 4) * progress)

        for x in range(max(0, roof_left), min(w, roof_right)):
            c = list(roof_color)
            c[0] = max(0, min(255, c[0] + random.randint(-5, 5)))
            if is_ruined and random.random() < 0.1:
                continue
            img[y, x] = c

    # Draw door
    door_w = max(8, w // 6)
    door_h = max(12, wall_h // 2)
    dx = w // 2 - door_w // 2
    for y in range(h - door_h, h):
        for x in range(dx, dx + door_w):
            if 0 <= x < w:
                img[y, x] = door_color

    # Window
    win_size = max(6, w // 10)
    if w > 48:
        for wx_off in [w // 4, 3 * w // 4 - win_size]:
            for y in range(roof_h + wall_h // 4, roof_h + wall_h // 4 + win_size):
                for x in range(wx_off, wx_off + win_size):
                    if 0 <= y < h and 0 <= x < w:
                        img[y, x] = _rgba(140, 180, 220)

    return add_outline(img)


def generate_prop(stamp, noise, seed_offset=0):
    """Generate simple prop stamps (barrels, signs, fences, etc.)."""
    w, h = stamp.width, stamp.height
    img = np.zeros((h, w, 4), dtype=np.uint8)
    color = stamp.params.get("color", _rgba(140, 100, 55))
    shape = stamp.params.get("shape", "rect")

    cx, cy = w // 2, h // 2

    if shape == "barrel":
        # Oval barrel
        for y in range(h):
            for x in range(w):
                dx = (x - cx) / (w * 0.4)
                dy = (y - cy) / (h * 0.45)
                if dx*dx + dy*dy < 1.0:
                    c = list(color)
                    # Horizontal bands
                    if y % (h // 4) < 2:
                        c = [max(0, c[0]-30), max(0, c[1]-30), max(0, c[2]-30), 255]
                    img[y, x] = c

    elif shape == "post":
        # Vertical post
        pw = max(4, w // 4)
        px = cx - pw // 2
        for y in range(h):
            for x in range(px, px + pw):
                if 0 <= x < w:
                    c = list(color)
                    c[0] = max(0, min(255, c[0] + random.randint(-8, 8)))
                    img[y, x] = c

    elif shape == "sign":
        # Post with sign board
        pw = max(3, w // 6)
        px = cx - pw // 2
        # Post
        for y in range(h // 3, h):
            for x in range(px, px + pw):
                if 0 <= x < w:
                    img[y, x] = color
        # Board
        for y in range(2, h // 3):
            for x in range(3, w - 3):
                c = list(color)
                c[0] = min(255, c[0] + 20)
                c[1] = min(255, c[1] + 15)
                img[y, x] = c

    elif shape in ("mushroom", "pumpkin", "shell", "crab", "chicken", "snowman",
                    "skull", "lilypad", "cobweb", "torch", "chest", "crystal",
                    "stump", "log", "bench", "crate", "cart", "trough",
                    "flowerbed", "umbrella"):
        # Generic elliptical shape with color
        for y in range(h):
            for x in range(w):
                dx = (x - cx) / (w * 0.42)
                dy = (y - cy) / (h * 0.42)
                if dx*dx + dy*dy < 1.0:
                    c = list(color)
                    c[0] = max(0, min(255, c[0] + random.randint(-12, 12)))
                    c[1] = max(0, min(255, c[1] + random.randint(-8, 8)))
                    img[y, x] = c

    else:
        # Default rectangle
        for y in range(2, h - 2):
            for x in range(2, w - 2):
                img[y, x] = color

    return add_outline(img)


def generate_fence_line(stamp, noise, seed_offset=0):
    """Generate a fence segment with posts and rails."""
    w, h = stamp.width, stamp.height
    img = np.zeros((h, w, 4), dtype=np.uint8)
    color = stamp.params.get("color", _rgba(140, 100, 55))
    n_posts = stamp.params.get("posts", 3)

    post_w = max(3, w // 16)
    rail_h = 2

    # Draw rails
    for rail_y in [h // 3, 2 * h // 3]:
        for y in range(rail_y, rail_y + rail_h):
            for x in range(w):
                if 0 <= y < h:
                    img[y, x] = color

    # Draw posts
    spacing = w // max(1, n_posts - 1) if n_posts > 1 else w // 2
    for i in range(n_posts):
        px = i * spacing if n_posts > 1 else w // 2
        px = min(px, w - post_w)
        for y in range(2, h - 2):
            for x in range(px, min(px + post_w, w)):
                c = list(color)
                c[0] = max(0, c[0] - 15)
                img[y, x] = c

    return add_outline(img)


STAMP_GENERATORS = {
    "fractal_tree": generate_fractal_tree,
    "noise_blob": generate_noise_blob,
    "geometric_building": generate_geometric_building,
    "prop_simple": generate_prop,
    "fence_line": generate_fence_line,
}


# ═══════════════════════════════════════════════════════════════════════
# Section 6: Atlas Composer
# ═══════════════════════════════════════════════════════════════════════

ATLAS_WIDTH = 640
COLUMNS = ATLAS_WIDTH // TILE_SIZE  # 20


class AtlasComposer:
    """Compose tiles and stamps into a single atlas PNG."""

    def __init__(self):
        self.tiles = []
        self.stamps = []

    def add_tile(self, tile):
        """Add a 32x32 tile. Returns 1-indexed tile ID."""
        assert tile.shape == (TILE_SIZE, TILE_SIZE, 4), f"Tile must be {TILE_SIZE}x{TILE_SIZE}, got {tile.shape[:2]}"
        self.tiles.append(tile)
        return len(self.tiles)

    def add_tiles(self, tiles):
        """Add multiple tiles. Returns list of 1-indexed tile IDs."""
        return [self.add_tile(t) for t in tiles]

    def add_stamp(self, name, image, category):
        """Add a stamp image. Position computed during compose()."""
        self.stamps.append({"name": name, "image": image, "category": category})

    def compose(self):
        """Build the final atlas image. Returns (PIL Image, stamps_text)."""
        # Calculate tile grid area
        tile_rows = math.ceil(len(self.tiles) / COLUMNS) if self.tiles else 0
        tile_area_h = tile_rows * TILE_SIZE

        # Calculate stamp area using shelf packing
        stamp_y = tile_area_h
        shelf_x = 0
        shelf_y = stamp_y
        shelf_h = 0
        stamp_entries = []

        for s in self.stamps:
            img = s["image"]
            sh, sw = img.shape[:2]

            if shelf_x + sw > ATLAS_WIDTH:
                # New shelf
                shelf_y += shelf_h
                shelf_x = 0
                shelf_h = 0

            stamp_entries.append({
                "name": s["name"],
                "category": s["category"],
                "px": shelf_x,
                "py": shelf_y,
                "w": sw,
                "h": sh,
                "image": img,
            })
            shelf_x += sw + 2  # 2px gap
            shelf_h = max(shelf_h, sh)

        total_h = shelf_y + shelf_h if stamp_entries else tile_area_h
        total_h = max(total_h, TILE_SIZE)

        # Create image
        atlas = np.zeros((total_h, ATLAS_WIDTH, 4), dtype=np.uint8)

        # Place tiles
        for i, tile in enumerate(self.tiles):
            col = i % COLUMNS
            row = i // COLUMNS
            y0 = row * TILE_SIZE
            x0 = col * TILE_SIZE
            atlas[y0:y0+TILE_SIZE, x0:x0+TILE_SIZE] = tile

        # Place stamps
        for entry in stamp_entries:
            y0, x0 = entry["py"], entry["px"]
            img = entry["image"]
            sh, sw = img.shape[:2]
            atlas[y0:y0+sh, x0:x0+sw] = img

        # Generate stamps text
        lines = []
        for entry in stamp_entries:
            lines.append(f"{entry['name']}|{entry['px']}|{entry['py']}|{entry['w']}|{entry['h']}|{entry['category']}")

        pil_img = Image.fromarray(atlas, "RGBA")
        return pil_img, "\n".join(lines) + "\n" if lines else ""


# ═══════════════════════════════════════════════════════════════════════
# Section 7: Main / CLI
# ═══════════════════════════════════════════════════════════════════════

def generate_biome(biome_name, output_dir, seed=None):
    """Generate a complete tileset for a single biome."""
    if biome_name not in BIOMES:
        print(f"Unknown biome: {biome_name}")
        return False

    biome = BIOMES[biome_name]
    if seed is None:
        seed = random.randint(0, 999999)
    noise = SimplexNoise2D(seed)
    random.seed(seed)

    print(f"Generating {biome.name} tileset (seed={seed})...")
    composer = AtlasComposer()
    manifest = {"biome": biome.name, "seed": seed}

    # 1. Base terrain tiles
    manifest["terrains"] = {}
    for ti, terrain in enumerate(biome.terrains):
        ids = composer.add_tiles(generate_base_tiles(terrain, noise, count=4, seed_offset=ti))
        manifest["terrains"][terrain.name] = ids
        print(f"  {terrain.name}: tiles {ids[0]}-{ids[-1]}")

    # 2. Transition tiles
    manifest["transitions"] = {}
    for pi, (a_idx, b_idx) in enumerate(biome.transition_pairs):
        ta = biome.terrains[a_idx]
        tb = biome.terrains[b_idx]
        trans = generate_transition_tiles(ta, tb, noise, seed_offset=pi)
        ids = composer.add_tiles(trans)
        key = f"{ta.name}_to_{tb.name}"
        manifest["transitions"][key] = ids
        print(f"  transitions {key}: tiles {ids[0]}-{ids[-1]}")

    # 3. Decorations
    decos = generate_decoration_tiles(biome, noise, count=8, seed_offset=100)
    deco_ids = composer.add_tiles(decos)
    manifest["decorations"] = deco_ids
    print(f"  decorations: tiles {deco_ids[0]}-{deco_ids[-1]}")

    # 4. Water
    water = generate_water_tiles(biome, noise, frames=4, seed_offset=200)
    water_ids = composer.add_tiles(water)
    manifest["water"] = {"first": water_ids[0], "last": water_ids[-1]}
    print(f"  water: tiles {water_ids[0]}-{water_ids[-1]}")

    # 5. Stamps
    manifest["stamps"] = []
    for si, sdef in enumerate(biome.stamps):
        gen_func = STAMP_GENERATORS.get(sdef.generator, generate_prop)
        stamp_img = gen_func(sdef, noise, seed_offset=si + 300)
        composer.add_stamp(sdef.name, stamp_img, sdef.category)
        manifest["stamps"].append({"name": sdef.name, "category": sdef.category,
                                    "size": f"{sdef.width}x{sdef.height}"})
        print(f"  stamp: {sdef.name} ({sdef.category}, {sdef.width}x{sdef.height})")

    # Compose and save
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    atlas_img, stamps_text = composer.compose()

    tileset_path = output_path / f"{biome.name}_tileset.png"
    stamps_path = output_path / f"{biome.name}_stamps.txt"
    manifest_path = output_path / f"{biome.name}_manifest.json"

    atlas_img.save(str(tileset_path))
    stamps_path.write_text(stamps_text)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

    print(f"\nSaved:")
    print(f"  {tileset_path} ({atlas_img.size[0]}x{atlas_img.size[1]})")
    print(f"  {stamps_path} ({len(biome.stamps)} stamps)")
    print(f"  {manifest_path}")
    print(f"  Total tiles: {len(composer.tiles)}")
    return True


def main():
    parser = argparse.ArgumentParser(
        prog="generate_tileset",
        description="Procedural tileset generator for Twilight Engine",
    )
    parser.add_argument("--biome", default=None,
                        help=f"Biome name or 'all'. Choices: {', '.join(BIOMES.keys())}")
    parser.add_argument("--output", "-o", default=".",
                        help="Output directory (default: current dir)")
    parser.add_argument("--seed", type=int, default=None,
                        help="Random seed for reproducibility")
    parser.add_argument("--list-biomes", action="store_true",
                        help="List available biomes and exit")

    args = parser.parse_args()

    if args.list_biomes or args.biome is None:
        print("Available biomes:")
        for name, biome in BIOMES.items():
            n_terrains = len(biome.terrains)
            n_stamps = len(biome.stamps)
            terrain_names = ", ".join(t.name for t in biome.terrains)
            print(f"  {name:12s}  terrains: {terrain_names}")
            print(f"               stamps: {n_stamps} objects")
        if args.biome is None and not args.list_biomes:
            print("\nUsage: python generate_tileset.py --biome grasslands --output /tmp/tiles/")
        return

    if args.biome == "all":
        for name in BIOMES:
            generate_biome(name, args.output, seed=args.seed)
            print()
    else:
        if args.biome not in BIOMES:
            print(f"Unknown biome: {args.biome}")
            print(f"Available: {', '.join(BIOMES.keys())}")
            return
        generate_biome(args.biome, args.output, seed=args.seed)


if __name__ == "__main__":
    main()
