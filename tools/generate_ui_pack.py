#!/usr/bin/env python3
"""Procedural pixel-art UI/HUD component generator.

Generates a complete UI sprite sheet with 9-slice panels, buttons, bars,
frames, and HUD elements in multiple color themes. Output is engine-compatible.

Usage:
    python tools/generate_ui_pack.py --output games/demo/assets/textures/
    python tools/generate_ui_pack.py --theme dark --output /tmp/ui/
    python tools/generate_ui_pack.py --list-themes

9-Slice System:
    Each panel is divided into 9 regions:
    [TL][T ][TR]    Corners (TL,TR,BL,BR) stay fixed size
    [L ][ C][R ]    Edges (T,B,L,R) tile/stretch along their axis
    [BL][B ][BR]    Center (C) fills the remaining space
"""

import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

TILE_SIZE = 16  # Base tile size for UI elements


# ═══════════════════════════════════════════════════════════════
# Color Themes
# ═══════════════════════════════════════════════════════════════

THEMES = {
    "fantasy": {
        "name": "Fantasy RPG",
        "panel_bg": (42, 35, 48),
        "panel_border_outer": (85, 65, 55),
        "panel_border_inner": (120, 95, 75),
        "panel_highlight": (155, 130, 100),
        "button_bg": (65, 55, 70),
        "button_hover": (85, 75, 95),
        "button_pressed": (45, 38, 52),
        "button_border": (110, 90, 75),
        "bar_bg": (30, 25, 35),
        "bar_hp": (65, 180, 55),
        "bar_mp": (55, 100, 200),
        "bar_xp": (200, 170, 40),
        "bar_border": (80, 65, 55),
        "text_primary": (240, 230, 210),
        "text_secondary": (180, 165, 140),
        "text_gold": (255, 215, 80),
        "text_danger": (220, 60, 60),
        "tooltip_bg": (35, 30, 42, 230),
        "tooltip_border": (100, 85, 70),
    },
    "dark": {
        "name": "Dark UI",
        "panel_bg": (25, 25, 30),
        "panel_border_outer": (55, 55, 65),
        "panel_border_inner": (80, 80, 95),
        "panel_highlight": (110, 110, 130),
        "button_bg": (40, 40, 50),
        "button_hover": (55, 55, 70),
        "button_pressed": (30, 30, 38),
        "button_border": (75, 75, 90),
        "bar_bg": (20, 20, 25),
        "bar_hp": (50, 200, 80),
        "bar_mp": (60, 120, 220),
        "bar_xp": (220, 180, 50),
        "bar_border": (60, 60, 70),
        "text_primary": (220, 220, 230),
        "text_secondary": (150, 150, 170),
        "text_gold": (255, 220, 80),
        "text_danger": (230, 60, 60),
        "tooltip_bg": (20, 20, 28, 240),
        "tooltip_border": (70, 70, 85),
    },
    "medieval": {
        "name": "Medieval Stone",
        "panel_bg": (55, 50, 45),
        "panel_border_outer": (95, 85, 70),
        "panel_border_inner": (130, 115, 95),
        "panel_highlight": (170, 155, 130),
        "button_bg": (70, 62, 52),
        "button_hover": (90, 80, 68),
        "button_pressed": (50, 44, 38),
        "button_border": (120, 105, 85),
        "bar_bg": (40, 36, 32),
        "bar_hp": (60, 170, 50),
        "bar_mp": (50, 90, 190),
        "bar_xp": (190, 160, 40),
        "bar_border": (100, 88, 72),
        "text_primary": (230, 220, 200),
        "text_secondary": (170, 155, 130),
        "text_gold": (245, 210, 75),
        "text_danger": (210, 55, 55),
        "tooltip_bg": (45, 40, 36, 235),
        "tooltip_border": (110, 98, 80),
    },
    "cute": {
        "name": "Cute Fantasy",
        "panel_bg": (60, 52, 68),
        "panel_border_outer": (110, 85, 75),
        "panel_border_inner": (155, 125, 105),
        "panel_highlight": (195, 170, 145),
        "button_bg": (75, 65, 85),
        "button_hover": (100, 88, 112),
        "button_pressed": (55, 48, 65),
        "button_border": (140, 115, 95),
        "bar_bg": (45, 38, 52),
        "bar_hp": (70, 190, 65),
        "bar_mp": (65, 110, 210),
        "bar_xp": (210, 180, 50),
        "bar_border": (120, 100, 82),
        "text_primary": (245, 235, 220),
        "text_secondary": (190, 175, 155),
        "text_gold": (255, 220, 85),
        "text_danger": (225, 65, 65),
        "tooltip_bg": (50, 42, 58, 230),
        "tooltip_border": (130, 108, 90),
    },
}


