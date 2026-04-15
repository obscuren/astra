# NPC Triad: Archon Remnant, Void Reaver, Archon Sentinel — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` (unblocks Nova Stage 2 Echoes 1–3 combat).

## Summary

Add three new enemy NPCs so Nova arc Stage 2 can play. Each `NpcRole` ships with a dedicated builder in `src/npcs/`, a glyph/color in the theme table, and a string mapping in the quest-spawn dispatcher. All three scale to location level via the existing `scale_to_level` path; the Sentinel additionally sets `elite=true`, which pushes it into miniboss territory through pre-existing logic.

No AI changes. No new damage types, loot, or dialog systems. Just three new NPC entries composed from existing infrastructure.

---

## Goals

- Three new `NpcRole` enum values: `ArchonRemnant`, `VoidReaver`, `ArchonSentinel`.
- Per-role builder files mirroring the layout of existing `src/npcs/*.cpp`.
- Stats that match the Nova doc's intent: mid-tier Precursor defender (Remnant), standard pirate (Reaver), high-AV penetration-testing miniboss (Sentinel).
- Rendering + string-id dispatch wired into the same places every other role uses.
- Dev-console `spawn <role>` command to smoke-test each in isolation.

## Non-goals

- New AI / behavior patterns (all three use generic `process_npc_turn` pursuit).
- Precursor-Linguist dead-language examine text.
- Custom loot drops.
- New damage types, affinities, or shield mechanics (all use existing `DamageType` / `TypeAffinity`).
- Nova quest wiring — that's a separate spec once all primitives land.

---

## Data Model

### `NpcRole` extension (`include/astra/npc.h`)

Append three values to the enum:

```cpp
enum class NpcRole {
    // ... existing ...
    ArchonRemnant,
    VoidReaver,
    ArchonSentinel,
};
```

### Per-role builders

Each NPC lives in its own `.cpp` under `src/npcs/`. Builders mirror the `build_xytomorph` / `build_pirate_grunt` / `build_pirate_captain` pattern: construct a default `Npc`, set fields, return by value.

#### Archon Remnant — `src/npcs/archon_remnant.cpp`

```cpp
Npc build_archon_remnant(std::mt19937& /*rng*/) {
    Npc n;
    n.role = "Archon Remnant";
    n.faction = "Faction_ArchonRemnants";
    n.role_enum = NpcRole::ArchonRemnant;
    n.hp = n.max_hp = 15;
    n.av = 3;
    n.dv = 9;
    n.damage_dice = Dice{1, 6, 0};
    n.damage_type = DamageType::Plasma;
    n.type_affinity = TypeAffinity{-2, +3, 0, 0, 0};  // +plasma, -kinetic
    n.quickness = 100;
    n.elite = false;
    return n;
}
```

#### Void Reaver — `src/npcs/void_reaver.cpp`

```cpp
Npc build_void_reaver(std::mt19937& /*rng*/) {
    Npc n;
    n.role = "Void Reaver";
    n.faction = "Faction_VoidReavers";
    n.role_enum = NpcRole::VoidReaver;
    n.hp = n.max_hp = 20;
    n.av = 2;
    n.dv = 10;
    n.damage_dice = Dice{1, 8, 0};
    n.damage_type = DamageType::Kinetic;
    n.type_affinity = TypeAffinity{+1, 0, 0, 0, 0};
    n.quickness = 120;
    n.elite = false;
    return n;
}
```

#### Archon Sentinel — `src/npcs/archon_sentinel.cpp`

```cpp
Npc build_archon_sentinel(std::mt19937& /*rng*/) {
    Npc n;
    n.role = "Archon Sentinel";
    n.faction = "Faction_ArchonRemnants";
    n.role_enum = NpcRole::ArchonSentinel;
    n.hp = n.max_hp = 50;
    n.av = 10;                  // high AV — tests player's STR penetration
    n.dv = 10;
    n.damage_dice = Dice{2, 8, 0};
    n.damage_type = DamageType::Plasma;
    n.type_affinity = TypeAffinity{+2, +4, 0, 0, 0};  // resistant to both common types
    n.quickness = 80;           // ponderous but hits hard
    n.elite = true;             // miniboss — triggers scale bumps
    return n;
}
```

Exact field names and `Npc` fields used here are the ones visible in the current `include/astra/npc.h`. The plan will confirm each name against the struct.

---

## Integration

### 1. Dispatcher (`src/npc.cpp`)

Two switches need the new roles:

- **Role → builder dispatch** (around line 101). Add three `case NpcRole::...: return build_...(rng);` arms.
- **Quest string → NpcRole** (around line 121, `create_npc_by_role(const std::string&, std::mt19937&)`). Add three `if (role_name == "Archon Remnant") return create_npc(NpcRole::ArchonRemnant, Race::Unknown, rng);` entries (mirroring existing pattern for strings like `"Xytomorph"`).

Matching strings expected to be used by `QuestLocationMeta.npc_roles`:

- `"Archon Remnant"` → `NpcRole::ArchonRemnant`
- `"Void Reaver"` → `NpcRole::VoidReaver`
- `"Archon Sentinel"` → `NpcRole::ArchonSentinel`

### 2. Rendering (`src/terminal_theme.cpp`)

Around lines 1086-1105 is the role → glyph/color table. Add:

| Role | Glyph | Color |
|---|---|---|
| `ArchonRemnant` | `'R'` | `Color::Red` |
| `VoidReaver` | `'r'` | `Color::DarkGray` |
| `ArchonSentinel` | `'S'` | `Color::BrightYellow` |

### 3. Build system (`CMakeLists.txt`)

Add the three new `src/npcs/*.cpp` files to `ASTRA_SOURCES` alongside existing entries like `src/npcs/xytomorph.cpp`.

### 4. Exhaustive-switch coverage

After extending the enum, `-Wswitch` will flag any other switch over `NpcRole` that doesn't handle the new values. Each one gets an arm. Likely candidates (to be confirmed during implementation):

- `src/terminal_theme.cpp` (role glyph/color table) — core wiring, already called out above.
- Any journal / ui / character-screen display that stringifies `NpcRole`.
- Any faction-reputation lookup that keys off role.

The plan enumerates them via a build pass and patches each in one task.

---

## Dev-Console Smoke Test

Add a `spawn <role>` command to `src/dev_console.cpp`. Maps role-string to NpcRole via the same dispatch as quest spawns; places the NPC adjacent to the player (east, fallback to any free neighbour).

```
spawn archon_remnant
spawn void_reaver
spawn archon_sentinel
```

Smoke-test flow:

1. `spawn archon_remnant` → `R` in red appears next to `@`. Engage. Confirm plasma damage hits and that a kinetic weapon is reduced (negative kinetic affinity).
2. `spawn void_reaver` → `r` in dark gray. Confirm kinetic shots hit normally.
3. `spawn archon_sentinel` → `S` in bright yellow. Confirm attacks rarely penetrate at default STR; a STR-invested player can land damage.
4. `elite` scaling sanity-check: Sentinel's hp ≈ 100 after `scale_to_level` with elite bumps.

---

## Save / Load

No schema changes. `NpcRole` is serialized as `u8`; the three new values append cleanly at the end of the enum, so older saves never contain them and newer saves encode them as 14/15/16 (or wherever the existing enum ends).

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/npc.h` | MODIFY | Three new `NpcRole` values |
| `src/npcs/archon_remnant.cpp` | NEW | `build_archon_remnant` |
| `src/npcs/void_reaver.cpp` | NEW | `build_void_reaver` |
| `src/npcs/archon_sentinel.cpp` | NEW | `build_archon_sentinel` |
| `src/npc.cpp` | MODIFY | Dispatcher arms; string → role mapping |
| `src/terminal_theme.cpp` | MODIFY | Glyph/color rows; any other NpcRole switch |
| `CMakeLists.txt` | MODIFY | Add three new sources |
| `src/dev_console.cpp` | MODIFY | `spawn <role>` smoke-test command |

---

## Implementation Checklist (for the forthcoming plan)

1. Append three values to `NpcRole`.
2. Add `src/npcs/archon_remnant.cpp` + builder; register in CMake.
3. Add `src/npcs/void_reaver.cpp` + builder; register in CMake.
4. Add `src/npcs/archon_sentinel.cpp` + builder with `elite=true`; register in CMake.
5. Extend `src/npc.cpp` dispatcher + string map.
6. Extend theme table with glyph + color for each new role.
7. Patch any other `-Wswitch` sites the build surfaces.
8. Add `spawn <role>` dev-console command.
9. Smoke-test all three in a running game.

---

## Out of scope — explicitly deferred

- AI behavior changes (telegraphs, summons, phases).
- Loot / ground-item drops on death.
- Precursor-Linguist dead-language examine.
- Neutron-star / derelict-station systems.
- Nova arc wiring.
- Faction reputation changes from killing these NPCs beyond defaults.
