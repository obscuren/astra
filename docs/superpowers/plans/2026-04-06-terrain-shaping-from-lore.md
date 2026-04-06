# Terrain Shaping from Lore — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make lore events (terraforming, wars, weapon tests, beacons, megastructures) visibly reshape planetary surfaces at every zoom level through an influence map system.

**Architecture:** Lore pre-computes a `LoreInfluenceMap` (alien biome strength, scar intensity, landmark zones) before overworld generation. The overworld generator reads these float fields during terrain classification and POI placement. Detail map generators receive per-cell lore values for scatter palette selection and landmark structures. Shared edge seeding ensures cross-zone terrain continuity.

**Tech Stack:** C++20, terminal renderer with UTF-8 glyphs, existing fBm noise utilities.

**Spec:** `docs/superpowers/specs/2026-04-06-terrain-shaping-from-lore-design.md`

**Build:** `cmake -B build -DDEV=ON && cmake --build build`

**Run:** `./build/astra`

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/world_constants.h` | Create | Shared dimension/tuning constants |
| `include/astra/civ_aesthetics.h` | Create | Architecture-to-aesthetic lookup tables |
| `include/astra/lore_influence_map.h` | Create | LoreInfluenceMap struct + generation |
| `src/lore_influence_map.cpp` | Create | Influence map generation logic |
| `include/astra/lore_types.h` | Modify | Add `scar_count`, `primary_civ_index` to LoreSystemData |
| `include/astra/star_chart.h` | Modify | Add fields to LoreAnnotation |
| `include/astra/map_properties.h` | Modify | Add lore detail fields |
| `include/astra/tilemap.h` | Modify | Add 7 biomes to Biome enum |
| `include/astra/render_descriptor.h` | Modify | Add 4 animation types |
| `src/tilemap.cpp` | Modify | biome_colors() entries for new biomes |
| `src/galaxy_sim.cpp` | Modify | build_lore(): populate scar_count, primary_civ_index |
| `src/star_chart.cpp` | Modify | apply_lore_to_galaxy(): copy new fields |
| `src/game_world.cpp` | Modify | Generate influence map on landing, pass to generators, fix detail props |
| `src/generators/overworld_generator.cpp` | Modify | Consume influence map in layout + features |
| `src/generators/detail_map_generator.cpp` | Modify | Alien/scar scatter, landmark POIs, shared edge seeding, POI dedup |
| `src/terminal_theme.cpp` | Modify | Alien + scar biome glyphs, new animations |
| `src/animation.cpp` | Modify | Handle new animation types |
| `src/star_chart_viewer.cpp` | Modify | Orbital markers for megastructure/beacon |
| `CMakeLists.txt` | Modify | Add `src/lore_influence_map.cpp` to build |

---

## Task 1: Shared World Constants

**Files:**
- Create: `include/astra/world_constants.h`

- [ ] **Step 1: Create world_constants.h**

```cpp
#pragma once

namespace astra::world {

// --- Overworld dimensions ---
constexpr int overworld_width  = 128;
constexpr int overworld_height = 128;

// --- Detail map dimensions ---
constexpr int detail_map_width  = 80;
constexpr int detail_map_height = 50;

// --- Lore influence tuning ---
constexpr int landmark_zone_radius    = 3;     // 7x7 tile reserved zone
constexpr int min_scar_radius         = 8;
constexpr int max_scar_radius         = 20;
constexpr float terraform_min_coverage = 0.3f;
constexpr float terraform_max_coverage = 0.6f;
constexpr int terraform_min_origins    = 2;
constexpr int terraform_max_origins    = 5;

// --- Influence thresholds ---
constexpr float alien_strength_threshold = 0.15f;  // below this = no alien effect
constexpr float alien_full_replace       = 0.6f;   // above this = full alien tile
constexpr float scar_light_threshold     = 0.1f;   // scarred ground
constexpr float scar_medium_threshold    = 0.4f;   // scorched earth
constexpr float scar_heavy_threshold     = 0.7f;   // glassed/crater

}  // namespace astra::world
```

- [ ] **Step 2: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile (header-only, no consumers yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/world_constants.h
git commit -m "feat: add shared world constants header for terrain shaping"
```

---

## Task 2: Data Pipeline Extension — LoreSystemData and LoreAnnotation

**Files:**
- Modify: `include/astra/lore_types.h:234-247` (LoreSystemData)
- Modify: `include/astra/star_chart.h:29-40` (LoreAnnotation)
- Modify: `src/galaxy_sim.cpp:1010-1028` (build_lore LoreSystemData population)
- Modify: `src/star_chart.cpp:618-678` (apply_lore_to_galaxy)

- [ ] **Step 1: Add fields to LoreSystemData**

In `include/astra/lore_types.h`, add to `struct LoreSystemData` (after the `int lore_tier = 0;` line at ~247):

```cpp
    int scar_count = 0;          // number of PlanetScar events
    int primary_civ_index = -1;  // most significant civ index:
                                 //   1. megastructure_builder (if has_megastructure)
                                 //   2. terraformed_by (if terraformed)
                                 //   3. last entry in ruin_civ_ids (most recent occupant)
                                 //   4. -1 (no lore presence)
```

- [ ] **Step 2: Add fields to LoreAnnotation**

In `include/astra/star_chart.h`, add `#include "lore_types.h"` if not already present, then add to `struct LoreAnnotation` (after `int terraformed_by_civ = -1;` at ~40):

```cpp
    int scar_count = 0;
    int primary_civ_index = -1;
    Architecture primary_civ_architecture = Architecture::Geometric;
```

- [ ] **Step 3: Populate scar_count and primary_civ_index in build_lore()**

In `src/galaxy_sim.cpp`, in the `build_lore()` function where `LoreSystemData` is populated (~lines 1010-1028), add after `lsd.terraformed_by = s.terraformed_by;`:

```cpp
    lsd.scar_count = static_cast<int>(s.scars.size());

    // Determine primary civ: megastructure builder > terraformer > most recent ruin layer
    if (s.has_megastructure && s.megastructure_builder >= 0)
        lsd.primary_civ_index = s.megastructure_builder;
    else if (s.terraformed && s.terraformed_by >= 0)
        lsd.primary_civ_index = s.terraformed_by;
    else if (!s.ruin_layers.empty())
        lsd.primary_civ_index = s.ruin_layers.back();
```

- [ ] **Step 4: Copy new fields in apply_lore_to_galaxy()**

