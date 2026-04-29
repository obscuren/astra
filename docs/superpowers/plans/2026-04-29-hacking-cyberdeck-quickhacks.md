# Hacking — Cyberdeck + Quickhacks (B-layer) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the B-layer of the Hacking feature: Cyberdeck and Program items, a `Hackable` trait, the bounded-terminal PDA Hacking tab, and an in-world quickhack flow with Detection. The "Jack In" verb on Precursor consoles is intentionally a stub returning "Grid not yet implemented" — the A-layer (Plan 3) fills that in.

**Architecture:** Three new game-data types live behind small headers (`hackable.h`, `cyberdeck.h`, `program.h`). The `Hackable` trait is an optional field on `FixtureData` and `Npc`. Cyberdeck is a new `ItemType::Cyberdeck` filling a new `EquipSlot::Cyberdeck` slot, with loaded programs stored as inventory items. A new `HackingSystem` coordinator owns the H-key targeting flow + per-zone Detection counter and dispatches quickhack effects via a small lookup table. The PDA Hacking tab renders a fake terminal with a hard-bounded command set (10 commands, every command also bound to a single-key shortcut as menu fallback).

**Tech Stack:** C++20, CMake. Same conventions as Plan 1: `astra::` namespace, `snake_case_` member vars, `#pragma once` headers in `include/astra/`. Build with `-DDEV=ON`. No backcompat shims — bump `SAVE_FILE_VERSION`.

**Design references:**
- Spec: `docs/superpowers/specs/2026-04-29-hacking-design.md` (read sections §2, §3, §4, §6 v1 cut)
- Plan 1: `docs/superpowers/plans/2026-04-29-hacking-pda-refactor.md` (foundation that this builds on)

**Notes for the implementer:**
- Astra has no unit-test infrastructure; "verify" means the project builds clean **and** the named feature behaves as described when run with `./build/astra-dev`. Each task's last step is a build check + a manual smoke check, then a commit.
- clangd often produces false-positive "undeclared identifier" / "unused-include" warnings as files are added. The cmake build is authoritative — if `cmake --build build -j` succeeds, ignore clangd diagnostics.
- Always build with `-DDEV=ON` (already configured in the worktree).
- After every task: build, run `./build/astra-dev` briefly, commit with a focused message.

---

## File map (decomposition decisions)

### New headers (`include/astra/`)

| Header | Responsibility | Approx. size |
|---|---|---|
| `hackable.h` | `Hackable` trait struct, `DeviceKind` enum, `HackState` enum. ~50 lines. |
| `cyberdeck.h` | `Cyberdeck` data struct (deck stats: ram_max, cpu, slots, stealth, cooling_rate, heat_cap), program-slot helper API. ~60 lines. |
| `program.h` | `Program` data struct (kind, tier, costs, effect_id, target_filter), `ProgramKind` enum, `ProgramId` enum. ~80 lines. |
| `hacking_system.h` | `HackingSystem` class — quickhack targeting, Detection counter, effect dispatch. ~80 lines. |
| `program_effects.h` | `apply_program_effect(ProgramId, Game&, Hackable&)` — single dispatch entry. ~20 lines. |

### New source files (`src/`)

| Source | Responsibility |
|---|---|
| `hackable.cpp` | Default-state factory `make_hackable(DeviceKind, int tier)`. ~50 lines. |
| `cyberdeck.cpp` | Tier presets + helper `loaded_program_at(deck, slot)`. ~80 lines. |
| `program.cpp` | Program registry + `find_program(ProgramId)` lookup. ~150 lines (8 programs × ~15 lines). |
| `hacking_system.cpp` | Targeting state, detection decay, quickhack execution, save serialization helpers. ~250 lines. |
| `program_effects.cpp` | Dispatch table for the 3 active QH effects (`reboot_optics`, `friendly_fire`, `data_leech`). ~120 lines. |

### Modified existing files

| File | Why |
|---|---|
| `include/astra/skill_defs.h` | Add `Cat_Hacking = 12` + 8 unlock IDs. |
| `src/skill_defs.cpp` | Register Cat_Hacking category in catalog (descriptions, sp_cost, attribute reqs). |
| `include/astra/item.h` | Add `ItemType::Cyberdeck`, `ItemType::Program`; `EquipSlot::Cyberdeck` + slot_count bump; `Equipment::cyberdeck`; cyberdeck/program data fields on `Item`. |
| `src/item.cpp` | Map new EquipSlot in `slot_ref()` + `equip_slot_name()`. |
| `include/astra/item_ids.h` | Add ID constants for cyberdecks (300-301), programs (310-317), code fragments (320-322). |
| `include/astra/item_defs.h` + `src/item_defs.cpp` | Builder declarations + implementations + `build_by_def_id()` switch entries. |
| `include/astra/loot_source.h` | Add `Category::Cyberdeck`, `Category::Program`, `Category::CodeFragment`. |
| `src/loot_table.cpp` | Loot entries for cyberdecks, programs, code fragments. |
| `src/tinkering.cpp` | Add code-fragment material defs + 3 schematic recipes. |
| `include/astra/tilemap.h` + `src/tilemap.cpp` | Add `Hackable` cyber field to `FixtureData`. |
| `include/astra/npc.h` | Add `Hackable` cyber field to `Npc`. |
| `include/astra/game.h` | Own `HackingSystem hacking_;`. Detection-by-zone storage. |
| `src/game.cpp` | Wire up Detection per-zone state on map enter; tick decay. |
| `src/game_input.cpp` | Bind `H` key for quickhack targeting; add quickhack-mode key dispatch. |
| `src/game_rendering.cpp` | Draw quickhack target cursor + side panel. |
| `src/game_interaction.cpp` | When interacting with a fixture that has Hackable, append QH-menu and (for Precursor consoles) "Jack In" stub. |
| `src/pda_hacking_tab.cpp` | Replace stub with full terminal subwindow + locked splash. |
| `include/astra/pda_screen.h` | Hacking-tab state members (terminal scrollback, command history, cursor). |
| `src/pda_screen.cpp` | Update `tab_help_body(PdaTab::Hacking)` with real help. |
| `include/astra/save_file.h` | Bump `SAVE_FILE_VERSION` (51 → 52). New fields: `MapState::detection`, `MapState::hackable_overrides[]`, `Player::cyberdeck_state` (deck currently equipped is in `Equipment::cyberdeck` already; loaded programs persist with deck). |
| `src/save_file.cpp` | Serialize/deserialize new fields. |
| `src/dev_console.cpp` | New verbs: `spawn-hackable <kind>`, `give cyberdeck <tier>`, `give program <id>`, `detection <n>` (set zone counter for testing). |
| `docs/items.md` | Cyberdeck + Program item families. |
| `docs/mechanics.md` | Detection formula, Heat→Trace coupling note (forward reference). |
| `docs/roadmap.md` | Tick the box for Plan 2. |
| `CMakeLists.txt` | Add new `.cpp` files. |

---

## Task list

| # | Task | Touches |
|---|---|---|
| 1 | `Cat_Hacking` skill enum + catalog entry (8 unlocks, descriptions, no runtime effects yet) | skill_defs.{h,cpp} |
| 2 | `Hackable` component header + struct on `FixtureData` and `Npc` | hackable.h/cpp, tilemap.h, npc.h |
| 3 | `EquipSlot::Cyberdeck` + `ItemType::Cyberdeck` + `Cyberdeck` data + 2 builders | item.h/cpp, item_ids.h, item_defs.h/cpp, save_file.cpp |
| 4 | `ItemType::Program` + `Program` data + 8 builders + program registry | program.h/cpp, item_ids.h, item_defs.h/cpp, item.h |
| 5 | Code-fragment material category + 3 program schematic recipes | tinkering.cpp, item_ids.h, item_defs.cpp, item_gen helpers |
| 6 | Loot entries for cyberdecks, programs, code fragments | loot_source.h, loot_table.cpp |
| 7 | `HackingSystem` skeleton + per-zone Detection counter + decay | hacking_system.h/cpp, game.h, game.cpp |
| 8 | Quickhack targeting flow on `H` key + side panel + popup | game_input.cpp, game_rendering.cpp, hacking_system.cpp |
| 9 | Three QH program effects (`reboot_optics`, `friendly_fire`, `data_leech`) | program_effects.h/cpp |
| 10 | Hackable interaction integration: QH menu + "Jack In" stub on Precursor consoles | game_interaction.cpp, dialog_manager.cpp |
| 11 | PDA Hacking tab — locked splash + terminal subwindow + bounded commands | pda_screen.h, pda_hacking_tab.cpp, pda_screen.cpp |
| 12 | Detection ↔ reputation coupling (≥50 investigate, ≥75 rep hit, =100 alarm) | hacking_system.cpp, faction.cpp |
| 13 | Save schema bump (51→52); persist Hackable state, Detection per zone, deck loadout | save_file.h/cpp |
| 14 | Dev console aids + docs (items.md, mechanics.md, roadmap.md) | dev_console.cpp, docs/ |

---

## Task 1 — `Cat_Hacking` skill category

**Files:**
- Modify: `include/astra/skill_defs.h`
- Modify: `src/skill_defs.cpp`

Plan 1 already added the empty `Hacking` PDA tab. This task adds the skill category itself — full enum, descriptions, registered in `skill_catalog()`. None of the unlocks have runtime effects yet (those land in Plans 3 & 4). `Cat_Hacking` is the gate that later tasks check for the "Jack In" verb stub.

- [ ] **Step 1: Add skill enum entries**

In `include/astra/skill_defs.h`, after the `Cat_Cooking` block (around line 73), append:

```cpp
    // Hacking
    Cat_Hacking         = 12,    // gate for jacking into the Grid (Plan 3+)
    Intrusion           = 1200,  // -1 trace per noisy action (Plan 3)
    IceBreaking         = 1201,  // +1 dmg vs ICE (Plan 3)
    DaemonMastery       = 1202,  // +1 deck slot (Plan 3)
    GhostProtocol       = 1203,  // first program each Grid run is heatless (Plan 3)
    DeepGridNavigator   = 1204,  // gateway crack chance + map reveal (Plan 3)
    NeuralFortitude     = 1205,  // halve black-ICE bleed-through (Plan 3)
    CodeCraft           = 1206,  // unlock T3 program tinker recipes (Plan 4)
    ConsciousnessAnchor = 1207,  // (capstone) full deep-Grid persistence (Plan 4)
```

- [ ] **Step 2: Add description helpers**

