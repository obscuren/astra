# Space Station Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make non-THA space stations useful and varied by introducing a station-type system (NormalHub / Scav / Pirate / Abandoned / Infested) with specialty-flavored hubs, unique per-station keepers, split THA dialog, and xytomorphs removed from populated stations.

**Architecture:** Add a `StationType` enum + `StationSpecialty` enum + `StationContext` value carried through generation and NPC spawning. Roll type deterministically from the station seed in `StationInfo` (replacing `bool derelict`). The map-generator factory dispatches on type to per-type generators. NPC builders take a `StationContext` and branch dialog on `is_tha`. THA (Sol, id=1) is hardcoded to `NormalHub` with `is_tha=true`.

**Tech Stack:** C++20, CMake, existing terminal renderer, no formal unit test framework — verification is manual builds (`-DDEV=ON -DSDL=OFF`) plus a dev-overlay to inspect station types across the galaxy.

**Spec:** `docs/superpowers/specs/2026-04-13-space-station-types-design.md`.

**Testing note:** This project has no unit test harness. Each task ends with a build step and a manual verification step. For logic with deterministic inputs (type roll, keeper-seed name generation) the dev overlay (Task 14) provides the verification surface — do not add a new test framework.

---

## Task 1: Add StationType / StationSpecialty enums + StationContext

**Files:**
- Create: `include/astra/station_type.h`

- [ ] **Step 1: Create the header**

```cpp
// include/astra/station_type.h
#pragma once

#include <cstdint>
#include <string>

namespace astra {

enum class StationType : uint8_t {
    NormalHub,
    Scav,
    Pirate,
    Abandoned,
    Infested,
};

enum class StationSpecialty : uint8_t {
    Generic,
    Mining,
    Research,
    Frontier,
    Trade,
    Industrial,
};

struct StationContext {
    bool is_tha = false;
    StationType type = StationType::NormalHub;
    StationSpecialty specialty = StationSpecialty::Generic;
    uint64_t keeper_seed = 0;
    std::string station_name;
};

const char* to_string(StationType);
const char* to_string(StationSpecialty);

}  // namespace astra
```

- [ ] **Step 2: Create the .cpp with string helpers**

Create `src/station_type.cpp`:

```cpp
#include "astra/station_type.h"

namespace astra {

const char* to_string(StationType t) {
    switch (t) {
        case StationType::NormalHub: return "NormalHub";
        case StationType::Scav:      return "Scav";
        case StationType::Pirate:    return "Pirate";
        case StationType::Abandoned: return "Abandoned";
        case StationType::Infested:  return "Infested";
    }
    return "?";
}

const char* to_string(StationSpecialty s) {
    switch (s) {
        case StationSpecialty::Generic:    return "Generic";
        case StationSpecialty::Mining:     return "Mining";
        case StationSpecialty::Research:   return "Research";
        case StationSpecialty::Frontier:   return "Frontier";
        case StationSpecialty::Trade:      return "Trade";
        case StationSpecialty::Industrial: return "Industrial";
    }
    return "?";
}

}  // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Find the source list in `CMakeLists.txt` and add `src/station_type.cpp`.

- [ ] **Step 4: Build**

Run: `cmake -B build -DDEV=ON -DSDL=OFF && cmake --build build`
Expected: success.

- [ ] **Step 5: Commit**

```bash
git add include/astra/station_type.h src/station_type.cpp CMakeLists.txt
git commit -m "feat(station): add StationType/StationSpecialty enums + StationContext"
```

---

## Task 2: Extend `StationInfo` with type/specialty/keeper_seed; remove `derelict`

**Files:**
- Modify: `include/astra/star_chart.h` (around line 24)
- Modify: `src/star_chart.cpp` (serialization)
- Modify: any call sites reading `station.derelict`

- [ ] **Step 1: Edit the struct**

Replace the `StationInfo` block in `include/astra/star_chart.h`:

```cpp
#include "astra/station_type.h"

struct StationInfo {
    std::string name;
    StationType type = StationType::NormalHub;
    StationSpecialty specialty = StationSpecialty::Generic;
    uint64_t keeper_seed = 0;
};
```

- [ ] **Step 2: Find existing readers**

Run: `grep -rn "station.derelict\|\.derelict" src include`
List every hit; each needs migration to `station.type == StationType::Abandoned`.

- [ ] **Step 3: Migrate each reader**

For each hit, replace `station.derelict` with `station.type == StationType::Abandoned` (or the equivalent check in context — a map dispatch will be rewritten in Task 4; only mechanical substitutions here).

- [ ] **Step 4: Migrate save/load**

In `src/star_chart.cpp`, find StationInfo serialization. Keep the on-disk layout compatible: read the old `derelict` bool and translate to `StationType::Abandoned` on load; on save, always write the new fields (bump a format-version constant if one exists; otherwise write an additive field).

Pseudocode:

```cpp
// on load (legacy path):
bool derelict;
in >> derelict;
info.type = derelict ? StationType::Abandoned : StationType::NormalHub;
info.specialty = StationSpecialty::Generic;
info.keeper_seed = 0;  // will be set deterministically on first visit (Task 3)

