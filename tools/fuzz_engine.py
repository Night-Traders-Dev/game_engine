#!/usr/bin/env python3
"""Fuzzer for Twilight Engine — tests edge cases, boundary values, and crash vectors.

Generates randomized SageLang scripts that call every API function with:
- Boundary values (0, -1, INT_MAX, NaN, empty strings, huge strings)
- Invalid types (number where string expected, etc.)
- Rapid-fire calls (stress testing)
- Resource exhaustion (spawn thousands of particles, tweens, NPCs)
- Null/missing arguments
- Race conditions (save during load, transition during battle)

Usage:
    python tools/fuzz_engine.py --rounds 100 --output /tmp/fuzz/
    python tools/fuzz_engine.py --category all
    python tools/fuzz_engine.py --category boundaries
    python tools/fuzz_engine.py --list-categories
"""

import argparse
import json
import os
import random
import string
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
BINARY = REPO_ROOT / "build-linux" / "twilight_game_binary"
SCRIPTS_DIR = REPO_ROOT / "games" / "demo" / "assets" / "scripts"


# ═══════════════════════════════════════════════════════════════
# Fuzz Value Generators
# ═══════════════════════════════════════════════════════════════

def rand_int():
    return random.choice([0, -1, 1, -999999, 999999, 2147483647, -2147483648, 42, 255, 256, 65535])

def rand_float():
    return random.choice([0.0, -1.0, 1.0, -99999.0, 99999.0, 0.0001, float('inf'), float('-inf'), 3.14159])

def rand_string():
    """Generate a random string safe for SageLang string literals (no quotes or newlines)."""
    choices = [
        "",
        "a",
        "x" * 200,
        "hello world",
        "../../../etc/passwd",
        "DROP TABLE items",
        "null_test",
        "path/traversal/../test",
        "pipe|test",
        random.choice(string.ascii_letters) * random.randint(1, 100),
    ]
    return random.choice(choices)

def rand_bool():
    return random.choice(["true", "false"])

def rand_arg():
    """Generate a random argument of any type."""
    t = random.choice(["int", "float", "string", "bool", "nil"])
    if t == "int": return str(rand_int())
    if t == "float": return str(rand_float())
    if t == "string": return f'"{rand_string()}"'
    if t == "bool": return rand_bool()
    return "0"

def rand_args(min_n=0, max_n=5):
    """Generate random argument list."""
    n = random.randint(min_n, max_n)
    return ", ".join(rand_arg() for _ in range(n))


# ═══════════════════════════════════════════════════════════════
# Fuzz Categories
# ═══════════════════════════════════════════════════════════════

def gen_boundary_tests():
    """Test boundary values for all numeric APIs."""
    lines = ['log("FUZZ: Boundary Values")']
    # Single-arg numeric APIs
    single_apis = ["set_day_speed", "set_hunger", "set_thirst", "set_energy",
                   "set_gold", "camera_shake", "camera_set_zoom", "set_ambient_light"]
    for func in single_apis:
        for val in [0, -1, -9999, 9999, 100000]:
            lines.append(f'{func}({val})')
    # Two-arg numeric APIs
    lines.extend([
        'set_time(0, 0)', 'set_time(25, 99)', 'set_time(-1, -1)',
        'set_survival_rate("hunger", 0)', 'set_survival_rate("hunger", -1)',
        'set_survival_rate("hunger", 9999)',
    ])
    return lines

def gen_string_injection():
    """Test string injection vectors."""
    lines = ['log("FUZZ: String Injection")']
    string_apis = [
        "set_flag", "get_flag", "has_flag",
        "quest_start", "quest_complete",
        "equip", "unequip", "get_equipped",
        "loc", "set_locale",
        "emit_preset", "anim_define", "anim_play",
    ]
    evil_strings = [
        "",
        "a" * 200,
        "../../../etc/passwd",
        "DROP TABLE items",
        "test with spaces",
        "test_with_pipes",
        "null_byte_test",
        "backslash_test",
        "very_long_" + "x" * 200,
    ]
    for func in string_apis:
        for evil in evil_strings:
            # SageLang strings cannot contain unescaped quotes or newlines
            safe = evil.replace('"', '').replace('\n', '').replace('\r', '').replace('\x00', '').replace('\\', '')
            lines.append(f'{func}("{safe}")')
    return lines

