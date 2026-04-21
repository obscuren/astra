# NPC Ranged Attacks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ranged attacks for NPCs with Bresenham line-of-sight, and wire Sentry Drone as the first turret enemy (6-tile plasma shots).

**Architecture:** Add `NpcAi` enum (`Melee`/`Turret`/`Kiter`) and three new fields on `Npc` (`attack_range`, `ranged_damage_dice`, `ranged_damage_type`). In `CombatSystem::process_npc_turn`, before the existing melee branch, check for a ranged opportunity: if AI is `Turret` and target is in `attack_range` with clear LOS, resolve a ranged hit. Turret AI holds position when it can't shoot. Covers both player-target and NPC-target (NPC-vs-NPC) paths. No projectile animation — reuse `anim_damage_flash` on the target tile. LOS uses a Bresenham walker against `TileMap::opaque(x,y)`.

**Tech Stack:** C++20, CMake (`-DDEV=ON`), namespace `astra`. No test framework — verification is clean build + manual test via dev console `spawn sentry_drone`.

**Scope boundary:** Sentry Drone only. Other NPCs stay melee. No projectile animation system. No NPC weapon-item equipping. `Kiter` AI is defined but not implemented (reserved for future ranged mob).

---

## File Structure

- **Modify** `include/astra/npc.h` — add `NpcAi` enum + three fields on `Npc`.
- **Modify** `src/game_combat.cpp` — add static `los_clear()`, static `ranged_hit_player()`, static `ranged_hit_npc()`, and new branches in `process_npc_turn()`.
- **Modify** `src/npcs/sentry_drone.cpp` — set `ai = NpcAi::Turret`, `attack_range = 6`, `ranged_damage_dice = 1d6`, `ranged_damage_type = Plasma`. Remove placeholder TODO comment.
- **Modify** `docs/formulas.md` — document NPC ranged attack rolls (per CLAUDE.md rule).
- **Modify** `docs/roadmap.md` — check off ranged-NPC item if present; otherwise add a completed entry.

No new files. No changes to `CombatSystem` class header (helpers are file-local statics).

---

### Task 1: Add `NpcAi` enum and ranged fields to `Npc`

**Files:**
- Modify: `include/astra/npc.h`

- [ ] **Step 1: Add the `NpcAi` enum above `NpcRole`**

In `include/astra/npc.h`, directly above the existing `enum class NpcRole : uint8_t {` line, insert:

```cpp
enum class NpcAi : uint8_t {
    Melee,    // adjacency attacks only (default)
    Turret,   // ranged attack in range+LOS, otherwise hold position
    Kiter,    // reserved: ranged attack in range+LOS, otherwise close the gap
};

```

(blank line after the closing brace, before `enum class NpcRole`).

- [ ] **Step 2: Add the three ranged fields plus the `ai` field to `struct Npc`**

In `include/astra/npc.h`, inside `struct Npc`, directly after the existing line:

```cpp
    DamageType damage_type = DamageType::Kinetic;
```

insert:

```cpp
    // Ranged attack (empty ranged_damage_dice disables ranged path)
    int attack_range = 1;              // chebyshev tiles; 1 = melee only
    Dice ranged_damage_dice;           // empty by default
    DamageType ranged_damage_type = DamageType::Kinetic;
    NpcAi ai = NpcAi::Melee;
```

- [ ] **Step 3: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: clean build, no warnings about the new fields.

- [ ] **Step 4: Commit**

```bash
git add include/astra/npc.h
git commit -m "feat(npc): add NpcAi enum and ranged attack fields"
```

---

### Task 2: Add LOS helper in `game_combat.cpp`

**Files:**
- Modify: `src/game_combat.cpp`

- [ ] **Step 1: Add `los_clear` static helper**

In `src/game_combat.cpp`, directly after the existing `static int chebyshev_dist(...)` function (around line 21), insert:

```cpp
// Bresenham line-of-sight from (x0,y0) to (x1,y1). Endpoints are excluded
// (attacker and target tiles are creatures, not obstacles). Returns false
// if any intervening tile is opaque (walls, closed doors, blocks_vision
// fixtures). Used by NPC ranged attacks.
static bool los_clear(const TileMap& map, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
        if (x == x1 && y == y1) break;
        if (map.opaque(x, y)) return false;
    }
    return true;
}
```

- [ ] **Step 2: Confirm `TileMap` header is reachable**

