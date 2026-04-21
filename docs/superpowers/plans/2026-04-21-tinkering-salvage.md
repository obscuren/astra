# Tinkering Salvage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Introduce a two-tier salvage system: ungated low-chance *Spare Parts* drops from any kill, plus `Cat_Tinkering`-gated auto-salvage (Spare Parts + Circuitry) from new mechanical enemies.

**Architecture:** New `CreatureFlag` bitfield (`uint64_t`) on `Npc` identifies mechanical vs. biological creatures. Salvage logic branches in `src/game_combat.cpp` on both the melee and ranged kill paths. Three new mechanical NPCs follow the existing per-file factory pattern under `src/npcs/`. New items use existing build_* / display_name infrastructure so log coloring works for free.

**Tech Stack:** C++20, CMake. No test framework — verification is build-clean + dev-mode (`-DDEV=ON`) runtime smoke-test via `debug_spawn` and kill logs.

**Spec:** `docs/superpowers/specs/2026-04-21-tinkering-salvage-design.md`

---

## File Structure

**New files:**
- `include/astra/creature_flags.h` — `CreatureFlag` enum + `has_flag` helper + `is_mechanical`/`is_biological` wrappers
- `src/npcs/rust_hound.cpp` — low-tier fast-melee mechanical NPC
- `src/npcs/sentry_drone.cpp` — mid-tier mechanical NPC (melee placeholder; TODO ranged)
- `src/npcs/archon_automaton.cpp` — high-tier heavy-melee mechanical NPC

**Modified files:**
- `include/astra/item_ids.h` — add `ITEM_SPARE_PARTS = 47`, `ITEM_CIRCUITRY = 48`
- `include/astra/item_defs.h` — declare `build_spare_parts()`, `build_circuitry()`
- `src/item_defs.cpp` — implement both builders
- `src/terminal_theme.cpp` — theme entries for new items + new NPC roles
- `src/save_file.cpp` — new item name→id entries; persist `Npc.flags` with version bump v35 → v36
- `include/astra/save_file.h` — bump version field default to `36`
- `include/astra/npc.h` — new `NpcRole` enum values; `uint64_t flags` field on `Npc`
- `include/astra/npc_defs.h` — declare `build_rust_hound`, `build_sentry_drone`, `build_archon_automaton`
- `src/npc.cpp` — dispatch new roles in `create_npc` and `create_npc_by_role`
- `src/npcs/*.cpp` (existing organic hostiles) — set `Biological` flag
- `src/debug_spawn.cpp` — spawn a mechanical NPC for verification
- `src/game_combat.cpp` — salvage branches in both melee and ranged kill paths
- `CMakeLists.txt` — add three new `.cpp` files to `ASTRA_SOURCES`

---

## Task 1: Add Spare Parts and Circuitry items

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp`
- Modify: `src/terminal_theme.cpp`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Add item IDs**

In `include/astra/item_ids.h`, after the existing `ITEM_VOID_MANTLE = 46;` line (around line 75), before the `// Synthesized items (1000+)` comment:

```cpp
// Salvage resources (47-48)
constexpr uint16_t ITEM_SPARE_PARTS         = 47;
constexpr uint16_t ITEM_CIRCUITRY           = 48;
```

- [ ] **Step 2: Declare builders**

In `include/astra/item_defs.h`, after the existing `// --- Junk ---` block (line 57–60):

```cpp
// --- Salvage ---
Item build_spare_parts();
Item build_circuitry();
```

- [ ] **Step 3: Implement builders**

In `src/item_defs.cpp`, after `build_empty_casing()` (around line 569), before the `// Crafting materials` block:

```cpp
// ---------------------------------------------------------------------------
// Salvage
// ---------------------------------------------------------------------------

Item build_spare_parts() {
    Item it;
    it.item_def_id = ITEM_SPARE_PARTS;
    it.id = 6010; it.name = "Spare Parts"; it.type = ItemType::Junk;
    it.description = "Usable parts pulled from wreckage. Good for repairs.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 4;
    return it;
}

Item build_circuitry() {
    Item it;
    it.item_def_id = ITEM_CIRCUITRY;
    it.id = 6011; it.name = "Circuitry"; it.type = ItemType::Junk;
    it.description = "Salvaged integrated circuits. Essential for advanced repair.";
    it.weight = 1;
    it.stackable = true; it.sell_value = 8;
    return it;
}
```

