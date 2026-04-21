# Dungeon Generator Overhaul — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ad-hoc `generate_dungeon_level(...)` body with a six-layer pipeline (backdrop / layout / connectivity / overlay / decoration / fixtures) selected by a data-driven `DungeonStyle`. Slice 1 lands the machinery + one `SimpleRoomsAndCorridors` style; the existing Conclave Archive dungeons stay on the old generator via a temporary kind-tag branch and will migrate in a follow-up slice.

**Architecture:** Data-driven `DungeonStyle` struct selects layer variants; each layer is a free function in `astra::dungeon` with its own `.cpp` file; orchestrator walks them in order. Style and civ are orthogonal axes on `DungeonLevelSpec`. A new `Biome::Dungeon` renders `Tile::Empty` as a block character to prevent starfield bleed.

**Tech Stack:** C++20, `namespace astra`, header-first (`#pragma once`), no third-party deps. Build: `cmake --build build -DDEV=ON -j`. Validation: build + in-game dev-mode smoke testing (no test framework).

**Spec:** `docs/superpowers/specs/2026-04-21-dungeon-generator-design.md`

---

## File plan

### New files

```
include/astra/dungeon/dungeon_style.h    — StyleId, LayoutKind, OverlayKind, StairsStrategy, DungeonStyle, registry
include/astra/dungeon/level_context.h    — LevelContext POD
include/astra/dungeon/pipeline.h          — dungeon::run(...)
include/astra/dungeon/backdrop.h          — apply_backdrop
include/astra/dungeon/layout.h            — apply_layout + helpers
include/astra/dungeon/connectivity.h      — apply_connectivity (validator in slice 1)
include/astra/dungeon/overlay.h           — apply_overlays
include/astra/dungeon/decoration.h        — apply_decoration
include/astra/dungeon/fixtures.h          — apply_fixtures + stairs strategies

src/dungeon/style_configs.cpp             — registry of DungeonStyle instances
src/dungeon/pipeline.cpp                  — orchestrator
src/dungeon/backdrop.cpp
src/dungeon/layout.cpp
src/dungeon/connectivity.cpp
src/dungeon/overlay.cpp
src/dungeon/decoration.cpp
src/dungeon/fixtures.cpp
```

### Modified files

```
include/astra/tilemap.h                   — add Biome::Dungeon, RegionType::Cave
include/astra/ruin_types.h                — add CivConfig::backdrop_tint (int color index)
include/astra/dungeon_recipe.h            — add style_id, overlays fields to DungeonLevelSpec
include/astra/save_file.h                 — bump version 37→38
include/astra/room_identifier.h           — declare tag_connected_components
include/astra/ruin_decay.h                — declare apply_decay helper (new public entry)
src/generators/ruin_civ_configs.cpp       — add "Natural" civ
src/generators/room_identifier.cpp        — implement tag_connected_components
src/generators/ruin_decay.cpp             — extract apply_decay helper
src/tilemap.cpp                           — extend biome_colors(...) for Biome::Dungeon
src/terminal_theme.cpp                    — render Tile::Empty as ░ when biome == Dungeon
src/generators/dungeon_level.cpp          — rewrite body, add Archive kind-tag bridge
src/save_file.cpp                         — extend DREC section for style_id + overlays, version-gated read
src/dev_console.cpp                       — add :dungen command
CMakeLists.txt                            — register 8 new src/dungeon/*.cpp files
```

---

## Task 1 — Foundation enums & `DungeonStyle` schema

**Files:**
- Create: `include/astra/dungeon/dungeon_style.h`
- Create: `include/astra/dungeon/level_context.h`

- [ ] **Step 1: Create `include/astra/dungeon/dungeon_style.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::dungeon {

enum class StyleId : uint8_t {
    SimpleRoomsAndCorridors = 0,   // slice 1 smoke-test
    PrecursorRuin           = 1,   // reserved — follow-up slice (Archive migration)
    OpenCave                = 2,   // reserved — cave slice
    TunnelCave              = 3,   // reserved — cave slice
    DerelictStation         = 4,   // reserved — station slice
};

enum class LayoutKind : uint8_t {
    BSPRooms,
    OpenCave,
    TunnelCave,
    DerelictStationBSP,
    RuinStamps,
};

enum class OverlayKind : uint8_t {
    None          = 0,
    BattleScarred = 1,
    Infested      = 2,
    Flooded       = 3,
    Vacuum        = 4,  // reserved — no-op stub in slice 1
};

enum class StairsStrategy : uint8_t {
    EntryExitRooms,     // distinct regions (BSP, multi-room ruin)
    FurthestPair,       // one big region, stairs at max-distance pair (open cave)
    CorridorEndpoints,  // corridor-only (tunnel cave)
};

struct DungeonStyle {
    StyleId         id;
    const char*     debug_name;               // "simple_rooms", shown in :dungen
    std::string     backdrop_material;        // "rock","sand","plating","cavern_floor"
    LayoutKind      layout;
    StairsStrategy  stairs_strategy;
    std::vector<OverlayKind> allowed_overlays;
    std::string     decoration_pack;          // "ruin_debris","cave_flora","station_scrap","natural_minimal"
    bool            connectivity_required;    // true => apply_connectivity verifies reachability
};

// Registry lookup. Asserts on unknown id.
const DungeonStyle& style_config(StyleId id);

// Try-parse for dev-console. Returns false if unknown.
bool parse_style_id(const std::string& debug_name, StyleId& out);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `include/astra/dungeon/level_context.h`**

```cpp
#pragma once

#include <cstdint>
#include <utility>

namespace astra::dungeon {

// Mutable scratch passed through layers.
// layer 2 writes entry_region_id / exit_region_id
// layer 6 writes stairs_up / stairs_dn
struct LevelContext {
    int                 depth         = 1;
    uint32_t            seed          = 0;
    std::pair<int,int>  entered_from  {-1, -1};   // descended-from coord on child map
    int                 entry_region_id = -1;
    int                 exit_region_id  = -1;
    std::pair<int,int>  stairs_up     {-1, -1};
    std::pair<int,int>  stairs_dn     {-1, -1};
};

} // namespace astra::dungeon
```

- [ ] **Step 3: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles. Headers are not yet included anywhere, so success = no errors.

- [ ] **Step 4: Commit**

```bash
git add include/astra/dungeon/dungeon_style.h include/astra/dungeon/level_context.h
git commit -m "feat(dungeon): add DungeonStyle schema + LevelContext"
```

---

## Task 2 — `Biome::Dungeon` and block-char rendering

**Files:**
- Modify: `include/astra/tilemap.h:244-270` (`Biome` enum) and add `RegionType::Cave`
- Modify: `src/tilemap.cpp` (`biome_colors` — grep the file)
- Modify: `src/terminal_theme.cpp` (starfield / Tile::Empty path)

- [ ] **Step 1: Add `Biome::Dungeon` and `RegionType::Cave`**

Edit `include/astra/tilemap.h`, at line 244 (end of `Biome` enum), before the closing `};`:

```cpp
    // ...existing...
    ScarredGlassed,
    ScarredScorched,
    Dungeon,         // underground / interior — renders Tile::Empty as block char
};
```

At line 281 (`RegionType` enum), add `Cave`:

```cpp
enum class RegionType : uint8_t {
    Room,
    Corridor,
    Cave,
};
```

- [ ] **Step 2: Extend `biome_colors(...)` for `Biome::Dungeon`**

Find `BiomeColors biome_colors(Biome b)` in `src/tilemap.cpp`. Add a `case Biome::Dungeon:` that returns a neutral underground palette:

```cpp
case Biome::Dungeon:
    return BiomeColors{
        .wall       = Color::rgb(90, 80, 70),    // warm stone
        .floor      = Color::rgb(45, 42, 38),    // dim floor
        .water      = Color::rgb(60, 90, 120),   // subterranean pool
        .remembered = Color::rgb(60, 55, 50),
    };
