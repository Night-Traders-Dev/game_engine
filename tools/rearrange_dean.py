#!/usr/bin/env python3
"""
Rearrange dean_sprites.png into the layout expected by the game engine.

Current layout (5x5 grid, 158x210 cells, extracted top-to-bottom left-to-right):
  Row 0: Front_f0, Back_f0, Left_f0, Right_f0, Front_f1
  Row 1: Back_f1, Left_f1, Right_f1, Front_idle, Back_idle
  Row 2: Left_idle, Right_idle, Battle_Ready0, Battle_Atk0, Battle_Gun0
  Row 3: Battle_Hurt0, Battle_Victory0, Battle_Ready1, Battle_Atk1, Battle_Gun1
  Row 4: Battle_Hurt1, Battle_Victory1, (empty), (empty), (empty)

Target layout (4x6 grid):
  Row 0: Front_f0, Front_f1, Back_f0, Back_f1
  Row 1: Left_f0, Left_f1, Right_f0, Right_f1
  Row 2: Front_idle, Back_idle, Left_idle, Right_idle
  Row 3: Battle_Ready0, Battle_Atk0, Battle_Gun0, Battle_Hurt0
  Row 4: Battle_Victory0, Battle_Ready1, Battle_Atk1, Battle_Gun1
  Row 5: Battle_Hurt1, Battle_Victory1, (empty), (empty)
"""

from PIL import Image

def main():
    src_path = '/home/kraken/Devel/game_engine/assets/textures/dean_sprites.png'
    out_path = '/home/kraken/Devel/game_engine/assets/textures/dean_sprites.png'

    img = Image.open(src_path).convert('RGBA')
    W, H = img.size
    print(f"Source: {W}x{H}")

    # Cell size in source
    cell_w, cell_h = 158, 210
    src_cols = W // cell_w  # 5
    print(f"Source grid: {src_cols}x{H // cell_h}")

    def get_cell(col, row):
        x = col * cell_w
        y = row * cell_h
        return img.crop((x, y, x + cell_w, y + cell_h))

    # Extract all cells from current layout
    # Current order in source (col, row):
    sprites = {
        'front_f0':     get_cell(0, 0),
        'back_f0':      get_cell(1, 0),
        'left_f0':      get_cell(2, 0),
        'right_f0':     get_cell(3, 0),
        'front_f1':     get_cell(4, 0),
        'back_f1':      get_cell(0, 1),
        'left_f1':      get_cell(1, 1),
        'right_f1':     get_cell(2, 1),
        'front_idle':   get_cell(3, 1),
        'back_idle':    get_cell(4, 1),
        'left_idle':    get_cell(0, 2),
        'right_idle':   get_cell(1, 2),
        'battle_ready0': get_cell(2, 2),
        'battle_atk0':   get_cell(3, 2),
        'battle_gun0':   get_cell(4, 2),
        'battle_hurt0':  get_cell(0, 3),
        'battle_vic0':   get_cell(1, 3),
        'battle_ready1': get_cell(2, 3),
        'battle_atk1':   get_cell(3, 3),
        'battle_gun1':   get_cell(4, 3),
        'battle_hurt1':  get_cell(0, 4),
        'battle_vic1':   get_cell(1, 4),
    }

    # Target layout (4 columns)
    target_layout = [
        # Row 0: Walk Front
        ['front_f0', 'front_f1', 'back_f0', 'back_f1'],
        # Row 1: Walk Left/Right
        ['left_f0', 'left_f1', 'right_f0', 'right_f1'],
        # Row 2: Idle
        ['front_idle', 'back_idle', 'left_idle', 'right_idle'],
        # Row 3: Battle row 1
        ['battle_ready0', 'battle_atk0', 'battle_gun0', 'battle_hurt0'],
        # Row 4: Battle row 2
        ['battle_vic0', 'battle_ready1', 'battle_atk1', 'battle_gun1'],
        # Row 5: Battle row 3
        ['battle_hurt1', 'battle_vic1', None, None],
    ]

    out_cols = 4
    out_rows = len(target_layout)
    out_w = out_cols * cell_w
    out_h = out_rows * cell_h

    out = Image.new('RGBA', (out_w, out_h), (0, 0, 0, 0))

    for row_idx, row in enumerate(target_layout):
        for col_idx, name in enumerate(row):
            if name is None:
                continue
            sprite = sprites[name]
            x = col_idx * cell_w
            y = row_idx * cell_h
            out.paste(sprite, (x, y))
            print(f"  ({col_idx},{row_idx}): {name}")

    out.save(out_path)
    print(f"\nSaved: {out_path} ({out_w}x{out_h})")
    print(f"Grid: {out_cols}x{out_rows} cells, {cell_w}x{cell_h}px each")


if __name__ == '__main__':
    main()