// on save:
out << (uint8_t)info.type << (uint8_t)info.specialty << info.keeper_seed;
```

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: success. Fix any stragglers flagged by the compiler.

- [ ] **Step 6: Commit**

```bash
git add -u
git commit -m "feat(station): replace derelict flag with StationType on StationInfo"
```

---

## Task 3: Deterministic type + specialty + keeper_seed roll

**Files:**
- Create: `include/astra/station_roll.h`
- Create: `src/station_roll.cpp`
- Modify: `src/star_chart.cpp` (call during station creation)

- [ ] **Step 1: Create the header**

```cpp
// include/astra/station_roll.h
#pragma once
#include "astra/station_type.h"
#include <cstdint>

namespace astra {

// Deterministic. Same seed, same result. THA is handled by the caller.
StationType roll_station_type(uint64_t station_seed);
StationSpecialty roll_station_specialty(uint64_t station_seed);
uint64_t derive_keeper_seed(uint64_t station_seed);

}  // namespace astra
```

- [ ] **Step 2: Implement**

```cpp
// src/station_roll.cpp
#include "astra/station_roll.h"

namespace astra {

namespace {
uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
}

StationType roll_station_type(uint64_t seed) {
    // 70 / 10 / 7 / 7 / 6
    uint32_t r = (uint32_t)(splitmix(seed ^ 0xA1) % 100);
    if (r < 70) return StationType::NormalHub;
    if (r < 80) return StationType::Scav;
    if (r < 87) return StationType::Pirate;
    if (r < 94) return StationType::Abandoned;
    return StationType::Infested;
}

StationSpecialty roll_station_specialty(uint64_t seed) {
    uint32_t r = (uint32_t)(splitmix(seed ^ 0xB2) % 6);
    switch (r) {
        case 0: return StationSpecialty::Generic;
        case 1: return StationSpecialty::Mining;
        case 2: return StationSpecialty::Research;
        case 3: return StationSpecialty::Frontier;
        case 4: return StationSpecialty::Trade;
        default: return StationSpecialty::Industrial;
    }
}

uint64_t derive_keeper_seed(uint64_t seed) {
    return splitmix(seed ^ 0xC3);
}

}  // namespace astra
```

- [ ] **Step 3: Wire into star-chart station creation**

In `src/star_chart.cpp` find where `StationInfo` is populated for a new station (currently sets name and `derelict`). After naming the station, add:

```cpp
#include "astra/station_roll.h"
// ...
uint64_t station_seed = /* existing per-station seed, or hash(system.id, "station") */;
info.type = roll_station_type(station_seed);
info.specialty = (info.type == StationType::NormalHub)
    ? roll_station_specialty(station_seed)
    : StationSpecialty::Generic;
info.keeper_seed = derive_keeper_seed(station_seed);

// THA override:
if (system.id == 1) {
    info.type = StationType::NormalHub;
    info.specialty = StationSpecialty::Generic;
    // keeper_seed is ignored for THA because is_tha takes the scripted path
}
```

If there is no existing per-station seed, use `std::hash<std::string>{}(system.name) ^ system.id` or the system's existing rng seed combined with a salt — whatever is consistent with the rest of the codebase. Pick the first stable source visible in the file and document it in a comment.

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/station_roll.cpp`.

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: success.

- [ ] **Step 6: Commit**

```bash
git add include/astra/station_roll.h src/station_roll.cpp src/star_chart.cpp CMakeLists.txt
git commit -m "feat(station): deterministic StationType/Specialty/keeper_seed roll"
```

---

## Task 4: MapGenerator dispatch on StationType

**Files:**
- Modify: `src/map_generator.cpp`
- Modify: `include/astra/map_generator.h` (if factory signature changes)

- [ ] **Step 1: Locate current dispatch**

Run: `grep -n "SpaceStation\|derelict\|hub_generator\|derelict_generator" src/map_generator.cpp include/astra/map_generator.h`

- [ ] **Step 2: Build a StationContext at the dispatch site**

Where the factory currently picks between hub and derelict generators, construct a `StationContext` from the `StationInfo` and replace the branch:

```cpp
#include "astra/station_type.h"

StationContext ctx{
    .is_tha = (system.id == 1),
    .type = station.type,
    .specialty = station.specialty,
    .keeper_seed = station.keeper_seed,
    .station_name = station.name,
};

switch (station.type) {
    case StationType::NormalHub:
        if (ctx.is_tha) return create_hub_generator(ctx);             // THA path (existing)
        return make_hub_station_generator(ctx);                       // generalized (Task 5)
    case StationType::Scav:
        return make_scav_station_generator(ctx);                      // Task 7
    case StationType::Pirate:
        return make_pirate_station_generator(ctx);                    // Task 8
    case StationType::Abandoned:
        return make_derelict_station_generator(ctx);                  // Task 9
    case StationType::Infested:
        return make_infested_station_generator(ctx);                  // Task 10
}
```

- [ ] **Step 3: Stub unknown generators**

For any generator not yet implemented, add a temporary `#if 0` guard or a stub that returns the existing `make_station_generator()` fallback so the build stays green. Track which are stubs with `// TODO(task-N):` comments so later tasks replace them.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: success (everything still routes through existing generators for now).

- [ ] **Step 5: Commit**

```bash
git add -u
git commit -m "feat(station): dispatch map generator on StationType (stubs for new types)"
```

---

## Task 5: Generalize hub_station_generator to take StationContext

**Files:**
- Modify: `include/astra/hub_station_generator.h` (or wherever `create_hub_generator` is declared)
- Modify: `src/generators/hub_station_generator.cpp`

- [ ] **Step 1: Add the new factory**

Declare alongside the existing THA factory:

```cpp
std::unique_ptr<MapGenerator> make_hub_station_generator(const StationContext& ctx);
```

- [ ] **Step 2: Refactor internals to take context**

In `hub_station_generator.cpp`, add a second entry point `make_hub_station_generator(ctx)` that calls the shared room/NPC placement code. The existing `create_hub_generator()` keeps working by constructing a `StationContext{ .is_tha=true, .type=NormalHub, .specialty=Generic, ... }` and calling the shared path.

- [ ] **Step 3: Gate THA-only rooms on `ctx.is_tha`**

Identify every hardcoded room that's THA-only (Observatory-with-Nova, full Command Center, Maintenance Tunnels entrance). Wrap them:

```cpp
if (ctx.is_tha) {
    add_observatory_with_nova(...);
    add_command_center(...);
    add_maintenance_tunnels_entrance(...);
}
```

Base roster (always added for NormalHub): Docking Bay, Storage Bay, Cantina, Station Keeper's office.

- [ ] **Step 4: Build and run**

Run: `cmake --build build && ./build/astra`
Expected: THA still looks identical when you enter it.

- [ ] **Step 5: Commit**

```bash
git add -u
git commit -m "refactor(station): generalize hub generator to take StationContext"
```

---

## Task 6: Specialty-driven room rosters in hub generator

**Files:**
- Modify: `src/generators/hub_station_generator.cpp`

- [ ] **Step 1: Implement specialty rosters**

After the base roster, before THA-only gating, add:

```cpp
if (!ctx.is_tha) {
    switch (ctx.specialty) {
        case StationSpecialty::Mining:
            add_room(RoomFlavor::Refinery);
            add_room(RoomFlavor::StorageBay);  // extra
            maybe_add(RoomFlavor::Armory, /*weight=*/0.15f);
            break;
        case StationSpecialty::Research:
            add_room(RoomFlavor::Lab);
            add_room(RoomFlavor::Observatory);
            maybe_add(RoomFlavor::CommandCenter, 0.2f);
            break;
        case StationSpecialty::Frontier:
            add_room(RoomFlavor::Armory);
            add_room(RoomFlavor::Barracks);
            maybe_add(RoomFlavor::Observatory, 0.2f);
            break;
        case StationSpecialty::Trade:
            add_room(RoomFlavor::MarketHall);
            add_room(RoomFlavor::Cantina);  // extra
            break;
        case StationSpecialty::Industrial:
            add_room(RoomFlavor::Engineering);
            add_room(RoomFlavor::Maintenance);
            break;
        case StationSpecialty::Generic:
            // random subset from the above pool
            add_random_subset_from_all_specialties(ctx.keeper_seed, /*count=*/2);
            break;
    }
}
```

Any `RoomFlavor` enumerators that don't yet exist (Refinery, Lab, MarketHall, Barracks, Maintenance) — add them to the existing `RoomFlavor` enum and give them a minimal flavor (floor tile + one decoration fixture reused from existing palettes). Keep it simple: each new flavor is a recolor/retagging of an existing room until content tasks replace them.

- [ ] **Step 2: Build and run**

Run: `cmake --build build && ./build/astra`
Expected: THA unchanged.

- [ ] **Step 3: Commit**

```bash
git add -u
git commit -m "feat(station): specialty-driven room rosters for NormalHub stations"
```