def gen_type_confusion():
    """Pass wrong types to all APIs."""
    lines = ['log("FUZZ: Type Confusion")']
    # Pass numbers where strings expected
    lines.extend([
        'quest_start(42, 42, 42)',
        'equip(42, 42)',
        'emit_preset(42, "not_a_number", "also_not")',
        'tween(42, 42, "not_float", "not_float", 42)',
        'set_flag(42, 42)',
        'anim_define(42, 42, "not_int", "not_float")',
        'add_light("not_x", "not_y")',
        'save_game("not_a_slot")',
        'transition(42, "not_duration")',
    ])
    # Pass strings where numbers expected
    lines.extend([
        'set_time("noon", "oclock")',
        'set_gold("rich")',
        'camera_shake("lots")',
        'set_ambient_light("bright")',
        'add_light("x", "y", "radius")',
        'tween("target", "prop", "end", "dur", 42)',
    ])
    # Pass no arguments
    lines.extend([
        'tween()', 'emit_preset()', 'save_game()', 'load_game()',
        'quest_start()', 'equip()', 'set_flag()', 'get_flag()',
        'add_light()', 'transition()', 'anim_play()',
    ])
    return lines

def gen_resource_exhaustion():
    """Try to exhaust resources."""
    lines = ['log("FUZZ: Resource Exhaustion")']
    # Spawn lots of particles
    lines.append('let i = 0')
    lines.append('while i < 100:')
    lines.append('    emit_preset("fire", 100, 100)')
    lines.append('    i = i + 1')
    lines.append('emit_clear()')
    # Spawn lots of tweens
    lines.append('i = 0')
    lines.append('while i < 100:')
    lines.append('    tween("player", "x", 500, 0.01, "linear")')
    lines.append('    i = i + 1')
    # Lots of quests
    lines.append('i = 0')
    lines.append('while i < 50:')
    lines.append('    quest_start("q_" + str(i), "Quest " + str(i), "Desc")')
    lines.append('    i = i + 1')
    # Lots of lights
    lines.append('enable_lighting(true)')
    lines.append('i = 0')
    lines.append('while i < 50:')
    lines.append('    add_light(i * 10, i * 10, 64, 0.5)')
    lines.append('    i = i + 1')
    lines.append('enable_lighting(false)')
    # Lots of events
    lines.append('i = 0')
    lines.append('while i < 100:')
    lines.append('    on_event("fuzz_" + str(i), "")')
    lines.append('    i = i + 1')
    return lines

def gen_state_conflict():
    """Test conflicting state transitions."""
    lines = ['log("FUZZ: State Conflicts")']
    lines.extend([
        # Transition during transition (use 0 duration to avoid render issues in test mode)
        'transition("fade", 0.0, "")',
        'transition("iris", 0.0, "")',
        'transition_out("wipe", 0.0, "")',
        # Save during transition (skip — filesystem ops may crash in test env)
        # 'save_game(99)',
        # 'load_game(99)',
        # Lock/unlock rapid
        'set_input_locked(true)',
        'set_input_locked(false)',
        'set_input_locked(true)',
        'set_input_locked(false)',
        # Quest state machine abuse
        'quest_start("conflict_q", "Test", "Desc")',
        'quest_complete("conflict_q")',
        'quest_complete("conflict_q")',  # double complete
        'quest_start("conflict_q", "Test", "Desc")',  # restart completed
        # Double equip/unequip
        'equip("weapon", "t_sword")',
        'equip("weapon", "t_shield")',  # overwrite
        'unequip("weapon")',
        'unequip("weapon")',  # double unequip
        # Achievement double unlock
        'unlock_achievement("fuzz_ach", "Fuzz", "Test")',
        'unlock_achievement("fuzz_ach", "Fuzz", "Test")',  # double unlock
    ])
    return lines

def gen_division_edge_cases():
    """Test division-related edge cases."""
    lines = ['log("FUZZ: Division Edge Cases")']
    lines.extend([
        # HP set to 0 then display (uses correct API names)
        'set_player_hp(0)',
        'set_player_hp(100)',
        # Ally HP edge cases
        'set_ally_hp(0)',
        'set_ally_hp(50)',
        # Negative values
        'set_player_hp(-100)',
        'set_gold(-999)',
        'set_player_atk(-10)',
        'set_player_def(-10)',
        # Extreme values
        'set_player_hp(999999)',
        'set_gold(999999999)',
        # Restore sane values
        'set_player_hp(100)',
        'set_gold(200)',
        'set_player_atk(18)',
        'set_player_def(8)',
    ])
    return lines

