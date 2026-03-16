# Desert Map Script
# Auto-generated biome level

proc desert_init():
    log("=== Desert loaded ===")

    # Trees and vegetation
    place_object(736, 384, "Palm Tree")
    place_object(96, 288, "Palm Tree")
    place_object(544, 192, "Palm Tree")
    place_object(832, 160, "Cactus Short")
    place_object(384, 480, "Cactus Tall")
    place_object(576, 224, "Cactus Short")
    place_object(192, 384, "Cactus Tall")
    place_object(224, 320, "Palm Tree")

    # Props and details
    place_object(640, 512, "Barrel")
    place_object(672, 416, "Skull")
    place_object(640, 512, "Skull")
    place_object(800, 224, "Barrel")

    # Atmosphere
    set_clear_color(0.15, 0.12, 0.08)
    set_clouds(true, 0.15, 8, 40)
    set_wind(0.25, 45)

    log("Desert ready")

proc desert_enter():
    set_clear_color(0.15, 0.12, 0.08)
    set_clouds(true, 0.15, 8, 40)
    set_wind(0.25, 45)
