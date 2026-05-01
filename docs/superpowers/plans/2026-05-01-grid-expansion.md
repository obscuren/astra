# Plan 5 — Grid Expansion & Change: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the LAN redesign on top of Plan 4: tag-driven capability model (DeviceKind retired), LAN auto-registration of every electrical fixture, A+B+E layered LAN sector generator, Connector + DeepGridGateway tile types, deep-Grid sector expansion (~60×40 with Anchor/Atlas/Frontier regions, Your.Anchor v1), nmap/ping/jack with IPs, dynamic LAN regeneration on dev spawn, tile-mutation persistence, and the 8 stitching gaps from the handoff.

**Architecture:** Approach B from the spec — 4 mergeable cuts. Each cut is internally complete and shippable.
1. Tag refactor + map-gen content pass + dev-console rewrite + persistence types.
2. LAN sector generator + mid-jack-in traversal + multi-Gateway encoding + tile-mutation persistence wiring.
3. Deep-Grid expansion + WarpAnchor tiles + Atlas + galaxy reseed.
4. nmap/ping/jack/lore + AI-contacts schema + stub cleanup + nmap-side breach.

**Tech Stack:** C++20, CMake, hand-rolled binary save format, the project's dev-console verb system for manual verification (no unit-test framework — verification is `cmake --build build -DDEV=ON` + dev-verb-driven smoke tests).

**Spec:** `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
**Parent specs:** `docs/superpowers/specs/2026-04-29-hacking-design.md`, `docs/superpowers/specs/2026-04-30-hacking-deep-grid-design.md`
**Handoff:** `docs/plans/2026-05-01-grid-loop-handoff.md`

---

## File map

**New files:**
- `include/astra/lan.h`, `src/lan.cpp` — `LanMetadata`, `LanRoom`, `LanFlavour`, auto-registration sweep, `lan_full_reset`, IP allocation.
- `include/astra/lan_sector_generator.h`, `src/lan_sector_generator.cpp` — A+B+E layered procedural generator.
- `include/astra/sector_runtime_state.h`, `src/sector_runtime_state.cpp` — `SectorMutation`, `SectorRuntimeState`, mutation overlay logic.
- `include/astra/grid_nmap_widget.h`, `src/grid_nmap_widget.cpp` — renamed widget (was `grid_netmap_widget`).
- `include/astra/deep_grid_sector.h`, `src/deep_grid_sector.cpp` — hand-authored 60×40 base; `WarpAnchor` tile + data; Atlas population.
- `include/astra/ip.h`, `src/ip.cpp` — `format_ip` / `parse_ip` helpers; subnet-base derivation.

**Modified files:**
- `include/astra/hackable.h`, `src/hackable.cpp` — `HackTag` enum, `HackTagMask`, `tags_for_fixture` table; `Hackable` struct (drop `device_kind` + `available_qh`; add `tags` + `ip`); retire `DeviceKind`.
- `include/astra/program.h`, `src/program.cpp` — `ProgramDef::target_filter` type change; 9 program-def updates.
- `src/program_effects.cpp` — `apply_breach_grid` fix.
- `include/astra/grid_network.h`, `src/grid_network.cpp` — `LanRoot` node kind; retire `RegionalDarknet` + `entry_redirect`; auto-registration helpers.
- `include/astra/grid_sector.h`, `src/grid_sector.cpp` — `Connector`, `DeepGridGateway`, `WarpAnchor` `GridTile`s; subnet sector unchanged; regional retired.
- `include/astra/grid_theme.h` — palette additions for `Connector` + `DeepGridGateway` + `WarpAnchor`.
- `src/grid_renderer.cpp` — box-drawing wall rendering; new tiles' glyph paths.
- `src/hacking_system.cpp` — `jack_in` sector dispatch (LAN / Subnet / DeepGridAnchor); mid-jack-in `traverse_to`.
- `src/grid_input.cpp` — `⊕` `WarpAnchor` tile interactions; sector-traversal call sites.
- `src/grid_anchor_layout.cpp` — replaced by `deep_grid_sector.cpp`'s hand-authored layout.
- `src/rebirth_sequence.cpp` — `Game::start_new_galaxy(fresh_seed)` call.
- `include/astra/consciousness_save.h`, `src/consciousness_save.cpp` — schema v2; `deep_grid_base` blob; `WarpAnchor` list; `ai_contacts` schema; `deep_grid_sector_state`.
- `src/pda_hacking_tab.cpp` — `nmap` (with `-l`/`-m` flags); `ping <ip>`; `jack <ip>`; `lore`; help/man cleanup.
- `src/game_input.cpp` — fixture-menu `JackInPort` tag check (line 724 region).
- `src/dev_console.cpp` — unified `:spawn` verb; remove `:spawn-hackable`; remove `:spawn-cyber` if present.
- `include/astra/world.h`, `src/world.cpp` — `on_hackable_removed`, `lan_full_reset` hooks; `lan_metadata_for(map_id)` accessor.
- `include/astra/save_file.h`, `src/save_file.cpp` — bump `SAVE_FILE_VERSION 59 → 60`; reject v59; add `Hackable.tags`/`.ip`, `LanMetadata`, `SectorRuntimeState` (de)serialization.
- `src/dev_console.cpp` — help text for unified `:spawn`; existing `:spawn-trap` stays.
- `docs/mechanics.md` — Hacking section updates: tags, LAN model, deep-Grid expansion, persistence rules.
- `docs/items.md` — implant tag stamps.
- `docs/roadmap.md` — Plan 5 done check; Plan 7 expanded scope (Your.Anchor, darknet).

**Map-gen pipelines that gain a one-line tag-Hackable attachment** (Cut 1, Tasks 13-15):
- `src/settlement_generator.cpp` — terminals, lights, locks, vending.
- `src/station_generator.cpp` (or wherever station interiors place fixtures).
- `src/asteroid_generator.cpp`.
- `src/ruin_generator.cpp` — Precursor consoles already wired; add tags.
- `src/dungeon_generator.cpp` — sparse hackables in mid/late dungeons.
- `src/ship_interior_generator.cpp` — CommandTerminal, ShipTerminal, conduits.
- `src/crashed_ship_generator.cpp` — Precursor consoles + conduits.
- `src/npc_factory.cpp` (or wherever NPC implants are stamped) — `npc.cyber` for cybernetic NPCs.

---

## Conventions

Every task follows this rhythm:
1. Write or modify code per the Files list.
2. Run `cmake -B build -DDEV=ON && cmake --build build`. Expected: success.
3. (Where applicable) Manual smoke test using a dev verb listed in the task. Expected behavior is stated.
4. `git add <files> && git commit` with the message shown.

Astra has no unit-test framework; verification is the build + dev-verb smoke test. Where a task is purely structural (e.g. enum addition with no runtime effect yet), the verification is "the build still passes."

Commits are at task granularity. Within a task, intermediate states may be uncommitted. The final commit lands at the end of the task. Plan 4 was 22 commits across 14 tasks; Plan 5 should land in 50-65 commits across the four cuts.

Never use `git rebase -i` or `git add -i`. Use `git add <paths>` and write commit messages directly via HEREDOC.

---

# Cut 1 — Tag refactor + map-gen content pass

Goal: every electrical fixture in the world is hackable through a tag-driven model. `DeviceKind` is gone. The LAN graph auto-registers from the world map. Dev `:spawn` is unified. Save schema is bumped. **No LAN sector or deep-Grid changes yet** — that's Cut 2-3. After this cut, jacking into the existing PrecursorConsole still works (regional darknet path stays) but every electrical fixture is in the LAN graph and visible in the existing netmap widget.

## Task 1 — `HackTag` enum + `tags_for_fixture` table

**Files:**
- Modify: `include/astra/hackable.h` — add `HackTag`, `HackTagMask`, `TagSet`, declare `tags_for_fixture`.
- Modify: `src/hackable.cpp` — implement `tags_for_fixture` table.

**Goal:** Define the capability tag bitmask and the per-FixtureType lookup. No runtime effects yet — pure data.

- [ ] **Step 1.1 — Add the enum and aliases to `hackable.h`**

```cpp
// include/astra/hackable.h (add near the top, before existing struct)
#include "astra/tilemap.h"   // for FixtureType

namespace astra {

enum class HackTag : uint32_t {
    None        = 0,
    Electronic  = 1u << 0,
    Locked      = 1u << 1,
    PowerNode   = 1u << 2,
    DataStore   = 1u << 3,
    HasOptics   = 1u << 4,
    Weaponized  = 1u << 5,
    Mobile      = 1u << 6,
    AlienTech   = 1u << 7,
    JackInPort  = 1u << 8,
};

using HackTagMask = uint32_t;
using TagSet      = HackTagMask;   // alias used at the program-filter site for AND-within readability

inline HackTagMask operator|(HackTag a, HackTag b) {
    return static_cast<HackTagMask>(a) | static_cast<HackTagMask>(b);
}
inline HackTagMask operator|(HackTagMask m, HackTag t) {
    return m | static_cast<HackTagMask>(t);
}
inline bool has_tag(HackTagMask m, HackTag t) {
    return (m & static_cast<HackTagMask>(t)) != 0;
}
inline bool covers(HackTagMask device, TagSet required) {
    return (device & required) == required;
}

HackTagMask tags_for_fixture(FixtureType type);

} // namespace astra
```

- [ ] **Step 1.2 — Implement `tags_for_fixture` in `hackable.cpp`**

```cpp
// src/hackable.cpp (add near the top, after existing helpers)
HackTagMask tags_for_fixture(FixtureType type) {
    using H = HackTag;
    switch (type) {
        // JackInPort + DataStore terminals
        case FixtureType::Console:
        case FixtureType::CommandTerminal:
        case FixtureType::ShipTerminal:
        case FixtureType::DataTerminal:
        case FixtureType::StarChart:
        case FixtureType::StarChartL:
        case FixtureType::StarChartR:
            return H::Electronic | H::DataStore | H::JackInPort;

        // Locked electronic surfaces
        case FixtureType::Door:    return H::Electronic | H::Locked;
        case FixtureType::Gate:    return H::Electronic | H::Locked;

        // Power / lighting
        case FixtureType::Conduit:    return H::Electronic | H::PowerNode;
        case FixtureType::Lamp:       return H::Electronic | H::PowerNode;
        case FixtureType::HoloLight:  return H::Electronic | H::PowerNode;
        case FixtureType::Torch:      return H::Electronic | H::PowerNode;

        // Commerce + stash
        case FixtureType::HealPod:       return H::Electronic | H::DataStore;
        case FixtureType::FoodTerminal:  return H::Electronic | H::DataStore;
        case FixtureType::WeaponDisplay: return H::Electronic | H::DataStore;
        case FixtureType::RepairBench:   return H::Electronic;
        case FixtureType::SupplyLocker:  return H::Electronic | H::Locked | H::DataStore;
        case FixtureType::Locker:        return H::Electronic | H::Locked | H::DataStore;
        case FixtureType::RestPod:       return H::Electronic;

        // Everything else: not hackable.
        default: return 0;
    }
}
```

- [ ] **Step 1.3 — Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: success (no callers yet).

- [ ] **Step 1.4 — Commit**

```
git add include/astra/hackable.h src/hackable.cpp
git commit -m "feat(hacking): HackTag enum + tags_for_fixture table"
```

## Task 2 — `Hackable` struct rework

**Files:**
- Modify: `include/astra/hackable.h` — drop `device_kind` and `available_qh`; add `tags` and `ip`.
- Modify: `src/hackable.cpp` — rewrite `make_hackable` to take a `FixtureType`.

**Goal:** Replace the kind-keyed Hackable with a tag-keyed Hackable. No callers updated yet — those follow in Task 4. Build will fail at call sites; expected.

- [ ] **Step 2.1 — Update `Hackable` in `hackable.h`**

```cpp
// include/astra/hackable.h — replace existing Hackable struct
struct Hackable {
    HackTagMask tags = 0;
    uint32_t    ip   = 0;             // packed 10.X.Y.host; assigned by LAN registration
    int         security_tier = 1;
    uint32_t    network_id    = 0;
    HackState   state         = HackState::Clean;
    int         jack_in_node_id   = -1;
    int         state_ticks_left = 0;
    std::vector<LoreFragmentSeed> lore_fragments;
    int         soul_mirror_progress = 0;
};

// Replaces the old DeviceKind-keyed factory; returns a Hackable populated
// from the fixture type's tag mask. Returns Hackable with tags=0 if the
// fixture is not hackable.
Hackable make_hackable(FixtureType type, int tier);

// Retired: DeviceKind enum, device_kind_name, the old make_hackable(DeviceKind, int).
// Removal happens in Task 5 once all callers are migrated.
```

- [ ] **Step 2.2 — Rewrite `make_hackable` in `hackable.cpp`**

```cpp
// src/hackable.cpp — new make_hackable
Hackable make_hackable(FixtureType type, int tier) {
    Hackable h;
    h.tags = tags_for_fixture(type);
    h.security_tier = tier;
    return h;
}
```

Keep the old `make_hackable(DeviceKind, int)` and `device_kind_name` defined but mark with `[[deprecated]]` for one task — they get removed in Task 5.

- [ ] **Step 2.3 — Build (expect failures)**

Run: `cmake --build build`
Expected: errors at call sites that read `cyber.device_kind` or `cyber.available_qh`. Note them; Task 4 fixes them.

- [ ] **Step 2.4 — Commit (with broken build)**

```
git add include/astra/hackable.h src/hackable.cpp
git commit -m "wip(hacking): tag-keyed Hackable struct (callers broken)"
```

WIP commit is intentional — Task 3 lands the program defs, Task 4 lands the call-site sweep. Build returns to green by end of Task 4.

## Task 3 — `ProgramDef::target_filter` type change + 9 program defs

**Files:**
- Modify: `include/astra/program.h` — `target_filter` from `vector<DeviceKind>` to `vector<TagSet>`.
- Modify: `src/program.cpp` — rewrite the 9 program registry entries with new filter shape.

**Goal:** Programs declare required tag sets. Filter semantics: AND within a TagSet, OR across the vector.

- [ ] **Step 3.1 — Update `ProgramDef` in `program.h`**

```cpp
// include/astra/program.h
#include "astra/hackable.h"   // for TagSet