- [ ] **Step 4: Theme entries**

In `src/terminal_theme.cpp`, after the existing `case ITEM_EMPTY_CASING:` line (around line 1182), before the `// Crafting materials` comment:

```cpp
        case ITEM_SPARE_PARTS:         return {'~', nullptr, Color::Yellow, Color::Default};
        case ITEM_CIRCUITRY:           return {'~', nullptr, Color::Cyan, Color::Default};
```

- [ ] **Step 5: Save-file name→id mapping**

In `src/save_file.cpp`, after the existing `{"Empty Casing", ITEM_EMPTY_CASING},` line (around line 252):

```cpp
        {"Spare Parts", ITEM_SPARE_PARTS},
        {"Circuitry", ITEM_CIRCUITRY},
```

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: clean build, no warnings related to new identifiers.

- [ ] **Step 7: Commit**

```bash
git add include/astra/item_ids.h include/astra/item_defs.h src/item_defs.cpp src/terminal_theme.cpp src/save_file.cpp
git commit -m "feat(items): add Spare Parts and Circuitry salvage items"
```

---

## Task 2: Creature flags header

**Files:**
- Create: `include/astra/creature_flags.h`

- [ ] **Step 1: Create header**

Create `include/astra/creature_flags.h`:

```cpp
#pragma once

#include <cstdint>

namespace astra {

struct Npc;

// Bitflags describing intrinsic creature traits. Stored as uint64_t on Npc.
// Room for 64 future traits (undead, psionic, synthetic, etc.).
enum class CreatureFlag : uint64_t {
    None       = 0,
    Mechanical = 1ull << 0,
    Biological = 1ull << 1,
};

constexpr uint64_t operator|(CreatureFlag a, CreatureFlag b) {
    return static_cast<uint64_t>(a) | static_cast<uint64_t>(b);
}

constexpr uint64_t operator|(uint64_t a, CreatureFlag b) {
    return a | static_cast<uint64_t>(b);
}

inline bool has_flag(uint64_t flags, CreatureFlag f) {
    return (flags & static_cast<uint64_t>(f)) != 0;
}

bool is_mechanical(const Npc& npc);
bool is_biological(const Npc& npc);

} // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: header unused yet — still compiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add include/astra/creature_flags.h
git commit -m "feat(npc): add CreatureFlag bitfield header"
```

---

## Task 3: Add flags field to Npc, implement helpers

**Files:**
- Modify: `include/astra/npc.h`
- Modify: `src/npc.cpp`

- [ ] **Step 1: Add flags field**

In `include/astra/npc.h`, inside the `struct Npc` body after the existing `NpcRole npc_role = NpcRole::Civilian;` line (around line 57):

```cpp
    uint64_t flags = 0;         // CreatureFlag bitfield (Mechanical, Biological, ...)
```

- [ ] **Step 2: Implement helper functions**

In `src/npc.cpp`, add at the top:

```cpp
#include "astra/creature_flags.h"
```

Add at end of `namespace astra` block (before the closing `}`):

```cpp
bool is_mechanical(const Npc& npc) {
    return has_flag(npc.flags, CreatureFlag::Mechanical);
}

bool is_biological(const Npc& npc) {
    return has_flag(npc.flags, CreatureFlag::Biological);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/npc.h src/npc.cpp
git commit -m "feat(npc): add flags bitfield and is_mechanical/is_biological helpers"
```

---

## Task 4: Persist Npc.flags in save file with version bump

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump save version**

In `include/astra/save_file.h`, find the line:

```cpp
    uint32_t version = 35;   // v35: QuestLocationMeta.target_moon_index
```

Change to:

```cpp
    uint32_t version = 36;   // v36: Npc.flags (CreatureFlag bitfield)
```

- [ ] **Step 2: Write flags in write_npc**

In `src/save_file.cpp`, inside `write_npc()`, at the end of the function (just before the final `}`, around line 665 after the interactions/quest block closes):