# ═══════════════════════════════════════════════════════════════
# 9-Slice Panel Generator
# ═══════════════════════════════════════════════════════════════

def draw_9slice_panel(img, x, y, w, h, theme, variant="window"):
    """Draw a 9-slice panel onto an image at (x, y) with size (w, h)."""
    draw = ImageDraw.Draw(img)
    bg = theme["panel_bg"]
    outer = theme["panel_border_outer"]
    inner = theme["panel_border_inner"]
    hi = theme["panel_highlight"]
    b = 2  # border thickness

    # Background fill
    draw.rectangle([x+b, y+b, x+w-b-1, y+h-b-1], fill=bg)

    # Outer border
    draw.rectangle([x, y, x+w-1, y+h-1], outline=outer)

    # Inner border (1px inside)
    draw.rectangle([x+1, y+1, x+w-2, y+h-2], outline=inner)

    # Highlight on top and left edges (light direction)
    draw.line([(x+2, y+2), (x+w-3, y+2)], fill=hi)  # top
    draw.line([(x+2, y+2), (x+2, y+h-3)], fill=hi)   # left

    # Shadow on bottom and right edges
    shadow = tuple(max(0, c - 30) for c in bg)
    draw.line([(x+2, y+h-3), (x+w-3, y+h-3)], fill=shadow)  # bottom
    draw.line([(x+w-3, y+2), (x+w-3, y+h-3)], fill=shadow)   # right

    if variant == "title":
        # Title bar accent
        draw.rectangle([x+2, y+2, x+w-3, y+12], fill=inner)
        draw.line([(x+2, y+13), (x+w-3, y+13)], fill=outer)


def draw_button(img, x, y, w, h, theme, state="normal"):
    """Draw a pixel-art button."""
    draw = ImageDraw.Draw(img)
    if state == "hover":
        bg = theme["button_hover"]
    elif state == "pressed":
        bg = theme["button_pressed"]
    else:
        bg = theme["button_bg"]

    border = theme["button_border"]
    hi = theme["panel_highlight"]

    draw.rectangle([x+1, y+1, x+w-2, y+h-2], fill=bg)
    draw.rectangle([x, y, x+w-1, y+h-1], outline=border)

    if state != "pressed":
        draw.line([(x+1, y+1), (x+w-2, y+1)], fill=hi)
        draw.line([(x+1, y+1), (x+1, y+h-2)], fill=hi)

    shadow = tuple(max(0, c - 25) for c in bg)
    draw.line([(x+1, y+h-2), (x+w-2, y+h-2)], fill=shadow)
    draw.line([(x+w-2, y+1), (x+w-2, y+h-2)], fill=shadow)


def draw_bar(img, x, y, w, h, theme, bar_type="hp", fill_pct=0.75):
    """Draw a progress bar (HP, MP, XP)."""
    draw = ImageDraw.Draw(img)
    bar_bg = theme["bar_bg"]
    border = theme["bar_border"]

    color_map = {"hp": theme["bar_hp"], "mp": theme["bar_mp"], "xp": theme["bar_xp"]}
    bar_color = color_map.get(bar_type, theme["bar_hp"])

    # Background
    draw.rectangle([x, y, x+w-1, y+h-1], fill=bar_bg, outline=border)

    # Fill
    fill_w = int((w - 4) * fill_pct)
    if fill_w > 0:
        draw.rectangle([x+2, y+2, x+2+fill_w-1, y+h-3], fill=bar_color)
        # Highlight on fill
        hi = tuple(min(255, c + 40) for c in bar_color)
        draw.line([(x+2, y+2), (x+2+fill_w-1, y+2)], fill=hi)


def draw_checkbox(img, x, y, size, theme, checked=False):
    draw = ImageDraw.Draw(img)
    draw.rectangle([x, y, x+size-1, y+size-1], fill=theme["panel_bg"], outline=theme["button_border"])
    if checked:
        # X mark
        draw.line([(x+3, y+3), (x+size-4, y+size-4)], fill=theme["text_gold"], width=1)
        draw.line([(x+3, y+size-4), (x+size-4, y+3)], fill=theme["text_gold"], width=1)


