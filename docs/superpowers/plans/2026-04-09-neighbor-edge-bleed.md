# Neighbor Edge Bleed Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Seamless terrain transitions between adjacent overworld tiles' detail maps by reading cached neighbor edge data and applying two-phase blending (5-tile verbatim stamp + 15-tile weighted blend).

**Architecture:** A new `EdgeStrip` struct captures tiles, fixtures, glyph overrides, and custom flags from a cached neighbor's TileMap border. `build_detail_props()` extracts strips from the location cache and attaches them to `MapProperties`. The v2 generator's `apply_neighbor_bleed()` is reworked to apply cached edge strips (two-phase) with procedural fallback when no cached neighbor exists.

**Tech Stack:** C++20, existing TileMap/MapProperties/BiomeProfile infrastructure

**Spec:** `docs/superpowers/specs/2026-04-09-neighbor-edge-bleed-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/edge_strip.h` | Create | `EdgeStripCell`, `EdgeStrip` structs, `extract_edge_strip()` declaration |
| `src/generators/edge_strip.cpp` | Create | `extract_edge_strip()` implementation |
| `include/astra/map_properties.h` | Modify | Add 4 `std::optional<EdgeStrip>` fields |
| `src/game_world.cpp` | Modify | `build_detail_props()` — extract edge strips from cached neighbors |
| `src/generators/detail_map_generator_v2.cpp` | Modify | Rework `apply_neighbor_bleed()` — two-phase stamp + blend with procedural fallback |
| `CMakeLists.txt` | Modify | Add `src/generators/edge_strip.cpp` to build |

---

### Task 1: Create EdgeStrip data structures

**Files:**
- Create: `include/astra/edge_strip.h`

- [ ] **Step 1: Create the header file**

```cpp
// include/astra/edge_strip.h
#pragma once

#include "astra/tilemap.h"

#include <optional>
#include <vector>

namespace astra {

struct EdgeStripCell {
    Tile tile = Tile::Empty;
    std::optional<FixtureData> fixture;
    uint8_t glyph_override = 0;
    uint8_t custom_flags = 0;
};

enum class EdgeDirection : uint8_t { North, South, East, West };

struct EdgeStrip {
    int length = 0;   // 360 for N/S edges, 150 for E/W edges
    int depth  = 0;   // number of rows/cols captured (20)

    // Row-major: cells[depth_idx * length + along_idx]
    // depth_idx 0 = the shared boundary line, depth_idx (depth-1) = deepest into neighbor
    std::vector<EdgeStripCell> cells;

    const EdgeStripCell& at(int depth_idx, int along_idx) const {
        return cells[depth_idx * length + along_idx];
    }
};

// Extract an edge strip from a TileMap.
// dir = which edge of the source map to read.
// strip_depth = how many rows/cols to capture (typically 20).
EdgeStrip extract_edge_strip(const TileMap& map, EdgeDirection dir, int strip_depth);

} // namespace astra
```

- [ ] **Step 2: Verify it compiles**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | head -30`
Expected: Clean build (header is not yet included anywhere, no compile errors from existing code)

- [ ] **Step 3: Commit**

```bash
git add include/astra/edge_strip.h
git commit -m "feat(phase5): add EdgeStrip data structures for neighbor bleed"
```

---

### Task 2: Implement extract_edge_strip()

**Files:**
- Create: `src/generators/edge_strip.cpp`
- Modify: `CMakeLists.txt` — add new source file

- [ ] **Step 1: Create the implementation file**

```cpp
// src/generators/edge_strip.cpp
#include "astra/edge_strip.h"

