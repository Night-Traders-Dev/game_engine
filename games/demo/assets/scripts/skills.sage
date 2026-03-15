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

proc init_dean_skills():
    set_skill("dean", "riflery", 8)
    set_skill("dean", "hardiness", 7)
    set_skill("dean", "nerve", 5)
    set_skill("dean", "tactics", 3)
    set_skill("dean", "unholiness", 2)
    set_skill("dean", "exorcism", 4)
    log("Warrior stats: STR 8  VIT 7  SPD 5  SPI 3  MAG 2  LCK 4")

# ── Black Mage (party member) ──
# Classic FF Black Mage: high MAG/SPI, low STR/VIT

proc init_sam_skills():
    set_skill("sam", "riflery", 3)
    set_skill("sam", "hardiness", 4)
    set_skill("sam", "nerve", 6)
    set_skill("sam", "tactics", 7)
    set_skill("sam", "unholiness", 8)
    set_skill("sam", "exorcism", 5)
    log("Black Mage stats: STR 3  VIT 4  SPD 6  SPI 7  MAG 8  LCK 5")
