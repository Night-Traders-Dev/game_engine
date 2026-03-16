# Volcanic Map Script
# Auto-generated biome level

proc volcanic_init():
    log("=== Volcanic loaded ===")

    # Trees and vegetation
    place_object(352, 512, "Charred Tree")
    place_object(704, 288, "Charred Tree")
    place_object(384, 576, "Charred Tree")
    place_object(736, 416, "Charred Tree")
    place_object(608, 160, "Charred Tree")
    place_object(768, 320, "Charred Tree")
    place_object(320, 320, "Charred Tree")
    place_object(704, 512, "Charred Tree")

    # Props and details
    place_object(128, 544, "Fire Pit")
    place_object(640, 320, "Lava Rock")
    place_object(608, 192, "Lava Rock")
    place_object(704, 352, "Obsidian Shard")

    # Atmosphere
    set_clear_color(0.08, 0.02, 0.01)
    set_clouds(true, 0.6, 10, 30)
    set_wind(0.1, 15)

    log("Volcanic ready")

proc volcanic_enter():
    set_clear_color(0.08, 0.02, 0.01)
    set_clouds(true, 0.6, 10, 30)
    set_wind(0.1, 15)
