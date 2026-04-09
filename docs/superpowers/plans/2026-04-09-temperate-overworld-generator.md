# Temperate Overworld Generator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Voronoi region-based overworld generator for temperate terrestrial planets, producing large coherent biome patches with rivers that flow from mountains along region boundaries.

**Architecture:** `TemperateOverworldGenerator` subclass of `OverworldGeneratorBase`. Overrides `classify_terrain` (Voronoi lookup), `carve_rivers` (combined downhill + boundary-following), and `place_pois` (reuses shared POI logic). Factory updated to dispatch based on body properties.

**Tech Stack:** C++20, existing OverworldGeneratorBase infrastructure, Voronoi tessellation via brute-force nearest-seed lookup

**Spec:** `docs/superpowers/specs/2026-04-09-temperate-overworld-generator-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/overworld_generator.h` | Modify | Update factory signature, declare `place_default_pois` |
| `src/generators/default_overworld_generator.cpp` | Modify | Extract POI logic to `place_default_pois` free function |
| `src/generators/temperate_overworld_generator.cpp` | Create | Voronoi classification + A+B rivers + POI delegation |
| `src/map_generator.cpp` | Modify | Pass `MapProperties` to overworld factory, update call sites |
| `src/game_world.cpp` | Modify | Pass props to `create_generator` for overworld |
| `src/map_editor.cpp` | Modify | Pass props to `create_generator` for overworld |
| `CMakeLists.txt` | Modify | Add `temperate_overworld_generator.cpp` |

---

### Task 1: Update factory signature to accept MapProperties

**Files:**
- Modify: `include/astra/overworld_generator.h`
- Modify: `src/map_generator.cpp`
- Modify: `src/generators/default_overworld_generator.cpp`

- [ ] **Step 1: Update the factory declaration in the header**

In `include/astra/overworld_generator.h`, change the factory signature (line 66):

```cpp
// --- Factory ---
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props);
```

Also add the forward declaration for MapProperties at the top (after the existing includes):

```cpp
#include "astra/map_generator.h"
#include "astra/celestial_body.h"
#include "astra/map_properties.h"

#include <vector>
```

- [ ] **Step 2: Update the factory definition in default_overworld_generator.cpp**

At the bottom of `src/generators/default_overworld_generator.cpp`, change the factory function:

```cpp
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& /*props*/) {
    return std::make_unique<DefaultOverworldGenerator>();
}
```

- [ ] **Step 3: Update the forward declaration and call in map_generator.cpp**

In `src/map_generator.cpp`, change the forward declaration (line 378):

```cpp
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props);
```

Change `create_generator` to accept optional props. Add a new overload:

```cpp
std::unique_ptr<MapGenerator> create_generator(MapType type, const MapProperties& props) {
    if (type == MapType::Overworld) {
        return make_overworld_generator(props);
    }
    return create_generator(type);
}
```

- [ ] **Step 4: Update call sites**

In `src/game_world.cpp` (around line 1200), change:
```cpp
            auto gen = create_generator(MapType::Overworld);
```
To:
```cpp
            auto gen = create_generator(MapType::Overworld, props);
```

In `src/map_editor.cpp` (around line 810), change:
```cpp
                        auto gen = create_generator(MapType::Overworld);
```
To:
```cpp
                        auto gen = create_generator(MapType::Overworld, props);
```

Also add the new overload declaration in `map_generator.h`. Find `std::unique_ptr<MapGenerator> create_generator(MapType type);` and add after it:

```cpp
std::unique_ptr<MapGenerator> create_generator(MapType type, const MapProperties& props);
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build, still returns DefaultOverworldGenerator for everything

- [ ] **Step 6: Commit**

```bash
git add include/astra/overworld_generator.h include/astra/map_generator.h \
    src/generators/default_overworld_generator.cpp src/map_generator.cpp \
    src/game_world.cpp src/map_editor.cpp
git commit -m "feat(overworld): update factory to accept MapProperties for generator dispatch"
```

---

### Task 2: Extract POI logic to shared function

**Files:**
- Modify: `include/astra/overworld_generator.h`
- Modify: `src/generators/default_overworld_generator.cpp`

- [ ] **Step 1: Declare the shared POI function in the header**

In `include/astra/overworld_generator.h`, add after the `ow_fbm` declaration:

```cpp
// --- Shared POI placement (used by Default and Temperate generators) ---
void place_default_pois(TileMap* map, const MapProperties* props,
                        const std::vector<float>& elevation, std::mt19937& rng);
