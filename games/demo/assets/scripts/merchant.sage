# Merchant NPC

proc greeting():
    say("Merchant", "Welcome to my shop!")
    say("Merchant", "I have potions and scrolls for sale.")

proc buy():
    say("Merchant", "Here, take these supplies.")
    add_item("potion", "Potion", 3, "consumable", "Heals 30 HP", 30, 0, "", "use_potion")
    add_item("fire_scroll", "Fire Scroll", 2, "weapon", "Deals fire damage", 0, 25, "fire", "use_fire_scroll")
    say("Merchant", "Good luck out there!")