In `src/star_chart.cpp`, in `apply_lore_to_galaxy()` where flags are copied (~lines 649-657), add after the existing copies:

```cpp
    sys.lore.scar_count = lsd.scar_count;
    sys.lore.primary_civ_index = lsd.primary_civ_index;
    if (lsd.primary_civ_index >= 0 &&
        lsd.primary_civ_index < static_cast<int>(lore.civilizations.size())) {
        sys.lore.primary_civ_architecture =
            lore.civilizations[lsd.primary_civ_index].architecture;
    }
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 6: Commit**

```bash
git add include/astra/lore_types.h include/astra/star_chart.h src/galaxy_sim.cpp src/star_chart.cpp
git commit -m "feat: propagate scar_count and primary_civ_index through lore pipeline"
```

---

## Task 3: New Biomes and BiomeColors

**Files:**
- Modify: `include/astra/tilemap.h:209-222` (Biome enum)
- Modify: `src/tilemap.cpp:380-408` (biome_colors)

- [ ] **Step 1: Add 7 new biomes to Biome enum**

In `include/astra/tilemap.h`, extend `enum class Biome` (before the closing `};`):

```cpp
enum class Biome : uint8_t {
    Station, Rocky, Volcanic, Ice, Sandy, Aquatic, Fungal, Crystal, Corroded,
    Forest, Grassland, Jungle,
    // Alien terrain (one per Architecture type)
    AlienCrystalline, AlienOrganic, AlienGeometric, AlienVoid, AlienLight,
    // Scar terrain
    ScarredGlassed, ScarredScorched,
};
```

- [ ] **Step 2: Add BiomeColors entries**

In `src/tilemap.cpp`, in the `biome_colors()` function, add cases for the 7 new biomes:

```cpp
    case Biome::AlienCrystalline:
        return {Color::Cyan, Color::DarkCyan, Color::White, Color::DarkGray};
    case Biome::AlienOrganic:
        return {Color::Red, Color::DarkRed, Color::Magenta, Color::DarkGray};
    case Biome::AlienGeometric:
        return {Color::Yellow, Color::DarkYellow, Color::White, Color::DarkGray};
    case Biome::AlienVoid:
        return {Color::DarkMagenta, Color::DarkGray, Color::Magenta, Color::DarkGray};
    case Biome::AlienLight:
        return {static_cast<Color>(228), Color::Yellow, Color::White, Color::DarkGray};
    case Biome::ScarredGlassed:
        return {static_cast<Color>(208), static_cast<Color>(94), Color::DarkYellow, Color::DarkGray};
    case Biome::ScarredScorched:
        return {Color::DarkGray, static_cast<Color>(52), Color::DarkRed, Color::DarkGray};
```

Note: `static_cast<Color>(N)` is used for 256-color values — check that the existing codebase uses this pattern (it does in `terminal_theme.cpp` for NaturalObstacle colors like `static_cast<Color>(94)` and `static_cast<Color>(58)`).

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp
git commit -m "feat: add alien and scar biome types with color palettes"
```

---

## Task 4: Architecture-to-Aesthetic Mapping

**Files:**
- Create: `include/astra/civ_aesthetics.h`

- [ ] **Step 1: Create civ_aesthetics.h**

```cpp
#pragma once

#include "astra/lore_types.h"
#include "astra/tilemap.h"

namespace astra {

// Map an Architecture type to its alien Biome for terrain rendering.
inline Biome alien_biome_for_architecture(Architecture arch) {
    switch (arch) {
        case Architecture::Crystalline: return Biome::AlienCrystalline;
        case Architecture::Organic:     return Biome::AlienOrganic;
        case Architecture::Geometric:   return Biome::AlienGeometric;
        case Architecture::VoidCarved:  return Biome::AlienVoid;
        case Architecture::LightWoven:  return Biome::AlienLight;
    }
    return Biome::AlienGeometric;
}

// Scar biome by intensity threshold.
// Call with scar_intensity already confirmed > scar_light_threshold.
inline Biome scar_biome_for_intensity(float intensity) {
    return intensity >= 0.7f ? Biome::ScarredGlassed : Biome::ScarredScorched;
}

// Does this alien architecture have pulsing/animated fixtures?
inline bool architecture_has_animation(Architecture arch) {
    return arch == Architecture::Organic || arch == Architecture::LightWoven;
}

}  // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile (header-only, no consumers yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/civ_aesthetics.h
git commit -m "feat: add architecture-to-aesthetic mapping header"
```

---

## Task 5: MapProperties Extension

**Files:**
- Modify: `include/astra/map_properties.h:27-65`

- [ ] **Step 1: Add lore detail fields to MapProperties**

In `include/astra/map_properties.h`, add `#include "astra/lore_types.h"` at the top if not present. Then add new fields after the existing lore block (`lore_plague_origin` at ~line 54):

```cpp
    int lore_scar_count = 0;
    Architecture lore_civ_architecture = Architecture::Geometric;
    int lore_primary_civ_index = -1;

    // Detail map lore context (populated from influence map at overworld cell)
    float lore_alien_strength = 0.0f;
    Architecture lore_alien_architecture = Architecture::Geometric;
    float lore_scar_intensity = 0.0f;
```

- [ ] **Step 2: Copy scar_count and architecture in game_world.cpp**

In `src/game_world.cpp`, where lore annotations are copied to MapProperties (~lines 1102-1110), add after the existing copies:

```cpp
    props.lore_scar_count = la.scar_count;
    props.lore_civ_architecture = la.primary_civ_architecture;
    props.lore_primary_civ_index = la.primary_civ_index;
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add include/astra/map_properties.h src/game_world.cpp
git commit -m "feat: extend MapProperties with scar count and civ architecture"
```

---

## Task 6: Lore Influence Map

**Files:**
- Create: `include/astra/lore_influence_map.h`
- Create: `src/lore_influence_map.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/lore_types.h"
#include "astra/map_properties.h"
#include "astra/world_constants.h"

#include <cstdint>
#include <vector>

namespace astra {

enum class LandmarkType : uint8_t {
    None = 0,
    Beacon,
    Megastructure,
};

struct LoreInfluenceMap {
    int width  = 0;
    int height = 0;
    std::vector<float> alien_strength;   // 0.0-1.0
    std::vector<float> scar_intensity;   // 0.0-1.0
    std::vector<LandmarkType> landmark;  // per-cell

    // Returns true if this map has any lore influence at all.
    bool empty() const { return width == 0; }

    float alien_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return alien_strength[y * width + x];
    }
    float scar_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return scar_intensity[y * width + x];
    }
    LandmarkType landmark_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return LandmarkType::None;
        return landmark[y * width + x];
    }
};

// Generate a lore influence map for an overworld surface.
// Returns an empty map if no lore features apply.
LoreInfluenceMap generate_lore_influence(
    const MapProperties& props,
    int map_width, int map_height,
    unsigned seed);

}  // namespace astra
```