struct ProgramDef {
    ProgramId      id;
    ProgramKind    kind;
    int            tier;
    int            ram_cost;
    int            heat_cost;
    const char*    display_name;
    const char*    file_name;
    const char*    description;
    int            qh_cooldown;          // ticks
    std::vector<TagSet> target_filter;   // QH only; empty for non-QH
};
```

- [ ] **Step 3.2 — Rewrite the program registry**

```cpp
// src/program.cpp — replace static const std::vector<ProgramDef> regs = { ... };
const std::vector<ProgramDef>& program_registry() {
    using H = HackTag;
    static const std::vector<ProgramDef> regs = {
        // ATK / STL / UTL — empty filter (used in Grid, not as QH)
        { ProgramId::IcebreakerLite, ProgramKind::Atk, 1, 2, 2, "Icebreaker Lite", "icebreaker_lite.exe",
          "Light cracker for white ICE. (Used in the Grid — Plan 3.)", 0, {} },
        { ProgramId::GhostTrace,     ProgramKind::Stl, 1, 3, 0, "Ghost Trace",     "ghost_trace.exe",
          "Sheds Trace and hides you from white ICE briefly. (Plan 3.)", 0, {} },
        { ProgramId::Cooldown,       ProgramKind::Stl, 1, 2, 0, "Cooldown",        "cooldown.exe",
          "Drops Heat by 4. (Plan 3.)", 0, {} },
        { ProgramId::Breach,         ProgramKind::Utl, 1, 3, 3, "Breach",          "breach.exe",
          "Burns one firewall tile or one gateway lock level. (Plan 3.)", 0, {} },
        { ProgramId::Decrypt,        ProgramKind::Utl, 1, 2, 1, "Decrypt",         "decrypt.exe",
          "Reads one encrypted file. (Plan 3.)", 0, {} },
        { ProgramId::PulseHammer,    ProgramKind::Atk, 3, 4, 5, "Pulse Hammer",    "pulse_hammer.exe",
          "AoE 1d6 dmg to all ICE adjacent to target tile.", 0, {} },
        { ProgramId::DaemonHijack,   ProgramKind::Utl, 3, 5, 4, "Daemon Hijack",   "daemon_hijack.exe",
          "Take control of one ICE for 3 turns.", 0, {} },

        // QH — tag-keyed filters
        { ProgramId::RebootOptics,   ProgramKind::Qh, 1, 1, 0, "Reboot Optics",    "reboot_optics.qh",
          "Soft-reboots a camera or turret's optics. Blinded for 4 turns.",
          1, { H::HasOptics } },
        { ProgramId::FriendlyFire,   ProgramKind::Qh, 2, 3, 0, "Friendly Fire",    "friendly_fire.qh",
          "Re-targets a turret onto its allies for 2 turns.",
          3, { H::Weaponized | H::Mobile } },     // AND: Weaponized AND Mobile
        { ProgramId::DataLeech,      ProgramKind::Qh, 1, 2, 0, "Data Leech",       "data_leech.qh",
          "Drains a packet of operational data from a hackable.",
          2, { H::DataStore } },
    };
    return regs;
}
```

- [ ] **Step 3.3 — Build (expect failures still)**

Run: `cmake --build build`
Expected: `program.cpp` compiles, but `program_effects.cpp` and any call site that reads `target_filter` for kind comparison still fails. Note errors.

- [ ] **Step 3.4 — Commit**

```
git add include/astra/program.h src/program.cpp
git commit -m "wip(hacking): tag-keyed ProgramDef::target_filter"
```

## Task 4 — Migrate every `device_kind ==` site to `(tags & ...)` mask check

**Files:**
- Modify: `src/program_effects.cpp` — replace device_kind equality checks with tag-mask covers.
- Modify: `src/dev_console.cpp` — `:spawn-hackable` (Task 16 will remove it; for now adapt to FixtureType).
- Modify: `src/save_file.cpp` — drop `device_kind` and `available_qh` from Hackable I/O; add `tags` and `ip`.
- Search across the codebase for any other `device_kind`, `DeviceKind::`, `available_qh` references and replace.

**Goal:** Build returns to green. All gameplay code consults tags, not kind.

- [ ] **Step 4.1 — Find all callers**

```
grep -rn "device_kind\|DeviceKind::\|available_qh" include/ src/
```

Expected hits: `program_effects.cpp`, `dev_console.cpp`, `save_file.cpp`, possibly a few UI sites.

- [ ] **Step 4.2 — Update `program_effects.cpp`**

Wherever code reads `cyber.device_kind == DeviceKind::Camera` or similar, replace with the tag check. The most common pattern:

```cpp
// before
bool can_target = false;
for (auto k : prog.target_filter) {
    if (k == cyber.device_kind) { can_target = true; break; }
}

// after
bool can_target = false;
for (TagSet req : prog.target_filter) {
    if (covers(cyber.tags, req)) { can_target = true; break; }
}
```

- [ ] **Step 4.3 — Update `save_file.cpp`**

Find the Hackable read/write blocks. Drop `device_kind` u8 read/write. Drop `available_qh` vector read/write. Add `tags` u32 read/write. Add `ip` u32 read/write.

```cpp
// in write_hackable(...)
w.write_u32(h.tags);
w.write_u32(h.ip);
w.write_i32(h.security_tier);
w.write_u32(h.network_id);
w.write_u8(static_cast<uint8_t>(h.state));
w.write_i32(h.jack_in_node_id);
w.write_i32(h.state_ticks_left);
// (lore_fragments + soul_mirror_progress unchanged)

// in read_hackable(...)
h.tags = r.read_u32();
h.ip   = r.read_u32();
h.security_tier = r.read_i32();
h.network_id    = r.read_u32();
h.state         = static_cast<HackState>(r.read_u8());
h.jack_in_node_id   = r.read_i32();
h.state_ticks_left  = r.read_i32();
// (lore_fragments + soul_mirror_progress unchanged)
```

This breaks save compat — Task 14 bumps `SAVE_FILE_VERSION` and adds the v59 reject.

- [ ] **Step 4.4 — Build until green**

```
cmake --build build
```

Iterate until success. Expect ~5-10 small fixes across the codebase.

- [ ] **Step 4.5 — Commit**

```
git add -A
git commit -m "refactor(hacking): migrate device_kind callers to tag-mask checks"
```

## Task 5 — Remove `DeviceKind` enum + `device_kind_name`

**Files:**
- Modify: `include/astra/hackable.h` — delete `DeviceKind` enum + `device_kind_name` decl.
- Modify: `src/hackable.cpp` — delete `device_kind_name` impl + `[[deprecated]]` `make_hackable(DeviceKind, int)`.

**Goal:** Final removal of the retired type. Code that still compiles is the proof.

- [ ] **Step 5.1 — Delete from header**

```cpp
// include/astra/hackable.h — remove the lines:
//   enum class DeviceKind : uint8_t { ... };
//   const char* device_kind_name(DeviceKind k);
```

- [ ] **Step 5.2 — Delete from impl**

```cpp
// src/hackable.cpp — remove device_kind_name() implementation
//   and the [[deprecated]] make_hackable(DeviceKind, int) overload.
```

- [ ] **Step 5.3 — Build**

Run: `cmake --build build`
Expected: success. If anything still references `DeviceKind`, it's a stragglers from Task 4 — fix them.

- [ ] **Step 5.4 — Commit**

```
git add include/astra/hackable.h src/hackable.cpp
git commit -m "refactor(hacking): retire DeviceKind enum"
```

## Task 6 — `LanMetadata` + `LanRoom` + `LanFlavour` types

**Files:**
- Create: `include/astra/lan.h` — types only.
- Create: `src/lan.cpp` — empty stub for now (helpers come in Task 7).

**Goal:** Type definitions for the LAN data model. No serialization yet.

- [ ] **Step 6.1 — Header**

```cpp
// include/astra/lan.h
#pragma once

#include "astra/grid_network.h"
#include "astra/sector_runtime_state.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra {

enum class LanFlavour : uint8_t { Station, Asteroid, Dungeon, Precursor };

struct Rect { int x, y, w, h; };

struct LanRoom {
    std::string name;                                 // "security", "lobby", "exec"; never rendered in-sector
    Rect        extents = {};
    int         tier = 1;                             // 1=open doorway, 2=breach-required, 3=inner sanctum
    std::vector<GridNodeId> contained_subnets;
};

struct LanMetadata {
    GridNodeId      lan_root;
    bool            has_deep_grid_edge = false;       // true iff connected (LanRoot → shared DeepGridAnchor)
    std::string     region_label;                     // "Heavens Above"
    std::string     display_name;                     // "Concourse LAN"
    LanFlavour      flavour = LanFlavour::Station;
    int             security_tier = 1;
    bool            connected = false;
    uint32_t        gen_seed = 0;
    uint32_t        subnet_base = 0;                  // packed 10.X.Y.0
    std::vector<LanRoom>           rooms;
    uint64_t        last_visited_tick = 0;
    int             nodes_total = 0;
    int             nodes_cracked = 0;
    int             ice_killed = 0;
    int             lore_extracted = 0;
    uint16_t        origin_galaxy_id = 0;
    SectorRuntimeState                              lan_sector_state;
    std::unordered_map<uint32_t, SectorRuntimeState> subnet_states;   // keyed by Subnet GridNodeId.value
};

} // namespace astra
```

- [ ] **Step 6.2 — Empty impl stub**

```cpp
// src/lan.cpp
#include "astra/lan.h"

namespace astra {
// Helpers populated in Task 7.
} // namespace astra
```

- [ ] **Step 6.3 — Add to CMakeLists.txt**

Add `src/lan.cpp` to the source list. Run `cmake -B build -DDEV=ON` to verify configure.

- [ ] **Step 6.4 — Build & commit**

```
cmake --build build
git add include/astra/lan.h src/lan.cpp CMakeLists.txt
git commit -m "feat(lan): LanMetadata, LanRoom, LanFlavour types"
```

## Task 7 — `SectorRuntimeState` + `SectorMutation` types

**Files:**
- Create: `include/astra/sector_runtime_state.h`
- Create: `src/sector_runtime_state.cpp`

**Goal:** Tile-mutation overlay types used by `LanMetadata` and `consciousness.dat`. Pure data + apply helper.

- [ ] **Step 7.1 — Header**

```cpp
// include/astra/sector_runtime_state.h
#pragma once

#include "astra/grid_sector.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace astra {

struct SectorMutation {
    uint8_t  x = 0, y = 0;
    GridTile new_tile = GridTile::Floor;
};

struct SectorRuntimeState {
    std::vector<SectorMutation>                       mutations;
    std::vector<std::pair<uint8_t, uint8_t>>          killed_ice;
};

void apply_mutations(GridSector& sector, const SectorRuntimeState& state);

} // namespace astra
```

- [ ] **Step 7.2 — Impl**

```cpp
// src/sector_runtime_state.cpp
#include "astra/sector_runtime_state.h"

namespace astra {

void apply_mutations(GridSector& sector, const SectorRuntimeState& state) {
    for (const auto& m : state.mutations) {
        if (m.x < sector.w && m.y < sector.h) {
            sector.set(m.x, m.y, m.new_tile);
        }
    }
    // killed_ice is consumed by ICE-spawning logic, not by tile rewriter.
}

} // namespace astra
```

- [ ] **Step 7.3 — Add to CMakeLists.txt**

Add `src/sector_runtime_state.cpp`. Configure + build.

- [ ] **Step 7.4 — Commit**

```
git add include/astra/sector_runtime_state.h src/sector_runtime_state.cpp CMakeLists.txt
git commit -m "feat(grid): SectorMutation + SectorRuntimeState + apply_mutations"
```

## Task 8 — IP allocation helpers

**Files:**
- Create: `include/astra/ip.h`, `src/ip.cpp` — `format_ip`, `parse_ip`, `derive_subnet_base`.

**Goal:** `10.X.Y.host` packing + display.

- [ ] **Step 8.1 — Header**

```cpp
// include/astra/ip.h
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace astra {

// Packed 10.X.Y.0/24 derived deterministically from a map seed.
uint32_t derive_subnet_base(uint32_t map_seed);

// Compose 10.X.Y.host from base + host octet.
inline uint32_t pack_ip(uint32_t base, uint8_t host) {
    return (base & 0xFFFFFF00u) | host;
}

// "10.42.7.5"
std::string format_ip(uint32_t ip);

// Parse "10.X.Y.Z" into packed; nullopt on malformed input.
std::optional<uint32_t> parse_ip(std::string_view s);

} // namespace astra
```

- [ ] **Step 8.2 — Impl**

```cpp
// src/ip.cpp
#include "astra/ip.h"

#include <cstdio>
#include <cstdlib>

