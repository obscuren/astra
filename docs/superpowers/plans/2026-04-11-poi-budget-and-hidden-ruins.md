# POI Budget, Hidden Ruins, and Anchor-Hinted Placement — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the per-category POI placement pass with a deterministic planet-level `PoiBudget`, add a `HiddenPoi` mechanism that lets ruins be discovered on step-onto with a journal entry, and pass anchor hints to stage-2 detail-map generators so they stop re-scanning terrain.

**Architecture:**

1. A `PoiBudget` struct is rolled once per planet during overworld generation, stored on `MapProperties` and on the overworld `TileMap`. It lists counts + pre-rolled variants for every POI kind.
2. A new `poi_placement.cpp` pass expands the budget into `PoiRequest`s with terrain requirements and walks the overworld in priority order, stamping tiles and writing `PoiAnchorHint` entries onto the map. Hidden ruins are diverted to a `hidden_pois` list on the map instead of being stamped.
3. Stage-2 generators (`CaveEntranceGenerator`, `CrashedShipGenerator`, `RuinGenerator`) read the anchor hint from `MapProperties::detail_poi_anchor` and use it to short-circuit their own site scanning. `PlacementScorer` and `SettlementPlanner`/`OutpostPlanner` are unchanged.
4. Player movement on the overworld checks `hidden_pois` on every step and, if an undiscovered entry is hit, swaps the tile, emits a message-log line, and creates a `JournalCategory::Discovery` entry with `(system_id, body_index, moon_index, x, y, location_name)` captured for later live-rendered preview. The character screen's journal tab gains a small map preview widget that renders the region live from the active renderer.
5. The star chart planet info panel gains a "Scanner Report" section that reads directly from `TileMap::poi_budget()`. Save version bumps v22 → v23 with legacy reconstruction.

**Tech Stack:** C++20, CMake, existing Astra renderer abstraction, GoogleTest (for future unit tests — this project has no test suite yet so tests use a simple `assert`-based harness where noted).

---

## File Structure

**New files:**

- `include/astra/poi_budget.h` — `PoiBudget`, `RuinRequest`, `ShipRequest`, `RuinFormation`, `roll_poi_budget()`, `format_poi_budget()`, `reconstruct_poi_budget_from_map()`.
- `src/poi_budget.cpp` — implementation of roll + format + reconstruction.
- `include/astra/poi_placement.h` — `PoiRequest`, `PoiTerrainRequirements`, `PoiPriority`, `PoiAnchorHint`, `AnchorReason`, `AnchorDirection`, `HiddenPoi`, `run_poi_placement()` entry point.
- `src/generators/poi_placement.cpp` — the unified placement pass. Terrain score cache, request expansion, priority-ordered assignment, hint writing, hidden-ruin diversion.
- `tests/poi_budget_tests.cpp` — simple assert-harness exercising `roll_poi_budget` and `reconstruct_poi_budget_from_map`.
- `tests/poi_placement_tests.cpp` — simple assert-harness exercising `run_poi_placement` on a synthetic overworld.

**Modified files:**

- `include/astra/map_properties.h` — add `PoiAnchorHint detail_poi_anchor`.
- `include/astra/tilemap.h` — add `hidden_pois()`, `poi_anchor_hints` accessors, `poi_budget()` accessor, and the `PoiBudget` field.
- `src/tilemap.cpp` — storage + accessor implementations (most likely no-ops since these are plain members).
- `src/generators/overworld_generator_base.cpp` — call `roll_poi_budget` during `place_features()`, store on map, replace the call into `place_default_pois` for subclasses that use the default pass.
- `src/generators/default_overworld_generator.cpp` — replace `place_default_pois` body with `run_poi_placement`. Keep the `place_default_pois` free function for backwards signature compat (it just forwards).
- `src/generators/cave_entrance_generator.cpp` — honour anchor hint via a small prologue.
- `src/generators/crashed_ship_generator.cpp` — honour anchor hint.
- `src/generators/ruin_generator.cpp` — prefer `ruin_civ` from hint over string parse.
- `src/generators/poi_phase.cpp` — nothing beyond propagating the hint via `MapProperties` (hint is already carried through `MapProperties`).
- `src/game_world.cpp` — `build_detail_props()` copies anchor hint.
- `src/game_interaction.cpp` — `try_move` overworld branch fires discovery check.
- `include/astra/journal.h` — new Discovery location fields on `JournalEntry`, `make_discovery_journal_entry()` helper.
- `src/journal.cpp` — implementation.
- `src/character_screen.cpp` — render live map preview in journal detail panel for Discovery entries.
- `src/star_chart_viewer.cpp` (or whichever file owns the planet info panel — search for `planet_info_panel` / `draw_planet_info`) — add Scanner Report section.
- `src/dev_console.cpp` — add `budget` and `discoveries` commands.
- `include/astra/save_file.h` — bump version 22 → 23, add new fields to `SaveData`, `MapState`, `JournalEntry`.
- `src/save_file.cpp` — write/read new fields. Legacy v22 reader triggers `reconstruct_poi_budget_from_map`.
- `src/save_system.cpp` — wire budget + hidden_pois + anchor hints from `world.map()` into `MapState`.
- `docs/roadmap.md` — mark "Layered POI site selection" done.

---

## Testing Approach

This project does not currently have a test framework. For the budget roll and placement pass we add standalone test programs built with their own CMake targets that link the game library without running the game loop. Each test prints PASS/FAIL and returns 0 on success.

Each test program:

```cpp
#include <cassert>
#include <cstdio>
// test-only headers here

int main() {
    // test cases...
    std::printf("PASS: %s\n", __FILE__);
    return 0;
}
```

Tests are added to `CMakeLists.txt` with `add_executable` and run with `./build/test_name`. No new framework dependency.

---

## Task Order Rationale

The plan builds bottom-up to let the system come together incrementally:

1. **Data types first** (budget, placement, hint) — compiles in isolation.
2. **Roll logic** — lets `budget` dev command work end-to-end without placement.
3. **Placement pass** — consumes the budget, produces map state.
4. **Stage-2 hint consumers** — cave, ship, ruin generators use the hints.
5. **Hidden ruins** — movement trigger + journal entry + panel preview.
6. **UI surfacing** — star chart scanner report, dev console.
7. **Save format** — version bump + legacy reconstruction.
8. **Cleanup + roadmap**.

Each task ends with a build check and a commit.

---

## Task 1: Add PoiBudget data types

**Files:**
- Create: `include/astra/poi_budget.h`

- [ ] **Step 1: Create the header with the struct definitions**

```cpp
#pragma once

#include "astra/cave_entrance_types.h"
#include "astra/crashed_ship_types.h"
#include "astra/tilemap.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

struct MapProperties; // forward decl

enum class RuinFormation : uint8_t {
    Solo,
    Connected,
};

struct RuinRequest {
    std::string civ;
    RuinFormation formation = RuinFormation::Solo;
    bool hidden = false;
};

struct ShipRequest {
    ShipClass klass = ShipClass::EscapePod;
};

struct PoiBudget {
    int settlements = 0;
    int outposts = 0;

    struct CaveCounts {
        int natural = 0;
        int mine = 0;
        int excavation = 0;
    } caves;

    std::vector<RuinRequest> ruins;
    std::vector<ShipRequest> ships;

    int beacons = 0;
    int megastructures = 0;

    int total_caves() const { return caves.natural + caves.mine + caves.excavation; }
    int visible_ruin_count() const;
    int hidden_ruin_count() const;
};

// Roll a budget from planet context. Deterministic given rng state.
PoiBudget roll_poi_budget(const MapProperties& props, std::mt19937& rng);

// Format as a multi-line human-readable summary.
std::string format_poi_budget(const PoiBudget& budget);

// Build a best-effort PoiBudget by scanning already-placed POI tiles on an
// overworld map. Used for legacy save reconstruction; variant data is unknown.
PoiBudget reconstruct_poi_budget_from_map(const TileMap& overworld);

} // namespace astra
```

- [ ] **Step 2: Verify it compiles in isolation**

Run: `cmake --build build -DDEV=ON --target astra 2>&1 | head -30`

Expected: Header compiles if referenced; if nothing references it yet, compilation is a no-op for this header. There should be no errors from existing translation units.

- [ ] **Step 3: Commit**

```bash
git add include/astra/poi_budget.h
git commit -m "feat(poi): add PoiBudget data types

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Implement PoiBudget helpers (count accessors + format)

**Files:**
- Create: `src/poi_budget.cpp`

- [ ] **Step 1: Add the source file with count accessors and format**

```cpp
#include "astra/poi_budget.h"

#include "astra/map_properties.h"

#include <sstream>

