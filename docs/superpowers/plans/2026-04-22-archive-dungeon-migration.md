# Archive Dungeon Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the Conclave Archive off the legacy `RuinGenerator` body onto the new layered dungeon pipeline, redesign its three levels as a sealed Precursor vault, and land pipeline layer 6.iii (style-required fixtures) as a first-class feature.

**Architecture:** Register `StyleId::PrecursorRuin` with a new `LayoutKind::PrecursorVault` carver that dispatches per-depth into three authored topologies (fractured entry, nave+chapels, antechamber+vault). Add a `required_fixtures` catalog to `DungeonStyle` (six `FixtureKind` values × five `PlacementSlot` resolvers) placed in layer 6.iii before quest-fixture resolution. Quest fixtures gain a `"required_plinth"` placement hint that resolves against style-placed plinth locations. Delete the legacy `old_impl::` namespace and `kind_tag == "conclave_archive"` bridge. Bump `SAVE_FILE_VERSION` to 39 and reject older saves — no backcompat per policy.

**Tech Stack:** C++20, CMake, existing Astra pipeline (`src/dungeon/*.cpp`, `include/astra/dungeon/*.h`). No new third-party deps. No test framework — validation is `cmake --build build -DDEV=ON -j` plus `:dungen precursor_ruin Precursor` and full Archive quest playthrough.

**Validation model.** Each task ends with a build step (`cmake --build build -DDEV=ON -j`) and, where meaningful, a dev-console smoke test. Commit after each task. If the build warns, fix in-task.

**Spec:** `docs/superpowers/specs/2026-04-22-archive-dungeon-migration-design.md`.
**Kickoff context:** `docs/plans/2026-04-21-archive-dungeon-migration-kickoff.md`.

---

## File Structure

**New files:**
- *(none — all changes extend existing pipeline files.)*

**Modified files:**
- `include/astra/dungeon/dungeon_style.h` — add `FixtureKind`, `PlacementSlot`, `RequiredFixture`, `IntRange`, depth-mask helpers, `DungeonStyle::required_fixtures` field.
- `include/astra/dungeon/level_context.h` — add chamber-tag fields and `placed_required_fixtures` map.
- `include/astra/dungeon/layout.h` — no API change; document `PrecursorVault` contract.
- `src/dungeon/layout.cpp` — add `layout_precursor_vault(...)` dispatch + three per-depth carvers; wire into `apply_layout` switch.
- `src/dungeon/fixtures.cpp` — implement layer 6.iii (`place_required_fixtures`), call it before `place_quest_fixtures`, add `"required_plinth"` placement hint.
- `src/dungeon/decoration.cpp` — add `precursor_vault` pack handler.
- `src/dungeon/style_configs.cpp` — register `PrecursorRuin` style; extend `parse_style_id`.
- `include/astra/tilemap.h` — add new `FixtureType` values (`Plinth`, `Altar`, `Inscription`, `Pillar`, `Brazier`).
- `include/astra/ruin_types.h` — add `CivConfig::inscription_text_pool`.
- `src/generators/ruin_civ_configs.cpp` — populate Precursor inscription pool.
- `include/astra/dungeon/conclave_archive.h` — no API change.
- `src/dungeon/conclave_archive.cpp` — set `style_id`, add `overlays`, drop flavor inscription fixtures, use `"required_plinth"` hint.
- `src/generators/dungeon_level.cpp` — delete `kind_tag == "conclave_archive"` branch + `old_impl::` namespace.
- `include/astra/save_file.h` — bump version 38 → 39.
- `src/save_file.cpp` — reject saves at version < 39 with clear error.
- `src/renderer/*` (terminal renderer fixture rendering) — add glyph/color mappings for new `FixtureType` values.
- `docs/roadmap.md` — check off Archive migration item.

**Files to touch only to verify no breakage:**
- `include/astra/dungeon/pipeline.h`, `include/astra/dungeon/fixtures.h`, `include/astra/dungeon/decoration.h` — API should remain stable.

---

## Task 1: Extend `DungeonStyle` with `required_fixtures` types

**Files:**
- Modify: `include/astra/dungeon/dungeon_style.h`

- [ ] **Step 1: Add `FixtureKind`, `PlacementSlot`, `IntRange`, `RequiredFixture`, depth-mask helpers, and `DungeonStyle::required_fixtures` field.**

Replace the contents of `include/astra/dungeon/dungeon_style.h` with:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra::dungeon {

enum class StyleId : uint8_t {
    SimpleRoomsAndCorridors = 0,
    PrecursorRuin           = 1,
    OpenCave                = 2,
    TunnelCave              = 3,
    DerelictStation         = 4,
};

enum class LayoutKind : uint8_t {
    BSPRooms,
    OpenCave,
    TunnelCave,
    DerelictStationBSP,
    RuinStamps,
    PrecursorVault,     // authored per-depth topology (Archive migration)
};

enum class OverlayKind : uint8_t {
    None          = 0,
    BattleScarred = 1,
    Infested      = 2,
    Flooded       = 3,
    Vacuum        = 4,
};

enum class StairsStrategy : uint8_t {
    EntryExitRooms,
    FurthestPair,
    CorridorEndpoints,
};

// Layer 6.iii catalog (Archive migration).
enum class FixtureKind : uint8_t {
    Plinth,
    Altar,
    Inscription,
    Pillar,
    ResonancePillar,
    Brazier,
};

enum class PlacementSlot : uint8_t {
    SanctumCenter,    // center of the single terminal chamber
    ChapelCenter,     // centers of chapel-tagged rooms
    EachRoomOnce,     // one per non-terminal room
    WallAttached,     // attached to an interior wall of any room
    FlankPair,        // two copies flanking the nearest previously-placed target
};

struct IntRange { int min; int max; };

struct RequiredFixture {
    FixtureKind   kind;
    PlacementSlot where;
    IntRange      count;
    uint32_t      depth_mask;   // bit 0 = depth 1, bit 1 = depth 2, ...
};

// Helpers for authoring depth_mask entries.
constexpr uint32_t depth_mask_bit(int depth) {
    return (depth >= 1 && depth <= 32) ? (1u << (depth - 1)) : 0u;
}
constexpr uint32_t depth_mask_all(int max_depth) {
    uint32_t m = 0;
    for (int d = 1; d <= max_depth; ++d) m |= depth_mask_bit(d);
    return m;
}

struct DungeonStyle {
    StyleId                      id;
    const char*                  debug_name;
    std::string                  backdrop_material;
    LayoutKind                   layout;
    StairsStrategy               stairs_strategy;
    std::vector<OverlayKind>     allowed_overlays;
    std::string                  decoration_pack;
    bool                         connectivity_required;
    std::vector<RequiredFixture> required_fixtures;   // layer 6.iii catalog
};

const DungeonStyle& style_config(StyleId id);
bool parse_style_id(const std::string& debug_name, StyleId& out);

} // namespace astra::dungeon
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: Build succeeds. Existing `DungeonStyle` initializers in `src/dungeon/style_configs.cpp` still compile because `required_fixtures` defaults to an empty `std::vector`.

- [ ] **Step 3: Commit.**

```bash
git add include/astra/dungeon/dungeon_style.h
git commit -m "feat(dungeon): add required_fixtures catalog types to DungeonStyle"
```