namespace astra {

uint32_t derive_subnet_base(uint32_t map_seed) {
    // 10.X.Y.0  where X.Y = (map_seed >> 8) & 0xFFFF, host octet zeroed.
    return 0x0A000000u | ((map_seed >> 8) & 0x00FFFF00u);
}

std::string format_ip(uint32_t ip) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%u.%u.%u.%u",
                  (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return buf;
}

std::optional<uint32_t> parse_ip(std::string_view s) {
    unsigned a, b, c, d;
    if (std::sscanf(std::string(s).c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return std::nullopt;
    if (a > 255 || b > 255 || c > 255 || d > 255) return std::nullopt;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

} // namespace astra
```

- [ ] **Step 8.3 — CMake + build + commit**

```
git add include/astra/ip.h src/ip.cpp CMakeLists.txt
git commit -m "feat(net): IP packing + format/parse helpers"
```

## Task 9 — Auto-registration sweep + IP allocation

**Files:**
- Modify: `include/astra/lan.h` — declare `register_hackables_in_lan`.
- Modify: `src/lan.cpp` — implement sweep, IP allocation, room clustering.

**Goal:** Walk a world map's fixtures + NPC implants, register each Hackable as a Subnet node, allocate IPs, cluster rooms by k-means, populate `LanMetadata`.

- [ ] **Step 9.1 — Add helper declarations**

```cpp
// include/astra/lan.h — add at the end of namespace astra
class World;
void register_hackables_in_lan(World& world, int map_id, GridNetwork& net, LanMetadata& meta);
LanFlavour infer_flavour(const World& world, int map_id);
bool       infer_connected(LanFlavour f);
```

- [ ] **Step 9.2 — Implement sweep**

```cpp
// src/lan.cpp — full implementation
#include "astra/lan.h"
#include "astra/world.h"
#include "astra/ip.h"
#include "astra/random.h"
#include <algorithm>
#include <cmath>

namespace astra {

LanFlavour infer_flavour(const World& world, int map_id) {
    // Map kinds: Station, Asteroid, Dungeon, PrecursorRuin, Settlement (treat as Station), etc.
    auto kind = world.map_kind_for(map_id);
    switch (kind) {
        case MapKind::Station:
        case MapKind::Settlement:
            return LanFlavour::Station;
        case MapKind::Asteroid:    return LanFlavour::Asteroid;
        case MapKind::Dungeon:     return LanFlavour::Dungeon;
        case MapKind::PrecursorRuin:
        case MapKind::CrashedShip:
            return LanFlavour::Precursor;
        default:                   return LanFlavour::Station;
    }
}

bool infer_connected(LanFlavour f) {
    // Spec §3 Q5: clean rule. Station + Asteroid + Precursor connected; Dungeon isolated.
    return f != LanFlavour::Dungeon;
}

namespace {

struct HackableLoc {
    Hackable* h;
    int       x, y;
    bool      is_npc_implant;
};

std::vector<HackableLoc> collect_hackables(World& world, int map_id) {
    std::vector<HackableLoc> out;
    auto& m = world.map_for(map_id);
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            int fid = m.fixture_id(x, y);
            if (fid < 0) continue;
            auto& fd = m.fixture_data_mut(fid);
            if (!fd.cyber) continue;
            if (!has_tag(fd.cyber->tags, HackTag::Electronic)) continue;
            out.push_back({ &*fd.cyber, x, y, false });
        }
    }
    for (auto& npc : world.npcs_for(map_id)) {
        if (!npc.alive() || !npc.cyber) continue;
        if (!has_tag(npc.cyber->tags, HackTag::Electronic)) continue;
        out.push_back({ &*npc.cyber, npc.x, npc.y, true });
    }
    std::sort(out.begin(), out.end(), [](const HackableLoc& a, const HackableLoc& b) {
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    return out;
}

std::vector<LanRoom> cluster_rooms(const std::vector<HackableLoc>& hacks, int office_count, LanFlavour flavour);
const char* pick_room_name(LanFlavour flavour, int idx);

} // anon

void register_hackables_in_lan(World& world, int map_id, GridNetwork& net, LanMetadata& meta) {
    auto hacks = collect_hackables(world, map_id);
    meta.flavour       = infer_flavour(world, map_id);
    meta.connected     = infer_connected(meta.flavour);
    meta.has_deep_grid_edge = meta.connected;
    meta.gen_seed      = world.map_seed_for(map_id) ^ 0xA5A5A5A5u;
    meta.subnet_base   = derive_subnet_base(world.map_seed_for(map_id));
    meta.region_label  = world.region_label_for(map_id);
    meta.display_name  = meta.region_label + " LAN";
    meta.security_tier = world.map_tier_for(map_id);
    meta.nodes_total   = static_cast<int>(hacks.size());
    meta.nodes_cracked = 0;
    meta.origin_galaxy_id = world.galaxy_id();

    if (hacks.empty()) {
        meta.lan_root = {};
        return;
    }

    // 1) LanRoot node.
    GridNode root_node;
    root_node.kind          = GridNodeKind::LanRoot;
    root_node.label         = meta.display_name;
    root_node.security_tier = meta.security_tier;
    root_node.source_seed   = meta.gen_seed;
    meta.lan_root = net.add_node(root_node);

    // 2) Subnet node + IP per Hackable.
    int host = 1;
    for (auto& [h, x, y, is_npc] : hacks) {
        h->ip = pack_ip(meta.subnet_base, static_cast<uint8_t>(host));
        GridNode sn;
        sn.kind          = GridNodeKind::Subnet;
        sn.security_tier = h->security_tier;
        sn.source_seed   = (meta.gen_seed << 8) | host;
        sn.label         = format_ip(h->ip);
        GridNodeId sid = net.add_node(sn);
        h->jack_in_node_id = static_cast<int>(sid.value);

        GridEdge e;
        e.from = meta.lan_root;
        e.to   = sid;
        e.gateway_tier = (host == 1) ? 0 : 1;   // first subnet open; rest tier-1 by default
        e.cracked = false;
        net.add_edge(e);

        ++host;
        if (host >= 254) break;   // cap; deep-Grid gateway uses 254
    }

    // 3) Connected → edge LanRoot → shared DeepGridAnchor.
    if (meta.connected) {
        GridNodeId anchor = world.consciousness_save().shared_deep_grid_anchor_id(net);
        GridEdge e;
        e.from = meta.lan_root;
        e.to   = anchor;
        e.gateway_tier = 2;
        e.cracked = false;
        net.add_edge(e);
    }

    // 4) Cluster into rooms (k-means by world coords).
    int office_count = std::max(1, static_cast<int>(hacks.size() + 2) / 3);
    meta.rooms = cluster_rooms(hacks, office_count, meta.flavour);
}

namespace {

// Lloyd's k-means on (x, y) with deterministic init from world coords.
std::vector<LanRoom> cluster_rooms(const std::vector<HackableLoc>& hacks, int k, LanFlavour flavour) {
    std::vector<LanRoom> rooms(k);
    for (int i = 0; i < k; ++i) {
        rooms[i].name = pick_room_name(flavour, i);
        rooms[i].tier = 1;
    }
    if (hacks.empty() || k == 1) {
        if (!rooms.empty()) {
            for (const auto& hl : hacks) (void)hl;  // placeholder; LanRoom.contained_subnets attached at node-id time
        }
        return rooms;
    }
    // Deterministic init: spread by N-quantile.
    std::vector<std::pair<int,int>> centroids(k);
    for (int i = 0; i < k; ++i) {
        const auto& h = hacks[(i * hacks.size()) / k];
        centroids[i] = { h.x, h.y };
    }
    std::vector<int> assign(hacks.size(), 0);
    for (int iter = 0; iter < 8; ++iter) {
        for (size_t i = 0; i < hacks.size(); ++i) {
            int best = 0; long long bd = LLONG_MAX;
            for (int c = 0; c < k; ++c) {
                long long dx = hacks[i].x - centroids[c].first;
                long long dy = hacks[i].y - centroids[c].second;
                long long d = dx*dx + dy*dy;
                if (d < bd) { bd = d; best = c; }
            }
            assign[i] = best;
        }
        std::vector<long long> sx(k, 0), sy(k, 0);
        std::vector<int>       cn(k, 0);
        for (size_t i = 0; i < hacks.size(); ++i) {
            sx[assign[i]] += hacks[i].x;
            sy[assign[i]] += hacks[i].y;
            cn[assign[i]] += 1;
        }
        for (int c = 0; c < k; ++c) {
            if (cn[c] > 0) centroids[c] = { (int)(sx[c]/cn[c]), (int)(sy[c]/cn[c]) };
        }
    }
    // Compute extents per room.
    struct BB { int min_x, min_y, max_x, max_y; bool has; };
    std::vector<BB> bb(k, { INT_MAX, INT_MAX, INT_MIN, INT_MIN, false });
    for (size_t i = 0; i < hacks.size(); ++i) {
        BB& b = bb[assign[i]];
        b.has = true;
        b.min_x = std::min(b.min_x, hacks[i].x);
        b.min_y = std::min(b.min_y, hacks[i].y);
        b.max_x = std::max(b.max_x, hacks[i].x);
        b.max_y = std::max(b.max_y, hacks[i].y);
    }
    for (int c = 0; c < k; ++c) {
        if (bb[c].has) rooms[c].extents = { bb[c].min_x, bb[c].min_y, bb[c].max_x - bb[c].min_x + 1, bb[c].max_y - bb[c].min_y + 1 };
    }
    // Assign tier to one room as inner sanctum if k >= 9.
    if (k >= 9) {
        int innermost = 0;  // pick the room with the smallest extent area as proxy
        int best_area = INT_MAX;
        for (int c = 0; c < k; ++c) {
            int a = rooms[c].extents.w * rooms[c].extents.h;
            if (a > 0 && a < best_area) { best_area = a; innermost = c; }
        }
        rooms[innermost].tier = 3;
    } else if (k >= 5) {
        rooms.back().tier = 2;   // last room mid-tier
    }
    return rooms;
}

const char* pick_room_name(LanFlavour flavour, int idx) {
    static const char* station_pool[]   = { "lobby", "security", "ops", "rnd", "accounting", "hr", "exec", "archive", "comms", "server-rack" };
    static const char* asteroid_pool[]  = { "dispatch", "ore-vault", "life-support", "hangar", "foreman", "tool-bay" };
    static const char* dungeon_pool[]   = { "vault", "antechamber", "oubliette", "sanctum", "records" };
    static const char* precursor_pool[] = { "nave", "ossuary", "glyph-vault", "chorus", "sanctum" };
    auto pick = [&](const char* const* pool, int N) { return pool[idx % N]; };
    switch (flavour) {
        case LanFlavour::Station:   return pick(station_pool,   10);
        case LanFlavour::Asteroid:  return pick(asteroid_pool,  6);
        case LanFlavour::Dungeon:   return pick(dungeon_pool,   5);
        case LanFlavour::Precursor: return pick(precursor_pool, 5);
    }
    return "lan";
}

} // anon

} // namespace astra
```

- [ ] **Step 9.3 — Add `LanRoot` to `GridNodeKind`**

```cpp
// include/astra/grid_network.h — extend the enum
enum class GridNodeKind : uint8_t {
    Subnet,
    LanRoot,             // NEW: per-LAN root node (Plan 5)
    RegionalDarknet,     // RETIRED (keep enum slot for save compat through Cut 1; remove in Cut 2)
    DeepGridAnchor,
};
```

- [ ] **Step 9.4 — Add `World` accessors**

Add stubs (real implementations in Task 10):
```cpp
// include/astra/world.h
class World {
public:
    // ... existing ...
    Map&                map_for(int map_id);
    const Map&          map_for(int map_id) const;
    std::vector<Npc>&   npcs_for(int map_id);
    uint32_t            map_seed_for(int map_id) const;
    int                 map_tier_for(int map_id) const;
    std::string         region_label_for(int map_id) const;
    enum class MapKind { Station, Asteroid, Dungeon, PrecursorRuin, CrashedShip, Settlement };
    MapKind             map_kind_for(int map_id) const;
    uint16_t            galaxy_id() const;
    LanMetadata&        lan_metadata_for(int map_id);
    ConsciousnessSave&  consciousness_save();
};
```

For Cut 1 — implementations can return reasonable defaults (e.g. `MapKind::Station` for everything; first map only). Cut 2 wires real per-map data.

- [ ] **Step 9.5 — Build & commit**

```
cmake --build build
git add include/astra/lan.h src/lan.cpp include/astra/grid_network.h include/astra/world.h src/world.cpp
git commit -m "feat(lan): auto-registration sweep + IP allocation + room clustering"
```

## Task 10 — `World::on_hackable_removed` + `lan_full_reset`

**Files:**
- Modify: `include/astra/world.h` — declare both hooks.
- Modify: `src/world.cpp` — implement.

**Goal:** Two distinct topology-change paths, per spec §12.

- [ ] **Step 10.1 — Declare**

```cpp
// include/astra/world.h, inside class World
public:
    // Production path: NPC death, future fixture deletion.
    // Metadata-only update; preserves persistence.
    void on_hackable_removed(GridNodeId subnet_id);

    // Dev-only path: :spawn fixture or other hackable-add tooling.
    // Wipes LanMetadata.lan_sector_state + subnet_states; bumps gen_seed.
    void lan_full_reset(int map_id);
```

- [ ] **Step 10.2 — Implement**

```cpp
// src/world.cpp
void World::on_hackable_removed(GridNodeId subnet_id) {
    // Remove the subnet node + its incoming edge from grid_network_.
    // Do NOT touch any LanMetadata state.
    auto& net = grid_network_;
    auto& edges = net.edges_mut();
    edges.erase(std::remove_if(edges.begin(), edges.end(),
        [&](const GridEdge& e) { return e.from == subnet_id || e.to == subnet_id; }),
        edges.end());
    // Mark the node "removed" — grid_network has no remove API today; leave it
    // and tag with a sentinel kind. Add the API in this task:
    if (auto* n = net.find_mut(subnet_id)) {
        n->kind = GridNodeKind::Subnet;       // already; just zero its label as a tombstone.
        n->label = "[removed]";
        n->security_tier = 0;
    }
    // Decrement nodes_total on the LAN this subnet belonged to (best-effort).
    // Iterate metadatas and find the LAN whose lan_root has any edge to this node id.
    for (auto& [map_id, meta] : lan_metadatas_) {
        // (find via stored mapping; for Cut 1 keep the relationship simple)
        if (meta.lan_root.value == 0) continue;
        // Decrement only if subnet_id was in this LAN's subnet states or referenced.
        auto it = meta.subnet_states.find(subnet_id.value);
        if (it != meta.subnet_states.end()) {
            meta.subnet_states.erase(it);
        }
    }
}

void World::lan_full_reset(int map_id) {
    auto it = lan_metadatas_.find(map_id);
    if (it == lan_metadatas_.end()) return;
    LanMetadata& meta = it->second;
    meta.lan_sector_state = SectorRuntimeState{};
    meta.subnet_states.clear();
    meta.gen_seed ^= 0xCAFEBEEFu;   // bump
    // Drop existing subnet nodes belonging to this LAN; the next sweep re-adds.
    auto& net = grid_network_;
    if (meta.lan_root.valid()) {
        // Collect subnets reachable from lan_root.
        std::vector<GridNodeId> to_drop;
        for (const auto& e : net.edges()) {
            if (e.from == meta.lan_root && net.find(e.to) && net.find(e.to)->kind == GridNodeKind::Subnet) {
                to_drop.push_back(e.to);
            }
        }
        for (GridNodeId id : to_drop) on_hackable_removed(id);
        // Drop the LanRoot too.
        auto& edges = net.edges_mut();
        edges.erase(std::remove_if(edges.begin(), edges.end(),
            [&](const GridEdge& e) { return e.from == meta.lan_root || e.to == meta.lan_root; }),
            edges.end());
    }
    meta = LanMetadata{};   // clear; sweep repopulates
    register_hackables_in_lan(*this, map_id, net, meta);
    lan_metadatas_[map_id] = meta;
}
```

- [ ] **Step 10.3 — Build & commit**

```
cmake --build build
git add include/astra/world.h src/world.cpp
git commit -m "feat(lan): on_hackable_removed (production) + lan_full_reset (dev)"
```

## Task 11 — Map-gen content pass: settlements + stations

**Files:**
- Modify: `src/settlement_generator.cpp`
- Modify: `src/station_generator.cpp` (or wherever station interiors lay fixtures)

**Goal:** Every electrical fixture placed by these generators auto-attaches a tagged Hackable. After this task, map enter triggers the LAN registration sweep on settlement / station maps and `nmap` shows entries.

- [ ] **Step 11.1 — Audit fixture placement in `settlement_generator.cpp`**

```
grep -n "make_fixture\|FixtureType::" src/settlement_generator.cpp | head -30
```

For each `FixtureType` placed, decide via spec §14 whether it gains a Hackable. The pattern:

```cpp
// before
FixtureData fd = make_fixture(FixtureType::DataTerminal);
fd.interactable = true;

// after
FixtureData fd = make_fixture(FixtureType::DataTerminal);
fd.interactable = true;
if (HackTagMask t = tags_for_fixture(FixtureType::DataTerminal); t != 0) {
    fd.cyber = make_hackable(FixtureType::DataTerminal, /*tier*/ map_tier);
}
```

Apply the same pattern to every electrical FixtureType placed: `Console`, `Lamp`, `HoloLight`, `Locker`, `Conduit`, `Door` (electronic only), `FoodTerminal`, `WeaponDisplay`, etc.

- [ ] **Step 11.2 — Repeat for `station_generator.cpp`**

Same pattern. Common station fixtures: `CommandTerminal`, `ShipTerminal`, `Console`, `Conduit`, `HoloLight`.

- [ ] **Step 11.3 — Trigger registration on map enter**

Find the map-enter / map-load code path (typically in `Game::enter_map` or `World::load_map`). Call `register_hackables_in_lan(world, map_id, grid_network, lan_metadatas_[map_id])` after fixture placement is complete.

- [ ] **Step 11.4 — Build & manual smoke**

```
cmake --build build
./build/astra
```
- Start a new game (dev mode).
- Land on Heavens Above (settlement).
- Open PDA hacking tab; run `:netmap` (still old name in Cut 1).
- Expected: multiple subnet nodes visible in the netmap, one per electrical fixture in the settlement.

- [ ] **Step 11.5 — Commit**

```
git add src/settlement_generator.cpp src/station_generator.cpp src/game.cpp src/world.cpp
git commit -m "feat(lan): map-gen content pass (settlement + station)"
```

## Task 12 — Map-gen content pass: asteroids + ruins + crashed ships

**Files:**
- Modify: `src/asteroid_generator.cpp`, `src/ruin_generator.cpp`, `src/crashed_ship_generator.cpp`.

**Goal:** Same pattern as Task 11. Apply to remaining generators.

- [ ] **Step 12.1 — `asteroid_generator.cpp`**

Apply the `make_hackable(type, tier)` pattern to every electrical fixture placed. Common: `Console`, `Conduit`, `Lamp`.

- [ ] **Step 12.2 — `ruin_generator.cpp`**

Precursor consoles already exist. Apply tags via `make_hackable(FixtureType::Console, tier)` (Console has the AlienTech bit when its variant is the Precursor one — check `make_fixture` for variants; if Precursor is a separate type, ensure the tag table covers it).

- [ ] **Step 12.3 — `crashed_ship_generator.cpp`**

Apply pattern. Common: Precursor consoles + conduits.

- [ ] **Step 12.4 — Build, smoke, commit**

```
cmake --build build
./build/astra
```
- Land on an asteroid; smoke test `:netmap`.
- Visit a ruin; same.
- Commit:
```
git add src/asteroid_generator.cpp src/ruin_generator.cpp src/crashed_ship_generator.cpp
git commit -m "feat(lan): map-gen content pass (asteroid + ruin + crashed ship)"
```

## Task 13 — Map-gen content pass: dungeons + ship interior + NPC implants

**Files:**
- Modify: `src/dungeon_generator.cpp`, `src/ship_interior_generator.cpp`, `src/npc_factory.cpp`.

**Goal:** Tag the remaining sources. Dungeons are sparse; ship interiors get CommandTerminal/ShipTerminal; cybernetic NPCs get implants tagged.

- [ ] **Step 13.1 — `dungeon_generator.cpp`**

Apply pattern; sparse — most dungeons stay non-electrical. Add 1-3 hackables to mid/late dungeons (e.g. a console + a couple of conduits or cameras).

- [ ] **Step 13.2 — `ship_interior_generator.cpp`**

Apply pattern. Common: `CommandTerminal` (ARIA), `ShipTerminal`, `Conduit`, `HoloLight`.

- [ ] **Step 13.3 — `npc_factory.cpp`**

For NPCs that already get a `npc.cyber` slot (e.g. cybernetic faction members): set tags to `Electronic | Mobile`. If no factory currently sets `npc.cyber`, this is a no-op for Cut 1.

```cpp
// when constructing a cybernetic NPC
npc.cyber = make_hackable(FixtureType::Console, 1);   // borrow Console tags as a placeholder
npc.cyber->tags = HackTag::Electronic | HackTag::Mobile;   // override per-NPC
```

(A future task can add a dedicated `make_implant_hackable()` helper if NPCs grow more diverse.)

- [ ] **Step 13.4 — Build, smoke, commit**

```
cmake --build build
git add src/dungeon_generator.cpp src/ship_interior_generator.cpp src/npc_factory.cpp
git commit -m "feat(lan): map-gen content pass (dungeon + ship + NPC implants)"
```

## Task 14 — Save schema bump v59 → v60 (galaxy save)

**Files:**
- Modify: `include/astra/save_file.h` — bump `SAVE_FILE_VERSION`.
- Modify: `src/save_file.cpp` — reject v59; write/read new fields.

**Goal:** Reserve all Plan 5 galaxy-save fields up front; reject v59. Cuts 2-4 fill empty fields without re-bumping.

- [ ] **Step 14.1 — Bump `SAVE_FILE_VERSION`**

```cpp
// include/astra/save_file.h
constexpr uint32_t SAVE_FILE_VERSION = 60;
```

- [ ] **Step 14.2 — Reject v59**

```cpp
// src/save_file.cpp — load entry
uint32_t version = r.read_u32();
if (version != SAVE_FILE_VERSION) {
    log("save: version " + std::to_string(version) + " not supported (expected " +
        std::to_string(SAVE_FILE_VERSION) + "). Save was created on a previous version of Plan 5 or earlier.");
    return false;
}
```

- [ ] **Step 14.3 — Add LanMetadata + per-map keying**

In the save-write block, after existing world-level data, write the LAN map:

```cpp
// write
w.write_u32(static_cast<uint32_t>(world.lan_metadatas().size()));
for (auto& [map_id, meta] : world.lan_metadatas()) {
    w.write_i32(map_id);
    write_lan_metadata(w, meta);
}

// read
uint32_t n_lans = r.read_u32();
for (uint32_t i = 0; i < n_lans; ++i) {
    int map_id = r.read_i32();
    LanMetadata meta;
    read_lan_metadata(r, meta);
    world.lan_metadatas_mut()[map_id] = std::move(meta);
}
```

`write_lan_metadata` and `read_lan_metadata` cover every field in `LanMetadata` including `lan_sector_state` and `subnet_states` (empty maps are fine; Cut 2 starts populating them).

- [ ] **Step 14.4 — Add SectorRuntimeState (de)serialization**

```cpp
void write_sector_runtime_state(BinaryWriter& w, const SectorRuntimeState& s) {
    w.write_u32(static_cast<uint32_t>(s.mutations.size()));
    for (const auto& m : s.mutations) {
        w.write_u8(m.x);
        w.write_u8(m.y);
        w.write_u8(static_cast<uint8_t>(m.new_tile));
    }
    w.write_u32(static_cast<uint32_t>(s.killed_ice.size()));
    for (const auto& [x, y] : s.killed_ice) {
        w.write_u8(x);
        w.write_u8(y);
    }
}
void read_sector_runtime_state(BinaryReader& r, SectorRuntimeState& s) {
    uint32_t nm = r.read_u32();
    s.mutations.resize(nm);
    for (auto& m : s.mutations) {
        m.x = r.read_u8();
        m.y = r.read_u8();
        m.new_tile = static_cast<GridTile>(r.read_u8());
    }
    uint32_t ni = r.read_u32();
    s.killed_ice.resize(ni);
    for (auto& [x, y] : s.killed_ice) {
        x = r.read_u8();
        y = r.read_u8();
    }
}
```

- [ ] **Step 14.5 — Build & smoke**

```
cmake --build build
./build/astra
```
- New game; play briefly; save; quit; reload. Expected: clean.
- An old v59 save: rejected with the version message.

- [ ] **Step 14.6 — Commit**

```
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): SAVE_FILE_VERSION 59→60; reserve Plan 5 fields"
```

## Task 15 — `consciousness.dat` schema v1 → v2

**Files:**
- Modify: `include/astra/consciousness_save.h` — bump version constant; declare new fields.
- Modify: `src/consciousness_save.cpp` — reject v1; serialize the new fields (empty in Cut 1).

**Goal:** Reserve all Plan 5 consciousness fields. Cut 3 fills `deep_grid_base` and `WarpAnchor` list; Cut 4 fills `ai_contacts`.

- [ ] **Step 15.1 — Bump version**

```cpp
// include/astra/consciousness_save.h
constexpr uint32_t CONSCIOUSNESS_SAVE_VERSION = 2;
```

- [ ] **Step 15.2 — Add new fields**

```cpp
// include/astra/consciousness_save.h, inside ConsciousnessSave struct
struct WarpAnchorRecord {
    uint16_t    galaxy_id = 0;
    uint32_t    region_seed = 0;
    std::string lan_display_name;
    int         nodes_total = 0;
    int         nodes_cracked = 0;
    bool        warpable = true;
};

struct AiContactRecord {
    std::string id;            // e.g. "aria.heavens-above"
    std::string display_name;
    uint16_t    origin_galaxy_id = 0;
    // (Plan 7 adds the rest; Plan 5 just writes id + display_name)
};

struct ConsciousnessSave {
    // ... existing fields from Plan 4 ...
    GridSector              deep_grid_base;            // expanded ~60×40 (Cut 3 populates)
    SectorRuntimeState      deep_grid_sector_state;    // Cut 3 populates
    std::vector<WarpAnchorRecord> warp_anchors;        // Cut 3 populates
    std::vector<AiContactRecord>  ai_contacts;         // Cut 4 populates (placeholder)
};
```

- [ ] **Step 15.3 — Reject v1**

```cpp
// src/consciousness_save.cpp — load entry
uint32_t version = r.read_u32();
if (version != CONSCIOUSNESS_SAVE_VERSION) {
    log("consciousness.dat: version " + std::to_string(version) +
        " not supported (expected " + std::to_string(CONSCIOUSNESS_SAVE_VERSION) +
        "). Plan 4 consciousness state is wiped on Plan 5 launch.");
    return false;
}
```

- [ ] **Step 15.4 — Serialize new fields**

Write empty defaults in Cut 1 (Cuts 3-4 fill in real data):

```cpp
// write
write_grid_sector(w, c.deep_grid_base);
write_sector_runtime_state(w, c.deep_grid_sector_state);
w.write_u32(static_cast<uint32_t>(c.warp_anchors.size()));
for (const auto& a : c.warp_anchors) {
    w.write_u16(a.galaxy_id);
    w.write_u32(a.region_seed);
    w.write_string(a.lan_display_name);
    w.write_i32(a.nodes_total);
    w.write_i32(a.nodes_cracked);
    w.write_u8(a.warpable ? 1 : 0);
}
w.write_u32(static_cast<uint32_t>(c.ai_contacts.size()));
for (const auto& a : c.ai_contacts) {
    w.write_string(a.id);
    w.write_string(a.display_name);
    w.write_u16(a.origin_galaxy_id);
}

// read
read_grid_sector(r, c.deep_grid_base);
read_sector_runtime_state(r, c.deep_grid_sector_state);
uint32_t na = r.read_u32();
c.warp_anchors.resize(na);
for (auto& a : c.warp_anchors) {
    a.galaxy_id = r.read_u16();
    a.region_seed = r.read_u32();
    a.lan_display_name = r.read_string();
    a.nodes_total = r.read_i32();
    a.nodes_cracked = r.read_i32();
    a.warpable = r.read_u8() != 0;
}
uint32_t nac = r.read_u32();
c.ai_contacts.resize(nac);
for (auto& a : c.ai_contacts) {
    a.id = r.read_string();
    a.display_name = r.read_string();
    a.origin_galaxy_id = r.read_u16();
}
```

- [ ] **Step 15.5 — Build & commit**

```
cmake --build build
git add include/astra/consciousness_save.h src/consciousness_save.cpp
git commit -m "feat(save): consciousness.dat v1→v2; reserve Plan 5 fields"
```

## Task 16 — Unified `:spawn` dev verb

**Files:**
- Modify: `src/dev_console.cpp` — rewrite the `spawn` branch to dispatch by subkind.

**Goal:** One verb routes NPC + fixture (+ ICE + trap) spawns. `:spawn-hackable` is removed in Task 17.

- [ ] **Step 16.1 — Rewrite the `spawn` branch**

```cpp
// src/dev_console.cpp — replace existing else if (verb == "spawn") body
else if (verb == "spawn") {
    if (args.size() < 2) {
        log("usage: spawn <name>");
        log("       spawn npc <role>");
        log("       spawn fixture <FixtureType>");
        log("       spawn ice <white|gray|black>");
        log("       spawn trap <kind>");
        log("note: 'spawn fixture' on a live LAN performs a destructive reset of");
        log("      that LAN's persistence (cracked firewalls, looted nodes).");
        return;
    }
    // Subkind-explicit forms.
    if (args[1] == "npc" && args.size() >= 3)     return cmd_spawn_npc(game, args[2]);
    if (args[1] == "fixture" && args.size() >= 3) return cmd_spawn_fixture(game, args[2]);
    if (args[1] == "ice" && args.size() >= 3)     return cmd_spawn_ice(game, args[2]);
    if (args[1] == "trap" && args.size() >= 3)    return cmd_spawn_trap(game, args[2]);

    // Auto-detect: NPC role first, then FixtureType.
    if (npc_role_exists(args[1]))  return cmd_spawn_npc(game, args[1]);
    if (fixture_type_from_name(args[1]).has_value()) return cmd_spawn_fixture(game, args[1]);
    log("spawn: unknown name '" + args[1] + "' (not an NPC role or FixtureType)");
}
```

- [ ] **Step 16.2 — Implement the helpers**

`cmd_spawn_npc` is the existing NPC spawn logic, factored into a function. `cmd_spawn_fixture` is the new fixture-with-Hackable path:

```cpp
static void cmd_spawn_fixture(Game& game, const std::string& type_name) {
    auto type = fixture_type_from_name(type_name);
    if (!type) { log("spawn fixture: unknown FixtureType '" + type_name + "'"); return; }
    auto& m = game.world().map();
    auto& player = game.player();
    static const int dxs[] = {1, -1, 0, 0};
    static const int dys[] = {0, 0, 1, -1};
    bool placed = false;
    int placed_x = 0, placed_y = 0;
    for (int i = 0; i < 4 && !placed; ++i) {
        int nx = player.x + dxs[i];
        int ny = player.y + dys[i];
        if (m.passable(nx, ny) && m.fixture_id(nx, ny) < 0) {
            FixtureData fd = make_fixture(*type);
            fd.interactable = true;
            if (HackTagMask t = tags_for_fixture(*type); t != 0) {
                fd.cyber = make_hackable(*type, /*tier*/ 1);
            }
            m.place_fixture(nx, ny, std::move(fd));
            placed = true;
            placed_x = nx;
            placed_y = ny;
        }
    }
    if (!placed) { log("spawn fixture: no adjacent passable tile"); return; }
    log("spawned " + type_name + " at (" + std::to_string(placed_x) + "," + std::to_string(placed_y) + ")");
    // Trigger destructive LAN reset if the fixture is electrical.
    if (HackTagMask t = tags_for_fixture(*type); has_tag(t, HackTag::Electronic)) {
        game.world().lan_full_reset(game.world().current_map_id());
        log("  → LAN reset (cracked/loot/decrypt state wiped — testing only).");
    }
}
```

`fixture_type_from_name` is a string→FixtureType helper:

```cpp
static std::optional<FixtureType> fixture_type_from_name(std::string_view s) {
    // Case-insensitive comparison; PascalCase or lowercase both ok.
    auto eq = [&](const char* needle) {
        if (s.size() != std::strlen(needle)) return false;
        for (size_t i = 0; i < s.size(); ++i) {
            if (std::tolower(s[i]) != std::tolower(needle[i])) return false;
        }
        return true;
    };
    if (eq("Console"))         return FixtureType::Console;
    if (eq("CommandTerminal")) return FixtureType::CommandTerminal;
    if (eq("ShipTerminal"))    return FixtureType::ShipTerminal;
    if (eq("DataTerminal"))    return FixtureType::DataTerminal;
    if (eq("StarChart"))       return FixtureType::StarChart;
    if (eq("Door"))            return FixtureType::Door;
    if (eq("Gate"))            return FixtureType::Gate;
    if (eq("Conduit"))         return FixtureType::Conduit;
    if (eq("Lamp"))            return FixtureType::Lamp;
    if (eq("HoloLight"))       return FixtureType::HoloLight;
    if (eq("Locker"))          return FixtureType::Locker;
    if (eq("SupplyLocker"))    return FixtureType::SupplyLocker;
    if (eq("HealPod"))         return FixtureType::HealPod;
    if (eq("FoodTerminal"))    return FixtureType::FoodTerminal;
    if (eq("WeaponDisplay"))   return FixtureType::WeaponDisplay;
    if (eq("RepairBench"))     return FixtureType::RepairBench;
    if (eq("RestPod"))         return FixtureType::RestPod;
    if (eq("Camera"))          return FixtureType::Console;   // shim — `Camera` was a DeviceKind label, no separate FixtureType today
    if (eq("Turret"))          return FixtureType::Console;   // same — placeholder
    return std::nullopt;
}
```

(The `Camera`/`Turret` placeholder shim acknowledges that the current codebase doesn't yet have separate FixtureType variants for those — Plan 6+ may add them. For now they fall back to `Console`.)

- [ ] **Step 16.3 — Update help line**

```cpp
// in the help output near the top of dev_console.cpp
log("  spawn <name>                  - NPC role or FixtureType (auto-detect)");
log("  spawn npc <role>              - explicit NPC");
log("  spawn fixture <FixtureType>   - fixture (auto-Hackable if Electronic; resets LAN)");
log("  spawn ice <color>             - ICE in mid-jack-in sector");
log("  spawn trap <kind>             - trap");
```

- [ ] **Step 16.4 — Build & smoke**

```
cmake --build build
./build/astra
```
- New game, dev mode.
- Issue `:spawn fixture Camera` (placeholder Console shim for now). Expected: fixture placed; "LAN reset" log line.
- Issue `:netmap`. Expected: new entry with new IP.

- [ ] **Step 16.5 — Commit**

```
git add src/dev_console.cpp
git commit -m "feat(dev): unified :spawn (npc/fixture/ice/trap)"
```

## Task 17 — Remove `:spawn-hackable`

**Files:**
- Modify: `src/dev_console.cpp` — delete the `else if (verb == "spawn-hackable")` block + its help line.

**Goal:** Final removal. No alias.

- [ ] **Step 17.1 — Delete the block**

Remove lines 1318-end of the `spawn-hackable` block (including the entire ~120-line if-branch).

- [ ] **Step 17.2 — Delete help line**

Find and remove the `"  spawn-hackable <kind>         - place a hackable at adjacent tile"` line.

- [ ] **Step 17.3 — Build, smoke, commit**

```
cmake --build build
./build/astra
:spawn-hackable
# Expected: "unknown command: spawn-hackable" (or whatever the dispatcher's default is)
```

```
git add src/dev_console.cpp
git commit -m "refactor(dev): retire :spawn-hackable verb"
```

---

# Cut 2 — LAN sector generator + traversal + multi-Gateway encoding

Goal: jacking into a LAN renders a procedural cyberspace map. Player walks the LAN, breaches firewalls, enters per-device subnets via `⌬` Gateway tiles, returns. Tile mutations persist across visits. **Deep-Grid is still the existing 30×20 anchor** — that's Cut 3.

## Task 18 — `Connector` + `DeepGridGateway` `GridTile` additions

**Files:**
- Modify: `include/astra/grid_sector.h` — extend `GridTile` enum.
- Modify: `include/astra/grid_theme.h` — palette entries.

**Goal:** Three new tiles in the GridTile enum. Renderer + generator wire them up in later tasks.

- [ ] **Step 18.1 — Extend `GridTile`**

```cpp
// include/astra/grid_sector.h
enum class GridTile : uint8_t {
    Floor,
    Firewall,
    DataNode,
    Gateway,             // existing — subnet gateway (BrightMagenta ⌬)
    ExitNode,            // existing — jack-out (BrightWhite ⊙)
    EncryptedFile,
    Wall,                // existing — generic blocker
    Connector,           // NEW — decorative wiring (DarkGray ═║...)
    DeepGridGateway,     // NEW — connected-LAN portal to deep-Grid (BrightCyan ⊕)
    WarpAnchor,          // NEW — Atlas warp tile (BrightWhite ◉; Cut 3 uses it)
};
```

- [ ] **Step 18.2 — Extend `grid_theme`**

```cpp
// include/astra/grid_theme.h
constexpr Color connector         = Color::DarkGray;
constexpr Color deep_grid_gateway = Color::BrightCyan;   // (or substitute per renderer's enum)
constexpr Color warp_anchor       = Color::BrightWhite;

constexpr const char* connector_glyph         = "═";   // generic; renderer picks neighbour-resolved variant
constexpr const char* deep_grid_gateway_glyph = "⊕";
constexpr const char* warp_anchor_glyph       = "◉";
```

- [ ] **Step 18.3 — Build & commit**

```
cmake --build build
git add include/astra/grid_sector.h include/astra/grid_theme.h
git commit -m "feat(grid): Connector, DeepGridGateway, WarpAnchor tiles"
```

## Task 19 — Box-drawing wall renderer

**Files:**
- Modify: `src/grid_renderer.cpp` — neighbour-aware glyph picker for `Wall` and `Connector`.

**Goal:** Walls and connectors render as box-drawing chars, auto-resolved by N/S/E/W neighbours. Existing sectors (subnet, current 30×20 deep-Grid base) get free polish.

- [ ] **Step 19.1 — Add `wall_glyph_for_neighbours`**

```cpp
// src/grid_renderer.cpp (anonymous namespace)
const char* wall_glyph_for_neighbours(bool n, bool s, bool e, bool w) {
    int code = (n?1:0) | (s?2:0) | (e?4:0) | (w?8:0);
    switch (code) {
        case 0:  return "•";                // isolated (rare)
        case 1:  case 2:  case 3:  return "║";   // vertical
        case 4:  case 8:  case 12: return "═";   // horizontal
        case 5:  return "╚";                // n + e
        case 6:  return "╔";                // s + e
        case 9:  return "╝";                // n + w
        case 10: return "╗";                // s + w
        case 7:  return "╠";                // n + s + e
        case 11: return "╣";                // n + s + w
        case 13: return "╩";                // n + e + w
        case 14: return "╦";                // s + e + w
        case 15: return "╬";                // all four
        default: return "║";
    }
}

bool is_connectable(GridTile t) {
    return t == GridTile::Wall || t == GridTile::Connector;
}
```

- [ ] **Step 19.2 — Use in `glyph_for` / render path**

The current `glyph_for` returns a single string per tile. For wall/connector we need neighbour-aware resolution. Refactor:

```cpp
// In the render loop, replace the per-tile glyph_for + color_for call sites for
// Wall and Connector with a neighbour-aware variant:
const auto& sec = s.sector;
auto neigh = [&](int x, int y) -> bool {
    if (x < 0 || y < 0 || x >= sec.w || y >= sec.h) return false;
    return is_connectable(sec.at(x, y));
};
for (int y = 0; y < ...; ++y) {
    for (int x = 0; x < ...; ++x) {
        GridTile t = sec.at(tx, ty);
        const char* glyph;
        Color       color;
        if (t == GridTile::Wall || t == GridTile::Connector) {
            glyph = wall_glyph_for_neighbours(
                neigh(tx, ty - 1), neigh(tx, ty + 1),
                neigh(tx + 1, ty), neigh(tx - 1, ty));
            color = (t == GridTile::Connector) ? grid_theme::connector : grid_theme::floor;
        } else {
            glyph = glyph_for(t);
            color = color_for(t);
        }
        r.draw_glyph(origin_x + x, origin_y + y, glyph, color);
    }
}
```

(Wall color uses `grid_theme::floor` (Blue) per spec §6 — substrate.)

- [ ] **Step 19.3 — Build & smoke**

```
cmake --build build
./build/astra
```
- Jack into a Precursor console (existing path; uses the regional darknet).
- Walls should now render as `╔══╗║╚╝═...` instead of black space.

- [ ] **Step 19.4 — Commit**

```
git add src/grid_renderer.cpp
git commit -m "feat(grid): box-drawing wall + connector renderer"
```

## Task 20 — `LanSectorGenerator` skeleton + size scaling

**Files:**
- Create: `include/astra/lan_sector_generator.h`, `src/lan_sector_generator.cpp`.

**Goal:** `compute_lan_size(N)` + the empty-sector skeleton. No tiles stamped yet.

- [ ] **Step 20.1 — Header**

```cpp
// include/astra/lan_sector_generator.h
#pragma once

#include "astra/grid_sector.h"
#include "astra/lan.h"

#include <cstdint>

namespace astra {

struct LanSizeParams {
    int office_count = 0;
    int ring_count   = 0;
    int width        = 0;
    int height       = 0;
};

LanSizeParams compute_lan_size(int n_nodes);

GridSector generate_lan_sector(const LanMetadata& meta);

} // namespace astra
```

- [ ] **Step 20.2 — Implement `compute_lan_size`**

```cpp
// src/lan_sector_generator.cpp
#include "astra/lan_sector_generator.h"
#include "astra/random.h"

#include <algorithm>
#include <cmath>

namespace astra {

LanSizeParams compute_lan_size(int n_nodes) {
    LanSizeParams p;
    p.office_count = std::max(1, (n_nodes + 2) / 3);
    p.ring_count   = (p.office_count == 1) ? 0
                   : (p.office_count <= 4) ? 1
                   : (p.office_count <= 9) ? 2
                                            : 3;
    int sq = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(p.office_count))));
    p.width  = std::clamp(20 + 6 * sq, 24, 80);
    p.height = std::clamp(12 + 4 * sq, 14, 40);
    return p;
}