```cpp
    // v36: creature flags bitfield
    w.write_u64(npc.flags);
```

Note: If `BinaryWriter::write_u64` does not exist, add it next to the existing `write_u32` helper in the same file (search for `void write_u32` near the top of the file). Implementation:

```cpp
    void write_u64(uint64_t v) {
        out_.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
```

Mirror for `BinaryReader::read_u64`:

```cpp
    uint64_t read_u64() {
        uint64_t v = 0;
        in_.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }
```

- [ ] **Step 3: Read flags in read_npc**

In `src/save_file.cpp`, inside `read_npc()`, just before the final `return npc;` (around line 1468, after the pre-v25 faction fallback block):

```cpp
    // v36: creature flags bitfield
    if (version >= 36) {
        npc.flags = r.read_u64();
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Runtime verify save/load roundtrip**

Run: `./build/astra-dev`
- Start a new game.
- Save (F5 or menu).
- Quit and reload the save.
- Expected: no crash, game resumes; `npc.flags` silently defaults to `0` for any pre-existing NPC entry (acceptable — will be re-set by factories on spawn going forward).

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): persist Npc.flags bitfield (save v36)"
```

---

## Task 5: Tag existing organic NPCs as Biological

**Files:**
- Modify: `src/npcs/xytomorph.cpp`
- Modify: `src/npcs/drifter.cpp`
- Modify: `src/npcs/scavenger.cpp`
- Modify: `src/npcs/prospector.cpp`
- Modify: `src/npcs/pirate_captain.cpp`
- Modify: `src/npcs/pirate_grunt.cpp`
- Modify: `src/npcs/void_reaver.cpp`

- [ ] **Step 1: Tag each hostile organic NPC**

In each file above, add the include at top:

```cpp
#include "astra/creature_flags.h"
```

And inside the factory function, right before the final `return npc;`:

```cpp
    npc.flags |= static_cast<uint64_t>(CreatureFlag::Biological);
```

Skip non-hostile / friendly NPCs (merchants, keepers, civilians, commanders, hub NPCs, Nova) — they do not participate in combat so the flag is cosmetic; leave at default `0`.

Also skip `archon_remnant.cpp`, `archon_sentinel.cpp`, `conclave_sentry.cpp` — the Archon units are machines (plasma-hardened chassis per `archon_remnant.cpp:25`) and should stay flag-less for now. They are not part of this plan's mechanical-salvage loop.

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/npcs/xytomorph.cpp src/npcs/drifter.cpp src/npcs/scavenger.cpp src/npcs/prospector.cpp src/npcs/pirate_captain.cpp src/npcs/pirate_grunt.cpp src/npcs/void_reaver.cpp
git commit -m "feat(npc): tag organic hostiles with Biological flag"
```

---

## Task 6: Add NpcRole entries and dispatch for mechanical enemies

**Files:**
- Modify: `include/astra/npc.h`
- Modify: `include/astra/npc_defs.h`

- [ ] **Step 1: Add enum values**

In `include/astra/npc.h`, inside the `enum class NpcRole : uint8_t` block (ends at line 34 with `ConclaveSentry,`), append:

```cpp
    RustHound,
    SentryDrone,
    ArchonAutomaton,
```

- [ ] **Step 2: Declare builders**

In `include/astra/npc_defs.h`, after the existing `Npc build_conclave_sentry(std::mt19937& rng);` line (line 43):

```cpp
// Mechanical enemies
Npc build_rust_hound(std::mt19937& rng);
Npc build_sentry_drone(std::mt19937& rng);
Npc build_archon_automaton(std::mt19937& rng);
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: **will fail** — `create_npc` switch is non-exhaustive. We'll fix in Task 10.

Skip the expectation; proceed to Task 7.