---

## Task 2: Extend `LevelContext` with chamber tags and placed-fixtures map

**Files:**
- Modify: `include/astra/dungeon/level_context.h`

- [ ] **Step 1: Add chamber-tag fields and placed-required-fixtures map.**

Replace contents:

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra::dungeon {

// Mutable scratch passed through layers.
// - layer 2 writes entry/exit region ids, sanctum_region_id, chapel_region_ids, rooms.
// - layer 6.iii writes placed_required_fixtures.
// - layer 6.i/6.ii read placed_required_fixtures.
struct LevelContext {
    int                              depth             = 1;
    uint32_t                         seed              = 0;
    std::pair<int,int>               entered_from      {-1, -1};
    int                              entry_region_id   = -1;
    int                              exit_region_id    = -1;
    std::pair<int,int>               stairs_up         {-1, -1};
    std::pair<int,int>               stairs_dn         {-1, -1};

    // Layout chamber tagging (PrecursorVault populates these).
    int                              sanctum_region_id = -1;       // terminal chamber
    std::vector<int>                 chapel_region_ids;            // symmetric side chapels

    // Layer 6.iii output: fixture-kind -> list of placed tile positions.
    std::unordered_map<int, std::vector<std::pair<int,int>>>
                                     placed_required_fixtures;
};

inline int kind_key(FixtureKind k) { return static_cast<int>(k); }

} // namespace astra::dungeon
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: Build succeeds. `#include` additions in `level_context.h` pull `dungeon_style.h` into any translation unit that included only `level_context.h`; check `src/dungeon/pipeline.cpp` still builds.

- [ ] **Step 3: Commit.**

```bash
git add include/astra/dungeon/level_context.h
git commit -m "feat(dungeon): extend LevelContext with chamber tags + placed fixtures map"
```

---

## Task 3: Add new `FixtureType` values + terminal renderer glyphs

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: terminal renderer fixture-rendering table (locate with grep in Step 0)

- [ ] **Step 0: Locate fixture glyph/color mapping.**

Run: `grep -rn "FixtureType::ResonancePillar" src/ include/ | head -20`
The mapping used by the terminal renderer is the authoritative place to add new glyph/color pairs. Record the exact file/function for Step 2.

- [ ] **Step 1: Add five new `FixtureType` entries in `include/astra/tilemap.h`.**

Insert into the decorative block near line 335 (right after `ResonancePillar`), keeping the existing entries intact:

```cpp
    ResonancePillar,// '~'  — Precursor resonance pillar (cyan, decorative)
    Plinth,         // 'T'  — Precursor stone pedestal (impassable, hosts quest fixture)
    Altar,          // '⊤'  — Precursor altar (impassable, interactable for flavor prompt)
    Inscription,    // '¶'  — wall-attached Precursor rune tablet (interactable, shows text)
    Pillar,         // 'I'  — structural pillar (impassable, blocks LoS)
    Brazier,        // 'Ω'  — Precursor brazier (impassable, light source flavor)
    Table,          // 'o'  — cantina tables, command tables
    // ...rest unchanged...
```

If any of the Unicode glyphs fail to render on the terminal backend (the `Cell` struct is ASCII-only per memory `project_cell_refactor`), substitute an ASCII fallback: `Plinth='T'`, `Altar='A'`, `Inscription='i'`, `Pillar='I'`, `Brazier='*'`. Use whichever the rendering table supports — do not change `Cell` in this slice.

- [ ] **Step 2: Extend the terminal renderer fixture glyph/color mapping.**

Using the file found in Step 0, add cases for the five new enum values. For each: glyph (from Step 1), color (civ-neutral muted stone tones), passable=`false`, interactable per the table below.

| FixtureType | Glyph | Color | Passable | Interactable |
|---|---|---|---|---|
| `Plinth` | `T` | 145 (warm stone) | false | false |
| `Altar` | `A` | 179 (muted gold) | false | true |
| `Inscription` | `i` | 109 (faded rune) | false | true |
| `Pillar` | `I` | 145 (warm stone) | false | false |
| `Brazier` | `*` | 208 (ember) | false | false |

Altar and Inscription have `interactable = true` so the player gets an 'e' prompt on them. Interaction wiring for these two types:

- **Inscription:** when the player presses 'e' adjacent, the interaction handler reads `FixtureData::quest_fixture_id` (overloaded as per-fixture flavor text — layer 6.iii stores the picked inscription line there in Task 9) and displays it as a message/popup. *No* routing through the `QuestFixture` registry — `FixtureType::Inscription` is handled directly in the interaction dispatch.
- **Altar:** displays a fixed flavor message ("A weathered Precursor altar. The stone is warm."); no per-fixture text in this slice.

Find the existing fixture-interaction dispatch (likely in `src/interaction/` or near the input handler) and add the two `FixtureType` cases. If the dispatch is table-driven, add table entries; if it's a switch, add two cases.

- [ ] **Step 3: Build and smoke-render.**

Run: `cmake --build build -DDEV=ON -j`
Run: `./build/astra` → enter a test dungeon where `Plinth` etc. don't yet spawn; confirm no compile/link errors. Actual in-world verification happens at Task 14+.

- [ ] **Step 4: Commit.**

```bash
git add include/astra/tilemap.h src/renderer/...
git commit -m "feat(fixtures): add Plinth/Altar/Inscription/Pillar/Brazier fixture types"
```

---

## Task 4: Add `CivConfig::inscription_text_pool` and populate Precursor entries

**Files:**
- Modify: `include/astra/ruin_types.h`
- Modify: `src/generators/ruin_civ_configs.cpp`

- [ ] **Step 1: Add `inscription_text_pool` field.**

In `include/astra/ruin_types.h`, inside `struct CivConfig`, add:

```cpp
    // Flavor text drawn by Inscription required-fixtures (layer 6.iii).
    std::vector<std::string> inscription_text_pool;
```

- [ ] **Step 2: Populate Precursor pool.**

In `src/generators/ruin_civ_configs.cpp`, locate the `CivConfig` initializer for `"Precursor"` and append:

```cpp
cfg.inscription_text_pool = {
    "The silence here is older than breath. We were asked to listen; we obeyed.",
    "Seven stars, seven seals. One we broke ourselves.",
    "Those who walk the nave without gift are asked only to remember.",
    "The crystal hums the first name. The second was never written.",
    "We sealed this vault so that hunger could not enter it.",
    "Do not kneel. The architects did not want kneeling.",
    "The rite of descent is simple: go down until the light changes.",
    "What you carry out of this place you must carry forever.",
};
```

- [ ] **Step 3: Wire inscription text retrieval.**

Add a helper in `ruin_types.h` (or a small inline in `ruin_civ_configs.cpp`):

```cpp
// In ruin_types.h:
const std::string& pick_inscription(const CivConfig& civ, std::mt19937& rng);
```

Implementation in `src/generators/ruin_civ_configs.cpp`:

```cpp
const std::string& pick_inscription(const CivConfig& civ, std::mt19937& rng) {
    static const std::string empty{};
    if (civ.inscription_text_pool.empty()) return empty;
    std::uniform_int_distribution<size_t> d(0, civ.inscription_text_pool.size() - 1);
    return civ.inscription_text_pool[d(rng)];
}
```

- [ ] **Step 4: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 5: Commit.**

