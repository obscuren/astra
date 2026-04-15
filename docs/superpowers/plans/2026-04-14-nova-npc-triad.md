# Nova NPC Triad Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three new enemy NPCs (Archon Remnant, Void Reaver, Archon Sentinel) so Nova Stage 2 Echoes 1-3 can have combat. Each lands in its own `src/npcs/*.cpp` with a builder, wired through the existing dispatcher, string map, theme, and CMake.

**Architecture:** Follows the existing per-role builder pattern (`src/npcs/xytomorph.cpp`, `src/npcs/pirate_captain.cpp`). Each NPC adds one enum value, one builder file, one dispatcher case, one theme case, one string-map entry, and one CMake source line. No AI or damage-system changes; `scale_to_level` handles per-location difficulty; Sentinel's `elite=true` triggers existing miniboss scaling.

**Tech Stack:** C++20, existing `Npc` / `NpcRole` / `Dice` / `TypeAffinity`. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-14-nova-npc-triad-design.md`

**No save version bump** — `NpcRole` serializes as `u8`; new values append at the tail.

**Worktree:** run this plan in `.worktrees/nova-npc-triad` on branch `feat/nova-npc-triad`, forked from `main`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/npc.h` | MODIFY | Three new `NpcRole` values |
| `include/astra/npc_defs.h` | MODIFY | Three new builder decls |
| `src/npcs/archon_remnant.cpp` | NEW | `build_archon_remnant(std::mt19937&)` |
| `src/npcs/void_reaver.cpp` | NEW | `build_void_reaver(std::mt19937&)` |
| `src/npcs/archon_sentinel.cpp` | NEW | `build_archon_sentinel(std::mt19937&)` |
| `src/npc.cpp` | MODIFY | Dispatcher arms in `create_npc`; string map in `create_npc_by_role` |
| `src/terminal_theme.cpp` | MODIFY | Three glyph/color cases in the NpcRole switch |
| `CMakeLists.txt` | MODIFY | Register three new sources |
| `src/dev_console.cpp` | MODIFY | `spawn <role>` command |

Build: `cmake --build build` (DEV build). Run: `./build/astra-dev --term`.
Commit prefixes: `feat(npc):`, `feat(dev):`.

---

### Task 1: Archon Remnant

**Files:**
- Create: `src/npcs/archon_remnant.cpp`
- Modify: `include/astra/npc.h` — add enum value
- Modify: `include/astra/npc_defs.h` — add builder decl
- Modify: `src/npc.cpp` — dispatcher arm + string map
- Modify: `src/terminal_theme.cpp` — glyph/color
- Modify: `CMakeLists.txt` — register source

- [ ] **Step 1: Add enum value**

In `include/astra/npc.h` around line 15 (`enum class NpcRole`), append after `Prospector,`:

```cpp
    ArchonRemnant,
```

- [ ] **Step 2: Declare builder**

In `include/astra/npc_defs.h`, next to `Npc build_xytomorph(std::mt19937& rng);`, add:

```cpp
Npc build_archon_remnant(std::mt19937& rng);
```

- [ ] **Step 3: Create the builder file**

Create `src/npcs/archon_remnant.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_remnant(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::ArchonRemnant;
    npc.role        = "Archon Remnant";
    npc.name        = "Archon Remnant";   // no personal name; machines
    npc.hp          = 15;
    npc.max_hp      = 15;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 100;
    npc.base_xp     = 30;
    npc.base_damage = 2;
    npc.dv          = 9;
    npc.av          = 3;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Plasma;
    // +3 plasma, -2 kinetic (brittle chassis vs. bullets; plasma-hardened).
    npc.type_affinity = {-2, 3, 0, 0, 0};
    return npc;
}

} // namespace astra
```

- [ ] **Step 4: Add dispatcher arm**

In `src/npc.cpp`, the `create_npc` switch (around line 101) currently ends with `case NpcRole::Prospector: return build_prospector(race, rng);`. Append:

```cpp
        case NpcRole::ArchonRemnant: return build_archon_remnant(rng);
```