`TileMap` is used via `game.world().map()` elsewhere in this file, but the static takes it by reference. Confirm `#include "astra/tilemap.h"` is reachable transitively through `astra/game.h`. If a direct compile error appears on `TileMap`, add `#include "astra/tilemap.h"` at the top of `src/game_combat.cpp` alongside the other astra includes.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build. The helper is unused at this point — a `-Wunused-function` warning is acceptable and will resolve in Task 4.

- [ ] **Step 4: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(combat): add Bresenham LOS helper for ranged attacks"
```

---

### Task 3: Add ranged hit helpers (player and NPC targets)

**Files:**
- Modify: `src/game_combat.cpp`

- [ ] **Step 1: Add `ranged_hit_player` static helper**

In `src/game_combat.cpp`, directly above `void CombatSystem::attack_npc_vs_npc(...)` (around line 176), insert:

```cpp
static void ranged_hit_player(Npc& npc, Game& game) {
    auto& rng = game.world().rng();
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log("You evade " + display_name(npc) + "'s shot!");
        return;
    }
    int attack_roll = natural + npc.level / 2;
    if (natural != 20 && attack_roll < game.player().effective_dv()) {
        game.log("You evade " + display_name(npc) + "'s shot!");
        return;
    }

    const Dice& dmg = npc.ranged_damage_dice;
    DamageType dtype = npc.ranged_damage_type;

    if (game.player().shield_hp > 0) {
        auto pen = roll_penetration(rng, npc.level / 3, 0, dmg);
        if (pen.total_damage <= 0) {
            game.log(display_name(npc) + "'s shot is absorbed by your shield.");
            return;
        }
        int absorbed = shield_absorb(pen.total_damage, dtype, game.player().shield_affinity);
        game.player().shield_hp -= absorbed;
        if (game.player().shield_hp < 0) game.player().shield_hp = 0;
        game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
        game.log(display_name(npc) + " shoots your shield for " +
                 std::to_string(absorbed) + " " + display_name(dtype) + " damage. [Shield " +
                 std::to_string(game.player().shield_hp) + "/" +
                 std::to_string(game.player().shield_max_hp) + "]");
        return;
    }

    int eff_av = game.player().effective_av(dtype);
    auto pen = roll_penetration(rng, npc.level / 3, eff_av, dmg);
    if (pen.total_damage <= 0) {
        game.log(display_name(npc) + " shoots at you but deals no damage.");
        return;
    }

    int damage = apply_resistance(pen.total_damage, dtype, game.player().resistances);
    damage = apply_damage_effects(game.player().effects, damage);
    if (damage <= 0) {
        game.log(display_name(npc) + " shoots at you but deals no damage.");
        return;
    }
    game.player().hp -= damage;
    if (game.player().hp < 0) game.player().hp = 0;
    game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
    game.log(display_name(npc) + " shoots you for " +
             std::to_string(damage) + " " + display_name(dtype) + " damage!");
    if (game.player().hp <= 0) {
        game.set_death_message("Shot by " + display_name(npc));
    }
}
```

- [ ] **Step 2: Add `ranged_hit_npc` static helper**

Directly below `ranged_hit_player`, insert:

```cpp
static void ranged_hit_npc(Npc& attacker, Npc& defender, Game& game) {
    auto& rng = game.world().rng();
    int natural = roll_d20(rng);
    if (natural == 1) {
        game.log(display_name(defender) + " evades " + display_name(attacker) + "'s shot!");
        return;
    }
    int attack_roll = natural + attacker.level / 2;
    if (natural != 20 && attack_roll < defender.dv) {
        game.log(display_name(defender) + " evades " + display_name(attacker) + "'s shot!");
        return;
    }

    const Dice& dmg = attacker.ranged_damage_dice;
    DamageType dtype = attacker.ranged_damage_type;

    int effective_av = defender.av + defender.type_affinity.for_type(dtype);
    auto pen = roll_penetration(rng, attacker.level / 3, effective_av, dmg);
    if (pen.total_damage <= 0) {
        game.log(display_name(attacker) + "'s shot has no effect on " + display_name(defender) + ".");
        return;
    }

    int damage = apply_damage_effects(defender.effects, pen.total_damage);
    if (damage <= 0) {
        game.log(display_name(attacker) + "'s shot has no effect on " + display_name(defender) + ".");
        return;
    }
    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, defender.x, defender.y);
    game.log(display_name(attacker) + " shoots " + display_name(defender) +
             " for " + std::to_string(damage) + " " + display_name(dtype) + " damage!");
    if (!defender.alive()) {
        game.log(display_name(defender) + " is destroyed by " + display_name(attacker) + "!");
    }
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build. Helpers are still unused — warning acceptable until Task 4.