```bash
git add include/astra/ruin_types.h src/generators/ruin_civ_configs.cpp
git commit -m "feat(civ): add inscription_text_pool + pick_inscription helper"
```

---

## Task 5: Implement `layout_precursor_vault` L1 carver (Outer Ruin)

**Files:**
- Modify: `src/dungeon/layout.cpp`

- [ ] **Step 1: Add shared chamber-tagging helpers.**

At the top of the anonymous namespace in `src/dungeon/layout.cpp`, add after the existing `Rect` struct and carving helpers:

```cpp
// Sets ctx.sanctum_region_id to the region id at the center of `terminal_rect`.
// Caller must run tag_connected_components first.
void tag_sanctum(TileMap& map, LevelContext& ctx, const Rect& terminal) {
    ctx.sanctum_region_id =
        map.region_id(terminal.x + terminal.w / 2, terminal.y + terminal.h / 2);
}

void tag_chapels(TileMap& map, LevelContext& ctx,
                 const std::vector<Rect>& chapels) {
    ctx.chapel_region_ids.clear();
    ctx.chapel_region_ids.reserve(chapels.size());
    for (const auto& r : chapels) {
        int rid = map.region_id(r.x + r.w / 2, r.y + r.h / 2);
        if (rid >= 0) ctx.chapel_region_ids.push_back(rid);
    }
}

// Rubble-interrupted narrow corridor: carves a 1-wide line but sprinkles
// impassable debris tiles at ~20% density along the middle 60% of the run.
void carve_corridor_broken_h(TileMap& m, int x1, int x2, int y,
                             std::mt19937& rng) {
    if (x1 > x2) std::swap(x1, x2);
    int len = x2 - x1;
    int m0 = x1 + len * 20 / 100;
    int m1 = x1 + len * 80 / 100;
    std::uniform_int_distribution<int> d(0, 99);
    for (int x = x1; x <= x2; ++x) {
        if (!inbounds(m, x, y)) continue;
        if (x > m0 && x < m1 && d(rng) < 20) {
            // leave as Wall — creates a rubble-gap feel; pathable gaps on either side
            continue;
        }
        m.set(x, y, Tile::Floor);
    }
}
```

- [ ] **Step 2: Add the L1 carver.**

Add to the anonymous namespace in `src/dungeon/layout.cpp`:

```cpp
void layout_precursor_vault_l1(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    const int W = map.width();
    const int H = map.height();

    // Entry room — upper-left quadrant.
    Rect entry { 2, 2, 8, 6 };
    // Terminal chamber — lower-right, medium size.
    Rect terminal { W - 12, H - 9, 10, 7 };

    carve_rect(map, entry);
    carve_rect(map, terminal);

    // 4-6 side rooms scattered between entry and terminal.
    std::uniform_int_distribution<int> dcount(4, 6);
    int n = dcount(rng);
    std::vector<Rect> side_rooms;
    side_rooms.reserve(n);
    for (int i = 0; i < n; ++i) {
        std::uniform_int_distribution<int> dw(4, 7);
        std::uniform_int_distribution<int> dh(3, 5);
        std::uniform_int_distribution<int> dx(entry.x + entry.w + 2, terminal.x - 6);
        std::uniform_int_distribution<int> dy(2, H - 8);
        int w = dw(rng), h = dh(rng);
        int x = dx(rng), y = dy(rng);
        Rect r { x, y, w, h };
        // Reject overlaps.
        bool overlap = false;
        for (const auto& o : side_rooms) {
            if (std::abs((r.x + r.w/2) - (o.x + o.w/2)) < (r.w + o.w)/2 + 1 &&
                std::abs((r.y + r.h/2) - (o.y + o.h/2)) < (r.h + o.h)/2 + 1) {
                overlap = true; break;
            }
        }
        if (overlap) continue;
        side_rooms.push_back(r);
        carve_rect(map, r);
    }

    // Processional: rubble-broken 1-wide line from entry center to terminal center.
    int ax = entry.x + entry.w / 2, ay = entry.y + entry.h / 2;
    int bx = terminal.x + terminal.w / 2, by = terminal.y + terminal.h / 2;
    carve_corridor_broken_h(map, ax, bx, ay, rng);
    carve_v(map, ay, by, bx);

    // Short 1-wide stubs from each side room to the processional.
    for (const auto& r : side_rooms) {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        carve_v(map, cy, ay, cx);
    }

    tag_connected_components(map, RegionType::Room);
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, terminal);
    // No chapels on L1.
    ctx.chapel_region_ids.clear();
}
```

- [ ] **Step 3: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build. (Not yet dispatched — Task 8 wires it in.)

- [ ] **Step 4: Commit.**

```bash
git add src/dungeon/layout.cpp
git commit -m "feat(dungeon): PrecursorVault L1 (Outer Ruin) carver"
```

---

## Task 6: Implement L2 carver (Inner Sanctum: nave + chapels)

**Files:**
- Modify: `src/dungeon/layout.cpp`

- [ ] **Step 1: Add the L2 carver.**

Add to the anonymous namespace in `src/dungeon/layout.cpp` after the L1 function:

```cpp
void layout_precursor_vault_l2(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    const int W = map.width();
    const int H = map.height();

    // Entry room — left end, small.
    Rect entry { 2, H/2 - 3, 8, 6 };
    // Terminal chamber — right end, grand.
    Rect terminal { W - 14, H/2 - 5, 12, 10 };

    carve_rect(map, entry);
    carve_rect(map, terminal);

    // 3-wide central nave spanning entry -> terminal, y-centered.
    int nave_y_top = H / 2 - 1;
    int nave_x0 = entry.x + entry.w;
    int nave_x1 = terminal.x;
    for (int y = nave_y_top; y <= nave_y_top + 2; ++y) {
        carve_h(map, nave_x0, nave_x1, y);
    }

    // Symmetric chapels — 2..4 per side.
    std::uniform_int_distribution<int> dchap(2, 4);
    int per_side = dchap(rng);
    std::vector<Rect> chapels;

    int span = nave_x1 - nave_x0;
    int step = span / (per_side + 1);
    for (int i = 1; i <= per_side; ++i) {
        int x_center = nave_x0 + step * i;
        // North chapel.
        Rect north { x_center - 3, nave_y_top - 6, 6, 5 };
        // South chapel.
        Rect south { x_center - 3, nave_y_top + 3 + 1, 6, 5 };

        carve_rect(map, north);
        carve_rect(map, south);
        // 1-wide branches from chapel door to nave edge.
        carve_v(map, north.y + north.h, nave_y_top, x_center);
        carve_v(map, nave_y_top + 3, south.y, x_center);

        chapels.push_back(north);
        chapels.push_back(south);
    }

    tag_connected_components(map, RegionType::Room);

    int ax = entry.x + entry.w / 2, ay = entry.y + entry.h / 2;
    int bx = terminal.x + terminal.w / 2, by = terminal.y + terminal.h / 2;
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, terminal);
    tag_chapels(map, ctx, chapels);
}
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 3: Commit.**

```bash
git add src/dungeon/layout.cpp
git commit -m "feat(dungeon): PrecursorVault L2 (Inner Sanctum) carver"
```

---

## Task 7: Implement L3 carver (Crystal Vault: antechamber → approach → vault)

**Files:**
- Modify: `src/dungeon/layout.cpp`

- [ ] **Step 1: Add the L3 carver.**

```cpp
void layout_precursor_vault_l3(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    (void)rng;
    const int W = map.width();
    const int H = map.height();

    // Antechamber — left side, modest.
    Rect antechamber { 2, H/2 - 3, 8, 6 };
    // Vault — right side, dominant.
    Rect vault { W - 18, H/2 - 7, 16, 14 };

    carve_rect(map, antechamber);
    carve_rect(map, vault);

    // 3-wide ceremonial approach corridor.
    int corridor_y0 = H / 2 - 1;
    int corridor_x0 = antechamber.x + antechamber.w;
    int corridor_x1 = vault.x;
    for (int y = corridor_y0; y <= corridor_y0 + 2; ++y) {
        carve_h(map, corridor_x0, corridor_x1, y);
    }

    tag_connected_components(map, RegionType::Room);

    int ax = antechamber.x + antechamber.w / 2, ay = antechamber.y + antechamber.h / 2;
    int bx = vault.x + vault.w / 2, by = vault.y + vault.h / 2;
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, vault);
    ctx.chapel_region_ids.clear();
}
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 3: Commit.**

