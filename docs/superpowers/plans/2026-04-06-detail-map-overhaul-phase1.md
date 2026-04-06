# Detail Map Overhaul Phase 1: Base Natural Terrain — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat noise-threshold detail map generator with a composable terrain feature system that produces distinct, interesting terrain per biome.

**Architecture:** Biomes are defined as compositions of reusable terrain features (RidgeWalk, DenseCanopy, IslandArchipelago, etc.). Features are functions that take a TileMap + noise field and carve geography. The detail map generator orchestrates: noise → features → edge blending → connectivity → scatter. Existing POI handlers and scatter system are preserved.

**Tech Stack:** C++20, existing fBm noise utilities, terminal renderer.

**Spec:** `docs/superpowers/specs/2026-04-06-detail-map-overhaul-design.md`

**Build:** `cmake -B build -DDEV=ON && cmake --build build`

**Run:** `./build/astra-dev`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/generators/terrain_features.h` | Create | TerrainFeature type, FeatureConfig, BiomeDefinition, biome_definition(), noise field struct |
| `src/generators/terrain_features.cpp` | Create | All terrain feature implementations + biome definition table |
| `src/generators/detail_map_generator.cpp` | Rewrite | Orchestration: noise → features → edges → connectivity → scatter + POI |
| `CMakeLists.txt` | Modify | Add terrain_features.cpp |

---

## Task 1: Terrain Feature System — Types and Interface

**Files:**
- Create: `src/generators/terrain_features.h`

- [ ] **Step 1: Create the header with core types**

```cpp
#pragma once

#include "astra/tilemap.h"
#include "astra/world_constants.h"

#include <functional>
#include <random>
#include <vector>

namespace astra {

// Raw noise field generated in Phase 1 of detail map generation.
// Features read this to shape terrain relative to the underlying noise.
struct NoiseField {
    int width = 0;
    int height = 0;
    std::vector<float> elevation;  // 0.0-1.0
    float scale = 0.08f;          // noise frequency used
    unsigned seed = 0;
};

// Configuration for a single terrain feature invocation.
struct FeatureConfig {
    float intensity = 0.5f;  // 0.0-1.0: how dominant this feature is
    Tile wall_tile = Tile::Wall;
    Tile floor_tile = Tile::Floor;
    Tile water_tile = Tile::Water;
};

// A terrain feature function signature.
// Modifies the TileMap in place, reading the noise field for guidance.
using TerrainFeatureFn = void(*)(TileMap& map, std::mt19937& rng,
                                  const NoiseField& noise,
                                  const FeatureConfig& cfg);

// A single feature invocation in a biome definition.
struct FeatureSpec {
    TerrainFeatureFn fn;
    FeatureConfig config;
};

// Complete biome definition: base density + ordered feature list.
struct BiomeDefinition {
    float base_wall_density = 0.1f;
    float base_water_density = 0.0f;
    std::vector<FeatureSpec> features;  // structural first, detail second
};

// Look up the terrain feature composition for a biome.
const BiomeDefinition& biome_definition(Biome b);

// ── Terrain feature functions ──
// Each carves/places geometry into the map using the noise field.

void feature_ridge_walk(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg);
void feature_cliff_band(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg);
void feature_dense_canopy(TileMap& map, std::mt19937& rng,
                          const NoiseField& noise, const FeatureConfig& cfg);
void feature_clearing(TileMap& map, std::mt19937& rng,
                      const NoiseField& noise, const FeatureConfig& cfg);
void feature_island_archipelago(TileMap& map, std::mt19937& rng,
                                const NoiseField& noise, const FeatureConfig& cfg);
void feature_dune_field(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg);
void feature_crater_bowl(TileMap& map, std::mt19937& rng,
                         const NoiseField& noise, const FeatureConfig& cfg);
void feature_pool_cluster(TileMap& map, std::mt19937& rng,
                          const NoiseField& noise, const FeatureConfig& cfg);
void feature_lava_channels(TileMap& map, std::mt19937& rng,
                           const NoiseField& noise, const FeatureConfig& cfg);
void feature_narrow_pass(TileMap& map, std::mt19937& rng,
                         const NoiseField& noise, const FeatureConfig& cfg);

// ── Noise utilities (shared with detail_map_generator) ──

float tf_hash_noise(int x, int y, unsigned seed);
float tf_smooth_noise(float fx, float fy, unsigned seed);
float tf_fbm(float x, float y, unsigned seed, float scale, int octaves = 4);

NoiseField generate_noise_field(int w, int h, unsigned seed, float scale = 0.08f);

}  // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile (header only, no consumers yet).