// Stub for Task 21+
GridSector generate_lan_sector(const LanMetadata& meta) {
    LanSizeParams p = compute_lan_size(meta.nodes_total);
    GridSector sec(p.width, p.height);
    for (int y = 0; y < p.height; ++y) {
        for (int x = 0; x < p.width; ++x) {
            sec.set(x, y, GridTile::Floor);
        }
    }
    return sec;
}

} // namespace astra
```

- [ ] **Step 20.3 — CMake + build + commit**

```
git add include/astra/lan_sector_generator.h src/lan_sector_generator.cpp CMakeLists.txt
git commit -m "feat(lan): LanSectorGenerator skeleton + size scaling"
```

## Task 21 — LAN generator: outer firewall ring + spawn / exit tiles

**Files:**
- Modify: `src/lan_sector_generator.cpp` — stamp outer ring, spawn point, jack-out.

**Goal:** Walking into a generated LAN produces a bordered floor plan with a `⊙` jack-out near the spawn.

- [ ] **Step 21.1 — Stamp outer firewall**

```cpp
// extend generate_lan_sector(...)
GridSector generate_lan_sector(const LanMetadata& meta) {
    LanSizeParams p = compute_lan_size(meta.nodes_total);
    GridSector sec(p.width, p.height);

    // 1) Floor everywhere by default.
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x)
            sec.set(x, y, GridTile::Floor);

    // 2) Outer firewall perimeter — only if at least 1 ring.
    if (p.ring_count >= 1) {
        for (int x = 0; x < p.width; ++x) {
            sec.set(x, 0, GridTile::Firewall);
            sec.set(x, p.height - 1, GridTile::Firewall);
        }
        for (int y = 0; y < p.height; ++y) {
            sec.set(0, y, GridTile::Firewall);
            sec.set(p.width - 1, y, GridTile::Firewall);
        }
    }

    // 3) ⊙ ExitNode placement — bottom-edge, just inside the perimeter.
    int exit_x = p.width / 2;
    int exit_y = p.height - 2;
    sec.set(exit_x, exit_y, GridTile::ExitNode);

    return sec;
}
```

- [ ] **Step 21.2 — Build & smoke**

We can't test this in-game until Cut 2's traversal lands. Just build for now.

```
cmake --build build
git add src/lan_sector_generator.cpp
git commit -m "feat(lan): outer firewall ring + jack-out placement"
```

## Task 22 — LAN generator: office room placement

**Files:**
- Modify: `src/lan_sector_generator.cpp` — place LanRoom rectangles inside the perimeter.

**Goal:** Each `LanRoom.extents` becomes a firewall-bounded floor rectangle inside the LAN sector. Tier-1 rooms get a 1-tile floor doorway; tier-2/3 rooms are fully bounded.

- [ ] **Step 22.1 — Map world-coord room extents into LAN sector coords**

The `LanMetadata.rooms` extents are in world-map coords. For LAN sector, we need to project them into the sector's interior. Use a simple grid-pack:

```cpp
struct LanRoomLayout {
    Rect       sector_extents;
    std::string name;
    int         tier;
};