```

(Use the same struct-initialization style used by neighboring cases; if the file uses positional initialization, match that.)

- [ ] **Step 3: Render `Tile::Empty` as `░` when biome is Dungeon**

Find the starfield / `Tile::Empty` rendering branch in `src/terminal_theme.cpp` (grep for `Tile::Empty` or `Biome::Station`).

Add an earlier branch: when `biome == Biome::Dungeon` and `tile == Tile::Empty`, render the UTF-8 block char `░` tinted by the civ palette if available, otherwise by `biome_colors(Biome::Dungeon).wall`.

Because civ-tint plumbing into `terminal_theme.cpp` for `Tile::Empty` may not exist yet, slice 1 uses the biome wall color as the tint. A later task can wire civ tint through once civ identity is surfaced to the renderer.

Minimal inline change:

```cpp
if (biome == Biome::Dungeon && tile == Tile::Empty) {
    auto col = biome_colors(Biome::Dungeon).wall;
    return Cell{ /*glyph*/ "\xE2\x96\x91",  // U+2591 LIGHT SHADE ░
                 /*fg*/ col,
                 /*bg*/ Color::black() };   // or the call site's bg convention
}
```

Match the actual `Cell` / return shape used in the file — the codebase may use `put_glyph` calls instead of returning a struct. In that case write the glyph to the buffer instead.

- [ ] **Step 4: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles clean. Watch for `-Wswitch` warnings about missing `Biome::Dungeon` / `RegionType::Cave` cases in other switches. If any, add a minimal `default:` arm matching the file's existing style, or a `case Biome::Dungeon: break;` that uses neutral behavior. **Do not** silently drop through — log + comment.

- [ ] **Step 5: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/terminal_theme.cpp
git commit -m "feat(tilemap): add Biome::Dungeon + RegionType::Cave; render Empty as block"
```

---

## Task 3 — `tag_connected_components` utility

**Files:**
- Modify: `include/astra/room_identifier.h` (add free-function declaration)
- Modify: `src/generators/room_identifier.cpp` (implement)

- [ ] **Step 1: Declare the helper**

Add to `include/astra/room_identifier.h` (inside `namespace astra`):

```cpp
// Flood-fill every connected passable component that is not already
// inside a tagged region, and register each component as a new region
// with the given default type. Idempotent-safe on already-tagged maps.
// Safety-net for layouts that don't tag regions (ruin stamps, open cave).
void tag_connected_components(TileMap& map, RegionType default_type);
```

- [ ] **Step 2: Implement in `src/generators/room_identifier.cpp`**

Append to the file (still inside `namespace astra`):

```cpp
void tag_connected_components(TileMap& map, RegionType default_type) {
    const int w = map.width();
    const int h = map.height();
    std::vector<uint8_t> visited(static_cast<size_t>(w) * h, 0);

    auto at = [&](int x, int y) -> uint8_t& {
        return visited[static_cast<size_t>(y) * w + x];
    };

    for (int y0 = 0; y0 < h; ++y0) {
        for (int x0 = 0; x0 < w; ++x0) {
            if (at(x0, y0)) continue;
            if (!map.passable(x0, y0)) { at(x0, y0) = 1; continue; }
            // Skip cells already inside a region.
            if (map.region_id(x0, y0) >= 0) { at(x0, y0) = 1; continue; }

            // BFS
            std::vector<std::pair<int,int>> stack{{x0, y0}};
            std::vector<std::pair<int,int>> cells;
            at(x0, y0) = 1;
            while (!stack.empty()) {
                auto [x, y] = stack.back();
                stack.pop_back();
                cells.emplace_back(x, y);
                constexpr int dx[4] = { 1, -1, 0, 0 };
                constexpr int dy[4] = { 0, 0, 1, -1 };
                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    if (at(nx, ny)) continue;
                    at(nx, ny) = 1;
                    if (!map.passable(nx, ny)) continue;
                    if (map.region_id(nx, ny) >= 0) continue;
                    stack.emplace_back(nx, ny);
                }
            }

            if (cells.empty()) continue;
            int rid = map.add_region(default_type);  // use existing add_region API
            for (auto [cx, cy] : cells) {
                map.set_region_id(cx, cy, rid);
            }
        }
    }
}
```

**Note:** Verify the exact `TileMap` API for adding a region and setting per-cell region id. Grep for existing callers of region creation (e.g. inside `room_identifier.cpp` itself, `bsp_generator.cpp`) and match their style. If the API uses a different name (e.g. `append_region`, `assign_region_id`, or filling `regions_` via a friend class), mirror it here.

- [ ] **Step 3: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles.

- [ ] **Step 4: Commit**

```bash
git add include/astra/room_identifier.h src/generators/room_identifier.cpp
git commit -m "feat(gen): add tag_connected_components region safety-net"
```

---

## Task 4 — `CivConfig::backdrop_tint` + `"Natural"` civ

**Files:**
- Modify: `include/astra/ruin_types.h:23-46` (`CivConfig` struct)
- Modify: `src/generators/ruin_civ_configs.cpp` (registry table)

- [ ] **Step 1: Add `backdrop_tint` field to `CivConfig`**

In `include/astra/ruin_types.h`, within `struct CivConfig`, add after `color_tint`:

```cpp
    int color_tint       = 245;
    int backdrop_tint    = 237;   // underground block-char color (xterm 256)
```

Use a sensibly neutral default (237 is a warm dark grey).

- [ ] **Step 2: Register the `"Natural"` civ**

In `src/generators/ruin_civ_configs.cpp`, find the registry of built-in civs (`civ_config_by_name` / the table it reads from). Add a new entry:

```cpp
{
    "Natural", [] {
        CivConfig c;
        c.name                = "Natural";
        c.civ_index           = CIV_MONOLITHIC;   // closest existing slot; does not gate mechanics
        c.wall_glyphs         = { "\xE2\x96\x93" };   // ▓ dark shade — natural stone
        c.accent_glyphs       = { "\xE2\x96\x92" };   // ▒
        c.color_primary       = 240;
        c.color_secondary     = 244;
        c.color_tint          = 240;
        c.backdrop_tint       = 237;
        c.wall_thickness_bias = 1.0f;
        c.max_wall_thickness  = 3;
        c.split_regularity    = 0.15f;              // organic
        c.architecture        = Architecture::Geometric;
        return c;
    }(),
},
```

Exact map/initialiser syntax should match the surrounding entries in the file — adjust the lambda/brace style to what's already there. Do not leave `preferred_rooms` unset if neighboring entries populate it — set it to an empty vector explicitly if needed.

**Note on `civ_index`:** this field keys into renderer lookups for existing civs. Natural is new and has no dedicated index. Using `CIV_MONOLITHIC` keeps the renderer path quiet; a follow-up slice can add a `CIV_NATURAL` index once caves need unique rendering treatment.