- [ ] **Step 5: Add string map**

In `src/npc.cpp::create_npc_by_role` (around line 121), before the fallback `return`, add:

```cpp
    if (role_name == "Archon Remnant")   return create_npc(NpcRole::ArchonRemnant, Race::Human, rng);
```

- [ ] **Step 6: Theme case**

In `src/terminal_theme.cpp` around line 1105 (after `case NpcRole::Prospector:`), add:

```cpp
        case NpcRole::ArchonRemnant: return {'R', nullptr, Color::Red, Color::Default};
```

- [ ] **Step 7: CMake**

In `CMakeLists.txt`, find the list of `src/npcs/*.cpp` sources (grep for `src/npcs/xytomorph.cpp`). Add in alphabetical order (after `src/npcs/armsdealer.cpp` or similar):

```cmake
    src/npcs/archon_remnant.cpp
```

- [ ] **Step 8: Build**

Run: `cmake --build build`
Expected: clean build, no `-Wswitch` warnings.

- [ ] **Step 9: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h src/npcs/archon_remnant.cpp src/npc.cpp src/terminal_theme.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(npc): Archon Remnant — mid-tier Precursor defender

Plasma-damage automated defense. 15 hp / 3 av / 1d6 plasma. Glyph
'R' in red. Faction_ArchonRemnants. Used by Nova Stage 2 Echo 1.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Void Reaver

**Files:**
- Create: `src/npcs/void_reaver.cpp`
- Modify: `include/astra/npc.h`, `include/astra/npc_defs.h`, `src/npc.cpp`, `src/terminal_theme.cpp`, `CMakeLists.txt`

- [ ] **Step 1: Enum value**

In `include/astra/npc.h`, append after the `ArchonRemnant,` line added in Task 1:

```cpp
    VoidReaver,
```

- [ ] **Step 2: Builder decl**

In `include/astra/npc_defs.h`, add:

```cpp
Npc build_void_reaver(std::mt19937& rng);
```

- [ ] **Step 3: Builder file**

Create `src/npcs/void_reaver.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_void_reaver(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;
    npc.npc_role    = NpcRole::VoidReaver;
    npc.role        = "Void Reaver";
    npc.name        = "Void Reaver";
    npc.hp          = 20;
    npc.max_hp      = 20;
    npc.faction     = Faction_VoidReavers;
    npc.quickness   = 120;
    npc.base_xp     = 40;
    npc.base_damage = 3;
    npc.dv          = 10;
    npc.av          = 2;
    npc.damage_dice = Dice::make(1, 8);
    npc.damage_type = DamageType::Kinetic;
    // +1 kinetic (hardened armor vs. bullets).
    npc.type_affinity = {1, 0, 0, 0, 0};
    return npc;
}

} // namespace astra
```

- [ ] **Step 4: Dispatcher arm**

In `src/npc.cpp::create_npc`, after the `ArchonRemnant` arm:

```cpp
        case NpcRole::VoidReaver: return build_void_reaver(rng);
```

- [ ] **Step 5: String map**

In `create_npc_by_role`, near the entry added in Task 1:

```cpp
    if (role_name == "Void Reaver")      return create_npc(NpcRole::VoidReaver, Race::Human, rng);
```

- [ ] **Step 6: Theme case**

In `src/terminal_theme.cpp`, after the Archon Remnant case:

```cpp
        case NpcRole::VoidReaver: return {'r', nullptr, Color::DarkGray, Color::Default};
```

- [ ] **Step 7: CMake**

Add to `CMakeLists.txt` alphabetically:

```cmake
    src/npcs/void_reaver.cpp
```

- [ ] **Step 8: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 9: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h src/npcs/void_reaver.cpp src/npc.cpp src/terminal_theme.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(npc): Void Reaver — pirate-style kinetic combatant

Fast-moving human pirate with kinetic weapon. 20 hp / 2 av / 1d8
kinetic. Glyph 'r' in dark gray. Faction_VoidReavers. Used by Nova
Stage 2 Echo 2.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Archon Sentinel (miniboss)

