# Vampire - Dialogue Script

proc encounter():
    say("Vampire", "The night is young, hunter...")
    say("Vampire", "You reek of dead man's blood. Cute.")

proc taunt():
    say("Vampire", "You think your little tricks will stop me?")
    say("Vampire", "I've been around for centuries, Winchester.")
    say("Dean", "Yeah, well, I've killed older.")

proc pre_battle():
    say("Vampire", "Enough talk.")
    say("Vampire", "Time to feed.")
    start_battle("Vampire", 60, 15)

proc defeat():
    say("Vampire", "No... this can't be...")
    say("Dean", "Believe it, fangs.")
    say("Sam", "That's another one down.")
    set_flag("vampire_defeated", true)