---

## Task 7: Strip xytomorphs from populated stations

**Files:**
- Modify: `src/generators/hub_station_generator.cpp`
- Modify: `src/npc_spawner.cpp`

- [ ] **Step 1: Find xytomorph spawns in station generators**

Run: `grep -rn "xytomorph\|Xytomorph\|build_xytomorph" src/generators src/npc_spawner.cpp src/game_world.cpp`

- [ ] **Step 2: Guard xytomorph spawns by station type**

In every station-interior spawn site (not the Maintenance Tunnels dungeon, and not the Abandoned/Infested generators added later), wrap xytomorph spawn calls in:

```cpp
if (ctx.type == StationType::Abandoned || ctx.type == StationType::Infested) {
    spawn_xytomorphs(...);
}
```

For Maintenance Tunnels code (invoked from THA only), no change.

- [ ] **Step 3: Build and run**

Run: `cmake --build build && ./build/astra`
Enter THA: no xytomorphs in the hub itself. Enter Maintenance Tunnels: xytomorphs still present.

- [ ] **Step 4: Commit**

```bash
git add -u
git commit -m "fix(station): remove xytomorph spawns from populated station interiors"
```

---

## Task 8: Scav station generator + scav NPCs

**Files:**
- Create: `src/generators/scav_station_generator.{h,cpp}`
- Create: `src/npcs/scav_keeper.cpp`
- Create: `src/npcs/scav_merchant.cpp` (junk dealer)
- Modify: `include/astra/npc_defs.h` (role enum if needed)
- Modify: `src/npc_spawner.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Generator skeleton**

```cpp
// src/generators/scav_station_generator.h
#pragma once
#include "astra/map_generator.h"
#include "astra/station_type.h"
#include <memory>

namespace astra {
std::unique_ptr<MapGenerator> make_scav_station_generator(const StationContext& ctx);
}
```

```cpp
// src/generators/scav_station_generator.cpp
#include "astra/scav_station_generator.h"
#include "astra/hub_station_generator.h"  // reuse shared helpers

namespace astra {

std::unique_ptr<MapGenerator> make_scav_station_generator(const StationContext& ctx) {
    // 4-5 rooms: Docking Bay, Mess Hall (Cantina-variant), Scrap Yard (Storage/Armory-variant),
    // Keeper's nook, Bunk room. Reuse station building blocks.
    // See hub_station_generator for the room placement helpers.
    // No xytomorphs. Salvage-tile decoration for Scrap Yard.
    // Spawn NPCs via spawn_scav_npcs(ctx, map) during post-build.
    ...
}

}  // namespace astra
```

Pattern after `make_derelict_station_generator`'s structure. If a shared helper doesn't exist, factor one from `hub_station_generator.cpp` as part of this task (rooms_place_helper, etc.) — keep the factoring minimal.

- [ ] **Step 2: Scav keeper NPC**

```cpp
// src/npcs/scav_keeper.cpp
#include "astra/npc.h"
#include "astra/station_type.h"
#include "astra/station_dialog.h"  // from Task 12

namespace astra {

Npc build_scav_keeper(const StationContext& ctx) {
    Npc n;
    n.role = NpcRole::StationKeeper;
    n.name = pick_scav_keeper_name(ctx.keeper_seed);    // helper from Task 12
    n.glyph = '@';
    n.color = Color::Yellow;
    n.hostile = false;
    n.dialog = build_scav_keeper_dialog(ctx);
    return n;
}

}  // namespace astra
```

- [ ] **Step 3: Scav merchant (junk dealer)**

```cpp
// src/npcs/scav_merchant.cpp
#include "astra/npc.h"