- [ ] **Step 2: Create the implementation**

```cpp
#include "astra/lore_influence_map.h"
#include "astra/world_constants.h"

#include <cmath>
#include <random>

namespace astra {

// ── Noise (reused pattern from overworld/detail generators) ──

static float hash_noise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

static float smooth_noise(float fx, float fy, unsigned seed) {
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float dx = fx - ix;
    float dy = fy - iy;
    float sx = dx * dx * (3.0f - 2.0f * dx);
    float sy = dy * dy * (3.0f - 2.0f * dy);
    float n00 = hash_noise(ix, iy, seed);
    float n10 = hash_noise(ix + 1, iy, seed);
    float n01 = hash_noise(ix, iy + 1, seed);
    float n11 = hash_noise(ix + 1, iy + 1, seed);
    float top = n00 + sx * (n10 - n00);
    float bot = n01 + sx * (n11 - n01);
    return top + sy * (bot - top);
}

static float fbm(float x, float y, unsigned seed, float scale, int octaves = 4) {
    float value = 0.0f, amplitude = 1.0f, total_amp = 0.0f, freq = scale;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * smooth_noise(x * freq, y * freq, seed + i * 31u);
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

// ── Alien biome patches ──

static void generate_alien_patches(LoreInfluenceMap& map, const MapProperties& props,
                                   std::mt19937& rng) {
    if (!props.lore_terraformed) return;

    using namespace astra::world;
    int w = map.width, h = map.height;
    unsigned noise_seed = static_cast<unsigned>(rng());

    // Determine number of origin points and target coverage
    std::uniform_int_distribution<int> origin_count(terraform_min_origins, terraform_max_origins);
    int num_origins = origin_count(rng);

    std::uniform_int_distribution<int> xdist(w / 8, w - w / 8);
    std::uniform_int_distribution<int> ydist(h / 8, h - h / 8);

    struct Origin { float x, y, radius; };
    std::vector<Origin> origins;
    for (int i = 0; i < num_origins; ++i) {
        float radius = static_cast<float>(std::min(w, h)) *
            (0.2f + 0.3f * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));
        origins.push_back({static_cast<float>(xdist(rng)),
                           static_cast<float>(ydist(rng)), radius});
    }

    // Target coverage: terraform_min_coverage to terraform_max_coverage
    float target_coverage = std::uniform_real_distribution<float>(
        terraform_min_coverage, terraform_max_coverage)(rng);

    // Peak strength scales down for older civilizations
    // (props don't carry epoch_end_bya directly, so we use a uniform peak for now;
    //  epoch-based degradation can be added when WorldLore is accessible during gen)
    float peak_strength = 0.9f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float max_influence = 0.0f;
            for (const auto& o : origins) {
                float dx = (x - o.x) / o.radius;
                float dy = (y - o.y) / o.radius;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= 1.0f) continue;

                // Noise-modulated falloff for ragged edges
                float noise = fbm(static_cast<float>(x), static_cast<float>(y),
                                  noise_seed + static_cast<unsigned>(&o - &origins[0]) * 97u,
                                  0.05f, 3);
                float edge = 1.0f - dist;
                float strength = edge * peak_strength * (0.6f + 0.8f * noise);
                max_influence = std::max(max_influence, strength);
            }
            map.alien_strength[y * w + x] = std::min(max_influence, 1.0f);
        }
    }

    // Scale to target coverage: count cells above threshold, adjust if needed
    // (rough — we accept natural variance rather than forcing exact coverage)
    (void)target_coverage;
}

// ── Scar zones ──

static void generate_scar_zones(LoreInfluenceMap& map, const MapProperties& props,
                                std::mt19937& rng) {
    int scar_count = props.lore_scar_count;
    if (scar_count <= 0 && !props.lore_weapon_test && !props.lore_battle_site) return;

    // Guarantee at least 1 scar for flagged systems even if scar_count is 0
    if (scar_count == 0) scar_count = 1;

    using namespace astra::world;
    int w = map.width, h = map.height;
    unsigned noise_seed = static_cast<unsigned>(rng());

    // Radius scales with total scar count
    float base_radius = static_cast<float>(min_scar_radius) +
        static_cast<float>(max_scar_radius - min_scar_radius) *
        std::min(1.0f, static_cast<float>(scar_count) / 6.0f);

    std::uniform_int_distribution<int> xdist(w / 6, w - w / 6);
    std::uniform_int_distribution<int> ydist(h / 6, h - h / 6);

    for (int s = 0; s < scar_count; ++s) {
        float cx = static_cast<float>(xdist(rng));
        float cy = static_cast<float>(ydist(rng));

        // Avoid placing on landmarks
        int icx = static_cast<int>(cx), icy = static_cast<int>(cy);
        if (map.landmark_at(icx, icy) != LandmarkType::None) continue;

        float radius = base_radius * (0.8f + 0.4f *
            std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float dx = (x - cx) / radius;
                float dy = (y - cy) / radius;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= 1.0f) continue;

                float noise = fbm(static_cast<float>(x), static_cast<float>(y),
                                  noise_seed + static_cast<unsigned>(s) * 137u,
                                  0.08f, 3);
                float edge = 1.0f - dist;
                float intensity = edge * edge * (0.7f + 0.6f * noise);

                // Additive — overlapping scars intensify
                int idx = y * w + x;
                map.scar_intensity[idx] = std::min(map.scar_intensity[idx] + intensity, 1.0f);
            }
        }
    }
}

// ── Landmarks ──

static void place_landmarks(LoreInfluenceMap& map, const MapProperties& props,
                            std::mt19937& rng) {
    using namespace astra::world;
    int w = map.width, h = map.height;

    auto place = [&](LandmarkType type) {
        // Find suitable location: away from edges, on passable-ish terrain
        // We try random locations and pick one not already occupied
        std::uniform_int_distribution<int> xdist(w / 4, 3 * w / 4);
        std::uniform_int_distribution<int> ydist(h / 4, 3 * h / 4);

        int cx = xdist(rng), cy = ydist(rng);

        int r = landmark_zone_radius;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    map.landmark[py * w + px] = type;
                }
            }
        }
    };

    if (props.lore_beacon) place(LandmarkType::Beacon);
    if (props.lore_megastructure) place(LandmarkType::Megastructure);
}

// ── Public API ──

LoreInfluenceMap generate_lore_influence(
    const MapProperties& props,
    int map_width, int map_height,
    unsigned seed)
{
    // Quick exit: no lore features to shape terrain
    if (!props.lore_terraformed && props.lore_scar_count == 0 &&
        !props.lore_weapon_test && !props.lore_battle_site &&
        !props.lore_beacon && !props.lore_megastructure) {
        return {};
    }

    LoreInfluenceMap map;
    map.width  = map_width;
    map.height = map_height;
    int size = map_width * map_height;
    map.alien_strength.resize(size, 0.0f);
    map.scar_intensity.resize(size, 0.0f);
    map.landmark.resize(size, LandmarkType::None);

    std::mt19937 rng(seed ^ 0x4C0E'1F40u);

    // Order matters: landmarks first (scars avoid them), then alien, then scars
    place_landmarks(map, props, rng);
    generate_alien_patches(map, props, rng);
    generate_scar_zones(map, props, rng);

    return map;
}

}  // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/lore_influence_map.cpp` to the source list in `CMakeLists.txt`, alongside the other `src/*.cpp` files.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/lore_influence_map.h src/lore_influence_map.cpp CMakeLists.txt
git commit -m "feat: add LoreInfluenceMap generation (alien patches, scars, landmarks)"
```

---

## Task 7: Overworld Generator — Consume Influence Map

**Files:**
- Modify: `src/generators/overworld_generator.cpp:88-137` (classify_terrestrial), `:377-469` (generate_layout), `:471-713` (place_features)
- Modify: `include/astra/map_properties.h` (add influence map pointer)

- [ ] **Step 1: Add influence map pointer to MapProperties**

In `include/astra/map_properties.h`, add `#include "astra/lore_influence_map.h"` and add a field:

```cpp
    // Lore influence map (set before overworld generation, nullptr if no lore)
    const LoreInfluenceMap* lore_influence = nullptr;
```

- [ ] **Step 2: Generate and attach influence map before overworld generation**

In `src/game_world.cpp`, find where the overworld is generated on landing (where `MapProperties` is built and the generator is called). After populating lore fields into `props`, add:

```cpp
    #include "astra/lore_influence_map.h"
    // (add include at top of file)

    // Generate lore influence map
    auto lore_infl = generate_lore_influence(props, props.width, props.height,
                                             overworld_seed ^ 0x1F4Cu);
    props.lore_influence = lore_infl.empty() ? nullptr : &lore_infl;
```

Make sure `lore_infl` stays alive until after `gen->generate()` returns (declare it in the same scope).

- [ ] **Step 3: Modify generate_layout() to consume alien_strength and scar_intensity**

In `src/generators/overworld_generator.cpp`, in `generate_layout()`, after the terrain classification loop where each cell gets a tile, add lore influence:

After `Tile t = classify_terrestrial(elev, moist, ctx);` (or equivalent), add:

```cpp
    // Lore terrain shaping
    if (props_->lore_influence) {
        float alien = props_->lore_influence->alien_at(x, y);
        float scar  = props_->lore_influence->scar_at(x, y);

        // Scar overrides alien (destruction post-dates terraforming)
        if (scar > world::scar_light_threshold) {
            if (scar > world::scar_heavy_threshold) {
                t = Tile::Wall;  // crater core — impassable
            } else if (scar > world::scar_medium_threshold) {
                t = Tile::OW_Desert;  // scorched earth — passable barren
            }
            // light scar (0.1-0.4): keep natural tile, detail map handles degradation
        } else if (alien > world::alien_strength_threshold) {
            // Alien biome replacement
            if (alien > world::alien_full_replace) {
                // Full alien — use a distinct alien overworld tile
                // For now, map to existing tiles that will get alien biome coloring
                // The biome assignment handles the visual
                t = Tile::OW_Plains;  // placeholder tile; biome coloring does the work
            } else {
                // Probabilistic blend
                std::uniform_real_distribution<float> prob(0.0f, 1.0f);
                if (prob(rng) < alien) {
                    t = Tile::OW_Plains;  // alien tile
                }
            }
        }
    }
```

Note: The actual alien visual comes from biome coloring, not tile type. The overworld biome for the map should be set to the alien biome when significant lore influence exists. This needs to be handled when setting the map biome after generation.

- [ ] **Step 4: Modify place_features() to place landmarks first and exclude landmark zones**

In `place_features()`, at the very beginning (before existing POI placement logic), add:

```cpp
    // Place lore landmarks first (beacon, megastructure)
    if (props_->lore_influence) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                auto lm = props_->lore_influence->landmark_at(x, y);
                if (lm == LandmarkType::Beacon) {
                    // Find center of beacon zone and place stamp
                    // (scan to find center — it's the cell with max surrounding landmark cells)
                    // For simplicity: mark the zone center with a unique overworld tile
                    // We only need to place once per landmark
                    // ... (see step 5 for stamp placement)
                }
            }
        }
    }
```