- [ ] **Step 4: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h
git commit -m "feat(npc): declare RustHound/SentryDrone/ArchonAutomaton roles and builders"
```

---

## Task 7: Implement Rust Hound

**Files:**
- Create: `src/npcs/rust_hound.cpp`

- [ ] **Step 1: Write factory**

Create `src/npcs/rust_hound.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_rust_hound(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::RustHound;
    npc.role        = "Rust Hound";
    npc.name        = "Rust Hound";
    npc.hp          = 8;
    npc.max_hp      = 8;
    npc.faction     = Faction_Feral;   // feral scavenger drone; hostile to everyone
    npc.quickness   = 140;              // fast
    npc.base_xp     = 20;
    npc.base_damage = 1;
    npc.dv          = 11;
    npc.av          = 1;
    npc.damage_dice = Dice::make(1, 4);
    npc.damage_type = DamageType::Kinetic;
    // Hardened chassis: +2 kinetic resistance, weak to electrical.
    npc.type_affinity = {2, 0, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
```

- [ ] **Step 2: Build** — still won't link fully until all three exist + dispatch; defer.

- [ ] **Step 3: Commit**

```bash
git add src/npcs/rust_hound.cpp
git commit -m "feat(npc): add Rust Hound mechanical enemy"
```

---

## Task 8: Implement Sentry Drone

**Files:**
- Create: `src/npcs/sentry_drone.cpp`

- [ ] **Step 1: Write factory**

Create `src/npcs/sentry_drone.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_sentry_drone(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::SentryDrone;
    npc.role        = "Sentry Drone";
    npc.name        = "Sentry Drone";
    npc.hp          = 14;
    npc.max_hp      = 14;
    npc.faction     = Faction_Feral;
    npc.quickness   = 100;
    npc.base_xp     = 35;
    npc.base_damage = 2;
    npc.dv          = 10;
    npc.av          = 4;
    npc.damage_dice = Dice::make(1, 6);
    // TODO: ranged attack — ships as melee placeholder until ranged combat lands.
    npc.damage_type = DamageType::Plasma;
    npc.type_affinity = {1, 2, -3, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add src/npcs/sentry_drone.cpp
git commit -m "feat(npc): add Sentry Drone mechanical enemy (melee placeholder)"
```

---

## Task 9: Implement Archon Automaton

**Files:**
- Create: `src/npcs/archon_automaton.cpp`

- [ ] **Step 1: Write factory**

Create `src/npcs/archon_automaton.cpp`:

```cpp
#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_archon_automaton(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Human;   // chassis race; irrelevant for a machine
    npc.npc_role    = NpcRole::ArchonAutomaton;
    npc.role        = "Archon Automaton";
    npc.name        = "Archon Automaton";
    npc.hp          = 28;
    npc.max_hp      = 28;
    npc.faction     = Faction_ArchonRemnants;
    npc.quickness   = 80;               // heavy, slow
    npc.base_xp     = 70;
    npc.base_damage = 3;
    npc.dv          = 8;
    npc.av          = 6;
    npc.damage_dice = Dice::make(2, 6);
    npc.damage_type = DamageType::Plasma;
    // Archon chassis: hardened, plasma-hot. Brittle vs. kinetic.
    npc.type_affinity = {-2, 4, 0, 0, 0};
    npc.flags       = static_cast<uint64_t>(CreatureFlag::Mechanical);
    return npc;
}

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add src/npcs/archon_automaton.cpp
git commit -m "feat(npc): add Archon Automaton heavy mechanical enemy"
```

---

## Task 10: Wire dispatch, theme, and CMakeLists

**Files:**
- Modify: `src/npc.cpp`
- Modify: `src/terminal_theme.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add source files to CMake**

In `CMakeLists.txt`, after the existing `src/npcs/void_reaver.cpp` line (line 39):

```cmake
    src/npcs/rust_hound.cpp
    src/npcs/sentry_drone.cpp
    src/npcs/archon_automaton.cpp
```

- [ ] **Step 2: Dispatch in create_npc**

In `src/npc.cpp`, inside `create_npc()` after the `case NpcRole::ConclaveSentry:` line (line 120):

```cpp
        case NpcRole::RustHound:      return build_rust_hound(rng);
        case NpcRole::SentryDrone:    return build_sentry_drone(rng);
        case NpcRole::ArchonAutomaton: return build_archon_automaton(rng);
```

- [ ] **Step 3: Dispatch in create_npc_by_role**

In `src/npc.cpp`, inside `create_npc_by_role()` after the `"Conclave Sentry"` line (line 134):

```cpp
    if (role_name == "Rust Hound")       return create_npc(NpcRole::RustHound, Race::Human, rng);
    if (role_name == "Sentry Drone")     return create_npc(NpcRole::SentryDrone, Race::Human, rng);
    if (role_name == "Archon Automaton") return create_npc(NpcRole::ArchonAutomaton, Race::Human, rng);
```

- [ ] **Step 4: Terminal theme entries**

In `src/terminal_theme.cpp`, after the `case NpcRole::ConclaveSentry:` line (line 1109):

```cpp
        case NpcRole::RustHound:       return {'h', nullptr, Color::Yellow, Color::Default};
        case NpcRole::SentryDrone:     return {'d', nullptr, Color::Cyan, Color::Default};
        case NpcRole::ArchonAutomaton: return {'A', nullptr, Color::BrightRed, Color::Default};
```

- [ ] **Step 5: Regenerate build files and build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: clean build, no warnings.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/npc.cpp src/terminal_theme.cpp
git commit -m "feat(npc): wire mechanical enemies into dispatch, theme, build"
```

---

## Task 11: Salvage drop logic in game_combat.cpp

**Files:**
- Modify: `src/game_combat.cpp`

- [ ] **Step 1: Add includes and helper**

In `src/game_combat.cpp`, add to the existing include block at top:

```cpp
#include "astra/creature_flags.h"
#include "astra/item_defs.h"
#include "astra/skill_defs.h"
```

Directly after the existing `roll_d10` helper (around line 26), add a static salvage helper:

```cpp
static void apply_salvage_on_kill(Game& game, Npc& npc, std::mt19937& rng) {
    if (is_mechanical(npc)) {
        // Gated: requires Cat_Tinkering. Mechanical kills do NOT roll the
        // universal 5% floor-drop — machines have no flesh to scavenge.
        if (!player_has_skill(game.player(), SkillId::Cat_Tinkering)) return;

        if (std::uniform_int_distribution<int>(0, 99)(rng) >= 40) return;

        int spare_count = 1 + (std::uniform_int_distribution<int>(0, 1)(rng));
        Item spare = build_spare_parts();
        spare.stack_count = spare_count;
        game.player().inventory.items.push_back(spare);

        bool got_circuitry = std::uniform_int_distribution<int>(0, 99)(rng) < 30;
        Item circ;
        if (got_circuitry) {
            circ = build_circuitry();
            game.player().inventory.items.push_back(circ);
        }

        std::string msg = "You salvage " + display_name(spare) +
                          " from the " + npc.name + ".";
        if (got_circuitry) {
            msg = "You salvage " + display_name(spare) + " and " +
                  display_name(circ) + " from the " + npc.name + ".";
        }
        game.log(msg);
        return;
    }

    // Ungated universal path: 5% chance to drop Spare Parts to the ground.
    if (std::uniform_int_distribution<int>(0, 99)(rng) < 5) {
        Item spare = build_spare_parts();
        game.world().ground_items().push_back({npc.x, npc.y, std::move(spare)});
    }
}
```

- [ ] **Step 2: Call helper from melee kill path**

In `src/game_combat.cpp`, inside the melee-kill block (around line 413, right after the existing loot-drop-to-ground block that ends with `game.world().ground_items().push_back(...)`), add:

```cpp
        apply_salvage_on_kill(game, npc, rng);
```

It must land before the outer `}` that closes the dead-NPC branch. Exact insertion point: after the existing line `game.world().ground_items().push_back({npc.x, npc.y, std::move(loot)});` and its closing `}`.

- [ ] **Step 3: Call helper from ranged kill path**

Same file, inside the ranged-kill block (around line 652, after the existing `game.world().ground_items().push_back({target_npc_->x, target_npc_->y, std::move(loot)});` line, before `target_npc_ = nullptr;`):

```cpp
        apply_salvage_on_kill(game, *target_npc_, rng);
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(combat): salvage drops on kill (Cat_Tinkering gates mechanical loot)"
```

---

## Task 12: Add mechanical NPC to debug_spawn for verification

**Files:**
- Modify: `src/debug_spawn.cpp`

- [ ] **Step 1: Spawn a Rust Hound near the player in dev mode**

In `src/debug_spawn.cpp`, append after the existing Full-Xytomorph block (before the final `}` of `debug_spawn`):

```cpp
    // Rust Hound — mechanical enemy for salvage testing
    {
        Npc hound = create_npc(NpcRole::RustHound, Race::Human, rng);
        if (map.find_open_spot_other_room(player_x, player_y,
                                          hound.x, hound.y, occupied, &rng)) {
            occupied.push_back({hound.x, hound.y});
            npcs.push_back(std::move(hound));
        }
    }
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Runtime verification — ungated drop**