namespace astra {

Npc build_scav_junk_dealer(const StationContext& ctx) {
    Npc n = build_merchant_base();     // existing helper
    n.name = "Junk Dealer";
    n.inventory = roll_scrap_inventory(ctx.keeper_seed);  // reuse merchant inventory fn, mark used/scrap
    n.price_multiplier = 0.6f;         // cheaper buy, lower sell
    return n;
}

}  // namespace astra
```

If `build_merchant_base()` doesn't exist under that name, look for the analogue in `src/npcs/merchant.cpp` and match its pattern.

- [ ] **Step 4: Spawner**

In `src/npc_spawner.cpp`, add:

```cpp
void spawn_scav_npcs(const StationContext& ctx, Map& map) {
    place_npc(map, find_room(RoomFlavor::KeepersNook),  build_scav_keeper(ctx));
    place_npc(map, find_room(RoomFlavor::ScrapYard),    build_scav_junk_dealer(ctx));
    // 1-2 scav civilians in Mess Hall
    for (int i = 0; i < roll_range(ctx.keeper_seed, 1, 2); ++i) {
        place_npc(map, find_room(RoomFlavor::MessHall), build_civilian(/*scav flavor*/ true));
    }
}
```

- [ ] **Step 5: Wire into dispatch**

In `map_generator.cpp`, replace the Task-4 stub for `Scav` with the real `make_scav_station_generator(ctx)`.

- [ ] **Step 6: Build and run**

Run: `cmake --build build && ./build/astra`
Use dev tools to jump to a known-Scav station seed (or use the Task-14 overlay). Verify: no xytomorphs, scav keeper with rolled name, junk dealer, salvage flavor.

- [ ] **Step 7: Commit**

```bash
git add -u
git commit -m "feat(station): Scav station generator and NPCs"
```

---

## Task 9: Pirate station generator + pirate NPCs

**Files:**
- Create: `src/generators/pirate_station_generator.{h,cpp}`
- Create: `src/npcs/pirate_captain.cpp`
- Create: `src/npcs/pirate_grunt.cpp`
- Create: `src/npcs/black_market_vendor.cpp`
- Modify: `src/npc_spawner.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Generator**

5–6 rooms: Docking Bay (patrolled), Brig, Captain's Quarters, Cantina-turned-den, Black Market back room, scattered loot stashes. Mirror the scav generator structure (Task 8, Step 1), swapping the room flavors and the NPC spawn call to `spawn_pirate_npcs(ctx, map)`.

- [ ] **Step 2: Pirate captain**

```cpp
// src/npcs/pirate_captain.cpp
#include "astra/npc.h"

namespace astra {

Npc build_pirate_captain(const StationContext& ctx) {
    Npc n;
    n.role = NpcRole::Boss;
    n.name = pick_pirate_captain_name(ctx.keeper_seed);
    n.glyph = '@';
    n.color = Color::Red;
    n.hostile = true;
    n.hp = n.hp_max = 40;      // tougher than grunts; tune to existing scale
    n.attack = 8;
    n.loot_table = LootTable::PirateCaptain;  // stub: drop one key item + credits
    return n;
}

}  // namespace astra
```

If `LootTable::PirateCaptain` doesn't exist, add the enumerator and stub it to drop a single "Pirate Key" item for now.

- [ ] **Step 3: Pirate grunt**

Straightforward hostile NPC, similar stats to existing hostile humans. Reuse any existing grunt builder if present; otherwise small new file.

- [ ] **Step 4: Black-market vendor**

```cpp
// src/npcs/black_market_vendor.cpp
#include "astra/npc.h"

namespace astra {

Npc build_black_market_vendor(const StationContext& ctx) {
    Npc n = build_merchant_base();
    n.name = "Fixer";
    n.hostile = false;                       // neutral inside a pirate station
    n.price_multiplier = 1.4f;               // premium
    n.inventory = roll_contraband_inventory(ctx.keeper_seed);  // reuse merchant inv fn, flag some items contraband
    return n;
}

}  // namespace astra
```

Add an `bool contraband = false;` field on the item struct (or reuse an existing tag field) — minimal addition; no gameplay effect yet beyond flavor.

- [ ] **Step 5: Spawner**

```cpp
void spawn_pirate_npcs(const StationContext& ctx, Map& map) {
    place_npc(map, find_room(RoomFlavor::CaptainsQuarters), build_pirate_captain(ctx));
    place_npc(map, find_room(RoomFlavor::BlackMarket),      build_black_market_vendor(ctx));
    for (int i = 0; i < roll_range(ctx.keeper_seed, 3, 5); ++i) {
        place_npc(map, any_patrolled_room(), build_pirate_grunt());
    }
}
```

- [ ] **Step 6: Wire into dispatch**

Replace the Pirate stub in `map_generator.cpp`.

- [ ] **Step 7: Build and run**

Run: `cmake --build build && ./build/astra`
Verify at a Pirate-seed station: grunts hostile on sight, captain in quarters drops key, black-market vendor is neutral and expensive.

- [ ] **Step 8: Commit**

```bash
git add -u
git commit -m "feat(station): Pirate station generator, captain, grunts, black market"
```

---

## Task 10: Abandoned tweaks (repurpose derelict generator)

**Files:**
- Modify: `src/generators/derelict_station_generator.cpp`

- [ ] **Step 1: Update factory signature**