std::vector<LanRoomLayout> pack_rooms_into_sector(
    const std::vector<LanRoom>& rooms, int sector_w, int sector_h, int ring_inset)
{
    std::vector<LanRoomLayout> out;
    int n = static_cast<int>(rooms.size());
    if (n == 0) return out;
    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    int rows = (n + cols - 1) / cols;
    int avail_w = sector_w - 2 * (ring_inset + 2);
    int avail_h = sector_h - 2 * (ring_inset + 2);
    int cell_w = avail_w / cols;
    int cell_h = avail_h / rows;
    int room_w = std::max(5, cell_w - 2);   // 5×3 minimum interior
    int room_h = std::max(3, cell_h - 2);
    for (int i = 0; i < n; ++i) {
        int cx = i % cols;
        int cy = i / cols;
        int x0 = ring_inset + 2 + cx * cell_w + 1;
        int y0 = ring_inset + 2 + cy * cell_h + 1;
        out.push_back({ Rect{ x0, y0, room_w, room_h }, rooms[i].name, rooms[i].tier });
    }
    return out;
}
```

- [ ] **Step 22.2 — Stamp room walls + doorways**

```cpp
void stamp_room(GridSector& sec, const LanRoomLayout& room) {
    int x0 = room.sector_extents.x;
    int y0 = room.sector_extents.y;
    int w  = room.sector_extents.w;
    int h  = room.sector_extents.h;
    // Bound with firewall.
    for (int x = x0; x < x0 + w; ++x) {
        sec.set(x, y0, GridTile::Firewall);
        sec.set(x, y0 + h - 1, GridTile::Firewall);
    }
    for (int y = y0; y < y0 + h; ++y) {
        sec.set(x0, y, GridTile::Firewall);
        sec.set(x0 + w - 1, y, GridTile::Firewall);
    }
    // Interior floor.
    for (int y = y0 + 1; y < y0 + h - 1; ++y)
        for (int x = x0 + 1; x < x0 + w - 1; ++x)
            sec.set(x, y, GridTile::Floor);
    // Tier-1: open doorway. South wall, middle column.
    if (room.tier == 1) {
        int dx = x0 + w / 2;
        sec.set(dx, y0 + h - 1, GridTile::Floor);
    }
    // Tier-2/3: fully bounded; player must breach.
}
```

- [ ] **Step 22.3 — Wire into generator**

```cpp
GridSector generate_lan_sector(const LanMetadata& meta) {
    LanSizeParams p = compute_lan_size(meta.nodes_total);
    GridSector sec(p.width, p.height);
    /* floor + outer ring as in Task 21 */ ...

    auto packed = pack_rooms_into_sector(meta.rooms, p.width, p.height, /*ring_inset*/ 1);
    for (const auto& r : packed) stamp_room(sec, r);

    /* exit node placement as before */ ...
    return sec;
}
```

- [ ] **Step 22.4 — Build & commit**

```
cmake --build build
git add src/lan_sector_generator.cpp
git commit -m "feat(lan): office room placement + doorways"
```

## Task 23 — LAN generator: connector wiring + Gateway tile placement

**Files:**
- Modify: `src/lan_sector_generator.cpp` — connector routes between rooms; `⌬` per subnet.

**Goal:** Visible bus traces between rooms; one `⌬` Gateway tile per subnet inside its room, carrying `target_node_id`.

- [ ] **Step 23.1 — Stamp `⌬` per subnet**

```cpp
void stamp_subnet_gateways(GridSector& sec, const GridNetwork& net,
                            const LanMetadata& meta,
                            const std::vector<LanRoomLayout>& packed)
{
    int packed_idx = 0;
    for (const auto& room : packed) {
        // Each subnet contained in this room gets a ⌬ tile.
        const LanRoom& src = meta.rooms[packed_idx++];
        int gx = room.sector_extents.x + 2;
        int gy = room.sector_extents.y + 1;
        for (GridNodeId sid : src.contained_subnets) {
            if (gx >= room.sector_extents.x + room.sector_extents.w - 1) {
                gx = room.sector_extents.x + 2;
                gy++;
            }
            sec.set(gx, gy, GridTile::Gateway);
            // GatewayTileData encoding (Task 24 adds the per-tile metadata struct).
            sec.gateway_target.emplace(std::pair{gx, gy}, sid);
            gx += 2;
        }
    }
}
```

- [ ] **Step 23.2 — Stamp connector traces between rooms**

```cpp
void route_connector(GridSector& sec, int x1, int y1, int x2, int y2) {
    // Manhattan L-route. Stay on Floor; replace Floor with Connector.
    int x = x1, y = y1;
    while (x != x2) {
        if (sec.at(x, y) == GridTile::Floor) sec.set(x, y, GridTile::Connector);
        x += (x2 > x) ? 1 : -1;
    }
    while (y != y2) {
        if (sec.at(x, y) == GridTile::Floor) sec.set(x, y, GridTile::Connector);
        y += (y2 > y) ? 1 : -1;
    }
}

