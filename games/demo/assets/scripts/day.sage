set_day_speed(6)           # Full day in 240 seconds
set_time(22, 0)             # Set to 10 PM
if is_night():
    spawn_loop("Skeleton", 30, 3)
