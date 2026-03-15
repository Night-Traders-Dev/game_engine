# ═══════════════════════════════════════════════
# Twilight Engine — Debug API
# ═══════════════════════════════════════════════
#
# Available debug functions (registered from C++):
#   debug(msg)       — Debug level log (grey in console)
#   info(msg)        — Info level log (green)
#   warn(msg)        — Warning level log (yellow)
#   error(msg)       — Error level log (red)
#   print(a, b, ...) — Print multiple values (cyan, script level)
#   assert_true(cond, msg) — Assert condition, log error if false
#
# Usage in your .sage files:
#   debug("Player position: " + str(player_x))
#   warn("Enemy HP is very low")
#   print("Damage dealt:", damage, "to", target_name)
#   assert_true(hp > 0, "HP should never be negative")

# ── Test functions ──

proc debug_test():
    debug("This is a debug message")
    info("This is an info message")
    warn("This is a warning message")
    error("This is an error message")
    print("Multi-arg print:", 42, true, "hello")
    assert_true(true, "This should pass")
    assert_true(false, "This assertion SHOULD fail (test)")
    log("Debug API test complete")

proc dump_battle_state():
    print("=== Battle State ===")
    print("Enemy:", enemy_name, "HP:", enemy_hp, "/", enemy_max_hp, "ATK:", enemy_atk)
    print("Warrior HP:", dean_hp, "/", dean_max_hp, "ATK:", dean_atk, "DEF:", dean_def)
    print("Black Mage HP:", sam_hp, "/", sam_max_hp, "ATK:", sam_atk)
    print("Active fighter:", active_fighter)
    print("====================")
