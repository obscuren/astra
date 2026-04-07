# Settlement POI System (Phase 6) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add terrain-aware settlement generation to detail map v2 as a composable POI phase.

**Architecture:** Layered components — PlacementScorer finds location, SettlementPlanner builds blueprint, then BuildingGenerator/PathRouter/PerimeterBuilder/ExteriorDecorator execute it onto the TileMap. Data flows through a `SettlementPlan` struct; nothing writes to the TileMap until the plan is complete.

**Tech Stack:** C++20, built with `cmake -B build -DDEV=ON && cmake --build build`

**Spec:** `docs/superpowers/specs/2026-04-07-settlement-poi-design.md`

---

## File Structure

```
include/astra/
  settlement_types.h     — All POI settlement types: CivStyle, BuildingType, Anchor, BuildingSpec,
                            BridgeSpec, PerimeterSpec, TerrainMod, FurnitureEntry, FurniturePalette,
                            SettlementPlan, PlacementResult
  placement_scorer.h     — PlacementScorer class declaration
  settlement_planner.h   — SettlementPlanner class declaration
  building_generator.h   — BuildingGenerator class declaration
  path_router.h          — PathRouter class declaration
  perimeter_builder.h    — PerimeterBuilder class declaration
  exterior_decorator.h   — ExteriorDecorator class declaration

src/generators/
  civ_styles.cpp         — frontier(), advanced(), ruined() style constructors
  furniture_palettes.cpp — palettes per BuildingType, role→fixture resolution
  placement_scorer.cpp   — region scoring + anchor discovery
  settlement_planner.cpp — blueprint assembly + terrain sculpting decisions
  building_generator.cpp — shape gen, walls, doors, windows, furnishing
  path_router.cpp        — L-shaped path routing + bridge generation
  perimeter_builder.cpp  — optional walled enclosure
  exterior_decorator.cpp — lamps, benches, scatter clearing
  poi_phase.cpp          — orchestrator called from v2 generator

Modify:
  include/astra/tilemap.h              — new FixtureTypes, remove_fixture()
  src/tilemap.cpp                      — make_fixture() cases, remove_fixture() impl
  src/generators/detail_map_generator_v2.cpp — call poi_phase from place_features()
  src/dev_console.cpp                  — biome_test settlement flag
  src/game.cpp                         — dev_command_biome_test settlement param
  include/astra/game.h                 — dev_command_biome_test signature
  CMakeLists.txt                       — add new .cpp files
```

---

### Task 1: New FixtureTypes + remove_fixture

Add the fixture types needed for settlements and a way to clear fixtures from tiles.

**Files:**
- Modify: `include/astra/tilemap.h:298-335` (FixtureType enum)
- Modify: `include/astra/tilemap.h:388-496` (TileMap class — add remove_fixture)
- Modify: `src/tilemap.cpp:298-368` (make_fixture switch)

- [ ] **Step 1: Add new FixtureType entries**

In `include/astra/tilemap.h`, add after `SettlementProp` in the `FixtureType` enum:

```cpp
    // Settlement furniture (Phase 6)
    CampStove,          // 'o'  — frontier cooking
    Lamp,               // '*'  — frontier/advanced lighting
    HoloLight,          // '*'  — advanced lighting (blue tint)
    Locker,             // '='  — advanced storage
    BookCabinet,        // '['  — frontier knowledge storage
    DataTerminal,       // '#'  — advanced knowledge terminal
    Bench,              // '='  — outdoor/indoor seating
    Chair,              // 'h'  — advanced seating
    Gate,               // '/'  — perimeter gate (passable)
    BridgeRail,         // '|'  — bridge railing (impassable)
    BridgeFloor,        // '.'  — bridge surface (passable)
    Planter,            // '"'  — decorative vegetation
```

- [ ] **Step 2: Add remove_fixture to TileMap**

In `include/astra/tilemap.h`, add to the TileMap public interface (after `add_fixture`):

```cpp
    void remove_fixture(int x, int y);
```

In `src/tilemap.cpp`, add the implementation (after the `add_fixture` method):

```cpp
void TileMap::remove_fixture(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int idx = y * width_ + x;
    fixture_ids_[idx] = -1;
    // Note: the FixtureData stays in fixtures_ vector (IDs are stable indices).
    // This is fine — orphaned entries are harmless and avoid index invalidation.
}
```

- [ ] **Step 3: Add make_fixture cases for new types**

In `src/tilemap.cpp`, add cases in `make_fixture()` before the closing `}`:

```cpp
        case FixtureType::CampStove:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Lamp:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 6; break;
        case FixtureType::HoloLight:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8; break;
        case FixtureType::Locker:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::BookCabinet:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::DataTerminal:
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 2; break;
        case FixtureType::Bench:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Chair:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Gate:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::BridgeRail:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::BridgeFloor:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::Planter:
            fd.passable = false; fd.interactable = false; break;
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile with no errors or warnings about unhandled enum cases.

- [ ] **Step 5: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp
git commit -m "feat: add settlement fixture types and remove_fixture()"
```

---

### Task 2: Settlement Types Header

All data types for the settlement system in a single header.

**Files:**
- Create: `include/astra/settlement_types.h`

- [ ] **Step 1: Create the types header**

```cpp
#pragma once

#include "astra/tilemap.h"
#include "astra/map_properties.h"

#include <optional>
#include <string>
#include <vector>

namespace astra {

// ── Building types ──

enum class BuildingType : uint8_t {
    MainHall,
    Market,
    Dwelling,
    Distillery,
    Lookout,
    Workshop,
    Storage,
};

// ── Anchor types ──

enum class AnchorType : uint8_t {
    Center,       // flat clearing — main hall, plaza
    Waterfront,   // water edge — distillery, dock
    Elevated,     // high ground — lookout
};

struct Anchor {
    int x = 0;
    int y = 0;
    AnchorType type = AnchorType::Center;
};

// ── Civilization style ──

struct CivStyle {
    std::string name;

    // Tile types for structural elements
    Tile wall_tile    = Tile::StructuralWall;
    Tile floor_tile   = Tile::IndoorFloor;
    Tile path_tile    = Tile::Floor;

    // Fixture types per role
    FixtureType lighting  = FixtureType::Torch;
    FixtureType storage   = FixtureType::Crate;
    FixtureType seating   = FixtureType::Bench;
    FixtureType cooking   = FixtureType::CampStove;
    FixtureType knowledge = FixtureType::BookCabinet;
    FixtureType display   = FixtureType::Rack;

    // Perimeter
    Tile perimeter_wall = Tile::Wall;
    FixtureType gate    = FixtureType::Gate;

    // Bridge
    FixtureType bridge_rail  = FixtureType::BridgeRail;
    FixtureType bridge_floor = FixtureType::BridgeFloor;

    // Ruined style: probability that a wall/perimeter tile is missing
    float decay = 0.0f;  // 0.0 = pristine, 0.3-0.4 = ruined
};

// Style constructors (defined in civ_styles.cpp)
CivStyle civ_frontier();
CivStyle civ_advanced();
CivStyle civ_ruined();
CivStyle select_civ_style(const MapProperties& props);

// ── Furniture palette ──

struct FurnitureEntry {
    FixtureType type;
    float frequency    = 0.5f;   // probability of appearing
    bool wall_adjacent = false;  // must be placed against a wall
    bool needs_clearance = false; // needs open tile in front
    bool prefers_corner  = false; // prefers corner placement
};

struct FurniturePalette {
    std::vector<FurnitureEntry> entries;
};

// Returns a palette for the given building type, resolved through civ style
FurniturePalette furniture_palette(BuildingType type, const CivStyle& style);

// ── Building shape ──

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct BuildingShape {
    Rect primary;                        // main rectangle
    std::vector<Rect> extensions;        // wings, alcoves, porches
    int door_x = 0, door_y = 0;         // primary door position
    int door2_x = -1, door2_y = -1;     // secondary door (-1 = none)
};

// ── Building spec (part of the plan) ──

struct BuildingSpec {
    BuildingType type = BuildingType::Dwelling;
    BuildingShape shape;
    AnchorType anchor = AnchorType::Center;
};

// ── Bridge spec ──

struct BridgeSpec {
    int start_x = 0, start_y = 0;
    int end_x = 0, end_y = 0;
    int width = 1;  // 1 or 2 tiles wide
};

// ── Path spec ──

struct PathSpec {
    int from_x = 0, from_y = 0;
    int to_x = 0, to_y = 0;
    int width = 1;  // 1 or 2
};

// ── Perimeter spec ──

struct PerimeterSpec {
    Rect bounds;
    std::vector<std::pair<int,int>> gate_positions;
};

// ── Terrain modification ──

enum class TerrainModType : uint8_t {
    Level,      // flatten elevation in rect
    RaiseBluff, // increase elevation + cliff face
    CutBank,    // flatten near water
    Clear,      // remove structure masks in rect
};

struct TerrainMod {
    TerrainModType type;
    Rect area;
    float target_elevation = 0.5f;  // for Level/RaiseBluff
};

// ── Placement result ──

struct PlacementResult {
    Rect footprint;                  // bounding region for the settlement
    std::vector<Anchor> anchors;     // terrain feature anchors
    bool valid = false;              // false if no suitable location found
};

// ── Settlement plan (the full blueprint) ──

struct SettlementPlan {
    PlacementResult placement;
    CivStyle style;
    std::vector<BuildingSpec> buildings;
    std::vector<PathSpec> paths;
    std::vector<BridgeSpec> bridges;
    std::optional<PerimeterSpec> perimeter;
    std::vector<TerrainMod> terrain_mods;
    int size_category = 0;  // 0=small, 1=medium, 2=large
};

} // namespace astra
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. Header is include-only, no new .cpp yet.

- [ ] **Step 3: Commit**

```bash
git add include/astra/settlement_types.h
git commit -m "feat: add settlement type definitions header"
```

---

### Task 3: CivStyle Definitions

Three initial civilization styles: frontier, advanced, ruined.

**Files:**
- Create: `src/generators/civ_styles.cpp`
- Modify: `CMakeLists.txt` — add to sources

- [ ] **Step 1: Create civ_styles.cpp**

```cpp
#include "astra/settlement_types.h"