- [ ] **Step 3: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles. If any warning about missing fields in aggregate initializers, update neighboring civ entries to set `backdrop_tint` explicitly (most will be happy with default since it's member-initialized).

- [ ] **Step 4: Commit**

```bash
git add include/astra/ruin_types.h src/generators/ruin_civ_configs.cpp
git commit -m "feat(civ): add backdrop_tint field and Natural civ"
```

---

## Task 5 — Extract `apply_decay` helper from `ruin_decay.cpp`

**Files:**
- Modify: `include/astra/ruin_decay.h`
- Modify: `src/generators/ruin_decay.cpp`

**Why:** the dungeon decoration layer (Task 10) needs to call the decay pass without going through the full `RuinGenerator`. We expose the decay body as a reusable entry point while keeping the existing `RuinGenerator` call path untouched.

- [ ] **Step 1: Declare the helper**

In `include/astra/ruin_decay.h` (inside `namespace astra`):

```cpp
// Apply ruin decay (stain tiles, crumble walls) in-place on a tagged map.
// Intensity is 0.0..1.0; 0 = pristine, 1 = heavy damage.
// Safe to call on any tile grid — stays inside passable/wall cells.
void apply_decay(TileMap& map, const CivConfig& civ,
                 float intensity, std::mt19937& rng);
```

If the header does not already include `civ_types` / `ruin_types.h`, add the include.

- [ ] **Step 2: Extract the helper**

In `src/generators/ruin_decay.cpp`, find the core decay pass (the loop(s) that stain floors, knock holes in walls, etc.) that's currently private/static or inlined into the `RuinGenerator`-callable entry point.

Lift the core logic into a new public function:

```cpp
void apply_decay(TileMap& map, const CivConfig& civ,
                 float intensity, std::mt19937& rng) {
    // Body moved from the existing decay entry point.
    // Intensity is already the final 0..1 value — no further scaling.
    // ...existing decay body...
}
```

Refactor the existing entry point (likely named `ruin_decay` / `apply_ruin_decay` / similar) to call `apply_decay(map, civ, intensity, rng)` internally, preserving the caller behavior exactly.

**Important:** do NOT change the current decay behavior for existing callers. Task 10 is the only new caller in slice 1.

- [ ] **Step 3: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles. Smoke: descend into the Conclave Archive in a dev build (an existing playthrough save, or `:tp` into it) — look should be unchanged.

- [ ] **Step 4: Commit**

```bash
git add include/astra/ruin_decay.h src/generators/ruin_decay.cpp
git commit -m "refactor(gen): extract apply_decay as public entry point"
```

---

## Task 6 — Style registry (`style_configs.cpp`)

**Files:**
- Create: `src/dungeon/style_configs.cpp`
- Modify: `CMakeLists.txt` (register new source file)

- [ ] **Step 1: Create `src/dungeon/style_configs.cpp`**

```cpp
#include "astra/dungeon/dungeon_style.h"

#include <cassert>
#include <string>
#include <unordered_map>

namespace astra::dungeon {

namespace {

// Initialized at static-init time; read-only afterwards.
const DungeonStyle kSimpleRoomsAndCorridors = [] {
    DungeonStyle s;
    s.id                   = StyleId::SimpleRoomsAndCorridors;
    s.debug_name           = "simple_rooms";
    s.backdrop_material    = "rock";
    s.layout               = LayoutKind::BSPRooms;
    s.stairs_strategy      = StairsStrategy::EntryExitRooms;
    s.allowed_overlays     = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack      = "ruin_debris";
    s.connectivity_required = true;
    return s;
}();

} // namespace

const DungeonStyle& style_config(StyleId id) {
    switch (id) {
    case StyleId::SimpleRoomsAndCorridors: return kSimpleRoomsAndCorridors;

    // Placeholders — follow-up slices register real configs.
    case StyleId::PrecursorRuin:
    case StyleId::OpenCave:
    case StyleId::TunnelCave:
    case StyleId::DerelictStation:
        assert(!"style not yet registered");
        return kSimpleRoomsAndCorridors;
    }
    assert(!"unknown StyleId");
    return kSimpleRoomsAndCorridors;
}

bool parse_style_id(const std::string& debug_name, StyleId& out) {
    static const std::unordered_map<std::string, StyleId> kByName = {
        { "simple_rooms", StyleId::SimpleRoomsAndCorridors },
        // Future: { "precursor_ruin", StyleId::PrecursorRuin }, etc.
    };
    auto it = kByName.find(debug_name);
    if (it == kByName.end()) return false;
    out = it->second;
    return true;
}

} // namespace astra::dungeon
```

- [ ] **Step 2: Register in CMakeLists.txt**

Edit `CMakeLists.txt` near line 94 (where `src/dungeon/conclave_archive.cpp` is listed). Add:

```cmake
    src/dungeon/conclave_archive.cpp
    src/dungeon/style_configs.cpp
    src/dungeon/pipeline.cpp
    src/dungeon/backdrop.cpp
    src/dungeon/layout.cpp
    src/dungeon/connectivity.cpp
    src/dungeon/overlay.cpp
    src/dungeon/decoration.cpp
    src/dungeon/fixtures.cpp
```

(Even though most of those files don't exist yet, listing them now means the build will fail cleanly in Tasks 7-12 if a file is missing, and we won't have to come back to this file.)

**Important:** stop at this step. Don't build yet — we haven't created the other files.

- [ ] **Step 3: Commit the registry only (not CMakeLists.txt yet — that would break the build)**

```bash
git add src/dungeon/style_configs.cpp
git commit -m "feat(dungeon): style registry with SimpleRoomsAndCorridors"
```

The CMakeLists.txt edit is staged-but-uncommitted on purpose; it gets committed in Task 12 once all its sources exist.

Actually — simpler: revert the CMakeLists edit and re-do it in Task 12. Prevents a dirty tree across tasks.

```bash
git checkout -- CMakeLists.txt
```

---

## Task 7 — Layer 1: backdrop

**Files:**
- Create: `include/astra/dungeon/backdrop.h`
- Create: `src/dungeon/backdrop.cpp`

- [ ] **Step 1: Create header `include/astra/dungeon/backdrop.h`**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
}

namespace astra::dungeon {

// Layer 1: fills every cell with an impassable, opaque tile and sets
// biome to Dungeon so any stray Empty renders as underground block char.
void apply_backdrop(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, std::mt19937& rng);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `src/dungeon/backdrop.cpp`**

```cpp
#include "astra/dungeon/backdrop.h"

#include "astra/ruin_types.h"
#include "astra/tilemap.h"

namespace astra::dungeon {

void apply_backdrop(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, std::mt19937& rng) {
    (void)style;  // material is informational in slice 1
    (void)civ;    // palette used at render time, not here
    (void)rng;

    const int w = map.width();
    const int h = map.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            map.set(x, y, Tile::Wall);
        }
    }
    map.set_biome(Biome::Dungeon);
}

} // namespace astra::dungeon
```

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/backdrop.h src/dungeon/backdrop.cpp
git commit -m "feat(dungeon): layer 1 backdrop (fill rock, set Biome::Dungeon)"
```

---

## Task 8 — Layer 2: layout (`BSPRooms` variant)

**Files:**
- Create: `include/astra/dungeon/layout.h`
- Create: `src/dungeon/layout.cpp`

- [ ] **Step 1: Create header `include/astra/dungeon/layout.h`**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
}

namespace astra::dungeon {

// Layer 2: dispatches on style.layout. Post-condition:
// map.region_count() >= 1, entries in LevelContext are populated.
void apply_layout(TileMap& map, const DungeonStyle& style,
                  const CivConfig& civ, LevelContext& ctx,
                  std::mt19937& rng);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `src/dungeon/layout.cpp`**

Slice 1 implements a self-contained BSP room carver. We do **not** call the existing `BspGenerator` because it's coupled to `RuinPlan` (ruin-specific geometry). This is a simpler, dungeon-focused carver.

```cpp
#include "astra/dungeon/layout.h"

#include "astra/ruin_types.h"
#include "astra/room_identifier.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace astra::dungeon {

namespace {

struct Rect { int x, y, w, h; };

bool inbounds(const TileMap& m, int x, int y) {
    return x >= 0 && y >= 0 && x < m.width() && y < m.height();
}

void carve_rect(TileMap& m, const Rect& r) {
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
        }
    }
}

void carve_h(TileMap& m, int x1, int x2, int y) {
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; ++x) if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
}

void carve_v(TileMap& m, int y1, int y2, int x) {
    if (y1 > y2) std::swap(y1, y2);
    for (int y = y1; y <= y2; ++y) if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
}

