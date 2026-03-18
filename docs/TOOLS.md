# Tools

| Tool | Description |
|------|-------------|
| `tools/generate_tileset.py` | Procedural pixel-art tileset generator for 10 biome types (grasslands, forest, desert, snow, swamp, volcanic, beach, cave, urban, farmland). Outputs engine-compatible PNG + stamps.txt |
| `tools/generate_parallax_bg.py` | Procedural parallax background generator. 6 biomes (forest, cave, night, sunset, snow, desert), 5 layers each, horizontally tileable, pixel-art style |
| `tools/generate_ui_pack.py` | Procedural UI/HUD sprite sheet generator. 4 themes (fantasy, dark, medieval, cute), 47 components each |
| `tools/wire_biome_levels.py` | Generates tilesets + maps + scripts and wires them into the demo with portals |
| `tools/tw_test/` | Automated test tool: launches game, sends keyboard input via XTest, captures screenshots via X11, runs smoke tests |
| `tools/scale_assets.py` | Multi-resolution asset scaler (2x, 3x, 4x) with nearest-neighbor for pixel art |
| `tools/extract_tileset.py` | Removes background color from tileset PNGs |
| `tools/fuzz_engine.py` | Security fuzzer: 7 fuzz categories (boundary, type, injection, exhaustion, division, string, resource) |