namespace astra {

CivStyle civ_frontier() {
    CivStyle s;
    s.name = "Frontier";
    s.wall_tile    = Tile::StructuralWall;
    s.floor_tile   = Tile::IndoorFloor;
    s.path_tile    = Tile::Floor;
    s.lighting     = FixtureType::Torch;
    s.storage      = FixtureType::Crate;
    s.seating      = FixtureType::Bench;
    s.cooking      = FixtureType::CampStove;
    s.knowledge    = FixtureType::BookCabinet;
    s.display      = FixtureType::Rack;
    s.perimeter_wall = Tile::Wall;
    s.gate         = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay        = 0.0f;
    return s;
}

CivStyle civ_advanced() {
    CivStyle s;
    s.name = "Advanced";
    s.wall_tile    = Tile::StructuralWall;
    s.floor_tile   = Tile::IndoorFloor;
    s.path_tile    = Tile::Floor;
    s.lighting     = FixtureType::HoloLight;
    s.storage      = FixtureType::Locker;
    s.seating      = FixtureType::Chair;
    s.cooking      = FixtureType::FoodTerminal;
    s.knowledge    = FixtureType::DataTerminal;
    s.display      = FixtureType::WeaponDisplay;
    s.perimeter_wall = Tile::StructuralWall;
    s.gate         = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay        = 0.0f;
    return s;
}

CivStyle civ_ruined() {
    CivStyle s;
    s.name = "Ruined";
    s.wall_tile    = Tile::Wall;
    s.floor_tile   = Tile::Floor;
    s.path_tile    = Tile::Floor;
    s.lighting     = FixtureType::Torch;  // mostly broken, placed sparsely
    s.storage      = FixtureType::Crate;
    s.seating      = FixtureType::Debris;
    s.cooking      = FixtureType::Debris;
    s.knowledge    = FixtureType::Debris;
    s.display      = FixtureType::Debris;
    s.perimeter_wall = Tile::Wall;
    s.gate         = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay        = 0.35f;
    return s;
}

CivStyle select_civ_style(const MapProperties& props) {
    if (props.lore_plague_origin) return civ_ruined();
    if (props.lore_alien_strength > 0.3f) return civ_advanced();
    if (props.lore_tier >= 2) return civ_advanced();
    return civ_frontier();
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/civ_styles.cpp` to the source list in CMakeLists.txt.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add src/generators/civ_styles.cpp CMakeLists.txt
git commit -m "feat: add three civilization style definitions"
```

---

### Task 4: Furniture Palettes

Data-driven furniture palettes per building type, resolved through CivStyle.

**Files:**
- Create: `src/generators/furniture_palettes.cpp`
- Modify: `CMakeLists.txt` — add to sources

- [ ] **Step 1: Create furniture_palettes.cpp**

```cpp
#include "astra/settlement_types.h"

namespace astra {

// Helper: create entry using the style's fixture for a role
static FurnitureEntry entry(FixtureType type, float freq,
                            bool wall = false, bool clearance = false,
                            bool corner = false) {
    return {type, freq, wall, clearance, corner};
}

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style) {
    FurniturePalette p;

    switch (type) {
        case BuildingType::Dwelling:
            p.entries = {
                entry(style.cooking,   0.8f, true, true),
                entry(style.seating,   0.7f, false, false),
                entry(style.storage,   0.6f, true, false, true),
                entry(style.knowledge, 0.3f, true, false),
            };
            break;

        case BuildingType::Market:
            p.entries = {
                entry(style.display,   0.9f, true, true),
                entry(style.storage,   0.8f, true, false, true),
                entry(style.seating,   0.3f, false, false),
            };
            break;

        case BuildingType::MainHall:
            p.entries = {
                entry(FixtureType::Table,   0.9f, false, false),
                entry(style.seating,        0.8f, false, false),
                entry(style.knowledge,      0.5f, true, true),
                entry(style.display,        0.4f, true, true),
                entry(FixtureType::Console, 0.6f, true, true),
            };
            break;

        case BuildingType::Distillery:
            p.entries = {
                entry(FixtureType::Conduit, 0.9f, true, false),
                entry(style.storage,        0.8f, true, false, true),
                entry(style.cooking,        0.4f, true, true),
            };
            break;

        case BuildingType::Lookout:
            p.entries = {
                entry(style.knowledge, 0.6f, true, true),
                entry(style.seating,   0.5f, false, false),
            };
            break;

        case BuildingType::Workshop:
            p.entries = {
                entry(FixtureType::Table,   0.9f, false, false),
                entry(style.storage,        0.7f, true, false, true),
                entry(FixtureType::Conduit, 0.5f, true, false),
                entry(style.display,        0.4f, true, true),
            };
            break;

        case BuildingType::Storage:
            p.entries = {
                entry(style.storage,         0.9f, true, false, true),
                entry(style.storage,         0.7f, true, false),
                entry(FixtureType::Shelf,    0.5f, true, false),
            };
            break;
    }

    return p;
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/furniture_palettes.cpp` to the source list.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add src/generators/furniture_palettes.cpp CMakeLists.txt
git commit -m "feat: add data-driven furniture palettes per building type"
```

---

### Task 5: PlacementScorer

Finds the best settlement location on the 360×150 map using terrain channel data.

**Files:**
- Create: `include/astra/placement_scorer.h`
- Create: `src/generators/placement_scorer.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"

namespace astra {

class PlacementScorer {
public:
    // Scan the map for the best settlement location.
    // chunk_size controls the scanning resolution (e.g. 10 = score every 10th tile).
    // footprint_w/h is the desired settlement footprint.
    PlacementResult score(const TerrainChannels& channels,
                          const TileMap& map,
                          int footprint_w, int footprint_h,
                          int edge_margin = 15) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/placement_scorer.h"

#include <cmath>
#include <limits>

namespace astra {

// ── Helpers ──

// Compute elevation variance in a rectangular region
static float elevation_variance(const TerrainChannels& ch,
                                int rx, int ry, int rw, int rh) {
    float sum = 0.0f;
    float sum_sq = 0.0f;
    int count = 0;
    for (int y = ry; y < ry + rh && y < ch.height; ++y) {
        for (int x = rx; x < rx + rw && x < ch.width; ++x) {
            float e = ch.elev(x, y);
            sum += e;
            sum_sq += e * e;
            ++count;
        }
    }
    if (count == 0) return 1.0f;
    float mean = sum / count;
    return (sum_sq / count) - (mean * mean);
}

// Count water tiles adjacent to (but not inside) a region
static int water_proximity(const TileMap& map, int rx, int ry, int rw, int rh,
                           int scan_margin = 8) {
    int count = 0;
    int x0 = std::max(0, rx - scan_margin);
    int y0 = std::max(0, ry - scan_margin);
    int x1 = std::min(map.width(), rx + rw + scan_margin);
    int y1 = std::min(map.height(), ry + rh + scan_margin);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (map.get(x, y) == Tile::Water) ++count;
        }
    }
    return count;
}

// Check if region overlaps structure walls (cliffs, craters)
static int structure_wall_count(const TerrainChannels& ch,
                                int rx, int ry, int rw, int rh) {
    int count = 0;
    for (int y = ry; y < ry + rh && y < ch.height; ++y) {
        for (int x = rx; x < rx + rw && x < ch.width; ++x) {
            if (ch.struc(x, y) == StructureMask::Wall) ++count;
        }
    }
    return count;
}

// Find the highest elevation point in a region (for elevated anchor)
static std::pair<int,int> highest_point(const TerrainChannels& ch,
                                         int rx, int ry, int rw, int rh) {
    float best = -1.0f;
    int bx = rx, by = ry;
    for (int y = ry; y < ry + rh && y < ch.height; ++y) {
        for (int x = rx; x < rx + rw && x < ch.width; ++x) {
            float e = ch.elev(x, y);
            if (e > best) { best = e; bx = x; by = y; }
        }
    }
    return {bx, by};
}

// Find water edge point closest to region center
static std::pair<int,int> nearest_water_edge(const TileMap& map,
                                              int cx, int cy,
                                              int rx, int ry, int rw, int rh,
                                              int scan = 12) {
    int best_dist = std::numeric_limits<int>::max();
    int bx = -1, by = -1;
    int x0 = std::max(0, rx - scan);
    int y0 = std::max(0, ry - scan);
    int x1 = std::min(map.width(), rx + rw + scan);
    int y1 = std::min(map.height(), ry + rh + scan);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (map.get(x, y) != Tile::Water) continue;
            // Check if adjacent to non-water (i.e. an edge)
            bool edge = false;
            for (auto [dx, dy] : {std::pair{1,0},{-1,0},{0,1},{0,-1}}) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < map.width() && ny >= 0 && ny < map.height()
                    && map.get(nx, ny) != Tile::Water) {
                    edge = true;
                    break;
                }
            }
            if (!edge) continue;
            int dist = std::abs(x - cx) + std::abs(y - cy);
            if (dist < best_dist) { best_dist = dist; bx = x; by = y; }
        }
    }
    return {bx, by};
}

// ── Main scoring ──

