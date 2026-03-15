# Patrol routes — auto-assigned to spawned NPCs via spawn callback
#
# IMPORTANT: All patrol routes are CLOSED LOOPS — the last waypoint
# connects back to the first with a short walk, not a teleport across the map.
# For a square patrol: define waypoints clockwise/counterclockwise around the perimeter.

proc init_patrols():
    set_spawn_callback("Skeleton", "on_skeleton_spawn")

    # Original Skeleton: tight clockwise loop in northeast
    npc_add_waypoint("Skeleton", 600, 320)
    npc_add_waypoint("Skeleton", 680, 320)
    npc_add_waypoint("Skeleton", 680, 420)
    npc_add_waypoint("Skeleton", 600, 420)
    npc_set_route("Skeleton", "patrol")
    npc_start_route("Skeleton")
    log("Patrols: base Skeleton + spawn callback registered")

proc on_skeleton_spawn():
    let name = spawned_npc
    let idx = spawned_index
    log("Patrol assigned to: " + name)

    npc_set_despawn_day(name, true)

    # Each route is a closed loop — waypoints form a circuit
    # so the NPC walks smoothly from the last point back to the first.

    if idx == 0:
        # Northwest — small clockwise square
        npc_add_waypoint(name, 300, 220)
        npc_add_waypoint(name, 400, 220)
        npc_add_waypoint(name, 400, 320)
        npc_add_waypoint(name, 300, 320)
        npc_set_route(name, "patrol")

    if idx == 1:
        # Southeast — clockwise rectangle
        npc_add_waypoint(name, 550, 450)
        npc_add_waypoint(name, 680, 450)
        npc_add_waypoint(name, 680, 550)
        npc_add_waypoint(name, 550, 550)
        npc_set_route(name, "patrol")

    if idx == 2:
        # Southwest — clockwise triangle loop
        npc_add_waypoint(name, 320, 420)
        npc_add_waypoint(name, 420, 480)
        npc_add_waypoint(name, 320, 540)
        npc_set_route(name, "patrol")

    if idx == 3:
        # Center — clockwise diamond
        npc_add_waypoint(name, 480, 300)
        npc_add_waypoint(name, 560, 380)
        npc_add_waypoint(name, 480, 460)
        npc_add_waypoint(name, 400, 380)
        npc_set_route(name, "patrol")

    if idx == 4:
        # Wide perimeter — clockwise around the map
        npc_add_waypoint(name, 350, 250)
        npc_add_waypoint(name, 700, 250)
        npc_add_waypoint(name, 700, 500)
        npc_add_waypoint(name, 350, 500)
        npc_set_route(name, "patrol")

    npc_start_route(name)