Add a landmark placement pass that finds the center of each contiguous landmark zone and places a single POI stamp there. For beacon: use `Tile::OW_CaveEntrance` as the tile type (it already has a detail map handler; we'll add a beacon-specific handler in Task 9). For megastructure: same approach.

Create two new stamp entries or reuse existing stamps. The landmark detail map will be the main gameplay content — the overworld tile is just the entry point.

Also modify the existing POI placement loops to skip cells where `landmark_at() != None`.

- [ ] **Step 5: Add beacon and megastructure overworld tiles**

If the project doesn't already have `OW_Beacon` and `OW_Megastructure` tile types, add them to the `Tile` enum in `tilemap.h` after the existing overworld tiles:

```cpp
    OW_Beacon,          // Sgr A* beacon spire
    OW_Megastructure,   // megastructure ground anchor
```

Add corresponding entries to `tile_glyph()`, `overworld_glyph()`, and `terrain_wall_density()` / `terrain_water_density()` in detail_map_generator.cpp.

For `overworld_glyph()`:
- Beacon: `"⌾"` in Bright Cyan
- Megastructure: `"◈"` in Bright Yellow

For `tile_glyph()`:
- Beacon: `'*'`
- Megastructure: `'#'`

- [ ] **Step 6: Build and test manually**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Then: `./build/astra` — start a new game, use dev console to warp to a system with `lore_terraformed` or `lore_weapon_test` set. Land on a body and verify:
- Terraformed worlds show some altered terrain patches
- Weapon test / battle site worlds show scar zones (walls/desert patches)
- Beacon/megastructure systems show landmark tiles on overworld

Expected: Visual differences on lore-affected worlds. Terrain generation doesn't crash.

- [ ] **Step 7: Commit**

```bash
git add include/astra/map_properties.h include/astra/tilemap.h src/game_world.cpp \
    src/generators/overworld_generator.cpp src/tilemap.cpp
git commit -m "feat: overworld generator consumes lore influence map for terrain shaping"
```

---

## Task 8: Terminal Theme — Alien and Scar Biome Visuals

**Files:**
- Modify: `src/terminal_theme.cpp:582-686` (NaturalObstacle biome cases)
- Modify: `src/terminal_theme.cpp:14-42` (biome_palette / theme biome colors)

- [ ] **Step 1: Add alien biome NaturalObstacle cases**

In `src/terminal_theme.cpp`, in the `resolve_fixture_visual()` NaturalObstacle switch on biome (starting ~line 582), add new cases before the `default:`:

```cpp
            case Biome::AlienCrystalline: {
                static const ResolvedVisual variants[] = {
                    {'*', "\xe2\x97\x86", Color::Cyan, Color::Default},            // ◆
                    {'o', "\xe2\x97\x87", Color::White, Color::Default},            // ◇
                    {'^', "\xe2\x96\xb3", static_cast<Color>(51), Color::Default},  // △ bright cyan
                    {'v', "\xe2\x96\xbd", Color::Cyan, Color::Default},             // ▽
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::AlienOrganic: {
                static const ResolvedVisual variants[] = {
                    {'O', "\xce\x98", Color::Red, Color::Default},                  // Θ
                    {'~', "\xe2\x88\x9e", Color::Magenta, Color::Default},          // ∞
                    {'S', "\xc2\xa7", Color::DarkRed, Color::Default},              // §
                    {'~', "~", static_cast<Color>(52), Color::Default},             // ~ dark red
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::AlienGeometric: {
                static const ResolvedVisual variants[] = {
                    {'o', "\xe2\x96\xa1", Color::Yellow, Color::Default},            // □
                    {'*', "\xe2\x96\xaa", Color::White, Color::Default},             // ▪
                    {'#', "\xe2\x95\xac", Color::DarkYellow, Color::Default},        // ╬
                    {'+', "\xe2\x94\xbc", Color::Yellow, Color::Default},            // ┼
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::AlienVoid: {
                static const ResolvedVisual variants[] = {
                    {'O', "\xe2\x97\x8f", Color::DarkMagenta, Color::Default},      // ●
                    {'o', "\xe2\x97\x8e", Color::DarkGray, Color::Default},          // ◎
                    {'0', "\xe2\x88\x85", Color::Magenta, Color::Default},           // ∅
                    {'v', "\xe2\x96\xbc", Color::DarkMagenta, Color::Default},       // ▼
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::AlienLight: {
                static const ResolvedVisual variants[] = {
                    {'*', "\xe2\x9c\xa6", static_cast<Color>(228), Color::Default},  // ✦ bright yellow
                    {'o', "\xe2\x9c\xa7", Color::White, Color::Default},              // ✧
                    {'*', "\xe2\x88\x97", Color::Yellow, Color::Default},             // ∗
                    {'*', "\xe2\x98\x86", static_cast<Color>(230), Color::Default},  // ☆ bright white
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::ScarredGlassed: {
                static const ResolvedVisual variants[] = {
                    {'#', "\xe2\x96\x93", Color::DarkYellow, Color::Default},        // ▓
                    {'.', "\xe2\x96\x91", static_cast<Color>(208), Color::Default},  // ░ orange
                    {'~', "\xe2\x89\x88", Color::DarkGray, Color::Default},          // ≈
                    {'.', "\xc2\xb7", Color::DarkGray, Color::Default},              // · dim
                };
                vis = variants[seed % 4]; break;
            }
            case Biome::ScarredScorched: {
                static const ResolvedVisual variants[] = {
                    {'!', "\xe2\x89\xa0", Color::DarkGray, Color::Default},          // ≠
                    {',', ",", static_cast<Color>(52), Color::Default},              // , dark red
                    {'~', "~", static_cast<Color>(208), Color::Default},             // ~ orange
                    {'%', "%", Color::DarkGray, Color::Default},                     // % ash
                };
                vis = variants[seed % 4]; break;
            }
```

- [ ] **Step 2: Add theme biome colors for new biomes**

In the `biome_palette()` function (or equivalent theme color function), add entries matching what was added in `biome_colors()` in Task 3.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add src/terminal_theme.cpp
git commit -m "feat: add alien and scar biome fixture glyphs to terminal theme"
```

---

## Task 9: Detail Map — Alien Scatter, Scar Terrain, and Landmark POIs

**Files:**
- Modify: `src/generators/detail_map_generator.cpp:410-517` (scatter_biome_features), `:616-675` (place_features), `:165-244` (generate_layout)
- Modify: `src/game_world.cpp:421-466` (build_detail_props)

- [ ] **Step 1: Pass lore values to detail map via build_detail_props()**

In `src/game_world.cpp`, in `build_detail_props()` (~line 421), after the existing property setup, add influence map reads:

```cpp
    // Lore influence at this overworld cell (if influence map exists)
    // The influence map is stored on the overworld MapProperties or WorldManager.
    // For now, read from the stored lore influence map.
    if (world_.lore_influence_map() && !world_.lore_influence_map()->empty()) {
        props.lore_alien_strength = world_.lore_influence_map()->alien_at(ow_x, ow_y);
        props.lore_scar_intensity = world_.lore_influence_map()->scar_at(ow_x, ow_y);
        props.lore_alien_architecture = /* from MapProperties of parent overworld */ 
            world_.overworld_props().lore_civ_architecture;
    }
```

Note: This requires storing the `LoreInfluenceMap` on the `WorldManager` when the overworld is generated, so it persists for detail map entries. Add a `LoreInfluenceMap` member to `WorldManager` and set it during overworld generation.

- [ ] **Step 2: Add alien scatter palettes to scatter_biome_features()**

In `detail_map_generator.cpp`, in `scatter_biome_features()`, add cases for the 7 new biomes in the switch:

```cpp
        case Biome::AlienCrystalline:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // crystal shard
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // crystal cluster
                {1, ScatterStamp::Cluster3, natural_obstacle()},  // spire group
            };
            attempts = area / 80;
            break;
        case Biome::AlienOrganic:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // growth node
                {2, ScatterStamp::Block2x2, natural_obstacle()},  // tendril mass
            };
            attempts = area / 70;
            break;
        case Biome::AlienGeometric:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // geometric block
                {1, ScatterStamp::Line3,    natural_obstacle()},  // aligned pillars
            };
            attempts = area / 100;
            break;
        case Biome::AlienVoid:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // void fissure
                {1, ScatterStamp::Cluster3, natural_obstacle()},  // debris cluster
            };
            attempts = area / 100;
            break;
        case Biome::AlienLight:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // light pillar
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // glow pool
            };
            attempts = area / 90;
            break;
        case Biome::ScarredGlassed:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // melted rock
                {2, ScatterStamp::Block2x2, natural_obstacle()},  // glass formation
                {1, ScatterStamp::Cluster3, natural_obstacle()},  // debris field
            };
            attempts = area / 70;
            break;
        case Biome::ScarredScorched:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // charred stump
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // wreckage
            };
            attempts = area / 90;
            break;