PlacementResult PlacementScorer::score(const TerrainChannels& channels,
                                        const TileMap& map,
                                        int footprint_w, int footprint_h,
                                        int edge_margin) const {
    PlacementResult result;
    result.valid = false;

    const int w = channels.width;
    const int h = channels.height;
    const int step = 5;  // scan every 5th position

    float best_score = -std::numeric_limits<float>::max();
    int best_x = -1, best_y = -1;

    for (int ry = edge_margin; ry + footprint_h < h - edge_margin; ry += step) {
        for (int rx = edge_margin; rx + footprint_w < w - edge_margin; rx += step) {
            // Hard constraint: too many structure walls
            int walls = structure_wall_count(channels, rx, ry, footprint_w, footprint_h);
            float wall_ratio = static_cast<float>(walls) / (footprint_w * footprint_h);
            if (wall_ratio > 0.15f) continue;

            // Flatness score (lower variance = better)
            float variance = elevation_variance(channels, rx, ry, footprint_w, footprint_h);
            float flat_score = 1.0f - std::min(variance * 20.0f, 1.0f);

            // Water proximity bonus
            int water = water_proximity(map, rx, ry, footprint_w, footprint_h);
            float water_score = std::min(static_cast<float>(water) / 100.0f, 0.3f);

            // Penalty for being too close to edges (soft, beyond the hard margin)
            float cx = rx + footprint_w / 2.0f;
            float cy = ry + footprint_h / 2.0f;
            float edge_dist = std::min({cx, cy, w - cx, h - cy});
            float center_bonus = std::min(edge_dist / 60.0f, 0.2f);

            float total = flat_score * 3.0f + water_score + center_bonus - wall_ratio * 2.0f;

            if (total > best_score) {
                best_score = total;
                best_x = rx;
                best_y = ry;
            }
        }
    }

    if (best_x < 0) return result;

    result.valid = true;
    result.footprint = {best_x, best_y, footprint_w, footprint_h};

    // ── Discover anchors ──

    int center_x = best_x + footprint_w / 2;
    int center_y = best_y + footprint_h / 2;

    // Always have a center anchor
    result.anchors.push_back({center_x, center_y, AnchorType::Center});

    // Waterfront anchor (if water is nearby)
    auto [wx, wy] = nearest_water_edge(map, center_x, center_y,
                                        best_x, best_y, footprint_w, footprint_h);
    if (wx >= 0) {
        // Place anchor on land side of water edge
        for (auto [dx, dy] : {std::pair{1,0},{-1,0},{0,1},{0,-1}}) {
            int lx = wx + dx, ly = wy + dy;
            if (lx >= best_x && lx < best_x + footprint_w
                && ly >= best_y && ly < best_y + footprint_h
                && map.get(lx, ly) != Tile::Water) {
                result.anchors.push_back({lx, ly, AnchorType::Waterfront});
                break;
            }
        }
    }

    // Elevated anchor (if significant elevation difference exists)
    auto [ex, ey] = highest_point(channels, best_x, best_y, footprint_w, footprint_h);
    float high_elev = channels.elev(ex, ey);
    float center_elev = channels.elev(center_x, center_y);
    if (high_elev - center_elev > 0.1f) {
        result.anchors.push_back({ex, ey, AnchorType::Elevated});
    }

    return result;
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/placement_scorer.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/placement_scorer.h src/generators/placement_scorer.cpp CMakeLists.txt
git commit -m "feat: add settlement placement scorer with terrain-aware scoring"
```

---

### Task 6: SettlementPlanner

The brain — assembles the full blueprint from anchors and context.

**Files:**
- Create: `include/astra/settlement_planner.h`
- Create: `src/generators/settlement_planner.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"

#include <random>

namespace astra {

class SettlementPlanner {
public:
    // Build a complete settlement plan from placement result + context.
    SettlementPlan plan(const PlacementResult& placement,
                        const TerrainChannels& channels,
                        const TileMap& map,
                        const MapProperties& props,
                        std::mt19937& rng) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/settlement_planner.h"

#include <algorithm>

namespace astra {

// ── Size determination ──

static int determine_size(const MapProperties& props) {
    // 0=small(3-5), 1=medium(5-8), 2=large(8-12)
    Biome b = props.biome;
    bool harsh = (b == Biome::Volcanic || b == Biome::ScarredScorched
               || b == Biome::ScarredGlassed || b == Biome::Ice);
    bool lush = (b == Biome::Forest || b == Biome::Jungle
              || b == Biome::Grassland || b == Biome::Marsh);

    if (harsh || props.lore_tier == 0) return 0;
    if (lush && props.lore_tier >= 2) return 2;
    return 1;
}

// Building size ranges: {min_w, max_w, min_h, max_h}
struct SizeRange { int min_w, max_w, min_h, max_h; };

static SizeRange building_size(BuildingType type) {
    switch (type) {
        case BuildingType::MainHall:    return {8, 12, 6, 8};
        case BuildingType::Market:      return {6, 8,  4, 6};
        case BuildingType::Dwelling:    return {4, 6,  3, 5};
        case BuildingType::Distillery:  return {6, 8,  5, 6};
        case BuildingType::Lookout:     return {3, 5,  3, 4};
        case BuildingType::Workshop:    return {5, 7,  4, 5};
        case BuildingType::Storage:     return {4, 6,  3, 4};
    }
    return {4, 6, 3, 5};
}

// ── Shape generation ──

static BuildingShape make_shape(BuildingType type, const SizeRange& sr,
                                int ox, int oy, int door_dir_x, int door_dir_y,
                                std::mt19937& rng) {
    BuildingShape shape;
    std::uniform_int_distribution<int> dw(sr.min_w, sr.max_w);
    std::uniform_int_distribution<int> dh(sr.min_h, sr.max_h);
    int w = dw(rng);
    int h = dh(rng);

    // Center the building on the origin
    shape.primary = {ox - w / 2, oy - h / 2, w, h};

    // Optionally add an extension for larger buildings
    bool add_extension = (type == BuildingType::MainHall
                       || type == BuildingType::Market
                       || type == BuildingType::Workshop)
                       && (rng() % 3 != 0);  // 66% chance

    if (add_extension) {
        // Add a wing on a random side (not the door side)
        int ext_w = 2 + rng() % 2;
        int ext_h = 2 + rng() % 2;
        // Pick side opposite to door direction
        Rect ext;
        if (door_dir_x > 0) {
            // Door faces right, add extension left
            ext = {shape.primary.x - ext_w, shape.primary.y + 1, ext_w, ext_h};
        } else if (door_dir_x < 0) {
            ext = {shape.primary.x + shape.primary.w, shape.primary.y + 1, ext_w, ext_h};
        } else if (door_dir_y > 0) {
            ext = {shape.primary.x + 1, shape.primary.y - ext_h, ext_w, ext_h};
        } else {
            ext = {shape.primary.x + 1, shape.primary.y + shape.primary.h, ext_w, ext_h};
        }
        shape.extensions.push_back(ext);
    }

    // Place door on the side facing the door direction
    if (door_dir_y > 0) {
        // Door on south wall
        shape.door_x = shape.primary.x + w / 2;
        shape.door_y = shape.primary.y + h - 1;
    } else if (door_dir_y < 0) {
        // Door on north wall
        shape.door_x = shape.primary.x + w / 2;
        shape.door_y = shape.primary.y;
    } else if (door_dir_x > 0) {
        // Door on east wall
        shape.door_x = shape.primary.x + w - 1;
        shape.door_y = shape.primary.y + h / 2;
    } else {
        // Door on west wall
        shape.door_x = shape.primary.x;
        shape.door_y = shape.primary.y + h / 2;
    }

    // Secondary door for larger buildings
    if (w >= 7 || h >= 6) {
        // Opposite side from primary door
        shape.door2_x = shape.primary.x + w - 1 - (shape.door_x - shape.primary.x);
        shape.door2_y = shape.primary.y + h - 1 - (shape.door_y - shape.primary.y);
    }

    return shape;
}

// ── Check if a rect overlaps any existing building ──

static bool overlaps_any(const Rect& r, const std::vector<BuildingSpec>& buildings,
                         int gap = 3) {
    Rect expanded = {r.x - gap, r.y - gap, r.w + gap * 2, r.h + gap * 2};
    for (const auto& b : buildings) {
        const auto& p = b.shape.primary;
        if (expanded.x < p.x + p.w && expanded.x + expanded.w > p.x
            && expanded.y < p.y + p.h && expanded.y + expanded.h > p.y)
            return true;
        for (const auto& ext : b.shape.extensions) {
            if (expanded.x < ext.x + ext.w && expanded.x + expanded.w > ext.x
                && expanded.y < ext.y + ext.h && expanded.y + expanded.h > ext.y)
                return true;
        }
    }
    return false;
}

// ── Check if rect is within footprint and not on water ──

static bool rect_valid(const Rect& r, const Rect& footprint, const TileMap& map) {
    if (r.x < footprint.x || r.y < footprint.y
        || r.x + r.w > footprint.x + footprint.w
        || r.y + r.h > footprint.y + footprint.h)
        return false;
    // Check for water tiles inside the rect
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (x >= 0 && x < map.width() && y >= 0 && y < map.height()
                && map.get(x, y) == Tile::Water)
                return false;
        }
    }
    return true;
}

// ── Growth: find position for next building near existing ones ──

static bool find_growth_position(const std::vector<BuildingSpec>& existing,
                                  const Rect& footprint,
                                  const TileMap& map,
                                  const SizeRange& sr,
                                  int& out_x, int& out_y,
                                  int& dir_x, int& dir_y,
                                  std::mt19937& rng) {
    // Try positions around existing buildings
    static constexpr int offsets[][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };

    std::vector<std::tuple<int,int,int,int>> candidates;
    int gap = 4;  // space between buildings for paths

    for (const auto& b : existing) {
        int cx = b.shape.primary.x + b.shape.primary.w / 2;
        int cy = b.shape.primary.y + b.shape.primary.h / 2;

        for (auto [dx, dy] : offsets) {
            int nx = cx + dx * (b.shape.primary.w / 2 + sr.min_w / 2 + gap);
            int ny = cy + dy * (b.shape.primary.h / 2 + sr.min_h / 2 + gap);

            Rect test = {nx - sr.min_w / 2, ny - sr.min_h / 2, sr.max_w, sr.max_h};
            if (!rect_valid(test, footprint, map)) continue;
            if (overlaps_any(test, existing, 3)) continue;

            // Direction back toward the existing building (for door placement)
            candidates.push_back({nx, ny, -dx, -dy});
        }
    }

    if (candidates.empty()) return false;

    auto [rx, ry, rdx, rdy] = candidates[rng() % candidates.size()];
    out_x = rx;
    out_y = ry;
    dir_x = rdx;
    dir_y = rdy;
    return true;
}

// ── Terrain mods ──

static void add_terrain_mods(SettlementPlan& plan,
                              const TerrainChannels& channels,
                              const PlacementResult& placement) {
    // Level the center area for plaza
    int cx = placement.footprint.x + placement.footprint.w / 2;
    int cy = placement.footprint.y + placement.footprint.h / 2;
    float center_elev = channels.elev(cx, cy);
    plan.terrain_mods.push_back({
        TerrainModType::Level,
        {cx - 6, cy - 4, 12, 8},
        center_elev
    });

    // Clear structure masks in entire footprint
    plan.terrain_mods.push_back({
        TerrainModType::Clear,
        placement.footprint,
        0.0f
    });

    // Check for elevated anchor — if not natural, raise a bluff
    for (const auto& anchor : placement.anchors) {
        if (anchor.type == AnchorType::Elevated) {
            float elev = channels.elev(anchor.x, anchor.y);
            if (elev - center_elev < 0.1f) {
                // Not naturally elevated — sculpt a bluff
                plan.terrain_mods.push_back({
                    TerrainModType::RaiseBluff,
                    {anchor.x - 3, anchor.y - 2, 6, 4},
                    center_elev + 0.25f
                });
            }
        }
    }
}

// ── Bridge detection ──

static void detect_bridges(SettlementPlan& plan, const TileMap& map,
                            const Rect& footprint) {
    // Scan for water bodies that intersect potential path corridors
    // from settlement center toward each map edge
    int cx = footprint.x + footprint.w / 2;
    int cy = footprint.y + footprint.h / 2;

    // Check the four cardinal directions from center to map edge
    struct Direction { int dx, dy; };
    Direction dirs[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    for (auto [dx, dy] : dirs) {
        int x = cx, y = cy;
        bool in_water = false;
        int water_start_x = 0, water_start_y = 0;

        while (x >= 0 && x < map.width() && y >= 0 && y < map.height()) {
            bool is_water = (map.get(x, y) == Tile::Water);

            if (is_water && !in_water) {
                water_start_x = x;
                water_start_y = y;
                in_water = true;
            } else if (!is_water && in_water) {
                // Found a water crossing
                plan.bridges.push_back({
                    water_start_x - dx,  // start on land
                    water_start_y - dy,
                    x, y,                // end on land
                    2                    // main path width
                });
                in_water = false;
                break;  // one bridge per direction
            }

            x += dx;
            y += dy;
        }
    }
}

// ── Perimeter decision ──

static void plan_perimeter(SettlementPlan& plan, const MapProperties& props) {
    bool walled = (props.lore_tier >= 2)
               || (props.biome == Biome::Volcanic)
               || (props.biome == Biome::ScarredScorched)
               || (props.biome == Biome::ScarredGlassed);

    if (!walled) return;

    // Compute bounding rect of all buildings + padding
    int min_x = 9999, min_y = 9999, max_x = -1, max_y = -1;
    for (const auto& b : plan.buildings) {
        const auto& r = b.shape.primary;
        min_x = std::min(min_x, r.x);
        min_y = std::min(min_y, r.y);
        max_x = std::max(max_x, r.x + r.w);
        max_y = std::max(max_y, r.y + r.h);
        for (const auto& ext : b.shape.extensions) {
            min_x = std::min(min_x, ext.x);
            min_y = std::min(min_y, ext.y);
            max_x = std::max(max_x, ext.x + ext.w);
            max_y = std::max(max_y, ext.y + ext.h);
        }
    }

    int pad = 4;
    PerimeterSpec perim;
    perim.bounds = {min_x - pad, min_y - pad, (max_x - min_x) + pad * 2, (max_y - min_y) + pad * 2};

    // Place gates at the midpoints of each side that has a path
    // For simplicity, always two gates: north and south (or east/west)
    int mid_x = perim.bounds.x + perim.bounds.w / 2;
    int mid_y = perim.bounds.y + perim.bounds.h / 2;
    perim.gate_positions.push_back({mid_x, perim.bounds.y});
    perim.gate_positions.push_back({mid_x, perim.bounds.y + perim.bounds.h - 1});

    plan.perimeter = perim;
}

// ── Main planner ──

SettlementPlan SettlementPlanner::plan(const PlacementResult& placement,
                                        const TerrainChannels& channels,
                                        const TileMap& map,
                                        const MapProperties& props,
                                        std::mt19937& rng) const {
    SettlementPlan sp;
    sp.placement = placement;
    sp.style = select_civ_style(props);
    sp.size_category = determine_size(props);

    // Determine building count
    int min_buildings, max_buildings;
    switch (sp.size_category) {
        case 0: min_buildings = 3; max_buildings = 5; break;
        case 1: min_buildings = 5; max_buildings = 8; break;
        case 2: min_buildings = 8; max_buildings = 12; break;
        default: min_buildings = 3; max_buildings = 5; break;
    }
    std::uniform_int_distribution<int> count_dist(min_buildings, max_buildings);
    int target_count = count_dist(rng);

    // ── Terrain modifications ──
    add_terrain_mods(sp, channels, placement);

    // ── Place anchor buildings ──

    for (const auto& anchor : placement.anchors) {
        BuildingType type;
        switch (anchor.type) {
            case AnchorType::Center:     type = BuildingType::MainHall; break;
            case AnchorType::Waterfront: type = BuildingType::Distillery; break;
            case AnchorType::Elevated:   type = BuildingType::Lookout; break;
        }

        auto sr = building_size(type);
        // Door faces toward center anchor
        int center_x = placement.footprint.x + placement.footprint.w / 2;
        int center_y = placement.footprint.y + placement.footprint.h / 2;
        int dx = (center_x > anchor.x) ? 1 : (center_x < anchor.x) ? -1 : 0;
        int dy = (center_y > anchor.y) ? 1 : (center_y < anchor.y) ? -1 : 0;
        if (dx == 0 && dy == 0) dy = 1;

        auto shape = make_shape(type, sr, anchor.x, anchor.y, dx, dy, rng);

        if (rect_valid(shape.primary, placement.footprint, map)
            && !overlaps_any(shape.primary, sp.buildings)) {
            sp.buildings.push_back({type, shape, anchor.type});
        }
    }

    // ── Place market near center ──

    if (sp.buildings.size() < static_cast<size_t>(target_count)) {
        auto sr = building_size(BuildingType::Market);
        int ox, oy, dx, dy;
        if (find_growth_position(sp.buildings, placement.footprint, map, sr,
                                  ox, oy, dx, dy, rng)) {
            auto shape = make_shape(BuildingType::Market, sr, ox, oy, dx, dy, rng);
            if (rect_valid(shape.primary, placement.footprint, map)
                && !overlaps_any(shape.primary, sp.buildings)) {
                sp.buildings.push_back({BuildingType::Market, shape, AnchorType::Center});
            }
        }
    }

    // ── Grow remaining buildings ──

    // Building types to fill with (weighted toward dwellings)
    BuildingType fill_types[] = {
        BuildingType::Dwelling, BuildingType::Dwelling, BuildingType::Dwelling,
        BuildingType::Workshop, BuildingType::Storage, BuildingType::Dwelling,
    };
    int fill_idx = 0;

    int attempts = 0;
    while (static_cast<int>(sp.buildings.size()) < target_count && attempts < 50) {
        BuildingType type = fill_types[fill_idx % 6];
        ++fill_idx;
        auto sr = building_size(type);

        int ox, oy, dx, dy;
        if (find_growth_position(sp.buildings, placement.footprint, map, sr,
                                  ox, oy, dx, dy, rng)) {
            auto shape = make_shape(type, sr, ox, oy, dx, dy, rng);
            if (rect_valid(shape.primary, placement.footprint, map)
                && !overlaps_any(shape.primary, sp.buildings)) {
                sp.buildings.push_back({type, shape, AnchorType::Center});
            }
        }
        ++attempts;
    }

    // ── Plan paths (door-to-door connections) ──

    // Connect each building's door to the nearest other building's door
    for (size_t i = 0; i < sp.buildings.size(); ++i) {
        int best_j = -1;
        int best_dist = std::numeric_limits<int>::max();
        for (size_t j = 0; j < sp.buildings.size(); ++j) {
            if (i == j) continue;
            int dist = std::abs(sp.buildings[i].shape.door_x - sp.buildings[j].shape.door_x)
                     + std::abs(sp.buildings[i].shape.door_y - sp.buildings[j].shape.door_y);
            if (dist < best_dist) { best_dist = dist; best_j = static_cast<int>(j); }
        }
        if (best_j >= 0) {
            sp.paths.push_back({
                sp.buildings[i].shape.door_x, sp.buildings[i].shape.door_y,
                sp.buildings[best_j].shape.door_x, sp.buildings[best_j].shape.door_y,
                1  // branch path
            });
        }
    }

    // Entry path from settlement center to nearest map edge (2-wide)
    int cx = placement.footprint.x + placement.footprint.w / 2;
    int cy = placement.footprint.y + placement.footprint.h / 2;
    int dist_n = cy, dist_s = map.height() - cy;
    int dist_w = cx, dist_e = map.width() - cx;
    int min_dist = std::min({dist_n, dist_s, dist_w, dist_e});

    if (min_dist == dist_n) {
        sp.paths.push_back({cx, cy, cx, 0, 2});
    } else if (min_dist == dist_s) {
        sp.paths.push_back({cx, cy, cx, map.height() - 1, 2});
    } else if (min_dist == dist_w) {
        sp.paths.push_back({cx, cy, 0, cy, 2});
    } else {
        sp.paths.push_back({cx, cy, map.width() - 1, cy, 2});
    }

    // ── Bridge detection ──
    detect_bridges(sp, map, placement.footprint);

    // ── Perimeter decision ──
    plan_perimeter(sp, props);

    return sp;
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/settlement_planner.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/settlement_planner.h src/generators/settlement_planner.cpp CMakeLists.txt
git commit -m "feat: add settlement planner with anchor growth and terrain sculpting"
```

---

### Task 7: BuildingGenerator

Places buildings on the TileMap from BuildingSpecs.

**Files:**
- Create: `include/astra/building_generator.h`
- Create: `src/generators/building_generator.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class BuildingGenerator {
public:
    // Render a single building onto the tilemap
    void generate(TileMap& map,
                  const BuildingSpec& spec,
                  const CivStyle& style,
                  std::mt19937& rng) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/building_generator.h"

namespace astra {

// ── Helpers ──

// Check if (x,y) is on the perimeter of a rect
static bool is_perimeter(int x, int y, const Rect& r) {
    return (x == r.x || x == r.x + r.w - 1 || y == r.y || y == r.y + r.h - 1);
}

// Check if (x,y) is a corner of a rect
static bool is_corner(int x, int y, const Rect& r) {
    return (x == r.x || x == r.x + r.w - 1) && (y == r.y || y == r.y + r.h - 1);
}

// Check if (x,y) is an interior tile of any rect in the shape
static bool is_interior(int x, int y, const BuildingShape& shape) {
    const auto& p = shape.primary;
    if (x > p.x && x < p.x + p.w - 1 && y > p.y && y < p.y + p.h - 1)
        return true;
    for (const auto& ext : shape.extensions) {
        if (x > ext.x && x < ext.x + ext.w - 1 && y > ext.y && y < ext.y + ext.h - 1)
            return true;
    }
    return false;
}

// Check if (x,y) is part of the shape (primary or any extension)
static bool in_shape(int x, int y, const BuildingShape& shape) {
    if (shape.primary.contains(x, y)) return true;
    for (const auto& ext : shape.extensions) {
        if (ext.contains(x, y)) return true;
    }
    return false;
}

// Check if (x,y) is on the boundary of the entire shape
static bool is_shape_edge(int x, int y, const BuildingShape& shape) {
    if (!in_shape(x, y, shape)) return false;
    // Check if any neighbor is outside the shape
    for (auto [dx, dy] : {std::pair{1,0},{-1,0},{0,1},{0,-1}}) {
        if (!in_shape(x + dx, y + dy, shape)) return true;
    }
    return false;
}

// Check if a position is adjacent to the door
static bool near_door(int x, int y, const BuildingShape& shape) {
    if (std::abs(x - shape.door_x) + std::abs(y - shape.door_y) <= 1)
        return true;
    if (shape.door2_x >= 0
        && std::abs(x - shape.door2_x) + std::abs(y - shape.door2_y) <= 1)
        return true;
    return false;
}

// Check if a position is a shape corner (corner of any rect in shape)
static bool is_shape_corner(int x, int y, const BuildingShape& shape) {
    if (is_corner(x, y, shape.primary)) return true;
    for (const auto& ext : shape.extensions) {
        if (is_corner(x, y, ext)) return true;
    }
    return false;
}

// ── Window placement ──

static void place_windows(TileMap& map, const BuildingShape& shape,
                          const CivStyle& style, std::mt19937& rng) {
    // Walk shape edges, place windows every 3-4 tiles (not near doors, not on corners)
    int spacing = 3;
    int since_last = 0;

    auto try_window = [&](int x, int y) {
        if (!is_shape_edge(x, y, shape)) return;
        if (is_shape_corner(x, y, shape)) { since_last = 0; return; }
        if (near_door(x, y, shape)) { since_last = 0; return; }

        ++since_last;
        if (since_last >= spacing) {
            // Apply decay — ruined style may skip windows
            if (style.decay > 0.0f) {
                std::uniform_real_distribution<float> d(0.0f, 1.0f);
                if (d(rng) < style.decay) return;
            }
            map.add_fixture(x, y, make_fixture(FixtureType::Window));
            since_last = 0;
        }
    };

    // Walk primary rect perimeter
    const auto& p = shape.primary;
    for (int x = p.x; x < p.x + p.w; ++x) { try_window(x, p.y); }
    for (int y = p.y; y < p.y + p.h; ++y) { try_window(p.x + p.w - 1, y); }
    for (int x = p.x + p.w - 1; x >= p.x; --x) { try_window(x, p.y + p.h - 1); }
    for (int y = p.y + p.h - 1; y >= p.y; --y) { try_window(p.x, y); }
}

// ── Interior furnishing ──

static void furnish_interior(TileMap& map, const BuildingSpec& spec,
                              const CivStyle& style, std::mt19937& rng) {
    auto palette = furniture_palette(spec.type, style);
    const auto& shape = spec.shape;
    const auto& p = shape.primary;

    // Collect valid interior positions categorized by placement type
    struct Spot { int x, y; bool wall_adj; bool corner; };
    std::vector<Spot> spots;

    for (int y = p.y + 1; y < p.y + p.h - 1; ++y) {
        for (int x = p.x + 1; x < p.x + p.w - 1; ++x) {
            if (map.get(x, y) != style.floor_tile) continue;
            if (map.fixture_id(x, y) >= 0) continue;
            if (x == shape.door_x && y == shape.door_y) continue;
            if (shape.door2_x >= 0 && x == shape.door2_x && y == shape.door2_y) continue;

            bool adj = (x == p.x + 1 || x == p.x + p.w - 2
                     || y == p.y + 1 || y == p.y + p.h - 2);
            bool corn = (x == p.x + 1 || x == p.x + p.w - 2)
                     && (y == p.y + 1 || y == p.y + p.h - 2);
            spots.push_back({x, y, adj, corn});
        }
    }

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    for (const auto& entry : palette.entries) {
        if (prob(rng) > entry.frequency) continue;

        // Find a suitable spot
        for (auto it = spots.begin(); it != spots.end(); ++it) {
            if (entry.wall_adjacent && !it->wall_adj) continue;
            if (entry.prefers_corner && !it->corner) continue;

            if (entry.needs_clearance) {
                // Check at least one adjacent tile is free
                bool clear = false;
                for (auto [dx, dy] : {std::pair{1,0},{-1,0},{0,1},{0,-1}}) {
                    int nx = it->x + dx, ny = it->y + dy;
                    if (nx > p.x && nx < p.x + p.w - 1
                        && ny > p.y && ny < p.y + p.h - 1
                        && map.fixture_id(nx, ny) < 0) {
                        clear = true;
                        break;
                    }
                }
                if (!clear) continue;
            }

            auto fd = make_fixture(entry.type);
            map.add_fixture(it->x, it->y, fd);
            spots.erase(it);
            break;
        }
    }
}

// ── Main generate ──

void BuildingGenerator::generate(TileMap& map,
                                  const BuildingSpec& spec,
                                  const CivStyle& style,
                                  std::mt19937& rng) const {
    const auto& shape = spec.shape;

    // Helper to process all rects in shape
    auto for_each_rect = [&](auto fn) {
        fn(shape.primary);
        for (const auto& ext : shape.extensions) fn(ext);
    };

    std::uniform_real_distribution<float> decay_roll(0.0f, 1.0f);

    // 1. Clear existing fixtures in footprint + margin
    for_each_rect([&](const Rect& r) {
        for (int y = r.y - 1; y < r.y + r.h + 1; ++y) {
            for (int x = r.x - 1; x < r.x + r.w + 1; ++x) {
                if (x >= 0 && x < map.width() && y >= 0 && y < map.height()) {
                    map.remove_fixture(x, y);
                }
            }
        }
    });

    // 2. Place floor tiles (interior)
    for_each_rect([&](const Rect& r) {
        for (int y = r.y; y < r.y + r.h; ++y) {
            for (int x = r.x; x < r.x + r.w; ++x) {
                if (x >= 0 && x < map.width() && y >= 0 && y < map.height()) {
                    map.set(x, y, style.floor_tile);
                }
            }
        }
    });

    // 3. Place walls (shape edges)
    for_each_rect([&](const Rect& r) {
        for (int y = r.y; y < r.y + r.h; ++y) {
            for (int x = r.x; x < r.x + r.w; ++x) {
                if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;
                if (!is_shape_edge(x, y, shape)) continue;

                // Ruined style may have gaps
                if (style.decay > 0.0f && decay_roll(rng) < style.decay) continue;

                map.set(x, y, style.wall_tile);
            }
        }
    });

    // 4. Place doors
    auto place_door = [&](int dx, int dy) {
        if (dx >= 0 && dx < map.width() && dy >= 0 && dy < map.height()) {
            map.set(dx, dy, style.floor_tile);
            map.add_fixture(dx, dy, make_fixture(FixtureType::Door));
        }
    };
    place_door(shape.door_x, shape.door_y);
    if (shape.door2_x >= 0) {
        place_door(shape.door2_x, shape.door2_y);
    }

    // 5. Windows
    place_windows(map, shape, style, rng);

    // 6. Interior furnishing
    furnish_interior(map, spec, style, rng);
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/building_generator.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/building_generator.h src/generators/building_generator.cpp CMakeLists.txt
git commit -m "feat: add building generator with composite shapes and furnishing"
```

---

### Task 8: PathRouter

Connects buildings with paths and builds bridges over water.

**Files:**
- Create: `include/astra/path_router.h`
- Create: `src/generators/path_router.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"

namespace astra {

class PathRouter {
public:
    void route(TileMap& map,
               const SettlementPlan& plan) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/path_router.h"

namespace astra {

// ── Carve a single tile as path ──

static void carve_path_tile(TileMap& map, int x, int y, Tile path_tile) {
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    Tile current = map.get(x, y);
    // Don't overwrite structural walls or indoor floors (buildings)
    if (current == Tile::StructuralWall || current == Tile::IndoorFloor) return;
    if (current == Tile::Water) return;

    map.set(x, y, path_tile);
    map.remove_fixture(x, y);  // clear scatter on path
}

// ── L-shaped path between two points ──

static void carve_l_path(TileMap& map, const PathSpec& path, Tile path_tile) {
    int x0 = path.from_x, y0 = path.from_y;
    int x1 = path.to_x, y1 = path.to_y;
    int w = path.width;

    // Pick the shorter L: horizontal-first or vertical-first
    // Use horizontal-first for consistency
    int dx = (x1 > x0) ? 1 : -1;
    int dy = (y1 > y0) ? 1 : -1;

    // Horizontal segment
    for (int x = x0; x != x1 + dx; x += dx) {
        for (int wi = 0; wi < w; ++wi) {
            carve_path_tile(map, x, y0 + wi, path_tile);
        }
    }

    // Vertical segment
    for (int y = y0; y != y1 + dy; y += dy) {
        for (int wi = 0; wi < w; ++wi) {
            carve_path_tile(map, x1 + wi, y, path_tile);
        }
    }
}

// ── Bridge construction ──

static void build_bridge(TileMap& map, const BridgeSpec& bridge,
                          const CivStyle& style) {
    int dx = 0, dy = 0;
    if (bridge.end_x != bridge.start_x)
        dx = (bridge.end_x > bridge.start_x) ? 1 : -1;
    if (bridge.end_y != bridge.start_y)
        dy = (bridge.end_y > bridge.start_y) ? 1 : -1;

    // Determine bridge orientation (horizontal or vertical)
    bool horizontal = (dx != 0);
    int length = horizontal
        ? std::abs(bridge.end_x - bridge.start_x) + 1
        : std::abs(bridge.end_y - bridge.start_y) + 1;

    int x = bridge.start_x;
    int y = bridge.start_y;

    for (int i = 0; i < length; ++i) {
        // Bridge floor
        for (int w = 0; w < bridge.width; ++w) {
            int fx = horizontal ? x : x + w;
            int fy = horizontal ? y + w : y;
            if (fx >= 0 && fx < map.width() && fy >= 0 && fy < map.height()) {
                map.set(fx, fy, Tile::Floor);
                map.remove_fixture(fx, fy);
                map.add_fixture(fx, fy, make_fixture(style.bridge_floor));
            }
        }

        // Railings on both sides
        if (horizontal) {
            int ry1 = y - 1;
            int ry2 = y + bridge.width;
            if (ry1 >= 0 && ry1 < map.height()) {
                map.set(x, ry1, Tile::Floor);
                map.remove_fixture(x, ry1);
                map.add_fixture(x, ry1, make_fixture(style.bridge_rail));
            }
            if (ry2 >= 0 && ry2 < map.height()) {
                map.set(x, ry2, Tile::Floor);
                map.remove_fixture(x, ry2);
                map.add_fixture(x, ry2, make_fixture(style.bridge_rail));
            }
        } else {
            int rx1 = x - 1;
            int rx2 = x + bridge.width;
            if (rx1 >= 0 && rx1 < map.width()) {
                map.set(rx1, y, Tile::Floor);
                map.remove_fixture(rx1, y);
                map.add_fixture(rx1, y, make_fixture(style.bridge_rail));
            }
            if (rx2 >= 0 && rx2 < map.width()) {
                map.set(rx2, y, Tile::Floor);
                map.remove_fixture(rx2, y);
                map.add_fixture(rx2, y, make_fixture(style.bridge_rail));
            }
        }

        // Support pillars at ends
        bool is_end = (i == 0 || i == length - 1);
        if (is_end) {
            for (int w = -1; w <= bridge.width; ++w) {
                int px = horizontal ? x : x + w;
                int py = horizontal ? y + w : y;
                if (px >= 0 && px < map.width() && py >= 0 && py < map.height()) {
                    if (w == -1 || w == bridge.width) {
                        // Pillar positions
                        map.set(px, py, Tile::Wall);
                    }
                }
            }
        }

        x += dx;
        y += dy;
    }
}

// ── Main route ──

void PathRouter::route(TileMap& map, const SettlementPlan& plan) const {
    // Carve all paths
    for (const auto& path : plan.paths) {
        carve_l_path(map, path, plan.style.path_tile);
    }

    // Build bridges
    for (const auto& bridge : plan.bridges) {
        build_bridge(map, bridge, plan.style);
    }
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/path_router.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/path_router.h src/generators/path_router.cpp CMakeLists.txt
git commit -m "feat: add path router with L-shaped paths and bridge generation"
```

---

### Task 9: PerimeterBuilder

Optional walled enclosure with gates.

**Files:**
- Create: `include/astra/perimeter_builder.h`
- Create: `src/generators/perimeter_builder.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class PerimeterBuilder {
public:
    void build(TileMap& map,
               const SettlementPlan& plan,
               std::mt19937& rng) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/perimeter_builder.h"

namespace astra {

void PerimeterBuilder::build(TileMap& map,
                              const SettlementPlan& plan,
                              std::mt19937& rng) const {
    if (!plan.perimeter.has_value()) return;

    const auto& perim = plan.perimeter.value();
    const auto& style = plan.style;
    const auto& r = perim.bounds;

    std::uniform_real_distribution<float> decay_roll(0.0f, 1.0f);

    // Place perimeter walls
    for (int x = r.x; x < r.x + r.w; ++x) {
        for (int y = r.y; y < r.y + r.h; ++y) {
            // Only place on the border
            bool border = (x == r.x || x == r.x + r.w - 1
                        || y == r.y || y == r.y + r.h - 1);
            if (!border) continue;

            if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;

            // Check if this is a gate position (or adjacent to one for 2-wide gates)
            bool is_gate = false;
            for (const auto& [gx, gy] : perim.gate_positions) {
                if (std::abs(x - gx) <= 1 && y == gy) { is_gate = true; break; }
                if (std::abs(y - gy) <= 1 && x == gx) { is_gate = true; break; }
            }

            if (is_gate) {
                // Gate opening — place gate fixture on passable floor
                map.set(x, y, Tile::Floor);
                map.remove_fixture(x, y);
                map.add_fixture(x, y, make_fixture(style.gate));
                continue;
            }

            // Ruined style: gaps in walls
            if (style.decay > 0.0f && decay_roll(rng) < style.decay) continue;

            // Don't overwrite existing buildings
            Tile current = map.get(x, y);
            if (current == Tile::StructuralWall || current == Tile::IndoorFloor) continue;

            map.set(x, y, style.perimeter_wall);
            map.remove_fixture(x, y);
        }
    }

    // Advanced style: corner towers (2x2 rooms with lighting)
    if (style.name == "Advanced") {
        int corners[][2] = {
            {r.x, r.y}, {r.x + r.w - 2, r.y},
            {r.x, r.y + r.h - 2}, {r.x + r.w - 2, r.y + r.h - 2}
        };
        for (auto [tx, ty] : corners) {
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    int px = tx + dx, py = ty + dy;
                    if (px >= 0 && px < map.width() && py >= 0 && py < map.height()) {
                        map.set(px, py, style.perimeter_wall);
                    }
                }
            }
            // Light inside the tower
            int lx = tx + 1, ly = ty + 1;
            if (lx >= 0 && lx < map.width() && ly >= 0 && ly < map.height()) {
                map.set(lx, ly, style.floor_tile);
                map.add_fixture(lx, ly, make_fixture(style.lighting));
            }
        }
    }
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/perimeter_builder.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/perimeter_builder.h src/generators/perimeter_builder.cpp CMakeLists.txt
git commit -m "feat: add perimeter builder with gates and corner towers"
```

---

### Task 10: ExteriorDecorator

Lamps, benches, scatter clearing, transition zones.

**Files:**
- Create: `include/astra/exterior_decorator.h`
- Create: `src/generators/exterior_decorator.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class ExteriorDecorator {
public:
    void decorate(TileMap& map,
                  const SettlementPlan& plan,
                  std::mt19937& rng) const;
};

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/exterior_decorator.h"

namespace astra {

// ── Clear scatter within settlement footprint ──

static void clear_scatter(TileMap& map, const Rect& footprint, int margin = 2) {
    for (int y = footprint.y - margin; y < footprint.y + footprint.h + margin; ++y) {
        for (int x = footprint.x - margin; x < footprint.x + footprint.w + margin; ++x) {
            if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            FixtureType ft = map.fixture(fid).type;
            // Only clear natural scatter, not settlement fixtures
            if (ft == FixtureType::NaturalObstacle || ft == FixtureType::ShoreDebris) {
                map.remove_fixture(x, y);
            }
        }
    }
}

// ── Place lamps along paths ──

static void place_path_lights(TileMap& map, const SettlementPlan& plan,
                               std::mt19937& rng) {
    const auto& style = plan.style;
    int spacing = 6;

    // Walk each path and place lights at intervals
    for (const auto& path : plan.paths) {
        int dx = (path.to_x > path.from_x) ? 1 : (path.to_x < path.from_x) ? -1 : 0;
        int dy = (path.to_y > path.from_y) ? 1 : (path.to_y < path.from_y) ? -1 : 0;

        int x = path.from_x, y = path.from_y;
        int steps = 0;

        // Horizontal segment
        while (x != path.to_x) {
            if (steps % spacing == 0) {
                // Place lamp offset from path (above or below)
                int ly = y - 1;
                if (ly >= 0 && ly < map.height()
                    && map.get(ly == y - 1 ? x : x, ly) == Tile::Floor
                    && map.fixture_id(x, ly) < 0) {
                    map.add_fixture(x, ly, make_fixture(style.lighting));
                }
            }
            x += dx;
            ++steps;
        }

        // Vertical segment
        steps = 0;
        while (y != path.to_y) {
            if (steps % spacing == 0) {
                int lx = x - 1;
                if (lx >= 0 && lx < map.width()
                    && map.get(lx, y) == Tile::Floor
                    && map.fixture_id(lx, y) < 0) {
                    map.add_fixture(lx, y, make_fixture(style.lighting));
                }
            }
            y += dy;
            ++steps;
        }
    }
    (void)rng;
}

// ── Place lamps flanking building doors ──

static void place_door_lights(TileMap& map, const SettlementPlan& plan) {
    const auto& style = plan.style;

    for (const auto& building : plan.buildings) {
        int dx = building.shape.door_x;
        int dy = building.shape.door_y;

        // Try placing lights on both sides of the door
        for (auto [ox, oy] : {std::pair{1, 0}, std::pair{-1, 0},
                               std::pair{0, 1}, std::pair{0, -1}}) {
            int lx = dx + ox, ly = dy + oy;
            if (lx < 0 || lx >= map.width() || ly < 0 || ly >= map.height()) continue;
            Tile t = map.get(lx, ly);
            if (t == Tile::StructuralWall || t == Tile::Wall) {
                // Place light on the wall next to the door
                if (map.fixture_id(lx, ly) < 0) {
                    map.add_fixture(lx, ly, make_fixture(style.lighting));
                }
                break;
            }
        }
    }
}

// ── Place benches near center/plaza ──

static void place_benches(TileMap& map, const SettlementPlan& plan,
                           std::mt19937& rng) {
    int cx = plan.placement.footprint.x + plan.placement.footprint.w / 2;
    int cy = plan.placement.footprint.y + plan.placement.footprint.h / 2;

    // Place 2-4 benches near the center
    std::uniform_int_distribution<int> count(2, 4);
    int n = count(rng);

    for (int i = 0; i < n; ++i) {
        std::uniform_int_distribution<int> ox(-4, 4);
        std::uniform_int_distribution<int> oy(-3, 3);
        int bx = cx + ox(rng), by = cy + oy(rng);

        if (bx >= 0 && bx < map.width() && by >= 0 && by < map.height()
            && map.get(bx, by) == plan.style.path_tile
            && map.fixture_id(bx, by) < 0) {
            map.add_fixture(bx, by, make_fixture(plan.style.seating));
        }
    }
}

// ── Place planters on lush biomes ──

static void place_planters(TileMap& map, const SettlementPlan& plan,
                            const Rect& footprint, std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    for (const auto& building : plan.buildings) {
        const auto& r = building.shape.primary;
        // Place planters near building corners (outside)
        int corners[][2] = {
            {r.x - 1, r.y - 1}, {r.x + r.w, r.y - 1},
            {r.x - 1, r.y + r.h}, {r.x + r.w, r.y + r.h}
        };
        for (auto [px, py] : corners) {
            if (prob(rng) < 0.3f) continue;
            if (px < 0 || px >= map.width() || py < 0 || py >= map.height()) continue;
            if (!footprint.contains(px, py)) continue;
            if (map.get(px, py) != Tile::Floor && map.get(px, py) != plan.style.path_tile) continue;
            if (map.fixture_id(px, py) >= 0) continue;
            map.add_fixture(px, py, make_fixture(FixtureType::Planter));
        }
    }
}

// ── Main decorate ──

void ExteriorDecorator::decorate(TileMap& map,
                                  const SettlementPlan& plan,
                                  std::mt19937& rng) const {
    // 1. Clear natural scatter in settlement area
    clear_scatter(map, plan.placement.footprint);

    // 2. Path lighting
    place_path_lights(map, plan, rng);

    // 3. Door lighting
    place_door_lights(map, plan);

    // 4. Benches near plaza
    place_benches(map, plan, rng);

    // 5. Planters on non-harsh biomes
    place_planters(map, plan, plan.placement.footprint, rng);
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/exterior_decorator.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/exterior_decorator.h src/generators/exterior_decorator.cpp CMakeLists.txt
git commit -m "feat: add exterior decorator with lighting, benches, and scatter clearing"
```

---

### Task 11: POI Phase Orchestrator

The top-level function that wires everything together, called from the v2 generator.

**Files:**
- Create: `src/generators/poi_phase.cpp`
- Create: `include/astra/poi_phase.h`
- Modify: `src/generators/detail_map_generator_v2.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"
#include "astra/map_properties.h"

#include <random>

namespace astra {

// Run the POI phase on a detail map. Currently handles settlements only.
void poi_phase(TileMap& map,
               const TerrainChannels& channels,
               const MapProperties& props,
               std::mt19937& rng);

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/poi_phase.h"
#include "astra/placement_scorer.h"
#include "astra/settlement_planner.h"
#include "astra/building_generator.h"
#include "astra/path_router.h"
#include "astra/perimeter_builder.h"
#include "astra/exterior_decorator.h"

namespace astra {

// ── Terrain sculpting ──

static void apply_terrain_mods(TileMap& map, TerrainChannels& channels,
                                const std::vector<TerrainMod>& mods) {
    for (const auto& mod : mods) {
        const auto& r = mod.area;

        switch (mod.type) {
            case TerrainModType::Level: {
                for (int y = r.y; y < r.y + r.h && y < channels.height; ++y) {
                    for (int x = r.x; x < r.x + r.w && x < channels.width; ++x) {
                        if (x < 0 || y < 0) continue;
                        channels.elev(x, y) = mod.target_elevation;
                        // If this was a wall due to high elevation, make it floor
                        if (map.get(x, y) == Tile::Wall) {
                            map.set(x, y, Tile::Floor);
                        }
                    }
                }
                break;
            }
            case TerrainModType::RaiseBluff: {
                for (int y = r.y; y < r.y + r.h && y < channels.height; ++y) {
                    for (int x = r.x; x < r.x + r.w && x < channels.width; ++x) {
                        if (x < 0 || y < 0) continue;
                        channels.elev(x, y) = mod.target_elevation;
                    }
                }
                // Place wall tiles on the edges of the bluff
                for (int y = r.y; y < r.y + r.h; ++y) {
                    for (int x = r.x; x < r.x + r.w; ++x) {
                        if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;
                        bool edge = (x == r.x || x == r.x + r.w - 1
                                  || y == r.y || y == r.y + r.h - 1);
                        if (edge) {
                            map.set(x, y, Tile::Wall);
                        } else {
                            map.set(x, y, Tile::Floor);
                        }
                    }
                }
                break;
            }
            case TerrainModType::CutBank: {
                for (int y = r.y; y < r.y + r.h && y < channels.height; ++y) {
                    for (int x = r.x; x < r.x + r.w && x < channels.width; ++x) {
                        if (x < 0 || y < 0) continue;
                        channels.elev(x, y) = mod.target_elevation;
                        if (map.get(x, y) == Tile::Wall) {
                            map.set(x, y, Tile::Floor);
                        }
                    }
                }
                break;
            }
            case TerrainModType::Clear: {
                for (int y = r.y; y < r.y + r.h && y < channels.height; ++y) {
                    for (int x = r.x; x < r.x + r.w && x < channels.width; ++x) {
                        if (x < 0 || y < 0) continue;
                        channels.struc(x, y) = StructureMask::None;
                        if (map.get(x, y) == Tile::Wall) {
                            map.set(x, y, Tile::Floor);
                        }
                    }
                }
                break;
            }
        }
    }
}

// ── Main POI phase ──

void poi_phase(TileMap& map,
               const TerrainChannels& channels,
               const MapProperties& props,
               std::mt19937& rng) {
    // Only handle settlements for now
    if (!props.detail_has_poi || props.detail_poi_type != Tile::OW_Settlement) return;

    // 1. Score placement
    PlacementScorer scorer;
    // Settlement footprint depends on expected size
    int foot_w = 60, foot_h = 40;  // medium default
    Biome b = props.biome;
    bool harsh = (b == Biome::Volcanic || b == Biome::ScarredScorched
               || b == Biome::ScarredGlassed || b == Biome::Ice);
    bool lush = (b == Biome::Forest || b == Biome::Jungle
              || b == Biome::Grassland || b == Biome::Marsh);
    if (harsh) { foot_w = 40; foot_h = 30; }
    if (lush && props.lore_tier >= 2) { foot_w = 80; foot_h = 50; }

    auto placement = scorer.score(channels, map, foot_w, foot_h);
    if (!placement.valid) return;

    // 2. Plan the settlement
    // We need a mutable copy of channels for terrain sculpting
    TerrainChannels mutable_channels = channels;

    SettlementPlanner planner;
    auto plan = planner.plan(placement, mutable_channels, map, props, rng);

    // 3. Apply terrain modifications
    apply_terrain_mods(map, mutable_channels, plan.terrain_mods);

    // 4. Generate buildings
    BuildingGenerator builder;
    for (const auto& spec : plan.buildings) {
        builder.generate(map, spec, plan.style, rng);
    }

    // 5. Route paths and build bridges
    PathRouter router;
    router.route(map, plan);

    // 6. Build perimeter (if planned)
    PerimeterBuilder perimeter;
    perimeter.build(map, plan, rng);

    // 7. Exterior decoration
    ExteriorDecorator decorator;
    decorator.decorate(map, plan, rng);
}

} // namespace astra
```

- [ ] **Step 3: Wire into v2 generator**

In `src/generators/detail_map_generator_v2.cpp`, add include at the top:

```cpp
#include "astra/poi_phase.h"
```

At the end of `DetailMapGeneratorV2::place_features()`, after the riparian lush zone block (after the closing `}` of the `if (has_lush_zone)` block, before the closing `}` of `place_features`), add:

```cpp

    // --- POI Phase (Phase 6) ---
    if (props_->detail_has_poi) {
        // Reconstruct channels for POI scoring
        // (channels were local to generate_layout, so we rebuild a lightweight version)
        TerrainChannels poi_channels(w, h);
        // Re-read tile data to infer channels from the composited map
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                Tile t = map_->get(x, y);
                if (t == Tile::Wall) poi_channels.elev(x, y) = 0.9f;
                else if (t == Tile::Water) poi_channels.moist(x, y) = 0.9f;
                else poi_channels.elev(x, y) = 0.3f;
            }
        }
        poi_phase(*map_, poi_channels, *props_, rng);
    }
```

- [ ] **Step 4: Add poi_phase.cpp to CMakeLists.txt**

Add `src/generators/poi_phase.cpp` to the source list.

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 6: Commit**

```bash
git add include/astra/poi_phase.h src/generators/poi_phase.cpp \
  src/generators/detail_map_generator_v2.cpp CMakeLists.txt
git commit -m "feat: add POI phase orchestrator and wire into v2 generator"
```

---

### Task 12: Dev Console Testing Support

Extend `biome_test` to accept a `settlement` flag for rapid iteration.

**Files:**
- Modify: `src/dev_console.cpp`
- Modify: `src/game.cpp`
- Modify: `include/astra/game.h`

- [ ] **Step 1: Update dev_command_biome_test signature**

In `include/astra/game.h`, find the existing `dev_command_biome_test` declaration and change it to:

```cpp
    void dev_command_biome_test(Biome biome, int layer, bool settlement = false);
```

- [ ] **Step 2: Update game.cpp implementation**

In `src/game.cpp`, update the function signature and add settlement support:

```cpp
void Game::dev_command_biome_test(Biome biome, int layer, bool settlement) {
    (void)layer;
    animations_.clear();
    unsigned seed = static_cast<unsigned>(std::time(nullptr));

    auto props = default_properties(MapType::DetailMap);
    props.biome = biome;
    props.width = 360;
    props.height = 150;
    props.light_bias = 100;

    if (settlement) {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_Settlement;
        props.lore_tier = 1;  // default to frontier; use 2+ for advanced/walled
    }

    world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
    auto gen = make_detail_map_generator_v2();
    gen->generate(world_.map(), props, seed);
    world_.map().set_biome(biome);

    std::string label = "[DEV] Biome Test: " + biome_profile(biome).name;
    if (settlement) label += " + Settlement";
    world_.map().set_location_name(label);

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();
```

(Keep the rest of the function as-is.)

- [ ] **Step 3: Update dev_console.cpp parsing**

In `src/dev_console.cpp`, update the `biome_test` help text:

```cpp
        log("  biome_test <biome> [settlement] - generate v2 detail map for biome");
```

Update the biome_test handler to detect the `settlement` flag:

```cpp
    else if (verb == "biome_test" && args.size() >= 2) {
        Biome biome;
        if (!parse_biome(args[1], biome)) {
            log("Unknown biome: " + args[1]);
            log("Options: grassland, forest, jungle, sandy, rocky, volcanic,");
            log("  aquatic, ice, fungal, crystal, corroded,");
            log("  alien_crystalline, alien_organic, alien_geometric,");
            log("  alien_void, alien_light, scarred_scorched, scarred_glassed");
            return;
        }
        int layer = 0;
        bool settlement = false;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "settlement") settlement = true;
            else {
                try { layer = std::stoi(args[i]); } catch (...) {
                    log("Invalid arg: " + args[i]);
                    return;
                }
            }
        }
        game.dev_command_biome_test(biome, layer, settlement);
        std::string msg = "Biome test: " + args[1] + " (360x150)";
        if (settlement) msg += " + settlement";
        log(msg);
    }
```

- [ ] **Step 4: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. Run game, open dev console, type `biome_test grassland settlement`.

- [ ] **Step 5: Commit**

```bash
git add include/astra/game.h src/game.cpp src/dev_console.cpp
git commit -m "feat: add settlement flag to biome_test dev command"
```

---

### Task 13: Renderer Support for New Fixtures

Add rendering for the new fixture types in the terminal theme.

**Files:**
- Modify: `src/terminal_theme.cpp` (fixture rendering section)

- [ ] **Step 1: Find the fixture rendering section**

Search for the fixture type switch in `terminal_theme.cpp` and add cases for the new types. The exact location depends on how the renderer dispatches fixture glyphs.

- [ ] **Step 2: Add rendering for new fixtures**

Find the fixture glyph/color resolution code and add entries for each new fixture type:

```cpp
        case FixtureType::CampStove:
            return {'o', "\xe2\x96\xa3", Color::Red, Color::Default};  // ▣ red
        case FixtureType::Lamp:
            return {'*', "\xe2\x9c\xa7", Color::Yellow, Color::Default};  // ✧ yellow
        case FixtureType::HoloLight:
            return {'*', "\xe2\x9c\xa7", Color::Cyan, Color::Default};  // ✧ cyan
        case FixtureType::Locker:
            return {'=', "\xe2\x96\x90", Color::White, Color::Default};  // ▐ white
        case FixtureType::BookCabinet:
            return {'[', "\xe2\x96\x90", static_cast<Color>(130), Color::Default};  // ▐ brown
        case FixtureType::DataTerminal:
            return {'#', "\xe2\x96\x88", Color::Cyan, Color::Default};  // █ cyan
        case FixtureType::Bench:
            return {'=', "\xe2\x94\x80", static_cast<Color>(137), Color::Default};  // ─ tan
        case FixtureType::Chair:
            return {'h', "\xe2\x94\xac", Color::White, Color::Default};  // ┬ white
        case FixtureType::Gate:
            return {'/', "\xe2\x95\x91", static_cast<Color>(137), Color::Default};  // ║ tan
        case FixtureType::BridgeRail:
            return {'|', "\xe2\x94\x82", static_cast<Color>(137), Color::Default};  // │ tan
        case FixtureType::BridgeFloor:
            return {'.', "\xe2\x94\x80", static_cast<Color>(137), Color::Default};  // ─ tan
        case FixtureType::Planter:
            return {'"', "\xe2\x99\xa3", Color::Green, Color::Default};  // ♣ green
```

Note: The exact return type and struct fields depend on how the existing fixture rendering is structured in `terminal_theme.cpp`. Match the pattern of existing fixture cases.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. New fixture types render visibly in `biome_test grassland settlement`.

- [ ] **Step 4: Commit**

```bash
git add src/terminal_theme.cpp
git commit -m "feat: add terminal rendering for settlement fixture types"
```

---

### Task 14: Preserve Channels for POI Phase

The current v2 generator creates `TerrainChannels` as a local in `generate_layout()` and discards them. The POI phase needs them. Store them as a member.

**Files:**
- Modify: `src/generators/detail_map_generator_v2.cpp`

- [ ] **Step 1: Store channels as member**

In the `DetailMapGeneratorV2` class definition at the top of `detail_map_generator_v2.cpp`, add a private member:

```cpp
    TerrainChannels channels_;
```

- [ ] **Step 2: Populate in generate_layout**

In `generate_layout()`, replace the local `TerrainChannels channels(w, h)` with:

```cpp
    channels_ = TerrainChannels(w, h);
```

And change all references from `channels` to `channels_` in `generate_layout()`.

- [ ] **Step 3: Use in place_features**

Replace the `poi_phase` call in `place_features()` that reconstructs channels from tile data with:

```cpp
    // --- POI Phase (Phase 6) ---
    if (props_->detail_has_poi) {
        poi_phase(*map_, channels_, *props_, rng);
    }
```

Remove the channel reconstruction block added in Task 11.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. Settlements now use actual terrain channel data for placement scoring.

- [ ] **Step 5: Commit**

```bash
git add src/generators/detail_map_generator_v2.cpp
git commit -m "feat: preserve terrain channels for POI phase consumption"
```

---

### Task 15: Visual Testing and Tuning

Run the settlement generation across multiple biomes and verify it looks correct.

**Files:** None new — this is a testing/tuning task.

- [ ] **Step 1: Test across biomes**

Build and run the game. Open dev console and test:

```
biome_test grassland settlement
biome_test forest settlement
biome_test rocky settlement
biome_test volcanic settlement
biome_test sandy settlement
biome_test marsh settlement
biome_test jungle settlement
biome_test ice settlement
```

For each, verify:
- Settlement is visible and findable (path from edge)
- Buildings have walls, doors, windows, interior furniture
- Paths connect buildings
- Scatter is cleared around settlement
- Lamps/lights are placed
- No crashes or visual glitches

- [ ] **Step 2: Test style selection**

Modify `dev_command_biome_test` temporarily to test different lore_tier values:

```
props.lore_tier = 0;   // → frontier style
props.lore_tier = 2;   // → advanced style, walled
props.lore_plague_origin = true;  // → ruined style
```

Verify each style looks distinct.

- [ ] **Step 3: Test water interactions**

```
biome_test marsh settlement
biome_test grassland settlement  (re-run until water is near)
```

Look for:
- Bridges over water bodies
- Waterfront anchor buildings (distillery near water)
- No buildings placed on water

- [ ] **Step 4: Fix any issues found**

Tune placement scoring weights, building sizes, path widths, and decoration density as needed.

- [ ] **Step 5: Commit fixes**

```bash
git add -u
git commit -m "fix: tune settlement generation after visual testing"
```
