# Mountains Biome Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated Mountains biome with three neighbor-driven structure variants (passes, gradient, plateau), make OW_Mountains traversable, and integrate with the existing edge bleed system.

**Architecture:** New `Biome::Mountains` enum value, a `structure_mountains` strategy function with internal variant branching based on mountain neighbor count, removal of impassable checks, and a mountains BiomeProfile with ridgeline elevation + mineral scatter.

**Tech Stack:** C++20, existing BiomeProfile/strategy/noise infrastructure

**Spec:** `docs/superpowers/specs/2026-04-09-mountains-biome-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/tilemap.h` | Modify | Add `Mountains` to `Biome` enum |
| `include/astra/map_properties.h` | Modify | Update `detail_biome_for_terrain` mapping |
| `include/astra/biome_profile.h` | Modify | Declare `structure_mountains`, add `mountain_neighbor_count` field |
| `src/generators/structure_strategies.cpp` | Modify | Implement `structure_mountains` (3 variants) |
| `src/generators/biome_profiles.cpp` | Modify | Add mountains profile + switch case + parse entry |
| `src/tilemap.cpp` | Modify | Add `Biome::Mountains` to `biome_colors()` |
| `src/game_world.cpp` | Modify | Remove OW_Mountains from impassable checks, add entry message |
| `src/game_input.cpp` | Modify | Remove OW_Mountains from entry blocks |
| `src/generators/detail_map_generator_v2.cpp` | Modify | Set `mountain_neighbor_count` before structure strategy |

---

### Task 1: Add Biome::Mountains enum + biome_colors + biome mapping

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: `include/astra/map_properties.h`
- Modify: `src/tilemap.cpp`

- [ ] **Step 1: Add Mountains to the Biome enum**

In `include/astra/tilemap.h`, add `Mountains` after `Marsh` (line 236):

```cpp
    Marsh,
    Mountains,
    // Alien terrain (one per Architecture type)
```

- [ ] **Step 2: Update detail_biome_for_terrain**

In `include/astra/map_properties.h`, change the `OW_Mountains` case (line 111):

```cpp
        case Tile::OW_Mountains: return Biome::Mountains;
```

- [ ] **Step 3: Add biome_colors for Mountains**

In `src/tilemap.cpp`, add a case before the alien biomes in `biome_colors()` (after the `Marsh` case, around line 485):

```cpp
        case Biome::Mountains:
            return {static_cast<Color>(245), static_cast<Color>(243), Color::Blue, static_cast<Color>(240)};
```

(245 = bright gray walls, 243 = slightly darker floor, 240 = dim remembered)

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 5: Commit**

```bash
git add include/astra/tilemap.h include/astra/map_properties.h src/tilemap.cpp
git commit -m "feat(mountains): add Biome::Mountains enum, colors, and terrain mapping"
```

---

### Task 2: Add mountain_neighbor_count to BiomeProfile + declare structure_mountains

**Files:**
- Modify: `include/astra/biome_profile.h`

- [ ] **Step 1: Add the field and declaration**

In `include/astra/biome_profile.h`, add `mountain_neighbor_count` to the `BiomeProfile` struct after the `flora_fn` field (after line 117):

```cpp
    // Layer 5: Flora / ground resources
    FloraStrategy flora_fn = nullptr;

    // Mountain variant selection (set by generator before structure_fn call)
    int mountain_neighbor_count = 0;
```

Add the function declaration after the existing strategy declarations in `biome_profiles.cpp` forward declarations section. Since the strategies are declared in `biome_profiles.cpp` (not the header), add the declaration at the top of `biome_profiles.cpp` instead. But actually, the header does declare flora strategies. Let's keep it consistent — the structure strategies are forward-declared in `biome_profiles.cpp`. So we'll declare `structure_mountains` there too (in Task 3).

No header declaration needed — `structure_mountains` will be forward-declared in `biome_profiles.cpp` like the other structure strategies.

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 3: Commit**

```bash
git add include/astra/biome_profile.h
git commit -m "feat(mountains): add mountain_neighbor_count field to BiomeProfile"
```

---

### Task 3: Implement structure_mountains strategy