Run: `./build/astra-dev`
- Start a new character (do NOT spend SP on Tinkering).
- Kill Xytomorphs repeatedly in the starting room. Over ~20 kills you should see at least one "Spare Parts" dropped on the ground (5% rate).
- Expected: no salvage log message for Xytomorphs; ground drops only.

- [ ] **Step 4: Runtime verification — mechanical kill without skill**

Still in dev mode:
- Find and kill the Rust Hound (no Tinkering skill).
- Expected: **no** salvage message, **no** items added to inventory, **no** ground drop.

- [ ] **Step 5: Runtime verification — mechanical kill with skill**

- Use dev console or character creation to grant `Cat_Tinkering` (look in `src/dev_console.cpp` / `src/character_screen.cpp` for the skill-grant path; simplest: create a new character and purchase `Cat_Tinkering` for 100 SP during creation).
- Kill Rust Hounds (respawn via re-entering the map or dev spawn).
- Expected: on ~40% of kills, log shows colored `"You salvage Spare Parts from the Rust Hound."` (sometimes including `"and Circuitry"`), and inventory count increases by 1–2 Spare Parts (+ optional Circuitry).

If any of the above fail, do NOT proceed — return to the relevant task and fix.

- [ ] **Step 6: Commit**

```bash
git add src/debug_spawn.cpp
git commit -m "chore(dev): spawn Rust Hound in debug_spawn for salvage testing"
```