```

- [ ] **Step 3: Override biome in detail map based on lore influence**

In `detail_map_generator.cpp`, at the start of `place_features()`, check if lore values require a biome override for scatter purposes:

```cpp
    Biome scatter_biome = props_->biome;

    // Lore overrides
    if (props_->lore_scar_intensity > world::scar_medium_threshold) {
        scatter_biome = props_->lore_scar_intensity > world::scar_heavy_threshold
            ? Biome::ScarredGlassed : Biome::ScarredScorched;
    } else if (props_->lore_alien_strength > world::alien_strength_threshold) {
        scatter_biome = alien_biome_for_architecture(props_->lore_alien_architecture);
    }

    scatter_biome_features(map_, rng, scatter_biome);
```

Replace the existing `scatter_biome_features(map_, rng, props_->biome);` call.

- [ ] **Step 4: Add scar terrain shaping to generate_layout()**

In `generate_layout()`, after the normal terrain classification, add scar influence on wall/water density:

```cpp
    // Scar intensity increases wall density (rubble, debris)
    float scar = props_->lore_scar_intensity;
    if (scar > world::scar_light_threshold) {
        wall_threshold += scar * 0.3f;  // more walls in scarred areas
        water_threshold *= (1.0f - scar * 0.5f);  // less water (evaporated/drained)
    }
```

- [ ] **Step 5: Add beacon and megastructure POI handlers**

In `place_features()`, add new cases for the beacon and megastructure detail POI types:

```cpp
        case Tile::OW_Beacon: {
            // Central spire: ring of walls with portal at center
            int spire_r = 4;
            for (int dy = -spire_r; dy <= spire_r; ++dy) {
                for (int dx = -spire_r; dx <= spire_r; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (!in_bounds(px, py)) continue;
                    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    // Ring wall
                    if (dist > spire_r - 1.5f && dist < spire_r - 0.5f)
                        map_->set(px, py, Tile::Wall);
                    // Inner floor
                    if (dist < spire_r - 1.5f)
                        map_->set(px, py, Tile::Floor);
                }
            }
            // Portal at center (interaction point)
            map_->set(cx, cy, Tile::Portal);

            // Satellite pylons at cardinal directions
            for (auto [ddx, ddy] : std::initializer_list<std::pair<int,int>>{{0,-7},{0,7},{-7,0},{7,0}}) {
                int px = cx + ddx, py = cy + ddy;
                if (in_bounds(px, py)) {
                    map_->set(px, py, Tile::Wall);
                    if (in_bounds(px+1, py)) map_->set(px+1, py, Tile::Wall);
                    if (in_bounds(px, py+1)) map_->set(px, py+1, Tile::Wall);
                }
            }
            break;
        }
        case Tile::OW_Megastructure: {
            // Large rectangular foundation
            int fw = 10, fh = 8;
            for (int dy = -fh/2; dy <= fh/2; ++dy) {
                for (int dx = -fw/2; dx <= fw/2; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (!in_bounds(px, py)) continue;
                    // Thick outer walls
                    if (std::abs(dx) >= fw/2 - 1 || std::abs(dy) >= fh/2 - 1)
                        map_->set(px, py, Tile::Wall);
                    else
                        map_->set(px, py, Tile::Floor);
                }
            }
            // Interior fixtures: machinery
            auto console_fd = make_fixture(FixtureType::Console);
            map_->add_fixture(cx - 2, cy, console_fd);
            map_->add_fixture(cx + 2, cy, console_fd);

            // Access corridors (doors on each side)
            map_->set(cx, cy - fh/2, Tile::Floor);  // north door
            map_->set(cx, cy + fh/2, Tile::Floor);  // south door
            break;
        }
```

- [ ] **Step 6: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. Test by entering detail maps on lore-affected worlds.

- [ ] **Step 7: Commit**

```bash
git add src/generators/detail_map_generator.cpp src/game_world.cpp \
    include/astra/world_manager.h
git commit -m "feat: detail map consumes lore influence — alien scatter, scars, landmarks"
```

---

## Task 10: Detail Map Fix — POI Deduplication (Center Zone Only)

**Files:**
- Modify: `src/game_world.cpp:421-466` (build_detail_props)

- [ ] **Step 1: Restrict POI to center zone**

In `build_detail_props()`, change the POI detection block (~lines 457-463) to only set `detail_has_poi` for the center zone:

Replace:
```cpp
    // Check for POI
    Tile t = props.detail_terrain;
    if (t == Tile::OW_CaveEntrance || t == Tile::OW_Settlement ||
        t == Tile::OW_Ruins || t == Tile::OW_CrashedShip ||
        t == Tile::OW_Outpost || t == Tile::OW_Landing) {
        props.detail_has_poi = true;
        props.detail_poi_type = t;
    }
```

With:
```cpp
    // POI only generates in center zone (1,1) — surrounding zones are natural terrain
    int zx = world_.zone_x();
    int zy = world_.zone_y();
    if (zx == 1 && zy == 1) {
        Tile t = props.detail_terrain;
        if (t == Tile::OW_CaveEntrance || t == Tile::OW_Settlement ||
            t == Tile::OW_Ruins || t == Tile::OW_CrashedShip ||
            t == Tile::OW_Outpost || t == Tile::OW_Landing ||
            t == Tile::OW_Beacon || t == Tile::OW_Megastructure) {
            props.detail_has_poi = true;
            props.detail_poi_type = t;
        }
    }
