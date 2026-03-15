# Azazel (Yellow-Eyed Demon) - Dialogue Script

proc greeting():
    say("Azazel", "Well, well... the Winchester boys.")
    say("Azazel", "You have no idea what's coming.")
    say("Azazel", "I've got plans for your brother, Dean.")
    say("Dean", "You son of a bitch.")

proc threat():
    say("Azazel", "You can't stop what's already in motion.")
    say("Azazel", "Your daddy couldn't. And neither can you.")
    say("Sam", "Watch us.")
    say("Azazel", "Oh, I will. With great interest.")

proc final_encounter():
    say("Azazel", "This isn't over, Winchesters.")
    say("Azazel", "Not by a long shot.")
    say("Dean", "We'll be ready.")
    say("Sam", "Count on it.")
    set_flag("azazel_confronted", true)
