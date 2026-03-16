# Snow Map Script
# Auto-generated biome level

proc snow_init():
    log("=== Snow loaded ===")

    # Trees and vegetation
    place_object(96, 256, "Snow Pine")
    place_object(352, 544, "Bare Tree")
    place_object(256, 320, "Frozen Bush")
    place_object(736, 512, "Snow Pine")
    place_object(96, 128, "Snow Pine")
    place_object(544, 352, "Bare Tree")
    place_object(416, 576, "Snow Pine")
    place_object(672, 288, "Bare Tree")

    # Props and details
    place_object(608, 160, "Log Pile")
    place_object(416, 480, "Ice Rock")
    place_object(672, 320, "Log Pile")
    place_object(608, 352, "Log Pile")

    # Atmosphere
    set_clear_color(0.08, 0.08, 0.12)
    set_clouds(true, 0.5, 15, 50)
    set_wind(0.3, 60)

    log("Snow ready")

proc snow_enter():
    set_clear_color(0.08, 0.08, 0.12)
    set_clouds(true, 0.5, 15, 50)
    set_wind(0.3, 60)
