# Bobby Singer - Dialogue Script
# Functions can be called from the engine: call_function("greeting")

proc greeting():
    say("Bobby", "You idjits better be prepared before heading out.")
    say("Bobby", "I've got some supplies if you need 'em.")
    say("Bobby", "Also, watch your back. Something ain't right in this town.")

proc supplies():
    say("Bobby", "Let me see what I got in the truck.")
    say("Bobby", "Here, take this shotgun. Rock salt rounds.")
    say("Bobby", "And some holy water. Never leave home without it.")
    set_flag("has_shotgun", true)
    set_flag("has_holy_water", true)

proc warning():
    say("Bobby", "Listen to me, boy.")
    say("Bobby", "There's something big coming. I can feel it.")
    say("Bobby", "You and Sam need to stick together on this one.")
    say("Dean", "We always do, Bobby.")

proc after_battle():
    say("Bobby", "You boys alright?")
    say("Dean", "Nothing a cold beer won't fix.")
    say("Bobby", "I'll hold you to that. Now get moving.")
    set_flag("bobby_checked_in", true)

proc farewell():
    say("Bobby", "Stay safe out there, ya hear?")
    say("Bobby", "And call me if things go sideways.")
    say("Dean", "Will do, Bobby.")