In `src/skill_defs.cpp`, after `advanced_fire_making_description()` (search for it; it's near the bottom of the description block before `skill_catalog()`), insert:

```cpp
static std::string hacking_category_description() {
    std::string s = "Penetration of electronic systems and the discipline to walk in cyberspace as a second body.\n\n";
    s += colored("Passive:", Color::White);
    s += " Required to ";
    s += colored("Jack In", Color::Cyan);
    s += " at compatible Precursor consoles. Quickhacks against in-world devices ";
    s += "do ";
    s += colored("not", Color::Yellow);
    s += " require this category — only an equipped cyberdeck.";
    return s;
}

static std::string intrusion_description() {
    return "Light-footed cyber-intrusion technique. Reduces the trace your actions leave.\n\n"
           + colored("Passive:", Color::White)
           + " -1 Trace per noisy action while in the Grid. (Plan 3.)";
}
static std::string ice_breaking_description() {
    return "Cracking discipline against hostile counter-intrusion software.\n\n"
           + colored("Passive:", Color::White)
           + " +1 damage vs ICE while in the Grid. (Plan 3.)";
}
static std::string daemon_mastery_description() {
    return "Compiled program slot expansion via long-running background processes.\n\n"
           + colored("Passive:", Color::White)
           + " +1 cyberdeck slot. (Plan 3.)";
}
static std::string ghost_protocol_description() {
    return "Pre-launch heat-sink protocol that absorbs the first program of each session.\n\n"
           + colored("Passive:", Color::White)
           + " Your first program each Grid run is heatless. (Plan 3.)";
}
static std::string deep_grid_navigator_description() {
    return "Pathing intuition for the deep-Grid lattice. Cracks gateways and maps unfamiliar nets.\n\n"
           + colored("Passive:", Color::White)
           + " +25% gateway crack chance and reveal of one tier-up map node per session. (Plan 3.)";
}
static std::string neural_fortitude_description() {
    return "Disciplined dissociation. The body bleeds less when the avatar is hurt.\n\n"
           + colored("Passive:", Color::White)
           + " Halve black-ICE bleed-through to real HP. (Plan 3.)";
}
static std::string code_craft_description() {
    return "Personal toolchain for compiling exotic programs from raw fragments.\n\n"
           + colored("Passive:", Color::White)
           + " Unlocks T3 program tinkering recipes. (Plan 4.)";
}
static std::string consciousness_anchor_description() {
    std::string s = "(Capstone.) The full anchoring rite. Your selfhood persists past the rebirth.\n\n";
    s += colored("Passive:", Color::White);
    s += " Full deep-Grid persistence: base, programs, AI rep, currency, and lore archive ";
    s += "carry across Sgr A*. (Plan 4.)";
    return s;
}
```

- [ ] **Step 3: Register the category in `skill_catalog()`**

In `src/skill_defs.cpp`, inside `skill_catalog()` after the `Cat_Cooking` entry (the last existing category, around line 537-543), append:

```cpp
        {SkillId::Cat_Hacking, "Hacking",
         hacking_category_description(), 100, {
            {SkillId::Intrusion, "Intrusion",
             intrusion_description(),
             true, 100, 14, "Intelligence"},
            {SkillId::IceBreaking, "Ice Breaking",
             ice_breaking_description(),
             true, 100, 13, "Intelligence"},
            {SkillId::DaemonMastery, "Daemon Mastery",
             daemon_mastery_description(),
             true, 100, 14, "Intelligence"},
            {SkillId::GhostProtocol, "Ghost Protocol",
             ghost_protocol_description(),
             true, 150, 15, "Willpower"},
            {SkillId::DeepGridNavigator, "Deep-Grid Navigator",
             deep_grid_navigator_description(),
             true, 150, 15, "Intelligence"},
            {SkillId::NeuralFortitude, "Neural Fortitude",
             neural_fortitude_description(),
             true, 200, 16, "Willpower"},
            {SkillId::CodeCraft, "Code Craft",
             code_craft_description(),
             true, 200, 16, "Intelligence"},
            {SkillId::ConsciousnessAnchor, "Consciousness Anchor",
             consciousness_anchor_description(),
             true, 300, 18, "Willpower"},
         }},
```

- [ ] **Step 4: Build**

```
cmake --build build -j
```
Expected: clean build (warnings about unused unhandled enum cases in unrelated switches are fine — those are pre-existing).

- [ ] **Step 5: Smoke check**

Run `./build/astra-dev`. Open PDA (`Tab`), navigate to the Skills tab, scroll to `Hacking` category. Expand it (`Space`) and verify all 8 sub-skills are listed with descriptions.

- [ ] **Step 6: Commit**

```
git add include/astra/skill_defs.h src/skill_defs.cpp
git commit -m "feat(skills): add Cat_Hacking category and 8 unlock skills

Declarative-only — none of the unlocks have runtime effects yet (those
land with the Grid in Plan 3 and persistence in Plan 4). Cat_Hacking
itself is the gate for the future Jack In verb."
```

---

## Task 2 — `Hackable` component

**Files:**
- Create: `include/astra/hackable.h`
- Create: `src/hackable.cpp`
- Modify: `include/astra/tilemap.h` (add `cyber` field on `FixtureData`)
- Modify: `include/astra/npc.h` (add `cyber` field on `Npc`)
- Modify: `CMakeLists.txt`

The `Hackable` trait is a tiny POD struct attached optionally to fixtures and NPCs. It carries enough info for the QH targeting flow to pick valid programs and for the effect handlers to mutate state. We do NOT add new `FixtureType` enum entries — Plan 2 reuses existing `FixtureType::Console`, `FixtureType::Door`, and `FixtureType::Conduit` and identifies the *device kind* via this trait, not the tile.

- [ ] **Step 1: Create `include/astra/hackable.h`**

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace astra {

// Kind of hackable device. Identifies which quickhack target_filter set
// applies and which interaction effects fire. Decoupled from FixtureType:
// a security camera and a console may render the same glyph, but their
// device_kind differs.
enum class DeviceKind : uint8_t {
    Turret,
    Camera,
    Door,
    PowerConduit,
    PrecursorConsole,
    // Future (Plan 3+): Drone, MineTrap, Vendor, Light, Elevator,
    // Hazard, NpcImplant, ShipSystem, ReputationServer, Wreckage.
};

const char* device_kind_name(DeviceKind k);

enum class HackState : uint8_t {
    Clean,         // never been hacked
    Compromised,   // at least one QH applied; effect timer running
    Alarmed,       // detected; broadcasts to faction
};

// Forward-declared in headers that don't need ProgramId (program.h includes hackable.h).
enum class ProgramId : uint16_t;

struct Hackable {
    DeviceKind device_kind = DeviceKind::Turret;
    int security_tier = 1;        // 1..3 — gates QH/jack-in availability
    uint32_t network_id = 0;      // 0 = unwired (subnet of one)
    HackState state = HackState::Clean;

    // Program ids that are valid against this device.
    // Filled by make_hackable() based on device_kind.
    std::vector<ProgramId> available_qh;

    // For PrecursorConsole only — Plan 3 will use this; Plan 2 stubs the verb.
    int jack_in_node_id = -1;

    // Compromised-state cooldown timer in ticks. Decremented per game tick;
    // when it hits 0 the state collapses back to Clean (or to Alarmed if a
    // detection event flagged it).
    int state_ticks_left = 0;
};

// Default-constructs a Hackable with device-appropriate available_qh
// programs filled in. Use this everywhere a Hackable is added to a
// fixture or NPC — never hand-fill the available_qh list.
Hackable make_hackable(DeviceKind kind, int tier);

} // namespace astra
```

- [ ] **Step 2: Create `src/hackable.cpp`**

```cpp
#include "astra/hackable.h"
#include "astra/program.h"

namespace astra {

const char* device_kind_name(DeviceKind k) {
    switch (k) {
        case DeviceKind::Turret:           return "Turret";
        case DeviceKind::Camera:           return "Camera";
        case DeviceKind::Door:             return "Door";
        case DeviceKind::PowerConduit:     return "Power Conduit";
        case DeviceKind::PrecursorConsole: return "Precursor Console";
    }
    return "?";
}

Hackable make_hackable(DeviceKind kind, int tier) {
    Hackable h;
    h.device_kind = kind;
    h.security_tier = tier;
    switch (kind) {
        case DeviceKind::Turret:
            h.available_qh = { ProgramId::RebootOptics, ProgramId::FriendlyFire };
            break;
        case DeviceKind::Camera:
            h.available_qh = { ProgramId::RebootOptics };
            break;
        case DeviceKind::Door:
            h.available_qh = { /* future: bypass_lock — Plan 3 */ };
            break;
        case DeviceKind::PowerConduit:
            h.available_qh = { /* future: blackout — Plan 3 */ };
            break;
        case DeviceKind::PrecursorConsole:
            h.available_qh = {}; // jack-in only
            break;
    }
    return h;
}

} // namespace astra
```

- [ ] **Step 3: Add the cyber field to `FixtureData`**

In `include/astra/tilemap.h`, add `#include "astra/hackable.h"` near the top (after other astra includes). Then in the `FixtureData` struct (around line 449-471), before the closing brace, add:

```cpp
    // Optional cyber trait — present when this fixture is hackable.
    // std::optional kept out of the header dependency surface; sentinel via
    // a heap pointer would also work, but FixtureData is value-typed in the
    // tilemap so we keep it inline.
    std::optional<Hackable> cyber;
```

This requires `#include <optional>` — confirm it's already present (`grep -n "include <optional>" include/astra/tilemap.h`); add it if not.

- [ ] **Step 4: Add the cyber field to `Npc`**

In `include/astra/npc.h`, add `#include "astra/hackable.h"` and `#include <optional>` near the top, then in the `Npc` struct (after the `flags` and `interactions` fields, around line 79), insert:

```cpp
    std::optional<Hackable> cyber;       // present iff this NPC is hackable
```

- [ ] **Step 5: Wire CMakeLists**

In `CMakeLists.txt`, find the list of `src/*.cpp` files and add `src/hackable.cpp` alongside the others (alphabetical order — between `src/grenade.cpp` and `src/help_screen.cpp` if alphabetised, or wherever it fits the existing pattern).

- [ ] **Step 6: Build**

```
cmake --build build -j
```
Expected: clean build. (`hacking_system.h`, `program.h` aren't introduced yet — `hackable.cpp` includes `program.h` which is created in Task 4. **Move forward expecting this task to fail to compile in isolation; it'll build clean after Task 4 lands.** To still verify Task 2's body is well-formed, swap the `#include "astra/program.h"` for a forward declaration `enum class ProgramId : uint16_t { RebootOptics = 0, FriendlyFire = 1, DataLeech = 2, _Count };` in `src/hackable.cpp` *temporarily* — Task 4 will replace it with the real include. Use the temporary form to make this task commit-buildable.)

- [ ] **Step 7: Commit**

```
git add include/astra/hackable.h src/hackable.cpp \
        include/astra/tilemap.h include/astra/npc.h \
        CMakeLists.txt
git commit -m "feat(hacking): introduce Hackable trait and DeviceKind

Optional cyber field on FixtureData and Npc. make_hackable() factory
fills device-appropriate available_qh program lists. ProgramId is
forward-declared until program.h lands in Task 4."
```

---

## Task 3 — Cyberdeck item type + equipment slot

**Files:**
- Modify: `include/astra/item.h`
- Modify: `src/item.cpp`
- Modify: `include/astra/item_ids.h`
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp`
- Create: `include/astra/cyberdeck.h`
- Create: `src/cyberdeck.cpp`
- Modify: `CMakeLists.txt`

Cyberdeck is a new `ItemType::Cyberdeck` that goes in a new `EquipSlot::Cyberdeck` slot. Its program loadout is stored as an array of `Item` (programs) inside the deck's data. v1 ships two tiers: T1 "Pidgin Mark I" (pawn-shop) and T2 "Polyglot DCK-2" (corp surplus).

- [ ] **Step 1: Create `include/astra/cyberdeck.h`**

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace astra {

struct Item;  // forward — programs are stored as Items

// Per-deck stats.
struct CyberdeckStats {
    int  ram_max       = 4;
    int  cpu           = 1;
    int  slots         = 3;       // number of program slots
    int  stealth       = 0;       // additive bonus to Trace reduction (Plan 3)
    int  cooling_rate  = 1;       // heat decay per turn (Plan 3)
    int  heat_cap      = 10;      // max heat (Plan 3)
};

// We cap loaded programs at 6 to keep the deck struct trivially copyable.
// Slot count from CyberdeckStats::slots gates how many of these are live.
inline constexpr int kCyberdeckMaxSlots = 6;

struct CyberdeckData {
    CyberdeckStats stats;
    int  ram_current = 0;          // current RAM available (regenerates between sessions)
    int  heat_current = 0;         // Plan 3
    // Loaded programs; nullopt = empty slot. Index < stats.slots is live.
    std::array<std::optional<Item>, kCyberdeckMaxSlots> loaded;
};

// Tier presets.
CyberdeckStats cyberdeck_stats_tier1();
CyberdeckStats cyberdeck_stats_tier2();

} // namespace astra
```

- [ ] **Step 2: Create `src/cyberdeck.cpp`**

```cpp
#include "astra/cyberdeck.h"

namespace astra {

CyberdeckStats cyberdeck_stats_tier1() {
    CyberdeckStats s;
    s.ram_max      = 4;
    s.cpu          = 1;
    s.slots        = 3;
    s.stealth      = 0;
    s.cooling_rate = 1;
    s.heat_cap     = 10;
    return s;
}

CyberdeckStats cyberdeck_stats_tier2() {
    CyberdeckStats s;
    s.ram_max      = 8;
    s.cpu          = 2;
    s.slots        = 4;
    s.stealth      = 1;
    s.cooling_rate = 1;
    s.heat_cap     = 12;
    return s;
}

} // namespace astra
```

- [ ] **Step 3: Add `EquipSlot::Cyberdeck` and `ItemType::Cyberdeck`**

In `include/astra/item.h`:

(a) Add a new entry to the `ItemType` enum (after `Turret`):
```cpp
    Cyberdeck,    // hacking deck — held in EquipSlot::Cyberdeck
    Program,      // .exe / .qh loadable into a cyberdeck slot
```

(b) Add to the `EquipSlot` enum (after `Shield`):
```cpp
    Cyberdeck,
```

(c) Bump `equip_slot_count`:
```cpp
static constexpr int equip_slot_count = 13;
```

(d) Add an optional cyberdeck field to `Equipment` (after `shield`):
```cpp
    std::optional<Item> cyberdeck;
```

(e) Add a top-level field on `Item` for the cyberdeck data payload:
```cpp
    // Cyberdeck payload — non-empty only when type == ItemType::Cyberdeck.
    // Holds RAM/heat/slot state and currently loaded programs.
    std::optional<struct CyberdeckData> deck;
```

This requires forward-declaring or including `cyberdeck.h`. Add `#include "astra/cyberdeck.h"` to the top of `item.h` (same pattern as the existing `astra/aura_grant.h` include).

- [ ] **Step 4: Update `src/item.cpp` slot helpers**

In `src/item.cpp`, find `Equipment::slot_ref` and the `equip_slot_name` function. Add:

```cpp
// in slot_ref:
        case EquipSlot::Cyberdeck: return cyberdeck;
```
(both const and non-const versions)

```cpp
// in equip_slot_name:
        case EquipSlot::Cyberdeck: return "Cyberdeck";
```

Also, add to `item_type_name`:
```cpp
        case ItemType::Cyberdeck: return "Cyberdeck";
        case ItemType::Program:   return "Program";
```

- [ ] **Step 5: Add item IDs**

In `include/astra/item_ids.h`, after the synthesized items block (around line 213), append:

```cpp
// Cyberdecks (300-301)
constexpr uint16_t ITEM_PIDGIN_MK1     = 300;   // T1 cyberdeck
constexpr uint16_t ITEM_POLYGLOT_DCK2  = 301;   // T2 cyberdeck

// Programs (310-317)
constexpr uint16_t ITEM_PROG_ICEBREAKER_LITE = 310;
constexpr uint16_t ITEM_PROG_GHOST_TRACE     = 311;
constexpr uint16_t ITEM_PROG_COOLDOWN        = 312;
constexpr uint16_t ITEM_PROG_BREACH          = 313;
constexpr uint16_t ITEM_PROG_DECRYPT         = 314;
constexpr uint16_t ITEM_PROG_REBOOT_OPTICS   = 315;
constexpr uint16_t ITEM_PROG_FRIENDLY_FIRE   = 316;
constexpr uint16_t ITEM_PROG_DATA_LEECH      = 317;

// Code fragments (320-322)
constexpr uint16_t ITEM_CODE_FRAGMENT_T1     = 320;
constexpr uint16_t ITEM_CODE_FRAGMENT_T2     = 321;
constexpr uint16_t ITEM_CODE_FRAGMENT_T3     = 322;
```

- [ ] **Step 6: Add cyberdeck builders**

In `include/astra/item_defs.h`, before the closing `} // namespace astra`, declare:

```cpp
// --- Cyberdecks ---
Item build_pidgin_mk1();
Item build_polyglot_dck2();
```

In `src/item_defs.cpp`, add a new `// --- Cyberdecks ---` section (place near armor or accessories — alphabetical doesn't matter, follow nearby convention):

```cpp
// ---------------------------------------------------------------------------
// Cyberdecks
// ---------------------------------------------------------------------------

Item build_pidgin_mk1() {
    Item it;
    it.item_def_id = ITEM_PIDGIN_MK1;
    it.id = 9000; it.name = "Pidgin Mark I"; it.type = ItemType::Cyberdeck;
    it.description = "A pawn-shop deck. Three slots, four RAM. Chunky but it boots.";
    it.slot = EquipSlot::Cyberdeck; it.rarity = Rarity::Common;
    it.weight = 2;
    it.stackable = false; it.buy_value = 250; it.sell_value = 80;
    CyberdeckData d;
    d.stats = cyberdeck_stats_tier1();
    d.ram_current = d.stats.ram_max;
    it.deck = std::move(d);
    return it;
}

Item build_polyglot_dck2() {
    Item it;
    it.item_def_id = ITEM_POLYGLOT_DCK2;
    it.id = 9001; it.name = "Polyglot DCK-2"; it.type = ItemType::Cyberdeck;
    it.description = "Corp surplus. Cleaner thermal envelope, four slots, eight RAM.";
    it.slot = EquipSlot::Cyberdeck; it.rarity = Rarity::Uncommon;
    it.weight = 2;
    it.stackable = false; it.buy_value = 600; it.sell_value = 200;
    CyberdeckData d;
    d.stats = cyberdeck_stats_tier2();
    d.ram_current = d.stats.ram_max;
    it.deck = std::move(d);
    return it;
}
```

- [ ] **Step 7: Wire `build_by_def_id` dispatch**

In `src/item_defs.cpp`, find `Item build_by_def_id(uint16_t def_id)` (line 1351) and inside its switch add:

```cpp
        case ITEM_PIDGIN_MK1:               return build_pidgin_mk1();
        case ITEM_POLYGLOT_DCK2:            return build_polyglot_dck2();
```

- [ ] **Step 8: Update CMakeLists**

Add `src/cyberdeck.cpp` to the source list in `CMakeLists.txt` (alphabetical; near `src/cooking_*.cpp` or wherever fits).

- [ ] **Step 9: Build**

```
cmake --build build -j
```
Expected: clean build (with the same caveat about Task 2's `program.h` placeholder still in place).

- [ ] **Step 10: Smoke check**

Run `./build/astra-dev`, open the dev console (`` ` ``), type `give item pidgin_mk1` (loot identifier — registered in Task 6, but for now use the giver's other path: `give item 300` if the dev console supports raw def_id; otherwise spawn after Task 6 is in). If `give item` doesn't accept raw IDs yet, defer the smoke check to Task 6.

Open the PDA → Equipment tab. Verify there is a new `Cyberdeck` paper-doll slot below the existing slots (it'll show as empty for now — this is expected; the paper-doll layout extension is not in scope for Plan 2 unless the existing layout breaks. If it does break, fix it in this task).

- [ ] **Step 11: Commit**

```
git add include/astra/item.h src/item.cpp \
        include/astra/cyberdeck.h src/cyberdeck.cpp \
        include/astra/item_ids.h \
        include/astra/item_defs.h src/item_defs.cpp \
        CMakeLists.txt
git commit -m "feat(hacking): add Cyberdeck item type + equipment slot

ItemType::Cyberdeck + EquipSlot::Cyberdeck. Two tiers: Pidgin Mk I (T1)
and Polyglot DCK-2 (T2). Loaded programs and per-deck RAM/heat live in
Item::deck (CyberdeckData). build_by_def_id wired."
```

---

## Task 4 — Program item type + 8 starter programs

**Files:**
- Create: `include/astra/program.h`
- Create: `src/program.cpp`
- Modify: `include/astra/item.h` (Program data on Item)
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp`
- Modify: `src/hackable.cpp` (drop the temp forward-decl from Task 2 in favor of the real `program.h` include)
- Modify: `CMakeLists.txt`

Programs are loadable items. Each one declares a kind (ATK/STL/UTL/QH), tier, RAM/heat costs, and an effect id. v1 ships 8 programs but only the 3 quickhacks have active effects in Plan 2.

- [ ] **Step 1: Create `include/astra/program.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class DeviceKind : uint8_t;

enum class ProgramKind : uint8_t {
    Atk,   // .exe — Grid combat (Plan 3)
    Stl,   // .exe — Stealth/cooldown (Plan 3)
    Utl,   // .exe — Utility (Plan 3)
    Qh,    // .qh — real-world quickhack (Plan 2 active)
};

const char* program_kind_name(ProgramKind k);
const char* program_kind_short(ProgramKind k);  // "ATK", "STL", "UTL", "QH"

// Stable per-program id. Used by effect dispatch and save format.
enum class ProgramId : uint16_t {
    IcebreakerLite = 1,
    GhostTrace     = 2,
    Cooldown       = 3,
    Breach         = 4,
    Decrypt        = 5,
    RebootOptics   = 100,
    FriendlyFire   = 101,
    DataLeech      = 102,
};

struct ProgramDef {
    ProgramId           id;
    ProgramKind         kind;
    int                 tier        = 1;
    int                 ram_cost    = 1;
    int                 heat_cost   = 0;
    const char*         name        = "";          // canonical display name
    const char*         filename    = "";          // "icebreaker_lite.exe" / "reboot_optics.qh"
    const char*         description = "";
    int                 detection_cost = 1;        // QH only: amount added to zone Detection
    std::vector<DeviceKind> target_filter;         // QH only: which device_kinds it can target
};

const std::vector<ProgramDef>& program_registry();
const ProgramDef* find_program(ProgramId id);

// Per-Item payload — populated only when Item::type == ItemType::Program.
struct ProgramData {
    ProgramId id = ProgramId::IcebreakerLite;
};

} // namespace astra
```

- [ ] **Step 2: Create `src/program.cpp`**

```cpp
#include "astra/program.h"
#include "astra/hackable.h"

namespace astra {

const char* program_kind_name(ProgramKind k) {
    switch (k) {
        case ProgramKind::Atk: return "Attack";
        case ProgramKind::Stl: return "Stealth";
        case ProgramKind::Utl: return "Utility";
        case ProgramKind::Qh:  return "Quickhack";
    }
    return "?";
}
const char* program_kind_short(ProgramKind k) {
    switch (k) {
        case ProgramKind::Atk: return "ATK";
        case ProgramKind::Stl: return "STL";
        case ProgramKind::Utl: return "UTL";
        case ProgramKind::Qh:  return "QH";
    }
    return "?";
}

const std::vector<ProgramDef>& program_registry() {
    using K = ProgramKind;
    using D = DeviceKind;
    static const std::vector<ProgramDef> regs = {
        // ATK / STL / UTL — placeholders for Plan 3 (no effects in Plan 2)
        { ProgramId::IcebreakerLite, K::Atk, 1, 2, 2, "Icebreaker Lite", "icebreaker_lite.exe",
          "Light cracker for white ICE. (Used in the Grid — Plan 3.)", 0, {} },
        { ProgramId::GhostTrace,     K::Stl, 1, 3, 0, "Ghost Trace",     "ghost_trace.exe",
          "Sheds Trace and hides you from white ICE briefly. (Plan 3.)", 0, {} },
        { ProgramId::Cooldown,       K::Stl, 1, 2, 0, "Cooldown",        "cooldown.exe",
          "Drops Heat by 4. (Plan 3.)", 0, {} },
        { ProgramId::Breach,         K::Utl, 1, 3, 3, "Breach",          "breach.exe",
          "Burns one firewall tile or one gateway lock level. (Plan 3.)", 0, {} },
        { ProgramId::Decrypt,        K::Utl, 1, 2, 1, "Decrypt",         "decrypt.exe",
          "Reads one encrypted file. (Plan 3.)", 0, {} },

        // QH — the active Plan 2 layer.
        { ProgramId::RebootOptics,   K::Qh, 1, 1, 0, "Reboot Optics",    "reboot_optics.qh",
          "Soft-reboots a camera or turret's optics. Blinded for 4 turns.",
          1, { D::Camera, D::Turret } },
        { ProgramId::FriendlyFire,   K::Qh, 2, 3, 0, "Friendly Fire",    "friendly_fire.qh",
          "Re-targets a turret onto its allies for 2 turns.",
          3, { D::Turret } },
        { ProgramId::DataLeech,      K::Qh, 1, 2, 0, "Data Leech",       "data_leech.qh",
          "Drains a packet of operational data from a hackable.",
          2, { D::Camera, D::PowerConduit, D::PrecursorConsole } },
    };
    return regs;
}

const ProgramDef* find_program(ProgramId id) {
    for (const auto& p : program_registry())
        if (p.id == id) return &p;
    return nullptr;
}

} // namespace astra
```

- [ ] **Step 3: Add the Program payload field on `Item`**

In `include/astra/item.h`, after the existing `std::optional<struct CyberdeckData> deck;` field, add:

```cpp
    // Program payload — non-empty only when type == ItemType::Program.
    std::optional<struct ProgramData> program;
```

Add `#include "astra/program.h"` to the top of `item.h` next to `cyberdeck.h`.

- [ ] **Step 4: Add program builders**

In `include/astra/item_defs.h`:
```cpp
// --- Programs ---
Item build_program_icebreaker_lite();
Item build_program_ghost_trace();
Item build_program_cooldown();
Item build_program_breach();
Item build_program_decrypt();
Item build_program_reboot_optics();
Item build_program_friendly_fire();
Item build_program_data_leech();
```

In `src/item_defs.cpp`, add a `// --- Programs ---` section near the cyberdeck section. Use a small helper to keep the eight builders DRY:

```cpp
// ---------------------------------------------------------------------------
// Programs
// ---------------------------------------------------------------------------

namespace {
Item make_program_(uint16_t def_id, uint32_t inv_id, ProgramId pid,
                   const char* name, const char* desc,
                   Rarity rarity, int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = inv_id; it.name = name; it.type = ItemType::Program;
    it.description = desc;
    it.rarity = rarity;
    it.weight = 0;          // weightless data
    it.stackable = false;   // programs are individual; copies are distinct loads
    it.buy_value = buy;
    it.sell_value = sell;
    ProgramData pd;
    pd.id = pid;
    it.program = pd;
    return it;
}
} // namespace

Item build_program_icebreaker_lite() {
    return make_program_(ITEM_PROG_ICEBREAKER_LITE, 9100, ProgramId::IcebreakerLite,
        "icebreaker_lite.exe",
        "ATK | tier 1 | 2 RAM, 2 Heat. Light cracker for white ICE. (Used in the Grid.)",
        Rarity::Common, 80, 25);
}
Item build_program_ghost_trace() {
    return make_program_(ITEM_PROG_GHOST_TRACE, 9101, ProgramId::GhostTrace,
        "ghost_trace.exe",
        "STL | tier 1 | 3 RAM. Sheds Trace and hides you briefly. (Used in the Grid.)",
        Rarity::Uncommon, 120, 40);
}
Item build_program_cooldown() {
    return make_program_(ITEM_PROG_COOLDOWN, 9102, ProgramId::Cooldown,
        "cooldown.exe",
        "STL | tier 1 | 2 RAM. Drops Heat by 4. (Used in the Grid.)",
        Rarity::Common, 60, 20);
}
Item build_program_breach() {
    return make_program_(ITEM_PROG_BREACH, 9103, ProgramId::Breach,
        "breach.exe",
        "UTL | tier 1 | 3 RAM, 3 Heat. Burns one firewall tile. (Used in the Grid.)",
        Rarity::Uncommon, 100, 35);
}
Item build_program_decrypt() {
    return make_program_(ITEM_PROG_DECRYPT, 9104, ProgramId::Decrypt,
        "decrypt.exe",
        "UTL | tier 1 | 2 RAM, 1 Heat. Reads one encrypted file. (Used in the Grid.)",
        Rarity::Common, 70, 22);
}
Item build_program_reboot_optics() {
    return make_program_(ITEM_PROG_REBOOT_OPTICS, 9105, ProgramId::RebootOptics,
        "reboot_optics.qh",
        "QH | tier 1 | 1 RAM, +1 Detection. Blinds a camera or turret for 4 turns.",
        Rarity::Common, 50, 18);
}
Item build_program_friendly_fire() {
    return make_program_(ITEM_PROG_FRIENDLY_FIRE, 9106, ProgramId::FriendlyFire,
        "friendly_fire.qh",
        "QH | tier 2 | 3 RAM, +3 Detection. A turret targets its allies for 2 turns.",
        Rarity::Uncommon, 180, 60);
}
Item build_program_data_leech() {
    return make_program_(ITEM_PROG_DATA_LEECH, 9107, ProgramId::DataLeech,
        "data_leech.qh",
        "QH | tier 1 | 2 RAM, +2 Detection. Drains operational data from a hackable.",
        Rarity::Uncommon, 90, 30);
}
```

- [ ] **Step 5: Wire dispatch**

In `build_by_def_id()` in `src/item_defs.cpp`, add:

```cpp
        case ITEM_PROG_ICEBREAKER_LITE:     return build_program_icebreaker_lite();
        case ITEM_PROG_GHOST_TRACE:         return build_program_ghost_trace();
        case ITEM_PROG_COOLDOWN:            return build_program_cooldown();
        case ITEM_PROG_BREACH:              return build_program_breach();
        case ITEM_PROG_DECRYPT:             return build_program_decrypt();
        case ITEM_PROG_REBOOT_OPTICS:       return build_program_reboot_optics();
        case ITEM_PROG_FRIENDLY_FIRE:       return build_program_friendly_fire();
        case ITEM_PROG_DATA_LEECH:          return build_program_data_leech();
```

- [ ] **Step 6: Restore real include in `hackable.cpp`**

In `src/hackable.cpp`, replace the temporary forward-declared `enum class ProgramId` from Task 2 with the real `#include "astra/program.h"`. (The contents of `make_hackable()` already reference `ProgramId::RebootOptics`, etc. — these now resolve correctly.)

- [ ] **Step 7: CMakeLists**

Add `src/program.cpp` to the source list.

- [ ] **Step 8: Build**

```
cmake --build build -j
```
Expected: clean build.

- [ ] **Step 9: Smoke check**

Defer until Task 6 (loot) lands so `give item` can resolve identifiers.

- [ ] **Step 10: Commit**

```
git add include/astra/program.h src/program.cpp \
        include/astra/item.h \
        include/astra/item_defs.h src/item_defs.cpp \
        src/hackable.cpp \
        CMakeLists.txt
git commit -m "feat(hacking): add Program item type + 8 starter programs

Five .exe (ATK/STL/UTL — placeholders for Plan 3) and three .qh
(reboot_optics, friendly_fire, data_leech — active in Plan 2).
ProgramRegistry/find_program lookup. ProgramData stored on Item."
```

---

## Task 5 — Code-fragment material category + 3 program schematic recipes

**Files:**
- Modify: `src/tinkering.cpp` (add 3 material defs + 3 schematic recipes)
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp` (3 new code-fragment item builders)
- Modify: `src/loot_table.cpp` (drop entries for code fragments — done partially in Task 6)

Code fragments are crafting materials. They follow the existing pattern (junk-typed reagents like Spare Parts / Circuitry, or pure mats). T1/T2/T3 fragments are pure mats so they're rare drops. Three schematic recipes are added: `icebreaker_lite.exe` (Atk), `decrypt.exe` (Utl), `reboot_optics.qh` (Qh).

- [ ] **Step 1: Add code fragment item builders**

In `include/astra/item_defs.h`:
```cpp
// --- Code fragments ---
Item build_code_fragment_t1();
Item build_code_fragment_t2();
Item build_code_fragment_t3();
```

In `src/item_defs.cpp`, near the existing crafting-materials section:

```cpp
// ---------------------------------------------------------------------------
// Code fragments — material category for program tinkering
// ---------------------------------------------------------------------------

Item build_code_fragment_t1() {
    Item it;
    it.item_def_id = ITEM_CODE_FRAGMENT_T1;
    it.id = 7100;  // material id matches Item::id used in tinkering recipes
    it.name = "Code Fragment (T1)";
    it.type = ItemType::CraftingMaterial;
    it.description = "A scrap of compiled cyber-code. Smells like a cheap deck.";
    it.weight = 0; it.stackable = true; it.sell_value = 4;
    return it;
}
Item build_code_fragment_t2() {
    Item it;
    it.item_def_id = ITEM_CODE_FRAGMENT_T2;
    it.id = 7101;
    it.name = "Code Fragment (T2)";
    it.type = ItemType::CraftingMaterial;
    it.description = "Mid-tier daemon-class fragment. Worth a few hours of compile.";
    it.weight = 0; it.stackable = true; it.sell_value = 12;
    return it;
}
Item build_code_fragment_t3() {
    Item it;
    it.item_def_id = ITEM_CODE_FRAGMENT_T3;
    it.id = 7102;
    it.name = "Code Fragment (T3)";
    it.type = ItemType::CraftingMaterial;
    it.description = "Pristine. Rumoured to be lifted off a deep-Grid cache.";
    it.weight = 0; it.stackable = true; it.sell_value = 40;
    return it;
}
```

Wire dispatch:

```cpp
        case ITEM_CODE_FRAGMENT_T1:         return build_code_fragment_t1();
        case ITEM_CODE_FRAGMENT_T2:         return build_code_fragment_t2();
        case ITEM_CODE_FRAGMENT_T3:         return build_code_fragment_t3();
```

- [ ] **Step 2: Register material defs in `material_catalog()`**

In `src/tinkering.cpp` (around line 21), append to the `catalog` initializer-list (place these after the T3 block, before the closing `};`):

```cpp
        // --- Code fragments (Plan 2 hacking) ---
        { 7100, "Code Fragment (T1)",  T::Common,   ',', static_cast<uint8_t>(Color::BrightCyan), 4,  false },
        { 7101, "Code Fragment (T2)",  T::Uncommon, '+', static_cast<uint8_t>(Color::BrightCyan), 12, false },
        { 7102, "Code Fragment (T3)",  T::Rare,     '*', static_cast<uint8_t>(Color::BrightCyan), 40, false },
```

- [ ] **Step 3: Add 3 program schematic recipes**

In `src/tinkering.cpp` `schematic_recipes()` (around line 762), append three recipes inside the static initializer (after the existing turret recipes; reuse the next free schematic_id — current max is 21, so use 30-32 to leave headroom):

```cpp
        // --- Programs (Plan 2 hacking) ---
        { 30, ITEM_PROG_ICEBREAKER_LITE, ITEM_PROG_ICEBREAKER_LITE,
              "icebreaker_lite.exe", "ATK program. Light cracker for white ICE.",
              { {7100, 2}, {7011, 1}, {7003, 1} }, 1 },     // 2 T1 frag + polymer + circuit board
        { 31, ITEM_PROG_DECRYPT, ITEM_PROG_DECRYPT,
              "decrypt.exe", "UTL program. Reads one encrypted file.",
              { {7100, 1}, {7012, 1}, {7003, 1} }, 1 },     // 1 T1 frag + glass shard + circuit board
        { 32, ITEM_PROG_REBOOT_OPTICS, ITEM_PROG_REBOOT_OPTICS,
              "reboot_optics.qh", "QH program. Blinds a camera or turret for 4 turns.",
              { {7100, 1}, {7010, 1}, {31, 1} }, 1 },       // 1 T1 frag + copper wire + broken circuit
```

(Note: `output_id` and `output_def_id` both use the same `ITEM_PROG_*` constant. Items use `Item::id == item_def_id` for programs since they're not stackable.)

- [ ] **Step 4: Update `LearnedSchematic` discovery hooks (none needed)**

Schematic recipes are looked up by id automatically. The PDA Tinkering tab displays them via the existing `tinkering` flow.

- [ ] **Step 5: Build**

```
cmake --build build -j
```
Expected: clean build.

- [ ] **Step 6: Smoke check**

Run `./build/astra-dev`. Open dev console, type `give item code_fragment_t1` (defer if loot identifier isn't registered yet — it is in Task 6). Open PDA → Tinkering → schematics catalog and verify the 3 new program recipes are visible (assuming the player has learned them — for now just confirm they exist as defined recipes).

- [ ] **Step 7: Commit**

```
git add include/astra/item_defs.h src/item_defs.cpp src/tinkering.cpp
git commit -m "feat(hacking): add code-fragment material + 3 program recipes

T1/T2/T3 code fragments as crafting materials. Three program schematics:
icebreaker_lite.exe, decrypt.exe, reboot_optics.qh."
```

---

## Task 6 — Loot integration

**Files:**
- Modify: `include/astra/loot_source.h`
- Modify: `src/loot_table.cpp`

Wire the new items into the loot pipeline so they actually drop. We add three new `Category` values (Cyberdeck, Program, CodeFragment) and entries that route through existing source masks (BlackMarket, MerchantArms, Chest).

- [ ] **Step 1: Add Category values**

In `include/astra/loot_source.h`, extend the `Category` enum (after `QuestItem`):

```cpp
    Cyberdeck,
    Program,
    CodeFragment,
```

In the `category_name` switch, add:
```cpp
        case Category::Cyberdeck:    return "cyberdeck";
        case Category::Program:      return "program";
        case Category::CodeFragment: return "code fragment";
```

- [ ] **Step 2: Add loot entries**

In `src/loot_table.cpp`, add a section after the existing turret entries (around line 90). Use the same schema as nearby entries:

```cpp
        // ----- Cyberdecks ----------------------------------------------
        LootEntry{ ITEM_PIDGIN_MK1,         "pidgin_mk1",         R::Common,    R::Uncommon,   8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,                            T::Tech,     1, C::Cyberdeck },
        LootEntry{ ITEM_POLYGLOT_DCK2,      "polyglot_dck2",      R::Uncommon,  R::Rare,       4, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                            T::Tech,     2, C::Cyberdeck },

        // ----- Programs ------------------------------------------------
        LootEntry{ ITEM_PROG_ICEBREAKER_LITE, "icebreaker_lite",  R::Common,    R::Uncommon,  10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                            T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_GHOST_TRACE,     "ghost_trace",      R::Uncommon,  R::Rare,       6, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     2, C::Program },
        LootEntry{ ITEM_PROG_COOLDOWN,        "cooldown",         R::Common,    R::Common,    12, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_BREACH,          "breach",           R::Uncommon,  R::Rare,       8, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_DECRYPT,         "decrypt",          R::Common,    R::Uncommon,  10, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_REBOOT_OPTICS,   "reboot_optics",    R::Common,    R::Common,    14, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,                          T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_FRIENDLY_FIRE,   "friendly_fire",    R::Uncommon,  R::Rare,       6, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     2, C::Program },
        LootEntry{ ITEM_PROG_DATA_LEECH,      "data_leech",       R::Common,    R::Uncommon,   8, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },

        // ----- Code fragments (crafting material) ----------------------
        LootEntry{ ITEM_CODE_FRAGMENT_T1,     "code_fragment_t1", R::Common,    R::Common,    20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     1, C::CodeFragment },
        LootEntry{ ITEM_CODE_FRAGMENT_T2,     "code_fragment_t2", R::Uncommon,  R::Uncommon,  10, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,      T::Tech,     2, C::CodeFragment },
        LootEntry{ ITEM_CODE_FRAGMENT_T3,     "code_fragment_t3", R::Rare,      R::Rare,       3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::CodeFragment },
```

- [ ] **Step 3: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 4: Smoke check**

Run `./build/astra-dev`. In dev console, run:
```
give item pidgin_mk1
give item reboot_optics
give item code_fragment_t1
```
Each should appear in the player's inventory. Open PDA → Equipment, equip the Pidgin Mark I (it slots into the Cyberdeck slot). Open PDA → Equipment again — confirm the cyberdeck slot now shows the deck.

- [ ] **Step 5: Commit**

```
git add include/astra/loot_source.h src/loot_table.cpp
git commit -m "feat(hacking): wire cyberdecks, programs, code fragments into loot

3 new Category values + entries routed through Chest, MerchantArms,
BlackMarket, ScavMerchant, and NpcDrop sources."
```

---

## Task 7 — `HackingSystem` skeleton + per-zone Detection counter

**Files:**
- Create: `include/astra/hacking_system.h`
- Create: `src/hacking_system.cpp`
- Modify: `include/astra/game.h` (own a `HackingSystem hacking_;`)
- Modify: `src/game.cpp` (tick decay each step)
- Modify: `CMakeLists.txt`

This task introduces the coordinator class that owns: (a) quickhack-targeting state, (b) per-zone Detection counter, (c) the dispatch entry into the program-effect table. The actual quickhack flow lands in Task 8; this task only stands up the data model and decay loop.

- [ ] **Step 1: Create `include/astra/hacking_system.h`**

```cpp
#pragma once

#include "astra/program.h"

#include <cstdint>
#include <unordered_map>

namespace astra {

class Game;
struct Hackable;
struct Item;
class TileMap;

// Per-zone Detection state. A "zone" here is keyed by the active map's
// map_id (uint32_t). Zones decay independently so going to overworld and
// back doesn't reset.
struct DetectionState {
    int  value      = 0;     // [0, 100]
    int  decay_acc  = 0;     // tick accumulator; -1 per N ticks while out of LOS
};

class HackingSystem {
public:
    HackingSystem() = default;

    // ── Targeting ──
    bool targeting() const { return targeting_; }
    int  target_x() const { return target_x_; }
    int  target_y() const { return target_y_; }
    int  blink_phase() const { return blink_phase_; }
    void tick_blink() { ++blink_phase_; }

    // Open targeting at the player's position with cursor centered on the
    // nearest visible Hackable (fixture or NPC). No-op + log on the game
    // side if the player has no cyberdeck equipped.
    void begin_quickhack_targeting(Game& game);
    void cancel_targeting() { targeting_ = false; }
    void handle_targeting_input(int key, Game& game);

    // ── Detection ──
    int  detection(uint32_t map_id) const;
    void add_detection(uint32_t map_id, int delta);
    void tick_detection(uint32_t map_id, int player_x, int player_y, const TileMap& map);

    // Reset all detection on save load — used by save_file.cpp.
    void clear_all_detection() { detection_.clear(); }

    // For save serialization (non-const access).
    std::unordered_map<uint32_t, DetectionState>& detection_map_mut() { return detection_; }
    const std::unordered_map<uint32_t, DetectionState>& detection_map() const { return detection_; }

    // ── Quickhack execution ──
    // Picks the right ProgramDef from `program`, validates RAM, debits
    // RAM on the player's deck, bumps Detection on the active map, and
    // applies the program's effect to `target`. Returns a human-readable
    // message describing the outcome (success or refusal reason).
    std::string execute_quickhack(Game& game, const Item& program, Hackable& target,
                                  int target_x, int target_y);

private:
    bool targeting_ = false;
    int  target_x_ = 0;
    int  target_y_ = 0;
    int  blink_phase_ = 0;

    std::unordered_map<uint32_t, DetectionState> detection_;
};

} // namespace astra
```

- [ ] **Step 2: Create `src/hacking_system.cpp` (skeleton)**

```cpp
#include "astra/hacking_system.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/program.h"
#include "astra/program_effects.h"   // dispatch (Task 9)
#include "astra/visibility_map.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cmath>

namespace astra {

namespace {
constexpr int kDetectionDecayInterval = 5;   // every 5 ticks while out of LOS, -1
constexpr int kDetectionMax = 100;
constexpr int kDetectionMin = 0;
} // namespace

int HackingSystem::detection(uint32_t map_id) const {
    auto it = detection_.find(map_id);
    return (it == detection_.end()) ? 0 : it->second.value;
}

void HackingSystem::add_detection(uint32_t map_id, int delta) {
    auto& st = detection_[map_id];
    st.value = std::clamp(st.value + delta, kDetectionMin, kDetectionMax);
}

void HackingSystem::tick_detection(uint32_t map_id, int /*player_x*/, int /*player_y*/,
                                   const TileMap& /*map*/) {
    auto it = detection_.find(map_id);
    if (it == detection_.end()) return;
    auto& st = it->second;
    if (st.value <= kDetectionMin) return;

    // Decay only while no hostile observer is in LOS — for v1 just decay
    // unconditionally on a slow timer. Future work can gate on FOV.
    if (++st.decay_acc >= kDetectionDecayInterval) {
        st.decay_acc = 0;
        st.value = std::max(kDetectionMin, st.value - 1);
    }
}

void HackingSystem::begin_quickhack_targeting(Game& /*game*/) {
    // Implemented in Task 8.
    targeting_ = true;
    blink_phase_ = 0;
}

void HackingSystem::handle_targeting_input(int /*key*/, Game& /*game*/) {
    // Implemented in Task 8.
}

std::string HackingSystem::execute_quickhack(Game& game, const Item& program,
                                             Hackable& target, int tx, int ty) {
    if (!program.program) return "Not a program.";
    const ProgramDef* def = find_program(program.program->id);
    if (!def) return "Unknown program.";
    if (def->kind != ProgramKind::Qh)
        return "Only .qh programs can be fired in the real world.";

    // Filter check.
    bool ok = std::any_of(def->target_filter.begin(), def->target_filter.end(),
                          [&](DeviceKind k){ return k == target.device_kind; });
    if (!ok) {
        return std::string("Program rejects ") + device_kind_name(target.device_kind) + ".";
    }

    // Spend RAM.
    auto& deck_slot = game.player().equipment.cyberdeck;
    if (!deck_slot || !deck_slot->deck) return "No cyberdeck equipped.";
    auto& deck = *deck_slot->deck;
    if (deck.ram_current < def->ram_cost) {
        return "Not enough RAM (" + std::to_string(deck.ram_current) + "/" +
               std::to_string(def->ram_cost) + ").";
    }
    deck.ram_current -= def->ram_cost;

    // Bump detection on active map.
    uint32_t mid = game.world().current_map_id();
    add_detection(mid, def->detection_cost);

    // Apply effect.
    apply_program_effect(def->id, game, target, tx, ty);

    target.state = HackState::Compromised;
    return std::string(def->name) + " executed.";
}

} // namespace astra
```

- [ ] **Step 3: Add `HackingSystem` to `Game`**

In `include/astra/game.h`, add `#include "astra/hacking_system.h"` near the other system includes. In the `Game` class (private member section, near `combat_`, `dialog_`, `input_`), add:

```cpp
    HackingSystem hacking_;
```

Add a public accessor (mirror `combat()` accessor):
```cpp
    HackingSystem& hacking() { return hacking_; }
    const HackingSystem& hacking() const { return hacking_; }
```

- [ ] **Step 4: Tick detection each game step**

In `src/game.cpp`, find the function that advances the world tick (look for `Game::advance_world` or `Game::tick_systems`). In the per-tick block, add:

```cpp
    hacking_.tick_detection(world_.current_map_id(),
                            player_.x, player_.y, world_.map());
```

If `world_.current_map_id()` doesn't exist, look for the map_id field on `MapState`/`WorldManager` and use the equivalent (e.g. `world_.active_map().map_id()` or a helper).

- [ ] **Step 5: Update `CMakeLists.txt`**

Add `src/hacking_system.cpp` to the source list.

- [ ] **Step 6: Build**

```
cmake --build build -j
```
Expected: build will fail because `program_effects.h` doesn't exist yet (Task 9 creates it). For this task's commit, **stub it temporarily**: create `include/astra/program_effects.h` with:

```cpp
#pragma once
namespace astra {
class Game;
struct Hackable;
enum class ProgramId : uint16_t;
inline void apply_program_effect(ProgramId, Game&, Hackable&, int, int) {}
}
```

This compiles; Task 9 replaces the body with the real dispatch table. (Yes, this is technically a placeholder — but it's a known-temporary file that the next task overwrites entirely. Document it in the commit message.)

- [ ] **Step 7: Smoke check**

Run `./build/astra-dev`. Game still launches; detection has no observable effect yet. No regressions.

- [ ] **Step 8: Commit**

```
git add include/astra/hacking_system.h src/hacking_system.cpp \
        include/astra/program_effects.h \
        include/astra/game.h src/game.cpp \
        CMakeLists.txt
git commit -m "feat(hacking): HackingSystem skeleton + per-zone Detection

Per-map detection counter with linear decay (-1 per 5 ticks).
execute_quickhack() validates RAM/filter, debits, dispatches effect.
program_effects.h is a stub here — Task 9 fills the dispatch table."
```

---

## Task 8 — Quickhack targeting flow on the `H` key

**Files:**
- Modify: `src/game_input.cpp` (bind `H`, intercept while targeting)
- Modify: `src/game_rendering.cpp` (draw cursor + side panel during targeting)
- Modify: `src/hacking_system.cpp` (real targeting body)

The flow mirrors `CombatSystem::begin_targeting()` / `handle_targeting_input()` from `src/game_combat.cpp` line 699-772 — read that first.

- [ ] **Step 1: Implement targeting body in `HackingSystem`**

In `src/hacking_system.cpp`, replace the stub `begin_quickhack_targeting()` and `handle_targeting_input()` bodies:

```cpp
namespace {

// Helper: find a Hackable on a given tile, returns nullptr if none.
// Searches fixtures first, then NPCs.
struct HackTarget {
    Hackable* hack = nullptr;
    int tx = 0, ty = 0;
    std::string name;
};
HackTarget hackable_at(Game& game, int x, int y) {
    HackTarget t{};
    t.tx = x; t.ty = y;
    auto& world = game.world();
    Tile tile = world.map().get(x, y);
    if (tile == Tile::Fixture) {
        int fid = world.map().fixture_id(x, y);
        if (fid >= 0) {
            FixtureData& fd = world.map_mut().fixture_mut(fid);
            if (fd.cyber) {
                t.hack = &*fd.cyber;
                t.name = device_kind_name(fd.cyber->device_kind);
                return t;
            }
        }
    }
    for (auto& npc : world.npcs_mut()) {
        if (npc.x == x && npc.y == y && npc.cyber && npc.alive()) {
            t.hack = &*npc.cyber;
            t.name = npc.label();
            return t;
        }
    }
    return t;
}

} // namespace

void HackingSystem::begin_quickhack_targeting(Game& game) {
    if (!game.player().equipment.cyberdeck ||
        !game.player().equipment.cyberdeck->deck) {
        game.log("You need an equipped cyberdeck to quickhack.");
        return;
    }
    targeting_ = true;
    blink_phase_ = 0;

    // Snap cursor to nearest visible Hackable.
    int best_d = 9999;
    int best_x = game.player().x, best_y = game.player().y;
    auto& world = game.world();
    for (int y = 0; y < world.map().height(); ++y) {
        for (int x = 0; x < world.map().width(); ++x) {
            if (world.visibility().get(x, y) != Visibility::Visible) continue;
            auto t = hackable_at(game, x, y);
            if (!t.hack) continue;
            int d = std::abs(x - game.player().x) + std::abs(y - game.player().y);
            if (d < best_d) { best_d = d; best_x = x; best_y = y; }
        }
    }
    target_x_ = best_x;
    target_y_ = best_y;
    game.log("Quickhack targeting. Move cursor, [Enter] confirm, [Esc] cancel.");
}

void HackingSystem::handle_targeting_input(int key, Game& game) {
    auto step = [&](int dx, int dy) {
        int nx = target_x_ + dx;
        int ny = target_y_ + dy;
        if (nx < 0 || nx >= game.world().map().width()) return;
        if (ny < 0 || ny >= game.world().map().height()) return;
        target_x_ = nx; target_y_ = ny;
    };
    switch (key) {
        case 'k': case KEY_UP:    step( 0, -1); break;
        case 'j': case KEY_DOWN:  step( 0,  1); break;
        case 'h': case KEY_LEFT:  step(-1,  0); break;
        case 'l': case KEY_RIGHT: step( 1,  0); break;
        case '\033':
            targeting_ = false;
            game.log("Quickhack cancelled.");
            break;
        case '\n': case '\r': {
            auto t = hackable_at(game, target_x_, target_y_);
            if (!t.hack) {
                game.log("No hackable target there.");
                return;
            }
            // Build list of loaded QH programs whose target_filter matches.
            auto& deck_slot = game.player().equipment.cyberdeck;
            if (!deck_slot || !deck_slot->deck) {
                targeting_ = false;
                game.log("No deck equipped.");
                return;
            }
            std::vector<int> menu_slots;
            for (int i = 0; i < kCyberdeckMaxSlots; ++i) {
                const auto& slot = deck_slot->deck->loaded[i];
                if (!slot || !slot->program) continue;
                const ProgramDef* def = find_program(slot->program->id);
                if (!def || def->kind != ProgramKind::Qh) continue;
                bool match = std::any_of(def->target_filter.begin(),
                                         def->target_filter.end(),
                                         [&](DeviceKind k){ return k == t.hack->device_kind; });
                if (match) menu_slots.push_back(i);
            }
            if (menu_slots.empty()) {
                game.log("No loaded quickhack matches " + t.name + ".");
                return;
            }
            // Open the program-pick menu via Game::open_qh_picker (added below).
            game.open_qh_picker(target_x_, target_y_, menu_slots);
            targeting_ = false;
            return;
        }
        default: break;
    }
}
```

- [ ] **Step 2: Add `Game::open_qh_picker` + execution wiring**

In `include/astra/game.h`, add a new public method declaration:

```cpp
    // Quickhack target picker — opens a popup of loaded QH programs that
    // match the hackable at (tx, ty) and dispatches to HackingSystem on
    // confirm. menu_slots is the list of CyberdeckData::loaded[] indices
    // (filtered by target_filter).
    void open_qh_picker(int tx, int ty, const std::vector<int>& menu_slots);
```

In `src/game_input.cpp` (or `src/game_interaction.cpp` — pick the file with the closest existing menu pattern), implement:

```cpp
void Game::open_qh_picker(int tx, int ty, const std::vector<int>& menu_slots) {
    qh_picker_.reset();
    qh_picker_.title = "Quickhack:";
    auto& deck = *player_.equipment.cyberdeck->deck;
    for (size_t i = 0; i < menu_slots.size(); ++i) {
        const auto& slot = deck.loaded[menu_slots[i]];
        const ProgramDef* def = find_program(slot->program->id);
        char k = static_cast<char>('a' + i);
        std::string label = std::string(def->filename) + "  (" +
                            std::to_string(def->ram_cost) + " RAM)";
        qh_picker_.add_option(k, label);
    }
    qh_picker_.selection = 0;
    qh_picker_.open = true;
    qh_picker_target_x_ = tx;
    qh_picker_target_y_ = ty;
    qh_picker_slots_ = menu_slots;
}
```

Add private members to the Game class for the picker state:

```cpp
    MenuState qh_picker_;
    std::vector<int> qh_picker_slots_;
    int qh_picker_target_x_ = 0;
    int qh_picker_target_y_ = 0;
```

- [ ] **Step 3: Bind `H` and intercept while targeting/picker is active**

In `src/game_input.cpp` `Game::handle_play_input(int key)`:

(a) Above the `// Targeting mode intercept` block, add:
```cpp
    // QH program picker intercept — handle BEFORE any other key consumers.
    if (qh_picker_.open) {
        MenuResult r = qh_picker_.handle_input(key);
        if (r == MenuResult::Selected) {
            int slot_idx = qh_picker_slots_[qh_picker_.selection];
            auto& deck = *player_.equipment.cyberdeck->deck;
            const Item& prog = *deck.loaded[slot_idx];
            // Find target Hackable.
            int tx = qh_picker_target_x_, ty = qh_picker_target_y_;
            Hackable* hack = nullptr;
            if (world_.map().get(tx, ty) == Tile::Fixture) {
                int fid = world_.map().fixture_id(tx, ty);
                if (fid >= 0 && world_.map_mut().fixture_mut(fid).cyber) {
                    hack = &*world_.map_mut().fixture_mut(fid).cyber;
                }
            }
            if (!hack) {
                for (auto& npc : world_.npcs_mut()) {
                    if (npc.x == tx && npc.y == ty && npc.cyber && npc.alive()) {
                        hack = &*npc.cyber;
                        break;
                    }
                }
            }
            if (hack) {
                std::string msg = hacking_.execute_quickhack(*this, prog, *hack, tx, ty);
                log(msg);
                advance_world(ActionCost::interact);
            }
            qh_picker_.open = false;
        } else if (r == MenuResult::Cancelled) {
            qh_picker_.open = false;
        }
        return;
    }

    // Quickhack targeting mode intercept (mirrors combat targeting).
    if (hacking_.targeting()) {
        hacking_.handle_targeting_input(key, *this);
        return;
    }
```

(b) In the `switch (key)` block, add an `H` case (capital H — lowercase `h` is bound to KEY_LEFT):
```cpp
        case 'H':
            hacking_.begin_quickhack_targeting(*this);
            break;
```

- [ ] **Step 4: Render the targeting cursor + side panel**

In `src/game_rendering.cpp`, find the post-`combat_.targeting()` rendering block (search for `combat_.targeting()` or `combat_.target_x()`). After it (or in a parallel block), add a similar render for `hacking_.targeting()`:

```cpp
    if (hacking_.targeting()) {
        int sx = hacking_.target_x() - camera_x_;
        int sy = hacking_.target_y() - camera_y_;
        // Cyan crosshair, blinking via blink_phase().
        bool on = (hacking_.blink_phase() / 30) % 2 == 0;
        if (on) {
            renderer_->put_glyph(sx, sy, "+", Color::BrightCyan);
        }

        // Side panel: device name, tier, state, available QH list.
        auto& world = world_;
        int tx = hacking_.target_x(), ty = hacking_.target_y();
        Hackable* hack = nullptr;
        std::string label;
        if (world.map().get(tx, ty) == Tile::Fixture) {
            int fid = world.map().fixture_id(tx, ty);
            if (fid >= 0 && world.map().fixture(fid).cyber) {
                hack = const_cast<Hackable*>(&*world.map().fixture(fid).cyber);
                label = device_kind_name(hack->device_kind);
            }
        }
        if (!hack) {
            for (auto& npc : world.npcs()) {
                if (npc.x == tx && npc.y == ty && npc.cyber) {
                    hack = const_cast<Hackable*>(&*npc.cyber);
                    label = npc.label();
                }
            }
        }
        // Draw a 4-line panel at the bottom-right corner of the world view.
        int px = world_view_right_ - 28;
        int py = world_view_bottom_ - 6;
        renderer_->draw_box(px, py, 28, 6, Color::BrightCyan);
        if (hack) {
            renderer_->put_text(px + 2, py + 1, label.c_str(), Color::White);
            std::string st;
            switch (hack->state) {
                case HackState::Clean:      st = "state: clean"; break;
                case HackState::Compromised: st = "state: compromised"; break;
                case HackState::Alarmed:    st = "state: ALARMED"; break;
            }
            renderer_->put_text(px + 2, py + 2,
                ("tier " + std::to_string(hack->security_tier)).c_str(),
                Color::Cyan);
            renderer_->put_text(px + 2, py + 3, st.c_str(), Color::Cyan);
        } else {
            renderer_->put_text(px + 2, py + 2, "no hackable here", Color::DarkGray);
        }
    }
```

(Note: `world_view_right_` / `world_view_bottom_` may have different names in `src/game_rendering.cpp` — adapt to whatever the existing rendering uses. The exact panel position can be polished later; the smoke check just needs a visible label.)

- [ ] **Step 5: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 6: Smoke check**

Run `./build/astra-dev`. With dev console: give yourself a deck and a `reboot_optics` program (loaded in slot 0 — but loading lands in Task 11, so for now manually equip the deck and trust that an empty-loadout deck will simply log "No loaded quickhack matches…" when targeting).

Press `H` with no deck → log "You need an equipped cyberdeck to quickhack."
Equip the Pidgin Mark I → press `H` → cursor crosshair appears → arrow keys move it → `Esc` cancels with log message.

The full target → execute round-trip needs Task 9 (effects) and Task 10 (a fixture with `Hackable`) plus Task 11 (a loaded program slot) — those tasks complete the loop.

- [ ] **Step 7: Commit**

```
git add include/astra/game.h \
        src/game_input.cpp src/game_rendering.cpp src/hacking_system.cpp
git commit -m "feat(hacking): H-key quickhack targeting + program picker

H opens the cursor; arrow keys move; Enter pops the program-picker
popup limited to loaded .qh programs whose target_filter matches the
hackable under cursor. Esc cancels. Mirrors combat targeting pattern."
```

---

## Task 9 — Three QH program effects

**Files:**
- Replace: `include/astra/program_effects.h` (drop the temp stub from Task 7)
- Create: `src/program_effects.cpp`
- Modify: `CMakeLists.txt`

Three quickhacks have real-world effects. Each gets a small handler. Effects mutate game state (NPC AI state, fixture state, message log) and return through the existing log/event pipeline.

- [ ] **Step 1: Write `include/astra/program_effects.h`**

Replace the stub created in Task 7:

```cpp
#pragma once

#include "astra/program.h"

namespace astra {

class Game;
struct Hackable;

// Apply a quickhack effect to a target. Pre-conditions are validated
// upstream by HackingSystem::execute_quickhack (program is .qh, target
// passes filter, RAM debited, Detection bumped). This function only
// performs the world mutation.
void apply_program_effect(ProgramId id, Game& game, Hackable& target,
                          int target_x, int target_y);

} // namespace astra
```

- [ ] **Step 2: Create `src/program_effects.cpp`**

```cpp
#include "astra/program_effects.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/npc.h"
#include "astra/world_manager.h"

namespace astra {

namespace {

// Add a temporary "blinded" effect to a target. For NPC turrets/cameras
// we use the existing EffectId::EmpDisabled (already handles the "can't
// fire / can't see" case) for v1 simplicity. Plan 3 may introduce a
// dedicated EffectId::OpticsRebooted.
void blind_for_turns(Hackable& target, int turns) {
    target.state = HackState::Compromised;
    target.state_ticks_left = turns;
}

void apply_reboot_optics(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    blind_for_turns(target, 4);
    game.log("The " + std::string(device_kind_name(target.device_kind)) +
             " judders and flickers offline.");
}

void apply_friendly_fire(Game& game, Hackable& target, int tx, int ty) {
    if (target.device_kind != DeviceKind::Turret) {
        game.log("Friendly Fire only works on turrets.");
        return;
    }
    target.state = HackState::Compromised;
    target.state_ticks_left = 2;
    // Find the NPC at (tx, ty) and flip its faction temporarily by
    // applying a Friendly-Fire effect that the combat AI consults.
    // Plan 2 uses a minimal model: change NPC faction to "Hijacked" for 2
    // turns; faction.cpp treats "Hijacked" as hostile to all including
    // its original faction.
    for (auto& npc : game.world().npcs_mut()) {
        if (npc.x == tx && npc.y == ty && npc.alive()) {
            npc.faction = "Hijacked";
            // Add an Effect with 2-turn duration that restores faction
            // when expired. (Plan 3 generalises this.)
            add_effect(npc.effects, EffectId::Hijacked, 2);
            break;
        }
    }
    game.log("The turret rotates onto its allies.");
}

void apply_data_leech(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    target.state = HackState::Compromised;
    target.state_ticks_left = 1;
    // Reward: small XP/credit drip + a flavor log line.
    game.player().money += 5 + (target.security_tier * 5);
    game.log("Data leeched: +" + std::to_string(5 + target.security_tier * 5) +
             "₡ skimmed off the bus.");
}

} // namespace

void apply_program_effect(ProgramId id, Game& game, Hackable& target, int tx, int ty) {
    switch (id) {
        case ProgramId::RebootOptics: apply_reboot_optics(game, target, tx, ty); break;
        case ProgramId::FriendlyFire: apply_friendly_fire(game, target, tx, ty); break;
        case ProgramId::DataLeech:    apply_data_leech(game, target, tx, ty); break;
        // .exe programs have no real-world effect in Plan 2.
        default: break;
    }
}

} // namespace astra
```

- [ ] **Step 3: Add `Hijacked` faction + `EffectId::Hijacked`**

In `include/astra/effect.h`, add to the `EffectId` enum (next to the existing entries):
```cpp
    Hijacked,    // hacking: NPC faction temporarily flipped; expires to original
```

In the effect tick / expiry path in `src/effect.cpp` (search for `EffectId::EmpDisabled` for the existing pattern), add a handler for `Hijacked`. On expiry, restore the NPC's original faction. To do this: store the original faction in the effect's `payload` field (whatever `EffectList` supports). If there's no payload mechanism, fall back to: when applying `Hijacked`, also store the original faction in a new transient `Npc::pre_hijack_faction` field; when the effect expires, copy back.

To keep this minimal, add a new field on `Npc`:
```cpp
    std::string pre_hijack_faction;   // restored when Hijacked effect expires
```

In `apply_friendly_fire`, before flipping faction, save original:
```cpp
    if (npc.pre_hijack_faction.empty()) npc.pre_hijack_faction = npc.faction;
    npc.faction = "Hijacked";
```

In the effect's expiry path (add a case in the appropriate `effect.cpp` switch), restore on Hijacked expiry. (Find where existing effects clear their state on expiration; mimic that pattern.)

- [ ] **Step 4: Wire CMakeLists**

Add `src/program_effects.cpp` to the source list.

- [ ] **Step 5: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 6: Smoke check**

Defer the full smoke check until Task 10 (you need a hackable fixture in the world to test against).

- [ ] **Step 7: Commit**

```
git add include/astra/program_effects.h src/program_effects.cpp \
        include/astra/effect.h src/effect.cpp \
        include/astra/npc.h \
        CMakeLists.txt
git commit -m "feat(hacking): three QH effects (reboot_optics, friendly_fire, data_leech)

reboot_optics blinds device 4 turns; friendly_fire flips a turret's
faction to 'Hijacked' for 2 turns via a new EffectId::Hijacked;
data_leech awards a small credit drip scaled to security_tier."
```

---

## Task 10 — Hackable interaction integration

**Files:**
- Modify: `src/game_interaction.cpp`
- Modify: `src/dialog_manager.cpp` (or wherever fixture interaction prompts live)

When the player presses interact (`Space`) on a fixture that has a `Hackable` cyber field, the existing menu gets new options: each available QH that matches the player's loaded programs becomes a row, plus (for Precursor consoles only) a stub "Jack In" row that returns `"The Grid is not yet implemented (Plan 3)."` if `Cat_Hacking` is unlocked, or `"Requires the Hacking skill category."` otherwise.

- [ ] **Step 1: Audit the existing fixture-interact dispatch**

Open `src/game_interaction.cpp` line 221 (`Game::try_interact`). The fixture branch calls `dialog_.interact_fixture(fid, *this)`. Open `src/dialog_manager.cpp` and find `interact_fixture(int fid, Game& game)`. Note where it builds the option menu — that's the insertion point.

- [ ] **Step 2: Add a hackable-aware branch to fixture interaction**

In `dialog_manager.cpp`'s `interact_fixture`, near the top of the function (after fetching the fixture data), add:

```cpp
    if (fd.cyber) {
        // Build a contextual menu: regular fixture action (if any) +
        // any matching loaded quickhacks + Jack In (Precursor only).
        // For v1, just SHOW the available quickhacks/jack-in and dispatch
        // to HackingSystem (or a stub) on selection. The fixture's normal
        // interact path can stay reachable via... actually for v1 it's
        // simpler to fully route hackable fixtures through the cyber menu
        // and skip the normal-interact prompt.
        present_hackable_menu(*fd.cyber, fid, game);
        return;
    }
```

Implement `present_hackable_menu` as a private helper:

```cpp
void DialogManager::present_hackable_menu(Hackable& hack, int fid, Game& game) {
    MenuState menu;
    menu.title = std::string(device_kind_name(hack.device_kind)) +
                 " (tier " + std::to_string(hack.security_tier) + ")";

    // Quickhack rows — only those player has loaded that match.
    auto& deck_slot = game.player().equipment.cyberdeck;
    std::vector<int> qh_slots;
    if (deck_slot && deck_slot->deck) {
        for (int i = 0; i < kCyberdeckMaxSlots; ++i) {
            const auto& s = deck_slot->deck->loaded[i];
            if (!s || !s->program) continue;
            const ProgramDef* def = find_program(s->program->id);
            if (!def || def->kind != ProgramKind::Qh) continue;
            for (DeviceKind k : def->target_filter) {
                if (k == hack.device_kind) { qh_slots.push_back(i); break; }
            }
        }
    }
    for (size_t i = 0; i < qh_slots.size(); ++i) {
        const ProgramDef* def =
            find_program(deck_slot->deck->loaded[qh_slots[i]]->program->id);
        char k = static_cast<char>('a' + i);
        menu.add_option(k,
            std::string(def->filename) + "  (" +
            std::to_string(def->ram_cost) + " RAM)");
    }

    // Jack In row — Precursor consoles only, gated by Cat_Hacking.
    bool can_jack = (hack.device_kind == DeviceKind::PrecursorConsole);
    if (can_jack) {
        char k = 'j';
        std::string label = "Jack In";
        if (!player_has_skill(game.player(), SkillId::Cat_Hacking)) {
            label += "  (requires Cat_Hacking)";
        }
        menu.add_option(k, label);
    }

    if (menu.options.empty()) {
        game.log(std::string(device_kind_name(hack.device_kind)) +
                 ": no compatible programs loaded.");
        return;
    }

    // Defer dispatch via cached state — DialogManager already has a
    // pending_action pattern; mirror it.
    pending_hackable_fixture_id_ = fid;
    pending_hackable_qh_slots_ = qh_slots;
    open_menu_(std::move(menu),
        [this, &game](char k) {
            this->resolve_hackable_action(k, game);
        });
}

void DialogManager::resolve_hackable_action(char k, Game& game) {
    int fid = pending_hackable_fixture_id_;
    auto& fd = game.world().map_mut().fixture_mut(fid);
    if (!fd.cyber) return;

    if (k == 'j') {
        if (!player_has_skill(game.player(), SkillId::Cat_Hacking)) {
            game.log("You need the Hacking skill to jack in.");
            return;
        }
        game.log("The Grid is not yet implemented (Plan 3 will add it).");
        return;
    }

    // QH row: a..z mapped to pending_hackable_qh_slots_.
    int row = k - 'a';
    if (row < 0 || row >= (int)pending_hackable_qh_slots_.size()) return;
    int slot_idx = pending_hackable_qh_slots_[row];
    auto& deck = *game.player().equipment.cyberdeck->deck;
    const Item& prog = *deck.loaded[slot_idx];
    auto fpos = pos_of_fixture_(fid, game.world().map());
    std::string msg = game.hacking().execute_quickhack(*this->_unused_for_compile_,  // see note
                                                       prog, *fd.cyber,
                                                       fpos.first, fpos.second);
    game.log(msg);
    game.advance_world_interact_();   // public helper; add if needed
}
```

(Note: the exact `MenuState` API and `pending_hackable_*` fields will need new struct entries on `DialogManager`. Add these with the same approach as existing `pending_aria_*` flags. The `_unused_for_compile_` is illustrative — pass `game` directly. Cleaning up these wiring details is part of the implementation work; do not leave the `_unused_for_compile_` placeholder in the actual commit.)

Add the matching declarations to `include/astra/dialog_manager.h`:

```cpp
    void present_hackable_menu(Hackable& hack, int fid, Game& game);
    void resolve_hackable_action(char k, Game& game);
private:
    int pending_hackable_fixture_id_ = -1;
    std::vector<int> pending_hackable_qh_slots_;
```

If your existing dialog manager uses a different menu pattern (lambda-based vs key-based), adapt accordingly. The contract: pressing the appropriate menu option must reach `HackingSystem::execute_quickhack` with the right Hackable.

- [ ] **Step 3: Add `pos_of_fixture_` helper if not present**

If `dialog_manager.cpp` doesn't already have a way to look up a fixture's tile coords from its id, add a small helper that scans the tilemap. The TileMap interface in `tilemap.h` provides `fixture_id(x, y)` but no reverse lookup; either add one, or pass coords through from `interact_fixture`'s callsite (preferred: change `interact_fixture(int fid, ...)` callers in `game_interaction.cpp` to also pass `(tx, ty)`).

- [ ] **Step 4: Build**

```
cmake --build build -j
```
Expected: clean (after wiring up the dialog_manager additions).

- [ ] **Step 5: Smoke check (now possible end-to-end)**

Run `./build/astra-dev`. Use the dev console to:
1. `give item pidgin_mk1` and equip it.
2. (Loading programs into deck slots is Task 11; for the smoke check use the **`H` key flow** which doesn't require dialog-menu entry.)
3. Spawn a hackable: until Task 14 lands the dev-console spawner, manually plant a fixture via the map editor: open with `F2`, place a `Console` fixture, exit play test. (Or use the existing `quest fixture` debug spawner.) Open the map editor and toggle `cyber` on it — actually, since the editor doesn't yet expose `cyber`, the practical flow is:
   - Defer this smoke check to Task 14, which adds `spawn-hackable <kind>` to the dev console.

For now: confirm the build is clean and the existing fixture-interact flow still works (e.g. open a Console with `Space` — should log the existing flavor message since no fixture has `cyber` set yet).

- [ ] **Step 6: Commit**

```
git add src/game_interaction.cpp src/dialog_manager.cpp \
        include/astra/dialog_manager.h
git commit -m "feat(hacking): hackable fixture interaction menu + jack-in stub

Fixtures with a cyber trait route to a QH-program menu. Precursor
consoles get a 'Jack In' row gated by Cat_Hacking — Plan 2 stubs the
verb with a 'Grid not yet implemented' log line."
```

---

## Task 11 — PDA Hacking tab: locked splash + bounded-terminal subwindow

**Files:**
- Modify: `include/astra/pda_screen.h`
- Modify: `src/pda_hacking_tab.cpp`
- Modify: `src/pda_screen.cpp` (tab_help_body update)

This is the biggest UX task. The Hacking tab has two states:

1. **Locked** (`Cat_Hacking` not unlocked): a splash explaining how to unlock it, plus a list of equipped quickhack programs (visible, usable in the world via `H`).
2. **Unlocked**: a fake terminal with a bounded command set. Each command has a single-key shortcut menu fallback.

The 10 v1 commands: `help`, `deck info`, `programs ls`, `programs load <slot> <id>`, `programs unload <slot>`, `netmap` (stub), `jack -t <node>` (stub), `lore` (stub), `clear`, `history`.

- [ ] **Step 1: Add Hacking tab state to `PdaScreen`**

In `include/astra/pda_screen.h`, add to the private members section (near the other tab-state blocks like Cooking):

```cpp
    // Hacking tab — terminal subwindow
    struct HackTermLine { std::string text; UITag tag = UITag::TextDim; };
    std::vector<HackTermLine> hack_term_lines_;
    std::vector<std::string>  hack_term_history_;
    std::string               hack_term_input_;
    int                       hack_term_history_cursor_ = -1;
    int                       hack_term_scroll_ = 0;

    // Programs ls/load helpers
    enum class HackTermFocus : uint8_t { Terminal, ProgramsList };
    HackTermFocus hack_term_focus_ = HackTermFocus::Terminal;
    int hack_term_load_slot_ = -1;       // active slot during a `load <slot>` flow
    int hack_term_load_inv_cursor_ = 0;

    void hack_term_emit(const std::string& line, UITag tag = UITag::TextDim);
    void hack_term_run_command(const std::string& line);
    void hack_term_cmd_help();
    void hack_term_cmd_deck_info();
    void hack_term_cmd_programs_ls();
    void hack_term_cmd_programs_load(const std::vector<std::string>& args);
    void hack_term_cmd_programs_unload(const std::vector<std::string>& args);
    void hack_term_cmd_netmap();
    void hack_term_cmd_jack(const std::vector<std::string>& args);
    void hack_term_cmd_lore();
    void hack_term_cmd_clear();
    void hack_term_cmd_history();
    void handle_hacking_key(int key);
```

- [ ] **Step 2: Replace `src/pda_hacking_tab.cpp` body**

```cpp
#include "astra/pda_screen.h"

#include "astra/cyberdeck.h"
#include "astra/program.h"
#include "astra/skill_defs.h"

#include <sstream>

namespace astra {

namespace {

bool has_cat_hacking(const Player& p) {
    return player_has_skill(p, SkillId::Cat_Hacking);
}

std::vector<std::string> tokenize_(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(std::move(tok));
    return out;
}

} // namespace

void PdaScreen::draw_hacking(UIContext& ctx) {
    if (!has_cat_hacking(*player_)) {
        // Locked splash.
        int cy = ctx.height() / 2 - 4;
        ctx.text({.x = ctx.width() / 2 - 14, .y = cy,
                  .content = "» HACKING (LOCKED) «",
                  .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 2,
                  .content = "Unlock 'Hacking' in the Skills tab to access the deck terminal.",
                  .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 4,
                  .content = "Quickhacks (.qh) work without this skill — see below.",
                  .tag = UITag::TextDim});

        // List of equipped quickhacks (visible, usable via H).
        ctx.text({.x = 4, .y = cy + 6, .content = "Loaded quickhacks:", .tag = UITag::TextNormal});
        int row = cy + 7;
        if (player_->equipment.cyberdeck && player_->equipment.cyberdeck->deck) {
            auto& deck = *player_->equipment.cyberdeck->deck;
            int found = 0;
            for (int i = 0; i < deck.stats.slots; ++i) {
                if (!deck.loaded[i] || !deck.loaded[i]->program) continue;
                const ProgramDef* def = find_program(deck.loaded[i]->program->id);
                if (!def || def->kind != ProgramKind::Qh) continue;
                std::string line = std::string("  [") + char('a' + i) + "] " + def->filename;
                ctx.text({.x = 6, .y = row + found, .content = line, .tag = UITag::TextNormal});
                ++found;
            }
            if (found == 0) {
                ctx.text({.x = 6, .y = row, .content = "  (no .qh loaded)", .tag = UITag::TextDim});
            }
        } else {
            ctx.text({.x = 6, .y = row, .content = "  (no cyberdeck equipped)", .tag = UITag::TextDim});
        }
        return;
    }

    // ── Unlocked: render the terminal subwindow ──
    // Header: deck info bar.
    if (player_->equipment.cyberdeck && player_->equipment.cyberdeck->deck) {
        auto& d = *player_->equipment.cyberdeck->deck;
        std::string header = "RAM " + std::to_string(d.ram_current) + "/" +
                             std::to_string(d.stats.ram_max) +
                             "  CPU " + std::to_string(d.stats.cpu) +
                             "  SLOTS " + std::to_string(d.stats.slots) +
                             "  STEALTH +" + std::to_string(d.stats.stealth) +
                             "  COOLING " + std::to_string(d.stats.cooling_rate) + "/turn";
        ctx.text({.x = 2, .y = 1, .content = header, .tag = UITag::TextNormal});
        std::string deck_name = "[ " + player_->equipment.cyberdeck->name + " ]";
        ctx.text({.x = ctx.width() - (int)deck_name.size() - 2, .y = 1,
                  .content = deck_name, .tag = UITag::TextDim});
    } else {
        ctx.text({.x = 2, .y = 1, .content = "(no cyberdeck equipped)",
                  .tag = UITag::TextDim});
    }

    // Scrollback area: 2 lines down to last-2; one prompt line at bottom.
    int top = 3;
    int bottom = ctx.height() - 2;
    int visible = bottom - top;
    int total = (int)hack_term_lines_.size();
    int start = std::max(0, total - visible - hack_term_scroll_);
    int end = std::min(total, start + visible);
    int row = top;
    for (int i = start; i < end; ++i, ++row) {
        ctx.text({.x = 2, .y = row, .content = hack_term_lines_[i].text,
                  .tag = hack_term_lines_[i].tag});
    }

    // Prompt line.
    std::string prompt = "pda> " + hack_term_input_ + "_";
    ctx.text({.x = 2, .y = bottom, .content = prompt, .tag = UITag::TextNormal});
}

void PdaScreen::hack_term_emit(const std::string& line, UITag tag) {
    hack_term_lines_.push_back({line, tag});
    if (hack_term_lines_.size() > 200) {
        hack_term_lines_.erase(hack_term_lines_.begin(),
                               hack_term_lines_.begin() + 50);
    }
}

void PdaScreen::handle_hacking_key(int key) {
    if (!has_cat_hacking(*player_)) return;  // locked: input does nothing

    if (key == '\n' || key == '\r') {
        if (!hack_term_input_.empty()) {
            hack_term_emit("pda> " + hack_term_input_, UITag::TextNormal);
            hack_term_history_.push_back(hack_term_input_);
            if (hack_term_history_.size() > 50) hack_term_history_.erase(hack_term_history_.begin());
            hack_term_run_command(hack_term_input_);
            hack_term_input_.clear();
            hack_term_history_cursor_ = -1;
        }
        return;
    }
    if (key == '\b' || key == 127) {
        if (!hack_term_input_.empty()) hack_term_input_.pop_back();
        return;
    }
    if (key == KEY_UP) {
        if (hack_term_history_.empty()) return;
        if (hack_term_history_cursor_ == -1)
            hack_term_history_cursor_ = (int)hack_term_history_.size() - 1;
        else if (hack_term_history_cursor_ > 0)
            --hack_term_history_cursor_;
        hack_term_input_ = hack_term_history_[hack_term_history_cursor_];
        return;
    }
    if (key == KEY_DOWN) {
        if (hack_term_history_cursor_ == -1) return;
        if (hack_term_history_cursor_ < (int)hack_term_history_.size() - 1) {
            ++hack_term_history_cursor_;
            hack_term_input_ = hack_term_history_[hack_term_history_cursor_];
        } else {
            hack_term_history_cursor_ = -1;
            hack_term_input_.clear();
        }
        return;
    }
    if (key == '\t') {
        // Prefix-match command names.
        static const char* cmds[] = {
            "help", "deck info", "programs ls", "programs load",
            "programs unload", "netmap", "jack -t", "lore",
            "clear", "history"
        };
        for (const char* c : cmds) {
            if (std::string(c).rfind(hack_term_input_, 0) == 0) {
                hack_term_input_ = c;
                hack_term_input_ += ' ';
                return;
            }
        }
        return;
    }
    // Single-key shortcuts (menu fallbacks) — only when input buffer is empty.
    if (hack_term_input_.empty()) {
        switch (key) {
            case '?': hack_term_run_command("help"); return;
            case 'P': hack_term_run_command("programs ls"); return;
            case 'N': hack_term_run_command("netmap"); return;
            case 'L': hack_term_run_command("lore"); return;
        }
    }
    if (key >= ' ' && key < 127) {
        hack_term_input_ += static_cast<char>(key);
        if (hack_term_input_.size() > 64) hack_term_input_.resize(64);
    }
}

void PdaScreen::hack_term_run_command(const std::string& line) {
    auto args = tokenize_(line);
    if (args.empty()) return;
    const std::string& v = args[0];

    if (v == "help") return hack_term_cmd_help();
    if (v == "deck") {
        if (args.size() >= 2 && args[1] == "info") return hack_term_cmd_deck_info();
        hack_term_emit("usage: deck info", UITag::TextDim);
        return;
    }
    if (v == "programs") {
        if (args.size() >= 2 && args[1] == "ls") return hack_term_cmd_programs_ls();
        if (args.size() >= 2 && args[1] == "load")
            return hack_term_cmd_programs_load(args);
        if (args.size() >= 2 && args[1] == "unload")
            return hack_term_cmd_programs_unload(args);
        hack_term_emit("usage: programs <ls|load|unload>", UITag::TextDim);
        return;
    }
    if (v == "netmap")  return hack_term_cmd_netmap();
    if (v == "jack")    return hack_term_cmd_jack(args);
    if (v == "lore")    return hack_term_cmd_lore();
    if (v == "clear")   return hack_term_cmd_clear();
    if (v == "history") return hack_term_cmd_history();
    hack_term_emit("?: unknown command. Try 'help'.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_help() {
    static const char* lines[] = {
        "Commands:",
        "  help                       — this list",
        "  deck info                  — deck stats",
        "  programs ls                — list loaded programs",
        "  programs load <slot> <id>  — load program from inventory",
        "  programs unload <slot>     — unload a slot",
        "  netmap                     — known networks (stub in Plan 2)",
        "  jack -t <node>             — jack in (Grid coming in Plan 3)",
        "  lore                       — decrypted archives",
        "  clear / history",
    };
    for (auto* s : lines) hack_term_emit(s, UITag::TextDim);
}

void PdaScreen::hack_term_cmd_deck_info() {
    if (!player_->equipment.cyberdeck || !player_->equipment.cyberdeck->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *player_->equipment.cyberdeck->deck;
    hack_term_emit("Deck: " + player_->equipment.cyberdeck->name);
    hack_term_emit("  RAM " + std::to_string(d.ram_current) + "/" + std::to_string(d.stats.ram_max));
    hack_term_emit("  CPU " + std::to_string(d.stats.cpu));
    hack_term_emit("  SLOTS " + std::to_string(d.stats.slots));
    hack_term_emit("  STEALTH +" + std::to_string(d.stats.stealth));
    hack_term_emit("  COOLING " + std::to_string(d.stats.cooling_rate) + "/turn");
    hack_term_emit("  HEAT_CAP " + std::to_string(d.stats.heat_cap));
}

void PdaScreen::hack_term_cmd_programs_ls() {
    if (!player_->equipment.cyberdeck || !player_->equipment.cyberdeck->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *player_->equipment.cyberdeck->deck;
    for (int i = 0; i < d.stats.slots; ++i) {
        if (!d.loaded[i] || !d.loaded[i]->program) {
            hack_term_emit("  [" + std::to_string(i) + "] (empty)");
            continue;
        }
        const ProgramDef* def = find_program(d.loaded[i]->program->id);
        if (!def) {
            hack_term_emit("  [" + std::to_string(i) + "] ???");
            continue;
        }
        std::string row = "  [" + std::to_string(i) + "] " + def->filename + "  " +
                          program_kind_short(def->kind) + "  " +
                          std::to_string(def->ram_cost) + " RAM, " +
                          std::to_string(def->heat_cost) + " Heat";
        hack_term_emit(row);
    }
}

void PdaScreen::hack_term_cmd_programs_load(const std::vector<std::string>& args) {
    // args = ["programs", "load", "<slot>", "<id>"]
    if (args.size() < 4) {
        hack_term_emit("usage: programs load <slot> <id>", UITag::TextDim);
        return;
    }
    if (!player_->equipment.cyberdeck || !player_->equipment.cyberdeck->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *player_->equipment.cyberdeck->deck;
    int slot = -1;
    try { slot = std::stoi(args[2]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        hack_term_emit("bad slot.", UITag::TextDim);
        return;
    }
    // Find program in inventory by filename.
    int inv_idx = -1;
    for (size_t i = 0; i < player_->inventory.items.size(); ++i) {
        const auto& it = player_->inventory.items[i];
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def && std::string(def->filename) == args[3]) { inv_idx = (int)i; break; }
    }
    if (inv_idx < 0) {
        hack_term_emit("no such program in inventory.", UITag::TextDim);
        return;
    }
    // Unload current occupant (back to inventory).
    if (d.loaded[slot]) {
        player_->inventory.items.push_back(std::move(*d.loaded[slot]));
        d.loaded[slot].reset();
    }
    d.loaded[slot] = std::move(player_->inventory.items[inv_idx]);
    player_->inventory.items.erase(player_->inventory.items.begin() + inv_idx);
    hack_term_emit("loaded " + args[3] + " into slot " + std::to_string(slot) + ".",
                   UITag::TextNormal);
}

void PdaScreen::hack_term_cmd_programs_unload(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        hack_term_emit("usage: programs unload <slot>", UITag::TextDim);
        return;
    }
    if (!player_->equipment.cyberdeck || !player_->equipment.cyberdeck->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *player_->equipment.cyberdeck->deck;
    int slot = -1;
    try { slot = std::stoi(args[2]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        hack_term_emit("bad slot.", UITag::TextDim);
        return;
    }
    if (!d.loaded[slot]) {
        hack_term_emit("slot already empty.", UITag::TextDim);
        return;
    }
    player_->inventory.items.push_back(std::move(*d.loaded[slot]));
    d.loaded[slot].reset();
    hack_term_emit("unloaded slot " + std::to_string(slot) + ".", UITag::TextNormal);
}

void PdaScreen::hack_term_cmd_netmap() {
    hack_term_emit("netmap: no networks discovered yet (Plan 3 will populate).",
                   UITag::TextDim);
}
void PdaScreen::hack_term_cmd_jack(const std::vector<std::string>& /*args*/) {
    hack_term_emit("The Grid is not yet implemented (Plan 3).", UITag::TextDim);
}
void PdaScreen::hack_term_cmd_lore() {
    hack_term_emit("no decrypted archives (Plan 3+).", UITag::TextDim);
}
void PdaScreen::hack_term_cmd_clear() {
    hack_term_lines_.clear();
}
void PdaScreen::hack_term_cmd_history() {
    for (size_t i = 0; i < hack_term_history_.size(); ++i) {
        hack_term_emit("  " + std::to_string(i) + "  " + hack_term_history_[i]);
    }
}

} // namespace astra
```

- [ ] **Step 3: Wire input through `PdaScreen::handle_input`**

In `src/pda_screen.cpp`, find `bool PdaScreen::handle_input(int key)` and add — after the existing tab-help intercept and BEFORE the per-tab key handlers — a Hacking branch:

```cpp
    if (active_tab_ == PdaTab::Hacking) {
        // Tab navigation (Tab / Shift-Tab) and ESC still work; everything
        // else is consumed by the terminal.
        if (key == '\033' || key == '\t' /* tab nav */ || /* ... existing nav keys */) {
            // fall through to default switch
        } else {
            handle_hacking_key(key);
            return true;
        }
    }
```

(Adapt to the actual existing switch — preserve tab nav and Escape, capture everything else.)

- [ ] **Step 4: Update `tab_help_body(PdaTab::Hacking)`**

In `src/pda_screen.cpp` line ~1519, replace:

```cpp
        case PdaTab::Hacking:
            return "Requires a cyberdeck and the Hacking skill.\n\n"
                   "Feature in development.";
```

with:

```cpp
        case PdaTab::Hacking:
            return "Cyberdeck terminal.\n\n"
                   "Type commands at the pda> prompt. 'help' lists all "
                   "commands. Tab to autocomplete. Up/Down for history.\n\n"
                   "[H in world] Quickhack a hackable target\n"
                   "[?] help / [P] programs ls / [N] netmap / [L] lore";
```

- [ ] **Step 5: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 6: Smoke check**

Run `./build/astra-dev`. Open dev console. Without unlocking the skill, open PDA → Hacking → confirm locked splash. Close, then unlock the skill (`give skill cat_hacking` or whatever the dev verb is — if not present, level up via `xp` until you can spend an SP, or just hard-add a SkillId via console; if no helper exists, defer).

Equip the Pidgin Mark I. Open Hacking tab → terminal renders. Try `help`, `deck info`, `programs ls`, `clear`, `history`, `netmap`, `jack -t foo`, `lore` — each should produce expected output. Try Tab autocomplete.

`give item reboot_optics` then in terminal: `programs load 0 reboot_optics.qh` → should move from inventory into deck slot 0. `programs ls` confirms. `programs unload 0` returns it to inventory.

- [ ] **Step 7: Commit**

```
git add include/astra/pda_screen.h src/pda_hacking_tab.cpp src/pda_screen.cpp
git commit -m "feat(hacking): PDA Hacking tab — locked splash + bounded terminal

When Cat_Hacking is locked: shows splash + list of loaded .qh programs
(usable via H). When unlocked: terminal subwindow with 10-command
bounded set, Tab-completion, Up/Down history, and shortcut menu keys
(?/P/N/L)."
```

---

## Task 12 — Detection ↔ reputation coupling

**Files:**
- Modify: `src/hacking_system.cpp`
- Modify: `src/faction.cpp` (or `src/game.cpp` if reputation hooks live there)

The Detection thresholds:
- ≥50: nearby NPCs investigate (set their `move_target_*` to last-quickhack location).
- ≥75: -10 reputation hit on the local zone's dominant faction.
- =100: zone alarm — flip all `Hackable.state` to `Alarmed`; trigger existing reputation hostility broadcast.

- [ ] **Step 1: Add a callback hook to detection-counter changes**

In `src/hacking_system.cpp`'s `add_detection`, after clamping the value, check for threshold crossings:

```cpp
void HackingSystem::add_detection(uint32_t map_id, int delta) {
    auto& st = detection_[map_id];
    int prev = st.value;
    st.value = std::clamp(st.value + delta, kDetectionMin, kDetectionMax);

    // Threshold callbacks — fire only when crossing upward.
    auto crossed = [&](int t) { return prev < t && st.value >= t; };
    if (crossed(50)) on_detection_threshold_(map_id, 50);
    if (crossed(75)) on_detection_threshold_(map_id, 75);
    if (crossed(100)) on_detection_threshold_(map_id, 100);
}
```

Add the helper and a member pointer back to Game (passed in via `set_game()` or via a single owning callback):

```cpp
private:
    Game* game_ = nullptr;
public:
    void bind_game(Game* g) { game_ = g; }
private:
    void on_detection_threshold_(uint32_t map_id, int threshold);
```

In `src/hacking_system.cpp`:

```cpp
void HackingSystem::on_detection_threshold_(uint32_t /*map_id*/, int threshold) {
    if (!game_) return;
    switch (threshold) {
        case 50:
            game_->log("Detected: nearby personnel are investigating.");
            // For v1, no live NPC redirect — Plan 3 will hook this into the
            // noise_event system to point every faction NPC's move_target_*
            // at the last-known quickhack tile.
            break;
        case 75: {
            game_->log("Local network is broadcasting your signature.");
            // Pick the dominant faction in the current zone and slam rep -10.
            std::string fac = game_->dominant_faction_in_current_map();
            if (!fac.empty()) {
                modify_faction_standing(game_->player(), fac, -10);
                game_->log("Reputation with " + fac + " worsens.");
            }
            break;
        }
        case 100: {
            game_->log("ZONE ALARM. The grid lights up.");
            // Flip all Hackable on this map to Alarmed.
            auto& m = game_->world().map_mut();
            for (int fid = 0; fid < m.fixture_count(); ++fid) {
                auto& fd = m.fixture_mut(fid);
                if (fd.cyber) fd.cyber->state = HackState::Alarmed;
            }
            for (auto& npc : game_->world().npcs_mut()) {
                if (npc.cyber) npc.cyber->state = HackState::Alarmed;
            }
            // Existing reputation-driven hostility plan flips NPC behavior;
            // we simply ensure it has fresh fuel by tanking faction rep.
            std::string fac = game_->dominant_faction_in_current_map();
            if (!fac.empty()) {
                modify_faction_standing(game_->player(), fac, -25);
            }
            break;
        }
    }
}
```

- [ ] **Step 2: Add `Game::dominant_faction_in_current_map()`**

In `include/astra/game.h`:
```cpp
    std::string dominant_faction_in_current_map() const;
```

In `src/game.cpp` (or `src/game_world.cpp`):
```cpp
std::string Game::dominant_faction_in_current_map() const {
    std::unordered_map<std::string, int> counts;
    for (const auto& npc : world_.npcs()) {
        if (!npc.faction.empty()) counts[npc.faction]++;
    }
    std::string best; int best_n = 0;
    for (const auto& [k, v] : counts) {
        if (v > best_n) { best_n = v; best = k; }
    }
    return best;
}
```

- [ ] **Step 3: Bind game pointer at startup**

In `Game`'s constructor or init function, call `hacking_.bind_game(this);` after both objects exist.

- [ ] **Step 4: Add `fixture_count()` to TileMap if missing**

Check `tilemap.h` for an accessor. If absent, add:
```cpp
    int fixture_count() const { return static_cast<int>(fixtures_.size()); }
```

- [ ] **Step 5: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 6: Smoke check**

Once Task 14 lands a `detection <n>` dev verb, this becomes testable. For now: confirm clean build and no regressions in the detection counter (it should still tick down).

- [ ] **Step 7: Commit**

```
git add include/astra/hacking_system.h src/hacking_system.cpp \
        include/astra/game.h src/game.cpp \
        include/astra/tilemap.h
git commit -m "feat(hacking): detection thresholds wire into reputation

50: 'investigating' log line.
75: -10 with dominant faction in zone.
100: ZONE ALARM — every Hackable on map flips to Alarmed, dominant
faction takes -25 (existing reputation-driven hostility kicks in).
Crossings fire only when value crosses upward."
```

---

## Task 13 — Save schema bump (51 → 52)

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

Persist:
- Per-zone Detection counter → `MapState::detection`.
- Hackable state on fixtures (the cyber field is already on `FixtureData`, but we need to extend the fixture serializer).
- Hackable state on NPCs (the cyber field on `Npc`).
- Cyberdeck loadout on the player's equipment (the deck's `loaded` array of programs).

Per `feedback_no_backcompat_pre_ship.md`: bump version, reject older saves, no migration shims.

- [ ] **Step 1: Bump `SAVE_FILE_VERSION`**

In `include/astra/save_file.h` line 31:
```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 52;   // v52: hacking (cyberdeck, programs, Hackable, Detection)
```

- [ ] **Step 2: Extend `MapState`**

In `save_file.h`, add to `MapState`:
```cpp
    int detection = 0;          // v52: per-zone Detection counter [0,100]
```

- [ ] **Step 3: Extend `FixtureData` serialization**

In `src/save_file.cpp`, find the fixture write/read functions (search for `FixtureData` or `fixture.type`). After existing fields, write/read the `cyber` optional. Use a 1-byte presence flag + the inline struct:

```cpp
// Write
{
    uint8_t has = fd.cyber ? 1 : 0;
    write_u8(out, has);
    if (has) {
        write_u8(out, static_cast<uint8_t>(fd.cyber->device_kind));
        write_i32(out, fd.cyber->security_tier);
        write_u32(out, fd.cyber->network_id);
        write_u8(out, static_cast<uint8_t>(fd.cyber->state));
        write_i32(out, fd.cyber->jack_in_node_id);
        write_i32(out, fd.cyber->state_ticks_left);
        write_u16(out, static_cast<uint16_t>(fd.cyber->available_qh.size()));
        for (auto pid : fd.cyber->available_qh)
            write_u16(out, static_cast<uint16_t>(pid));
    }
}

// Read
{
    uint8_t has = read_u8(in);
    if (has) {
        Hackable h;
        h.device_kind = static_cast<DeviceKind>(read_u8(in));
        h.security_tier = read_i32(in);
        h.network_id = read_u32(in);
        h.state = static_cast<HackState>(read_u8(in));
        h.jack_in_node_id = read_i32(in);
        h.state_ticks_left = read_i32(in);
        uint16_t n = read_u16(in);
        h.available_qh.reserve(n);
        for (uint16_t i = 0; i < n; ++i)
            h.available_qh.push_back(static_cast<ProgramId>(read_u16(in)));
        fd.cyber = std::move(h);
    }
}
```

(Use whichever helper functions `save_file.cpp` already has — `write_u8`/`read_u8`/etc. — adapt naming as needed.)

- [ ] **Step 4: Extend NPC serialization**

Mirror the same `cyber` write/read in the `Npc` serializer.

- [ ] **Step 5: Extend `MapState` serialization for `detection`**

In the MapState block, add a `write_i32(out, ms.detection)` / `ms.detection = read_i32(in)` pair.

- [ ] **Step 6: Extend cyberdeck/program serialization on `Item`**

In `src/save_file.cpp`'s Item read/write, after the existing optional fields (energy, ranged, etc.), add:

```cpp
// Write
{
    uint8_t has = it.deck ? 1 : 0;
    write_u8(out, has);
    if (has) {
        write_i32(out, it.deck->stats.ram_max);
        write_i32(out, it.deck->stats.cpu);
        write_i32(out, it.deck->stats.slots);
        write_i32(out, it.deck->stats.stealth);
        write_i32(out, it.deck->stats.cooling_rate);
        write_i32(out, it.deck->stats.heat_cap);
        write_i32(out, it.deck->ram_current);
        write_i32(out, it.deck->heat_current);
        // Loaded programs: write each as a recursive Item or as program-id only.
        for (int i = 0; i < kCyberdeckMaxSlots; ++i) {
            uint8_t sl = it.deck->loaded[i] ? 1 : 0;
            write_u8(out, sl);
            if (sl) write_item(out, *it.deck->loaded[i]);  // recursive
        }
    }
}
{
    uint8_t has = it.program ? 1 : 0;
    write_u8(out, has);
    if (has) write_u16(out, static_cast<uint16_t>(it.program->id));
}
```

```cpp
// Read
{
    uint8_t has = read_u8(in);
    if (has) {
        CyberdeckData d;
        d.stats.ram_max      = read_i32(in);
        d.stats.cpu          = read_i32(in);
        d.stats.slots        = read_i32(in);
        d.stats.stealth      = read_i32(in);
        d.stats.cooling_rate = read_i32(in);
        d.stats.heat_cap     = read_i32(in);
        d.ram_current        = read_i32(in);
        d.heat_current       = read_i32(in);
        for (int i = 0; i < kCyberdeckMaxSlots; ++i) {
            if (read_u8(in)) d.loaded[i] = read_item(in);
        }
        it.deck = std::move(d);
    }
}
{
    uint8_t has = read_u8(in);
    if (has) {
        ProgramData p;
        p.id = static_cast<ProgramId>(read_u16(in));
        it.program = p;
    }
}
```

(`write_item` / `read_item` already exist in `save_file.cpp`. Recursing through Item is safe because cyberdecks contain programs but programs do not contain decks.)

- [ ] **Step 7: Persist HackingSystem detection map**

The detection state is keyed by `map_id` and lives outside `MapState` in the in-memory `HackingSystem`. We persist it via the per-map field on `MapState`. On load, populate `hacking_.detection_map_mut()` from each `MapState::detection`. On save, copy from detection_map_mut().

In `src/save_system.cpp` (where saves get built from Game), find the per-map serialization loop and:

(a) On write, before serializing each MapState:
```cpp
ms.detection = game.hacking().detection(ms.map_id);
```

(b) On load, after restoring all MapStates:
```cpp
game.hacking().clear_all_detection();
for (const auto& ms : data.maps) {
    if (ms.detection > 0) {
        game.hacking().detection_map_mut()[ms.map_id] = {ms.detection, 0};
    }
}
```

- [ ] **Step 8: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 9: Smoke check**

Run `./build/astra-dev`. Spawn a deck, equip it, load a program. Save. Quit. Reload. Verify the loadout is intact.

Old saves (v51 or earlier) should be **rejected** with the existing version-check rejection path (no migration).

- [ ] **Step 10: Commit**

```
git add include/astra/save_file.h src/save_file.cpp src/save_system.cpp
git commit -m "feat(save): bump v51 -> v52 for hacking persistence

Persist Hackable cyber field on FixtureData and Npc, MapState::detection
counter, and Item::deck (CyberdeckData with loaded programs) +
Item::program payload. Per project rule, no migration shim — older
saves are rejected on load."
```

---

## Task 14 — Dev console + docs

**Files:**
- Modify: `src/dev_console.cpp`
- Modify: `docs/items.md`
- Modify: `docs/mechanics.md`
- Modify: `docs/roadmap.md`

Add a small set of dev verbs that make Plan 2 testable end-to-end, then update player-facing docs and tick the roadmap entry.

- [ ] **Step 1: Add `give skill <id>`**

In `src/dev_console.cpp`, find the existing `give` verb dispatcher. Add a sub-handler:

```cpp
else if (args.size() >= 2 && args[0] == "give" && args[1] == "skill") {
    if (args.size() < 3) { log("usage: give skill <id|name>"); return; }
    // Map "cat_hacking" → SkillId::Cat_Hacking, etc.
    SkillId target = parse_skill_arg_(args[2]);
    if (target == SkillId{}) { log("unknown skill"); return; }
    // Add to learned_skills if not present.
    if (std::find(player.learned_skills.begin(), player.learned_skills.end(), target) == player.learned_skills.end()) {
        player.learned_skills.push_back(target);
        log("Granted skill: " + std::string(find_skill(target) ? find_skill(target)->name : "?"));
    } else {
        log("Already learned.");
    }
}
```

`parse_skill_arg_` is a small lookup: try matching against every category's `unlock_id` name in the catalog.

- [ ] **Step 2: Add `spawn-hackable <kind>`**

```cpp
else if (verb == "spawn-hackable") {
    if (args.size() < 2) {
        log("usage: spawn-hackable <turret|camera|door|conduit|console>");
        return;
    }
    DeviceKind k;
    FixtureType ft;
    if      (args[1] == "turret")  { k = DeviceKind::Turret;           ft = FixtureType::Console; }
    else if (args[1] == "camera")  { k = DeviceKind::Camera;           ft = FixtureType::Console; }
    else if (args[1] == "door")    { k = DeviceKind::Door;             ft = FixtureType::Door; }
    else if (args[1] == "conduit") { k = DeviceKind::PowerConduit;     ft = FixtureType::Conduit; }
    else if (args[1] == "console") { k = DeviceKind::PrecursorConsole; ft = FixtureType::Console; }
    else { log("unknown kind"); return; }

    // Find an open adjacent tile to the player.
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    bool placed = false;
    for (int i = 0; i < 4 && !placed; ++i) {
        int nx = player.x + dx[i];
        int ny = player.y + dy[i];
        if (game.world().map().passable(nx, ny) &&
            game.world().map().fixture_id(nx, ny) < 0) {
            FixtureData fd = make_fixture(ft);
            fd.interactable = true;
            fd.cyber = make_hackable(k, 1);
            game.world().map_mut().add_fixture(nx, ny, fd);
            log("Placed " + std::string(device_kind_name(k)) + " at (" +
                std::to_string(nx) + "," + std::to_string(ny) + ").");
            placed = true;
        }
    }
    if (!placed) log("no adjacent tile available.");
}
```

- [ ] **Step 3: Add `detection <n>` for testing**

```cpp
else if (verb == "detection") {
    if (args.size() < 2) {
        log("usage: detection <0..100>  (sets active map's Detection)");
        return;
    }
    int n = std::stoi(args[1]);
    uint32_t mid = game.world().current_map_id();
    int cur = game.hacking().detection(mid);
    game.hacking().add_detection(mid, n - cur);
    log("Detection on map " + std::to_string(mid) + " = " + std::to_string(n));
}
```

- [ ] **Step 4: Update help text**

In `dev_console.cpp` `help` handler, add:
```cpp
log("  give skill <id|name>          - learn a skill");
log("  spawn-hackable <kind>         - place a hackable at adjacent tile");
log("  detection <n>                 - set zone detection counter");
```

- [ ] **Step 5: Update `docs/items.md`**

Add a new section near the existing weapon/armor listings:

```markdown
## Cyberdecks

A **cyberdeck** is the device required to fire any quickhack (`.qh`) program
in the world or to jack into the Grid (Plan 3+). Decks are equipped in the
`Cyberdeck` slot.

| Tier | Name              | RAM | CPU | Slots | Stealth | Heat cap | Notes |
|------|-------------------|-----|-----|-------|---------|----------|-------|
| 1    | Pidgin Mark I     | 4   | 1   | 3     | +0      | 10       | Pawn-shop deck. |
| 2    | Polyglot DCK-2    | 8   | 2   | 4     | +1      | 12       | Corp surplus. |

## Programs

Programs are loadable items. Their kind determines where they fire:

- **`.exe` (ATK / STL / UTL)** — used in the Grid (Plan 3).
- **`.qh` (QH)** — fires in the real world via the `H` keybind. Spends RAM
  and bumps the zone Detection counter.

| Filename                | Kind | Tier | RAM | Heat | Effect |
|-------------------------|------|------|-----|------|--------|
| icebreaker_lite.exe     | ATK  | 1    | 2   | 2    | (Plan 3) |
| ghost_trace.exe         | STL  | 1    | 3   | 0    | (Plan 3) |
| cooldown.exe            | STL  | 1    | 2   | 0    | (Plan 3) |
| breach.exe              | UTL  | 1    | 3   | 3    | (Plan 3) |
| decrypt.exe             | UTL  | 1    | 2   | 1    | (Plan 3) |
| reboot_optics.qh        | QH   | 1    | 1   | —    | Blinds camera/turret 4 turns. +1 Detection. |
| friendly_fire.qh        | QH   | 2    | 3   | —    | Turret targets allies 2 turns. +3 Detection. |
| data_leech.qh           | QH   | 1    | 2   | —    | Skim credits from a hackable. +2 Detection. |

## Code Fragments

Crafting material category. T1/T2/T3 fragments are inputs into program
schematics in the Tinkering tab.
```

- [ ] **Step 6: Update `docs/mechanics.md`**

Add a Hacking section:

```markdown
## Hacking — Detection (B-layer, Plan 2)

The **Detection** counter is per-zone, range `[0, 100]`. Every quickhack
fired in the world adds 1-3 to Detection (per program). Detection decays
`-1 every 5 ticks` while the zone is in steady state.

Threshold breakpoints:

- **≥ 50** — local NPCs investigate (log + future move-target redirect).
- **≥ 75** — dominant faction takes a `-10` reputation hit.
- **= 100** — ZONE ALARM. Every `Hackable` on the map flips to `Alarmed`,
  and the dominant faction takes another `-25`. Existing
  reputation-driven hostility runs from there.

### Heat → Trace coupling (forward reference, Plan 3)

Programs in the Grid spend `heat_cost` per fire. While Heat > 5, the
Trace counter ticks `+1/turn` extra. Heat decays at `cooling_rate` per
turn (set by the equipped deck's stats). Plan 3 will implement Heat and
Trace; Plan 2 ships the storage on the deck without using it.
```

- [ ] **Step 7: Update `docs/roadmap.md`**

Tick the box for Plan 2 (find the Hacking section and add a checked sub-bullet for "Plan 2 — Cyberdeck + Programs + Quickhacks (B-layer)").

- [ ] **Step 8: Build**

```
cmake --build build -j
```
Expected: clean.

- [ ] **Step 9: End-to-end smoke check**

Run `./build/astra-dev`. In dev console:
```
give item pidgin_mk1
give item reboot_optics
give skill cat_hacking
spawn-hackable camera
```
Equip the deck. Open PDA → Hacking → `programs load 0 reboot_optics.qh`. Close PDA. Walk adjacent to the placed Camera, press `H` → cursor; arrow-key onto the Camera; Enter → program picker; `a` → log "Reboot Optics executed." + "The Camera judders and flickers offline."

Run `detection 80` → log shows alarm at 100 once you fire two more quickhacks. `detection 100` → ZONE ALARM log line. Save / load / verify deck loadout persists.

- [ ] **Step 10: Commit**

```
git add src/dev_console.cpp \
        docs/items.md docs/mechanics.md docs/roadmap.md
git commit -m "feat(hacking): dev verbs + docs for cyberdeck/programs/Detection

dev console: give skill, spawn-hackable, detection. Docs: items.md
adds Cyberdeck/Program/Code Fragment sections; mechanics.md adds the
Detection formula + a Heat->Trace forward reference; roadmap.md ticks
the Plan 2 box."
```

---

## After all 14 tasks

- Run the full smoke loop one more time end-to-end.
- Verify `cmake --build build -j` is clean.
- Per `superpowers:finishing-a-development-branch`: present merge options to the user.
- Per `feedback_clean_commits.md`: if any task produced fix-on-fix commits, squash before merging. Plan 1 ended with 4 logical commits (spec / plan / refactor / etc.); Plan 2 should likely end with ~4-6 logical commits after squashing.

---

## Self-review notes

Spec coverage check (against §6 v1 cut for the B-layer):

- [x] Cat_Hacking skill category (full enum, 8 unlocks; only Cat_Hacking has runtime gate role) — Task 1
- [x] Cyberdeck item type, 2 tiers, EquipSlot — Task 3
- [x] Program item type, 8 starter (3 active QH) — Task 4
- [x] Code-fragment material category + 3 tinkering recipes — Task 5
- [x] Hackable component on FixtureData + Npc — Task 2
- [x] Hackable wired onto Turret/Camera/Door/PowerConduit/PrecursorConsole — via dev console spawner Task 14 (full map-gen integration is future work, called out below)
- [x] Quickhack flow with H keybind + cursor + popup — Task 8
- [x] PDA Hacking tab filled in: locked splash + bounded terminal — Task 11
- [x] Detection counter per zone — Task 7, threshold coupling Task 12
- [x] Save schema bump (51 → 52) — Task 13
- [x] Reputation/Detection coupling at 50/75/100 — Task 12
- [x] Loot integration — Task 6
- [x] Docs: items.md, mechanics.md, roadmap.md — Task 14
- [x] Dev console aids — Task 14

Explicitly deferred (per kickoff prompt and spec §6):

- Map-gen integration of Hackable fixtures (auto-placing Camera/Turret/PrecursorConsole during dungeon/station gen) — future work. v1 ships dev-console spawning only.
- Active runtime effects for the 5 .exe programs (icebreaker_lite, ghost_trace, cooldown, breach, decrypt) — Plan 3.
- Skill effects beyond `Cat_Hacking`'s gate role — Plan 3-4.
- Grid mode, sectors, ICE, Trace — Plan 3.
- consciousness.dat persistence + Sgr A* rebirth wiring + non-hacker access — Plan 4.
- Camera/Turret hackable on existing Sentry Drone NPCs — could be added in Task 2 by retrofitting `npc_defs.cpp`, but per spec §6 v1 the deployable turret stays out of scope. Listed as future work.

Type consistency check:

- `ProgramId` — same enum referenced in `program.h`, `hackable.h`, `program_effects.h`, save format. ✓
- `DeviceKind` — same enum in `hackable.h`, `program.h` (forward-declared), `program_effects.cpp`, dev console. ✓
- `CyberdeckData` — only mutated through `Equipment::cyberdeck->deck` everywhere. ✓
- `kCyberdeckMaxSlots = 6` — used in cyberdeck.h, save_file.cpp, pda_hacking_tab.cpp consistently. ✓

Placeholder scan: every "TBD" / "Plan 3" / "future work" label is paired with a runtime stub that does *something* visible (a log line, a no-op effect, a default catalog entry). No empty placeholders.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-29-hacking-cyberdeck-quickhacks.md`.

Per `feedback_plan_straight_to_impl.md`, proceeding directly to **Subagent-Driven Development** without re-asking — that was the user's standing preference for this branch.