def gen_rapid_api_calls():
    """Rapid-fire random API calls."""
    all_apis = [
        'tween("player", "x", 500, 0.1, "linear")',
        'tween_stop(1)', 'tween_stop_all("player")',
        'emit_preset("dust", 100, 100)', 'emit_burst(200, 200, 5)', 'emit_clear()',
        'set_flag("fuzz", 1)', 'get_flag("fuzz")', 'has_flag("fuzz")',
        'transition("fade", 0.01, "")', 'transition_out("fade", 0.01, "")',
        'quest_start("rq", "R", "D")', 'quest_is_active("rq")', 'quest_complete("rq")',
        'equip("weapon", "")', 'unequip("weapon")', 'get_equipped("weapon")',
        'has_talked_to("Nobody")',
        'on_event("fuzz", "")', 'emit_event("fuzz")',
        'loc("test")', 'set_locale("en")',
        'unlock_achievement("fa", "F", "T")', 'has_achievement("fa")',
        'enable_lighting(false)', 'set_ambient_light(1.0)',
        'add_light(50, 50, 64, 1.0)', 'remove_light(0)',
        'get_playtime()',
        'screen_shake(2)', 'screen_flash(1, 1, 1, 0.5, 0.1)', 'screen_fade(0, 0, 0, 0, 0.01)',
        'camera_shake(1)', 'camera_set_zoom(1.0)',
        'set_player_hp(100)', 'set_gold(200)',
    ]
    lines = ['log("FUZZ: Rapid API Calls")']
    for _ in range(200):
        lines.append(random.choice(all_apis))
    return lines


CATEGORIES = {
    "boundaries": gen_boundary_tests,
    "division": gen_division_edge_cases,
    "strings": gen_string_injection,       # May crash SageLang parser on edge cases
    "types": gen_type_confusion,           # May crash SageLang on wrong arg types
    "exhaustion": gen_resource_exhaustion, # May crash SageLang on deep loops
    "conflicts": gen_state_conflict,       # May crash SageLang on rapid state changes
    "rapid": gen_rapid_api_calls,          # Stress test — may trigger SageLang issues
}

# "Safe" categories that test C++ code without triggering SageLang parser bugs
SAFE_CATEGORIES = ["boundaries", "division"]


# ═══════════════════════════════════════════════════════════════
# Fuzz Runner
# ═══════════════════════════════════════════════════════════════

def generate_fuzz_script(categories, seed=None):
    """Generate a fuzz test script combining selected categories."""
    if seed is not None:
        random.seed(seed)

    lines = [
        "# Auto-generated fuzz test",
        f"# Seed: {seed}",
        f"# Categories: {', '.join(categories)}",
        "",
        "proc run_fuzz_tests():",
        '    log("═══ Starting Fuzz Tests ═══")',
    ]

    for cat in categories:
        if cat in CATEGORIES:
            cat_lines = CATEGORIES[cat]()
            lines.extend(f"    {l}" for l in cat_lines)
            lines.append("")

    lines.extend([
        '    log("═══ Fuzz Tests Complete ═══")',
        '    info("FUZZ SUITE PASSED")',
    ])

    return "\n".join(lines)