Change `make_derelict_station_generator()` to take `const StationContext& ctx`. Update dispatch in `map_generator.cpp` (already references Task 4's stub).

- [ ] **Step 2: Add loot + wanderers**

At the end of generation:

```cpp
// 1-2 wandering monsters (xytomorph)
int count = 1 + (splitmix(ctx.keeper_seed) & 1);
for (int i = 0; i < count; ++i) {
    place_npc(map, random_non_starting_room(ctx.keeper_seed), build_xytomorph_weak());
}

// Lootable containers
scatter_loot_containers(map, /*count=*/3 + (splitmix(ctx.keeper_seed ^ 0xD1) % 4), ctx.keeper_seed);
```

If `scatter_loot_containers` doesn't exist, add a simple version that places the existing `Crate` fixture with a rolled loot table.

- [ ] **Step 3: Build and run**

Verify an Abandoned station: no NPCs, some containers, 1–2 wandering xytomorphs.

- [ ] **Step 4: Commit**

```bash
git add -u
git commit -m "feat(station): Abandoned stations — loot containers + 1-2 wandering monsters"
```

---

## Task 11: Infested station generator

**Files:**
- Create: `src/generators/infested_station_generator.{h,cpp}`
- Modify: `src/map_generator.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Generator**

Base layout from the derelict generator (dark, damaged) but:
- Spawn heavy xytomorph presence: seed 3–5 rooms as nests, each with 2–3 xytomorphs (mix of weak and full). Total ≤ 12.
- Scatter "crew remains" loot piles: reuse existing container fixture with a grim-flavor loot table (credits + personal effects).
- No NPCs.

```cpp
std::unique_ptr<MapGenerator> make_infested_station_generator(const StationContext& ctx) {
    auto gen = make_derelict_station_generator(ctx);
    // Post-process: add xytomorph nests + loot.
    gen->add_post_pass([ctx](Map& map) {
        seed_xytomorph_nests(map, ctx.keeper_seed, /*nest_count=*/3 + (splitmix(ctx.keeper_seed) & 3));
        scatter_crew_remains(map, ctx.keeper_seed, /*count=*/4);
    });
    return gen;
}
```

If `MapGenerator` lacks `add_post_pass`, inline the post-pass directly in the generator body.

- [ ] **Step 2: Wire into dispatch**

Replace the Infested stub in `map_generator.cpp`.

- [ ] **Step 3: Build and run**

Verify an Infested station: dense xytomorphs, no NPCs, loot piles.

- [ ] **Step 4: Commit**

```bash
git add -u
git commit -m "feat(station): Infested station generator with xytomorph nests"
```

---

## Task 12: Dialog split — generic vs THA lines

**Files:**
- Create: `include/astra/station_dialog.h`
- Create: `src/station_dialog.cpp`
- Modify: `src/npcs/station_keeper.cpp`
- Modify: `src/npcs/hub_npcs.cpp` (Commander, Astronomer, Arms Dealer, Engineer)
- Modify: `src/npcs/nova.cpp` (leave alone — Nova is THA-only by construction; double-check spawn is gated by `is_tha`)

- [ ] **Step 1: Dialog header**

```cpp
// include/astra/station_dialog.h
#pragma once
#include "astra/station_type.h"
#include "astra/npc.h"

namespace astra {

Dialog build_station_keeper_dialog(const StationContext& ctx);
Dialog build_scav_keeper_dialog(const StationContext& ctx);
Dialog build_commander_dialog(const StationContext& ctx);
Dialog build_astronomer_dialog(const StationContext& ctx);
Dialog build_arms_dealer_dialog(const StationContext& ctx);
Dialog build_engineer_dialog(const StationContext& ctx);

}  // namespace astra
```

Use whatever `Dialog` type the codebase already has (check `include/astra/npc.h`).

- [ ] **Step 2: Station keeper dialog**

```cpp
// src/station_dialog.cpp
Dialog build_station_keeper_dialog(const StationContext& ctx) {
    if (ctx.is_tha) {
        return tha_station_keeper_dialog();   // existing THA lore: three centuries, the Collapse, cargo-hauler quest
    }
    return generic_station_keeper_dialog(ctx);
}

Dialog generic_station_keeper_dialog(const StationContext& ctx) {
    Dialog d;
    d.greeting = fmt("Welcome to {}.", ctx.station_name);
    d.nodes.push_back({"About this station",
        fmt("We're a {} hub — mostly {} traffic these days.",
            to_string(ctx.specialty), specialty_flavor(ctx.specialty))});
    d.nodes.push_back({"Rumors", pick_generic_rumor(ctx.keeper_seed)});
    d.nodes.push_back({"Quest hook", keeper_specialty_hook(ctx)});  // Task 13
    return d;
}
```

Move the existing THA lore out of `station_keeper.cpp` into `tha_station_keeper_dialog()` verbatim — do not rewrite it.

- [ ] **Step 3: Repeat for Commander/Astronomer/Arms Dealer/Engineer**

For each, identify the dialog blocks that reference THA-specific concepts (three centuries, the Collapse, Nova, the cargo hauler). Move those into a THA-only path; write a short generic path that references the current station's name + specialty.

Minimum viable generic dialog per role: greeting + 2 generic nodes. Keep it short — content can grow later.

- [ ] **Step 4: Update each NPC builder to take `const StationContext& ctx` and call the new dialog functions**

Example diff for `build_station_keeper`:

```cpp
// before
Npc build_station_keeper() { /* ... hardcoded dialog ... */ }
// after
Npc build_station_keeper(const StationContext& ctx) {
    Npc n = /* ... */;
    n.dialog = build_station_keeper_dialog(ctx);
    return n;
}
```

Update all call sites accordingly. The hub generator already has `ctx` after Task 5; generic stations get it via the spawner (Tasks 8, 9).

- [ ] **Step 5: Build and run**

Run: `cmake --build build && ./build/astra`
At THA: keeper dialog unchanged from before. At a non-THA NormalHub station (use dev overlay): keeper greets with station name + specialty, no Collapse lore.

- [ ] **Step 6: Commit**

```bash
git add -u
git commit -m "feat(npc): split dialog into THA-specific and generic paths"
```

---

## Task 13: Unique keeper name + archetype + specialty quest hook

**Files:**
- Create: `src/npcs/keeper_personas.cpp`
- Create: `include/astra/keeper_personas.h`
- Modify: `src/station_dialog.cpp`

- [ ] **Step 1: Name tables and archetype enum**

```cpp
// include/astra/keeper_personas.h
#pragma once
#include "astra/station_type.h"
#include <string>

namespace astra {

enum class KeeperArchetype : uint8_t {
    GruffVeteran,
    ChattyBureaucrat,
    NervousNewcomer,
    RetiredSpacer,
    CorporateStiff,
    EccentricLoner,
};

std::string pick_keeper_name(uint64_t keeper_seed);
std::string pick_scav_keeper_name(uint64_t keeper_seed);
std::string pick_pirate_captain_name(uint64_t keeper_seed);
KeeperArchetype pick_keeper_archetype(uint64_t keeper_seed);
const char* archetype_voice(KeeperArchetype);   // short tone descriptor used in dialog templates
std::string keeper_specialty_hook(const StationContext& ctx);

}  // namespace astra
```

- [ ] **Step 2: Implement with small name tables**

```cpp
// src/npcs/keeper_personas.cpp
#include "astra/keeper_personas.h"
#include "astra/station_roll.h"
#include <array>

namespace astra {

namespace {
const std::array<const char*, 16> FIRST_NAMES = {
    "Alva","Brix","Cass","Doran","Elin","Ferro","Gale","Haru",
    "Iska","Jonn","Koda","Lira","Mox","Nessa","Orin","Pell",
};
const std::array<const char*, 16> LAST_NAMES = {
    "Vance","Okoro","Sato","Reyes","Mogg","Ilyin","Hale","Crane",
    "Drex","Kato","Starr","Vega","Quill","Nash","Orlov","Park",
};
const std::array<const char*, 8> SCAV_NAMES = {
    "Rust","Chum","Wire","Bolts","Pike","Hex","Kettle","Crank",
};
const std::array<const char*, 8> PIRATE_NAMES = {
    "Blackjaw","Red Nils","Captain Vex","Ironsight","Ash","Kestrel","Marrow","Scar",
};
}

std::string pick_keeper_name(uint64_t seed) {
    auto a = FIRST_NAMES[(seed >> 4) % FIRST_NAMES.size()];
    auto b = LAST_NAMES[(seed >> 20) % LAST_NAMES.size()];
    return std::string(a) + " " + b;
}

std::string pick_scav_keeper_name(uint64_t seed) {
    return SCAV_NAMES[(seed >> 8) % SCAV_NAMES.size()];
}

std::string pick_pirate_captain_name(uint64_t seed) {
    return PIRATE_NAMES[(seed >> 12) % PIRATE_NAMES.size()];
}

KeeperArchetype pick_keeper_archetype(uint64_t seed) {
    return (KeeperArchetype)((seed >> 28) % 6);
}

const char* archetype_voice(KeeperArchetype a) {
    switch (a) {
        case KeeperArchetype::GruffVeteran:     return "gruff, terse, military cadence";
        case KeeperArchetype::ChattyBureaucrat: return "polite, over-explains forms";
        case KeeperArchetype::NervousNewcomer:  return "nervous, hedges every sentence";
        case KeeperArchetype::RetiredSpacer:    return "rambling, stories about the old days";
        case KeeperArchetype::CorporateStiff:   return "formal, scripted, brand-forward";
        case KeeperArchetype::EccentricLoner:   return "cryptic, odd non-sequiturs";
    }
    return "";
}

std::string keeper_specialty_hook(const StationContext& ctx) {
    switch (ctx.specialty) {
        case StationSpecialty::Mining:
            return "A shipment of ore went missing on the belt run last cycle. Worth looking into.";
        case StationSpecialty::Research:
            return "We lost a data core when the relay burned out. If you're handy, it's still down there.";
        case StationSpecialty::Frontier:
            return "A patrol didn't come back. If you head out that way, keep your eyes open.";
        case StationSpecialty::Trade:
            return "An overdue convoy's got everyone nervous — dock fees piling up.";
        case StationSpecialty::Industrial:
            return "The long-range relay's been flaking. If you know parts, there's pay in it.";
        default:
            return "Nothing urgent. Enjoy the station.";
    }
}

}  // namespace astra
```

- [ ] **Step 3: Wire archetype voice into generic keeper dialog**

In `generic_station_keeper_dialog`, prefix the greeting with the archetype voice selected from `pick_keeper_archetype(ctx.keeper_seed)`. Archetype only picks *tone*; the dialog tree is the same for now.

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/npcs/keeper_personas.cpp`.

- [ ] **Step 5: Build and run**

Run: `cmake --build build && ./build/astra`
Visit two non-THA NormalHub stations (dev overlay — Task 14). Confirm keeper names differ and specialty hooks match the station's specialty.

- [ ] **Step 6: Commit**

```bash
git add -u
git commit -m "feat(npc): per-station keeper names, archetypes, and specialty hooks"
```

---

## Task 14: Dev overlay — print station type on star chart

**Files:**
- Modify: `src/star_chart_viewer.cpp` (or wherever the chart is drawn)

- [ ] **Step 1: Add a toggle**

In the star chart viewer (dev-mode build), add a keypress (e.g., `T`) that toggles an overlay drawing each system's station type as a single-character code next to the system glyph:

```
N = NormalHub, S = Scav, P = Pirate, A = Abandoned, I = Infested
```

For NormalHub, on deepest zoom, also show specialty letter (`M/R/F/T/I/g`).

- [ ] **Step 2: Distribution tally**

When the toggle is active, render at the bottom of the chart a tally of types seen so far across discovered-or-seeded systems:

```
Stations: 70% N  10% S  7% P  7% A  6% I   (out of 1245)
```

Compute on-demand by iterating the star chart's known stations.

- [ ] **Step 3: Build and run**

Run: `cmake -B build -DDEV=ON -DSDL=OFF && cmake --build build && ./build/astra`
Open star chart, toggle `T`. Verify the tally is within a few percent of 70/10/7/7/6 over a galaxy-sized sample (hundreds+ stations).

- [ ] **Step 4: Commit**

```bash
git add -u
git commit -m "feat(dev): star-chart overlay shows station type + distribution tally"
```

---

## Task 15: Docs + roadmap update

**Files:**
- Modify: `docs/formulas.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Formulas**

Add a "Station Type Roll" section:

```
StationType roll:
  r = splitmix(seed ^ 0xA1) mod 100
  r <70 NormalHub | r <80 Scav | r <87 Pirate | r <94 Abandoned | else Infested

StationSpecialty (NormalHub only):
  r = splitmix(seed ^ 0xB2) mod 6 -> Generic/Mining/Research/Frontier/Trade/Industrial

keeper_seed = splitmix(station_seed ^ 0xC3)
```

- [ ] **Step 2: Roadmap**

Check the "Stations" / "Space" section box for station-type variety and link to the spec.

- [ ] **Step 3: Commit**

```bash
git add docs/formulas.md docs/roadmap.md
git commit -m "docs: station type roll + roadmap update"
```

---

## Final Verification Checklist

After all tasks merge, manually verify by running `./build/astra` and using the star chart overlay:

- [ ] Distribution across a discovered galaxy is ≈ 70/10/7/7/6.
- [ ] THA entry is unchanged (Nova, Commander, Astronomer, full lore dialog all present).
- [ ] A non-THA NormalHub station: generic keeper with rolled name + specialty quest hook, no Collapse lore, no Nova, no xytomorphs.
- [ ] Two different NormalHub stations have different keeper names.
- [ ] A Scav station: scav keeper, junk dealer, no xytomorphs, no armed conflict on sight.
- [ ] A Pirate station: grunts hostile on sight, captain in quarters drops loot, black-market vendor neutral and expensive.
- [ ] An Abandoned station: no NPCs, lootable containers, 1–2 wandering xytomorphs.
- [ ] An Infested station: dense xytomorphs, loot piles, no NPCs.
- [ ] THA Maintenance Tunnels still have xytomorphs.
- [ ] Loading a pre-existing save upgrades `derelict` stations to `StationType::Abandoned` correctly.
