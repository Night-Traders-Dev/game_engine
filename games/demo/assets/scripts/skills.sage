# ═══════════════════════════════════════════════
# Demo — H.U.N.T.E.R. Skills Init
# ═══════════════════════════════════════════════

proc init_dean_skills():
    set_skill("dean", "hardiness", 5)
    set_skill("dean", "unholiness", 2)
    set_skill("dean", "nerve", 6)
    set_skill("dean", "tactics", 5)
    set_skill("dean", "exorcism", 3)
    set_skill("dean", "riflery", 6)
    log("Hero skills initialized")

proc init_sam_skills():
    set_skill("sam", "hardiness", 4)
    set_skill("sam", "unholiness", 4)
    set_skill("sam", "nerve", 4)
    set_skill("sam", "tactics", 6)
    set_skill("sam", "exorcism", 6)
    set_skill("sam", "riflery", 3)
    log("Mage skills initialized")