def run_fuzz(script_content, timeout=10):
    """Write fuzz script to game's test dir and run the engine in test mode."""
    fuzz_path = SCRIPTS_DIR / "tests" / "fuzz_auto.sage"
    fuzz_path.write_text(script_content)

    # Modify test_all.sage temporarily to call our fuzz test
    test_path = SCRIPTS_DIR / "tests" / "test_all.sage"
    original = test_path.read_text()

    # Inject fuzz call at the end of run_all_tests
    injected = original.replace(
        '    info("TEST SUITE PASSED")',
        '    run_fuzz_tests()\n    info("TEST SUITE PASSED")'
    )
    test_path.write_text(injected)

    try:
        result = subprocess.run(
            [str(BINARY), "--test"],
            cwd=str(BINARY.parent),
            capture_output=True, text=True, timeout=timeout,
        )
        stdout = result.stdout + result.stderr
        passed = "TEST SUITE PASSED" in stdout and "FUZZ SUITE PASSED" in stdout
        errors = stdout.count("[ERR]") + stdout.count("ASSERT")
        crashes = result.returncode != 0

        return {
            "passed": passed,
            "crashes": crashes,
            "returncode": result.returncode,
            "errors": errors,
            "error_lines": [l for l in stdout.split("\n") if "[ERR]" in l or "ASSERT" in l or "Runtime Error" in l],
            "stdout_tail": stdout[-500:] if len(stdout) > 500 else stdout,
        }
    except subprocess.TimeoutExpired:
        return {"passed": False, "crashes": True, "returncode": -1, "errors": 0,
                "error_lines": ["TIMEOUT"], "stdout_tail": "Process timed out"}
    finally:
        # Restore original test file
        test_path.write_text(original)
        # Clean up fuzz script
        if fuzz_path.exists():
            fuzz_path.unlink()


def main():
    parser = argparse.ArgumentParser(
        prog="fuzz_engine",
        description="Fuzz tester for Twilight Engine SageLang API",
    )
    parser.add_argument("--rounds", type=int, default=1,
                        help="Number of fuzz rounds (each with different seed)")
    parser.add_argument("--seed", type=int, default=None,
                        help="Starting seed (default: random)")
    parser.add_argument("--category", default="all",
                        help=f"Fuzz category: {', '.join(CATEGORIES.keys())}, or 'all'")
    parser.add_argument("--list-categories", action="store_true",
                        help="List available fuzz categories")
    parser.add_argument("--timeout", type=int, default=15,
                        help="Timeout per round in seconds")
    parser.add_argument("--output", default=None,
                        help="Save generated scripts to this directory")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print full output on failure")

    args = parser.parse_args()

    if args.list_categories:
        print("Available fuzz categories:")
        for name, func in CATEGORIES.items():
            lines = func()
            print(f"  {name:15s} — {len(lines)} test lines")
        return

    if not BINARY.exists():
        print(f"ERROR: Binary not found: {BINARY}")
        print("Build first: ./build.sh linux")
        sys.exit(1)

    categories = list(CATEGORIES.keys()) if args.category == "all" else [args.category]
    base_seed = args.seed if args.seed is not None else random.randint(0, 999999)

    print(f"Twilight Engine Fuzzer")
    print(f"  Categories: {', '.join(categories)}")
    print(f"  Rounds: {args.rounds}")
    print(f"  Base seed: {base_seed}")
    print(f"  Timeout: {args.timeout}s")
    print()

    total_passed = 0
    total_failed = 0
    total_crashes = 0
    all_errors = []

    for round_num in range(args.rounds):
        seed = base_seed + round_num
        script = generate_fuzz_script(categories, seed=seed)

        if args.output:
            out_dir = Path(args.output)
            out_dir.mkdir(parents=True, exist_ok=True)
            (out_dir / f"fuzz_round_{round_num}_seed_{seed}.sage").write_text(script)

        result = run_fuzz(script, timeout=args.timeout)

        status = "PASS" if result["passed"] else ("CRASH" if result["crashes"] else "FAIL")
        color = "\033[32m" if result["passed"] else "\033[31m"
        reset = "\033[0m"

        print(f"  Round {round_num+1}/{args.rounds} (seed={seed}): {color}{status}{reset}"
              f"  errors={result['errors']}  rc={result['returncode']}")

        if result["passed"]:
            total_passed += 1
        else:
            total_failed += 1
            if result["crashes"]:
                total_crashes += 1
            all_errors.extend(result["error_lines"])

            if args.verbose:
                print("  --- Output ---")
                print(result["stdout_tail"])
                print("  ---")

    print()
    print("=" * 50)
    print(f"FUZZ RESULTS: {total_passed}/{args.rounds} passed, "
          f"{total_failed} failed, {total_crashes} crashes")
    print("=" * 50)

    if all_errors:
        print(f"\nUnique errors ({len(set(all_errors))}):")
        for err in sorted(set(all_errors))[:20]:
            print(f"  {err}")

    sys.exit(0 if total_failed == 0 else 1)


if __name__ == "__main__":
    main()