void stamp_connectors(GridSector& sec, const std::vector<LanRoomLayout>& packed) {
    for (size_t i = 1; i < packed.size(); ++i) {
        const auto& a = packed[i - 1].sector_extents;
        const auto& b = packed[i].sector_extents;
        int ax = a.x + a.w / 2;
        int ay = a.y + a.h;          // just outside south wall
        int bx = b.x + b.w / 2;
        int by = b.y + b.h;
        route_connector(sec, ax, ay, bx, by);
    }
}
```

- [ ] **Step 23.3 — Add gateway-target storage to GridSector**

```cpp
// include/astra/grid_sector.h, inside GridSector:
public:
    std::unordered_map<std::pair<int,int>, GridNodeId, PairHash> gateway_target;
```

(`PairHash` is a small std::pair<int,int> hasher; add if missing.)

- [ ] **Step 23.4 — Wire into generator**

```cpp
GridSector generate_lan_sector(const LanMetadata& meta, const GridNetwork& net) {
    /* ... */
    auto packed = pack_rooms_into_sector(meta.rooms, p.width, p.height, /*ring_inset*/ 1);
    for (const auto& r : packed) stamp_room(sec, r);
    stamp_connectors(sec, packed);
    stamp_subnet_gateways(sec, net, meta, packed);
    /* exit node + deep-grid gateway (Task 24) */
    return sec;
}
```

(Updated signature to take `const GridNetwork&`. Update header + callers.)

- [ ] **Step 23.5 — Build & commit**

```
cmake --build build
git add include/astra/grid_sector.h src/lan_sector_generator.cpp include/astra/lan_sector_generator.h
git commit -m "feat(lan): connector wiring + ⌬ gateway placement w/ target_node_id"
```

## Task 24 — LAN generator: `⊕` deep-Grid gateway placement (connected only)

**Files:**
- Modify: `src/lan_sector_generator.cpp` — stamp `⊕` in highest-tier room of connected LANs.

**Goal:** Connected LANs have exactly one `⊕` tile placed in the highest-tier office. Tile carries `target_node_id` to the shared `DeepGridAnchor`.

- [ ] **Step 24.1 — Add deep-grid placement**

```cpp
void stamp_deep_grid_gateway(GridSector& sec, const GridNetwork& net,
                              const LanMetadata& meta,
                              const std::vector<LanRoomLayout>& packed)
{
    if (!meta.connected) return;
    if (packed.empty()) return;
    // Pick highest-tier room (or last room if all tier-1).
    int best = 0;
    for (size_t i = 1; i < packed.size(); ++i) {
        if (packed[i].tier > packed[best].tier) best = i;
    }
    const auto& room = packed[best];
    int gx = room.sector_extents.x + room.sector_extents.w / 2;
    int gy = room.sector_extents.y + room.sector_extents.h / 2;
    sec.set(gx, gy, GridTile::DeepGridGateway);
    // target_node_id = shared DeepGridAnchor (look up from world)
    GridNodeId anchor = find_deep_grid_anchor(net);   // helper that scans for a DeepGridAnchor node
    sec.gateway_target.emplace(std::pair{gx, gy}, anchor);
}
```

- [ ] **Step 24.2 — Build & commit**

```
cmake --build build
git add src/lan_sector_generator.cpp
git commit -m "feat(lan): ⊕ DeepGridGateway placement on connected LANs"
```

## Task 25 — `apply_breach_grid` — multi-Gateway encoding fix

**Files:**
- Modify: `src/program_effects.cpp` — `apply_breach_grid` reads `target_node_id` from the gateway tile under cursor.

**Goal:** Breach the edge identified by the gateway tile, not "first locked edge of current node".

- [ ] **Step 25.1 — Update logic**

```cpp
// src/program_effects.cpp — replace existing apply_breach_grid body
EffectResult apply_breach_grid(GridSession& s, const ProgramDef& prog) {
    int cx = s.cursor_x;
    int cy = s.cursor_y;
    GridTile t = s.sector.at(cx, cy);
    if (t != GridTile::Firewall && t != GridTile::Gateway && t != GridTile::DeepGridGateway) {
        return { false, "breach: target is not a firewall or gateway" };
    }
    if (t == GridTile::Firewall) {
        s.sector.set(cx, cy, GridTile::Floor);
        s.lan_meta_mut().lan_sector_state.mutations.push_back({ (uint8_t)cx, (uint8_t)cy, GridTile::Floor });
        return { true, "firewall breached" };
    }
    // Gateway / DeepGridGateway: find the edge by target_node_id.
    auto it = s.sector.gateway_target.find({cx, cy});
    if (it == s.sector.gateway_target.end()) return { false, "breach: gateway has no target node" };
    GridNodeId tgt = it->second;
    auto& net = s.world->grid_network();
    GridEdge* edge = nullptr;
    for (auto& e : net.edges_mut()) {
        if (e.to == tgt && (e.from == s.current_node_id || e.from == s.lan_root_id())) {
            edge = &e; break;
        }
    }
    if (!edge) return { false, "breach: edge not found for gateway target" };
    edge->cracked = true;
    return { true, "gateway cracked" };
}
```

- [ ] **Step 25.2 — Build & commit**

```
cmake --build build
git add src/program_effects.cpp
git commit -m "fix(grid): apply_breach_grid uses gateway tile target_node_id"
```

## Task 26 — `HackingSystem::jack_in` sector dispatch

**Files:**
- Modify: `src/hacking_system.cpp` — switch on `GridNodeKind`.

**Goal:** Jacking into a `LanRoot` enters the LAN sector. `Subnet` enters per-device subnet. `DeepGridAnchor` loads `consciousness.dat.deep_grid_base` if self-owned.

- [ ] **Step 26.1 — Update `jack_in`**

```cpp
// src/hacking_system.cpp — replace existing jack_in body
void HackingSystem::jack_in(GridNodeId target_id) {
    const GridNode* target = world_->grid_network().find(target_id);
    if (!target) return;
    GridSession& s = session_;
    s.world = world_;

    switch (target->kind) {
        case GridNodeKind::LanRoot: {
            int map_id = world_->map_id_for_lan(target_id);
            const auto& meta = world_->lan_metadata_for(map_id);
            s.sector = generate_lan_sector(meta, world_->grid_network());
            apply_mutations(s.sector, meta.lan_sector_state);
            // Spawn near ⊙
            for (int y = 0; y < s.sector.h; ++y) {
                for (int x = 0; x < s.sector.w; ++x) {
                    if (s.sector.at(x, y) == GridTile::ExitNode) {
                        s.avatar_x = x;
                        s.avatar_y = y - 1;
                        goto spawned;
                    }
                }
            }
            spawned:;
            break;
        }
        case GridNodeKind::Subnet: {
            s.sector = generate_subnet_sector(target->source_seed, target->security_tier);
            // Apply per-subnet runtime state.
            int map_id = world_->map_id_for_lan(world_->lan_for_subnet(target_id));
            auto& meta = world_->lan_metadata_for(map_id);
            auto it = meta.subnet_states.find(target_id.value);
            if (it != meta.subnet_states.end()) apply_mutations(s.sector, it->second);
            s.avatar_x = 1;
            s.avatar_y = 1;
            break;
        }
        case GridNodeKind::DeepGridAnchor: {
            const auto& cs = world_->consciousness_save();
            if (target->owned_by_consciousness_id == cs.consciousness_id && cs.deep_grid_base.w > 0) {
                s.sector = cs.deep_grid_base;
                apply_mutations(s.sector, cs.deep_grid_sector_state);
            } else {
                s.sector = make_consciousness_anchor_sector();
            }
            // Spawn at Anchor region's spawn tile (Cut 3 sets canonical xy)
            s.avatar_x = 1;
            s.avatar_y = 1;
            break;
        }
        default:
            return;
    }

    s.current_node_id = target_id;
    s.active = true;
}
```

- [ ] **Step 26.2 — Build & smoke**

```
cmake --build build
./build/astra
```
- New game; land on Heavens Above (settlement); approach a CommandTerminal; press `e`; pick "Jack In".
- Expected: enter a generated LAN sector with a `⊙` jack-out near spawn, firewall perimeter, office rooms each with `⌬` tiles, possibly a `⊕` if connected.
- Press `q` to jack out (or step on ⊙). Returns to world.

- [ ] **Step 26.3 — Commit**

```
git add src/hacking_system.cpp
git commit -m "feat(hacking): jack_in sector dispatch (LAN / Subnet / DeepGridAnchor)"
```

## Task 27 — Mid-jack-in sector traversal

**Files:**
- Modify: `src/grid_input.cpp` — gateway-tile interaction.
- Modify: `src/hacking_system.cpp` — `traverse_to(GridNodeId)`.

**Goal:** Stepping on a `⌬` swaps the active sector to that subnet without jack-out. `⊕` swaps to deep-Grid. Returning swaps back.

- [ ] **Step 27.1 — Add `HackingSystem::traverse_to`**

```cpp
// src/hacking_system.cpp
void HackingSystem::traverse_to(GridNodeId target_id) {
    // Save current sector state mutations into the appropriate LanMetadata bucket.
    save_current_sector_state();
    // Then dispatch as if jack_in, but skip session re-init.
    GridNodeId prev = session_.current_node_id;
    jack_in(target_id);
    session_.return_node_id = prev;   // for subnet → LAN return
}
```

- [ ] **Step 27.2 — Update grid input**

```cpp
// src/grid_input.cpp — when player steps on a Gateway/DeepGridGateway tile
GridTile t = sec.at(s.avatar_x, s.avatar_y);
if (t == GridTile::Gateway || t == GridTile::DeepGridGateway) {
    auto it = sec.gateway_target.find({s.avatar_x, s.avatar_y});
    if (it != sec.gateway_target.end()) {
        // Check edge cracked state.
        if (gateway_is_locked(net, s.current_node_id, it->second)) {
            // Show message; don't traverse.
            log("gateway locked — try breach.exe");
        } else {
            game.hacking().traverse_to(it->second);
        }
    }
}
```

- [ ] **Step 27.3 — Subnet → LAN return path**

In a subnet sector, the player encounters its single Gateway tile (already exists in current generator) which navigates back to the originating node. Update `grid_input` to check `s.return_node_id` and traverse back.

- [ ] **Step 27.4 — Build & smoke**

- Jack into a settlement LAN. Walk to a `⌬`. Step onto it. Expected: subnet sector loads.
- Walk in subnet to the gateway-back tile. Expected: returns to the LAN at the originating `⌬`.

- [ ] **Step 27.5 — Commit**

```
git add src/hacking_system.cpp src/grid_input.cpp
git commit -m "feat(hacking): mid-jack-in sector traversal (LAN ↔ subnet ↔ deep-Grid)"
```

## Task 28 — Tile-mutation persistence wiring

**Files:**
- Modify: `src/grid_input.cpp` — record mutations on cracked firewall, looted DataNode, decrypted EncryptedFile, killed ICE.

**Goal:** Every state-changing action records a `SectorMutation` (or `killed_ice` entry) into the active LAN's metadata.

- [ ] **Step 28.1 — Cracked firewall recording**

In `apply_breach_grid` (already done in Task 25), the mutation is written. Verify on save/reload that it persists.

- [ ] **Step 28.2 — Looted DataNode**

```cpp
// when player steps on or interacts with DataNode
if (t == GridTile::DataNode) {
    grant_data_packet(player);
    sec.set(x, y, GridTile::Floor);
    record_mutation(s, x, y, GridTile::Floor);
}
```

`record_mutation` is a small helper:

```cpp
void record_mutation(GridSession& s, int x, int y, GridTile new_tile) {
    auto& mut = active_runtime_state(s).mutations;
    mut.push_back({ (uint8_t)x, (uint8_t)y, new_tile });
}

