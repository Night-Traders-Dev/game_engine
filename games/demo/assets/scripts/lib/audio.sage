# ═══════════════════════════════════════
# Audio Library — Context-based music & sound events
# ═══════════════════════════════════════
# Usage: import audio
#
# Provides event-driven audio management:
# - Day/night ambient music transitions
# - Battle music with auto-crossfade
# - NPC interaction sounds
# - Footstep/movement sounds
# - UI feedback sounds

# ── Music Tracks (configure paths here) ──
let overworld_day = "assets/audio/overworld.mp3"
let overworld_night = "assets/audio/overworld.mp3"
let battle_music = "assets/audio/battle.wav"
let shop_music = "assets/audio/overworld.mp3"
let victory_music = ""
let defeat_music = ""

# ── SFX Paths (configure here) ──
let sfx_confirm = "assets/audio/confirm.wav"
let sfx_cancel = "assets/audio/cancel.wav"
let sfx_hit = "assets/audio/hit.wav"
let sfx_heal = "assets/audio/heal.wav"
let sfx_pickup = "assets/audio/pickup.wav"
let sfx_step = "assets/audio/step.wav"

# ── Audio State ──
let current_context = "overworld"

proc init_audio():
    set_music_volume(0.5)
    set_master_volume(0.8)
    log("Audio: initialized")

proc set_context(context):
    # Switch music based on game context
    # Contexts: "overworld", "battle", "shop", "night", "victory", "defeat"
    if context == current_context:
        return
    current_context = context

    if context == "overworld":
        if is_day():
            crossfade_music(overworld_day, 1.5, true)
        else:
            crossfade_music(overworld_night, 1.5, true)
    if context == "battle":
        crossfade_music(battle_music, 0.5, true)
    if context == "shop":
        crossfade_music(shop_music, 1.0, true)
    if context == "night":
        crossfade_music(overworld_night, 2.0, true)
    if context == "victory":
        if victory_music != "":
            play_music(victory_music, false)
    if context == "defeat":
        if defeat_music != "":
            play_music(defeat_music, false)
    log("Audio: context -> " + context)

proc update_ambient():
    # Call each frame to handle day/night music transitions
    if current_context == "overworld" or current_context == "night":
        if is_day() and current_context == "night":
            set_context("overworld")
        if is_night() and current_context == "overworld":
            set_context("night")

# ── Sound Effect Helpers ──
proc sfx_confirm_sound():
    play_sfx(sfx_confirm, 0.6)

proc sfx_cancel_sound():
    play_sfx(sfx_cancel, 0.5)

proc sfx_hit_sound():
    play_sfx(sfx_hit, 0.8)

proc sfx_heal_sound():
    play_sfx(sfx_heal, 0.7)

proc sfx_pickup_sound():
    play_sfx(sfx_pickup, 0.6)

proc sfx_step_sound():
    play_sfx(sfx_step, 0.3)

# ── Battle Audio Routines ──
proc on_battle_start():
    set_context("battle")

proc on_battle_victory():
    set_context("victory")
    # Return to overworld after 3 seconds (would need a timer callback)
    set_context("overworld")

proc on_battle_defeat():
    set_context("defeat")

proc on_shop_open():
    set_context("shop")

proc on_shop_close():
    set_context("overworld")