```bash
git add src/dungeon/layout.cpp
git commit -m "feat(dungeon): PrecursorVault L3 (Crystal Vault) carver"
```

---

## Task 8: Wire `PrecursorVault` into `apply_layout` dispatch

**Files:**
- Modify: `src/dungeon/layout.cpp`

- [ ] **Step 1: Add dispatch function and extend the switch.**

Inside the anonymous namespace:

```cpp
void layout_precursor_vault(TileMap& map, LevelContext& ctx,
                            std::mt19937& rng) {
    switch (ctx.depth) {
    case 1: layout_precursor_vault_l1(map, ctx, rng); break;
    case 2: layout_precursor_vault_l2(map, ctx, rng); break;
    case 3: layout_precursor_vault_l3(map, ctx, rng); break;
    default:
        // Beyond L3: reuse L3 for safety (should not occur for Archive).
        layout_precursor_vault_l3(map, ctx, rng);
        break;
    }
}
```

In `apply_layout`, replace the `LayoutKind::RuinStamps` assert-fallthrough line:

```cpp
void apply_layout(TileMap& map, const DungeonStyle& style,
                  const CivConfig& civ, LevelContext& ctx,
                  std::mt19937& rng) {
    (void)civ;
    switch (style.layout) {
    case LayoutKind::BSPRooms:
        layout_bsp_rooms(map, ctx, rng);
        break;
    case LayoutKind::PrecursorVault:
        layout_precursor_vault(map, ctx, rng);
        break;
    case LayoutKind::OpenCave:
    case LayoutKind::TunnelCave:
    case LayoutKind::DerelictStationBSP:
    case LayoutKind::RuinStamps:
        assert(!"layout kind not implemented");
        break;
    }
    assert(map.region_count() >= 1 && "layout must produce >=1 region");
}
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 3: Commit.**

```bash
git add src/dungeon/layout.cpp
git commit -m "feat(dungeon): dispatch LayoutKind::PrecursorVault per-depth"
```

---

## Task 9: Implement `PlacementSlot` resolvers (layer 6.iii scaffolding)

**Files:**
- Modify: `src/dungeon/fixtures.cpp`

- [ ] **Step 1: Add placement-slot resolvers in the anonymous namespace.**

At the top of the anonymous namespace (above existing helpers):

```cpp
// Maps our style FixtureKind to the concrete FixtureType the renderer uses.
FixtureType to_fixture_type(FixtureKind k) {
    switch (k) {
    case FixtureKind::Plinth:          return FixtureType::Plinth;
    case FixtureKind::Altar:           return FixtureType::Altar;
    case FixtureKind::Inscription:     return FixtureType::Inscription;
    case FixtureKind::Pillar:          return FixtureType::Pillar;
    case FixtureKind::ResonancePillar: return FixtureType::ResonancePillar;
    case FixtureKind::Brazier:         return FixtureType::Brazier;
    }
    return FixtureType::Table; // unreachable
}

bool is_interior_wall(const TileMap& m, int x, int y) {
    if (!inbounds_fix(m, x, y)) return false;
    if (m.passable(x, y)) return false;
    // Must have at least one orthogonal floor neighbor (= interior).
    static const int dx[4] = { 1,-1, 0, 0 };
    static const int dy[4] = { 0, 0, 1,-1 };
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i], ny = y + dy[i];
        if (inbounds_fix(m, nx, ny) && m.passable(nx, ny)) return true;
    }
    return false;
}

bool inbounds_fix(const TileMap& m, int x, int y) {
    return x >= 0 && y >= 0 && x < m.width() && y < m.height();
}

// Returns region open cells (passable, no fixture) for a given region.
std::vector<std::pair<int,int>> region_open_cells(const TileMap& m, int rid) {
    return collect_region_open(m, rid);
}

// Region centroid open cell (the passable cell closest to the centroid).
std::pair<int,int> region_center_open(const TileMap& m, int rid) {
    auto cells = region_open_cells(m, rid);
    if (cells.empty()) return {-1,-1};
    long sx = 0, sy = 0;
    for (auto& c : cells) { sx += c.first; sy += c.second; }
    int cx = static_cast<int>(sx / static_cast<long>(cells.size()));
    int cy = static_cast<int>(sy / static_cast<long>(cells.size()));
    int best_d = INT_MAX;
    std::pair<int,int> best = cells.front();
    for (auto& c : cells) {
        int dd = std::abs(c.first - cx) + std::abs(c.second - cy);
        if (dd < best_d) { best_d = dd; best = c; }
    }
    return best;
}

// Returns candidate wall-attached positions in the given region (interior walls
// with a passable orthogonal neighbor inside that region).
std::vector<std::pair<int,int>> region_wall_attached(const TileMap& m, int rid) {
    std::vector<std::pair<int,int>> out;
    if (rid < 0 || rid >= m.region_count()) return out;
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            if (!is_interior_wall(m, x, y)) continue;
            bool touches_rid = false;
            static const int dx[4] = { 1,-1, 0, 0 };
            static const int dy[4] = { 0, 0, 1,-1 };
            for (int i = 0; i < 4; ++i) {
                int nx = x + dx[i], ny = y + dy[i];
                if (inbounds_fix(m, nx, ny) && m.passable(nx, ny) &&
                    m.region_id(nx, ny) == rid) { touches_rid = true; break; }
            }
            if (touches_rid) out.emplace_back(x, y);
        }
    }
    return out;
}