- [ ] **Step 3: Commit**

```bash
git add src/generators/terrain_features.h
git commit -m "feat: add terrain feature system types and interface"
```

---

## Task 2: Noise Utilities and First Feature — RidgeWalk

**Files:**
- Create: `src/generators/terrain_features.cpp`
- Modify: `CMakeLists.txt` (add to source list)

- [ ] **Step 1: Create terrain_features.cpp with noise utilities**

```cpp
#include "terrain_features.h"

#include <algorithm>
#include <cmath>

namespace astra {

// ── Noise (same algorithms as detail_map_generator, extracted) ──

float tf_hash_noise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

float tf_smooth_noise(float fx, float fy, unsigned seed) {
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float dx = fx - ix, dy = fy - iy;
    float sx = dx * dx * (3.0f - 2.0f * dx);
    float sy = dy * dy * (3.0f - 2.0f * dy);
    float n00 = tf_hash_noise(ix, iy, seed);
    float n10 = tf_hash_noise(ix + 1, iy, seed);
    float n01 = tf_hash_noise(ix, iy + 1, seed);
    float n11 = tf_hash_noise(ix + 1, iy + 1, seed);
    float top = n00 + sx * (n10 - n00);
    float bot = n01 + sx * (n11 - n01);
    return top + sy * (bot - top);
}

float tf_fbm(float x, float y, unsigned seed, float scale, int octaves) {
    float value = 0.0f, amplitude = 1.0f, total_amp = 0.0f, freq = scale;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * tf_smooth_noise(x * freq, y * freq, seed + i * 31u);
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

NoiseField generate_noise_field(int w, int h, unsigned seed, float scale) {
    NoiseField nf;
    nf.width = w;
    nf.height = h;
    nf.scale = scale;
    nf.seed = seed;
    nf.elevation.resize(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            nf.elevation[y * w + x] = tf_fbm(static_cast<float>(x),
                                               static_cast<float>(y), seed, scale);
    return nf;
}

// Helper: check if tile at (x,y) is floor-like (safe to carve into)
static bool is_floor(const TileMap& map, int x, int y) {
    Tile t = map.get(x, y);
    return t == Tile::Floor || t == Tile::IndoorFloor;
}

// Helper: set tile only if in bounds
static void set_if_bounds(TileMap& map, int x, int y, Tile t) {
    if (x >= 0 && x < map.width() && y >= 0 && y < map.height())
        map.set(x, y, t);
}

} // namespace astra
```

- [ ] **Step 2: Implement RidgeWalk**

RidgeWalk creates mountain ridges by doing random walks and extruding walls perpendicular to the walk direction. Creates narrow passes where ridges don't quite connect.

Add to `terrain_features.cpp`:

```cpp
void feature_ridge_walk(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_ridges = 2 + static_cast<int>(cfg.intensity * 3);  // 2-5 ridges

    std::uniform_int_distribution<int> xdist(w / 6, w * 5 / 6);
    std::uniform_int_distribution<int> ydist(h / 6, h * 5 / 6);
    std::uniform_int_distribution<int> dir_dist(0, 3);  // N/S/E/W
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    for (int r = 0; r < num_ridges; ++r) {
        int cx = xdist(rng), cy = ydist(rng);
        int steps = 15 + static_cast<int>(cfg.intensity * 25);
        int dx = 0, dy = 0;

        // Initial direction
        switch (dir_dist(rng)) {
            case 0: dx = 0; dy = -1; break;  // N
            case 1: dx = 0; dy = 1;  break;  // S
            case 2: dx = -1; dy = 0; break;  // W
            case 3: dx = 1; dy = 0;  break;  // E
        }

        for (int s = 0; s < steps; ++s) {
            // Place wall at walk position
            set_if_bounds(map, cx, cy, cfg.wall_tile);

            // Extrude perpendicular walls (ridge thickness)
            int thickness = 1 + static_cast<int>(cfg.intensity * 2);
            for (int t = 1; t <= thickness; ++t) {
                if (dx == 0) {  // walking N/S, extrude E/W
                    set_if_bounds(map, cx - t, cy, cfg.wall_tile);
                    set_if_bounds(map, cx + t, cy, cfg.wall_tile);
                } else {  // walking E/W, extrude N/S
                    set_if_bounds(map, cx, cy - t, cfg.wall_tile);
                    set_if_bounds(map, cx, cy + t, cfg.wall_tile);
                }
            }

            // Advance with noise-guided wandering
            float n = noise.elevation[(std::clamp(cy, 0, h-1)) * w + std::clamp(cx, 0, w-1)];
            if (prob(rng) < 0.3f) {
                // Occasionally turn
                if (dx == 0) { dx = (prob(rng) < 0.5f) ? -1 : 1; dy = 0; }
                else { dy = (prob(rng) < 0.5f) ? -1 : 1; dx = 0; }
            }
            cx += dx;
            cy += dy;

            // Stay in bounds
            cx = std::clamp(cx, 2, w - 3);
            cy = std::clamp(cy, 2, h - 3);
        }
    }
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/generators/terrain_features.cpp` to the source list.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add src/generators/terrain_features.cpp src/generators/terrain_features.h CMakeLists.txt
git commit -m "feat: terrain feature system with noise utils and RidgeWalk"
```

---

## Task 3: Core Structural Features — CliffBand, DenseCanopy, IslandArchipelago

**Files:**
- Modify: `src/generators/terrain_features.cpp`

- [ ] **Step 1: Implement CliffBand**

Horizontal or vertical wall bands with noise-modulated edges. Creates cliff faces.

```cpp
void feature_cliff_band(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_bands = 1 + static_cast<int>(cfg.intensity * 2);  // 1-3 bands
    std::uniform_int_distribution<int> orient(0, 1);  // 0=horizontal, 1=vertical

    for (int b = 0; b < num_bands; ++b) {
        bool horizontal = orient(rng) == 0;
        int length = horizontal ? w : h;
        int span = horizontal ? h : w;

        // Band position: somewhere in the middle third
        int pos = span / 3 + static_cast<int>(rng()) % (span / 3);
        int band_width = 2 + static_cast<int>(cfg.intensity * 3);

        for (int i = 2; i < length - 2; ++i) {
            // Noise-modulated edge
            float n = tf_fbm(static_cast<float>(i), static_cast<float>(b),
                             noise.seed + 500u + b * 71u, 0.1f, 3);
            int offset = static_cast<int>((n - 0.5f) * 6.0f);

            for (int t = 0; t < band_width; ++t) {
                int px = horizontal ? i : (pos + t + offset);
                int py = horizontal ? (pos + t + offset) : i;
                set_if_bounds(map, px, py, cfg.wall_tile);
            }
        }

        // Carve 1-2 gaps (passes through the cliff)
        int num_gaps = 1 + rng() % 2;
        for (int g = 0; g < num_gaps; ++g) {
            int gap_pos = length / 4 + static_cast<int>(rng()) % (length / 2);
            int gap_width = 3 + rng() % 3;
            for (int i = gap_pos; i < gap_pos + gap_width && i < length - 2; ++i) {
                for (int t = -1; t <= band_width; ++t) {
                    int px = horizontal ? i : (pos + t);
                    int py = horizontal ? (pos + t) : i;
                    set_if_bounds(map, px, py, cfg.floor_tile);
                }
            }
        }
    }
}
```

- [ ] **Step 2: Implement DenseCanopy**

High-density noise walls with winding organic paths carved through. Creates forest/jungle feel.

```cpp
void feature_dense_canopy(TileMap& map, std::mt19937& rng,
                          const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    unsigned path_seed = static_cast<unsigned>(rng());

    // Fill with walls based on noise + intensity
    float threshold = 0.35f + (1.0f - cfg.intensity) * 0.3f;  // higher intensity = more walls
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            float n = noise.elevation[y * w + x];
            if (n > threshold)
                map.set(x, y, cfg.wall_tile);
        }
    }

    // Carve 3-5 winding organic paths through the canopy
    int num_paths = 3 + static_cast<int>(cfg.intensity * 2);
    for (int p = 0; p < num_paths; ++p) {
        // Start from a random edge
        bool from_left = rng() % 2 == 0;
        int px = from_left ? 2 : w - 3;
        int py = h / 4 + static_cast<int>(rng()) % (h / 2);
        int target_x = from_left ? w - 3 : 2;

        int step = from_left ? 1 : -1;
        while ((from_left && px < target_x) || (!from_left && px > target_x)) {
            // Carve 2-wide path
            for (int dy = -1; dy <= 1; ++dy)
                set_if_bounds(map, px, py + dy, cfg.floor_tile);

            // Advance with wander
            px += step;
            float wander = tf_fbm(static_cast<float>(px), static_cast<float>(py),
                                  path_seed + p * 37u, 0.06f, 3);
            py += (wander > 0.55f) ? 1 : (wander < 0.45f) ? -1 : 0;
            py = std::clamp(py, 3, h - 4);
        }
    }
}
```

- [ ] **Step 3: Implement IslandArchipelago**

Flood-fill water with noise-shaped land masses. Creates swamp/aquatic terrain.

```cpp
void feature_island_archipelago(TileMap& map, std::mt19937& rng,
                                const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();

    // Start with water everywhere, then carve islands using noise
    float land_threshold = 0.35f + (1.0f - cfg.intensity) * 0.2f;

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float n = noise.elevation[y * w + x];
            // Secondary noise for island shape variety
            float n2 = tf_fbm(static_cast<float>(x), static_cast<float>(y),
                              noise.seed + 777u, 0.12f, 3);
            float combined = n * 0.6f + n2 * 0.4f;

            if (combined < land_threshold) {
                map.set(x, y, cfg.water_tile);
            } else {
                map.set(x, y, cfg.floor_tile);
                // High points become walls (terrain features on islands)
                if (combined > land_threshold + 0.25f && rng() % 3 == 0)
                    map.set(x, y, cfg.wall_tile);
            }
        }
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add src/generators/terrain_features.cpp
git commit -m "feat: add CliffBand, DenseCanopy, IslandArchipelago terrain features"
```

---

## Task 4: Detail Features — Clearing, DuneField, CraterBowl, PoolCluster, LavaChannels

**Files:**
- Modify: `src/generators/terrain_features.cpp`

- [ ] **Step 1: Implement Clearing**

Circular/irregular open areas within dense terrain.

```cpp
void feature_clearing(TileMap& map, std::mt19937& rng,
                      const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_clearings = 1 + static_cast<int>(cfg.intensity * 3);  // 1-4

    std::uniform_int_distribution<int> xdist(w / 5, w * 4 / 5);
    std::uniform_int_distribution<int> ydist(h / 5, h * 4 / 5);

    for (int c = 0; c < num_clearings; ++c) {
        int cx = xdist(rng), cy = ydist(rng);
        float radius = 3.0f + cfg.intensity * 6.0f;  // 3-9 tiles

        for (int y = static_cast<int>(cy - radius - 1); y <= static_cast<int>(cy + radius + 1); ++y) {
            for (int x = static_cast<int>(cx - radius - 1); x <= static_cast<int>(cx + radius + 1); ++x) {
                if (x < 1 || x >= w - 1 || y < 1 || y >= h - 1) continue;
                float dx = static_cast<float>(x - cx);
                float dy = static_cast<float>(y - cy);
                float dist = std::sqrt(dx * dx + dy * dy);

                // Noise-modulated edge for organic shape
                float edge_noise = tf_fbm(static_cast<float>(x), static_cast<float>(y),
                                          noise.seed + 300u + c * 53u, 0.15f, 2);
                float effective_radius = radius * (0.7f + 0.6f * edge_noise);

                if (dist < effective_radius)
                    map.set(x, y, cfg.floor_tile);
            }
        }
    }
}
```

- [ ] **Step 2: Implement DuneField**

Rolling sine-wave wall bands with gaps. Creates desert terrain.

```cpp
void feature_dune_field(TileMap& map, std::mt19937& rng,
                        const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_dunes = 3 + static_cast<int>(cfg.intensity * 4);  // 3-7 dune lines
    unsigned dune_seed = static_cast<unsigned>(rng());

    for (int d = 0; d < num_dunes; ++d) {
        // Each dune is a horizontal wavy line
        int base_y = (d + 1) * h / (num_dunes + 1);
        float wavelength = 0.08f + 0.04f * (rng() % 100) / 100.0f;
        float amplitude = 2.0f + cfg.intensity * 3.0f;
        int thickness = 1 + static_cast<int>(cfg.intensity);

        for (int x = 2; x < w - 2; ++x) {
            float wave = std::sin(static_cast<float>(x) * wavelength +
                                  tf_fbm(static_cast<float>(x), 0.0f,
                                         dune_seed + d * 41u, 0.05f, 2) * 3.0f);
            int y = base_y + static_cast<int>(wave * amplitude);

            for (int t = 0; t < thickness; ++t)
                set_if_bounds(map, x, y + t, cfg.wall_tile);
        }
    }
}
```

- [ ] **Step 3: Implement CraterBowl**

Radial distance-based depression with rim walls.

```cpp
void feature_crater_bowl(TileMap& map, std::mt19937& rng,
                         const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_craters = 1 + static_cast<int>(cfg.intensity);  // 1-2

    std::uniform_int_distribution<int> xdist(w / 4, w * 3 / 4);
    std::uniform_int_distribution<int> ydist(h / 4, h * 3 / 4);

    for (int c = 0; c < num_craters; ++c) {
        int cx = xdist(rng), cy = ydist(rng);
        float outer_r = 6.0f + cfg.intensity * 8.0f;  // 6-14
        float inner_r = outer_r * 0.7f;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float dx = static_cast<float>(x - cx);
                float dy = static_cast<float>(y - cy);
                float dist = std::sqrt(dx * dx + dy * dy);

                float rim_noise = tf_fbm(static_cast<float>(x), static_cast<float>(y),
                                         noise.seed + 600u + c * 83u, 0.12f, 2) * 0.15f;

                if (dist < outer_r + rim_noise * outer_r && dist > inner_r) {
                    map.set(x, y, cfg.wall_tile);  // rim
                } else if (dist <= inner_r) {
                    // Interior: mostly floor with some water at center
                    if (dist < inner_r * 0.3f)
                        map.set(x, y, cfg.water_tile);  // crater pool
                    else
                        map.set(x, y, cfg.floor_tile);
                }
            }
        }
    }
}
```

- [ ] **Step 4: Implement PoolCluster**

Scattered water pools with noise edges.

```cpp
void feature_pool_cluster(TileMap& map, std::mt19937& rng,
                          const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_pools = 3 + static_cast<int>(cfg.intensity * 5);  // 3-8

    std::uniform_int_distribution<int> xdist(4, w - 5);
    std::uniform_int_distribution<int> ydist(4, h - 5);

    for (int p = 0; p < num_pools; ++p) {
        int cx = xdist(rng), cy = ydist(rng);
        float radius = 1.5f + cfg.intensity * 3.0f;  // 1.5-4.5

        for (int y = static_cast<int>(cy - radius - 1); y <= static_cast<int>(cy + radius + 1); ++y) {
            for (int x = static_cast<int>(cx - radius - 1); x <= static_cast<int>(cx + radius + 1); ++x) {
                if (x < 1 || x >= w - 1 || y < 1 || y >= h - 1) continue;
                // Don't overwrite walls
                if (map.get(x, y) == Tile::Wall) continue;

                float dx = static_cast<float>(x - cx);
                float dy = static_cast<float>(y - cy);
                float dist = std::sqrt(dx * dx + dy * dy);

                float edge_noise = tf_fbm(static_cast<float>(x), static_cast<float>(y),
                                          noise.seed + 900u + p * 67u, 0.2f, 2);
                if (dist < radius * (0.6f + 0.8f * edge_noise))
                    map.set(x, y, cfg.water_tile);
            }
        }
    }
}
```

- [ ] **Step 5: Implement LavaChannels**

Branching liquid channels cutting through rock.

```cpp
void feature_lava_channels(TileMap& map, std::mt19937& rng,
                           const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    int num_channels = 2 + static_cast<int>(cfg.intensity * 2);  // 2-4
    unsigned chan_seed = static_cast<unsigned>(rng());

    for (int c = 0; c < num_channels; ++c) {
        // Start from top or left edge
        bool from_top = rng() % 2 == 0;
        int px = from_top ? (w / 4 + static_cast<int>(rng()) % (w / 2)) : 2;
        int py = from_top ? 2 : (h / 4 + static_cast<int>(rng()) % (h / 2));

        int steps = std::max(w, h);
        for (int s = 0; s < steps; ++s) {
            // Place water (lava) in a 2-wide channel
            for (int t = -1; t <= 1; ++t) {
                if (from_top)
                    set_if_bounds(map, px + t, py, cfg.water_tile);
                else
                    set_if_bounds(map, px, py + t, cfg.water_tile);
            }

            // Advance downward/rightward with wander
            if (from_top) {
                py++;
                float wander = tf_fbm(static_cast<float>(px), static_cast<float>(py),
                                      chan_seed + c * 43u, 0.08f, 3);
                px += (wander > 0.6f) ? 1 : (wander < 0.4f) ? -1 : 0;
            } else {
                px++;
                float wander = tf_fbm(static_cast<float>(px), static_cast<float>(py),
                                      chan_seed + c * 43u, 0.08f, 3);
                py += (wander > 0.6f) ? 1 : (wander < 0.4f) ? -1 : 0;
            }

            px = std::clamp(px, 2, w - 3);
            py = std::clamp(py, 2, h - 3);
            if ((from_top && py >= h - 2) || (!from_top && px >= w - 2)) break;
        }
    }
}
```

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`

