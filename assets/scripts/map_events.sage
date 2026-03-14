# Map Events - Called by the engine for area triggers

proc on_enter_graveyard():
    log("Player entered graveyard area")
    if get_flag("cemetery_hint"):
        say("Dean", "This must be the place the stranger mentioned.")
        say("Sam", "Stay sharp.")

proc on_enter_gas_mart():
    log("Player near Gas Mart")
    if not get_flag("has_shotgun"):
        say("Dean", "We should talk to Bobby about supplies.")

proc on_pond_approach():
    if not get_flag("vampire_defeated"):
        log("Vampire encounter zone")
        say("Sam", "Dean, I've got a bad feeling about this place.")
        say("Dean", "When don't you?")