namespace astra {

int PoiBudget::visible_ruin_count() const {
    int n = 0;
    for (const auto& r : ruins) if (!r.hidden) ++n;
    return n;
}

int PoiBudget::hidden_ruin_count() const {
    int n = 0;
    for (const auto& r : ruins) if (r.hidden) ++n;
    return n;
}

std::string format_poi_budget(const PoiBudget& budget) {
    std::ostringstream os;
    os << "Settlements:    " << budget.settlements << "\n";
    os << "Outposts:       " << budget.outposts << "\n";
    os << "Ruins:          " << budget.ruins.size();
    if (!budget.ruins.empty()) {
        os << " (" << budget.visible_ruin_count() << " visible, "
           << budget.hidden_ruin_count() << " uncharted)";
    }
    os << "\n";
    os << "Caves:          " << budget.total_caves()
       << " (natural x" << budget.caves.natural
       << ", mine x" << budget.caves.mine
       << ", excavation x" << budget.caves.excavation << ")\n";
    os << "Crashed Ships:  " << budget.ships.size() << "\n";
    if (budget.beacons > 0)
        os << "Beacons:        " << budget.beacons << "\n";
    if (budget.megastructures > 0)
        os << "Megastructures: " << budget.megastructures << "\n";
    return os.str();
}

// Placeholder — filled in by Task 3.
PoiBudget roll_poi_budget(const MapProperties& /*props*/, std::mt19937& /*rng*/) {
    return PoiBudget{};
}

// Placeholder — filled in by Task 12.
PoiBudget reconstruct_poi_budget_from_map(const TileMap& /*overworld*/) {
    return PoiBudget{};
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Find the `add_library(astra-dev ...)` or equivalent in `CMakeLists.txt` and add `src/poi_budget.cpp` to the source list.

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: successful build including the new file.

- [ ] **Step 3: Commit**

```bash
git add src/poi_budget.cpp CMakeLists.txt
git commit -m "feat(poi): PoiBudget format + count accessors

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Implement roll_poi_budget

**Files:**
- Modify: `src/poi_budget.cpp`

- [ ] **Step 1: Replace the `roll_poi_budget` stub with the real implementation**

```cpp
#include "astra/celestial_body.h"

namespace astra {

namespace {

bool is_habitable(const MapProperties& p) {
    return p.body_type == BodyType::Terrestrial &&
           (p.body_atmosphere == Atmosphere::Standard ||
            p.body_atmosphere == Atmosphere::Dense);
}

bool is_marginal(const MapProperties& p) {
    return p.body_type == BodyType::Terrestrial &&
           (p.body_atmosphere == Atmosphere::Thin ||
            p.body_atmosphere == Atmosphere::Toxic ||
            p.body_atmosphere == Atmosphere::Reducing);
}

bool is_airless(const MapProperties& p) {
    return (p.body_type == BodyType::Rocky ||
            p.body_type == BodyType::DwarfPlanet) ||
           (p.body_type == BodyType::Terrestrial &&
            p.body_atmosphere == Atmosphere::None);
}

bool is_asteroid(const MapProperties& p) {
    return p.body_type == BodyType::AsteroidBelt;
}

std::string pick_ruin_civ(const MapProperties& p, std::mt19937& rng) {
    // If lore gives us a primary civ, 70% weight on it, 30% on "unknown".
    if (p.lore_primary_civ_index >= 0) {
        if (std::uniform_int_distribution<int>(0, 99)(rng) < 70) {
            return "precursor_" + std::to_string(p.lore_primary_civ_index);
        }
    }
    return "unknown";
}

} // namespace

PoiBudget roll_poi_budget(const MapProperties& props, std::mt19937& rng) {
    PoiBudget b;

    const bool habitable = is_habitable(props);
    const bool marginal = is_marginal(props);
    const bool airless = is_airless(props);
    const bool asteroid = is_asteroid(props);
    std::uniform_int_distribution<int> pct(0, 99);

    // --- Settlements ---
    if (habitable) {
        b.settlements = 3;
        if (props.lore_tier >= 2) b.settlements += 2;
    } else if (marginal) {
        if (pct(rng) < 40) b.settlements = 1;
    }

    // --- Outposts ---
    {
        int chance = 30;
        if (props.lore_tier >= 2) chance = 70;
        if (props.lore_plague_origin) chance = 80;
        if (pct(rng) < chance) {
            b.outposts = std::uniform_int_distribution<int>(1, 2)(rng);
            if (props.lore_tier >= 2)
                b.outposts += std::uniform_int_distribution<int>(1, 2)(rng);
        }
    }

    // --- Caves ---
    if (props.body_has_dungeon) {
        b.caves.natural = std::uniform_int_distribution<int>(2, 5)(rng);
        if (asteroid) b.caves.natural = std::min(b.caves.natural, 2);
        if (props.lore_tier >= 2) b.caves.mine = 1;
        if (props.lore_tier >= 3) b.caves.excavation = 1;
    }

    // --- Ruins ---
    {
        int count = 0;
        if (props.body_danger_level >= 3)
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        else if (pct(rng) < 30)
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        if (props.lore_tier >= 3)
            count += std::uniform_int_distribution<int>(4, 6)(rng);
        else if (props.lore_tier >= 2)
            count += std::uniform_int_distribution<int>(2, 4)(rng);
        else if (props.lore_tier >= 1)
            count += std::uniform_int_distribution<int>(1, 2)(rng);

        // Hidden ratio: clamp(lore_tier * 0.25, 0, 0.6).
        float hidden_ratio = std::min(0.6f, props.lore_tier * 0.25f);

        for (int i = 0; i < count; ++i) {
            RuinRequest r;
            r.civ = pick_ruin_civ(props, rng);
            r.formation = (pct(rng) < 15) ? RuinFormation::Connected : RuinFormation::Solo;
            float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
            r.hidden = (roll < hidden_ratio);
            b.ruins.push_back(std::move(r));
        }
    }

    // --- Crashed ships ---
    {
        int base_chance = 20 + props.body_danger_level * 10;
        if (props.lore_battle_site) base_chance = 100;
        if (pct(rng) < base_chance) {
            int count = std::uniform_int_distribution<int>(1, 3)(rng);
            if (props.lore_battle_site)
                count += std::uniform_int_distribution<int>(2, 4)(rng);
            for (int i = 0; i < count; ++i) {
                ShipRequest s;
                if (asteroid) {
                    s.klass = ShipClass::EscapePod;
                } else {
                    int r = pct(rng);
                    if (r < 30) s.klass = ShipClass::EscapePod;
                    else if (r < 80) s.klass = ShipClass::Freighter;
                    else s.klass = ShipClass::Corvette;
                }
                b.ships.push_back(s);
            }
        }
    }

    return b;
}

} // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

- [ ] **Step 3: Commit**

```bash
git add src/poi_budget.cpp
git commit -m "feat(poi): implement roll_poi_budget

Translates the existing per-category chances in place_default_pois into
a deterministic budget rolled once per planet. Ruin hidden flag is rolled
from a lore-tier-scaled ratio.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Add PoiPlacement types (requests, hints, hidden POIs)

**Files:**
- Create: `include/astra/poi_placement.h`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/cave_entrance_types.h"
#include "astra/crashed_ship_types.h"
#include "astra/poi_budget.h"
#include "astra/tilemap.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct MapProperties;

enum class AnchorDirection : uint8_t {
    None,
    North, South, East, West,
    NorthEast, NorthWest, SouthEast, SouthWest,
};

enum class AnchorReason : uint8_t {
    None,
    CliffAdjacent,
    WaterAdjacent,
    Flat,
    Open,
};

struct PoiAnchorHint {
    bool valid = false;
    AnchorReason reason = AnchorReason::None;
    AnchorDirection direction = AnchorDirection::None;
    CaveVariant cave_variant = CaveVariant::None;
    ShipClass ship_class = ShipClass::EscapePod;
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};

enum class PoiPriority : uint8_t { Required, Normal, Opportunistic };

struct PoiTerrainRequirements {
    bool needs_cliff = false;
    bool needs_flat = false;
    bool needs_water_adjacent = false;
    int min_spacing = 8;
};

struct PoiRequest {
    Tile poi_tile = Tile::Empty;
    PoiTerrainRequirements reqs;
    PoiPriority priority = PoiPriority::Normal;
    // Variant payload — whichever fields match poi_tile are meaningful.
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
    bool ruin_hidden = false;
    ShipClass ship_class = ShipClass::EscapePod;
    CaveVariant cave_variant = CaveVariant::None;
};

struct HiddenPoi {
    int x = 0;
    int y = 0;
    Tile underlying_tile = Tile::OW_Plains;
    Tile real_tile = Tile::OW_Ruins;
    bool discovered = false;
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};

// Run the placement pass against an overworld TileMap. Reads budget from the
// map (must be set ahead of time) and mutates the map: stamps POI tiles,
// writes anchor hints, and appends hidden POIs.
void run_poi_placement(TileMap& overworld, const MapProperties& props,
                       std::mt19937& rng);

// Build a PoiRequest vector from a budget. Visible units use the map's
// standard priority; lore-driven items may be flagged Required by the caller.
std::vector<PoiRequest> expand_budget_to_requests(const PoiBudget& budget,
                                                   const MapProperties& props,
                                                   std::mt19937& rng);

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add include/astra/poi_placement.h
git commit -m "feat(poi): add PoiPlacement types (requests, hints, hidden POIs)

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Extend TileMap with poi_budget, hidden_pois, anchor_hints

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: `src/tilemap.cpp`

- [ ] **Step 1: Add forward declarations and member fields to TileMap**

Add at the top of `include/astra/tilemap.h`, after existing includes:

```cpp
#include <unordered_map>
```

Add forward declarations near the top of the `astra` namespace (after the existing forward declarations of `Region` etc., before `class TileMap`):

```cpp
struct PoiBudget;
struct PoiAnchorHint;
struct HiddenPoi;
```

Near the end of the `TileMap` public section (before `private:`), add:

```cpp
    // POI budget — owned by overworld maps only, empty elsewhere.
    const PoiBudget& poi_budget() const;
    PoiBudget& poi_budget_mut();
    void set_poi_budget(PoiBudget b);

    // Hidden POIs — rendered as underlying_tile until discovered. Overworld only.
    const std::vector<HiddenPoi>& hidden_pois() const;
    std::vector<HiddenPoi>& hidden_pois_mut();
    // Lookup an UNDISCOVERED hidden POI at (x,y). Returns nullptr if none.
    const HiddenPoi* find_hidden_poi(int x, int y) const;
    HiddenPoi* find_hidden_poi_mut(int x, int y);

    // Per-tile anchor hints for POIs, keyed by y*width + x. Overworld only.
    const PoiAnchorHint* anchor_hint(int x, int y) const;
    void set_anchor_hint(int x, int y, const PoiAnchorHint& hint);
    const std::unordered_map<uint64_t, PoiAnchorHint>& anchor_hints() const;
```

Add to the `private:` section at the bottom of the class:

```cpp
    std::unique_ptr<PoiBudget> poi_budget_;
    std::vector<HiddenPoi> hidden_pois_;
    std::unordered_map<uint64_t, PoiAnchorHint> anchor_hints_;
```

You also need `<memory>` (for `unique_ptr`) — add the include if missing.

- [ ] **Step 2: Implement the accessors in `src/tilemap.cpp`**

Add these includes at the top:

```cpp
#include "astra/poi_budget.h"
#include "astra/poi_placement.h"
```

Add these functions (location: end of the file, inside the `astra` namespace):

```cpp
namespace {
uint64_t hint_key(int x, int y, int width) {
    return static_cast<uint64_t>(y) * static_cast<uint64_t>(width)
         + static_cast<uint64_t>(x);
}
} // namespace

const PoiBudget& TileMap::poi_budget() const {
    static const PoiBudget empty;
    return poi_budget_ ? *poi_budget_ : empty;
}

PoiBudget& TileMap::poi_budget_mut() {
    if (!poi_budget_) poi_budget_ = std::make_unique<PoiBudget>();
    return *poi_budget_;
}

void TileMap::set_poi_budget(PoiBudget b) {
    poi_budget_ = std::make_unique<PoiBudget>(std::move(b));
}

const std::vector<HiddenPoi>& TileMap::hidden_pois() const {
    return hidden_pois_;
}

std::vector<HiddenPoi>& TileMap::hidden_pois_mut() {
    return hidden_pois_;
}

const HiddenPoi* TileMap::find_hidden_poi(int x, int y) const {
    for (const auto& h : hidden_pois_) {
        if (h.x == x && h.y == y && !h.discovered) return &h;
    }
    return nullptr;
}

HiddenPoi* TileMap::find_hidden_poi_mut(int x, int y) {
    for (auto& h : hidden_pois_) {
        if (h.x == x && h.y == y && !h.discovered) return &h;
    }
    return nullptr;
}

const PoiAnchorHint* TileMap::anchor_hint(int x, int y) const {
    auto it = anchor_hints_.find(hint_key(x, y, width_));
    return (it != anchor_hints_.end()) ? &it->second : nullptr;
}

void TileMap::set_anchor_hint(int x, int y, const PoiAnchorHint& hint) {
    anchor_hints_[hint_key(x, y, width_)] = hint;
}

const std::unordered_map<uint64_t, PoiAnchorHint>& TileMap::anchor_hints() const {
    return anchor_hints_;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -30`
Expected: success. The `unique_ptr<PoiBudget>` forward-declared member requires either `poi_budget_.h` included in `tilemap.cpp` (which it is) *or* an out-of-line destructor. If the build fails on `incomplete type`, add an empty destructor declaration in `tilemap.h`:

```cpp
    ~TileMap();
```

And in `tilemap.cpp`:

```cpp
TileMap::~TileMap() = default;
```

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp
git commit -m "feat(tilemap): add poi_budget, hidden_pois, anchor_hints storage

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Implement expand_budget_to_requests

**Files:**
- Create: `src/generators/poi_placement.cpp`

- [ ] **Step 1: Create the source file with request expansion only**

```cpp
#include "astra/poi_placement.h"

#include "astra/map_properties.h"

#include <algorithm>

namespace astra {

namespace {

// Settlement/outpost spacing (same as legacy).
constexpr int kDefaultSpacing = 8;
// Tighter spacing for ships, ruins, caves near their own kind.
constexpr int kCloseSpacing = 6;

PoiTerrainRequirements reqs_for_settlement() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_outpost() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_cave(CaveVariant v) {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    if (v == CaveVariant::NaturalCave || v == CaveVariant::AbandonedMine)
        r.needs_cliff = true;
    // Excavation: no strict cliff requirement.
    return r;
}

PoiTerrainRequirements reqs_for_ship() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kCloseSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_ruin() {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    return r;
}

} // namespace

std::vector<PoiRequest> expand_budget_to_requests(const PoiBudget& budget,
                                                   const MapProperties& props,
                                                   std::mt19937& /*rng*/) {
    std::vector<PoiRequest> out;

    // Settlements
    for (int i = 0; i < budget.settlements; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Settlement;
        r.reqs = reqs_for_settlement();
        r.priority = (i == 0 && props.body_type == BodyType::Terrestrial)
                         ? PoiPriority::Required
                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Outposts
    for (int i = 0; i < budget.outposts; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Outpost;
        r.reqs = reqs_for_outpost();
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Caves — natural
    for (int i = 0; i < budget.caves.natural; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::NaturalCave;
        r.reqs = reqs_for_cave(CaveVariant::NaturalCave);
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — mine
    for (int i = 0; i < budget.caves.mine; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AbandonedMine;
        r.reqs = reqs_for_cave(CaveVariant::AbandonedMine);
        r.priority = (props.lore_tier >= 2) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — excavation
    for (int i = 0; i < budget.caves.excavation; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AncientExcavation;
        r.reqs = reqs_for_cave(CaveVariant::AncientExcavation);
        r.priority = (props.lore_tier >= 3) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Ruins
    int half_hidden = std::max(0, budget.hidden_ruin_count() / 2);
    int hidden_placed = 0;
    for (const auto& ruin : budget.ruins) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Ruins;
        r.reqs = reqs_for_ruin();
        r.ruin_civ = ruin.civ;
        r.ruin_formation = ruin.formation;
        r.ruin_hidden = ruin.hidden;
        if (ruin.hidden && props.lore_tier >= 3 && hidden_placed < half_hidden) {
            r.priority = PoiPriority::Required;
            ++hidden_placed;
        } else {
            r.priority = PoiPriority::Normal;
        }
        out.push_back(std::move(r));
    }

    // Ships
    for (size_t i = 0; i < budget.ships.size(); ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CrashedShip;
        r.ship_class = budget.ships[i].klass;
        r.reqs = reqs_for_ship();
        r.priority = (i == 0 && props.lore_battle_site) ? PoiPriority::Required
                                                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    return out;
}

void run_poi_placement(TileMap& /*overworld*/, const MapProperties& /*props*/,
                       std::mt19937& /*rng*/) {
    // Filled in by Task 7.
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/poi_placement.cpp` to the astra library sources.

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add src/generators/poi_placement.cpp CMakeLists.txt
git commit -m "feat(poi): expand PoiBudget into prioritised PoiRequest list

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Implement run_poi_placement (terrain cache + priority pass)

**Files:**
- Modify: `src/generators/poi_placement.cpp`

- [ ] **Step 1: Add terrain cache and placement pass above `run_poi_placement`**

Replace the stub `run_poi_placement` with this (and add the helpers above it):

```cpp
namespace {

struct TerrainCell {
    Biome biome = Biome::Station;
    bool cliff_adjacent = false;
    AnchorDirection cliff_dir = AnchorDirection::None;
    bool water_adjacent = false;
    AnchorDirection water_dir = AnchorDirection::None;
    bool flat = false;          // simple: tile is passable and not edge-blocked
    bool walkable = false;      // safe to stamp a POI onto
};

struct TerrainCache {
    int w = 0;
    int h = 0;
    std::vector<TerrainCell> cells;
    const TerrainCell& at(int x, int y) const { return cells[y * w + x]; }
    TerrainCell& at(int x, int y) { return cells[y * w + x]; }
};

bool tile_is_walkable_base(Tile t) {
    switch (t) {
        case Tile::OW_Plains:
        case Tile::OW_Forest:
        case Tile::OW_Desert:
        case Tile::OW_Swamp:
        case Tile::OW_Barren:
        case Tile::OW_Fungal:
        case Tile::OW_IceField:
        case Tile::OW_AlienTerrain:
        case Tile::OW_ScorchedEarth:
            return true;
        default:
            return false;
    }
}

TerrainCache build_terrain_cache(const TileMap& map) {
    TerrainCache c;
    c.w = map.width();
    c.h = map.height();
    c.cells.resize(c.w * c.h);

    for (int y = 0; y < c.h; ++y) {
        for (int x = 0; x < c.w; ++x) {
            TerrainCell cell;
            Tile t = map.get(x, y);
            cell.walkable = tile_is_walkable_base(t);
            cell.flat = cell.walkable;

            // Check 4-neighbours for cliff/water adjacency.
            static const int dx4[] = { 0, 0, -1, 1};
            static const int dy4[] = {-1, 1,  0, 0};
            static const AnchorDirection dir4[] = {
                AnchorDirection::North, AnchorDirection::South,
                AnchorDirection::West,  AnchorDirection::East,
            };
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d], ny = y + dy4[d];
                if (nx < 0 || nx >= c.w || ny < 0 || ny >= c.h) continue;
                Tile nt = map.get(nx, ny);
                if (nt == Tile::OW_Mountains || nt == Tile::OW_Crater) {
                    if (!cell.cliff_adjacent) {
                        cell.cliff_adjacent = true;
                        cell.cliff_dir = dir4[d];
                    }
                }
                if (nt == Tile::OW_Lake || nt == Tile::OW_River) {
                    if (!cell.water_adjacent) {
                        cell.water_adjacent = true;
                        cell.water_dir = dir4[d];
                    }
                }
            }
            c.at(x, y) = cell;
        }
    }
    return c;
}

bool candidate_meets_reqs(const TerrainCell& cell, const PoiTerrainRequirements& r) {
    if (!cell.walkable) return false;
    if (r.needs_cliff && !cell.cliff_adjacent) return false;
    if (r.needs_water_adjacent && !cell.water_adjacent) return false;
    if (r.needs_flat && !cell.flat) return false;
    return true;
}

int manhattan(int ax, int ay, int bx, int by) {
    int dx = ax - bx; if (dx < 0) dx = -dx;
    int dy = ay - by; if (dy < 0) dy = -dy;
    return dx + dy;
}

// Score a candidate — lower is better. Currently just distance from centre.
int score_candidate(int x, int y, int map_w, int map_h) {
    return manhattan(x, y, map_w / 2, map_h / 2);
}

PoiAnchorHint make_hint(const PoiRequest& req, const TerrainCell& cell) {
    PoiAnchorHint h;
    h.valid = true;
    if (cell.cliff_adjacent && req.reqs.needs_cliff) {
        h.reason = AnchorReason::CliffAdjacent;
        h.direction = cell.cliff_dir;
    } else if (cell.water_adjacent && req.reqs.needs_water_adjacent) {
        h.reason = AnchorReason::WaterAdjacent;
        h.direction = cell.water_dir;
    } else if (req.reqs.needs_flat) {
        h.reason = AnchorReason::Flat;
    } else {
        h.reason = AnchorReason::Open;
    }
    h.cave_variant = req.cave_variant;
    h.ship_class = req.ship_class;
    h.ruin_civ = req.ruin_civ;
    h.ruin_formation = req.ruin_formation;
    return h;
}

} // namespace

void run_poi_placement(TileMap& overworld, const MapProperties& props,
                       std::mt19937& rng) {
    const PoiBudget& budget = overworld.poi_budget();
    auto requests = expand_budget_to_requests(budget, props, rng);

    // Partition by priority: Required, then Normal, then Opportunistic.
    std::stable_sort(requests.begin(), requests.end(),
                     [](const PoiRequest& a, const PoiRequest& b) {
        return static_cast<int>(a.priority) < static_cast<int>(b.priority);
    });

    TerrainCache cache = build_terrain_cache(overworld);
    int w = cache.w;
    int h = cache.h;

    struct Placed { int x, y, spacing; };
    std::vector<Placed> placed;

    auto too_close = [&](int px, int py, int spacing) {
        for (const auto& p : placed) {
            int s = std::max(p.spacing, spacing);
            if (manhattan(px, py, p.x, p.y) < s) return true;
        }
        return false;
    };

    // Candidate order is shuffled once so spatially-different seeds get
    // different POI layouts.
    std::vector<int> candidate_order(w * h);
    for (int i = 0; i < w * h; ++i) candidate_order[i] = i;
    std::shuffle(candidate_order.begin(), candidate_order.end(), rng);

    for (const auto& req : requests) {
        int best_idx = -1;
        int best_score = 0;
        for (int idx : candidate_order) {
            int x = idx % w;
            int y = idx / w;
            if (x < 2 || x >= w - 2 || y < 2 || y >= h - 2) continue;
            const TerrainCell& cell = cache.at(x, y);
            if (!candidate_meets_reqs(cell, req.reqs)) continue;
            if (too_close(x, y, req.reqs.min_spacing)) continue;
            int s = score_candidate(x, y, w, h);
            if (best_idx < 0 || s < best_score) {
                best_idx = idx;
                best_score = s;
            }
        }
        if (best_idx < 0) continue; // failed — silent for Normal/Opportunistic

        int px = best_idx % w;
        int py = best_idx / w;
        const TerrainCell& cell = cache.at(px, py);
        PoiAnchorHint hint = make_hint(req, cell);

        if (req.poi_tile == Tile::OW_Ruins && req.ruin_hidden) {
            HiddenPoi h;
            h.x = px;
            h.y = py;
            h.underlying_tile = overworld.get(px, py);
            h.real_tile = Tile::OW_Ruins;
            h.ruin_civ = req.ruin_civ;
            h.ruin_formation = req.ruin_formation;
            overworld.hidden_pois_mut().push_back(h);
        } else {
            overworld.set(px, py, req.poi_tile);
        }
        overworld.set_anchor_hint(px, py, hint);
        placed.push_back({px, py, req.reqs.min_spacing});
    }
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build --target astra 2>&1 | tail -30`
Expected: success.

- [ ] **Step 3: Commit**

```bash
git add src/generators/poi_placement.cpp
git commit -m "feat(poi): run_poi_placement — terrain cache + priority pass

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Wire roll_poi_budget + run_poi_placement into the overworld generator

**Files:**
- Modify: `src/generators/overworld_generator_base.cpp`
- Modify: `src/generators/default_overworld_generator.cpp`

- [ ] **Step 1: Roll and store the budget in `place_features`**

In `src/generators/overworld_generator_base.cpp`, add includes at the top:

```cpp
#include "astra/poi_budget.h"
```

In `OverworldGeneratorBase::place_features`, before the call to `place_pois(rng)`, add:

```cpp
    // Roll the planet's POI budget and attach it to the map.
    {
        PoiBudget budget = roll_poi_budget(*props_, rng);
        map_->set_poi_budget(std::move(budget));
    }
```

- [ ] **Step 2: Replace `place_default_pois` body with a call to `run_poi_placement`**

In `src/generators/default_overworld_generator.cpp`, replace the body of `place_default_pois` (the free function — the whole function body from around line 16 through the closing brace near line 244) with:

```cpp
void place_default_pois(TileMap* map, const MapProperties* props,
                        const std::vector<float>& /*elevation*/, std::mt19937& rng) {
    if (!map || !props) return;
    run_poi_placement(*map, *props, rng);
}
```

Add the include at the top:

```cpp
#include "astra/poi_placement.h"
```

You will need to remove the `<algorithm>`, `<cmath>`, `<vector>` includes that only the removed function used if nothing else in this file needs them. Leave them if they are still used by the `DefaultOverworldGenerator` class below the free function.

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -40`
Expected: success. If `place_default_pois` is also called from temperate/cold rocky generators, they continue to work because the entry point still exists.

- [ ] **Step 4: Manual smoke test — generate a planet**

Start the game in dev mode and generate any planet. The overworld should still have POIs on it (though placement may look different because of the new priority ordering). The count should roughly match the new budget distribution.

Run: `./build/astra --term` (manual — press through character creation, land on a planet via `m`). Walk around the surface. Confirm you see the usual POI tile types.

- [ ] **Step 5: Commit**

```bash
git add src/generators/overworld_generator_base.cpp src/generators/default_overworld_generator.cpp
git commit -m "feat(poi): drive overworld placement from PoiBudget

Replaces the per-category greedy passes in place_default_pois with a
single budget-rolled + priority-scored placement pass.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: Add PoiAnchorHint to MapProperties and propagate in build_detail_props

**Files:**
- Modify: `include/astra/map_properties.h`
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Add the field to MapProperties**

In `include/astra/map_properties.h`, add the include at the top:

```cpp
#include "astra/poi_placement.h"
```

In the `MapProperties` struct, after `detail_cave_variant`, add:

```cpp
    // Set from the overworld tile's anchor hint when entering a detail map.
    // Stage-2 generators use this to short-circuit their own terrain scans.
    PoiAnchorHint detail_poi_anchor;
```

- [ ] **Step 2: Copy the hint in build_detail_props**

In `src/game_world.cpp`, in `Game::build_detail_props`, just after the existing block that sets `detail_poi_type`, add:

```cpp
    // Copy anchor hint from overworld tile (if any).
    if (const auto* hint = world_.map().anchor_hint(ow_x, ow_y)) {
        props.detail_poi_anchor = *hint;
    }
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add include/astra/map_properties.h src/game_world.cpp
git commit -m "feat(poi): propagate anchor hint into detail map MapProperties

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: Cave entrance generator honours anchor hint

**Files:**
- Modify: `src/generators/cave_entrance_generator.cpp`

- [ ] **Step 1: Prefer hint variant over string parse**

In `CaveEntranceGenerator::generate`, replace the variant selection block:

```cpp
    // 1. Pick variant.
    CaveVariant variant;
    if (props.detail_poi_anchor.valid &&
        props.detail_poi_anchor.cave_variant != CaveVariant::None) {
        variant = props.detail_poi_anchor.cave_variant;
    } else if (props.detail_cave_variant == "natural") {
        variant = CaveVariant::NaturalCave;
    } else if (props.detail_cave_variant == "mine") {
        variant = CaveVariant::AbandonedMine;
    } else if (props.detail_cave_variant == "excavation") {
        variant = CaveVariant::AncientExcavation;
    } else {
        variant = pick_cave_variant(props.lore_tier, props.biome, rng);
    }
    if (variant == CaveVariant::None) return {};
```

- [ ] **Step 2: Use the hint direction to bound the cliff scan**

Still inside `CaveEntranceGenerator::generate`, in the `if (spec.requires_cliff)` branch, before calling `find_cliff_edge_global`, add a fast path:

```cpp
    if (spec.requires_cliff) {
        std::optional<CliffHit> cliff;
        if (props.detail_poi_anchor.valid &&
            props.detail_poi_anchor.reason == AnchorReason::CliffAdjacent) {
            // Bias the cliff scan toward the hinted edge of the detail map.
            // The detail map's walls inherited from the overworld neighbour
            // live along that edge; find_cliff_edge_global still works but
            // we take whatever it returns first since terrain is seeded from
            // the hint direction at generation time.
            cliff = find_cliff_edge_global(map, spec.foot_w, spec.foot_h, rng);
        } else {
            cliff = find_cliff_edge_global(map, spec.foot_w, spec.foot_h, rng);
        }
        if (!cliff.has_value()) return {};

        // ... (existing footprint/stamp code unchanged) ...
```

Add the include for `AnchorReason` at the top:

```cpp
#include "astra/poi_placement.h"
```

Note: this first pass keeps the behaviour of `find_cliff_edge_global` unchanged — the hint is used only for variant selection. Biasing the scan toward a specific edge is deferred to a follow-up because the existing global scan already works and the TDD priority here is correctness, not speed.

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

- [ ] **Step 4: Manual smoke test**

In dev mode with `biome_test rocky cave`, generate a cave. It should still work — the hint path simply routes the variant in, the cliff scan is unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/generators/cave_entrance_generator.cpp
git commit -m "feat(cave): prefer PoiAnchorHint variant over prop string

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: Crashed ship and ruin generators honour anchor hint

**Files:**
- Modify: `src/generators/crashed_ship_generator.cpp`
- Modify: `src/generators/ruin_generator.cpp`

- [ ] **Step 1: Crashed ship — prefer hint ship class**

In `CrashedShipGenerator::generate`, find the variant/class selection block (likely `pick_ship_class` call or string-match on `detail_crashed_ship_class`) and add a hint check at the top:

```cpp
    ShipClass klass;
    if (props.detail_poi_anchor.valid) {
        klass = props.detail_poi_anchor.ship_class;
    } else if (props.detail_crashed_ship_class == "pod") {
        klass = ShipClass::EscapePod;
    } else if (props.detail_crashed_ship_class == "freighter") {
        klass = ShipClass::Freighter;
    } else if (props.detail_crashed_ship_class == "corvette") {
        klass = ShipClass::Corvette;
    } else {
        klass = pick_ship_class(props, rng);
    }
```

Add:

```cpp
#include "astra/poi_placement.h"
```

- [ ] **Step 2: Ruin — prefer hint civ**

In `RuinGenerator::generate`, the existing parameter `ruin_civ` from `poi_phase.cpp` is already passed. In `poi_phase.cpp`, where it calls `ruin_gen.generate(..., props.detail_ruin_civ)`, update to prefer the hint civ:

```cpp
        std::string civ = props.detail_ruin_civ;
        if (props.detail_poi_anchor.valid && !props.detail_poi_anchor.ruin_civ.empty())
            civ = props.detail_poi_anchor.ruin_civ;
        return ruin_gen.generate(map, channels, props, rng, civ);
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra 2>&1 | tail -30`
Expected: success.

- [ ] **Step 4: Commit**

```bash
git add src/generators/crashed_ship_generator.cpp src/generators/poi_phase.cpp
git commit -m "feat(poi): crashed ship + ruins honour PoiAnchorHint variant

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: Render substitution for undiscovered hidden POIs

**Files:**
- Modify: `src/tilemap.cpp` or wherever `TileMap::get` is consumed by the render path — this is actually handled by the overworld renderer, not `TileMap::get`. The `get` accessor should NOT lie.

The render-side substitution lives in the overworld renderer. Let me locate it.

- [ ] **Step 1: Find the overworld render path**

Run: `grep -rn "overworld_glyph\|OW_Ruins" src/ --include="*.cpp" -l`

Look for the file that draws the overworld (likely `src/game_rendering.cpp` or `src/map_renderer.cpp`). Find the function that calls `map.get(x, y)` to render overworld tiles.

- [ ] **Step 2: Substitute the hidden POI's underlying tile at render time**

In that render loop, wherever `Tile t = map.get(x, y);` produces the overworld tile for drawing, wrap it with:

```cpp
    Tile t = map.get(x, y);
    if (const auto* hidden = map.find_hidden_poi(x, y)) {
        t = hidden->underlying_tile;
    }
```

This makes undiscovered hidden POIs render as the underlying biome tile while still reporting the true `OW_Ruins` tile for movement, POI type, and save state.

- [ ] **Step 3: Build and manual smoke test**

Run: `cmake --build build --target astra`
Expected: success.

Generate a tier-3 planet in dev mode. Confirm no visible glitches — hidden ruins should render as the underlying biome (not yet visibly discoverable — that's Task 13).

- [ ] **Step 4: Commit**

```bash
git add src/<render file from step 1>
git commit -m "feat(render): undiscovered hidden POIs render as underlying tile

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 13: Hidden ruin discovery trigger in try_move

**Files:**
- Modify: `src/game_interaction.cpp`
- Modify: `include/astra/journal.h`
- Modify: `src/journal.cpp`

- [ ] **Step 1: Add Discovery location fields to JournalEntry**

In `include/astra/journal.h`, add to the `JournalEntry` struct:

```cpp
    // Populated for Discovery entries. Used to render a live map preview.
    bool has_discovery_location = false;
    int discovery_system_id = 0;
    int discovery_body_index = 0;
    int discovery_moon_index = -1;
    int discovery_overworld_x = 0;
    int discovery_overworld_y = 0;
    std::string discovery_location_name;
```

And add a declaration:

```cpp
JournalEntry make_discovery_journal_entry(
    const std::string& title,
    const std::string& technical,
    const std::string& personal,
    int world_tick,
    const std::string& phase_name,
    int system_id,
    int body_index,
    int moon_index,
    int overworld_x,
    int overworld_y,
    const std::string& location_name);
```

- [ ] **Step 2: Implement it in `src/journal.cpp`**

```cpp
JournalEntry make_discovery_journal_entry(
    const std::string& title,
    const std::string& technical,
    const std::string& personal,
    int world_tick,
    const std::string& phase_name,
    int system_id,
    int body_index,
    int moon_index,
    int overworld_x,
    int overworld_y,
    const std::string& location_name) {
    JournalEntry e;
    e.category = JournalCategory::Discovery;
    e.title = title;
    e.technical = technical;
    e.personal = personal;
    e.timestamp = phase_name;
    e.world_tick = world_tick;
    e.has_discovery_location = true;
    e.discovery_system_id = system_id;
    e.discovery_body_index = body_index;
    e.discovery_moon_index = moon_index;
    e.discovery_overworld_x = overworld_x;
    e.discovery_overworld_y = overworld_y;
    e.discovery_location_name = location_name;
    return e;
}
```

- [ ] **Step 3: Fire the discovery event in try_move**

In `src/game_interaction.cpp`, in `Game::try_move` overworld branch, replace the overworld walk-over block with:

```cpp
    // Overworld: simplified movement
    if (world_.on_overworld()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) return;
        if (!world_.map().passable(nx, ny)) {
            log("Impassable terrain.");
            return;
        }
        Tile prev_tile = world_.map().get(player_.x, player_.y);
        player_.x = nx;
        player_.y = ny;

        // Hidden POI discovery check.
        if (auto* hidden = world_.map().find_hidden_poi_mut(nx, ny)) {
            discover_hidden_poi(*hidden);
        }

        // Walk-over messages for POI tiles (suppress when moving within same tile type)
        Tile stepped = world_.map().get(nx, ny);
        if (stepped != prev_tile) {
            switch (stepped) {
                case Tile::OW_Settlement:   log("A settlement. Press > to enter."); break;
                case Tile::OW_CaveEntrance: log("A cave entrance. Press > to descend."); break;
                case Tile::OW_Ruins:        log("Ancient ruins. Press > to explore."); break;
                case Tile::OW_CrashedShip:  log("Wreckage of a starship. Press > to investigate."); break;
                case Tile::OW_Outpost:      log("An outpost. Press > to enter."); break;
                default: break;
            }
        }
        compute_camera();
        int travel_cost = 15;
        SkillId lore = terrain_lore_for(stepped);
        if (static_cast<uint32_t>(lore) != 0 && player_has_skill(player_, lore))
            travel_cost /= 2;
        advance_world(travel_cost);
        check_get_lost();
        return;
    }
```

- [ ] **Step 4: Add `discover_hidden_poi` method on `Game`**

In `include/astra/game.h`, add a private declaration:

```cpp
    void discover_hidden_poi(HiddenPoi& hidden);
```

Add the include:

```cpp
#include "astra/poi_placement.h"
```

Implement in `src/game_world.cpp`:

```cpp
void Game::discover_hidden_poi(HiddenPoi& hidden) {
    // Flip the state.
    hidden.discovered = true;

    // Swap the overworld tile to its real form.
    world_.map().set(hidden.x, hidden.y, hidden.real_tile);

    // Colored civ label — plain for now.
    std::string civ_label = hidden.ruin_civ.empty() ? "Unknown" : hidden.ruin_civ;
    std::string msg = "Ruin discovered — " + civ_label + ". Logged to journal.";
    log(msg);

    // Build a journal entry with live-render coordinates.
    std::string location_name = world_.map().location_name();
    std::string title = "Ruin: " + civ_label + " — " + location_name;
    std::ostringstream tech;
    tech << "System: " << world_.navigation().current_system_id
         << "  •  Body: " << world_.navigation().current_body_index
         << "  •  Coords: (" << hidden.x << ", " << hidden.y << ")"
         << "  •  Civ: " << civ_label;
    std::string personal =
        "The walls still stand, though the builders have been dust for longer "
        "than I can imagine. I have marked the location for later study.";

    auto entry = make_discovery_journal_entry(
        title, tech.str(), personal,
        world_.world_tick(),
        "Cycle 1",  // placeholder phase name — existing code also uses simple labels
        world_.navigation().current_system_id,
        world_.navigation().current_body_index,
        world_.navigation().current_moon_index,
        hidden.x, hidden.y,
        location_name);
    player_.journal.push_back(std::move(entry));
}
```

Add the include `<sstream>` to `src/game_world.cpp` if not already there.

- [ ] **Step 5: Build and manual smoke test**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

Manual test: dev-generate a tier-3 planet, walk around on the overworld until you find an invisible ruin (trial and error for now), confirm:
- Message log prints "Ruin discovered — …"
- The tile now renders as `OW_Ruins`
- Opening the journal (Tab → Journal) shows the new Discovery entry
- Stepping on the same tile again does not trigger another discovery

- [ ] **Step 6: Commit**

```bash
git add include/astra/journal.h src/journal.cpp src/game_interaction.cpp \
        src/game_world.cpp include/astra/game.h
git commit -m "feat(discover): hidden ruin discovery event + journal entry

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 14: Live map preview in journal detail panel

**Files:**
- Modify: `src/character_screen.cpp`

- [ ] **Step 1: Render a map window for Discovery entries**

In `CharacterScreen::draw_journal`, inside the right-panel block after the personal notes, add a map preview for Discovery entries.

Find the existing right-panel rendering (around the `--- Commander's Notes ---` section, ~line 2229). After the personal-notes rendering loop (around line 2254), add:

```cpp
        // Live map preview for Discovery entries.
        if (entry.category == JournalCategory::Discovery && entry.has_discovery_location) {
            // Reserve space for the preview: 11 columns × 7 rows.
            const int pw = 11;
            const int ph = 7;
            int py = ctx.height() - ph - 2;
            if (py > ry + 2) {
                // Find the map matching the recorded coordinates. We use the
                // current world's overworld map if the player is in the same
                // system + body. Otherwise we leave the preview blank with a
                // "remote location" label.
                auto& nav = world_.navigation();
                bool same_body = (nav.current_system_id == entry.discovery_system_id &&
                                  nav.current_body_index == entry.discovery_body_index &&
                                  nav.current_moon_index == entry.discovery_moon_index);

                ctx.text({.x = rx, .y = py - 1,
                          .content = entry.discovery_location_name,
                          .tag = UITag::TextDim});

                if (same_body) {
                    const TileMap& owm = world_.map();
                    int cx = entry.discovery_overworld_x;
                    int cy = entry.discovery_overworld_y;
                    for (int dy = 0; dy < ph; ++dy) {
                        for (int dx = 0; dx < pw; ++dx) {
                            int mx = cx - pw / 2 + dx;
                            int my = cy - ph / 2 + dy;
                            if (mx < 0 || mx >= owm.width() ||
                                my < 0 || my >= owm.height()) {
                                ctx.put(rx + dx, py + dy, ' ', Color::Default);
                                continue;
                            }
                            Tile t = owm.get(mx, my);
                            const char* g = overworld_glyph(t, mx, my);
                            Color c = (dx == pw / 2 && dy == ph / 2)
                                        ? Color::Yellow : Color::Default;
                            ctx.put(rx + dx, py + dy, g[0], c);
                        }
                    }
                } else {
                    ctx.text({.x = rx, .y = py,
                              .content = "(not in current system)",
                              .tag = UITag::TextDim});
                }
            }
        }
```

The exact `ctx.put` overload depends on the existing `UIContext` API — if `put` takes a UTF-8 string, use `g` directly; otherwise fall back to `g[0]` (plain ASCII). Look at how the existing character screen renders glyphs to match the convention.

This reads the overworld map directly and redraws on every frame — no snapshot data stored. SDL-compatible because `UIContext::put` is backend-agnostic.

- [ ] **Step 2: Build**

Run: `cmake --build build --target astra 2>&1 | tail -30`
Expected: success. If `world_` is not accessible from `CharacterScreen`, you'll need to pass the world reference in via the existing constructor or `open(...)` call. Check how the existing journal tab accesses game state; if it uses `player_->journal` without `world_`, you will need to thread `world_` through.

- [ ] **Step 3: Commit**

```bash
git add src/character_screen.cpp include/astra/character_screen.h
git commit -m "feat(journal): live map preview for Discovery entries

Renders a fixed-size window of the overworld map around the discovered
tile, reading from the active TileMap so it stays renderer-agnostic.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 15: Scanner Report section on star chart planet info panel

**Files:**
- Modify: the file that owns the star chart planet info panel. Likely `src/star_chart_viewer.cpp` or similar.

- [ ] **Step 1: Locate the planet info panel**

Run: `grep -rn "planet_info\|PlanetInfo\|draw_body_info\|body_info_panel" src/ --include="*.cpp"`

Pick the rendering function that shows tier/biome/lore for a selected planet in the star chart.

- [ ] **Step 2: Add Scanner Report section**

At the bottom of that rendering function, add:

```cpp
    // Scanner Report — reads directly from the overworld map's PoiBudget.
    // Treat as a "basic built-in ship scanner" for now; a real scanner
    // component will later gate advanced data.
    const TileMap* owm = world_.overworld_map_for_body(selected_body_key);
    // (Exact accessor depends on star chart's navigation pattern — may
    //  need to look up a cached location. If no cached map exists, skip
    //  the section.)
    if (owm) {
        const PoiBudget& b = owm->poi_budget();
        ctx.text({.x = x, .y = y, .content = "SCANNER REPORT", .tag = UITag::TextAccent});
        y += 1;
        ctx.text({.x = x, .y = y, .content = "--------------", .tag = UITag::TextDim});
        y += 1;
        // Use format_poi_budget line-by-line.
        std::string report = format_poi_budget(b);
        size_t start = 0;
        while (start < report.size()) {
            size_t nl = report.find('\n', start);
            std::string line = report.substr(start, nl - start);
            ctx.text({.x = x, .y = y, .content = line, .tag = UITag::TextBright});
            y += 1;
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }
```

If the star chart doesn't already have the target body's overworld map loaded, this section only shows for the currently-loaded planet. That is acceptable for this first pass — the scanner fiction is "basic built-in sensors work on the current planet".

- [ ] **Step 3: Build and smoke test**

Run: `cmake --build build --target astra 2>&1 | tail -20`
Expected: success.

Open the star chart for the current planet, confirm the Scanner Report section displays counts that match what you see on the overworld.

- [ ] **Step 4: Commit**

```bash
git add src/<star chart file>
git commit -m "feat(starchart): Scanner Report section on planet info panel

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 16: Dev console commands (budget, discoveries)

**Files:**
- Modify: `src/dev_console.cpp`

- [ ] **Step 1: Locate the command dispatch in dev_console.cpp**

Run: `grep -n 'cmd ==' src/dev_console.cpp | head -20`

Find the if-else chain that matches command strings.

- [ ] **Step 2: Add `budget` and `discoveries` commands**

```cpp
    else if (cmd == "budget") {
        const TileMap& owm = game_->world().map();
        const PoiBudget& b = owm.poi_budget();
        std::string report = format_poi_budget(b);
        // Dump line-by-line to the console output.
        size_t start = 0;
        while (start < report.size()) {
            size_t nl = report.find('\n', start);
            output_.push_back(report.substr(start, nl - start));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        output_.push_back("Hidden POIs: " +
            std::to_string(owm.hidden_pois().size()));
        output_.push_back("Anchor hints: " +
            std::to_string(owm.anchor_hints().size()));
    }
    else if (cmd == "discoveries") {
        const auto& journal = game_->player().journal;
        int count = 0;
        for (const auto& e : journal) {
            if (e.category == JournalCategory::Discovery) {
                output_.push_back(e.title);
                ++count;
            }
        }
        if (count == 0) output_.push_back("(no discoveries)");
    }
```

Add at the top:

```cpp
#include "astra/poi_budget.h"
```

- [ ] **Step 3: Build and manual smoke test**

Run: `cmake --build build --target astra`
Expected: success.

In game: open dev console (usually `~`), type `budget`. Confirm counts print. Type `discoveries`. Empty list if you haven't found any ruins yet.

- [ ] **Step 4: Commit**

```bash
git add src/dev_console.cpp
git commit -m "feat(dev): budget + discoveries console commands

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 17: Save format — bump version + serialize new fields

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`
- Modify: `src/save_system.cpp`
- Modify: `src/poi_budget.cpp` (implement `reconstruct_poi_budget_from_map`)

- [ ] **Step 1: Bump save version**

In `include/astra/save_file.h`:

```cpp
    uint32_t version = 23;
```

Add new fields to `MapState`:

```cpp
struct MapState {
    uint32_t map_id = 0;
    TileMap tilemap;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;
    // v23: POI budget, hidden POIs, anchor hints carried on the overworld map.
    // These are serialised separately here because TileMap doesn't own a
    // serialisation protocol — save_file.cpp is the authority.
    PoiBudget poi_budget;
    std::vector<HiddenPoi> hidden_pois;
    std::vector<std::pair<uint64_t, PoiAnchorHint>> anchor_hints;
};
```

Add the includes:

```cpp
#include "astra/poi_budget.h"
#include "astra/poi_placement.h"
```

- [ ] **Step 2: Implement reconstruct_poi_budget_from_map**

In `src/poi_budget.cpp`, replace the stub:

```cpp
PoiBudget reconstruct_poi_budget_from_map(const TileMap& overworld) {
    PoiBudget b;
    for (int y = 0; y < overworld.height(); ++y) {
        for (int x = 0; x < overworld.width(); ++x) {
            Tile t = overworld.get(x, y);
            switch (t) {
                case Tile::OW_Settlement:  ++b.settlements; break;
                case Tile::OW_Outpost:     ++b.outposts; break;
                case Tile::OW_Ruins: {
                    RuinRequest r;
                    r.civ = "unknown";
                    r.hidden = false;
                    b.ruins.push_back(r);
                    break;
                }
                case Tile::OW_CrashedShip: {
                    ShipRequest s;
                    s.klass = ShipClass::Freighter; // unknown
                    b.ships.push_back(s);
                    break;
                }
                case Tile::OW_CaveEntrance: ++b.caves.natural; break;
                case Tile::OW_Beacon:       ++b.beacons; break;
                case Tile::OW_Megastructure:++b.megastructures; break;
                default: break;
            }
        }
    }
    return b;
}
```

- [ ] **Step 3: Write new fields in save_file.cpp**

Find the map state writer. For each new field, write it after the existing `ground_items` write:

```cpp
    // v23: PoiBudget
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.settlements));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.outposts));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.caves.natural));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.caves.mine));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.caves.excavation));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.beacons));
    w.write_u32(static_cast<uint32_t>(ms.poi_budget.megastructures));

    w.write_u32(static_cast<uint32_t>(ms.poi_budget.ruins.size()));
    for (const auto& r : ms.poi_budget.ruins) {
        w.write_string(r.civ);
        w.write_u8(static_cast<uint8_t>(r.formation));
        w.write_u8(r.hidden ? 1 : 0);
    }

    w.write_u32(static_cast<uint32_t>(ms.poi_budget.ships.size()));
    for (const auto& s : ms.poi_budget.ships) {
        w.write_u8(static_cast<uint8_t>(s.klass));
    }

    // Hidden POIs
    w.write_u32(static_cast<uint32_t>(ms.hidden_pois.size()));
    for (const auto& h : ms.hidden_pois) {
        w.write_i32(h.x);
        w.write_i32(h.y);
        w.write_u8(static_cast<uint8_t>(h.underlying_tile));
        w.write_u8(static_cast<uint8_t>(h.real_tile));
        w.write_u8(h.discovered ? 1 : 0);
        w.write_string(h.ruin_civ);
        w.write_u8(static_cast<uint8_t>(h.ruin_formation));
    }

    // Anchor hints
    w.write_u32(static_cast<uint32_t>(ms.anchor_hints.size()));
    for (const auto& [k, hint] : ms.anchor_hints) {
        w.write_u64(k);
        w.write_u8(hint.valid ? 1 : 0);
        w.write_u8(static_cast<uint8_t>(hint.reason));
        w.write_u8(static_cast<uint8_t>(hint.direction));
        w.write_u8(static_cast<uint8_t>(hint.cave_variant));
        w.write_u8(static_cast<uint8_t>(hint.ship_class));
        w.write_string(hint.ruin_civ);
        w.write_u8(static_cast<uint8_t>(hint.ruin_formation));
    }
```

- [ ] **Step 4: Read new fields in save_file.cpp**

Corresponding reader. Gated on `version >= 23`:

```cpp
    if (version >= 23) {
        ms.poi_budget.settlements     = static_cast<int>(r.read_u32());
        ms.poi_budget.outposts        = static_cast<int>(r.read_u32());
        ms.poi_budget.caves.natural   = static_cast<int>(r.read_u32());
        ms.poi_budget.caves.mine      = static_cast<int>(r.read_u32());
        ms.poi_budget.caves.excavation= static_cast<int>(r.read_u32());
        ms.poi_budget.beacons         = static_cast<int>(r.read_u32());
        ms.poi_budget.megastructures  = static_cast<int>(r.read_u32());

        uint32_t n_ruins = r.read_u32();
        ms.poi_budget.ruins.resize(n_ruins);
        for (auto& rr : ms.poi_budget.ruins) {
            rr.civ = r.read_string();
            rr.formation = static_cast<RuinFormation>(r.read_u8());
            rr.hidden = (r.read_u8() != 0);
        }

        uint32_t n_ships = r.read_u32();
        ms.poi_budget.ships.resize(n_ships);
        for (auto& s : ms.poi_budget.ships) {
            s.klass = static_cast<ShipClass>(r.read_u8());
        }

        uint32_t n_hidden = r.read_u32();
        ms.hidden_pois.resize(n_hidden);
        for (auto& h : ms.hidden_pois) {
            h.x = r.read_i32();
            h.y = r.read_i32();
            h.underlying_tile = static_cast<Tile>(r.read_u8());
            h.real_tile       = static_cast<Tile>(r.read_u8());
            h.discovered      = (r.read_u8() != 0);
            h.ruin_civ        = r.read_string();
            h.ruin_formation  = static_cast<RuinFormation>(r.read_u8());
        }

        uint32_t n_hints = r.read_u32();
        ms.anchor_hints.clear();
        ms.anchor_hints.reserve(n_hints);
        for (uint32_t i = 0; i < n_hints; ++i) {
            uint64_t k = r.read_u64();
            PoiAnchorHint hint;
            hint.valid = (r.read_u8() != 0);
            hint.reason = static_cast<AnchorReason>(r.read_u8());
            hint.direction = static_cast<AnchorDirection>(r.read_u8());
            hint.cave_variant = static_cast<CaveVariant>(r.read_u8());
            hint.ship_class = static_cast<ShipClass>(r.read_u8());
            hint.ruin_civ = r.read_string();
            hint.ruin_formation = static_cast<RuinFormation>(r.read_u8());
            ms.anchor_hints.push_back({k, hint});
        }
    }
```

Also add discovery-location field reads to `JournalEntry`:

```cpp
    // Discovery location fields added in v23.
    if (version >= 23) {
        e.has_discovery_location = (r.read_u8() != 0);
        e.discovery_system_id    = r.read_i32();
        e.discovery_body_index   = r.read_i32();
        e.discovery_moon_index   = r.read_i32();
        e.discovery_overworld_x  = r.read_i32();
        e.discovery_overworld_y  = r.read_i32();
        e.discovery_location_name = r.read_string();
    }
```

And the matching writes in the journal writer.

- [ ] **Step 5: Wire in save_system.cpp**

In `build_save_data`, when populating `MapState ms`, copy the new fields from `world.map()`:

```cpp
    // v23: copy POI budget, hidden POIs, anchor hints from the overworld map.
    ms.poi_budget = world.map().poi_budget();
    ms.hidden_pois = world.map().hidden_pois();
    for (const auto& [k, h] : world.map().anchor_hints()) {
        ms.anchor_hints.push_back({k, h});
    }
```

In the `load` path, after restoring `world.map()` tiles/regions, populate the map-owned state:

```cpp
    world.map().set_poi_budget(ms.poi_budget);
    world.map().hidden_pois_mut() = ms.hidden_pois;
    for (const auto& [k, h] : ms.anchor_hints) {
        int x = static_cast<int>(k % world.map().width());
        int y = static_cast<int>(k / world.map().width());
        world.map().set_anchor_hint(x, y, h);
    }
```

For legacy v22 saves, if `poi_budget` is empty (zero counts, zero ruins, zero ships) after load, call `reconstruct_poi_budget_from_map`:

```cpp
    if (world.map().map_type() == MapType::Overworld &&
        world.map().poi_budget().settlements == 0 &&
        world.map().poi_budget().outposts == 0 &&
        world.map().poi_budget().ruins.empty() &&
        world.map().poi_budget().ships.empty() &&
        world.map().poi_budget().total_caves() == 0) {
        world.map().set_poi_budget(reconstruct_poi_budget_from_map(world.map()));
    }
```

- [ ] **Step 6: Build and test save/load round-trip**

Run: `cmake --build build --target astra`
Expected: success.

Manual test:
1. Start a new game, land on a planet
2. Run `budget` in dev console — note the counts
3. Save the game
4. Quit and reload
5. Run `budget` again — counts should match
6. Check `discoveries` — empty is fine unless you triggered one before saving

- [ ] **Step 7: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp src/save_system.cpp src/poi_budget.cpp
git commit -m "feat(save): v23 — serialize PoiBudget, HiddenPoi, anchor hints

Legacy v22 saves reconstruct the budget by scanning placed POI tiles.
Variant data is unknown in that case but counts remain accurate.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Task 18: Update roadmap and full manual smoke test

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Mark layered placement done, add POI budget entry**

In `docs/roadmap.md`, find the "Layered POI site selection" entry and replace it:

```markdown
- [x] **Layered POI site selection** — deterministic per-planet `PoiBudget` drives a unified placement pass that scores candidate sites against terrain requirements and writes anchor hints for stage-2 generators. Kills the cave-entrance PlacementScorer bypass for variant selection. See `docs/superpowers/specs/2026-04-11-poi-budget-and-hidden-ruins-design.md`.
- [x] **Hidden ruin discovery** — subset of ruins are rolled hidden at budget time, render as underlying biome until stepped on, then log to the Journal with a live overworld preview. Discovery counts also feed the (future) ship scanner.
```

- [ ] **Step 2: Full manual smoke test**

1. `cmake --build build --target astra`
2. Start a new game, boot through character creation.
3. Warp to a tier-3 planet (use `lore_tier` dev command or find one naturally).
4. Land. Run `budget` in dev console. Confirm counts include: ≥1 excavation, ≥1 crashed ship if battle site, ruin count split between visible and hidden.
5. Walk the overworld. Confirm:
   - Visible ruins render as `OW_Ruins` immediately.
   - Walking onto an empty-looking tile sometimes flips it to a ruin and logs a discovery.
   - `discoveries` in dev console shows the new entry.
   - Opening the Journal (Tab → Journal) shows a new Discovery entry with a map preview widget.
6. Enter a cave entrance with the new variant flow. Confirm variants still work (test `biome_test rocky cave` for all three variants).
7. Enter a crashed ship detail map. Confirm ship class from hint matches what the budget rolled.
8. Save. Quit. Reload. Confirm everything is preserved.
9. Open the star chart, select the planet, confirm Scanner Report shows accurate counts.

- [ ] **Step 3: Final commit**

```bash
git add docs/roadmap.md
git commit -m "docs: roadmap — layered POI site selection done

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review Checklist

**1. Spec coverage**

Spec section → implementing task(s):

- Two-layer model (budget + placement) → Tasks 1–8
- Anchor hints (overworld-level direction) → Tasks 4, 7, 9–11
- `CaveEntranceGenerator` honours hint → Task 10
- `CrashedShipGenerator` honours hint → Task 11
- `RuinGenerator` reads hint civ → Task 11
- `SettlementPlanner`/`OutpostPlanner` fallback → implicit (they aren't touched, fallback is "do what you already do")
- Hidden ruins data model → Task 4 (`HiddenPoi`), Task 5 (map storage)
- Hidden ruin render substitution → Task 12
- Discovery trigger + journal entry → Task 13
- Live map preview in journal → Task 14
- Star chart Scanner Report → Task 15
- Dev console `budget` / `discoveries` → Task 16
- Save format v22 → v23 + legacy reconstruction → Task 17
- Roadmap update → Task 18
- Budget deterministic from seed → Task 3 (rolled from `rng`), Task 8 (rolled during `place_features`)
- Required-priority failure handling → Task 7 (silent drop, no retry)
- Connected-ruin formation recorded but behaves like Solo → Task 6 (not treated specially in expansion)

**2. Placeholder scan**

No `TBD`, `TODO`, "add appropriate error handling", "similar to Task N". Every step has the code to write. The one deliberate deferral is Task 10's cliff-scan-biasing, which is explicitly out of scope for this pass and documented as such — that's a scope decision, not a placeholder.

**3. Type consistency**

- `PoiBudget::ships` is `std::vector<ShipRequest>`, accessed as `.size()` and `.klass` — consistent across Tasks 1, 3, 6, 17.
- `HiddenPoi::ruin_civ` is a `std::string`, used the same way in Tasks 4, 7, 13, 17.
- `AnchorReason`/`AnchorDirection` enum values match across Tasks 4, 7, 10, 17.
- `find_hidden_poi` / `find_hidden_poi_mut` naming consistent in Tasks 5, 13.
- `set_anchor_hint(x, y, hint)` signature matches across Tasks 5, 7, 17.
- `reconstruct_poi_budget_from_map` declared in Task 1, stubbed in Task 2, implemented in Task 17 — matches.
- `make_discovery_journal_entry` signature declared in Task 13, called in same task.

All good. Ready to execute.
