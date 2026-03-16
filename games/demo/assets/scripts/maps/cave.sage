# Cave Map Script
# Auto-generated biome level

proc cave_init():
    log("=== Cave loaded ===")

    # Props and details
    place_object(704, 352, "Cobweb")
    place_object(192, 448, "Cobweb")
    place_object(224, 352, "Cobweb")
    place_object(288, 384, "Boulder")

    # Atmosphere
    set_clear_color(0.02, 0.02, 0.03)
    set_clouds(false, 0, 0, 0)
    set_god_rays(false, 0, 0)

    log("Cave ready")

proc cave_enter():
    set_clear_color(0.02, 0.02, 0.03)
    set_clouds(false, 0, 0, 0)
    set_god_rays(false, 0, 0)