- [ ] **Step 7: Commit**

```bash
git add src/generators/terrain_features.cpp
git commit -m "feat: add Clearing, DuneField, CraterBowl, PoolCluster, LavaChannels features"
```

---

## Task 5: NarrowPass and Biome Definition Table

**Files:**
- Modify: `src/generators/terrain_features.cpp`

- [ ] **Step 1: Implement NarrowPass**

Guarantees a walkable corridor through the map. Replaces the current horizontal+vertical path carving.

```cpp
void feature_narrow_pass(TileMap& map, std::mt19937& rng,
                         const NoiseField& noise, const FeatureConfig& cfg) {
    int w = map.width(), h = map.height();
    unsigned pass_seed = static_cast<unsigned>(rng());

    // Horizontal winding path
    {
        int py = h / 2;
        for (int x = 1; x < w - 1; ++x) {
            float wander = tf_fbm(static_cast<float>(x), 0.0f, pass_seed, 0.05f, 3);
            py += (wander > 0.6f) ? 1 : (wander < 0.4f) ? -1 : 0;
            py = std::clamp(py, 2, h - 3);

            // Carve 3-wide path
            for (int dy = -1; dy <= 1; ++dy)
                set_if_bounds(map, x, py + dy, cfg.floor_tile);
        }
    }

    // Vertical winding path
    {
        int px = w / 2;
        for (int y = 1; y < h - 1; ++y) {
            float wander = tf_fbm(0.0f, static_cast<float>(y), pass_seed + 111u, 0.05f, 3);
            px += (wander > 0.6f) ? 1 : (wander < 0.4f) ? -1 : 0;
            px = std::clamp(px, 2, w - 3);

            for (int dx = -1; dx <= 1; ++dx)
                set_if_bounds(map, px + dx, y, cfg.floor_tile);
        }
    }
}
```

