#!/usr/bin/env python3
"""
Procedural pixel-art parallax background generator.

Generates 4-5 layer parallax backgrounds for various biomes.
Uses only Python stdlib + Pillow. Noise is implemented inline.

Usage:
    python3 generate_parallax_bg.py --biome forest --output path/to/output/ --seed 42
"""

import argparse
import math
import os
import random
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw


# ---------------------------------------------------------------------------
# Inline value-noise implementation (tileable)
# ---------------------------------------------------------------------------

class ValueNoise:
    """Simple 1D value noise that tiles at a given period."""

    def __init__(self, rng: random.Random, period: int = 256):
        self.period = period
        self.values = [rng.random() for _ in range(period)]

    def _lerp(self, a, b, t):
        # Smoothstep interpolation
        t = t * t * (3 - 2 * t)
        return a + (b - a) * t

    def sample(self, x: float) -> float:
        """Sample noise at position x (tiles at self.period)."""
        x = x % self.period
        i = int(x) % self.period
        j = (i + 1) % self.period
        frac = x - int(x)
        return self._lerp(self.values[i], self.values[j], frac)

    def fbm(self, x: float, octaves: int = 4, lacunarity: float = 2.0,
            persistence: float = 0.5) -> float:
        """Fractal Brownian Motion — sum of multiple octaves."""
        value = 0.0
        amplitude = 1.0
        freq = 1.0
        max_amp = 0.0
        for _ in range(octaves):
            value += self.sample(x * freq) * amplitude
            max_amp += amplitude
            amplitude *= persistence
            freq *= lacunarity
        return value / max_amp


# ---------------------------------------------------------------------------
# Drawing helpers
# ---------------------------------------------------------------------------

def make_gradient(width: int, height: int, color_top: tuple, color_bot: tuple) -> Image.Image:
    """Create a vertical gradient image (RGB)."""
    img = Image.new("RGB", (width, height))
    pixels = img.load()
    for y in range(height):
        t = y / max(height - 1, 1)
        r = int(color_top[0] + (color_bot[0] - color_top[0]) * t)
        g = int(color_top[1] + (color_bot[1] - color_top[1]) * t)
        b = int(color_top[2] + (color_bot[2] - color_top[2]) * t)
        for x in range(width):
            pixels[x, y] = (r, g, b)
    return img


def make_stars(img: Image.Image, rng: random.Random, count: int = 120,
               colors: list = None):
    """Scatter star pixels onto an image."""
    if colors is None:
        colors = [(255, 255, 255), (220, 220, 255), (255, 255, 200)]
    pixels = img.load()
    w, h = img.size
    for _ in range(count):
        x = rng.randint(0, w - 1)
        y = rng.randint(0, int(h * 0.7))
        c = rng.choice(colors)
        pixels[x, y] = c
        # Occasional bigger star (2x2)
        if rng.random() < 0.15:
            for dx in range(2):
                for dy in range(2):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        pixels[nx, ny] = c