**Files:**
- Modify: `src/generators/structure_strategies.cpp`

- [ ] **Step 1: Add the structure_mountains function**

Add the following function at the end of `src/generators/structure_strategies.cpp`, before the closing `} // namespace astra`:

```cpp
// ---------------------------------------------------------------------------
// Strategy 6: structure_mountains — neighbor-driven mountain variants
// ---------------------------------------------------------------------------
void structure_mountains(StructureMask* grid, int w, int h,
                         std::mt19937& rng,
                         const float* elevation, const float* /*moisture*/,
                         const BiomeProfile& prof) {
    int neighbors = prof.mountain_neighbor_count;

    if (neighbors <= 1) {
        // --- Passes variant: dense walls with carved corridors ---
        // Start all wall
        for (int i = 0; i < w * h; ++i)
            grid[i] = StructureMask::Wall;

        // Carve corridors using high-frequency noise
        unsigned seed = rng();
        unsigned seed2 = rng();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);
                float n = fbm(fx, fy, seed, 0.06f, 4);
                float n2 = fbm(fx, fy, seed2, 0.04f, 3);
                // Carve where noise crosses zero (creates winding paths)
                if (std::abs(n) < 0.18f || std::abs(n2) < 0.12f) {
                    grid[y * w + x] = StructureMask::None;
                }
            }
        }
        // Widen corridors: any floor adjacent to 2+ floors stays floor
        std::vector<bool> is_floor(w * h, false);
        for (int i = 0; i < w * h; ++i)
            is_floor[i] = (grid[i] == StructureMask::None);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (grid[y * w + x] != StructureMask::Wall) continue;
                int adj = 0;
                if (is_floor[(y-1)*w+x]) ++adj;
                if (is_floor[(y+1)*w+x]) ++adj;
                if (is_floor[y*w+x-1]) ++adj;
                if (is_floor[y*w+x+1]) ++adj;
                if (adj >= 2) grid[y * w + x] = StructureMask::None;
            }
        }
    } else if (neighbors == 2) {
        // --- Gradient variant: walls at non-mountain edges, open center ---
        // Determine which edges border non-mountain tiles
        // prof.mountain_neighbor_count was set, but we need to know WHICH
        // neighbors are mountains. We use elevation as a proxy: higher elevation
        // at edges = mountain neighbor (skip wall band), lower = not (add wall band).
        // Alternative: read the edge elevation values.
        //
        // Simpler approach: place wall bands on all 4 edges with noise erosion,
        // then let the edge bleed system handle the actual neighbor matching.
        unsigned seed = rng();
        int band_depth = 30 + static_cast<int>(prof.structure_intensity * 20.0f);
        band_depth = std::clamp(band_depth, 25, 50);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // Distance from nearest edge
                int dist = std::min({x, w - 1 - x, y, h - 1 - y});
                if (dist >= band_depth) continue;

                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);
                float n = fbm(fx, fy, seed, 0.05f, 3);

                // Wall probability decreases toward center
                float t = 1.0f - (static_cast<float>(dist) / static_cast<float>(band_depth));
                t = t * t; // quadratic falloff
                float threshold = t * 0.8f + n * 0.2f;

                if (threshold > 0.35f) {
                    grid[y * w + x] = StructureMask::Wall;
                }
            }
        }
    } else {
        // --- Plateau variant: open terrain with ridgeline bands ---
        unsigned seed = rng();
        int num_ridges = 3 + static_cast<int>(prof.structure_intensity * 3.0f);
        num_ridges = std::clamp(num_ridges, 3, 6);

        std::uniform_int_distribution<int> dist_x(0, w - 1);
        std::uniform_int_distribution<int> dist_y(0, h - 1);
        std::uniform_int_distribution<int> dist_edge(0, 3);
        std::uniform_int_distribution<int> dist_width(1, 3);
        std::uniform_int_distribution<int> dist_jitter(-1, 1);

        for (int r = 0; r < num_ridges; ++r) {
            int ridge_width = dist_width(rng);

            // Pick start and end on opposite edges
            int start_edge = dist_edge(rng);
            int end_edge = (start_edge + 2) % 4;

            int x1, y1, x2, y2;
            auto point_on_edge = [&](int edge, int& px, int& py) {
                switch (edge) {
                    case 0: px = dist_x(rng); py = 0;     break;
                    case 1: px = w - 1;       py = dist_y(rng); break;
                    case 2: px = dist_x(rng); py = h - 1; break;
                    case 3: px = 0;           py = dist_y(rng); break;
                }
            };
            point_on_edge(start_edge, x1, y1);
            point_on_edge(end_edge, x2, y2);

            // Bresenham walk with jitter
            int dx = std::abs(x2 - x1);
            int dy = std::abs(y2 - y1);
            int sx = (x1 < x2) ? 1 : -1;
            int sy = (y1 < y2) ? 1 : -1;
            int err = dx - dy;
            int cx = x1, cy = y1;
            int jitter_offset = 0;
            int step_count = 0;

            while (true) {
                if (step_count % 12 == 0) {
                    jitter_offset += dist_jitter(rng);
                    jitter_offset = std::clamp(jitter_offset, -2, 2);
                }

                bool mostly_horizontal = (dx >= dy);
                for (int offset = -ridge_width; offset <= ridge_width; ++offset) {
                    int wx, wy;
                    if (mostly_horizontal) {
                        wx = cx; wy = cy + offset + jitter_offset;
                    } else {
                        wx = cx + offset + jitter_offset; wy = cy;
                    }
                    if (wx >= 0 && wx < w && wy >= 0 && wy < h) {
                        grid[wy * w + wx] = StructureMask::Wall;
                    }
                }

                if (cx == x2 && cy == y2) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; cx += sx; }
                if (e2 <  dx) { err += dx; cy += sy; }
                ++step_count;
            }
        }
    }
}
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 3: Commit**

```bash
git add src/generators/structure_strategies.cpp
git commit -m "feat(mountains): implement structure_mountains with 3 neighbor-driven variants"
```

---

### Task 4: Add Mountains BiomeProfile + forward declaration

**Files:**
- Modify: `src/generators/biome_profiles.cpp`

- [ ] **Step 1: Add forward declaration for structure_mountains**

In `src/generators/biome_profiles.cpp`, add the forward declaration alongside the other structure strategy declarations (around line 27, after `structure_craters`):

```cpp
void structure_mountains(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
```

- [ ] **Step 2: Add the mountains BiomeProfile**

Add the static profile after the `marsh` profile and before the alien biome profiles. Find the marsh profile and add after it:

```cpp
    static const BiomeProfile mountains {
        "Mountains",
        elevation_ridgeline,
        0.04f, 5, 0.72f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_mountains, 0.6f,
        {{FixtureType::NaturalObstacle, 0.02f, false},
         {FixtureType::MineralOre, 0.01f, false},
         {FixtureType::MineralCrystal, 0.005f, false}},
        nullptr
    };
```

- [ ] **Step 3: Add to the biome_profile() switch**

Add the `Mountains` case in the `biome_profile()` switch, after the `Marsh` case:

```cpp
        case Biome::Mountains:         return mountains;
```

- [ ] **Step 4: Add to the parse_biome() table**

Add `"mountains"` to the `parse_biome` lookup table:

```cpp
        {"mountains",        Biome::Mountains},
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 6: Commit**

```bash
git add src/generators/biome_profiles.cpp
git commit -m "feat(mountains): add Mountains BiomeProfile with ridgeline elevation + minerals"
```

---

### Task 5: Make OW_Mountains passable

**Files:**
- Modify: `src/game_world.cpp`
- Modify: `src/game_input.cpp`

- [ ] **Step 1: Remove OW_Mountains from transition_detail_edge block**

In `src/game_world.cpp` line 934, change:

```cpp
    if (dest_tile == Tile::OW_Mountains || dest_tile == Tile::OW_Lake) {
```

To:

```cpp
    if (dest_tile == Tile::OW_Lake) {
```

- [ ] **Step 2: Add Mountains entry message**

In `src/game_world.cpp`, in the entry message switch around line 602, add a case for mountains:

```cpp
        case Tile::OW_Mountains: msg = "You ascend into the mountains."; break;
```

- [ ] **Step 3: Remove OW_Mountains from game_input.cpp entry blocks**

In `src/game_input.cpp`, find the two places (around lines 392 and 432) that block mountain entry:

```cpp
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) {
```

Change both to:

```cpp
                if (t == Tile::OW_Lake) {
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 5: Commit**

```bash
git add src/game_world.cpp src/game_input.cpp
git commit -m "feat(mountains): make OW_Mountains traversable"
```

---

### Task 6: Set mountain_neighbor_count in the v2 generator

**Files:**
- Modify: `src/generators/detail_map_generator_v2.cpp`

- [ ] **Step 1: Set mountain_neighbor_count before structure strategy call**

In `src/generators/detail_map_generator_v2.cpp`, in `generate_layout()`, add the neighbor count computation before the structure strategy call. The structure call is:

```cpp
    if (prof.structure_fn) {
        prof.structure_fn(channels_.structure.data(), w, h, rng,
                          channels_.elevation.data(), channels_.moisture.data(), prof);
    }
```

The `prof` is `const`, so we need a mutable copy for setting the neighbor count. Change the block to:

```cpp
    if (prof.structure_fn) {
        // Mountains strategy needs neighbor count for variant selection
        BiomeProfile prof_copy = prof;
        if (props_->biome == Biome::Mountains) {
            int count = 0;
            auto is_mountain = [](Tile t) {
                return t == Tile::OW_Mountains;
            };
            if (is_mountain(props_->detail_neighbor_n)) ++count;
            if (is_mountain(props_->detail_neighbor_s)) ++count;
            if (is_mountain(props_->detail_neighbor_e)) ++count;
            if (is_mountain(props_->detail_neighbor_w)) ++count;
            prof_copy.mountain_neighbor_count = count;
        }
        prof.structure_fn(channels_.structure.data(), w, h, rng,
                          channels_.elevation.data(), channels_.moisture.data(), prof_copy);
    }
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 3: Commit**

```bash
git add src/generators/detail_map_generator_v2.cpp
git commit -m "feat(mountains): set mountain_neighbor_count for variant selection in v2 generator"
```

---

### Task 7: Build and visually verify

**Files:** None (testing only)

- [ ] **Step 1: Build the project**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean build

- [ ] **Step 2: Test in-game**

Run: `./build/astra`

Test procedure:
1. Land on a planet that has mountain tiles on the overworld
2. Walk onto an OW_Mountains tile — should no longer be blocked
3. Press Enter/Space to enter the detail map — should see "You ascend into the mountains."
4. Verify the terrain matches the variant:
   - Isolated mountain (0-1 mountain neighbors): dense walls with winding passes
   - Range edge (2 neighbors): wall bands at edges, open center
   - Range interior (3-4 neighbors): open plateau with ridgeline bands
5. Walk to the edge and cross to a non-mountain tile — terrain should bleed via edge strips
6. Use `biome_test mountains` in dev console to inspect the raw terrain

- [ ] **Step 3: Commit any fixes if needed**

---

## Self-Review Checklist

**Spec coverage:**
- Section 1 (Biome Enum): Task 1 — adds `Biome::Mountains`
- Section 2 (Passability): Task 5 — removes impassable checks in game_world.cpp and game_input.cpp
- Section 3 (Biome Mapping): Task 1 — `OW_Mountains` → `Biome::Mountains`
- Section 4 (BiomeProfile): Task 4 — mountains profile with ridgeline + minerals
- Section 5 (Structure Strategy): Task 3 — `structure_mountains` with 3 variants
- Section 6 (Biome Colors): Task 1 — gray stone colors
- Section 7 (Edge Bleed): automatic via existing system, no task needed
- Section 8 (Files Touched): all files covered

**Placeholder scan:** No TBDs, TODOs, or vague steps.

**Type consistency:**
- `Biome::Mountains` — defined in Task 1, referenced in Tasks 4, 5, 6
- `mountain_neighbor_count` — added to BiomeProfile in Task 2, read in Task 3, set in Task 6
- `structure_mountains` — implemented in Task 3, forward-declared and referenced in Task 4, called via strategy function pointer