- [ ] **Step 4: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(combat): add NPC ranged hit resolution helpers"
```

---

### Task 4: Wire ranged attack into `process_npc_turn` — player target branch

**Files:**
- Modify: `src/game_combat.cpp` (the `if (target.is_player) { ... }` block, currently around lines 258–336)

- [ ] **Step 1: Insert ranged path at top of player-target branch**

Find this existing block in `process_npc_turn`:

```cpp
    if (target.is_player) {
        int dist = target.distance;
        if (dist <= 1) {
            auto& rng = game.world().rng();
```

Change it so the `int dist = target.distance;` line is immediately followed by a ranged check, before the `if (dist <= 1)` line. Result:

```cpp
    if (target.is_player) {
        int dist = target.distance;

        // Ranged attack: in range, has ranged weapon, and clear LOS.
        if (npc.ai == NpcAi::Turret
            && dist > 1
            && dist <= npc.attack_range
            && !npc.ranged_damage_dice.empty()
            && los_clear(game.world().map(), npc.x, npc.y,
                         game.player().x, game.player().y)) {
            ranged_hit_player(npc, game);
            return;
        }

        if (dist <= 1) {
            auto& rng = game.world().rng();
```

- [ ] **Step 2: Make Turret AI hold position when it can't shoot**

Still inside the player-target branch, find the end of the melee block (after the `if (dist <= 1) { ... return; }` block closes) and before the existing movement candidates. Currently:

```cpp
            return;
        }
        int dx = sign(game.player().x - npc.x);
        int dy = sign(game.player().y - npc.y);
```

Insert a Turret hold guard between the closing `}` of the melee block and the `int dx = ...` line:

```cpp
            return;
        }

        // Turrets don't chase — they hold position when they can't shoot.
        if (npc.ai == NpcAi::Turret) return;

        int dx = sign(game.player().x - npc.x);
        int dy = sign(game.player().y - npc.y);
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build, `-Wunused-function` on `ranged_hit_npc` may still warn (resolved in Task 5).

- [ ] **Step 4: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(combat): wire NPC ranged attack vs player with turret hold"
```

---

### Task 5: Wire ranged attack into `process_npc_turn` — NPC target branch

**Files:**
- Modify: `src/game_combat.cpp` (the `if (target.npc) { ... }` block, currently around lines 338–358)

- [ ] **Step 1: Insert ranged path at top of NPC-target branch**

Find this existing block:

```cpp
    if (target.npc) {
        int dist = target.distance;
        if (dist <= 1) {
            attack_npc_vs_npc(npc, *target.npc, game);
            return;
        }
```

Change it so the ranged check precedes the melee check. Result:

```cpp
    if (target.npc) {
        int dist = target.distance;

        // Ranged attack: in range, has ranged weapon, and clear LOS.
        if (npc.ai == NpcAi::Turret
            && dist > 1
            && dist <= npc.attack_range
            && !npc.ranged_damage_dice.empty()
            && los_clear(game.world().map(), npc.x, npc.y,
                         target.npc->x, target.npc->y)) {
            ranged_hit_npc(npc, *target.npc, game);
            return;
        }

        if (dist <= 1) {
            attack_npc_vs_npc(npc, *target.npc, game);
            return;
        }
```

- [ ] **Step 2: Make Turret hold in the NPC-target branch**

Still in the NPC-target branch, find the movement candidates:

```cpp
        int dx = sign(target.npc->x - npc.x);
        int dy = sign(target.npc->y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
```

Insert a hold guard directly above `int dx = ...`:

```cpp
        // Turrets don't chase — they hold position when they can't shoot.
        if (npc.ai == NpcAi::Turret) return;

        int dx = sign(target.npc->x - npc.x);
        int dy = sign(target.npc->y - npc.y);
        struct { int x, y; } candidates[] = {{dx, dy}, {dx, 0}, {0, dy}};
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build, no unused-function warnings (both ranged helpers now used).

- [ ] **Step 4: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(combat): wire NPC ranged attack vs NPC with turret hold"
```

---

### Task 6: Configure Sentry Drone as first turret enemy

**Files:**
- Modify: `src/npcs/sentry_drone.cpp`

- [ ] **Step 1: Set ranged stats and AI, remove placeholder TODO**

Replace the entire body of `build_sentry_drone` in `src/npcs/sentry_drone.cpp`. The current contents end with:

```cpp
    npc.damage_dice = Dice::make(1, 6);
    // TODO: ranged attack — ships as melee placeholder until ranged combat lands.
    npc.damage_type = DamageType::Plasma;
    npc.type_affinity = {1, 2, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
```

Replace with:

```cpp
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Plasma;
    // Turret: ranged plasma bolt, holds position when it can't shoot.
    npc.ai                 = NpcAi::Turret;
    npc.attack_range       = 6;
    npc.ranged_damage_dice = Dice::make(1, 6);
    npc.ranged_damage_type = DamageType::Plasma;
    npc.type_affinity = {1, 2, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Manual test**

Run `./build/astra-dev`, start a new game or load. Open dev console, type `spawn sentry_drone`. Verify:
- Drone spawns but does NOT rush you when you're 2–6 tiles away (it stays put).
- Drone fires at you from range with "shoots you for N Plasma damage!" or evade/absorb message.
- Drone stops firing when you break LOS (duck behind a wall) — no log line about shooting.
- When adjacent, it still melees ("strikes you").
- Spawn two mobs that are hostile to each other (e.g. `spawn sentry_drone` then `spawn rust_hound` nearby — the drone belongs to `Faction_Feral`, hound likely different; confirm with dev console) and watch the drone shoot the hound at range.

If any of these fail, stop and debug before continuing.

- [ ] **Step 4: Commit**

```bash
git add src/npcs/sentry_drone.cpp
git commit -m "feat(npc): sentry drone fires ranged plasma bolts at 6 tiles"
```

---

### Task 7: Documentation

**Files:**
- Modify: `docs/formulas.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Document NPC ranged formulas**

Open `docs/formulas.md` and locate the section covering NPC combat (grep for "NPC" or "process_npc_turn" if unsure). Append a subsection describing the ranged attack path. Use this content verbatim; adapt heading level to match surrounding sections:

```markdown
### NPC Ranged Attacks

Preconditions: attacker has `ai == NpcAi::Turret`, chebyshev distance in `[2, attack_range]`, non-empty `ranged_damage_dice`, and Bresenham LOS (no opaque tile between attacker and target).

- Attack roll: `1d20 + level/2` vs target DV (player: `effective_dv()`; NPC: `dv`). Natural 1 always misses; natural 20 always hits.
- Penetration: `1d10 + level/3` vs effective AV (+ `type_affinity` for NPC target). Natural 1 = no damage; natural 10 = always penetrates. Each 4 points of excess triggers another damage roll.
- Shield (player only): penetration rolled against AV=0; damage scaled by `shield_affinity`; never bypasses shield.
- Damage: `ranged_damage_dice` of `ranged_damage_type`, then `apply_resistance` (player) and `apply_damage_effects` (player or NPC).

Turret AI holds position when out of range or LOS is blocked; it never pursues.
```

- [ ] **Step 2: Update roadmap**

Open `docs/roadmap.md`. If there is an unchecked item mentioning ranged NPCs / turrets / Sentry Drone ranged, check it off. If no such item exists, add a single completed line in the combat section:

```markdown
- [x] NPC ranged attacks (Turret AI, Sentry Drone fires plasma bolts at 6 tiles)
```

- [ ] **Step 3: Commit**

```bash
git add docs/formulas.md docs/roadmap.md
git commit -m "docs: NPC ranged attack formulas and roadmap entry"
```

---

## Verification Checklist (after all tasks)

- [ ] `cmake --build build` exits clean with no warnings introduced by this change.
- [ ] `./build/astra-dev` + `spawn sentry_drone` produces a ranged enemy that shoots at 2–6 tiles, holds position, and still melees adjacent.
- [ ] LOS respects walls/closed doors (drone stops firing when you break LOS).
- [ ] Drone vs. hostile NPC: drone shoots the hound at range.
- [ ] `git log --oneline` shows 6–7 focused commits (one per task step 4).
- [ ] No other NPC behavior changed — existing melee enemies still charge and hit as before.