**Files:**
- Create: `src/npcs/archon_sentinel.cpp`
- Modify: `include/astra/npc.h`, `include/astra/npc_defs.h`, `src/npc.cpp`, `src/terminal_theme.cpp`, `CMakeLists.txt`

- [ ] **Step 1: Enum value**

In `include/astra/npc.h`, append after `VoidReaver,`:

```cpp
    ArchonSentinel,
```

- [ ] **Step 2: Builder decl**

In `include/astra/npc_defs.h`:

```cpp
Npc build_archon_sentinel(std::mt19937& rng);
```

- [ ] **Step 3: Builder file**

Create `src/npcs/archon_sentinel.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_sentinel(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;
    npc.npc_role    = NpcRole::ArchonSentinel;
    npc.role        = "Archon Sentinel";
    npc.name        = "Archon Sentinel";
    npc.hp          = 50;
    npc.max_hp      = 50;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 80;   // ponderous but brutal
    npc.base_xp     = 120;
    npc.base_damage = 5;
    npc.dv          = 10;
    npc.av          = 10;   // high — forces STR-penetration play
    npc.damage_dice = Dice::make(2, 8);
    npc.damage_type = DamageType::Plasma;
    // Resistant to both common types; +4 plasma (barely dents), +2 kinetic.
    npc.type_affinity = {2, 4, 0, 0, 0};
    npc.elite       = true;   // miniboss — triggers hp x2, dv+2, av+1
    return npc;
}

} // namespace astra
```

- [ ] **Step 4: Dispatcher arm**

In `src/npc.cpp::create_npc`, after the `VoidReaver` arm:

```cpp
        case NpcRole::ArchonSentinel: return build_archon_sentinel(rng);
```

- [ ] **Step 5: String map**

In `create_npc_by_role`:

```cpp
    if (role_name == "Archon Sentinel")  return create_npc(NpcRole::ArchonSentinel, Race::Human, rng);
```

- [ ] **Step 6: Theme case**

In `src/terminal_theme.cpp`:

```cpp
        case NpcRole::ArchonSentinel: return {'S', nullptr, Color::BrightYellow, Color::Default};
```

(Glyph `S` is shared with `Scavenger` — colors differentiate, matching the existing pattern where `D` is shared across Drifter and Medic.)

- [ ] **Step 7: CMake**

Add to `CMakeLists.txt` alphabetically:

```cmake
    src/npcs/archon_sentinel.cpp
```

- [ ] **Step 8: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 9: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h src/npcs/archon_sentinel.cpp src/npc.cpp src/terminal_theme.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(npc): Archon Sentinel — elite miniboss with high AV

Ponderous Precursor defender. 50 hp / 10 av / 2d8 plasma /
elite=true (scale_to_level doubles hp, bumps dv+2 av+1 at level).
Forces player to use STR penetration mechanics. Glyph 'S' in
BrightYellow. Used by Nova Stage 2 Echo 3.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Dev-console `spawn <role>` command

**Files:**
- Modify: `src/dev_console.cpp` — new top-level verb

- [ ] **Step 1: Find the verb-dispatch chain**

In `src/dev_console.cpp`, the dispatcher is an `else if (verb == "...")` chain (around lines 104-147 is the help; the actual handlers follow). Locate where `chart` is handled (added in the star-systems work). Add `spawn` as a sibling.

- [ ] **Step 2: Add the handler**

Insert this block near the `chart` handler:

```cpp
    else if (verb == "spawn") {
        if (args.size() < 2) {
            log("Usage: spawn <role>  (archon_remnant|void_reaver|archon_sentinel)");
            return;
        }
        std::string role_arg = args[1];
        std::string role_name;
        if      (role_arg == "archon_remnant")  role_name = "Archon Remnant";
        else if (role_arg == "void_reaver")     role_name = "Void Reaver";
        else if (role_arg == "archon_sentinel") role_name = "Archon Sentinel";
        else {
            log("spawn: unknown role '" + role_arg +
                "' (archon_remnant|void_reaver|archon_sentinel)");
            return;
        }

        Npc npc = create_npc_by_role(role_name, game.world().rng());
        // Place adjacent to player; walk the 8 neighbours until a passable
        // empty tile is found.
        const int dx[] = {1, -1, 0, 0, 1, 1, -1, -1};
        const int dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
        bool placed = false;
        for (int i = 0; i < 8 && !placed; ++i) {
            int nx = game.player().x + dx[i];
            int ny = game.player().y + dy[i];
            if (nx < 0 || nx >= game.world().map().width()) continue;
            if (ny < 0 || ny >= game.world().map().height()) continue;
            if (!game.world().map().passable(nx, ny)) continue;
            bool occupied = false;
            for (const auto& other : game.world().npcs()) {
                if (other.alive() && other.x == nx && other.y == ny) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            npc.x = nx;
            npc.y = ny;
            game.world().npcs().push_back(std::move(npc));
            log("Spawned " + role_name + " at (" + std::to_string(nx) +
                "," + std::to_string(ny) + ")");
            placed = true;
        }
        if (!placed) log("spawn: no adjacent passable tile");
    }
```

- [ ] **Step 3: Update help text**

Find the help block (the verb list printed on bare-verb help, around line 134). Add a new line with existing formatting:

```cpp
    log("  spawn <role> - spawn an enemy NPC adjacent to player");
```

- [ ] **Step 4: Confirm includes**

At the top of `src/dev_console.cpp`, confirm these are present (add if missing):

```cpp
#include "astra/npc.h"
```

(Other includes like `astra/game.h` are already pulled in for existing handlers.)

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): spawn <role> — dev-console command to spawn enemy NPCs

Supports archon_remnant | void_reaver | archon_sentinel. Finds the
first passable, unoccupied neighbour of the player and places the
NPC there. Smoke-test surface for the Nova triad.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: End-to-end smoke test

**Files:** none modified — manual verification.

- [ ] **Step 1: Launch**

```
./build/astra-dev --term
```

- [ ] **Step 2: Remnant**

Start a new game. Open dev console (backtick). Run:

```
spawn archon_remnant
```

Expected: `R` in red appears next to `@`. Close console. Attack with any weapon; confirm:
- Remnant fights back with plasma damage (check the combat log).
- HP visible roughly 15 at level 1.
- Dies after a couple of hits.

- [ ] **Step 3: Reaver**

```
spawn void_reaver
```

Expected: `r` in dark gray. Faster turn order (quickness 120). Kinetic damage. ~20 hp.

- [ ] **Step 4: Sentinel**

```
spawn archon_sentinel
```

Expected: `S` in bright yellow. Most attacks bounce (av 10 + elite bump to 11). Penetration rolls rarely succeed without STR investment. HP should read ~100 after the elite x2 bump from `scale_to_level` at level 1 (hp=50 base → elite=true → scale_to_level doubles to 100).

- [ ] **Step 5: Confirm faction-tagging**

In the combat log or inspect panel, the NPCs should show the correct faction. If there's a faction display, confirm Archon Remnants / Void Reavers show.

- [ ] **Step 6: No commit**

Smoke test only.

---

## Acceptance Criteria

- `cmake --build build` clean at every commit.
- `spawn archon_remnant` / `spawn void_reaver` / `spawn archon_sentinel` each produce the expected NPC with correct glyph/color/stats.
- Sentinel's high AV (10) is apparent: low-STR attacks fail to penetrate; high-STR or plasma-piercing does damage.
- Kinetic weapons hit Remnant for reduced damage (−2 kinetic affinity).
- No `-Wswitch` warnings anywhere in the codebase after Task 3.
- Existing NPC behavior (Xytomorph, Drifter, etc.) unchanged.

---

## Out of Scope (explicitly deferred)

- AI changes (telegraphs, summons, phase transitions).
- Loot drops on death.
- Precursor Linguist dead-language examine text.
- Faction reputation adjustments from kills.
- Nova quest wiring.
- Neutron-star / derelict-station systems.