- [ ] **Step 2: Implement biome_definition() lookup**

```cpp
const BiomeDefinition& biome_definition(Biome b) {
    // Static definitions — constructed once
    static const auto defs = []() {
        std::map<Biome, BiomeDefinition> m;

        m[Biome::Rocky] = {0.3f, 0.0f, {
            {feature_cliff_band, {0.6f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_crater_bowl, {0.3f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_narrow_pass, {1.0f}},
        }};
        m[Biome::Forest] = {0.15f, 0.05f, {
            {feature_dense_canopy, {0.5f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_clearing, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_pool_cluster, {0.2f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Jungle] = {0.25f, 0.1f, {
            {feature_dense_canopy, {0.7f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_clearing, {0.2f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_pool_cluster, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Grassland] = {0.05f, 0.02f, {
            {feature_dune_field, {0.2f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_clearing, {0.6f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Sandy] = {0.08f, 0.0f, {
            {feature_dune_field, {0.5f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_crater_bowl, {0.15f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Ice] = {0.1f, 0.15f, {
            {feature_cliff_band, {0.3f, Tile::Ice, Tile::Floor, Tile::Ice}},
            {feature_pool_cluster, {0.4f, Tile::Wall, Tile::Floor, Tile::Ice}},
        }};
        m[Biome::Volcanic] = {0.2f, 0.3f, {
            {feature_lava_channels, {0.6f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_cliff_band, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Aquatic] = {0.05f, 0.5f, {
            {feature_island_archipelago, {0.7f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Fungal] = {0.2f, 0.1f, {
            {feature_dense_canopy, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_pool_cluster, {0.3f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_clearing, {0.3f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Crystal] = {0.15f, 0.05f, {
            {feature_cliff_band, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_clearing, {0.4f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};
        m[Biome::Corroded] = {0.15f, 0.1f, {
            {feature_crater_bowl, {0.3f, Tile::Wall, Tile::Floor, Tile::Water}},
            {feature_dune_field, {0.3f, Tile::Wall, Tile::Floor, Tile::Water}},
        }};

        // Mountains: use Rocky definition with higher intensity
        m[Biome::Station] = {0.1f, 0.0f, {}};  // station fallback — no features

        return m;
    }();

    static const BiomeDefinition fallback = {0.1f, 0.0f, {}};
    auto it = defs.find(b);
    return (it != defs.end()) ? it->second : fallback;
}
```