---

## Task 13: Final whole-feature verification

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build`
Expected: clean build from scratch, no warnings.

- [ ] **Step 2: Save/load roundtrip with mechanical NPCs alive**

- Start a new game, kill one Xytomorph (leaving a Rust Hound alive on the map).
- Save (F5 or menu).
- Quit, reload the save.
- Expected: Rust Hound still on map, still attacks, still marked mechanical (kill it and verify — without skill: no salvage; with skill: salvage works).

- [ ] **Step 3: Visual inspection**

- Rust Hound glyph = `'h'` yellow
- Sentry Drone glyph = `'d'` cyan (use dev console / map editor to spawn)
- Archon Automaton glyph = `'A'` bright red (use dev console / map editor to spawn)
- Salvage log message renders colored item names (Spare Parts in yellow, Circuitry in cyan) matching theme entries from Task 1.

- [ ] **Step 4: Update roadmap**

In `docs/roadmap.md`, add a checked entry under the appropriate section:

```markdown
- [x] Tinkering salvage system (2026-04-21) — Spare Parts / Circuitry, mechanical enemies, `Cat_Tinkering`-gated auto-salvage
```

(Find the most appropriate section by reading the file first; if unclear, append under a "Combat / Items" section or the newest dated entry.)

- [ ] **Step 5: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs: mark tinkering salvage feature complete"
```

---

## Self-Review Notes

- **Spec coverage:** items (Task 1), creature flags (Tasks 2–3, 5), save persistence (Task 4), three mechanical NPCs (Tasks 6–10), drop logic (Task 11), verification (Tasks 12–13) — all spec sections covered.
- **Item IDs:** spec originally said 33/34; corrected to 47/48 in the spec during planning because 33/34 are already occupied. All references here use 47/48.
- **Ranged combat:** Sentry Drone ships as melee with a `TODO` comment per the spec's "deferred" note.
- **Save version:** bumped 35 → 36 in one atomic task; legacy saves default `flags = 0` which is harmless (no gameplay gating depends on Biological yet).
- **Test framework:** project has no unit tests; verification is runtime via dev mode. Each task ends with a build check, and Task 12 is the dedicated salvage runtime test.