```

- [ ] **Step 2: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: Enter a settlement on the overworld. Walk to adjacent zones (edges of detail map). Verify only the center zone has settlement structures.

- [ ] **Step 3: Commit**

```bash
git add src/game_world.cpp
git commit -m "fix: POI structures only generate in center detail zone (1,1)"
```

---

## Task 11: Detail Map Fix — Shared Edge Seeding for Zone Connectivity

**Files:**
- Modify: `src/generators/detail_map_generator.cpp:109-161` (compute_edge_weights), `:165-244` (generate_layout)

- [ ] **Step 1: Add shared edge seed computation**

In `detail_map_generator.cpp`, add a function to compute a deterministic seed for a shared edge between two adjacent zones:

```cpp
// Compute a deterministic seed for the shared edge between two zones.
// Both zones sharing this edge will produce the same seed, ensuring matching terrain.
// Parameters: overworld tile coords, zone coords, and which edge (0=N, 1=S, 2=W, 3=E).
static unsigned shared_edge_seed(unsigned base_seed, int ow_x, int ow_y,
                                  int zone_x, int zone_y, int edge_dir) {
    // For a north edge of zone (zx, zy), the shared edge is the same as
    // the south edge of zone (zx, zy-1). We normalize to the smaller coordinate.
    int edge_ow_x = ow_x, edge_ow_y = ow_y;
    int edge_zx = zone_x, edge_zy = zone_y;

    // Normalize: horizontal edges (N/S) use the zone with smaller zy
    // Vertical edges (W/E) use the zone with smaller zx
    if (edge_dir == 0) { // North edge → same as south edge of zone above
        edge_zy = zone_y - 1;
        if (edge_zy < 0) { edge_ow_y -= 1; edge_zy = 2; } // previous overworld tile
    } else if (edge_dir == 1) { // South edge — already normalized (this zone is "above")
        // south edge of (zx, zy) = north edge of (zx, zy+1) → normalize to (zx, zy)
    } else if (edge_dir == 2) { // West edge → same as east edge of zone to the left
        edge_zx = zone_x - 1;
        if (edge_zx < 0) { edge_ow_x -= 1; edge_zx = 2; }
    } else { // East edge — already normalized
    }

    return base_seed
        ^ (static_cast<unsigned>(edge_ow_x + 1000) * 7919u)
        ^ (static_cast<unsigned>(edge_ow_y + 1000) * 6271u)
        ^ (static_cast<unsigned>(edge_zx) * 3571u)
        ^ (static_cast<unsigned>(edge_zy) * 4517u)
        ^ (static_cast<unsigned>(edge_dir & 1) * 8831u); // 0=horizontal, 1=vertical
}
```

- [ ] **Step 2: Generate shared edge terrain strips**

Add a function that generates a 1-tile-wide strip of terrain values (wall density, water density) for a shared edge:

```cpp
struct EdgeStrip {
    std::vector<float> wall_density;   // length = edge dimension (w for H, h for V)
    std::vector<float> water_density;
};

static EdgeStrip generate_edge_strip(unsigned seed, int length, float base_wall, float base_water) {
    EdgeStrip strip;
    strip.wall_density.resize(length);
    strip.water_density.resize(length);
    for (int i = 0; i < length; ++i) {
        float n = fbm(static_cast<float>(i), 0.0f, seed, 0.1f, 3);
        strip.wall_density[i]  = base_wall  * (0.5f + n);
        strip.water_density[i] = base_water * (0.3f + 0.7f * n);
    }
    return strip;
}
```

- [ ] **Step 3: Blend toward shared edge strips in generate_layout()**

Modify the edge blending logic in `generate_layout()` to use shared edge strips instead of just density interpolation. The existing `compute_edge_weights()` gives blend weights; the shared edge strip gives the target to blend toward:

In the blending section of `generate_layout()`, before the per-cell loop, compute edge strips:

```cpp
    // Shared edge strips for zone connectivity
    // Props must carry zone coordinates and overworld position
    unsigned edge_base_seed = seed ^ 0xED6Eu;
    int zx = props_->zone_x;  // need to add these to MapProperties
    int zy = props_->zone_y;
    int owx = props_->overworld_x;
    int owy = props_->overworld_y;

    auto north_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 0), w, center_wall, center_water);
    auto south_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 1), w, center_wall, center_water);
    auto west_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 2), h, center_wall, center_water);
    auto east_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 3), h, center_wall, center_water);
```

Then in the per-cell blend computation, blend toward the edge strip values instead of (or in addition to) neighbor tile densities:

```cpp
    if (ew.north > 0.0f) {
        wall_threshold  = blend(wall_threshold,  north_strip.wall_density[x],  ew.north);
        water_threshold = blend(water_threshold, north_strip.water_density[x], ew.north);
    }
    if (ew.south > 0.0f) {
        wall_threshold  = blend(wall_threshold,  south_strip.wall_density[x],  ew.south);
        water_threshold = blend(water_threshold, south_strip.water_density[x], ew.south);
    }
    if (ew.west > 0.0f) {
        wall_threshold  = blend(wall_threshold,  west_strip.wall_density[y],  ew.west);
        water_threshold = blend(water_threshold, west_strip.water_density[y], ew.west);
    }
    if (ew.east > 0.0f) {
        wall_threshold  = blend(wall_threshold,  east_strip.wall_density[y],  ew.east);
        water_threshold = blend(water_threshold, east_strip.water_density[y], ew.east);
    }
```

- [ ] **Step 4: Add zone/overworld coordinates to MapProperties**

In `include/astra/map_properties.h`, add:

```cpp
    // Zone position within the 3x3 grid (for shared edge seeding)
    int zone_x = 1;
    int zone_y = 1;
    int overworld_x = 0;
    int overworld_y = 0;
```

Populate these in `build_detail_props()` in `game_world.cpp`:

```cpp
    props.zone_x = world_.zone_x();
    props.zone_y = world_.zone_y();
    props.overworld_x = ow_x;
    props.overworld_y = ow_y;