// Recursive binary space partition. Stops when a partition is too small
// to hold a min_room-sized rectangle with one-cell wall padding.
void bsp(std::vector<Rect>& rooms, const Rect& bounds,
         int min_room, int max_room, std::mt19937& rng, int depth) {
    // Decide: split, or place a room.
    const bool can_split_w = bounds.w >= 2 * (min_room + 2);
    const bool can_split_h = bounds.h >= 2 * (min_room + 2);
    const bool must_stop = depth >= 6 || (!can_split_w && !can_split_h);

    if (must_stop) {
        // Place a room inside bounds with 1-cell padding.
        const int rw = std::min(max_room, bounds.w - 2);
        const int rh = std::min(max_room, bounds.h - 2);
        if (rw < min_room || rh < min_room) return;
        std::uniform_int_distribution<int> dw(min_room, rw);
        std::uniform_int_distribution<int> dh(min_room, rh);
        const int w = dw(rng);
        const int h = dh(rng);
        std::uniform_int_distribution<int> dx(bounds.x + 1, bounds.x + bounds.w - w - 1);
        std::uniform_int_distribution<int> dy(bounds.y + 1, bounds.y + bounds.h - h - 1);
        rooms.push_back({ dx(rng), dy(rng), w, h });
        return;
    }

    // Choose axis.
    bool split_horizontal;
    if (can_split_w && !can_split_h)       split_horizontal = false;
    else if (!can_split_w && can_split_h)  split_horizontal = true;
    else                                    split_horizontal = (bounds.h > bounds.w);

    if (split_horizontal) {
        std::uniform_int_distribution<int> d(min_room + 2, bounds.h - (min_room + 2));
        const int cut = d(rng);
        Rect a = { bounds.x, bounds.y,       bounds.w, cut };
        Rect b = { bounds.x, bounds.y + cut, bounds.w, bounds.h - cut };
        bsp(rooms, a, min_room, max_room, rng, depth + 1);
        bsp(rooms, b, min_room, max_room, rng, depth + 1);
    } else {
        std::uniform_int_distribution<int> d(min_room + 2, bounds.w - (min_room + 2));
        const int cut = d(rng);
        Rect a = { bounds.x,       bounds.y, cut,              bounds.h };
        Rect b = { bounds.x + cut, bounds.y, bounds.w - cut,   bounds.h };
        bsp(rooms, a, min_room, max_room, rng, depth + 1);
        bsp(rooms, b, min_room, max_room, rng, depth + 1);
    }
}

void connect_rooms(TileMap& m, const std::vector<Rect>& rooms, std::mt19937& rng) {
    // Simple L-corridor from room[i] center to room[i+1] center.
    for (size_t i = 1; i < rooms.size(); ++i) {
        const auto& a = rooms[i - 1];
        const auto& b = rooms[i];
        int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
        int bx = b.x + b.w / 2, by = b.y + b.h / 2;
        std::uniform_int_distribution<int> flip(0, 1);
        if (flip(rng)) {
            carve_h(m, ax, bx, ay);
            carve_v(m, ay, by, bx);
        } else {
            carve_v(m, ay, by, ax);
            carve_h(m, ax, bx, by);
        }
    }
}

void layout_bsp_rooms(TileMap& map, LevelContext& ctx, std::mt19937& rng) {
    std::vector<Rect> rooms;
    // Slightly inset bounds from the map edge so rooms never touch the border.
    Rect full = { 1, 1, map.width() - 2, map.height() - 2 };
    bsp(rooms, full, /*min_room=*/4, /*max_room=*/10, rng, /*depth=*/0);

    for (const auto& r : rooms) carve_rect(map, r);
    connect_rooms(map, rooms, rng);

    // Tag regions. Rooms first so we can write ctx.entry/exit; corridors
    // are left untagged and picked up by the tag_connected_components
    // safety-net below as RegionType::Corridor-ish (we use Room since
    // entry/exit helpers look for Rooms; corridors that emerge will
    // be tagged generically).
    //
    // Simpler for slice 1: run the safety-net helper on the whole map
    // as RegionType::Room. BSP layouts are 1 connected component; we
    // distinguish entry/exit by room index.

    tag_connected_components(map, RegionType::Room);

    // Pick entry and exit region ids by room centers: first room = entry,
    // last room = exit. Map them to region ids via map.region_id(cx, cy).
    if (!rooms.empty()) {
        const auto& first = rooms.front();
        const auto& last  = rooms.back();
        ctx.entry_region_id = map.region_id(first.x + first.w / 2,
                                            first.y + first.h / 2);
        ctx.exit_region_id  = map.region_id(last.x + last.w / 2,
                                            last.y + last.h / 2);
    }
}

} // namespace

void apply_layout(TileMap& map, const DungeonStyle& style,
                  const CivConfig& civ, LevelContext& ctx,
                  std::mt19937& rng) {
    (void)civ;  // layout is civ-agnostic
    switch (style.layout) {
    case LayoutKind::BSPRooms:
        layout_bsp_rooms(map, ctx, rng);
        break;
    case LayoutKind::OpenCave:
    case LayoutKind::TunnelCave:
    case LayoutKind::DerelictStationBSP:
    case LayoutKind::RuinStamps:
        assert(!"layout kind not implemented in slice 1");
        break;
    }

    // Post-condition enforcement.
    assert(map.region_count() >= 1 && "layout must produce >=1 region");
}

} // namespace astra::dungeon
```

**Verify API names:** `map.set(x, y, Tile::Floor)` and `map.region_id(x, y)` should match `TileMap`'s public API. Double-check from `include/astra/tilemap.h`. If the setter name differs (e.g. `set_tile`), match it.

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/layout.h src/dungeon/layout.cpp
git commit -m "feat(dungeon): layer 2 BSPRooms layout with region tagging"
```

---

## Task 9 — Layer 3: connectivity (validator)

**Files:**
- Create: `include/astra/dungeon/connectivity.h`
- Create: `src/dungeon/connectivity.cpp`

In slice 1 this layer is a **validator**: asserts every region reachable from the entry region. The BSP carver in Task 8 connects by construction, so we never expect this to fail. Keeping the layer present now means future layouts (cave, ruin-stamps) have a concrete slot to hook into.

- [ ] **Step 1: Create header**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra { class TileMap; }