Note: Mountains biome doesn't exist as a `Biome` enum value — mountain overworld tiles map to `Biome::Rocky` via `detail_biome_for_terrain()`. The Rocky definition handles mountain terrain with CliffBand + CraterBowl. We'll add a higher-intensity mountain variant if needed by checking the overworld tile type in the generator.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/generators/terrain_features.cpp src/generators/terrain_features.h
git commit -m "feat: add NarrowPass feature and biome definition lookup table"
```

---

## Task 6: Rewrite detail_map_generator — New Generation Pipeline

**Files:**
- Modify: `src/generators/detail_map_generator.cpp`

This is the core rewrite. The generator's `generate_layout()` changes from noise-threshold classification to the feature composition pipeline. Everything else (edge blending, scatter, POI handlers) is preserved.

- [ ] **Step 1: Add include and update generate_layout()**

Add `#include "terrain_features.h"` at the top of `detail_map_generator.cpp`.

Replace the body of `generate_layout()` (currently lines 222-431) with the new pipeline. The new function should:

1. Generate the noise field via `generate_noise_field(w, h, seed)`
2. Fill the map with `Floor` tiles as a base
3. Apply base wall/water density from `BiomeDefinition` using noise thresholds (same as current approach but reading from the definition)
4. Run each feature from the biome definition in order
5. Handle overworld tile upgrades — if the overworld tile is `OW_Mountains` (mapped to `Biome::Rocky`), boost feature intensity by looking at the overworld tile type in `props_->detail_terrain`
6. Apply shared edge blending (preserve existing edge strip code)
7. Apply lore scar density modifier (preserve existing code)