void add_required_fixture(TileMap& map, FixtureKind kind, int x, int y,
                          LevelContext& ctx, const CivConfig* civ,
                          std::mt19937& rng) {
    FixtureData fd;
    fd.type = to_fixture_type(kind);
    fd.interactable =
        (kind == FixtureKind::Altar || kind == FixtureKind::Inscription);
    fd.cooldown = (kind == FixtureKind::Inscription) ? -1 : 0;
    // For Inscription, stash the per-fixture flavor text in quest_fixture_id
    // (overloaded as a generic per-fixture string — this is the only existing
    // free-text field on FixtureData and is unused for non-QuestFixture types).
    // The interaction path for FixtureType::Inscription reads this verbatim.
    if (kind == FixtureKind::Inscription && civ) {
        fd.quest_fixture_id = pick_inscription(*civ, rng);
    }
    map.add_fixture(x, y, fd);
    ctx.placed_required_fixtures[kind_key(kind)].emplace_back(x, y);
}
```

> **Note on the `civ` parameter:** `apply_fixtures` doesn't currently take `CivConfig`. Task 10 extends its signature to pass civ through; for now this helper expects a pointer. If the existing signature already threads civ, drop the pointer and take a reference.

- [ ] **Step 2: Build to catch missing includes.**

Run: `cmake --build build -DDEV=ON -j`
Expected: The build *may* fail until Task 10 wires `civ` through `apply_fixtures`. If the only failure is "`civ` undefined in `apply_fixtures`", skip the commit and continue to Task 10 — commit both together. Otherwise fix include errors (likely needs `<climits>` and `"astra/ruin_types.h"`) and re-run.

- [ ] **Step 3: (Conditional) commit.**

Only commit if the build succeeds alone. Otherwise proceed to Task 10 and combine commits.

```bash
git add src/dungeon/fixtures.cpp
git commit -m "feat(dungeon): PlacementSlot resolver helpers for layer 6.iii"
```

---

## Task 10: Layer 6.iii orchestration — `place_required_fixtures` + wire into `apply_fixtures`

**Files:**
- Modify: `include/astra/dungeon/fixtures.h`
- Modify: `src/dungeon/fixtures.cpp`
- Modify: `src/dungeon/pipeline.cpp`

- [ ] **Step 1: Extend `apply_fixtures` signature to accept `CivConfig`.**

In `include/astra/dungeon/fixtures.h`:

```cpp
#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra { class TileMap; struct CivConfig; struct DungeonLevelSpec; }

namespace astra::dungeon {

void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, const DungeonLevelSpec& spec,
                    LevelContext& ctx, std::mt19937& rng);

} // namespace astra::dungeon
```

Update the call site in `src/dungeon/pipeline.cpp`:

```cpp
apply_fixtures(map, style, civ, spec, ctx, rng_fix);
```

- [ ] **Step 2: Implement `place_required_fixtures` in `src/dungeon/fixtures.cpp`.**

Add to the anonymous namespace:

```cpp
// Samples an inclusive count in [r.min, r.max]. Clamps non-positive ranges to 0.
int sample_count(IntRange r, std::mt19937& rng) {
    if (r.max < r.min || r.max <= 0) return 0;
    int lo = std::max(0, r.min);
    std::uniform_int_distribution<int> d(lo, r.max);
    return d(rng);
}

// Returns two cells flanking `target` on a random axis among those with
// open floor neighbors. Returns an empty vector if neither axis fits.
std::vector<std::pair<int,int>> flanking_cells_for(
        const TileMap& m, std::pair<int,int> target,
        const std::vector<std::pair<int,int>>& reserved,
        std::mt19937& rng) {
    struct Axis { std::pair<int,int> a, b; };
    std::vector<Axis> candidates;

    auto free_for_fixture = [&](int x, int y) {
        return open_at(m, x, y) &&
               std::find(reserved.begin(), reserved.end(),
                         std::pair<int,int>{x,y}) == reserved.end();
    };

    // Horizontal flank: (target.x-1,y) and (target.x+1,y)
    if (free_for_fixture(target.first - 1, target.second) &&
        free_for_fixture(target.first + 1, target.second)) {
        candidates.push_back({
            {target.first - 1, target.second},
            {target.first + 1, target.second}
        });
    }
    // Vertical flank.
    if (free_for_fixture(target.first, target.second - 1) &&
        free_for_fixture(target.first, target.second + 1)) {
        candidates.push_back({
            {target.first, target.second - 1},
            {target.first, target.second + 1}
        });
    }
    // Diagonal fallback (NE-SW).
    if (free_for_fixture(target.first - 1, target.second - 1) &&
        free_for_fixture(target.first + 1, target.second + 1)) {
        candidates.push_back({
            {target.first - 1, target.second - 1},
            {target.first + 1, target.second + 1}
        });
    }
    // Diagonal fallback (NW-SE).
    if (free_for_fixture(target.first + 1, target.second - 1) &&
        free_for_fixture(target.first - 1, target.second + 1)) {
        candidates.push_back({
            {target.first + 1, target.second - 1},
            {target.first - 1, target.second + 1}
        });
    }

    if (candidates.empty()) return {};
    std::uniform_int_distribution<size_t> d(0, candidates.size() - 1);
    auto& pick = candidates[d(rng)];
    return { pick.a, pick.b };
}

void place_required_fixtures(TileMap& map, const DungeonStyle& style,
                             const CivConfig& civ, LevelContext& ctx,
                             std::mt19937& rng) {
    std::vector<std::pair<int,int>> reserved;  // positions we've placed in this pass
    const uint32_t my_bit = (ctx.depth >= 1 && ctx.depth <= 32)
                              ? (1u << (ctx.depth - 1)) : 0u;

    for (const auto& rf : style.required_fixtures) {
        if ((rf.depth_mask & my_bit) == 0) continue;

        switch (rf.where) {

        case PlacementSlot::SanctumCenter: {
            auto p = region_center_open(map, ctx.sanctum_region_id);
            if (p.first < 0) break;
            add_required_fixture(map, rf.kind, p.first, p.second, ctx, &civ, rng);
            reserved.push_back(p);
            break;
        }

        case PlacementSlot::ChapelCenter: {
            for (int rid : ctx.chapel_region_ids) {
                int n = sample_count(rf.count, rng);
                auto cells = region_open_cells(map, rid);
                // Shuffle cells; pick first n non-reserved.
                std::shuffle(cells.begin(), cells.end(), rng);
                int placed = 0;
                for (auto& c : cells) {
                    if (placed >= n) break;
                    if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                    ++placed;
                }
            }
            break;
        }

        case PlacementSlot::EachRoomOnce: {
            for (int rid = 0; rid < map.region_count(); ++rid) {
                if (map.region(rid).type != RegionType::Room) continue;
                if (rid == ctx.sanctum_region_id) continue;  // terminal reserved for center slot
                int n = sample_count(rf.count, rng);
                if (n <= 0) continue;
                auto cells = region_open_cells(map, rid);
                std::shuffle(cells.begin(), cells.end(), rng);
                int placed = 0;
                for (auto& c : cells) {
                    if (placed >= n) break;
                    if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                    ++placed;
                }
            }
            break;
        }

        case PlacementSlot::WallAttached: {
            int total = sample_count(rf.count, rng);
            if (total <= 0) break;
            std::vector<std::pair<int,int>> candidates;
            for (int rid = 0; rid < map.region_count(); ++rid) {
                if (map.region(rid).type != RegionType::Room) continue;
                auto c = region_wall_attached(map, rid);
                candidates.insert(candidates.end(), c.begin(), c.end());
            }
            std::shuffle(candidates.begin(), candidates.end(), rng);
            int placed = 0;
            for (auto& c : candidates) {
                if (placed >= total) break;
                if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                // add_required_fixture handles Inscription text stashing and
                // interactable/cooldown defaults uniformly.
                add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                reserved.push_back(c);
                ++placed;
            }
            break;
        }

        case PlacementSlot::FlankPair: {
            // Target: most recently placed Plinth (L3) or Altar (L2) in the same pass.
            const std::vector<std::pair<int,int>>* targets = nullptr;
            auto it_plinth = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Plinth));
            auto it_altar  = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Altar));
            if (it_plinth != ctx.placed_required_fixtures.end() && !it_plinth->second.empty()) {
                targets = &it_plinth->second;
            } else if (it_altar != ctx.placed_required_fixtures.end() && !it_altar->second.empty()) {
                targets = &it_altar->second;
            }
            if (!targets) break;
            for (const auto& tgt : *targets) {
                auto cells = flanking_cells_for(map, tgt, reserved, rng);
                if (cells.size() != 2) continue;
                for (auto& c : cells) {
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                }
            }
            break;
        }
        }
    }
}
```

- [ ] **Step 3: Call `place_required_fixtures` before `place_quest_fixtures` and include `civ`.**

Replace the body of `apply_fixtures`:

```cpp
void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, const DungeonLevelSpec& spec,
                    LevelContext& ctx, std::mt19937& rng) {
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

    // 6.iii — required fixtures (must precede quest fixtures so quest hints
    // like "required_plinth" can resolve against placed locations).
    place_required_fixtures(map, style, civ, ctx, rng);

    place_quest_fixtures(map, spec, ctx, rng);
}
```

- [ ] **Step 4: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build. If Task 9 was uncommitted, add its helpers to this commit.

- [ ] **Step 5: Commit.**

```bash
git add include/astra/dungeon/fixtures.h src/dungeon/fixtures.cpp src/dungeon/pipeline.cpp
git commit -m "feat(dungeon): layer 6.iii place_required_fixtures with 5 placement slots"
```

---

## Task 11: Add `"required_plinth"` placement hint to layer 6.ii

**Files:**
- Modify: `src/dungeon/fixtures.cpp`

- [ ] **Step 1: Extend `place_quest_fixtures` with `required_plinth` hint.**

Inside `place_quest_fixtures`, add a branch before the final fallback:

```cpp
        } else if (pf.placement_hint == "required_plinth") {
            auto it = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Plinth));
            if (it != ctx.placed_required_fixtures.end() && !it->second.empty()) {
                // Pick a plinth (usually exactly one).
                std::uniform_int_distribution<size_t> d(0, it->second.size() - 1);
                auto p = it->second[d(rng)];
                // Quest fixture replaces the plinth's FixtureData in-place:
                // remove the plinth and spawn QuestFixture at the same tile.
                map.remove_fixture(p.first, p.second);
                fx = p.first; fy = p.second;
            }
            // If no plinth placed, fall through to the global-open fallback.
        }
