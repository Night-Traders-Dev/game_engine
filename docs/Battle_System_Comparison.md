# Battle System: C++ vs SageLang

## Overview

The Twilight Engine battle system uses a **hybrid architecture** — the core battle loop runs in C++ for performance and reliability, while individual combat actions are scriptable in SageLang for moddability and rapid iteration. The engine falls back to C++ implementations when no script is available.

---

## Architecture Comparison

```
┌─────────────────────────────────────────────────────────┐
│                   C++ Battle Loop                       │
│  (Phases, input handling, HP rolling, animations, UI)   │
│                                                         │
│  ┌─────────────┐    ┌──────────────────────────────┐    │
│  │  C++ Fallback│◄──│  SageLang Script Available?   │    │
│  │  (built-in)  │ NO│                               │    │
│  └─────────────┘    └──────────┬───────────────────┘    │
│                           YES  │                        │
│                     ┌──────────▼───────────────────┐    │
│                     │  sync_battle_to_script()     │    │
│                     │  call_function("attack")     │    │
│                     │  sync_battle_from_script()   │    │
│                     └──────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

---

## Side-by-Side: Player Attack

### C++ (Fallback)

```cpp
const char* fighter = (b.active_fighter == 0) ? "Dean" : "Sam";
int atk = (b.active_fighter == 0) ? b.player_atk : b.sam_atk;
int damage = atk + (game.rng() % 5) - 2;
if (damage < 1) damage = 1;
b.enemy_hp_actual -= damage;
b.last_damage = damage;
b.message = std::string(fighter) + " attacks! "
          + std::to_string(damage) + " damage!";
```

**Characteristics:**
- Fixed damage formula: `atk + random(0-4) - 2`
- No critical hits
- No item checks
- Requires recompilation to modify
- ~7 lines

### SageLang (Scripted)

```sage
proc attack_normal():
    let fighter_name = "Dean"
    let base_atk = dean_atk
    if active_fighter == 1:
        fighter_name = "Sam"
        base_atk = sam_atk

    let damage = base_atk + random(-2, 2)
    if damage < 1:
        damage = 1

    # Critical hit chance (10%)
    let crit = random(1, 10)
    if crit == 10:
        damage = damage * 2
        battle_msg = fighter_name + " lands a CRITICAL HIT! " + str(damage) + " damage!"
    else:
        battle_msg = fighter_name + " attacks! " + str(damage) + " damage!"

    enemy_hp = enemy_hp - damage
    battle_damage = damage
    battle_target = "enemy"
```

**Characteristics:**
- Same base formula but with 10% critical hit (2x damage)
- Dynamic message based on crit
- Modifiable without recompilation
- Hot-reloadable (restart game to pick up changes)
- ~20 lines

---

## Side-by-Side: Enemy Turn

### C++ (Fallback)

```cpp
int target = (game.rng() % 2 == 0 && b.sam_hp_actual > 0) ? 1 : 0;
if (b.player_hp_actual <= 0) target = 1;
if (b.sam_hp_actual <= 0) target = 0;

int def = (target == 0) ? b.player_def : 2;
int damage = b.enemy_atk + (game.rng() % 5) - 2 - def / 3;
if (damage < 1) damage = 1;

if (target == 0) {
    b.player_hp_actual -= damage;
    b.message = b.enemy_name + " attacks Dean! " + std::to_string(damage) + " damage!";
} else {
    b.sam_hp_actual -= damage;
    b.message = b.enemy_name + " attacks Sam! " + std::to_string(damage) + " damage!";
}
```

**Characteristics:**
- Random target selection
- Basic defense calculation
- All enemies behave identically
- No special attacks

### SageLang — Generic Enemy

```sage
proc enemy_turn():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    let target_def = dean_def
    if target == 1:
        target_name = "Sam"
        target_def = 2

    let damage = enemy_atk + random(-2, 2) - target_def / 3
    if damage < 1:
        damage = 1

    # Special attack chance (20%)
    let special = random(1, 5)
    if special == 5:
        damage = damage + random(5, 10)
        battle_msg = enemy_name + " unleashes a powerful attack on "
                   + target_name + "! " + str(damage) + " damage!"
    else:
        battle_msg = enemy_name + " attacks " + target_name
                   + "! " + str(damage) + " damage!"

    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    battle_damage = damage
    battle_target = target_name
```

**Characteristics:**
- 20% chance of special attack (+5-10 bonus damage)
- Custom message for special attacks
- Same basic target selection logic

### SageLang — Vampire (Enemy-Specific)

```sage
proc vampire_attack():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    let damage = enemy_atk + random(0, 5)
    let drain = damage / 3

    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    # Vampire heals from the drain
    enemy_hp = enemy_hp + drain
    if enemy_hp > enemy_max_hp:
        enemy_hp = enemy_max_hp

    battle_msg = enemy_name + " bites " + target_name
               + "! " + str(damage) + " damage! Drains "
               + str(drain) + " HP!"
    battle_damage = damage
    battle_target = target_name
```

**Characteristics:**
- Unique mechanic: drains HP (damages target AND heals self)
- Cannot be replicated in C++ fallback without hardcoding
- Easy to create new enemy types with unique behaviors

---

## Side-by-Side: Defend/Heal

### C++ (Fallback)

```cpp
const char* fighter = (b.active_fighter == 0) ? "Dean" : "Sam";
int heal = 8 + game.rng() % 8;
if (b.active_fighter == 0)
    b.player_hp_actual = std::min(b.player_hp_actual + heal, b.player_hp_max);
else
    b.sam_hp_actual = std::min(b.sam_hp_actual + heal, b.sam_hp_max);