```cpp
void DetailMapGenerator::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();
    unsigned seed = static_cast<unsigned>(rng());

    // Phase 1: Generate noise field
    auto noise = generate_noise_field(w, h, seed);

    // Phase 2: Look up biome definition
    Biome biome = props_->biome;
    const auto& def = biome_definition(biome);

    // Base density fill using noise
    float wall_d = def.base_wall_density;
    float water_d = def.base_water_density;

    // Intensity boost for mountain terrain
    float intensity_mult = 1.0f;
    if (props_->detail_terrain == Tile::OW_Mountains)
        intensity_mult = 1.5f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float n = noise.elevation[y * w + x];
            Tile t = Tile::Floor;
            if (water_d > 0.0f && n < water_d)
                t = Tile::Water;
            else if (n > (1.0f - wall_d))
                t = Tile::Wall;
            map_->set(x, y, t);
        }
    }

    // Phase 2b: Run terrain features
    for (const auto& spec : def.features) {
        FeatureConfig cfg = spec.config;
        cfg.intensity = std::min(1.0f, cfg.intensity * intensity_mult);
        spec.fn(*map_, rng, noise, cfg);
    }

    // Phase 3: Shared edge blending
    // [PRESERVE existing shared edge strip generation and blending code]
    // This needs to be adapted: instead of blending wall/water thresholds,
    // we blend at the tile level near edges. The simplest approach:
    // re-apply noise-based classification near edges using blended densities.
    // Keep the existing EdgeWeights, shared_edge_seed, EdgeStrip, and
    // generate_edge_strip functions. Apply edge strip blending to a
    // border zone (20% of map from each edge).
    
    unsigned edge_base_seed = seed ^ 0xED6Eu;
    int zx = props_->zone_x;
    int zy = props_->zone_y;
    int owx = props_->overworld_x;
    int owy = props_->overworld_y;

    auto north_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 0), w, wall_d, water_d);
    auto south_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 1), w, wall_d, water_d);
    auto west_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 2), h, wall_d, water_d);
    auto east_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 3), h, wall_d, water_d);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            EdgeWeights ew = compute_edge_weights(x, y, w, h, seed);
            float total_weight = ew.north + ew.south + ew.east + ew.west;
            if (total_weight < 0.01f) continue;

            // Blend toward edge strip values near borders
            float target_wall = wall_d;
            float target_water = water_d;
            float weight_sum = 0.0f;

            auto blend_edge = [&](float ew_val, float strip_wall, float strip_water) {
                if (ew_val > 0.0f) {
                    target_wall += (strip_wall - wall_d) * ew_val;
                    target_water += (strip_water - water_d) * ew_val;
                    weight_sum += ew_val;
                }
            };

            blend_edge(ew.north, north_strip.wall_density[x], north_strip.water_density[x]);
            blend_edge(ew.south, south_strip.wall_density[x], south_strip.water_density[x]);
            blend_edge(ew.west, west_strip.wall_density[y], west_strip.water_density[y]);
            blend_edge(ew.east, east_strip.wall_density[y], east_strip.water_density[y]);

            // Re-classify this cell using blended thresholds
            float n = noise.elevation[y * w + x];
            if (target_water > 0.0f && n < target_water)
                map_->set(x, y, Tile::Water);
            else if (n > (1.0f - target_wall))
                map_->set(x, y, Tile::Wall);
        }
    }

    // Phase 4: Lore scar density modifier
    float scar = props_->lore_scar_intensity;
    if (scar > world::scar_light_threshold) {
        // Add extra walls in scarred areas
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (map_->get(x, y) != Tile::Floor) continue;
                float n = noise.elevation[y * w + x];
                if (n > (1.0f - wall_d - scar * 0.3f))
                    map_->set(x, y, Tile::Wall);
            }
        }
    }

    // Special case: River overworld tiles still get sinusoidal water band
    if (props_->detail_terrain == Tile::OW_River) {
        float center_line = h * 0.5f;
        float wander_n = tf_fbm(0.0f, 0.0f, seed + 50u, 0.04f, 3);
        center_line += (wander_n - 0.5f) * h * 0.3f;
        for (int x = 0; x < w; ++x) {
            float band = 5.0f + 3.0f * tf_fbm(static_cast<float>(x), 10.0f, seed + 51u, 0.06f, 2);
            float cl = center_line + tf_fbm(static_cast<float>(x), 0.0f, seed + 50u, 0.04f, 3) * h * 0.15f;
            for (int y = 0; y < h; ++y) {
                float dist = std::abs(static_cast<float>(y) - cl);
                if (dist < band)
                    map_->set(x, y, Tile::Water);
            }
        }
    }
}
```