```

> **`map.remove_fixture(x, y)` precondition:** check `TileMap` API. If no `remove_fixture` method exists, replace with whatever current call path is used to overwrite a fixture (e.g. clearing the `fixture_id` at `(x,y)` and re-adding, or `map.add_fixture` with replace semantics). Grep for existing fixture-replacement code: `grep -rn "remove_fixture\|clear_fixture" src/ include/`. Pick the existing idiom; do not add a new API in this slice unless none exists.

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build. If `remove_fixture` doesn't exist and you added an equivalent clear path, note it in the commit.

- [ ] **Step 3: Commit.**

```bash
git add src/dungeon/fixtures.cpp
git commit -m "feat(dungeon): required_plinth placement hint for quest fixtures"
```

---

## Task 12: Add `precursor_vault` decoration pack

**Files:**
- Modify: `src/dungeon/decoration.cpp`

- [ ] **Step 1: Inspect `apply_decoration` dispatch.**

Run: `grep -n "decoration_pack" src/dungeon/decoration.cpp | head`. Identify where pack names dispatch (likely an if/else chain on `style.decoration_pack`).

- [ ] **Step 2: Add the `precursor_vault` branch.**

In `src/dungeon/decoration.cpp`, add a handler. It is tiles-only (no fixtures; those come from layer 6.iii). It scales density by `spec.decay_level`:

```cpp
namespace {

// Percent chance (0-100) of placing a rune-scatter floor decoration on a given
// open tile at a given decay_level. Clamped to decay 0..3.
int rune_density_for(int decay_level) {
    switch (decay_level) {
    case 0: return 3;   // L3 Crystal Vault — faint rune dust
    case 1: return 4;
    case 2: return 5;   // L2 Inner Sanctum — light dust
    case 3: return 8;   // L1 Outer Ruin — heavy
    default: return 3;
    }
}

// Percent chance of replacing an open tile with rubble at a given decay_level.
// Rubble = impassable decoration-tile (not a fixture).
int rubble_density_for(int decay_level) {
    switch (decay_level) {
    case 0: return 0;
    case 1: return 2;
    case 2: return 4;
    case 3: return 12;
    default: return 0;
    }
}

void decorate_precursor_vault(TileMap& map, const CivConfig& civ,
                              const DungeonLevelSpec& spec,
                              std::mt19937& rng) {
    (void)civ;
    const int decay = spec.decay_level;
    const int rune_chance = rune_density_for(decay);
    const int rubble_chance = rubble_density_for(decay);

    std::uniform_int_distribution<int> d(0, 99);
    for (int y = 1; y < map.height() - 1; ++y) {
        for (int x = 1; x < map.width() - 1; ++x) {
            if (!map.passable(x, y)) continue;
            if (map.fixture_id(x, y) >= 0) continue;

            int roll = d(rng);
            if (roll < rubble_chance) {
                // Rubble = decoration tile (glyph ',' dark grey), impassable-ish.
                // Use the existing debris tile variant — project convention is
                // set_tile(Tile::Debris) or equivalent. Check existing packs for the
                // idiom (grep "ruin_debris" decoration.cpp).
                map.set(x, y, Tile::Debris);
            } else if (roll < rubble_chance + rune_chance) {
                // Floor rune tile: use the existing floor-decoration tile variant
                // for "marked floor" (e.g. Tile::FloorDecoration or whatever
                // ruin_debris uses for non-impassable floor marks).
                map.set(x, y, Tile::FloorDecorated);
            }
        }
    }

    // Wall runes: for every region with RegionType::Room, mark a few interior
    // walls with a rune-wall tile, scaled by decay (fewer at high decay — walls
    // damaged). Uses the same "wall decoration" tile that ruin_debris uses for
    // scorched walls — verify name.
    int wall_runes_per_room = (decay <= 1) ? 3 : (decay == 2) ? 2 : 1;
    for (int rid = 0; rid < map.region_count(); ++rid) {
        if (map.region(rid).type != RegionType::Room) continue;
        auto walls = region_wall_attached_decoration(map, rid);
        std::shuffle(walls.begin(), walls.end(), rng);
        for (int i = 0; i < wall_runes_per_room && i < (int)walls.size(); ++i) {
            map.set(walls[i].first, walls[i].second, Tile::WallDecorated);
        }
    }
}

} // namespace
```

If `Tile::Debris`, `Tile::FloorDecorated`, or `Tile::WallDecorated` don't exist by those exact names, **match the idiom used in the existing `ruin_debris` pack handler.** This task's contract is: paint rubble where `ruin_debris` would paint rubble, paint floor runes where `ruin_debris` would paint debris-floor, paint wall runes where `ruin_debris` would paint scorched walls. If `ruin_debris` uses a different mechanism (e.g. semantic decoration IDs rather than tile enum values), reuse that mechanism verbatim.

`region_wall_attached_decoration` is the decoration-layer counterpart to `region_wall_attached` in fixtures.cpp — if decoration.cpp already has a helper that iterates interior walls of a region, reuse it; otherwise reproduce the same small loop inline.

- [ ] **Step 3: Wire `precursor_vault` into the dispatch.**

Inside `apply_decoration` (or the equivalent dispatch function in `decoration.cpp`), add:

```cpp
    if (style.decoration_pack == "precursor_vault") {
        decorate_precursor_vault(map, civ, spec, rng);
        return;
    }