def make_mountain_silhouette(width: int, height: int, noise: ValueNoise,
                              base_y: float, amplitude: float,
                              fill_color: tuple, octaves: int = 5,
                              noise_scale: float = 1.0) -> Image.Image:
    """Generate a mountain/hill silhouette layer (RGBA, transparent above)."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    points = []
    for x in range(width):
        n = noise.fbm(x * noise_scale, octaves=octaves)
        y = int(base_y + n * amplitude)
        y = max(0, min(height - 1, y))
        points.append((x, y))

    # Build polygon: top contour + bottom edge
    polygon = list(points) + [(width - 1, height), (0, height)]
    draw.polygon(polygon, fill=(*fill_color, 255))
    return img


def make_tree_line(width: int, height: int, noise: ValueNoise,
                   ground_y: float, ground_amp: float,
                   tree_color: tuple, rng: random.Random,
                   tree_height_range: tuple = (30, 70),
                   tree_width_range: tuple = (12, 28),
                   density: float = 0.08,
                   noise_scale: float = 1.5,
                   tree_style: str = "mixed") -> Image.Image:
    """Generate a tree-line silhouette layer."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Ground line
    ground = []
    for x in range(width):
        n = noise.fbm(x * noise_scale, octaves=3)
        y = int(ground_y + n * ground_amp)
        y = max(0, min(height - 1, y))
        ground.append(y)

    # Fill ground
    for x in range(width):
        gy = ground[x]
        draw.line([(x, gy), (x, height)], fill=(*tree_color, 255))

    # Place trees
    x = rng.randint(3, 10)
    while x < width:
        th = rng.randint(*tree_height_range)
        tw = rng.randint(*tree_width_range)
        gy = ground[min(x, width - 1)]
        top_y = gy - th

        style = tree_style
        if style == "mixed":
            style = rng.choice(["triangle", "round"])

        if style == "triangle":
            # Triangular (conifer)
            pts = [(x, top_y), (x - tw // 2, gy), (x + tw // 2, gy)]
            draw.polygon(pts, fill=(*tree_color, 255))
            # Trunk
            trunk_w = max(2, tw // 6)
            draw.rectangle([x - trunk_w // 2, gy, x + trunk_w // 2, gy + 4],
                           fill=(*tree_color, 255))
        elif style == "round":
            # Round (deciduous)
            r = tw // 2
            cy = top_y + r
            draw.ellipse([x - r, cy - r, x + r, cy + r],
                         fill=(*tree_color, 255))
            trunk_w = max(2, tw // 5)
            draw.rectangle([x - trunk_w // 2, cy + r - 2, x + trunk_w // 2, gy],
                           fill=(*tree_color, 255))
        elif style == "cactus":
            # Cactus shape
            trunk_w = max(3, tw // 4)
            draw.rectangle([x - trunk_w // 2, top_y, x + trunk_w // 2, gy],
                           fill=(*tree_color, 255))
            # Arms
            arm_y = top_y + th // 3
            arm_len = tw // 3
            # Left arm
            if rng.random() < 0.7:
                draw.rectangle([x - trunk_w // 2 - arm_len, arm_y,
                                x - trunk_w // 2, arm_y + trunk_w],
                               fill=(*tree_color, 255))
                draw.rectangle([x - trunk_w // 2 - arm_len, arm_y - arm_len // 2,
                                x - trunk_w // 2 - arm_len + trunk_w, arm_y],
                               fill=(*tree_color, 255))
            # Right arm
            if rng.random() < 0.7:
                draw.rectangle([x + trunk_w // 2, arm_y + 5,
                                x + trunk_w // 2 + arm_len, arm_y + 5 + trunk_w],
                               fill=(*tree_color, 255))
                draw.rectangle([x + trunk_w // 2 + arm_len - trunk_w, arm_y + 5 - arm_len // 2,
                                x + trunk_w // 2 + arm_len, arm_y + 5],
                               fill=(*tree_color, 255))

        spacing = int(1.0 / max(density, 0.01))
        x += rng.randint(max(1, spacing // 2), spacing)

    return img


def make_stalactites(width: int, height: int, noise: ValueNoise,
                     color: tuple, rng: random.Random,
                     ceiling_y: int = 0) -> Image.Image:
    """Generate stalactite ceiling layer for caves."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Ceiling rock
    for x in range(width):
        n = noise.fbm(x * 2.0, octaves=4)
        cy = ceiling_y + int(n * 40) + 20
        draw.line([(x, 0), (x, cy)], fill=(*color, 255))

    # Stalactites
    x = rng.randint(5, 20)
    while x < width:
        base_n = noise.fbm(x * 2.0, octaves=4)
        base_y = ceiling_y + int(base_n * 40) + 20
        length = rng.randint(20, 80)
        tip_w = rng.randint(2, 5)
        base_w = rng.randint(8, 18)

        # Tapered triangle
        pts = [(x - base_w // 2, base_y), (x + base_w // 2, base_y),
               (x + tip_w // 2, base_y + length), (x - tip_w // 2, base_y + length)]
        draw.polygon(pts, fill=(*color, 255))

        x += rng.randint(15, 50)

    return img


def make_rock_formations(width: int, height: int, noise: ValueNoise,
                         color: tuple, rng: random.Random,
                         ground_y: int = 400) -> Image.Image:
    """Generate rock formations for cave floors."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Floor
    for x in range(width):
        n = noise.fbm(x * 1.5, octaves=3)
        fy = ground_y + int(n * 30)
        draw.line([(x, fy), (x, height)], fill=(*color, 255))

    # Stalagmites
    x = rng.randint(5, 25)
    while x < width:
        n = noise.fbm(x * 1.5, octaves=3)
        base_y = ground_y + int(n * 30)
        h = rng.randint(15, 60)
        w = rng.randint(6, 16)
        pts = [(x - w // 2, base_y), (x + w // 2, base_y),
               (x + 1, base_y - h), (x - 1, base_y - h)]
        draw.polygon(pts, fill=(*color, 255))
        x += rng.randint(20, 55)

    return img


def make_foreground_bushes(width: int, height: int, noise: ValueNoise,
                           color: tuple, rng: random.Random,
                           base_y: int = 480) -> Image.Image:
    """Generate foreground bush/grass silhouette."""
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Ground fill
    for x in range(width):
        n = noise.fbm(x * 3.0, octaves=3)
        gy = base_y + int(n * 20)
        draw.line([(x, gy), (x, height)], fill=(*color, 255))

    # Bush blobs
    x = rng.randint(0, 15)
    while x < width:
        n = noise.fbm(x * 3.0, octaves=3)
        gy = base_y + int(n * 20)
        bw = rng.randint(15, 40)
        bh = rng.randint(10, 25)
        draw.ellipse([x - bw // 2, gy - bh, x + bw // 2, gy + 2],
                     fill=(*color, 255))
        x += rng.randint(10, 40)

    # Grass tufts
    for x in range(0, width, rng.randint(3, 8)):
        n = noise.fbm(x * 3.0, octaves=3)
        gy = base_y + int(n * 20)
        gh = rng.randint(4, 12)
        draw.line([(x, gy), (x - 1, gy - gh)], fill=(*color, 255))
        draw.line([(x, gy), (x + 1, gy - gh)], fill=(*color, 255))

    return img


def make_snow_trees(width: int, height: int, noise: ValueNoise,
                    tree_color: tuple, snow_color: tuple,
                    rng: random.Random,
                    ground_y: float = 380, ground_amp: float = -60,
                    noise_scale: float = 1.5) -> Image.Image:
    """Generate snow-covered tree line."""
    # Start with regular tree line
    img = make_tree_line(width, height, noise, ground_y, ground_amp,
                         tree_color, rng, tree_height_range=(25, 55),
                         tree_width_range=(14, 26), density=0.07,
                         noise_scale=noise_scale, tree_style="triangle")

    # Add snow caps — draw white on top portions of non-transparent pixels
    pixels = img.load()
    for x in range(width):
        found_top = False
        for y in range(height):
            if pixels[x, y][3] > 0 and not found_top:
                # Top of tree at this column — add snow for a few pixels
                snow_depth = rng.randint(3, 8)
                for sy in range(snow_depth):
                    if y + sy < height and pixels[x, y + sy][3] > 0:
                        pixels[x, y + sy] = (*snow_color, 255)
                found_top = True
            elif pixels[x, y][3] == 0 and found_top:
                found_top = False
    return img


def make_dune_silhouette(width: int, height: int, noise: ValueNoise,
                          base_y: float, amplitude: float,
                          fill_color: tuple, octaves: int = 3,
                          noise_scale: float = 0.8) -> Image.Image:
    """Generate smooth sand dune silhouette."""
    return make_mountain_silhouette(width, height, noise, base_y, amplitude,
                                    fill_color, octaves=octaves,
                                    noise_scale=noise_scale)


# ---------------------------------------------------------------------------
# Biome definitions
# ---------------------------------------------------------------------------

BIOMES = {
    "forest": {
        "sky_top": (30, 80, 160),
        "sky_bot": (130, 190, 230),
        "layers": [
            {"type": "mountain", "base_y": 300, "amp": -120, "color": (50, 80, 50),
             "octaves": 5, "scale": 0.8},
            {"type": "mountain", "base_y": 360, "amp": -80, "color": (40, 100, 45),
             "octaves": 4, "scale": 1.2},
            {"type": "trees", "ground_y": 400, "ground_amp": -40, "color": (25, 65, 30),
             "tree_h": (35, 70), "tree_w": (14, 28), "density": 0.07, "style": "mixed"},
            {"type": "foreground", "base_y": 490, "color": (15, 40, 18)},
        ],
    },
    "cave": {
        "sky_top": (20, 18, 25),
        "sky_bot": (45, 40, 50),
        "layers": [
            {"type": "stalactites", "color": (55, 50, 60), "ceiling_y": 0},
            {"type": "mountain", "base_y": 250, "amp": -100, "color": (60, 55, 65),
             "octaves": 4, "scale": 1.0},
            {"type": "rocks", "color": (50, 45, 55), "ground_y": 420},
            {"type": "foreground", "base_y": 500, "color": (30, 28, 35)},
        ],
    },
    "night": {
        "sky_top": (10, 10, 40),
        "sky_bot": (30, 20, 60),
        "stars": True,
        "layers": [
            {"type": "mountain", "base_y": 320, "amp": -140, "color": (20, 15, 40),
             "octaves": 5, "scale": 0.7},
            {"type": "mountain", "base_y": 380, "amp": -80, "color": (25, 20, 50),
             "octaves": 4, "scale": 1.1},
            {"type": "trees", "ground_y": 410, "ground_amp": -35, "color": (15, 12, 35),
             "tree_h": (30, 60), "tree_w": (12, 24), "density": 0.06, "style": "mixed"},
            {"type": "foreground", "base_y": 490, "color": (10, 8, 25)},
        ],
    },
    "sunset": {
        "sky_top": (200, 80, 50),
        "sky_bot": (255, 180, 80),
        "layers": [
            {"type": "mountain", "base_y": 310, "amp": -130, "color": (80, 30, 60),
             "octaves": 5, "scale": 0.75},
            {"type": "mountain", "base_y": 370, "amp": -70, "color": (60, 25, 50),
             "octaves": 4, "scale": 1.3},
            {"type": "trees", "ground_y": 400, "ground_amp": -40, "color": (40, 15, 35),
             "tree_h": (30, 65), "tree_w": (12, 26), "density": 0.07, "style": "mixed"},
            {"type": "foreground", "base_y": 490, "color": (25, 10, 22)},
        ],
    },
    "snow": {
        "sky_top": (160, 175, 200),
        "sky_bot": (210, 220, 235),
        "layers": [
            {"type": "mountain", "base_y": 280, "amp": -130, "color": (200, 210, 220),
             "octaves": 5, "scale": 0.8},
            {"type": "mountain", "base_y": 350, "amp": -80, "color": (170, 180, 195),
             "octaves": 4, "scale": 1.1},
            {"type": "snow_trees", "ground_y": 400, "ground_amp": -40,
             "tree_color": (40, 60, 45), "snow_color": (230, 235, 240)},
            {"type": "foreground", "base_y": 490, "color": (190, 200, 210)},
        ],
    },
    "desert": {
        "sky_top": (200, 130, 60),
        "sky_bot": (240, 200, 130),
        "layers": [
            {"type": "dune", "base_y": 330, "amp": -80, "color": (190, 150, 90),
             "octaves": 3, "scale": 0.5},
            {"type": "dune", "base_y": 380, "amp": -50, "color": (170, 130, 75),
             "octaves": 3, "scale": 0.8},
            {"type": "trees", "ground_y": 430, "ground_amp": -20, "color": (80, 100, 50),
             "tree_h": (20, 50), "tree_w": (8, 16), "density": 0.025, "style": "cactus"},
            {"type": "foreground", "base_y": 500, "color": (150, 115, 65)},
        ],
    },
}


# ---------------------------------------------------------------------------
# Main generation
# ---------------------------------------------------------------------------

def generate_biome(biome_name: str, output_dir: str, seed: int = 42,
                   width: int = 960, height: int = 540):
    biome = BIOMES.get(biome_name)
    if biome is None:
        print(f"Unknown biome: {biome_name}. Available: {', '.join(BIOMES.keys())}")
        sys.exit(1)

    rng = random.Random(seed)
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    # Use the width as noise period so it tiles horizontally
    noise_period = width

    layer_idx = 0

    # Layer 0: Sky gradient
    sky = make_gradient(width, height, biome["sky_top"], biome["sky_bot"])
    if biome.get("stars"):
        make_stars(sky, rng)
    sky.save(out / f"layer_{layer_idx}_sky.png", optimize=True)
    print(f"  layer_{layer_idx}_sky.png (gradient)")
    layer_idx += 1

    # Content layers
    for ldef in biome["layers"]:
        # Each layer gets its own noise generator for variety
        noise = ValueNoise(rng, period=noise_period)

        ltype = ldef["type"]

        if ltype == "mountain" or ltype == "dune":
            img = make_mountain_silhouette(
                width, height, noise,
                base_y=ldef["base_y"], amplitude=ldef["amp"],
                fill_color=ldef["color"], octaves=ldef.get("octaves", 5),
                noise_scale=ldef.get("scale", 1.0),
            )
            name = "mountains" if ltype == "mountain" else "dunes"

        elif ltype == "trees":
            img = make_tree_line(
                width, height, noise,
                ground_y=ldef["ground_y"], ground_amp=ldef["ground_amp"],
                tree_color=ldef["color"], rng=rng,
                tree_height_range=ldef.get("tree_h", (30, 60)),
                tree_width_range=ldef.get("tree_w", (12, 24)),
                density=ldef.get("density", 0.07),
                tree_style=ldef.get("style", "mixed"),
            )
            name = "trees"

        elif ltype == "stalactites":
            img = make_stalactites(
                width, height, noise,
                color=ldef["color"], rng=rng,
                ceiling_y=ldef.get("ceiling_y", 0),
            )
            name = "stalactites"

        elif ltype == "rocks":
            img = make_rock_formations(
                width, height, noise,
                color=ldef["color"], rng=rng,
                ground_y=ldef.get("ground_y", 420),
            )
            name = "rocks"

        elif ltype == "snow_trees":
            img = make_snow_trees(
                width, height, noise,
                tree_color=ldef["tree_color"],
                snow_color=ldef["snow_color"],
                rng=rng,
                ground_y=ldef.get("ground_y", 380),
                ground_amp=ldef.get("ground_amp", -60),
            )
            name = "snow_trees"

        elif ltype == "foreground":
            img = make_foreground_bushes(
                width, height, noise,
                color=ldef["color"], rng=rng,
                base_y=ldef.get("base_y", 480),
            )
            name = "foreground"

        else:
            print(f"  Unknown layer type: {ltype}")
            continue

        out_path = out / f"layer_{layer_idx}_{name}.png"
        img.save(out_path, optimize=True)
        print(f"  layer_{layer_idx}_{name}.png ({ltype})")
        layer_idx += 1

    print(f"  -> {layer_idx} layers generated")


def main():
    parser = argparse.ArgumentParser(
        description="Procedural pixel-art parallax background generator")
    parser.add_argument("--biome", required=True,
                        choices=list(BIOMES.keys()),
                        help="Biome preset to generate")
    parser.add_argument("--output", required=True,
                        help="Output directory for PNG layers")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed for reproducibility")
    parser.add_argument("--width", type=int, default=960,
                        help="Layer width in pixels (default: 960)")
    parser.add_argument("--height", type=int, default=540,
                        help="Layer height in pixels (default: 540)")
    args = parser.parse_args()

    print(f"Generating '{args.biome}' biome (seed={args.seed})...")
    generate_biome(args.biome, args.output, args.seed, args.width, args.height)
    print("Done!")


if __name__ == "__main__":
    main()
