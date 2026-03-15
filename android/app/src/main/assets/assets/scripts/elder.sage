# Village Elder — Classic FF NPC
# Functions named: elder_greeting, elder_quest, etc.

proc elder_greeting():
    say("Elder", "Welcome, brave adventurers.")
    say("Elder", "Monsters have been emerging from the Crystal Cave to the north.")
    say("Elder", "The darkness grows stronger each day...")
    say("Elder", "Please, you must restore the Crystal's light.")

proc elder_quest():
    say("Elder", "The Wind Crystal lies deep within the cave.")
    say("Elder", "Beware the undead that guard its chambers.")
    say("Elder", "Take these supplies for your journey.")
    add_item("potion", "Potion", 5, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_item("phoenix_down", "Phoenix Down", 1, "consumable", "Life to fallen ally", 0, 0, "", "use_phoenix_down")
    say("Elder", "May the light of the Crystals guide you.")