IMPORTANT: Read the existing `generate_layout()` carefully. The new version should preserve:
- The `shared_edge_seed`, `EdgeStrip`, `generate_edge_strip`, `compute_edge_weights` functions (they stay as static functions in the file)
- The river special case
- The lore scar modifier

The functions that can be REMOVED:
- `terrain_wall_density()` — replaced by `BiomeDefinition::base_wall_density`
- `terrain_water_density()` — replaced by `BiomeDefinition::base_water_density`
- The current cell-by-cell classification loop
- The moisture runoff pass (features handle water placement now)
- The horizontal/vertical path carving (replaced by `NarrowPass` feature)

BUT: `terrain_wall_density()` and `terrain_water_density()` may still be needed by the edge blending for neighbor overworld tiles. Check if the existing code references them for neighbor density computation. If so, keep them for that purpose.

- [ ] **Step 2: Build and fix errors**

Run: `cmake -B build -DDEV=ON && cmake --build build`
This will likely need iteration to fix compilation errors. Work through them.

- [ ] **Step 3: Test manually**

Run: `./build/astra-dev`
Start a new game, land on different body types and enter detail maps. Check:
- Mountains: ridges and cliff bands with narrow passes
- Forest: dense canopy with winding paths and clearings
- Swamp: island archipelago with water between
- Desert: rolling dune walls
- Volcanic: lava channels through rock

- [ ] **Step 4: Commit**

```bash
git add src/generators/detail_map_generator.cpp
git commit -m "feat: rewrite detail map generator to use composable terrain features"
```

---

## Task 7: Polish — Mountain Intensity, Crater Override, Edge Cases

**Files:**
- Modify: `src/generators/detail_map_generator.cpp`
- Modify: `src/generators/terrain_features.cpp`

- [ ] **Step 1: Handle OW_Crater terrain**

The crater overworld tile should use `CraterBowl` with high intensity. In the generator, before looking up the biome definition, check for crater:

```cpp
    // Crater terrain: override biome features with a dominant crater bowl
    if (props_->detail_terrain == Tile::OW_Crater) {
        FeatureConfig crater_cfg = {0.8f, Tile::Wall, Tile::Floor, Tile::Water};
        feature_crater_bowl(*map_, rng, noise, crater_cfg);
        // Still run NarrowPass for connectivity
        feature_narrow_pass(*map_, rng, noise, {1.0f});
        // Skip normal biome features
        return;  // (or use a flag to skip feature loop)
    }
```

Integrate this into the pipeline appropriately.

- [ ] **Step 2: Handle remaining overworld terrain overrides**

Some overworld tile types should modify feature behavior:
- `OW_Mountains`: boost intensity (already handled via `intensity_mult`)
- `OW_IceField`: use `Tile::Ice` for water tiles in features
- `OW_LavaFlow`: use `Tile::Water` (rendered as lava via biome colors) for water tiles
- `OW_Fungal`: already handled by Fungal biome definition

Apply tile type overrides to the `FeatureConfig` before running features.

- [ ] **Step 3: Verify edge blending still works**

Test by entering detail maps and walking between zones. Terrain at zone boundaries should blend.

- [ ] **Step 4: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Full manual test of all natural biomes.

- [ ] **Step 5: Commit**

```bash
git add src/generators/detail_map_generator.cpp src/generators/terrain_features.cpp
git commit -m "feat: terrain overrides for crater, ice, lava, and mountain detail maps"
```

---

## Task 8: Update Roadmap

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update roadmap**

This completes the base natural terrain portion of the detail map overhaul. Don't check any roadmap boxes yet (the full overhaul spans phases 1-3), but add a note under World Generation that Phase 1 is done if appropriate.

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs: note detail map overhaul phase 1 progress"
```
