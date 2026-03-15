# Mysterious Stranger - Dialogue Script

proc greeting():
    say("???", "You shouldn't be poking around here, hunter.")
    say("???", "I can smell it on you... the stench of righteousness.")

proc suspicious():
    say("???", "This town has secrets, Winchester.")
    say("???", "Secrets that are better left buried.")
    say("Dean", "I don't do well with secrets.")

proc reveal():
    say("???", "You want to know what's really going on?")
    say("???", "Fine. But don't say I didn't warn you.")
    say("???", "The cemetery. Midnight. Come alone.")
    say("Dean", "Yeah, that's not happening.")
    set_flag("cemetery_hint", true)