```

- [ ] **Step 2: Convert place_pois to call the shared function**

In `src/generators/default_overworld_generator.cpp`, rename the existing `place_pois` method body into a free function `place_default_pois`. The method then calls it:

Change the `place_pois` override to:

```cpp
void DefaultOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}
```

Then rename the existing `place_pois` implementation to `place_default_pois` as a free function. Change the function signature from:

```cpp
void DefaultOverworldGenerator::place_pois(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();
    ...
```

To a new free function defined BEFORE the class:

```cpp
void place_default_pois(TileMap* map, const MapProperties* props,
                        const std::vector<float>& /*elevation*/, std::mt19937& rng) {
    int w = map->width();
    int h = map->height();
    ...
```

Replace all `map_->` with `map->` and `props_->` with `props->` in the extracted function.

Then the class method becomes just:

```cpp
void DefaultOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build, behavior unchanged

- [ ] **Step 4: Commit**

```bash
git add include/astra/overworld_generator.h src/generators/default_overworld_generator.cpp
git commit -m "refactor(overworld): extract POI placement to shared place_default_pois function"
```

---

### Task 3: Create TemperateOverworldGenerator

**Files:**
- Create: `src/generators/temperate_overworld_generator.cpp`
- Modify: `CMakeLists.txt`

This is the core task. The generator uses Voronoi tessellation for terrain classification and combined A+B river generation.

- [ ] **Step 1: Create the generator file**

```cpp
// src/generators/temperate_overworld_generator.cpp
#include "astra/overworld_generator.h"
#include "astra/lore_influence_map.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// TemperateOverworldGenerator — Voronoi region-based terrain for temperate
// terrestrial planets. Large coherent biome patches with A+B rivers.
// ---------------------------------------------------------------------------

class TemperateOverworldGenerator : public OverworldGeneratorBase {
protected:
    void configure_noise(float& elev_scale, float& moist_scale,
                         const TerrainContext& ctx) override;
    Tile classify_terrain(int x, int y, float elev, float moist,
                          const TerrainContext& ctx) override;
    void carve_rivers(std::mt19937& rng) override;
    void place_pois(std::mt19937& rng) override;

private:
    // Voronoi seed data (populated during first classify_terrain call)
    struct VoronoiSeed {
        float x, y;
        Tile biome;
    };
    std::vector<VoronoiSeed> seeds_;
    bool seeds_initialized_ = false;

    void init_seeds(std::mt19937& rng);
    Tile biome_for_elevation_moisture(float elev, float moist);

    // Find the index of the nearest seed to (x, y)
    int nearest_seed(float x, float y) const;
    // Find the second-nearest seed
    int second_nearest_seed(float x, float y, int skip) const;
    // Distance from (x,y) to nearest Voronoi boundary
    float distance_to_boundary(float x, float y) const;

    // RNG stored from generate_layout for seed initialization
    std::mt19937* layout_rng_ = nullptr;
};

// ---------------------------------------------------------------------------
// Noise configuration
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                   const TerrainContext& /*ctx*/) {
    elev_scale = 0.08f;
    moist_scale = 0.12f;
}

// ---------------------------------------------------------------------------
// Voronoi seed initialization
// ---------------------------------------------------------------------------

Tile TemperateOverworldGenerator::biome_for_elevation_moisture(float elev, float moist) {
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev < 0.25f) return Tile::OW_Lake;
    if (elev < 0.35f && moist > 0.5f) return Tile::OW_Swamp;
    if (moist > 0.6f) return Tile::OW_Forest;
    if (moist > 0.3f) return Tile::OW_Plains;
    return Tile::OW_Desert;
}

void TemperateOverworldGenerator::init_seeds(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    std::uniform_int_distribution<int> count_dist(15, 25);
    int num_seeds = count_dist(rng);

    std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(w));
    std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(h));

    seeds_.clear();
    seeds_.reserve(num_seeds);

    for (int i = 0; i < num_seeds; ++i) {
        float sx = x_dist(rng);
        float sy = y_dist(rng);
        int ix = std::clamp(static_cast<int>(sx), 0, w - 1);
        int iy = std::clamp(static_cast<int>(sy), 0, h - 1);
        float elev = elevation_[iy * w + ix];
        float moist = moisture_[iy * w + ix];
        Tile biome = biome_for_elevation_moisture(elev, moist);
        seeds_.push_back({sx, sy, biome});
    }

    seeds_initialized_ = true;
}

int TemperateOverworldGenerator::nearest_seed(float x, float y) const {
    int best = 0;
    float best_dist = 1e9f;
    for (int i = 0; i < static_cast<int>(seeds_.size()); ++i) {
        float dx = x - seeds_[i].x;
        float dy = y - seeds_[i].y;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

int TemperateOverworldGenerator::second_nearest_seed(float x, float y, int skip) const {
    int best = -1;
    float best_dist = 1e9f;
    for (int i = 0; i < static_cast<int>(seeds_.size()); ++i) {
        if (i == skip) continue;
        float dx = x - seeds_[i].x;
        float dy = y - seeds_[i].y;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

float TemperateOverworldGenerator::distance_to_boundary(float x, float y) const {
    int n1 = nearest_seed(x, y);
    int n2 = second_nearest_seed(x, y, n1);
    if (n2 < 0) return 1e9f;

    float dx1 = x - seeds_[n1].x, dy1 = y - seeds_[n1].y;
    float dx2 = x - seeds_[n2].x, dy2 = y - seeds_[n2].y;
    float d1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float d2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    return d2 - d1; // 0 at boundary, positive inside cell
}

// ---------------------------------------------------------------------------
// classify_terrain — Voronoi lookup with edge smoothing
// ---------------------------------------------------------------------------

Tile TemperateOverworldGenerator::classify_terrain(int x, int y, float elev, float moist,
                                                    const TerrainContext& /*ctx*/) {
    if (!seeds_initialized_) {
        // Lazy init — we need rng but classify_terrain doesn't receive it.
        // Seeds are initialized in carve_rivers or we use a workaround.
        // Actually, init from a deterministic seed based on map dimensions.
        std::mt19937 seed_rng(static_cast<unsigned>(map_->width() * 7919 + map_->height() * 6271));
        init_seeds(seed_rng);
    }

    // Extreme elevation overrides Voronoi
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev < 0.25f) return Tile::OW_Lake;

    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    int n1 = nearest_seed(fx, fy);
    Tile biome = seeds_[n1].biome;

    // Skip Voronoi for seeds that are mountains/lakes (already handled by override)
    if (biome == Tile::OW_Mountains) return Tile::OW_Mountains;
    if (biome == Tile::OW_Lake) return Tile::OW_Lake;

    // Edge smoothing: at Voronoi boundaries, probabilistically pick between
    // the two nearest biomes based on local noise
    float boundary_dist = distance_to_boundary(fx, fy);
    if (boundary_dist < 2.0f) {
        int n2 = second_nearest_seed(fx, fy, n1);
        if (n2 >= 0) {
            Tile biome2 = seeds_[n2].biome;
            // Use local elevation as noise source for boundary jitter
            float jitter = (elev - 0.5f) * 2.0f; // -1 to 1
            float threshold = boundary_dist / 2.0f; // 0 at boundary, 1 at edge of zone
            if (jitter > threshold) {
                biome = biome2;
            }
        }
    }

    return biome;
}

// ---------------------------------------------------------------------------
// carve_rivers — Combined A+B: downhill from mountains, bending toward
// Voronoi boundaries as elevation decreases
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::carve_rivers(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Ensure seeds are initialized
    if (!seeds_initialized_) {
        init_seeds(rng);
    }

    // Find river sources: mountain-adjacent tiles at high elevation
    struct Pos { int x, y; };
    std::vector<Pos> sources;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float e = elevation_[y * w + x];
            if (e < 0.55f || e > 0.72f) continue;
            Tile t = map_->get(x, y);
            if (t == Tile::OW_Mountains || t == Tile::OW_Lake) continue;

            bool adj_mountain = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    if (map_->get(x + dx, y + dy) == Tile::OW_Mountains)
                        adj_mountain = true;
                }
            }
            if (adj_mountain) sources.push_back({x, y});
        }
    }

    if (sources.empty()) return;
    std::shuffle(sources.begin(), sources.end(), rng);

    // River count: 3 + mountain_seeds/3, clamped 3-6
    int mountain_seeds = 0;
    for (const auto& s : seeds_) {
        if (s.biome == Tile::OW_Mountains) ++mountain_seeds;
    }
    int num_rivers = std::clamp(3 + mountain_seeds / 3, 3, 6);
    num_rivers = std::min(num_rivers, static_cast<int>(sources.size()));

    for (int r = 0; r < num_rivers; ++r) {
        int cx = sources[r].x;
        int cy = sources[r].y;
        std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));

        for (int step = 0; step < 100; ++step) {
            if (cx <= 0 || cx >= w - 1 || cy <= 0 || cy >= h - 1) break;

            Tile cur = map_->get(cx, cy);
            if (cur == Tile::OW_Lake || cur == Tile::OW_River) break;
            if (cur != Tile::OW_Mountains) {
                map_->set(cx, cy, Tile::OW_River);
            }
            visited[cy][cx] = true;

            float cur_elev = elevation_[cy * w + cx];

            // Score each neighbor: blend downhill and boundary pull
            float downhill_weight = cur_elev * 2.0f;
            float boundary_weight = (1.0f - cur_elev) * 1.5f;

            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};

            float best_score = -1e9f;
            int bx = -1, by = -1;

            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny][nx]) continue;
                if (map_->get(nx, ny) == Tile::OW_Mountains) continue;

                float ne = elevation_[ny * w + nx];
                float downhill = (cur_elev - ne) * downhill_weight;

                float bd = distance_to_boundary(static_cast<float>(nx), static_cast<float>(ny));
                float boundary = (1.0f / (bd + 1.0f)) * boundary_weight;

                float score = downhill + boundary;
                if (score > best_score) {
                    best_score = score;
                    bx = nx;
                    by = ny;
                }
            }

            if (bx < 0) {
                // Basin: flood a small lake
                int lake_size = 3 + static_cast<int>(rng() % 6);
                std::vector<std::pair<int,int>> lake_candidates;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int lx = cx + dx, ly = cy + dy;
                        if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
                        Tile lt = map_->get(lx, ly);
                        if (lt == Tile::OW_Mountains || lt == Tile::OW_Lake) continue;
                        lake_candidates.push_back({lx, ly});
                    }
                }
                // Sort by elevation, fill lowest
                std::sort(lake_candidates.begin(), lake_candidates.end(),
                    [&](const auto& a, const auto& b) {
                        return elevation_[a.second * w + a.first] <
                               elevation_[b.second * w + b.first];
                    });
                for (int i = 0; i < std::min(lake_size, static_cast<int>(lake_candidates.size())); ++i) {
                    map_->set(lake_candidates[i].first, lake_candidates[i].second, Tile::OW_Lake);
                }
                break;
            }

            cx = bx;
            cy = by;
        }
    }
}

// ---------------------------------------------------------------------------
// place_pois — delegate to shared default POI logic
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/temperate_overworld_generator.cpp` after `default_overworld_generator.cpp` in the source list.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build (generator not wired to factory yet)

- [ ] **Step 4: Commit**

```bash
git add src/generators/temperate_overworld_generator.cpp CMakeLists.txt
git commit -m "feat(overworld): add TemperateOverworldGenerator with Voronoi regions + A+B rivers"
```

---

### Task 4: Wire factory to dispatch TemperateOverworldGenerator

**Files:**
- Modify: `src/generators/default_overworld_generator.cpp`

- [ ] **Step 1: Update the factory to dispatch based on body properties**

In `src/generators/default_overworld_generator.cpp`, change the factory function at the bottom. First add a forward declaration for the temperate generator before the factory:

```cpp
// Forward declaration
std::unique_ptr<MapGenerator> make_temperate_overworld_generator();
```

Then update the factory:

```cpp
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props) {
    if (props.body_type == BodyType::Terrestrial &&
        props.body_temperature == Temperature::Temperate &&
        (props.body_atmosphere == Atmosphere::Standard ||
         props.body_atmosphere == Atmosphere::Dense)) {
        return make_temperate_overworld_generator();
    }
    return std::make_unique<DefaultOverworldGenerator>();
}
```

- [ ] **Step 2: Add the temperate factory function in the temperate generator file**

At the bottom of `src/generators/temperate_overworld_generator.cpp`, before the closing `} // namespace astra`, add:

```cpp
std::unique_ptr<MapGenerator> make_temperate_overworld_generator() {
    return std::make_unique<TemperateOverworldGenerator>();
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 4: Test in-game**

Run: `./build/astra-dev`

1. Start a new game or travel to Sol system
2. Land on Earth (Terrestrial, Temperate, Standard atmosphere)
3. The overworld should show large coherent biome regions instead of per-tile noise
4. Rivers should flow from mountains toward region boundaries
5. Non-temperate planets (Mars, Mercury, etc.) should still use the default generator

- [ ] **Step 5: Commit**

```bash
git add src/generators/default_overworld_generator.cpp src/generators/temperate_overworld_generator.cpp
git commit -m "feat(overworld): wire TemperateOverworldGenerator for temperate terrestrial bodies"
```

---

### Task 5: Fix seed initialization timing

**Files:**
- Modify: `src/generators/temperate_overworld_generator.cpp`
- Modify: `src/generators/overworld_generator_base.cpp`

The Voronoi seeds need to be initialized BEFORE `classify_terrain` is called, but `classify_terrain` doesn't receive the RNG. The base class's `generate_layout` calls `classify_terrain` per-tile after noise generation.

- [ ] **Step 1: Add a pre-classification hook to the base class**

In `include/astra/overworld_generator.h`, add a new virtual hook in the base class:

```cpp
    // Called after noise generation, before terrain classification.
    // Use for pre-classification setup (e.g., Voronoi seed placement).
    virtual void pre_classify(std::mt19937& rng);
```

In `src/generators/overworld_generator_base.cpp`, add the default implementation (no-op):

```cpp
void OverworldGeneratorBase::pre_classify(std::mt19937& /*rng*/) {
    // Default: nothing to do
}
```

And call it in `generate_layout` between noise generation and classification:

```cpp
    // Step 3.5: Pre-classification hook
    pre_classify(rng);

    // Step 4: Classify terrain (virtual hook)
```

- [ ] **Step 2: Override pre_classify in TemperateOverworldGenerator**

In the temperate generator, override `pre_classify` to initialize Voronoi seeds:

Add to the class declaration:
```cpp
    void pre_classify(std::mt19937& rng) override;
```

Add the implementation:
```cpp
void TemperateOverworldGenerator::pre_classify(std::mt19937& rng) {
    init_seeds(rng);
}
```

Remove the lazy initialization workaround from `classify_terrain`:

```cpp
Tile TemperateOverworldGenerator::classify_terrain(int x, int y, float elev, float moist,
                                                    const TerrainContext& /*ctx*/) {
    // Extreme elevation overrides Voronoi
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev < 0.25f) return Tile::OW_Lake;
    // ... rest unchanged (remove the seeds_initialized_ check block)
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Clean build

- [ ] **Step 4: Commit**

```bash
git add include/astra/overworld_generator.h src/generators/overworld_generator_base.cpp \
    src/generators/temperate_overworld_generator.cpp
git commit -m "feat(overworld): add pre_classify hook for Voronoi seed initialization"
```

---

## Self-Review Checklist

**Spec coverage:**
- Section 1 (Voronoi Classification): Task 3 — seed placement, biome assignment, edge smoothing
- Section 2 (River Generation): Task 3 — combined A+B scoring, lake formation at basins
- Section 3 (Factory Changes): Tasks 1 + 4 — factory accepts MapProperties, dispatches temperate
- Section 4 (POI Placement): Task 2 — extracted to shared function, temperate delegates to it
- Section 5 (Files): All files covered
- Section 6/7 (Unchanged/Out of scope): Verified — no changes to detail maps, other body types, etc.

**Placeholder scan:** No TBDs, TODOs, or vague steps. All code blocks complete.

**Type consistency:**
- `make_overworld_generator(const MapProperties&)` — declared in Task 1 header, defined in Task 1 default, updated in Task 4
- `place_default_pois(TileMap*, const MapProperties*, const std::vector<float>&, std::mt19937&)` — declared in Task 2 header, defined in Task 2 default, called in Task 3
- `make_temperate_overworld_generator()` — forward-declared in Task 4 default, defined in Task 4 temperate
- `pre_classify(std::mt19937&)` — declared in Task 5 header, default in Task 5 base, override in Task 5 temperate
- `TemperateOverworldGenerator` class — defined in Task 3, wired in Task 4, refined in Task 5