```

- [ ] **Step 4: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 5: Commit.**

```bash
git add src/dungeon/decoration.cpp
git commit -m "feat(dungeon): precursor_vault decoration pack (tiles-only, decay-scaled)"
```

---

## Task 13: Register `PrecursorRuin` style and extend `parse_style_id`

**Files:**
- Modify: `src/dungeon/style_configs.cpp`

- [ ] **Step 1: Add the `kPrecursorRuinRequiredFixtures` table and `kPrecursorRuin` style.**

In `src/dungeon/style_configs.cpp`, after the existing `kSimpleRoomsAndCorridors`:

```cpp
namespace {

const std::vector<RequiredFixture> kPrecursorRuinRequiredFixtures = {
    // L3 vault — plinth first so FlankPair entries resolve against it.
    { FixtureKind::Plinth,          PlacementSlot::SanctumCenter, {1,1}, depth_mask_bit(3) },
    { FixtureKind::ResonancePillar, PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },

    // L2 chapels (1–2 altars per chapel, per spec §5.3).
    { FixtureKind::Altar,           PlacementSlot::ChapelCenter,  {1,2}, depth_mask_bit(2) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(2) },

    // Structural pillars in nave + vault.
    { FixtureKind::Pillar,          PlacementSlot::EachRoomOnce,  {0,2},
      depth_mask_bit(2) | depth_mask_bit(3) },

    // Inscriptions, all depths.
    { FixtureKind::Inscription,     PlacementSlot::WallAttached,  {1,2},
      depth_mask_all(3) },
};

const DungeonStyle kPrecursorRuin = [] {
    DungeonStyle s;
    s.id                    = StyleId::PrecursorRuin;
    s.debug_name            = "precursor_ruin";
    s.backdrop_material     = "rock";
    s.layout                = LayoutKind::PrecursorVault;
    s.stairs_strategy       = StairsStrategy::EntryExitRooms;
    s.allowed_overlays      = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack       = "precursor_vault";
    s.connectivity_required = true;
    s.required_fixtures     = kPrecursorRuinRequiredFixtures;
    return s;
}();

} // namespace
```

- [ ] **Step 2: Register in `style_config(...)` lookup.**

Find the registry switch/map in `style_configs.cpp` and add:

```cpp
const DungeonStyle& style_config(StyleId id) {
    switch (id) {
    case StyleId::SimpleRoomsAndCorridors: return kSimpleRoomsAndCorridors;
    case StyleId::PrecursorRuin:           return kPrecursorRuin;
    // ... other reserved styles assert-unknown ...
    }
    assert(!"unknown StyleId");
    return kSimpleRoomsAndCorridors;
}
```

- [ ] **Step 3: Extend `parse_style_id`.**

```cpp
bool parse_style_id(const std::string& debug_name, StyleId& out) {
    if (debug_name == "simple_rooms")   { out = StyleId::SimpleRoomsAndCorridors; return true; }
    if (debug_name == "precursor_ruin") { out = StyleId::PrecursorRuin;           return true; }
    return false;
}
```

- [ ] **Step 4: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build.

- [ ] **Step 5: Smoke test.**

Run: `./build/astra` (dev mode), open dev console, run `:dungen precursor_ruin Precursor`. Expect: a 3-depth dungeon with the fractured L1 layout, nave-and-chapels L2, and antechamber-approach-vault L3. Fixtures (plinth, altars, inscriptions, pillars, braziers) visible at their designated placements. No softlocks.

If fixtures overlap or FlankPair leaves only 1 of 2, check the ordering in `kPrecursorRuinRequiredFixtures` — the target (`Plinth`/`Altar`) must precede its flank pairs.

- [ ] **Step 6: Commit.**

```bash
git add src/dungeon/style_configs.cpp
git commit -m "feat(dungeon): register StyleId::PrecursorRuin with required_fixtures catalog"
```

---

## Task 14: Flip Archive recipes to `PrecursorRuin` style

**Files:**
- Modify: `src/dungeon/conclave_archive.cpp`

- [ ] **Step 1: Update recipe builder.**

Replace the body of `build_conclave_archive_levels()`:

```cpp
std::vector<DungeonLevelSpec> build_conclave_archive_levels() {
    std::vector<DungeonLevelSpec> out;
    out.reserve(3);

    // L1 — Outer Ruin (Conclave-held, battle-scarred).
    {
        DungeonLevelSpec l1;
        l1.style_id    = dungeon::StyleId::PrecursorRuin;
        l1.civ_name    = "Precursor";
        l1.decay_level = 3;
        l1.enemy_tier  = 1;
        l1.overlays    = { dungeon::OverlayKind::BattleScarred };
        l1.npc_roles   = {
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry", "Conclave Sentry", "Conclave Sentry",
            "Conclave Sentry", "Conclave Sentry",
            "Heavy Conclave Sentry", "Heavy Conclave Sentry",
            "Conclave Sentry Drone", "Conclave Sentry Drone",
            "Conclave Sentry Drone", "Conclave Sentry Drone",
        };
        out.push_back(std::move(l1));
    }

    // L2 — Inner Sanctum (reasserted Precursor defenders; fighting-stopped-here).
    {
        DungeonLevelSpec l2;
        l2.style_id    = dungeon::StyleId::PrecursorRuin;
        l2.civ_name    = "Precursor";
        l2.decay_level = 2;
        l2.enemy_tier  = 2;
        l2.overlays    = { dungeon::OverlayKind::BattleScarred };
        l2.npc_roles   = {
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant",
            "Archon Automaton", "Archon Automaton",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone",
        };
        out.push_back(std::move(l2));
    }

    // L3 — Crystal Vault (pristine; boss + nova resonance crystal on plinth).
    {
        DungeonLevelSpec l3;
        l3.style_id      = dungeon::StyleId::PrecursorRuin;
        l3.civ_name      = "Precursor";
        l3.decay_level   = 0;
        l3.enemy_tier    = 3;
        l3.is_boss_level = true;
        l3.overlays      = {};   // pristine
        l3.fixtures      = {
            PlannedFixture{ "nova_resonance_crystal", "required_plinth" },
        };
        l3.npc_roles     = {
            "Archon Automaton", "Archon Automaton", "Archon Automaton",
            "Archon Remnant", "Archon Remnant", "Archon Remnant",
            "Archon Remnant",
            "Archon Sentry Drone", "Archon Sentry Drone",
            "Archon Sentry Drone",
            "Archon Sentinel",
        };
        out.push_back(std::move(l3));
    }

    return out;
}
```

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build. Build still succeeds even with the legacy `old_impl::` branch still active — Archive won't route through the new pipeline yet.

- [ ] **Step 3: Commit.**

```bash
git add src/dungeon/conclave_archive.cpp
git commit -m "feat(archive): flip recipes to StyleId::PrecursorRuin + required_plinth hint"
```

---

## Task 15: Delete `kind_tag == "conclave_archive"` bridge and `old_impl::` namespace

**Files:**
- Modify: `src/generators/dungeon_level.cpp`

- [ ] **Step 1: Delete the bridge and legacy namespace.**

In `src/generators/dungeon_level.cpp`:
- Remove the `if (recipe.kind_tag == "conclave_archive") { old_impl::generate_archive_level_legacy(...); return; }` block.
- Remove the entire `namespace old_impl { ... }` body, including `generate_archive_level_legacy`, `find_fixture_xy`, `collect_region_open`, `region_centroid`, `place_planned_fixtures`, and any helpers only used by it.
- Remove any includes that were only there for `old_impl::`.

The file's only remaining responsibilities should be:
- `dungeon_level_seed(...)`
- `find_stairs_up(...)` / `find_stairs_down(...)`
- `generate_dungeon_level(...)` — pure pipeline front door.

- [ ] **Step 2: Build.**

Run: `cmake --build build -DDEV=ON -j`
Expected: clean build. Any "unused function" warnings from stragglers must be fixed by deleting the stragglers.

- [ ] **Step 3: Smoke test full Archive entry.**

Run: `./build/astra`, start a new game, navigate to Io (or use `:dungen precursor_ruin Precursor` to bypass the overworld journey), descend into the Archive. L1 should now render via the new pipeline (rather than the legacy body).

- [ ] **Step 4: Commit.**

```bash
git add src/generators/dungeon_level.cpp
git commit -m "refactor(dungeon): delete old_impl legacy body + conclave_archive kind_tag bridge"
```

---

## Task 16: Bump `SAVE_FILE_VERSION` and reject older saves

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump the constant.**

In `include/astra/save_file.h`:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 39;   // was 38 — Archive migration
```