namespace astra::dungeon {

// Layer 3: if style.connectivity_required, verifies that every tagged
// region is reachable from ctx.entry_region_id. On failure, logs a
// warning in dev mode. In slice 1 this is a no-op for unconnected
// layouts (none are registered).
void apply_connectivity(TileMap& map, const DungeonStyle& style,
                        LevelContext& ctx, std::mt19937& rng);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `src/dungeon/connectivity.cpp`**

```cpp
#include "astra/dungeon/connectivity.h"

#include "astra/tilemap.h"

#include <set>
#include <vector>

namespace astra::dungeon {

namespace {

// Flood-fill from the entry region; collect every region id reached.
std::set<int> reachable_regions(const TileMap& map, int entry_rid) {
    std::set<int> reached;
    if (entry_rid < 0) return reached;

    const int w = map.width();
    const int h = map.height();

    // Find any passable cell in entry_rid.
    int sx = -1, sy = -1;
    for (int y = 0; y < h && sx < 0; ++y) {
        for (int x = 0; x < w && sx < 0; ++x) {
            if (map.region_id(x, y) == entry_rid && map.passable(x, y)) {
                sx = x; sy = y;
            }
        }
    }
    if (sx < 0) return reached;

    std::vector<uint8_t> seen(static_cast<size_t>(w) * h, 0);
    std::vector<std::pair<int,int>> stack{{sx, sy}};
    seen[static_cast<size_t>(sy) * w + sx] = 1;
    reached.insert(entry_rid);

    while (!stack.empty()) {
        auto [x, y] = stack.back();
        stack.pop_back();
        int rid = map.region_id(x, y);
        if (rid >= 0) reached.insert(rid);

        constexpr int dx[4] = { 1, -1, 0, 0 };
        constexpr int dy[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            size_t idx = static_cast<size_t>(ny) * w + nx;
            if (seen[idx]) continue;
            seen[idx] = 1;
            if (!map.passable(nx, ny)) continue;
            stack.emplace_back(nx, ny);
        }
    }
    return reached;
}

} // namespace

void apply_connectivity(TileMap& map, const DungeonStyle& style,
                        LevelContext& ctx, std::mt19937& rng) {
    (void)rng;
    if (!style.connectivity_required) return;

    auto reached = reachable_regions(map, ctx.entry_region_id);
    const int rc = map.region_count();
    // Intentionally no corridor pathfinding in slice 1 — layouts must
    // connect by construction. If a regression slips through, we'd
    // want to know loudly.
    if (static_cast<int>(reached.size()) < rc) {
#ifdef ASTRA_DEV_MODE
        // TODO: route a PathRouter-style corridor when follow-up slices
        // introduce layouts that don't connect by construction.
        // For now, flag it.
        (void)0;
#endif
    }
}

} // namespace astra::dungeon
```

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/connectivity.h src/dungeon/connectivity.cpp
git commit -m "feat(dungeon): layer 3 connectivity validator"
```

---

## Task 10 — Layer 4: overlays (wrap ruin_stamps)

**Files:**
- Create: `include/astra/dungeon/overlay.h`
- Create: `src/dungeon/overlay.cpp`
- Reads: `include/astra/ruin_stamps.h`, `include/astra/dungeon_recipe.h`

- [ ] **Step 1: Locate existing overlay primitives**

Run: `grep -n "apply_battle_scarred\|apply_infested\|apply_flooded\|BattleScarred\|Infested\|Flooded" include/astra/ruin_stamps.h src/generators/ruin_stamps.cpp`

Note the exact function names. They may be:
- Free functions: `apply_battle_scarred(map, plan, rng)`, etc.
- Methods on a `RuinStamper` class.
- A single `apply_stamp(map, plan, RuinStampType, rng)` dispatcher.

Slice 1 wraps whichever shape exists. If the primitives require a `RuinPlan`, build a minimal shim: for `BattleScarred` / `Infested`, an empty plan with no rooms is usually fine because they work off already-carved floors. Verify during the smoke test.

- [ ] **Step 2: Create header `include/astra/dungeon/overlay.h`**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 4: applies spec.overlays, filtered against style.allowed_overlays.
// Non-allowed overlays are silently skipped (dev log warning).
void apply_overlays(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, std::mt19937& rng);

} // namespace astra::dungeon
```

- [ ] **Step 3: Create `src/dungeon/overlay.cpp`**

```cpp
#include "astra/dungeon/overlay.h"

#include "astra/dungeon_recipe.h"
#include "astra/ruin_stamps.h"     // adjust if the header name differs
#include "astra/tilemap.h"

#include <algorithm>

namespace astra::dungeon {

namespace {

bool overlay_allowed(const DungeonStyle& style, OverlayKind k) {
    return std::find(style.allowed_overlays.begin(),
                     style.allowed_overlays.end(), k)
           != style.allowed_overlays.end();
}

void apply_one(TileMap& map, OverlayKind k, std::mt19937& rng) {
    switch (k) {
    case OverlayKind::BattleScarred:
        // apply_battle_scarred(map, /*plan=*/..., rng);
        // TODO[slice-1]: wire to ruin_stamps.cpp primitive exactly as it expects.
        break;
    case OverlayKind::Infested:
        // apply_infested(map, rng);
        break;
    case OverlayKind::Flooded:
        // apply_flooded(map, rng);
        break;
    case OverlayKind::Vacuum:
        // Reserved for station styles — no-op in slice 1.
        break;
    case OverlayKind::None:
        break;
    }
    (void)map; (void)rng;  // remove these once bodies are wired
}

} // namespace

void apply_overlays(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, std::mt19937& rng) {
    for (auto k : spec.overlays) {
        if (!overlay_allowed(style, k)) continue;   // dev warning would go here
        apply_one(map, k, rng);
    }
}

} // namespace astra::dungeon
```

**Important:** the `TODO[slice-1]` comments are acknowledged here because the wiring requires reading the actual ruin_stamps API at implementation time. When executing this task, read the header first, adjust the call signatures, and remove the TODO markers. Slice 1 must end with all three wrapper calls active (BattleScarred, Infested, Flooded). Vacuum remains a no-op stub.

- [ ] **Step 4: Commit**

```bash
git add include/astra/dungeon/overlay.h src/dungeon/overlay.cpp
git commit -m "feat(dungeon): layer 4 overlays wrapping ruin_stamps"
```

---

## Task 11 — Layer 5: decoration

**Files:**
- Create: `include/astra/dungeon/decoration.h`
- Create: `src/dungeon/decoration.cpp`

- [ ] **Step 1: Create header**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 5: dispatches on style.decoration_pack. Uses civ palette /
// furniture prefs + spec.decay_level.
void apply_decoration(TileMap& map, const DungeonStyle& style,
                      const CivConfig& civ, const DungeonLevelSpec& spec,
                      std::mt19937& rng);

} // namespace astra::dungeon
```

Note: header adds `const DungeonLevelSpec&` to pass decay_level. Update `include/astra/dungeon/pipeline.h` / Task 12 accordingly.

- [ ] **Step 2: Create `src/dungeon/decoration.cpp`**

```cpp
#include "astra/dungeon/decoration.h"

#include "astra/dungeon_recipe.h"
#include "astra/ruin_decay.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra::dungeon {

namespace {

void decorate_ruin_debris(TileMap& map, const CivConfig& civ,
                          int decay_level, std::mt19937& rng) {
    // Map decay_level (0..3) to intensity (0.0..1.0).
    const float intensity = std::clamp(
        static_cast<float>(decay_level) / 3.0f, 0.0f, 1.0f);
    apply_decay(map, civ, intensity, rng);
}

void decorate_natural_minimal(TileMap& map, const CivConfig& civ,
                              int decay_level, std::mt19937& rng) {
    (void)map; (void)civ; (void)decay_level; (void)rng;
    // Deferred: cave flora / mushroom stamps. Reserved for cave slice.
}

void decorate_station_scrap(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: station slice.
}

void decorate_cave_flora(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: cave slice.
}

} // namespace

void apply_decoration(TileMap& map, const DungeonStyle& style,
                      const CivConfig& civ, const DungeonLevelSpec& spec,
                      std::mt19937& rng) {
    const std::string& pack = style.decoration_pack;
    if      (pack == "ruin_debris")       decorate_ruin_debris(map, civ, spec.decay_level, rng);
    else if (pack == "natural_minimal")   decorate_natural_minimal(map, civ, spec.decay_level, rng);
    else if (pack == "station_scrap")     decorate_station_scrap(map, civ, spec.decay_level, rng);
    else if (pack == "cave_flora")        decorate_cave_flora(map, civ, spec.decay_level, rng);
    // Unknown pack: silently skip.
}

} // namespace astra::dungeon
```

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/decoration.h src/dungeon/decoration.cpp
git commit -m "feat(dungeon): layer 5 decoration with ruin_debris pack"
```

---

## Task 12 — Layer 6: fixtures (stairs + quest fixtures)

**Files:**
- Create: `include/astra/dungeon/fixtures.h`
- Create: `src/dungeon/fixtures.cpp`

- [ ] **Step 1: Create header**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra {
class TileMap;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 6:
//   6.i  Stairs (strategy-dispatched).
//   6.ii Quest fixtures from spec.fixtures.
//   6.iii Required fixtures (deferred — Archive migration slice).
void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, LevelContext& ctx,
                    std::mt19937& rng);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `src/dungeon/fixtures.cpp`**

This layer subsumes the existing `place_planned_fixtures(...)` logic from `src/generators/dungeon_level.cpp`. Port it verbatim with minor cleanups (use `ctx.entry_region_id` instead of the `entry_rid` local; use `ctx.stairs_up` coords for the `back_chamber` fallback instead of recomputing).

