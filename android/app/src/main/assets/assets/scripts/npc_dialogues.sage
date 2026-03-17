# NPC Dialogues — Crystal Quest Demo
# Convention: {npc_name_lower}_greeting() is called when player interacts

proc woodsman_greeting():
    let talked = get_flag("woodsman_talked")
    if talked == 0:
        say("Woodsman", "Halt there, traveler!")
        say("Woodsman", "These woods aren't safe after dark.")
        say("Woodsman", "Wolves come out when the sun sets...")
        say("Woodsman", "I patrol this area to keep folks safe.")
        say("Woodsman", "If you're headed to the cave, go north through the desert portal.")
        set_flag("woodsman_talked", 1)
    else:
        let r = random(1, 4)
        if r == 1:
            say("Woodsman", "Stay alert. I heard howling last night.")
        if r == 2:
            say("Woodsman", "The merchant has good supplies. Stock up before you leave.")
        if r == 3:
            say("Woodsman", "The Elder knows more about the Crystal than anyone.")
        if r == 4:
            say("Woodsman", "There's a strange cave to the north... full of crystals.")

proc fox_greeting():
    say("Fox", "*yip yip!*")
    let r = random(1, 3)
    if r == 1:
        say("Fox", "*wags tail happily*")
    if r == 2:
        say("Fox", "*sniffs your pockets for food*")
    if r == 3:
        say("Fox", "*does a little spin*")

proc hermit_greeting():
    let talked = get_flag("hermit_talked")
    if talked == 0:
        say("Hermit", "Oh! A visitor! I don't get many of those.")
        say("Hermit", "Welcome to my humble abode.")
        say("Hermit", "I used to be an adventurer myself, you know.")
        say("Hermit", "Then I found this cozy house and... well...")
        say("Hermit", "Here, take this. It might help on your journey.")
        add_item("old_map", "Old Map", 1, "key_item", "A weathered map showing cave passages", 0, 0, "", "")
        set_flag("hermit_talked", 1)
    else:
        let r = random(1, 3)
        if r == 1:
            say("Hermit", "Make yourself at home! Well... not too at home.")
        if r == 2:
            say("Hermit", "The fireplace keeps the monsters away. Mostly.")
        if r == 3:
            say("Hermit", "Did you find the Crystal Cave yet? It's through the desert.")