namespace astra {

EdgeStrip extract_edge_strip(const TileMap& map, EdgeDirection dir, int strip_depth) {
    const int w = map.width();
    const int h = map.height();

    EdgeStrip strip;
    strip.depth = strip_depth;

    switch (dir) {
        case EdgeDirection::North:
        case EdgeDirection::South:
            strip.length = w;
            break;
        case EdgeDirection::East:
        case EdgeDirection::West:
            strip.length = h;
            break;
    }

    strip.cells.resize(strip.depth * strip.length);

    for (int d = 0; d < strip.depth; ++d) {
        for (int a = 0; a < strip.length; ++a) {
            int x = 0, y = 0;

            switch (dir) {
                case EdgeDirection::North:
                    // Row 0 of map = depth 0 (boundary), row strip_depth-1 = deepest
                    x = a;
                    y = d;
                    break;
                case EdgeDirection::South:
                    // Last row of map = depth 0 (boundary), going inward
                    x = a;
                    y = (h - 1) - d;
                    break;
                case EdgeDirection::East:
                    // Last col of map = depth 0, going inward
                    x = (w - 1) - d;
                    y = a;
                    break;
                case EdgeDirection::West:
                    // Col 0 of map = depth 0, going inward
                    x = d;
                    y = a;
                    break;
            }

            EdgeStripCell& cell = strip.cells[d * strip.length + a];
            cell.tile = map.get(x, y);
            cell.glyph_override = map.glyph_override(x, y);
            cell.custom_flags = map.get_custom_flags(x, y);

            int fid = map.fixture_id(x, y);
            if (fid >= 0) {
                cell.fixture = map.fixture(fid);
            }
        }
    }

    return strip;
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/edge_strip.cpp` after the `terrain_compositor.cpp` line in the source list (around line 48).

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build, no errors

- [ ] **Step 4: Commit**

```bash
git add src/generators/edge_strip.cpp CMakeLists.txt
git commit -m "feat(phase5): implement extract_edge_strip() for reading cached neighbor edges"
```

---

### Task 3: Add edge strip fields to MapProperties

**Files:**
- Modify: `include/astra/map_properties.h`

- [ ] **Step 1: Add the include and fields**

Add `#include "astra/edge_strip.h"` to the includes at the top of the file (after the existing includes).

Add 4 optional edge strip fields after the existing `detail_neighbor_e` field (after line 82):

```cpp
    // Cached neighbor edge strips for seamless detail map transitions
    std::optional<EdgeStrip> edge_strip_n;
    std::optional<EdgeStrip> edge_strip_s;
    std::optional<EdgeStrip> edge_strip_e;
    std::optional<EdgeStrip> edge_strip_w;
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 3: Commit**

```bash
git add include/astra/map_properties.h
git commit -m "feat(phase5): add optional EdgeStrip fields to MapProperties"
```

---

### Task 4: Extract edge strips in build_detail_props()

**Files:**
- Modify: `src/game_world.cpp` — `build_detail_props()` function (starts at line 420)

- [ ] **Step 1: Add the edge_strip include**

Add `#include "astra/edge_strip.h"` to the includes at the top of `src/game_world.cpp` (after the existing includes, around line 12).

- [ ] **Step 2: Add edge strip extraction after the neighbor tile sampling**

Insert the following block after the neighbor sampling code (after line 463, after `props.detail_neighbor_e = get_ow(ow_x + 1, ow_y);`), before the POI detection block:

```cpp
    // Extract edge strips from cached neighbor detail maps
    {
        static constexpr int bleed_depth = 20;
        auto try_extract = [&](int nx, int ny, EdgeDirection neighbor_edge)
                -> std::optional<EdgeStrip> {
            LocationKey neighbor_key = {
                world_.navigation().current_system_id,
                world_.navigation().current_body_index,
                world_.navigation().current_moon_index,
                false, nx, ny, 0};
            auto it = world_.location_cache().find(neighbor_key);
            if (it == world_.location_cache().end()) return std::nullopt;
            return extract_edge_strip(it->second.map, neighbor_edge, bleed_depth);
        };

        // B is south of A → read A's south edge
        props.edge_strip_n = try_extract(ow_x, ow_y - 1, EdgeDirection::South);
        // B is north of A → read A's north edge
        props.edge_strip_s = try_extract(ow_x, ow_y + 1, EdgeDirection::North);
        // B is east of A → read A's east edge
        props.edge_strip_w = try_extract(ow_x - 1, ow_y, EdgeDirection::East);
        // B is west of A → read A's west edge
        props.edge_strip_e = try_extract(ow_x + 1, ow_y, EdgeDirection::West);
    }
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(phase5): extract edge strips from cached neighbors in build_detail_props"
```

---

### Task 5: Rework apply_neighbor_bleed() — two-phase stamp + blend

**Files:**
- Modify: `src/generators/detail_map_generator_v2.cpp`

This is the core task. The existing `apply_neighbor_bleed()` (lines 60-118) is replaced with a new version that:
1. Applies cached edge strips (two-phase: verbatim stamp + weighted blend)
2. Falls back to procedural elevation blending for directions without cached data

- [ ] **Step 1: Add the edge_strip include**

Add `#include "astra/edge_strip.h"` to the includes at the top of the file (after the existing includes, around line 6).

- [ ] **Step 2: Move apply_neighbor_bleed to run after composite_terrain**

The existing call at line 51 calls `apply_neighbor_bleed(channels_, rng)` before `composite_terrain`. This is correct for the procedural fallback (which operates on channels), but the edge strip application needs to operate on the composited TileMap. Change `generate_layout()` to:

```cpp
void DetailMapGeneratorV2::generate_layout(std::mt19937& rng) {
    const auto& prof = biome_profile(props_->biome);
    const int w = map_->width();
    const int h = map_->height();

    channels_ = TerrainChannels(w, h);

    if (prof.elevation_fn) {
        prof.elevation_fn(channels_.elevation.data(), w, h, rng, prof);
    }

    if (prof.moisture_fn) {
        prof.moisture_fn(channels_.moisture.data(), w, h, rng,
                         channels_.elevation.data(), prof);
    }

    if (prof.structure_fn) {
        prof.structure_fn(channels_.structure.data(), w, h, rng,
                          channels_.elevation.data(), channels_.moisture.data(), prof);
    }

    // Procedural channel blending for uncached neighbors (modifies channels)
    apply_procedural_bleed(channels_, rng);

    composite_terrain(*map_, channels_, prof);

    // Cached edge strip application (modifies composited TileMap)
    apply_edge_strips(rng);
}
```

- [ ] **Step 3: Update the class declaration**

Replace the private `apply_neighbor_bleed` declaration with:

```cpp
private:
    void apply_procedural_bleed(TerrainChannels& channels, std::mt19937& rng);
    void apply_edge_strips(std::mt19937& rng);

    TerrainChannels channels_;
```

- [ ] **Step 4: Rename existing apply_neighbor_bleed to apply_procedural_bleed**

Rename the function definition (at line 60) from `apply_neighbor_bleed` to `apply_procedural_bleed`. Add a check to skip directions that have cached edge strips (those will be handled by `apply_edge_strips` instead):

```cpp
void DetailMapGeneratorV2::apply_procedural_bleed(TerrainChannels& channels,
                                                   std::mt19937& rng) {
    static constexpr int bleed_margin = 20;

    const int w = channels.width;
    const int h = channels.height;

    const Tile neighbors[4] = {
        props_->detail_neighbor_n,
        props_->detail_neighbor_s,
        props_->detail_neighbor_w,
        props_->detail_neighbor_e,
    };

    // Skip directions that have cached edge strips — they get real data later
    const bool has_strip[4] = {
        props_->edge_strip_n.has_value(),
        props_->edge_strip_s.has_value(),
        props_->edge_strip_w.has_value(),
        props_->edge_strip_e.has_value(),
    };

    for (int dir = 0; dir < 4; ++dir) {
        if (has_strip[dir]) continue;
        if (neighbors[dir] == Tile::Empty) continue;

        Biome neighbor_biome = detail_biome_for_terrain(neighbors[dir],
                                                         props_->biome);
        if (neighbor_biome == props_->biome) continue;

        const auto& neighbor_prof = biome_profile(neighbor_biome);
        if (!neighbor_prof.elevation_fn) continue;

        unsigned dir_seed = rng() ^ (static_cast<unsigned>(dir) * 7919u);
        std::mt19937 neighbor_rng(dir_seed);

        std::vector<float> neighbor_elev(w * h, 0.0f);
        neighbor_prof.elevation_fn(neighbor_elev.data(), w, h,
                                   neighbor_rng, neighbor_prof);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int distance = 0;
                switch (dir) {
                    case 0: distance = y; break;
                    case 1: distance = (h - 1) - y; break;
                    case 2: distance = x; break;
                    case 3: distance = (w - 1) - x; break;
                }

                if (distance >= bleed_margin) continue;

                float t = 1.0f - (static_cast<float>(distance) /
                                  static_cast<float>(bleed_margin));
                t = t * t;

                int idx = y * w + x;
                channels.elevation[idx] =
                    channels.elevation[idx] * (1.0f - t) +
                    neighbor_elev[idx] * t;
            }
        }
    }
}
```

- [ ] **Step 5: Implement apply_edge_strips()**

Add the new function after `apply_procedural_bleed`:

```cpp
void DetailMapGeneratorV2::apply_edge_strips(std::mt19937& rng) {
    static constexpr int verbatim_depth = 5;
    static constexpr int total_depth = 20;

    const int w = map_->width();
    const int h = map_->height();

    // Process strips in order: N, S, E, W (later overwrites earlier in corners)
    struct StripInfo {
        const std::optional<EdgeStrip>* strip;
        int dir; // 0=N, 1=S, 2=E, 3=W
    };
    StripInfo strips[4] = {
        {&props_->edge_strip_n, 0},
        {&props_->edge_strip_s, 1},
        {&props_->edge_strip_e, 2},
        {&props_->edge_strip_w, 3},
    };

    for (const auto& info : strips) {
        if (!info.strip->has_value()) continue;
        const EdgeStrip& strip = info.strip->value();

        // Deterministic RNG for blend probability
        unsigned strip_seed = rng() ^ (static_cast<unsigned>(info.dir) * 4973u);
        std::mt19937 blend_rng(strip_seed);

        int max_depth = std::min(strip.depth, total_depth);

        for (int d = 0; d < max_depth; ++d) {
            for (int a = 0; a < strip.length; ++a) {
                // Map strip coordinates to map coordinates
                int x = 0, y = 0;
                switch (info.dir) {
                    case 0: // North: strip boundary → map row 0
                        x = a; y = d;
                        break;
                    case 1: // South: strip boundary → map last row
                        x = a; y = (h - 1) - d;
                        break;
                    case 2: // East: strip boundary → map last col
                        x = (w - 1) - d; y = a;
                        break;
                    case 3: // West: strip boundary → map col 0
                        x = d; y = a;
                        break;
                }

                if (x < 0 || x >= w || y < 0 || y >= h) continue;

                const EdgeStripCell& src = strip.at(d, a);

                if (d < verbatim_depth) {
                    // Phase 1: Verbatim stamp
                    map_->set(x, y, src.tile);

                    // Remove existing fixture if any, then place strip's fixture
                    if (map_->fixture_id(x, y) >= 0) {
                        map_->remove_fixture(x, y);
                    }
                    if (src.fixture.has_value()) {
                        map_->add_fixture(x, y, src.fixture.value());
                    }

                    map_->set_glyph_override(x, y, src.glyph_override);
                    map_->set_custom_flags_byte(x, y, src.custom_flags);
                } else {
                    // Phase 2: Weighted blend
                    float t = 1.0f - (static_cast<float>(d - verbatim_depth) /
                                      static_cast<float>(total_depth - verbatim_depth));
                    t = t * t; // quadratic falloff

                    // Probability roll for this cell
                    float roll = static_cast<float>(blend_rng() % 10000) / 10000.0f;

                    bool stamp_tile = false;

                    // Walls, structural tiles, water: stamp if they differ from generated
                    if (src.tile == Tile::Wall || src.tile == Tile::StructuralWall ||
                        src.tile == Tile::IndoorFloor || src.tile == Tile::Path ||
                        src.tile == Tile::Water) {
                        if (map_->get(x, y) == Tile::Floor && roll < t) {
                            map_->set(x, y, src.tile);
                            stamp_tile = true;
                        }
                    }

                    // Fixtures: place if strip has one and generated cell doesn't
                    if (src.fixture.has_value() && map_->fixture_id(x, y) < 0) {
                        float fixture_roll = static_cast<float>(blend_rng() % 10000) / 10000.0f;
                        if (fixture_roll < t) {
                            map_->add_fixture(x, y, src.fixture.value());
                        }
                    }

                    // Glyph override: copy where tile was also stamped
                    if (stamp_tile && src.glyph_override != 0) {
                        map_->set_glyph_override(x, y, src.glyph_override);
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Clean build

- [ ] **Step 7: Commit**

```bash
git add src/generators/detail_map_generator_v2.cpp
git commit -m "feat(phase5): two-phase edge strip blending in apply_neighbor_bleed"
```

---

### Task 6: Build, run, and verify visually

**Files:** None (testing only)

- [ ] **Step 1: Build the project**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean build with no warnings in the files we touched

- [ ] **Step 2: Run the game and test transitions**

Run: `./build/astra`

Test procedure:
1. Land on a planet surface, enter overworld
2. Walk into any terrain tile to enter a detail map
3. Walk across zones to reach the edge of the overworld tile
4. Cross into an adjacent overworld tile (this caches the first tile and generates the second)
5. Walk back to the original tile (now the second tile is cached and the first restores from cache)
6. Walk to the border again — the second tile should now read the first tile's cached edge strip
7. Cross back into the second tile — it should regenerate with edge strips from the first, and terrain should flow seamlessly

What to look for:
- Rivers/water should continue across tile boundaries
- Wall formations should extend into neighboring tiles
- No harsh terrain cutoffs at borders
- The verbatim zone (5 tiles) should be an exact match
- The blend zone (tiles 5-19) should show a natural transition

- [ ] **Step 3: Commit any fixes if needed**

---

## Self-Review Checklist

**Spec coverage:**
- Section 1 (Edge Strip Data): Task 1 — `EdgeStripCell`, `EdgeStrip` structs with `at()` accessor
- Section 2 (Extraction): Task 2 — `extract_edge_strip()` reads tiles/fixtures/glyphs/flags; Task 4 — wired into `build_detail_props()`
- Section 3 (Two-Phase Blending): Task 5 — verbatim stamp (depth 0-4), weighted blend (depth 5-19), procedural fallback
- Section 4 (Corner Handling): Task 5 — strips processed N, S, E, W order; later overwrites earlier in corners
- Section 5 (POI Interaction): POI runs in `place_features()` after `generate_layout()`, naturally overwrites bleed tiles
- Section 6 (File Structure): All files covered across tasks 1-5
- Section 7 (Visual Example): Task 6 covers visual verification

**Placeholder scan:** No TBDs, TODOs, or vague steps. All code blocks are complete.

**Type consistency:**
- `EdgeStripCell` and `EdgeStrip` — consistent between Task 1 (definition) and Tasks 2, 4, 5 (usage)
- `EdgeDirection` enum — defined in Task 1, used in Tasks 2 and 4
- `extract_edge_strip(const TileMap&, EdgeDirection, int)` — declared in Task 1, implemented in Task 2, called in Task 4
- `apply_procedural_bleed` / `apply_edge_strips` — declared in Task 5 Step 3, defined in Steps 4-5, called in Step 2
- `props_->edge_strip_n` etc. — added in Task 3, read in Tasks 4-5