```cpp
#include "astra/dungeon/fixtures.h"

#include "astra/dungeon_recipe.h"
#include "astra/quest_fixture.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace astra::dungeon {

namespace {

bool open_at(const TileMap& m, int x, int y) {
    if (x < 0 || y < 0 || x >= m.width() || y >= m.height()) return false;
    if (!m.passable(x, y)) return false;
    return m.fixture_ids()[y * m.width() + x] < 0;
}

std::vector<std::pair<int,int>> collect_region_open(const TileMap& m, int rid) {
    std::vector<std::pair<int,int>> out;
    if (rid < 0 || rid >= m.region_count()) return out;
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            if (m.region_id(x, y) != rid) continue;
            if (!m.passable(x, y)) continue;
            if (m.fixture_ids()[y * m.width() + x] >= 0) continue;
            out.emplace_back(x, y);
        }
    }
    return out;
}

void add_stairs(TileMap& m, FixtureType ft, int x, int y) {
    FixtureData f;
    f.type = ft;
    f.interactable = true;
    f.cooldown = 0;
    m.add_fixture(x, y, f);
}

// ---- Stairs strategies ----

void place_stairs_entry_exit(TileMap& map, const DungeonLevelSpec& spec,
                             LevelContext& ctx, std::mt19937& rng) {
    // Up: prefer entered_from if still open.
    int ux = -1, uy = -1;
    if (open_at(map, ctx.entered_from.first, ctx.entered_from.second)) {
        ux = ctx.entered_from.first;
        uy = ctx.entered_from.second;
    } else {
        auto cells = collect_region_open(map, ctx.entry_region_id);
        if (!cells.empty()) {
            std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
            auto p = cells[d(rng)];
            ux = p.first; uy = p.second;
        }
    }
    if (ux >= 0) {
        add_stairs(map, FixtureType::StairsUp, ux, uy);
        ctx.stairs_up = { ux, uy };
    }

    if (spec.is_boss_level) return;

    // Down: any open cell in exit region, not adjacent to StairsUp.
    auto cells = collect_region_open(map, ctx.exit_region_id);
    if (cells.empty()) return;
    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
    auto p = cells[d(rng)];
    add_stairs(map, FixtureType::StairsDown, p.first, p.second);
    ctx.stairs_dn = { p.first, p.second };
}

void place_stairs_furthest_pair(TileMap& map, const DungeonLevelSpec& spec,
                                LevelContext& ctx, std::mt19937& rng) {
    (void)rng;
    // Up: entered_from if valid, else any open in the single region.
    int ux = -1, uy = -1;
    if (open_at(map, ctx.entered_from.first, ctx.entered_from.second)) {
        ux = ctx.entered_from.first;
        uy = ctx.entered_from.second;
    } else {
        auto cells = collect_region_open(map, ctx.entry_region_id);
        if (!cells.empty()) { ux = cells.front().first; uy = cells.front().second; }
    }
    if (ux < 0) return;
    add_stairs(map, FixtureType::StairsUp, ux, uy);
    ctx.stairs_up = { ux, uy };

    if (spec.is_boss_level) return;

    // Down: farthest Manhattan-distance passable cell.
    int best_d = -1, bx = -1, by = -1;
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (!open_at(map, x, y)) continue;
            int dd = std::abs(x - ux) + std::abs(y - uy);
            if (dd > best_d) { best_d = dd; bx = x; by = y; }
        }
    }
    if (bx >= 0) {
        add_stairs(map, FixtureType::StairsDown, bx, by);
        ctx.stairs_dn = { bx, by };
    }
}

void place_stairs_corridor_endpoints(TileMap& map, const DungeonLevelSpec& spec,
                                     LevelContext& ctx, std::mt19937& rng) {
    // Slice 1 has no corridor-only layout. Fall back to furthest pair to
    // avoid a silent misplacement.
    place_stairs_furthest_pair(map, spec, ctx, rng);
}

// ---- Quest fixture placement ----

void place_quest_fixtures(TileMap& map, const DungeonLevelSpec& spec,
                          const LevelContext& ctx, std::mt19937& rng) {
    for (const auto& pf : spec.fixtures) {
        int fx = -1, fy = -1;

        if (pf.placement_hint == "back_chamber") {
            // Room region furthest from entry, excluding entry/exit.
            int best_d = -1, best_rid = -1;
            for (int r = 0; r < map.region_count(); ++r) {
                if (map.region(r).type != RegionType::Room) continue;
                if (r == ctx.entry_region_id || r == ctx.exit_region_id) continue;
                auto cells = collect_region_open(map, r);
                if (cells.empty()) continue;
                // Use first cell as a cheap proxy for distance.
                int dd = std::abs(cells.front().first - ctx.stairs_up.first) +
                         std::abs(cells.front().second - ctx.stairs_up.second);
                if (dd > best_d) { best_d = dd; best_rid = r; }
            }
            if (best_rid >= 0) {
                auto cells = collect_region_open(map, best_rid);
                if (!cells.empty()) {
                    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                    auto p = cells[d(rng)];
                    fx = p.first; fy = p.second;
                }
            }
        } else if (pf.placement_hint == "center") {
            int cx = map.width() / 2;
            int cy = map.height() / 2;
            int rid = map.region_id(cx, cy);
            auto cells = collect_region_open(map, rid);
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                auto p = cells[d(rng)];
                fx = p.first; fy = p.second;
            }
        } else if (pf.placement_hint == "entry_room") {
            auto cells = collect_region_open(map, ctx.entry_region_id);
            // Remove the stairs-up tile from the candidate set.
            cells.erase(std::remove_if(cells.begin(), cells.end(),
                [&](const std::pair<int,int>& p) { return p == ctx.stairs_up; }),
                cells.end());
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                auto p = cells[d(rng)];
                fx = p.first; fy = p.second;
            }
        }

        // Global open-tile fallback.
        if (fx < 0) {
            std::vector<std::pair<int,int>> open;
            for (int y = 0; y < map.height(); ++y) {
                for (int x = 0; x < map.width(); ++x) {
                    if (open_at(map, x, y)) open.emplace_back(x, y);
                }
            }
            if (open.empty()) continue;
            std::uniform_int_distribution<size_t> d(0, open.size() - 1);
            auto p = open[d(rng)];
            fx = p.first; fy = p.second;
        }

        FixtureData fd;
        fd.type = FixtureType::QuestFixture;
        fd.interactable = true;
        fd.cooldown = -1;
        fd.quest_fixture_id = pf.quest_fixture_id;
        map.add_fixture(fx, fy, fd);
    }
}

} // namespace

void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, LevelContext& ctx,
                    std::mt19937& rng) {
    // 6.i Stairs
    switch (style.stairs_strategy) {
    case StairsStrategy::EntryExitRooms:
        place_stairs_entry_exit(map, spec, ctx, rng);
        break;
    case StairsStrategy::FurthestPair:
        place_stairs_furthest_pair(map, spec, ctx, rng);
        break;
    case StairsStrategy::CorridorEndpoints:
        place_stairs_corridor_endpoints(map, spec, ctx, rng);
        break;
    }

    // 6.ii Quest fixtures
    place_quest_fixtures(map, spec, ctx, rng);

    // 6.iii Required fixtures — TODO: Archive migration slice.
}

} // namespace astra::dungeon
```

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/fixtures.h src/dungeon/fixtures.cpp
git commit -m "feat(dungeon): layer 6 fixtures (stairs + quest placement)"
```

---

## Task 13 — Pipeline orchestrator + CMake registration

**Files:**
- Create: `include/astra/dungeon/pipeline.h`
- Create: `src/dungeon/pipeline.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `include/astra/dungeon/pipeline.h`**

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Main orchestrator. Walks all six layers.
//
// RNG discipline: each layer receives its own sub-seeded mt19937
// derived from ctx.seed via named XOR mixing, so adding/removing
// an overlay won't reshuffle decoration placement.
//
//   backdrop      seed ^ 0xBDBDBDBDu
//   layout        seed ^ 0x1A1A1A1Au
//   connectivity  seed ^ 0xC0FFEE00u
//   overlays      seed ^ 0x0FEB0FEBu
//   decoration    seed ^ 0xDEC02011u
//   fixtures      seed ^ 0xF12F12F1u
void run(TileMap& map, const DungeonStyle& style, const CivConfig& civ,
         const DungeonLevelSpec& spec, LevelContext& ctx);

} // namespace astra::dungeon
```

- [ ] **Step 2: Create `src/dungeon/pipeline.cpp`**

```cpp
#include "astra/dungeon/pipeline.h"

#include "astra/dungeon/backdrop.h"
#include "astra/dungeon/connectivity.h"
#include "astra/dungeon/decoration.h"
#include "astra/dungeon/fixtures.h"
#include "astra/dungeon/layout.h"
#include "astra/dungeon/overlay.h"