- [ ] **Step 2: Reject older saves on load.**

In `src/save_file.cpp`, locate the load path and ensure the version-mismatch branch presents a clear error. Expected existing shape:

```cpp
if (header.version != SAVE_FILE_VERSION) {
    log_error("Save file version %u is not supported (expected %u). "
              "Pre-release versions do not support backward compatibility.",
              header.version, SAVE_FILE_VERSION);
    return false;
}
```

If the existing code had a `version < SAVE_FILE_VERSION` migration branch, delete it. No migration code survives.

- [ ] **Step 3: Delete any version-gated `DREC` read/write branches.**

Grep: `grep -n "version" src/save_file.cpp`. Any `if (version <= 38)` or similar conditional reader paths should be deleted. With backcompat off, read/write code always assumes the current schema.

- [ ] **Step 4: Build and verify.**

Run: `cmake --build build -DDEV=ON -j`
Run: `./build/astra` with an old save (if any exists under `~/.local/share/astra/...`) — expect graceful rejection with a clear message, not a crash.

- [ ] **Step 5: Commit.**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "refactor(save): bump SAVE_FILE_VERSION 38->39 and reject older saves"
```

---

## Task 17: Full Archive quest playthrough smoke test

**Files:**
- No code changes in this task. This is a validation checkpoint.

- [ ] **Step 1: Fresh-game playthrough.**

Run: `./build/astra`. Start a new game on Heavens Above. Accept the Siege quest path that leads to the Conclave Archive on Io. Travel to Io. Enter the Archive hatch.

- [ ] **Step 2: Verify per-level spatial identity.**

- **L1 (Outer Ruin):** visibly fractured — processional corridor broken by rubble; 4–6 side rooms; battle-scarred overlay visible (scorch/damage decoration tiles).
- **L2 (Inner Sanctum):** 3-wide central nave running left→right; symmetric chapels above/below; altars visible in chapel centers with flanking braziers; inscriptions on interior walls; moderate decay.
- **L3 (Crystal Vault):** 3-wide approach corridor leading into a single large vault; plinth centered in the vault with the `nova_resonance_crystal` on it; two `ResonancePillar`s and two Braziers flanking; no side branches; pristine (no scorch, no rubble).

- [ ] **Step 3: Verify interactions.**

- All stairs traversable L1↔L2↔L3.
- All inscriptions show Precursor flavor text (drawn from `inscription_text_pool`).
- `nova_resonance_crystal` is interactable on the plinth tile; interacting triggers Nova's audio log and advances the Siege quest's second objective.
- No softlocks; no rooms cut off from the main graph; no NPCs stuck in walls.

- [ ] **Step 4: Verify zero softlocks under re-seed.**

Run `:dungen precursor_ruin Precursor` with five different seeds (dev console will regenerate). Confirm reachability of stairs, plinth, and all altars on every seed.

- [ ] **Step 5: Fix any issues encountered in-task.**

Typical issues to watch for:
- FlankPair resolves to fewer than 2 tiles (target too close to a wall) — widen the vault or tighten `flanking_cells_for` fallback logic.
- Inscriptions spawn on walls that are part of a 1-tile corridor gap — bias `region_wall_attached` to skip walls that are part of narrow passages.
- Chapels have no valid floor for an altar because the carver placed them too small — bump chapel size to 6×5.

Commit any fixes under `fix(dungeon): ...` messages.

---

## Task 18: Update `docs/roadmap.md`

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Check off the Archive migration item.**

In `docs/roadmap.md`, locate the Archive migration / dungeon overhaul section and tick the box corresponding to this slice (StyleId::PrecursorRuin + Archive migration). If the entry doesn't exist yet, add it under the dungeon-overhaul section:

```markdown
- [x] Archive Dungeon Migration — PrecursorRuin style + PrecursorVault layout + layer 6.iii required_fixtures (2026-04-22)
```

- [ ] **Step 2: Commit.**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): check off Archive dungeon migration"
```

---

## Self-review notes (for executing agent)

- **Spec coverage:** Every spec section (§2 style, §3 layout, §4 decoration pack, §5 layer 6.iii catalog + slots, §6 quest reshuffle, §7 recipe, §8 legacy deletion, §9 save bump, §10 constraints, §11 DoD) has at least one task.
- **Ordering:** Task 11 (`required_plinth` hint) depends on Task 10 (layer 6.iii orchestration) and Task 5-8 (layout). Task 14 (flip recipes) must come *before* Task 15 (delete bridge) or the `old_impl::` path would still be the only live code for Archive and you'd ship a broken build between tasks. Preserve this order.
- **FlankPair target ordering in Task 13:** `Plinth` / `Altar` entries appear *before* any `FlankPair` entry that depends on them. Do not reorder.
- **Renderer integration (Task 3):** Step 0 is non-optional — the exact file varies by terminal renderer structure. Do not guess paths.
- **`Tile::Debris` / `Tile::FloorDecorated` / `Tile::WallDecorated` names in Task 12:** match the idiom of the existing `ruin_debris` pack. If the names differ, rename during implementation.
- **`map.remove_fixture` in Task 11:** if no such API exists, use whatever `ruin_debris` uses to overwrite fixtures, or add a minimal private helper inline. Do not expand the public `TileMap` API in this slice.