SectorRuntimeState& active_runtime_state(GridSession& s) {
    int map_id = s.world->map_id_for_lan(s.lan_root_id());
    auto& meta = s.world->lan_metadata_for(map_id);
    if (s.current_node_id == meta.lan_root) return meta.lan_sector_state;
    return meta.subnet_states[s.current_node_id.value];
}
```

- [ ] **Step 28.3 — Decrypted EncryptedFile**

```cpp
if (t == GridTile::EncryptedFile && program_is_decrypt(...)) {
    grant_archive_or_lore(...);
    sec.set(x, y, GridTile::Floor);
    record_mutation(s, x, y, GridTile::Floor);
}
```

- [ ] **Step 28.4 — Killed ICE**

```cpp
// where ICE death is finalized
active_runtime_state(s).killed_ice.push_back({ (uint8_t)ice.x, (uint8_t)ice.y });
```

- [ ] **Step 28.5 — Smoke test persistence**

- Jack into a LAN.
- Crack a firewall, loot a DataNode, decrypt an EncryptedFile, kill an ICE in the LAN sector.
- Step onto a `⌬`, do the same in the subnet.
- Jack out. Save. Quit. Reload.
- Re-enter both sectors. Expected: all mutations persist; killed ICE absent.

- [ ] **Step 28.6 — Commit**

```
git add src/grid_input.cpp src/hacking_system.cpp
git commit -m "feat(grid): tile-mutation persistence (cracked, looted, decrypted, killed)"
```

## Task 29 — `JackInPort` fixture-menu integration

**Files:**
- Modify: `src/game_input.cpp` — line 724 region.

**Goal:** Every fixture with `JackInPort` tag offers "Jack In" in the fixture interaction menu, not just `PrecursorConsole`.

- [ ] **Step 29.1 — Replace the hardcoded check**

```cpp
// before (around line 724)
if (fd.cyber && fd.cyber->device_kind == DeviceKind::PrecursorConsole) {
    menu.add_option("Jack In", ...);
}
// after
if (fd.cyber && has_tag(fd.cyber->tags, HackTag::JackInPort)) {
    menu.add_option("Jack In", ...);
}
```

- [ ] **Step 29.2 — Smoke**

- Walk to a CommandTerminal in your ship. Press `e`. Expected: "Jack In" option.
- Walk to a settlement DataTerminal. Press `e`. Expected: "Jack In" option.

- [ ] **Step 29.3 — Commit**

```
git add src/game_input.cpp
git commit -m "feat(hacking): JackInPort tag drives fixture-menu Jack In option"
```

---

# Cut 3 — Deep-Grid expansion + saved base + galaxy reseed

Goal: replace the 30×20 `make_player_deep_grid_base` with a hand-authored ~60×40 sector. Atlas region populates with `WarpAnchor`s as connected LANs are cracked. `consciousness.dat.deep_grid_base` is loaded by `jack_in` for self-owned anchors. `RebirthSequence::apply` properly reseeds the galaxy.

## Task 30 — Hand-authored deep-Grid sector

**Files:**
- Create: `include/astra/deep_grid_sector.h`, `src/deep_grid_sector.cpp` — author functions.
- Modify: `src/grid_anchor_layout.cpp` — replace `make_player_deep_grid_base` body.

**Goal:** A 60×40 sector with Anchor + Atlas + Frontier regions, separated by firewall partitions.

- [ ] **Step 30.1 — Author the layout**

```cpp
// src/deep_grid_sector.cpp
#include "astra/deep_grid_sector.h"
#include "astra/grid_sector.h"

namespace astra {

GridSector make_deep_grid_base() {
    GridSector s(60, 40);
    // Floor everywhere by default.
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 60; ++x)
            s.set(x, y, GridTile::Floor);

    // Outer perimeter — firewall.
    for (int x = 0; x < 60; ++x) { s.set(x, 0, GridTile::Firewall); s.set(x, 39, GridTile::Firewall); }
    for (int y = 0; y < 40; ++y) { s.set(0, y, GridTile::Firewall); s.set(59, y, GridTile::Firewall); }

    // Anchor region: top-left ~12×10 (rows 1-10, cols 1-12).
    // Spawn tile is the center of Anchor.
    // Lore-archive DataNode placed at (3, 3).
    s.set(3, 3, GridTile::DataNode);
    // Anchor → Atlas: open doorway in the east wall of Anchor.
    // (The east wall is at col 12; punch a 2-tile gap.)
    for (int y = 1; y < 11; ++y) s.set(12, y, GridTile::Wall);   // structural, not breachable
    s.set(12, 5, GridTile::Floor);
    s.set(12, 6, GridTile::Floor);

    // Atlas region: top-middle ~24×20 (rows 1-20, cols 13-36).
    // Atlas → Frontier: firewall partition (rows 1-20, col 37).
    for (int y = 1; y < 21; ++y) s.set(37, y, GridTile::Firewall);

    // Frontier region: top-right ~24×10 (rows 1-10, cols 38-58).
    // Frontier sub-zone: firewalled.
    for (int x = 38; x < 59; ++x) s.set(x, 11, GridTile::Firewall);

    // Bottom half (rows 21-38) is open exploration space, currently empty;
    // Plan 7 expands here.