def draw_slider(img, x, y, w, h, theme, value=0.5):
    draw = ImageDraw.Draw(img)
    draw.rectangle([x, y+h//2-1, x+w-1, y+h//2+1], fill=theme["bar_bg"], outline=theme["bar_border"])
    # Knob
    kx = x + int((w - 6) * value)
    draw.rectangle([kx, y, kx+6, y+h-1], fill=theme["button_bg"], outline=theme["button_border"])


# ═══════════════════════════════════════════════════════════════
# Sprite Sheet Generator
# ═══════════════════════════════════════════════════════════════

def generate_ui_pack(theme_name, output_dir):
    """Generate a complete UI sprite sheet for the given theme."""
    if theme_name not in THEMES:
        print(f"Unknown theme: {theme_name}")
        return

    theme = THEMES[theme_name]
    print(f"Generating UI pack: {theme['name']} ({theme_name})")

    # Sheet layout: 512x512, packed with all components
    W, H = 512, 512
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    regions = {}  # name -> (x, y, w, h)
    y_cursor = 0

    # ── Panels (9-slice, multiple sizes) ──
    panel_sizes = [
        ("panel_small", 64, 48),
        ("panel_medium", 128, 96),
        ("panel_large", 200, 150),
        ("panel_wide", 200, 48),
        ("panel_tall", 80, 150),
        ("panel_title", 200, 150),
    ]
    x_cursor = 0
    for name, pw, ph in panel_sizes:
        variant = "title" if "title" in name else "window"
        draw_9slice_panel(img, x_cursor, y_cursor, pw, ph, theme, variant)
        regions[name] = (x_cursor, y_cursor, pw, ph)
        x_cursor += pw + 4
        if x_cursor + 200 > W:
            x_cursor = 0
            y_cursor += ph + 4

    y_cursor = max(y_cursor, 160)

    # ── Buttons (3 states x 3 sizes) ──
    button_configs = [
        ("btn_small", 48, 20),
        ("btn_medium", 80, 24),
        ("btn_large", 120, 28),
    ]
    x_cursor = 0
    for name, bw, bh in button_configs:
        for state in ["normal", "hover", "pressed"]:
            draw_button(img, x_cursor, y_cursor, bw, bh, theme, state)
            regions[f"{name}_{state}"] = (x_cursor, y_cursor, bw, bh)
            x_cursor += bw + 4
    y_cursor += 32

    # ── Bars (HP, MP, XP at various fill levels) ──
    x_cursor = 0
    for bar_type in ["hp", "mp", "xp"]:
        for pct in [1.0, 0.75, 0.5, 0.25]:
            bw, bh = 100, 12
            draw_bar(img, x_cursor, y_cursor, bw, bh, theme, bar_type, pct)
            regions[f"bar_{bar_type}_{int(pct*100)}"] = (x_cursor, y_cursor, bw, bh)
            x_cursor += bw + 4
            if x_cursor + 100 > W:
                x_cursor = 0
                y_cursor += bh + 4
    y_cursor += 16

    # ── Checkboxes ──
    x_cursor = 0
    for checked in [False, True]:
        draw_checkbox(img, x_cursor, y_cursor, 14, theme, checked)
        regions[f"checkbox_{'on' if checked else 'off'}"] = (x_cursor, y_cursor, 14, 14)
        x_cursor += 18

    # ── Sliders ──
    for val in [0.0, 0.5, 1.0]:
        draw_slider(img, x_cursor, y_cursor, 80, 14, theme, val)
        regions[f"slider_{int(val*100)}"] = (x_cursor, y_cursor, 80, 14)
        x_cursor += 84
    y_cursor += 18

    # ── 9-Slice Corner/Edge tiles (for runtime assembly) ──
    # These are the raw 16x16 tiles that the engine can use to build panels of any size
    x_cursor = 0
    slice_size = 16
    # Generate a panel that's exactly 48x48 (3x3 grid of 16x16 tiles)
    temp = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    draw_9slice_panel(temp, 0, 0, 48, 48, theme)

    slice_names = [
        ("9s_tl", 0, 0), ("9s_t", 16, 0), ("9s_tr", 32, 0),
        ("9s_l", 0, 16), ("9s_c", 16, 16), ("9s_r", 32, 16),
        ("9s_bl", 0, 32), ("9s_b", 16, 32), ("9s_br", 32, 32),
    ]
    for name, sx, sy in slice_names:
        tile = temp.crop((sx, sy, sx+16, sy+16))
        img.paste(tile, (x_cursor, y_cursor))
        regions[name] = (x_cursor, y_cursor, 16, 16)
        x_cursor += 18
    y_cursor += 20

    # ── Tooltip background ──
    draw_9slice_panel(img, 0, y_cursor, 160, 40, theme)
    regions["tooltip"] = (0, y_cursor, 160, 40)
    y_cursor += 44

    # ── Separator line ──
    draw = ImageDraw.Draw(img)
    draw.line([(0, y_cursor), (200, y_cursor)], fill=theme["panel_border_inner"])
    regions["separator"] = (0, y_cursor, 200, 1)
    y_cursor += 4

    # ── Scroll arrows ──
    for name, dx, dy in [("arrow_up", 0, -1), ("arrow_down", 0, 1), ("arrow_left", -1, 0), ("arrow_right", 1, 0)]:
        x, y = x_cursor, y_cursor
        draw.rectangle([x, y, x+11, y+11], fill=theme["button_bg"], outline=theme["button_border"])
        # Draw arrow
        cx, cy = x + 6, y + 6
        pts = []
        if dy == -1: pts = [(cx, cy-2), (cx-3, cy+2), (cx+3, cy+2)]
        elif dy == 1: pts = [(cx, cy+2), (cx-3, cy-2), (cx+3, cy-2)]
        elif dx == -1: pts = [(cx-2, cy), (cx+2, cy-3), (cx+2, cy+3)]
        elif dx == 1: pts = [(cx+2, cy), (cx-2, cy-3), (cx-2, cy+3)]
        if pts: draw.polygon(pts, fill=theme["text_primary"])
        regions[name] = (x, y, 12, 12)
        x_cursor += 16

    # ── Save ──
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)

    # Trim image to actual content
    bbox = img.getbbox()
    if bbox:
        actual_h = bbox[3] + 4
        img = img.crop((0, 0, W, actual_h))

    sheet_path = output / f"ui_{theme_name}.png"
    img.save(str(sheet_path))

    # Generate regions file (same format as cf_stamps.txt)
    stamps_path = output / f"ui_{theme_name}_regions.txt"
    with open(stamps_path, "w") as f:
        for name, (rx, ry, rw, rh) in sorted(regions.items()):
            f.write(f"{name}|{rx}|{ry}|{rw}|{rh}|ui\n")

    # Generate manifest
    manifest = {
        "theme": theme_name,
        "display_name": theme["name"],
        "sheet": str(sheet_path.name),
        "regions": {name: {"x": rx, "y": ry, "w": rw, "h": rh}
                    for name, (rx, ry, rw, rh) in regions.items()},
        "total_regions": len(regions),
    }
    manifest_path = output / f"ui_{theme_name}_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")

    print(f"  Sheet: {sheet_path} ({img.size[0]}x{img.size[1]})")
    print(f"  Regions: {stamps_path} ({len(regions)} regions)")
    print(f"  Manifest: {manifest_path}")
    print(f"  Components: {len(panel_sizes)} panels, {len(button_configs)*3} buttons, "
          f"12 bars, 2 checkboxes, 3 sliders, 9 9-slice tiles, 4 arrows")


def main():
    parser = argparse.ArgumentParser(
        prog="generate_ui_pack",
        description="Procedural pixel-art UI/HUD component generator",
    )
    parser.add_argument("--theme", default="fantasy",
                        help=f"Theme: {', '.join(THEMES.keys())} or 'all'")
    parser.add_argument("--output", "-o", default=".",
                        help="Output directory")
    parser.add_argument("--list-themes", action="store_true",
                        help="List available themes")

    args = parser.parse_args()

    if args.list_themes:
        print("Available UI themes:")
        for name, t in THEMES.items():
            print(f"  {name:12s} — {t['name']}")
        return

    if args.theme == "all":
        for name in THEMES:
            generate_ui_pack(name, args.output)
            print()
    else:
        generate_ui_pack(args.theme, args.output)


if __name__ == "__main__":
    main()