b.message = std::string(fighter) + " braces! Recovered "
          + std::to_string(heal) + " HP.";
```

### SageLang

```sage
proc defend():
    let fighter_name = "Dean"
    if active_fighter == 1:
        fighter_name = "Sam"

    let heal = 8 + random(0, 8)
    if active_fighter == 0:
        dean_hp = dean_hp + heal
        if dean_hp > dean_max_hp:
            dean_hp = dean_max_hp
    else:
        sam_hp = sam_hp + heal
        if sam_hp > sam_max_hp:
            sam_hp = sam_max_hp

    battle_damage = heal
    battle_msg = fighter_name + " braces! Recovered " + str(heal) + " HP."
    battle_target = fighter_name
```

---

## Feature Comparison

| Feature | C++ Fallback | SageLang Script |
|---------|-------------|-----------------|
| Basic attack | Fixed formula | Same formula + critical hits |
| Defend/heal | 8-15 HP | 8-16 HP |
| Enemy AI | Random target, uniform damage | Random target + 20% special attack |
| Vampire drain | Not supported | Damages target + heals self |
| Critical hits | No | 10% chance for 2x damage |
| Special attacks | No | 20% chance for +5-10 damage |
| Item attacks | No | Shotgun (25+ dmg), Holy Water (30+ dmg) |
| Game flags | No | Checks `has_shotgun`, `has_holy_water` |
| Custom messages | Template only | Dynamic per attack type |
| Per-enemy behavior | No | Yes (vampire_attack, etc.) |
| Mod without recompile | No | Yes — edit `.sage` files |
| Victory/defeat hooks | XP only | XP + flag setting |

---

## Data Flow

### How State Syncs Between C++ and SageLang

```
C++ BattleState                    SageLang Globals
─────────────────                  ─────────────────
enemy_hp_actual  ──────────────►   enemy_hp
enemy_hp_max     ──────────────►   enemy_max_hp
enemy_atk        ──────────────►   enemy_atk
enemy_name       ──────────────►   enemy_name
player_hp_actual ──────────────►   dean_hp
player_hp_max    ──────────────►   dean_max_hp
player_atk       ──────────────►   dean_atk
player_def       ──────────────►   dean_def
sam_hp_actual    ──────────────►   sam_hp
sam_hp_max       ──────────────►   sam_max_hp
sam_atk          ──────────────►   sam_atk
active_fighter   ──────────────►   active_fighter

          sync_battle_to_script()
          ═══════════════════════►

          call_function("attack_normal")
          ═══════════════════════►

          sync_battle_from_script()
          ◄═══════════════════════

enemy_hp_actual  ◄──────────────   enemy_hp
player_hp_actual ◄──────────────   dean_hp
sam_hp_actual    ◄──────────────   sam_hp
last_damage      ◄──────────────   battle_damage
message          ◄──────────────   battle_msg
```

---

## When Each System Is Used

| Scenario | System Used |
|----------|-------------|
| `battle_system.sage` loaded + function exists | SageLang |
| Script file missing or failed to load | C++ fallback |
| Script function not defined (e.g., no `attack_normal`) | C++ fallback |
| Script throws runtime error | C++ fallback (next call) |
| Enemy is Vampire + `vampire_attack` exists | SageLang (vampire-specific) |
| Enemy is Vampire + no `vampire_attack` | SageLang `enemy_turn()` or C++ fallback |

---

## Adding a New Enemy Type (SageLang Only)

To add a Demon with a fire attack, no C++ changes needed:

```sage
# In battle_system.sage

proc demon_fire_attack():
    let target = random(0, 1)
    if dean_hp <= 0:
        target = 1
    if sam_hp <= 0:
        target = 0

    let target_name = "Dean"
    if target == 1:
        target_name = "Sam"

    let damage = enemy_atk + random(5, 15)

    # Fire burns through defense
    if target == 0:
        dean_hp = dean_hp - damage
    else:
        sam_hp = sam_hp - damage

    battle_msg = enemy_name + " hurls hellfire at " + target_name
               + "! " + str(damage) + " damage!"
    battle_damage = damage
    battle_target = target_name
```

Then in C++ (one line in `update_battle`):
```cpp
if (b.enemy_name == "Demon" && game.script_engine->has_function("demon_fire_attack")) {
    game.script_engine->call_function("demon_fire_attack");
```

Or make it fully data-driven by naming the function after the enemy:
```cpp
std::string fn = lowercase(b.enemy_name) + "_attack";
if (game.script_engine->has_function(fn)) {
    game.script_engine->call_function(fn);
```

---

## Performance

| Metric | C++ | SageLang |
|--------|-----|----------|
| Function call overhead | ~0 ns | ~50-100 μs (parse + interpret) |
| Per-battle impact | None | < 1ms total |
| Memory per script | 0 | ~4KB (AST + source buffer) |
| Compilation needed | Yes | No |
| Suitable for | Frame-critical code | Turn-based actions |

Battle actions happen once per turn (player input), so SageLang's overhead is imperceptible. The real benefit is iteration speed — changing damage formulas takes seconds instead of a full rebuild.

---

## Conclusion

The hybrid approach gives us the best of both worlds:

- **C++ handles**: Battle phases, input, animations, HP rolling, rendering, audio triggers — anything that runs every frame
- **SageLang handles**: Damage calculation, healing, enemy AI decisions, special abilities, item checks, victory/defeat logic — anything that runs once per turn

This separation means:
- Game designers can tweak balance without touching C++
- New enemy types can be added in pure SageLang
- The game never crashes from a script error (falls back to C++)
- Performance-critical rendering stays in compiled code