    return s;
}

} // namespace astra
```

- [ ] **Step 30.2 — Replace `make_player_deep_grid_base`**

```cpp
// src/grid_anchor_layout.cpp — replace existing body
GridSector make_player_deep_grid_base() {
    return make_deep_grid_base();
}
```

- [ ] **Step 30.3 — Build & commit**

```
cmake --build build
git add include/astra/deep_grid_sector.h src/deep_grid_sector.cpp src/grid_anchor_layout.cpp CMakeLists.txt
git commit -m "feat(deep-grid): hand-authored 60×40 sector (Anchor/Atlas/Frontier)"
```

## Task 31 — `WarpAnchor` tile + Atlas population

**Files:**
- Modify: `include/astra/grid_sector.h` — `WarpAnchor` already added in Task 18; add `warp_anchor_data` map.
- Modify: `src/program_effects.cpp` — `apply_breach_grid` on `⊕` first crack: append `WarpAnchorRecord`.

**Goal:** First crack of a connected LAN's `⊕` adds a `WarpAnchorRecord` to `consciousness.dat.warp_anchors` and a `WarpAnchor` tile to the deep-Grid Atlas region.

- [ ] **Step 31.1 — Detect first-crack on `⊕`**

```cpp
// in apply_breach_grid, after edge->cracked = true for a DeepGridGateway target:
if (t == GridTile::DeepGridGateway) {
    bool first_crack = !already_indexed_in_atlas(s.world->consciousness_save(), s.lan_root_id());
    if (first_crack) {
        const auto& meta = s.world->lan_metadata_for(s.world->map_id_for_lan(s.lan_root_id()));
        WarpAnchorRecord rec;
        rec.galaxy_id = s.world->galaxy_id();
        rec.region_seed = s.world->map_seed_for(s.world->map_id_for_lan(s.lan_root_id()));
        rec.lan_display_name = meta.display_name;
        rec.nodes_total = meta.nodes_total;
        rec.nodes_cracked = meta.nodes_cracked;
        rec.warpable = true;
        s.world->consciousness_save_mut().warp_anchors.push_back(std::move(rec));
        // Stamp the tile in the deep-Grid sector.
        place_warp_anchor_in_atlas(s.world->consciousness_save_mut(), rec);
    }
}
```

- [ ] **Step 31.2 — `place_warp_anchor_in_atlas`**

```cpp
void place_warp_anchor_in_atlas(ConsciousnessSave& cs, const WarpAnchorRecord& rec) {
    GridSector& sec = cs.deep_grid_base;
    // Atlas region: cols 13-36, rows 1-20. Find next free Floor tile (in scan order).
    for (int y = 2; y < 20; ++y) {
        for (int x = 14; x < 36; ++x) {
            if (sec.at(x, y) == GridTile::Floor) {
                sec.set(x, y, GridTile::WarpAnchor);
                cs.deep_grid_sector_state.mutations.push_back({ (uint8_t)x, (uint8_t)y, GridTile::WarpAnchor });
                return;
            }
        }
    }
    // Atlas full — Plan 7 will design eviction.
}
```

- [ ] **Step 31.3 — Build & smoke**

- Reach the `⊕` in Heavens Above LAN. Run `breach.exe`.
- Expected: log message "deep-grid uplink registered". A `WarpAnchorRecord` is appended.
- Step on `⊕`. Expected: enter the deep-Grid sector. Spawn in Anchor.
- Walk east through Anchor doorway into Atlas. Expected: see one `WarpAnchor` tile.

- [ ] **Step 31.4 — Commit**

```
git add src/program_effects.cpp src/deep_grid_sector.cpp
git commit -m "feat(deep-grid): WarpAnchor placement on ⊕ first crack"
```

## Task 32 — Galaxy reseed in `RebirthSequence::apply`

**Files:**
- Modify: `src/rebirth_sequence.cpp` — replace MainMenu fallback with `Game::start_new_galaxy`.

**Goal:** Crossing Sgr A\* properly seeds a new galaxy and re-applies consciousness. Existing-galaxy save is wiped.

- [ ] **Step 32.1 — Update `RebirthSequence::apply`**

```cpp
// src/rebirth_sequence.cpp
void RebirthSequence::apply(Game& game) {
    // 1) Persist consciousness state.
    game.world().consciousness_save_mut().mark_warp_anchors_unwarpable_in_old_galaxy(game.world().galaxy_id());
    save_consciousness(game.world().consciousness_save());

    // 2) Generate a fresh galaxy seed.
    uint64_t fresh_seed = derive_fresh_seed(game.world().consciousness_save());

    // 3) Delete current galaxy save.
    delete_galaxy_save(game);

    // 4) Start the new galaxy with the fresh seed.
    game.start_new_galaxy(fresh_seed);

    // 5) Apply consciousness to the fresh galaxy.
    apply_consciousness_to_world(game.world(), game.world().consciousness_save());
}
```

- [ ] **Step 32.2 — `mark_warp_anchors_unwarpable_in_old_galaxy`**

```cpp
// in consciousness_save.cpp
void ConsciousnessSave::mark_warp_anchors_unwarpable_in_old_galaxy(uint16_t old_galaxy_id) {
    for (auto& rec : warp_anchors) {
        if (rec.galaxy_id == old_galaxy_id) rec.warpable = false;
    }
}
```

- [ ] **Step 32.3 — Build & smoke**

- Develop in dev mode. `:rebirth`. Expected: cinematic plays, galaxy regenerates, deep-Grid Atlas warpanchor (if any) is now ghosted.

- [ ] **Step 32.4 — Commit**

```
git add src/rebirth_sequence.cpp src/consciousness_save.cpp
git commit -m "feat(rebirth): RebirthSequence::apply triggers Game::start_new_galaxy"
```

---

# Cut 4 — nmap / ping / jack + stitching gaps

Goal: rename `netmap` → `nmap`. IP-driven `jack`/`ping`. `nmap -l` text list, `nmap -m` opens widget. `lore` reads consciousness archive. `ai_contacts` schema populated. All `(stub)` markers removed.

## Task 33 — Rename `GridNetmapWidget` → `GridNmapWidget`

**Files:**
- Rename: `include/astra/grid_netmap_widget.h` → `include/astra/grid_nmap_widget.h`
- Rename: `src/grid_netmap_widget.cpp` → `src/grid_nmap_widget.cpp`
- Modify: every includer + the dispatcher in `pda_hacking_tab.cpp`.

**Goal:** No alias. `netmap` command vanishes; `nmap` takes its place.

- [ ] **Step 33.1 — `git mv` the files**

```
git mv include/astra/grid_netmap_widget.h include/astra/grid_nmap_widget.h
git mv src/grid_netmap_widget.cpp src/grid_nmap_widget.cpp
```

- [ ] **Step 33.2 — Rename class & symbols**

`sed`-style replace: `GridNetmapWidget` → `GridNmapWidget`. `NetmapZoom` → `NmapMode`. `NetmapZoom::Regional` → `NmapMode::Lan`. `NetmapZoom::DeepGrid` → `NmapMode::Atlas`.

(Manually verify after replace; check `pda_hacking_tab.cpp` and `pda_screen.h`.)

- [ ] **Step 33.3 — CMake**

Update `CMakeLists.txt` source list.

- [ ] **Step 33.4 — Build & commit**

```
cmake -B build -DDEV=ON && cmake --build build
git add -A
git commit -m "refactor(pda): rename GridNetmapWidget → GridNmapWidget"
```

## Task 34 — `nmap` command with flags

**Files:**
- Modify: `src/pda_hacking_tab.cpp` — replace `hack_term_cmd_netmap` with `hack_term_cmd_nmap(args)`.

**Goal:** `nmap -l` enumerates; `nmap -m` opens widget; bare `nmap` prints usage.

- [ ] **Step 34.1 — Replace dispatch**

```cpp
// in the if-chain
if (v == "nmap")    return hack_term_cmd_nmap(args);
// remove: if (v == "netmap") return hack_term_cmd_netmap();
```

- [ ] **Step 34.2 — Implement**

```cpp
void PdaScreen::hack_term_cmd_nmap(const std::vector<std::string>& args) {
    if (args.size() < 2 || args[1] == "-h" || args[1] == "--help") {
        hack_term_emit("usage: nmap [-l|--list] [-m|--map] [-h|--help]", UITag::TextDim);
        return;
    }
    if (args[1] == "-l" || args[1] == "--list") return hack_term_cmd_nmap_list();
    if (args[1] == "-m" || args[1] == "--map")  return hack_term_cmd_nmap_map();
    hack_term_emit("nmap: unknown flag '" + args[1] + "'; try -l, -m, or -h.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_nmap_list() {
    if (!world_) { hack_term_emit("nmap: world unavailable.", UITag::TextDim); return; }
    int map_id = world_->current_map_id();
    const auto& meta = world_->lan_metadata_for(map_id);
    if (meta.nodes_total == 0) { hack_term_emit("nmap: no LAN on this map.", UITag::TextDim); return; }
    char header[128];
    std::snprintf(header, sizeof header, "LAN: %s   (%s/24)   %d nodes, %d cracked",
        meta.display_name.c_str(), format_ip(meta.subnet_base).c_str(),
        meta.nodes_total, meta.nodes_cracked);
    hack_term_emit(header);
    hack_term_emit("");
    hack_term_emit("  IP            HOST                   STATUS    TAGS");
    for (const auto& n : world_->grid_network().nodes()) {
        if (n.kind != GridNodeKind::Subnet) continue;
        // Resolve via tag of the underlying Hackable. (Skipped here; emit a 1-line summary per node.)
        char line[256];
        std::snprintf(line, sizeof line, "  %-13s %-22s %-9s %s",
                      format_ip(/*ip from node label*/).c_str(),
                      n.label.c_str(),
                      /*lock status*/ "open",
                      /*tag list*/ "Electronic");
        hack_term_emit(line);
    }
    if (meta.has_deep_grid_edge) {
        hack_term_emit("  10.X.Y.254    [⊕ deep-grid]          locked.3  DeepGridGateway");
    }
}

void PdaScreen::hack_term_cmd_nmap_map() {
    nmap_widget_.open();
}
```

- [ ] **Step 34.3 — Build & smoke**

```
cmake --build build
./build/astra
```
- Open PDA terminal. Type `nmap`. Expected: usage hint.
- `nmap -l`. Expected: list of nodes.
- `nmap -m`. Expected: visual widget opens.

- [ ] **Step 34.4 — Commit**

```
git add src/pda_hacking_tab.cpp include/astra/pda_screen.h
git commit -m "feat(pda): nmap command (list + map flags)"
```

## Task 35 — `ping IP` recon

**Files:**
- Modify: `src/pda_hacking_tab.cpp` — replace `hack_term_cmd_ping` body.

**Goal:** Resolves IP → fixture; emits 1-line latency + tier + tags + state. Free action.

- [ ] **Step 35.1 — Implement**

```cpp
void PdaScreen::hack_term_cmd_ping(const std::vector<std::string>& args) {
    if (args.size() < 2) { hack_term_emit("usage: ping <ip>", UITag::TextDim); return; }
    auto ip = parse_ip(args[1]);
    if (!ip) { hack_term_emit("ping: invalid IP '" + args[1] + "'", UITag::TextDim); return; }
    auto* h = world_->find_hackable_by_ip(*ip);
    if (!h) { hack_term_emit("ping: " + format_ip(*ip) + ": host unreachable", UITag::TextDim); return; }
    char line1[160];
    std::snprintf(line1, sizeof line1, "PING %s (%s):", format_ip(*ip).c_str(), h->label.c_str());
    hack_term_emit(line1);
    char line2[160];
    int latency = 1 + ((*ip) & 7);   // deterministic, cosmetic
    std::snprintf(line2, sizeof line2, "  64 bytes from %s: time=%dms", format_ip(*ip).c_str(), latency);
    hack_term_emit(line2);
    char line3[160];
    std::snprintf(line3, sizeof line3, "  tier:    %d (%s)", h->security_tier,
                  h->state == HackState::Compromised ? "compromised" : "locked");
    hack_term_emit(line3);
    hack_term_emit("  tags:    " + tag_mask_to_string(h->tags));
    hack_term_emit("  state:   " + std::string(hack_state_name(h->state)));
}
```

(`tag_mask_to_string` is a small helper that joins the tags as a comma list.)

- [ ] **Step 35.2 — Build & commit**

```
git add src/pda_hacking_tab.cpp
git commit -m "feat(pda): ping <ip> implementation"
```

## Task 36 — `jack IP` rewrite

**Files:**
- Modify: `src/pda_hacking_tab.cpp` — replace `jack -t <label>` with `jack <ip>`.

**Goal:** Same deferred-jack flow, but IP-keyed.

- [ ] **Step 36.1 — Update body**

```cpp
void PdaScreen::hack_term_cmd_jack(const std::vector<std::string>& args) {
    if (args.size() < 2) { hack_term_emit("usage: jack <ip>", UITag::TextDim); return; }
    auto ip = parse_ip(args[1]);
    if (!ip) { hack_term_emit("jack: invalid IP '" + args[1] + "'", UITag::TextDim); return; }
    if (!world_ || !player_) { hack_term_emit("jack: state unavailable.", UITag::TextDim); return; }
    if (!has_cat_hacking(*player_)) { hack_term_emit("jack: requires Cat_Hacking skill.", UITag::TextDim); return; }
    auto* h = world_->find_hackable_by_ip(*ip);
    if (!h) { hack_term_emit("jack: " + format_ip(*ip) + ": host unreachable", UITag::TextDim); return; }
    if (h->jack_in_node_id <= 0) { hack_term_emit("jack: target has no node id", UITag::TextDim); return; }
    jack_in_request_node_id_ = static_cast<uint32_t>(h->jack_in_node_id);
    hack_term_emit(">> uploading consciousness... <<");
}
```

- [ ] **Step 36.2 — Build & smoke**

- `nmap -l` → see IP. `jack 10.X.Y.5`. Expected: jack-in proceeds.

- [ ] **Step 36.3 — Commit**

```
git add src/pda_hacking_tab.cpp
git commit -m "feat(pda): jack <ip> (was jack -t <label>)"
```

## Task 37 — `lore` real implementation

**Files:**
- Modify: `src/pda_hacking_tab.cpp` — `hack_term_cmd_lore` reads `consciousness.dat.lore_archive`.

**Goal:** List archive ids; `cat <archive-id>` reads body text.

- [ ] **Step 37.1 — Implement**

```cpp
void PdaScreen::hack_term_cmd_lore() {
    const auto& cs = world_->consciousness_save();
    if (cs.lore_archive.empty()) {
        hack_term_emit("no decrypted archives.", UITag::TextDim);
        return;
    }
    hack_term_emit("decrypted archives:");
    for (const auto& a : cs.lore_archive) {
        char line[160];
        std::snprintf(line, sizeof line, "  %-24s  %s", a.archive_id.c_str(), a.commit_time.c_str());
        hack_term_emit(line);
    }
    hack_term_emit("");
    hack_term_emit("use: cat <archive-id>", UITag::TextDim);
}
```

`cat` already exists; extend to look up archive ids when the arg isn't a known file path.

- [ ] **Step 37.2 — Update `hack_term_cmd_cat` to recognize archive ids**

```cpp
// in hack_term_cmd_cat
const auto& cs = world_->consciousness_save();
for (const auto& a : cs.lore_archive) {
    if (a.archive_id == args[1]) {
        for (const auto& line : a.body_lines) hack_term_emit(line);
        return;
    }
}
// fallback to existing cat logic
```

- [ ] **Step 37.3 — Commit**

```
git add src/pda_hacking_tab.cpp
git commit -m "feat(pda): lore + cat <archive-id> from consciousness archive"
```

## Task 38 — `ai_contacts` schema population

**Files:**
- Modify: `src/program_effects.cpp` — on first deep-Grid uplink (Task 31), add an AI contact placeholder for that LAN's region.

**Goal:** Schema populated with placeholders; full UI deferred to Plan 7.

- [ ] **Step 38.1 — Append AI contact when first WarpAnchor lands**

```cpp
// in the same place place_warp_anchor_in_atlas is called:
AiContactRecord c;
c.id = "aria." + slugify(meta.region_label);
c.display_name = "ARIA — " + meta.region_label;
c.origin_galaxy_id = s.world->galaxy_id();
s.world->consciousness_save_mut().ai_contacts.push_back(std::move(c));
```

- [ ] **Step 38.2 — Commit**

```
git add src/program_effects.cpp
git commit -m "feat(deep-grid): ai_contacts placeholder on uplink"
```

## Task 39 — `nmap`-side `b`-key breach

**Files:**
- Modify: `src/grid_nmap_widget.cpp` — handle `b` on locked edge cursor.

**Goal:** Cursor on a `╳`-marked edge; press `b`; pays `breach.exe` cost; flips `cracked = true`. Charge taken from `HackingSystem` without opening a sector.

- [ ] **Step 39.1 — Add the handler**

```cpp
// inside GridNmapWidget::handle_key
if (key == 'b' || key == 'B') {
    if (auto* edge = edge_under_cursor()) {
        if (!edge->cracked && edge->gateway_tier > 0) {
            apply_breach_from_nmap(*edge, *world_);
        }
    }
    return true;
}
```

`apply_breach_from_nmap` (new helper in `program_effects.cpp` or `hacking_system.cpp`):

```cpp
void apply_breach_from_nmap(GridEdge& edge, World& world) {
    auto& deck = /* find equipped deck */;
    if (!can_pay_breach_cost(deck)) return;
    pay_breach_cost(deck);
    edge.cracked = true;
}
```

- [ ] **Step 39.2 — Commit**

```
git add src/grid_nmap_widget.cpp src/program_effects.cpp
git commit -m "feat(nmap): b-key breach from netmap (no sector entry)"
```

## Task 40 — Stub-marker cleanup

**Files:**
- Modify: `src/pda_hacking_tab.cpp` — `help` and `man` text overhaul.

**Goal:** Drop every `(stub in Plan X)` line. Update `nmap`/`ping`/`jack`/`lore` man pages.

- [ ] **Step 40.1 — Update `hack_term_cmd_help`**

```cpp
void PdaScreen::hack_term_cmd_help() {
    static const char* lines[] = {
        "commands:",
        "  help                — this list",
        "  ps                  — running programs",
        "  ls                  — list files in current dir",
        "  cat <file>          — read a file",
        "  echo <text>         — echo",
        "  uname               — system info",
        "  whoami              — current user",
        "  load <prog>         — load a program",
        "  unload <prog>       — unload a program",
        "  ping <ip>           — probe a node (free recon)",
        "  nmap [-l|-m|-h]     — list / map LAN nodes",
        "  jack <ip>           — jack into a node",
        "  lore                — list decrypted archives",
        "  history             — command history",
        "  clear               — clear the terminal",
        "  man <cmd>           — manual page",
        nullptr,
    };
    for (auto** p = lines; *p; ++p) hack_term_emit(*p, UITag::TextDim);
}
```

- [ ] **Step 40.2 — Update man entries**

Replace stub blocks with real shapes (mirror what's actually implemented in Tasks 34-37).

- [ ] **Step 40.3 — Update tab-complete list**

```cpp
const char* tab_completes[] = {
    "help", "ps", "ls", "cat ", "echo ", "uname", "whoami",
    "load ", "unload ", "ping ", "nmap ", "nmap -l", "nmap -m",
    "jack ", "lore", "history", "clear", "man ",
    nullptr,
};
```

- [ ] **Step 40.4 — Commit**

```
git add src/pda_hacking_tab.cpp
git commit -m "chore(pda): drop (stub) markers; refresh help/man/tab-complete"
```

## Task 41 — Self-anchor entry bypass in nmap widget

**Files:**
- Modify: `src/grid_nmap_widget.cpp` — `can_enter` predicate.

**Goal:** Player-owned `DeepGridAnchor` nodes bypass the lock predicate.

- [ ] **Step 41.1 — Update**

```cpp
bool can_enter(const GridNode& target, const ConsciousnessSave& cs, const GridNetwork& net) {
    if (target.owned_by_consciousness_id == cs.consciousness_id) return true;
    return !node_is_locked(net, target.id);
}
```

- [ ] **Step 41.2 — Commit**

```
git add src/grid_nmap_widget.cpp
git commit -m "feat(nmap): self-anchor entry bypasses lock predicate"
```

## Task 42 — Atlas view in nmap widget

**Files:**
- Modify: `src/grid_nmap_widget.cpp` — render warp anchors when in deep-Grid; Tab cycle.

**Goal:** When player is jacked into deep-Grid, `nmap -m` renders Atlas view: warp tiles by galaxy. Tab cycles LAN ↔ Atlas.

- [ ] **Step 42.1 — Render Atlas mode**

```cpp
void GridNmapWidget::render_atlas(UIContext& ctx, const ConsciousnessSave& cs, uint16_t live_galaxy_id) const {
    // group by galaxy
    std::unordered_map<uint16_t, std::vector<const WarpAnchorRecord*>> by_galaxy;
    for (const auto& a : cs.warp_anchors) by_galaxy[a.galaxy_id].push_back(&a);
    int y = 1;
    for (auto& [gid, list] : by_galaxy) {
        bool ghosted = (gid != live_galaxy_id);
        ctx.draw_line(2, y++, ghosted ? "GALAXY: (past life)" : "GALAXY: (current)");
        for (auto* rec : list) {
            char line[160];
            std::snprintf(line, sizeof line, "  [%s]   %d/%d",
                rec->lan_display_name.c_str(), rec->nodes_cracked, rec->nodes_total);
            ctx.draw_line(4, y++, line, ghosted ? UITag::TextDim : UITag::TextDefault);
        }
    }
}
```

- [ ] **Step 42.2 — Tab cycling**

```cpp
if (key == KEY_TAB) {
    if (player_in_deep_grid_) mode_ = (mode_ == NmapMode::Lan ? NmapMode::Atlas : NmapMode::Lan);
}
```

- [ ] **Step 42.3 — Smoke**

- Jack into deep-Grid. Open `nmap -m`. Press Tab. Expected: cycles to Atlas view; shows your warp anchors.

- [ ] **Step 42.4 — Commit**

```
git add src/grid_nmap_widget.cpp include/astra/grid_nmap_widget.h
git commit -m "feat(nmap): Atlas view + Tab cycle (deep-Grid only)"
```

## Task 43 — Final docs + roadmap update

**Files:**
- Modify: `docs/mechanics.md` — Hacking section additions.
- Modify: `docs/items.md` — implant tag stamps.
- Modify: `docs/roadmap.md` — Plan 5 done; Plan 7 expanded scope.

**Goal:** Documentation updated alongside code, per project convention.

- [ ] **Step 43.1 — `mechanics.md`**

Add a "LAN model" subsection under Hacking:
- Tags + program filter semantics.
- LAN auto-registration.
- LAN sector + traversal.
- Deep-Grid + Atlas + WarpAnchor semantics.
- Persistence rules (mutations, `:spawn fixture` reset, NPC death = metadata-only).

- [ ] **Step 43.2 — `items.md`**

Add HackTagMask column to relevant tables (implants).

- [ ] **Step 43.3 — `roadmap.md`**

```
- [x] **Plan 5 — Grid expansion + LAN redesign** (2026-05-XX)
  - Tag-driven capability model (DeviceKind retired)
  - LAN auto-registration; tile-mutation persistence
  - LAN sector generator (A+B+E layered, scales with N)
  - Deep-Grid expansion (60×40, Anchor v1, Atlas, Frontier reserved)
  - nmap/ping/jack with IPs
  - 8 stitching gaps closed

- [ ] **Plan 6 — UI / Grid HUD redesign** — designed against the locked LAN gameplay state.

- [ ] **Plan 7 — Darknet content + Your.Anchor full mechanics + AI contacts UI**
```

- [ ] **Step 43.4 — Commit**

```
git add docs/mechanics.md docs/items.md docs/roadmap.md
git commit -m "docs(plan-5): mechanics/items/roadmap updates"
```

---

## Final smoke — full Plan 5 acceptance run

After all tasks land, walk this scenario in dev mode:

1. **Tag refactor sanity.** `:spawn fixture Camera` (or any electrical FixtureType). The fixture appears; LAN reset log line shows. `nmap -l` lists the new IP.
2. **Heavens Above LAN run.** Approach a CommandTerminal. Press `e` → "Jack In". Walk the LAN sector. Step on a `⌬` → enter subnet → return. Run `breach.exe` on a firewall in the LAN. Step on `⊕` → enter deep-Grid. Walk to Atlas; see the `WarpAnchor` for Heavens Above. Press `nmap -m` + Tab → see Atlas view with this LAN's tile.
3. **Persistence.** Quit + reload mid-run. Re-enter Heavens Above LAN. Cracked firewall stays cracked. Looted DataNode stays floor. Killed ICE absent.
4. **Rebirth.** `:rebirth`. Cinematic plays; new galaxy generated. Deep-Grid Atlas now shows the previous Heavens Above tile dimmed; new galaxy's LANs appear ahead.
5. **No stubs.** `:help` text shows no `(stub)` markers. `:man nmap` / `:man ping` / `:man jack` / `:man lore` are all real shapes.

If all five pass, Plan 5 is complete. Open a PR with the squashed-cut commit history.

---

## Notes

- **No new programs** in this plan. The 9 existing ones are migrated; new programs are out of scope.
- **No HUD changes** (Plan 6).
- **Sgr A\* in-world warp trigger** is deferred to Plan 7+. `:rebirth` is the in-game path.
- **Your.Anchor full mechanics** (stash, customization, AI-contact locus, ownership rules beyond Plan 4) is Plan 7. Plan 5 ships only Anchor v1: spawn-hub geometry + lore-archive `DataNode`.