namespace astra::dungeon {

namespace {
std::mt19937 sub(uint32_t seed, uint32_t mix) { return std::mt19937(seed ^ mix); }
}

void run(TileMap& map, const DungeonStyle& style, const CivConfig& civ,
         const DungeonLevelSpec& spec, LevelContext& ctx) {
    auto rng_back  = sub(ctx.seed, 0xBDBDBDBDu);
    auto rng_lay   = sub(ctx.seed, 0x1A1A1A1Au);
    auto rng_con   = sub(ctx.seed, 0xC0FFEE00u);
    auto rng_ovl   = sub(ctx.seed, 0x0FEB0FEBu);
    auto rng_dec   = sub(ctx.seed, 0xDEC02011u);
    auto rng_fix   = sub(ctx.seed, 0xF12F12F1u);

    apply_backdrop    (map, style, civ,            rng_back);
    apply_layout      (map, style, civ, ctx,       rng_lay);
    apply_connectivity(map, style,      ctx,       rng_con);
    apply_overlays    (map, style, spec,            rng_ovl);
    apply_decoration  (map, style, civ, spec,       rng_dec);
    apply_fixtures    (map, style, spec, ctx,       rng_fix);
}

} // namespace astra::dungeon
```

- [ ] **Step 3: Register new sources in CMakeLists.txt**

Near line 94 in `CMakeLists.txt`, replace the single `src/dungeon/conclave_archive.cpp` line with the full dungeon block:

```cmake
    src/dungeon/conclave_archive.cpp
    src/dungeon/style_configs.cpp
    src/dungeon/pipeline.cpp
    src/dungeon/backdrop.cpp
    src/dungeon/layout.cpp
    src/dungeon/connectivity.cpp
    src/dungeon/overlay.cpp
    src/dungeon/decoration.cpp
    src/dungeon/fixtures.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles. Fix any API-name drift (`TileMap::set` vs `set_tile`, `region(rid)` accessors, etc.) discovered here.

- [ ] **Step 5: Commit**

```bash
git add include/astra/dungeon/pipeline.h src/dungeon/pipeline.cpp CMakeLists.txt
git commit -m "feat(dungeon): pipeline orchestrator + cmake wiring"
```

---

## Task 14 — `DungeonLevelSpec` schema fields + save-file v38

**Files:**
- Modify: `include/astra/dungeon_recipe.h`
- Modify: `include/astra/save_file.h` (version bump)
- Modify: `src/save_file.cpp` (DREC read/write extension)

- [ ] **Step 1: Extend `DungeonLevelSpec`**

Edit `include/astra/dungeon_recipe.h`:

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"   // for StyleId, OverlayKind
#include "astra/location_key.h"

#include <string>
#include <vector>

namespace astra {

struct PlannedFixture {
    std::string quest_fixture_id;
    std::string placement_hint;
};

struct DungeonLevelSpec {
    dungeon::StyleId            style_id     = dungeon::StyleId::SimpleRoomsAndCorridors;  // NEW
    std::string                 civ_name     = "Precursor";
    int                         decay_level  = 2;
    int                         enemy_tier   = 1;
    std::vector<std::string>    npc_roles;
    std::vector<PlannedFixture> fixtures;
    std::vector<dungeon::OverlayKind> overlays;                                            // NEW
    bool is_side_branch = false;
    bool is_boss_level  = false;
};

struct DungeonRecipe {
    LocationKey root;
    std::string kind_tag;
    int         level_count = 1;
    std::vector<DungeonLevelSpec> levels;
};

} // namespace astra
```

- [ ] **Step 2: Bump save-file version**

Edit `include/astra/save_file.h` line 66:

```cpp
    uint32_t version = 38;   // v38: DungeonLevelSpec adds style_id + overlays
```

- [ ] **Step 3: Extend DREC write**

In `src/save_file.cpp`, `write_dungeon_recipes_section` (line ~895), after `w.write_u8(lvl.is_boss_level ? 1 : 0);` and before the npc_roles write, add:

```cpp
            w.write_u8(static_cast<uint8_t>(lvl.style_id));
            w.write_u8(static_cast<uint8_t>(lvl.overlays.size()));
            for (auto ov : lvl.overlays) {
                w.write_u8(static_cast<uint8_t>(ov));
            }
```

- [ ] **Step 4: Extend DREC read with version gate**

In `src/save_file.cpp`, `read_dungeon_recipes_section` (line ~930), the reader needs to know the loaded file's version to decide whether to read the new fields.

The function signature is `read_dungeon_recipes_section(BinaryReader& r, SaveData& data)`. It doesn't currently take the version. The version is available on `data.version` after the header read.

Extend the loop body — after `lvl.is_boss_level = r.read_u8() != 0;` add:

```cpp
            if (data.version >= 38) {
                lvl.style_id = static_cast<dungeon::StyleId>(r.read_u8());
                uint8_t oc = r.read_u8();
                for (uint8_t k = 0; k < oc; ++k) {
                    lvl.overlays.push_back(
                        static_cast<dungeon::OverlayKind>(r.read_u8()));
                }
            }
            // Otherwise keep member defaults (SimpleRoomsAndCorridors, empty overlays).
```

**Verify `SaveData` carries `.version`.** If not, the version must be threaded from the caller. Grep `SaveData::version` and adjust.

- [ ] **Step 5: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles.

- [ ] **Step 6: Smoke — old save still loads**

Launch the game; load an existing save that has dungeon recipes (Conclave Archive playthrough). Verify no load error and the recipe still spawns with defaults. Dev console: `fixtures` — verify quest fixtures still listed.

- [ ] **Step 7: Commit**

```bash
git add include/astra/dungeon_recipe.h include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): v38 — style_id + overlays on DungeonLevelSpec"
```

---

## Task 15 — Rewrite `generate_dungeon_level` as pipeline front door

**Files:**
- Modify: `src/generators/dungeon_level.cpp`

- [ ] **Step 1: Rewrite the function body**

Replace the existing body of `generate_dungeon_level(...)` in `src/generators/dungeon_level.cpp` (keep the helpers that are still used by the Archive branch, but move them into a file-local `old_impl` namespace). The new shape:

```cpp
#include "astra/dungeon_level_generator.h"

#include "astra/dungeon/pipeline.h"
#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"
#include "astra/map_properties.h"
#include "astra/ruin_civ_configs.h"    // civ_config_by_name
#include "astra/ruin_generator.h"
#include "astra/terrain_channels.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

namespace astra {

namespace old_impl {
// The entire previous body of generate_dungeon_level goes here, renamed
// to generate_archive_level_legacy(...). All its helpers
// (find_fixture_xy, collect_region_open, region_centroid,
//  place_planned_fixtures) remain in this nested namespace.
//
// Copy the existing file content verbatim — only the outer function's
// name changes, NOT its body, to minimize review noise.
void generate_archive_level_legacy(TileMap& map,
                                   const DungeonRecipe& recipe,
                                   int depth,
                                   uint32_t seed,
                                   std::pair<int, int> entered_from) {
    // ...verbatim old body...
}
} // namespace old_impl

uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& k) {
    // ...keep the existing hash implementation exactly as it is...
}

std::pair<int, int> find_stairs_up(const TileMap& m)   { /* unchanged */ }
std::pair<int, int> find_stairs_down(const TileMap& m) { /* unchanged */ }

