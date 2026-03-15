# ═══════════════════════════════════════════════
# Twilight Engine Demo — Final Fantasy Style Skills
# ═══════════════════════════════════════════════
#
# Stats mapped to H.U.N.T.E.R. engine system:
#   Hardiness  -> Vitality  (HP bonus, physical defense)
#   Unholiness -> Magic     (spell power, MP equivalent)
#   Nerve      -> Speed     (turn order, evasion, crit)
#   Tactics    -> Spirit    (magic defense, healing power)
#   Exorcism   -> Luck      (rare drops, crit, status resist)
#   Riflery    -> Strength  (physical attack power)

# ── Warrior (player character) ──
# Classic FF Warrior: high STR/VIT, low MAG

proc init_player_stats():
    set_skill("player", "strength", 8)
    set_skill("player", "vitality", 7)
    set_skill("player", "agility", 5)
    set_skill("player", "tactics", 3)
    set_skill("player", "arcana", 2)
    set_skill("player", "spirit", 4)
    log("Warrior stats: STR 8  VIT 7  SPD 5  SPI 3  MAG 2  LCK 4")

# ── Black Mage (party member) ──
# Classic FF Black Mage: high MAG/SPI, low STR/VIT

proc init_ally_stats():
    set_skill("ally", "strength", 3)
    set_skill("ally", "vitality", 4)
    set_skill("ally", "agility", 6)
    set_skill("ally", "tactics", 7)
    set_skill("ally", "arcana", 8)
    set_skill("ally", "spirit", 5)
    log("Black Mage stats: STR 3  VIT 4  SPD 6  SPI 7  MAG 8  LCK 5")