```

- [ ] **Step 5: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: Enter a detail map, walk to the edge to enter an adjacent zone. Verify terrain at the boundary looks continuous (walls and open ground align).

- [ ] **Step 6: Commit**

```bash
git add src/generators/detail_map_generator.cpp include/astra/map_properties.h src/game_world.cpp
git commit -m "feat: shared edge seeding for detail map zone connectivity"
```

---

## Task 12: Animation Types for Lore Terrain

**Files:**
- Modify: `include/astra/render_descriptor.h:39-48` (AnimationType enum)
- Modify: `src/terminal_theme.cpp` (resolve_animation)
- Modify: `src/animation.cpp` (spawn_fixture_anims)

- [ ] **Step 1: Add animation types**

In `include/astra/render_descriptor.h`, add to `AnimationType`:

```cpp
enum class AnimationType : uint8_t {
    ConsoleBlink, WaterShimmer, ViewportShimmer, TorchFlicker,
    DamageFlash, HealPulse, Projectile, LevelUp,
    AlienPulse, ScarSmolder, BeaconGlow, MegastructureShift,
};
```

- [ ] **Step 2: Add resolve_animation() cases**

In `src/terminal_theme.cpp`, in `resolve_animation()`, add:

```cpp
    case AnimationType::AlienPulse: {
        // Slow color cycling: 4 frames, ~500ms each
        static const ResolvedVisual frames[] = {
            {'*', "\xe2\x9c\xa6", Color::Red, Color::Default},
            {'*', "\xe2\x9c\xa6", Color::DarkRed, Color::Default},
            {'*', "\xe2\x9c\xa6", Color::Magenta, Color::Default},
            {'*', "\xe2\x9c\xa6", Color::DarkRed, Color::Default},
        };
        return frames[frame_index % 4];
    }
    case AnimationType::ScarSmolder: {
        // Occasional flicker: 3 frames, ~800ms each
        static const ResolvedVisual frames[] = {
            {'.', "\xc2\xb7", Color::DarkGray, Color::Default},
            {'.', "\xc2\xb7", static_cast<Color>(208), Color::Default},  // orange flash
            {'.', "\xc2\xb7", Color::DarkGray, Color::Default},
        };
        return frames[frame_index % 3];
    }
    case AnimationType::BeaconGlow: {
        // Bright pulsing: 6 frames
        static const ResolvedVisual frames[] = {
            {'*', "\xe2\x8c\xbe", Color::Cyan, Color::Default},          // ⌾
            {'*', "\xe2\x8c\xbe", static_cast<Color>(51), Color::Default},
            {'*', "\xe2\x8c\xbe", Color::White, Color::Default},
            {'*', "\xe2\x8c\xbe", static_cast<Color>(51), Color::Default},
            {'*', "\xe2\x8c\xbe", Color::Cyan, Color::Default},
            {'*', "\xe2\x8c\xbe", Color::DarkCyan, Color::Default},
        };
        return frames[frame_index % 6];
    }
    case AnimationType::MegastructureShift: {
        // Slow structural alternation: 4 frames
        static const ResolvedVisual frames[] = {
            {'#', "\xe2\x97\x88", Color::Yellow, Color::Default},         // ◈
            {'#', "\xe2\x97\x88", Color::DarkYellow, Color::Default},
            {'#', "\xe2\x96\xa3", Color::Yellow, Color::Default},          // ▣
            {'#', "\xe2\x96\xa3", Color::DarkYellow, Color::Default},
        };
        return frames[frame_index % 4];
    }
```

- [ ] **Step 3: Handle animation spawning for lore fixtures**

In `src/animation.cpp`, in `spawn_fixture_anims()`, add logic to spawn `AlienPulse` for Organic/LightWoven biome NaturalObstacles, `ScarSmolder` for scar biome fixtures (1-in-5 chance), `BeaconGlow` for beacon portal tiles, and `MegastructureShift` for megastructure console fixtures. The exact integration depends on how the existing function iterates fixtures — follow the existing pattern for `TorchFlicker`.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/render_descriptor.h src/terminal_theme.cpp src/animation.cpp
git commit -m "feat: add AlienPulse, ScarSmolder, BeaconGlow, MegastructureShift animations"
```

---

## Task 13: Star Chart Orbital Markers

**Files:**
- Modify: `src/star_chart_viewer.cpp:778-857` (draw_system_info_text)

- [ ] **Step 1: Add orbital markers for megastructure/beacon**

In `src/star_chart_viewer.cpp`, in the function that draws the body list for a system, add markers after the system name or in the body listing area:

After the system header is drawn, check for lore annotations:

```cpp
    const auto& lore = sys.lore;
    if (lore.has_megastructure) {
        // Draw megastructure orbital marker
        ctx.text(x_offset, y++, " \xe2\x97\x88 Megastructure in orbit",
                 Color::Yellow);  // ◈
    }
    if (lore.beacon) {
        ctx.text(x_offset, y++, " \xe2\x8c\xbe Sgr A* Beacon detected",
                 Color::Cyan);  // ⌾
    }
```

The exact `ctx.text()` call and coordinates depend on the rendering context used — follow the existing pattern in `draw_system_info_text()`.

- [ ] **Step 2: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: Open star chart, navigate to a system with megastructure or beacon (use dev console to check). Verify markers appear in the system info panel.

- [ ] **Step 3: Commit**

```bash
git add src/star_chart_viewer.cpp
git commit -m "feat: show megastructure and beacon orbital markers on star chart"
```

---

## Task 14: Integration Testing and Polish

- [ ] **Step 1: Full playthrough test**

Run: `./build/astra` — start a new game and systematically test:

1. Use dev console `history` to find systems with terraforming, battles, weapon tests, beacons, megastructures
2. Travel to a terraformed system → land → verify alien biome patches on overworld
3. Travel to a battle/weapon test system → land → verify scar zones (walls + scorched terrain)
4. Travel to a beacon system → land → verify beacon landmark on overworld → enter detail map → verify beacon spire
5. Travel to a megastructure system → land → verify megastructure anchor → enter detail map → verify foundation
6. Enter a settlement → verify POI only in center zone, not all 9
7. Walk between detail map zones → verify terrain continuity at boundaries
8. Check star chart local view for orbital markers

- [ ] **Step 2: Fix any issues found during testing**

Address visual glitches, crashes, or incorrect behavior.

- [ ] **Step 3: Update roadmap**

In `docs/roadmap.md`, check the box:
```
- [x] **Terrain shaping from lore** — megastructures as orbital POIs, beacons as unique landmarks, terraforming alters biome, weapon tests scar terrain
```

- [ ] **Step 4: Final commit**

```bash
git add docs/roadmap.md
git commit -m "docs: mark terrain shaping from lore as complete"
```
