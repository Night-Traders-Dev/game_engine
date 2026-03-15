# Item Shop — Classic FF Merchant
# Functions named: merchant_greeting, merchant_buy, etc.

proc merchant_greeting():
    say("Merchant", "Welcome! What can I get for you?")
    say("Merchant", "I've got Potions, Phoenix Downs, and magic scrolls.")
    say("Merchant", "Stock up before heading into the cave!")

proc merchant_buy():
    say("Merchant", "Here you go! Stay safe out there.")
    add_item("potion", "Potion", 5, "consumable", "Restores 50 HP", 50, 0, "", "use_potion")
    add_item("phoenix_down", "Phoenix Down", 2, "consumable", "Life to fallen ally", 0, 0, "", "use_phoenix_down")
    add_item("fire", "Fire", 3, "weapon", "Fire magic - 30 dmg", 0, 30, "fire", "use_fire")
    add_item("cure", "Cure", 3, "consumable", "White magic - heal 40 HP", 40, 0, "holy", "use_cure")
    say("Merchant", "Come back anytime!")