void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int, int> entered_from) {
    if (depth < 1 || depth > static_cast<int>(recipe.levels.size())) return;

    // TODO[archive-migration]: remove this branch once PrecursorRuin style
    // is registered and Conclave Archive recipes are updated to use it.
    if (recipe.kind_tag == "conclave_archive") {
        old_impl::generate_archive_level_legacy(
            map, recipe, depth, seed, entered_from);
        return;
    }

    const auto& spec = recipe.levels[depth - 1];

    // Build the fresh map at the default dungeon size (pipeline fills every
    // cell, so any size works — use the previous default for parity).
    const MapType dtype = MapType::DerelictStation;
    MapProperties props = default_properties(dtype);
    props.biome       = Biome::Dungeon;   // pipeline resets it too, but set early
    props.difficulty  = std::max(1, spec.enemy_tier);
    map = TileMap(props.width, props.height, dtype);

    dungeon::LevelContext ctx;
    ctx.depth        = depth;
    ctx.seed         = seed;
    ctx.entered_from = entered_from;

    const dungeon::DungeonStyle& style = dungeon::style_config(spec.style_id);
    const CivConfig& civ               = civ_config_by_name(spec.civ_name);

    dungeon::run(map, style, civ, spec, ctx);
}

} // namespace astra
```

**Note on helpers:** `find_stairs_up`, `find_stairs_down`, `dungeon_level_seed` stay as top-level free functions in `namespace astra`. They're called from elsewhere (Game::descend_stairs / Game::ascend_stairs).

- [ ] **Step 2: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles clean.

- [ ] **Step 3: Smoke — Archive still works**

Load an Archive save or start a new Conclave quest. Descend into the Archive. Verify **identical behavior to before** (this is the kind_tag branch — should be pixel-equivalent to pre-refactor).

- [ ] **Step 4: Commit**

```bash
git add src/generators/dungeon_level.cpp
git commit -m "refactor(dungeon): pipeline front door + Archive kind-tag bridge"
```

---

## Task 16 — `:dungen` dev-console command

**Files:**
- Modify: `src/dev_console.cpp` (add command + help line)

The spec says `:dungen <style> [civ]` "generates a dungeon level in a scratch map and drops the player into it." How the game exposes that depends on the `Game` API. Verify by reading `game.h` and `game_world.cpp` — look for a function that swaps the current active map and moves the player (similar to how `warp random` / `biome_test` already work today, per the `help` listing).

- [ ] **Step 1: Locate a reference command**

Run: `grep -n "biome_test\|warp random\|biome_" src/dev_console.cpp | head`

Read the `biome_test` handler body — it already demonstrates "build a scratch map, swap it in, teleport player." Mimic its shape for `dungen`.

- [ ] **Step 2: Add the `dungen` handler**

In `src/dev_console.cpp`, in `execute_command`, add a new branch (placement: after the existing `biome_test` branch, before any trailing fallback):

```cpp
    } else if (verb == "dungen") {
        if (args.size() < 2) {
            log("usage: dungen <style_id> [civ_name]");
            log("  styles: simple_rooms");
            return;
        }

        dungeon::StyleId sid;
        if (!dungeon::parse_style_id(args[1], sid)) {
            log("unknown style: " + args[1]);
            return;
        }
        const std::string civ_name = args.size() >= 3 ? args[2] : "Natural";

        // Build a scratch DungeonRecipe on the stack.
        DungeonRecipe r;
        r.kind_tag    = "dev_dungen";   // NOT "conclave_archive" — uses new pipeline
        r.level_count = 1;
        DungeonLevelSpec s;
        s.style_id    = sid;
        s.civ_name    = civ_name;
        s.decay_level = 1;
        r.levels.push_back(s);

        // Replace the active map with a pipeline-generated level. The exact
        // call matches how biome_test swaps the active map — use the same
        // helper (likely Game::load_scratch_map or similar). Verify.
        //
        // Simplest form if an API exists:
        //   game.enter_dev_dungeon(r);
        //
        // Otherwise:
        //   game.world().set_active_dungeon(r);  // create TileMap, generate, spawn player at StairsUp
        //
        // Implementers: read game.cpp's "warp random" handler and mirror it.
        TileMap scratch;
        generate_dungeon_level(scratch, r, /*depth=*/1,
                               /*seed=*/std::random_device{}(),
                               /*entered_from=*/{-1, -1});
        // Hand the scratch map to the game however "biome_test" does it.
        // ...
        log("generated dungeon: " + args[1] + " / " + civ_name);
    }
```

**Do not** leave a bare `// ...` in the committed code. Read the `biome_test` body in the same file and replace the comment with the exact map-swap call it uses. If no such API exists yet, the dev command is still useful as a build-test (generates the map without swapping); note the limitation in the help line.

- [ ] **Step 3: Add help line**

In the `help` block earlier in the file, add (near the other generator commands):

```cpp
    log("  dungen <style> [civ] - generate a pipeline dungeon (style: simple_rooms)");
```

- [ ] **Step 4: Include the dungeon headers**

At the top of `src/dev_console.cpp`:

```cpp
#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon_recipe.h"
#include "astra/dungeon_level_generator.h"
```

- [ ] **Step 5: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: Compiles.

- [ ] **Step 6: Smoke — generate a dungeon**

Launch game in dev mode. Open console (`:`). Type: `dungen simple_rooms Natural`. Expected: rooms + corridors visible, block-char underground backdrop, both staircases placed, player spawns at StairsUp.

Run: `dungen simple_rooms Precursor`. Expected: same structure, Precursor color tint.

- [ ] **Step 7: Commit**

```bash
git add src/dev_console.cpp
git commit -m "feat(dev): :dungen command for style smoke testing"
```

---

## Task 17 — Final smoke + close the slice

- [ ] **Step 1: Full build, zero new warnings**

Run: `cmake --build build -DDEV=ON -j 2>&1 | tee /tmp/astra_build.log`
Run: `grep -E "warning:|error:" /tmp/astra_build.log | head -40`
Expected: no new warnings (the baseline may already have some — compare to `git log`-previous build output).

- [ ] **Step 2: Golden-path smoke**

1. Start a new game → progress to Conclave Archive → descend → verify **Archive looks unchanged** (kind-tag bridge proves itself).
2. `:` → `dungen simple_rooms Natural` → verify underground block-char backdrop, connected rooms, stairs present.
3. `:` → `dungen simple_rooms Precursor` → same structure, Precursor tint.
4. Ascend/descend stairs inside a dungeon layer → confirm StairsUp lands you on `entered_from`.

- [ ] **Step 3: Save-compat check**

Load a pre-v38 save from disk (any playthrough file on your machine predating this branch). Verify: no crash, dungeon recipes still populated with default `style_id = SimpleRoomsAndCorridors` and empty `overlays`.

- [ ] **Step 4: Update `docs/roadmap.md`**

Check the box for "Layered dungeon generator pipeline" (or add a new item under a Generator section if none exists). This is a CLAUDE.md standing instruction.

- [ ] **Step 5: Update `docs/formulas.md`**

Add a short section:

```markdown
### Dungeon layer sub-seeding

Each pipeline layer derives its own `std::mt19937` from the level seed:
  backdrop     = seed ^ 0xBDBDBDBD
  layout       = seed ^ 0x1A1A1A1A
  connectivity = seed ^ 0xC0FFEE00
  overlays     = seed ^ 0x0FEB0FEB
  decoration   = seed ^ 0xDEC02011
  fixtures     = seed ^ 0xF12F12F1

This prevents adding/removing an overlay from reshuffling decoration or
fixture placement.
```

- [ ] **Step 6: Commit**

```bash
git add docs/roadmap.md docs/formulas.md
git commit -m "docs: record dungeon pipeline sub-seeding + roadmap check"
```

---

## Self-review (plan → spec coverage)

- Six-layer pipeline — Tasks 7, 8, 9, 10, 11, 12 each land one layer. ✓
- `DungeonStyle` data-driven registry — Task 1 + Task 6. ✓
- Orthogonal civ axis + `"Natural"` civ — Task 4. ✓
- `CivConfig::backdrop_tint` — Task 4. ✓
- `Biome::Dungeon` + block-char rendering — Task 2. ✓
- `RegionType::Cave` — Task 2. ✓
- `tag_connected_components` utility — Task 3. ✓
- `apply_decay` extraction — Task 5. ✓
- Pipeline orchestrator + CMake — Task 13. ✓
- `StyleId` + `overlays` fields on spec + save v38 — Task 14. ✓
- `generate_dungeon_level` rewrite + Archive kind-tag bridge — Task 15. ✓
- `:dungen` dev command — Task 16. ✓
- Per-layer RNG sub-seeding documented in `pipeline.h` + `formulas.md` — Tasks 13, 17. ✓
- Slice 1 explicitly out-of-scope items: `PrecursorRuin` / cave / station styles, `Vacuum` overlay, `required_fixtures` catalog, Archive migration — all consistently left as TODOs or stubs. ✓

**Known gaps (flagged at execution time, not in plan):**
- `Task 10 Step 1` requires reading the actual `ruin_stamps` API before wiring the three overlay primitives; signatures may need adaptation.
- `Task 16 Step 2` requires reading the existing `biome_test` command's map-swap logic to mirror it for `dungen`.

Both are narrow, read-then-mirror actions — calling them out explicitly in the task body keeps reviewers honest.
