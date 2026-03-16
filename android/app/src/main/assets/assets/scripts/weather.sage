# Weather System Examples — Twilight Engine
# This file demonstrates various weather setups and techniques.
# Include it in your game.json scripts list or call functions directly.

# ═══════════════════════════════════════════
# PRESET WEATHER SETUPS
# ═══════════════════════════════════════════

# Quick presets using set_weather()
proc weather_clear():
    set_weather("clear")
    set_clear_color(0.05, 0.08, 0.12)
    log("Weather: Clear skies")

proc weather_light_rain():
    set_weather("rain", 0.3)
    set_wind(0.1, 20)
    set_clear_color(0.04, 0.05, 0.08)
    log("Weather: Light rain")

proc weather_heavy_rain():
    set_weather("rain", 0.9)
    set_wind(0.5, 30)
    set_fog(true, 0.15)
    set_clear_color(0.03, 0.04, 0.06)
    log("Weather: Heavy rain")

proc weather_thunderstorm():
    set_weather("storm", 0.9)
    set_wind(0.7, 45)
    set_fog(true, 0.2, 0.3, 0.3, 0.4)
    set_clear_color(0.02, 0.02, 0.04)
    log("Weather: Thunderstorm!")

proc weather_snow():
    set_weather("snow", 0.5)
    set_wind(0.15, 10)
    set_clear_color(0.08, 0.08, 0.1)
    log("Weather: Snowfall")

proc weather_blizzard():
    set_snow(true, 1.0)
    set_wind(0.8, 60)
    set_fog(true, 0.5, 0.9, 0.9, 0.95)
    set_clouds(true, 0.8, 40, 60)
    set_clear_color(0.06, 0.06, 0.08)
    log("Weather: Blizzard!")

proc weather_foggy_morning():
    set_weather("fog", 0.6)
    set_clouds(true, 0.3, 10, 90)
    set_god_rays(true, 0.25, 3)
    set_clear_color(0.06, 0.07, 0.08)
    log("Weather: Foggy morning")

proc weather_overcast():
    set_weather("cloudy", 0.7)
    set_god_rays(true, 0.15, 6)
    set_clear_color(0.04, 0.05, 0.07)
    log("Weather: Overcast with god rays")

# ═══════════════════════════════════════════
# ADVANCED: Individual control examples
# ═══════════════════════════════════════════

# Blood rain (horror game)
proc weather_blood_rain():
    set_rain(true, 0.6)
    set_rain_color(0.8, 0.1, 0.1, 0.5)
    set_clouds(true, 0.5, 20, 45)
    set_clear_color(0.05, 0.01, 0.01)
    log("Weather: Blood rain...")

# Magical aurora (fantasy)
proc weather_aurora():
    set_clouds(true, 0.3, 5, 90)
    set_god_rays(true, 0.4, 8)
    set_snow(true, 0.1)
    set_clear_color(0.02, 0.02, 0.06)
    log("Weather: Aurora borealis")

# Ash fall (volcanic area)
proc weather_ash():
    set_snow(true, 0.4)
    set_rain_color(0.3, 0.3, 0.3, 0.6)
    set_fog(true, 0.3, 0.4, 0.35, 0.3)
    set_clouds(true, 0.6, 25, 45)
    set_clear_color(0.04, 0.03, 0.02)
    log("Weather: Volcanic ash")

# Sandstorm (desert)
proc weather_sandstorm():
    set_fog(true, 0.6, 0.8, 0.7, 0.5)
    set_wind(0.9, 0)
    set_rain(true, 0.3)
    set_rain_color(0.8, 0.7, 0.4, 0.3)
    set_clouds(true, 0.4, 50, 0)
    set_clear_color(0.1, 0.08, 0.04)
    log("Weather: Sandstorm!")

# ═══════════════════════════════════════════
# DYNAMIC: Time-based weather changes
# ═══════════════════════════════════════════

# Call this from your map_init to set up automatic weather
proc init_dynamic_weather():
    log("Dynamic weather system initialized")
    # Weather changes based on time of day
    # Call check_weather() periodically from your game loop

# Check and apply weather based on time
proc check_weather():
    let h = get_hour()
    if h >= 5 and h < 8:
        # Early morning: foggy with god rays
        if is_raining() == false and is_snowing() == false:
            set_fog(true, 0.4)
            set_god_rays(true, 0.3, 4)
            set_clouds(true, 0.3, 10, 90)
    elif h >= 8 and h < 12:
        # Morning: clearing up
        set_fog(false)
        set_clouds(true, 0.2, 15, 60)
        set_god_rays(true, 0.15, 3)
    elif h >= 12 and h < 16:
        # Afternoon: clear or light clouds
        set_fog(false)
        set_god_rays(false)
        set_clouds(true, 0.15, 20, 45)
    elif h >= 16 and h < 19:
        # Late afternoon: building clouds
        set_clouds(true, 0.4, 20, 45)
        set_god_rays(true, 0.2, 5)
    elif h >= 19 and h < 21:
        # Evening: chance of rain
        set_god_rays(false)
        if random(1, 10) > 7:
            set_rain(true, 0.3)
            set_wind(0.2, 30)
    elif h >= 21 or h < 5:
        # Night: storms possible
        if random(1, 10) > 8:
            set_lightning(true, 6, 0.4)
            set_rain(true, 0.6)
            set_wind(0.4, 45)

# ═══════════════════════════════════════════
# PER-LEVEL WEATHER PRESETS
# ═══════════════════════════════════════════

# Forest level: dappled light through trees
proc weather_forest():
    set_clouds(true, 0.3, 12, 60)
    set_god_rays(true, 0.25, 4)
    set_wind(0.15, 30)
    set_fog(true, 0.1, 0.3, 0.4, 0.3)

# Cave entrance: dripping rain, no sky
proc weather_cave_entrance():
    set_rain(true, 0.2)
    set_fog(true, 0.3, 0.2, 0.2, 0.25)
    set_clouds(false)
    set_god_rays(false)

# Desert level: heat haze
proc weather_desert():
    set_fog(true, 0.15, 0.9, 0.85, 0.7)
    set_wind(0.3, 0)
    set_clouds(true, 0.1, 30, 0)

# Mountain peak: howling wind and snow
proc weather_mountain():
    set_snow(true, 0.7)
    set_wind(0.6, 270)
    set_fog(true, 0.3, 0.85, 0.9, 0.95)
    set_clouds(true, 0.5, 40, 270)

# Haunted area: eerie fog
proc weather_haunted():
    set_fog(true, 0.5, 0.3, 0.35, 0.4)
    set_clouds(true, 0.4, 8, 180)
    set_wind(0.1, 180)
