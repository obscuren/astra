# Detail Map Generation v2 — Phase 1: Foundation + Elevation Layer

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the layered channel architecture foundation with elevation-only terrain generation, BiomeProfile system, and a dev test tool to visually verify every biome.

**Architecture:** Open-first terrain using a hybrid channel/direct-painting pipeline. Phase 1 delivers the elevation channel (float grid), compositor, BiomeProfile with strategy functions, and a `biome_test` dev command. The old DetailMapGenerator stays untouched — v2 runs alongside it via the dev tool only.

**Tech Stack:** C++20, value noise fBm, no external dependencies

**Spec:** `docs/superpowers/specs/2026-04-07-detail-map-generation-v2-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/noise.h` | Create | Shared noise utilities (header-only) |
| `include/astra/biome_profile.h` | Create | BiomeProfile struct, strategy type aliases, lookup |
| `include/astra/terrain_channels.h` | Create | Channel grid data structures (header-only) |
| `include/astra/terrain_compositor.h` | Create | Compositor function declaration |
| `src/generators/biome_profiles.cpp` | Create | All 19 biome profile definitions |
| `src/generators/elevation_strategies.cpp` | Create | 4 elevation strategy implementations |
| `src/generators/terrain_compositor.cpp` | Create | Channel → TileMap compositor |
| `src/generators/detail_map_generator_v2.cpp` | Create | New generator orchestrator |
| `CMakeLists.txt` | Modify | Add 4 new source files |
| `include/astra/game.h` | Modify | Add `dev_command_biome_test` declaration |
| `src/game.cpp` | Modify | Add `dev_command_biome_test` implementation |
| `src/dev_console.cpp` | Modify | Add `biome_test` command dispatch |

---

### Task 1: Shared Noise Utility

**Files:**
- Create: `include/astra/noise.h`

- [ ] **Step 1: Create the noise header**

```cpp
#pragma once

#include <cmath>

namespace astra {

inline float hash_noise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

inline float smooth_noise(float fx, float fy, unsigned seed) {
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

inline float fbm(float x, float y, unsigned seed, float scale, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float total_amp = 0.0f;
    float freq = scale;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * smooth_noise(x * freq, y * freq, seed + i * 31u);
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

// Ridge noise: abs(noise) creates sharp creases at zero crossings.
// Used by elevation_ridgeline for cliff lines and mountain ridges.
inline float ridge_noise(float x, float y, unsigned seed, float scale, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float total_amp = 0.0f;
    float freq = scale;
    for (int i = 0; i < octaves; ++i) {
        float n = smooth_noise(x * freq, y * freq, seed + i * 31u);
        value += amplitude * (1.0f - std::abs(2.0f * n - 1.0f));
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

} // namespace astra
```

- [ ] **Step 2: Verify build**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (header not included anywhere yet, no effect)

- [ ] **Step 3: Commit**

```bash
git add include/astra/noise.h
git commit -m "feat: add shared noise utility header"
```

---

### Task 2: BiomeProfile Header

**Files:**
- Create: `include/astra/biome_profile.h`

- [ ] **Step 1: Create the biome profile header**

```cpp
#pragma once

#include "astra/tilemap.h"

#include <random>
#include <string>
#include <vector>

namespace astra {

struct BiomeProfile;

// --- Strategy function types ---

using ElevationStrategy = void(*)(float* grid, int w, int h,
                                   std::mt19937& rng,
                                   const BiomeProfile& prof);

using MoistureStrategy = void(*)(float* grid, int w, int h,
                                  std::mt19937& rng,
                                  const float* elevation,
                                  const BiomeProfile& prof);

enum class StructureMask : uint8_t { None, Wall, Floor, Water };

using StructureStrategy = void(*)(StructureMask* grid, int w, int h,
                                   std::mt19937& rng,
                                   const float* elevation,
                                   const float* moisture,
                                   const BiomeProfile& prof);

struct ScatterEntry {
    FixtureType type = FixtureType::NaturalObstacle;
    float density = 0.0f;
    bool blocks_vision = false;
};

struct BiomeProfile {
    std::string name;

    // Layer 1: Elevation
    ElevationStrategy elevation_fn = nullptr;
    float elevation_frequency = 0.03f;
    int   elevation_octaves  = 4;
    float wall_threshold     = 0.85f;

    // Layer 2: Moisture (Phase 2)
    MoistureStrategy moisture_fn = nullptr;
    float moisture_frequency = 0.04f;
    float water_threshold    = 0.7f;
    float flood_level        = 0.4f;

    // Layer 3: Structure (Phase 3)
    StructureStrategy structure_fn = nullptr;
    float structure_intensity = 0.5f;

    // Layer 4: Scatter (Phase 4)
    std::vector<ScatterEntry> scatter;
};

// Lookup a biome's profile
const BiomeProfile& biome_profile(Biome b);

// Parse a biome name string (e.g. "grassland") to enum. Returns false if unknown.
bool parse_biome(const std::string& name, Biome& out);

} // namespace astra
```

- [ ] **Step 2: Verify build**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (header not included anywhere yet)

- [ ] **Step 3: Commit**

```bash
git add include/astra/biome_profile.h
git commit -m "feat: add BiomeProfile struct and strategy type aliases"
```

---

### Task 3: Channel Grid Data Structure

**Files:**
- Create: `include/astra/terrain_channels.h`

- [ ] **Step 1: Create the terrain channels header**

```cpp
#pragma once

#include "astra/biome_profile.h"

#include <vector>

namespace astra {

struct TerrainChannels {
    int width = 0;
    int height = 0;
    std::vector<float> elevation;
    std::vector<float> moisture;
    std::vector<StructureMask> structure;

    TerrainChannels() = default;

    TerrainChannels(int w, int h)
        : width(w), height(h),
          elevation(w * h, 0.0f),
          moisture(w * h, 0.0f),
          structure(w * h, StructureMask::None) {}

    float  elev(int x, int y) const { return elevation[y * width + x]; }
    float& elev(int x, int y)       { return elevation[y * width + x]; }

    float  moist(int x, int y) const { return moisture[y * width + x]; }
    float& moist(int x, int y)       { return moisture[y * width + x]; }

    StructureMask  struc(int x, int y) const { return structure[y * width + x]; }
    StructureMask& struc(int x, int y)       { return structure[y * width + x]; }
};

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add include/astra/terrain_channels.h
git commit -m "feat: add TerrainChannels data structure"
```

---

### Task 4: Elevation Strategy Implementations

**Files:**
- Create: `src/generators/elevation_strategies.cpp`

- [ ] **Step 1: Create the elevation strategies file**

```cpp
#include "astra/biome_profile.h"
#include "astra/noise.h"

#include <cmath>

namespace astra {

// --- Gentle: low-amplitude rolling terrain ---
// Mostly floor with rare scattered wall peaks.
// Used by: Grassland, Sandy, Forest, Fungal, Jungle
void elevation_gentle(float* grid, int w, int h,
                      std::mt19937& rng,
                      const BiomeProfile& prof) {
    unsigned seed = rng();
    float freq = prof.elevation_frequency;
    int oct = prof.elevation_octaves;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = fbm(static_cast<float>(x), static_cast<float>(y),
                            seed, freq, oct);
            // Compress amplitude — keep most values well below wall threshold
            grid[y * w + x] = val * 0.7f;
        }
    }
}

// --- Rugged: higher amplitude, sharper peaks ---
// More wall outcrops but still majority floor.
// Used by: Rocky, Volcanic
void elevation_rugged(float* grid, int w, int h,
                      std::mt19937& rng,
                      const BiomeProfile& prof) {
    unsigned seed = rng();
    unsigned seed2 = rng();
    float freq = prof.elevation_frequency;
    int oct = prof.elevation_octaves;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = fbm(static_cast<float>(x), static_cast<float>(y),
                            seed, freq, oct);
            // Add a higher-frequency detail pass for sharper features
            float detail = fbm(static_cast<float>(x), static_cast<float>(y),
                                seed2, freq * 2.0f, 2);
            val = val * 0.7f + detail * 0.3f;
            // Sharpen: push mid-values higher to create more pronounced peaks
            val = std::pow(val, 0.8f);
            grid[y * w + x] = val;
        }
    }
}

// --- Flat: near-zero amplitude ---
// Almost entirely floor. Subtle variation for moisture interaction.
// Used by: Aquatic, Swamp, Ice
void elevation_flat(float* grid, int w, int h,
                    std::mt19937& rng,
                    const BiomeProfile& prof) {
    unsigned seed = rng();
    float freq = prof.elevation_frequency;
    int oct = prof.elevation_octaves;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = fbm(static_cast<float>(x), static_cast<float>(y),
                            seed, freq, oct);
            // Compress to bottom 15% — nothing reaches wall threshold
            grid[y * w + x] = val * 0.15f;
        }
    }
}

// --- Ridgeline: sharp creases forming cliff lines ---
// Ridge noise creates natural mountain ranges and crystal veins.
// Used by: Mountains (via Rocky biome), Crystal
void elevation_ridgeline(float* grid, int w, int h,
                         std::mt19937& rng,
                         const BiomeProfile& prof) {
    unsigned seed = rng();
    float freq = prof.elevation_frequency;
    int oct = prof.elevation_octaves;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = ridge_noise(static_cast<float>(x), static_cast<float>(y),
                                     seed, freq, oct);
            grid[y * w + x] = val;
        }
    }
}

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add src/generators/elevation_strategies.cpp
git commit -m "feat: implement 4 elevation strategies"
```

---

### Task 5: Biome Profile Definitions

**Files:**
- Create: `src/generators/biome_profiles.cpp`

- [ ] **Step 1: Create the biome profiles file**

```cpp
#include "astra/biome_profile.h"

#include <array>
#include <utility>

namespace astra {

// Forward declare elevation strategies
void elevation_gentle(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_rugged(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_flat(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_ridgeline(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);

const BiomeProfile& biome_profile(Biome b) {
    // Natural biomes
    static const BiomeProfile grassland {
        "Grassland", elevation_gentle, 0.02f, 3, 0.92f,
        nullptr, 0.04f, 0.7f, 0.4f,   // moisture (Phase 2)
        nullptr, 0.5f,                  // structure (Phase 3)
        {}                              // scatter (Phase 4)
    };
    static const BiomeProfile forest {
        "Forest", elevation_gentle, 0.03f, 4, 0.88f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f, {}
    };
    static const BiomeProfile jungle {
        "Jungle", elevation_gentle, 0.035f, 4, 0.86f,
        nullptr, 0.04f, 0.65f, 0.35f,
        nullptr, 0.7f, {}
    };
    static const BiomeProfile sandy {
        "Sandy", elevation_gentle, 0.015f, 3, 0.95f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.3f, {}
    };
    static const BiomeProfile rocky {
        "Rocky", elevation_rugged, 0.04f, 5, 0.78f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.6f, {}
    };
    static const BiomeProfile volcanic {
        "Volcanic", elevation_rugged, 0.05f, 5, 0.75f,
        nullptr, 0.04f, 0.5f, 0.3f,
        nullptr, 0.6f, {}
    };
    static const BiomeProfile aquatic {
        "Aquatic", elevation_flat, 0.01f, 2, 0.98f,
        nullptr, 0.04f, 0.4f, 0.6f,
        nullptr, 0.3f, {}
    };
    static const BiomeProfile swamp {
        "Swamp", elevation_flat, 0.015f, 2, 0.96f,
        nullptr, 0.04f, 0.5f, 0.5f,
        nullptr, 0.5f, {}
    };
    static const BiomeProfile ice {
        "Ice", elevation_flat, 0.015f, 2, 0.96f,
        nullptr, 0.04f, 0.6f, 0.4f,
        nullptr, 0.5f, {}
    };
    static const BiomeProfile fungal {
        "Fungal", elevation_gentle, 0.025f, 4, 0.90f,
        nullptr, 0.04f, 0.6f, 0.4f,
        nullptr, 0.6f, {}
    };
    static const BiomeProfile crystal {
        "Crystal", elevation_ridgeline, 0.035f, 4, 0.82f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.6f, {}
    };
    static const BiomeProfile corroded {
        "Corroded", elevation_rugged, 0.04f, 4, 0.80f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f, {}
    };

    // Alien biomes
    static const BiomeProfile alien_crystalline {
        "AlienCrystalline", elevation_ridgeline, 0.04f, 4, 0.80f,
        nullptr, 0.04f, 0.6f, 0.4f,
        nullptr, 0.7f, {}
    };
    static const BiomeProfile alien_organic {
        "AlienOrganic", elevation_gentle, 0.03f, 4, 0.88f,
        nullptr, 0.04f, 0.5f, 0.4f,
        nullptr, 0.7f, {}
    };
    static const BiomeProfile alien_geometric {
        "AlienGeometric", elevation_flat, 0.02f, 2, 0.96f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.8f, {}
    };
    static const BiomeProfile alien_void {
        "AlienVoid", elevation_flat, 0.02f, 3, 0.94f,
        nullptr, 0.04f, 0.6f, 0.4f,
        nullptr, 0.6f, {}
    };
    static const BiomeProfile alien_light {
        "AlienLight", elevation_flat, 0.015f, 2, 0.97f,
        nullptr, 0.04f, 0.6f, 0.4f,
        nullptr, 0.3f, {}
    };

    // Scar biomes
    static const BiomeProfile scarred_scorched {
        "ScarredScorched", elevation_rugged, 0.045f, 4, 0.80f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.4f, {}
    };
    static const BiomeProfile scarred_glassed {
        "ScarredGlassed", elevation_flat, 0.02f, 3, 0.94f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.7f, {}
    };

    // Station fallback (not used for detail maps, but covers the enum)
    static const BiomeProfile station_fallback {
        "Station", elevation_flat, 0.02f, 2, 0.98f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.3f, {}
    };

    switch (b) {
        case Biome::Grassland:         return grassland;
        case Biome::Forest:            return forest;
        case Biome::Jungle:            return jungle;
        case Biome::Sandy:             return sandy;
        case Biome::Rocky:             return rocky;
        case Biome::Volcanic:          return volcanic;
        case Biome::Aquatic:           return aquatic;
        case Biome::Ice:               return ice;
        case Biome::Fungal:            return fungal;
        case Biome::Crystal:           return crystal;
        case Biome::Corroded:          return corroded;
        case Biome::AlienCrystalline:  return alien_crystalline;
        case Biome::AlienOrganic:      return alien_organic;
        case Biome::AlienGeometric:    return alien_geometric;
        case Biome::AlienVoid:         return alien_void;
        case Biome::AlienLight:        return alien_light;
        case Biome::ScarredScorched:   return scarred_scorched;
        case Biome::ScarredGlassed:    return scarred_glassed;
        case Biome::Station:
        default:                       return station_fallback;
    }
}

bool parse_biome(const std::string& name, Biome& out) {
    static const std::pair<const char*, Biome> table[] = {
        {"grassland",   Biome::Grassland},
        {"forest",      Biome::Forest},
        {"jungle",      Biome::Jungle},
        {"sandy",       Biome::Sandy},
        {"rocky",       Biome::Rocky},
        {"volcanic",    Biome::Volcanic},
        {"aquatic",     Biome::Aquatic},
        // Note: no Biome::Swamp in enum. Swamp terrain maps to Aquatic
        // via detail_biome_for_terrain. "swamp" aliases to Aquatic for now.
        {"swamp",       Biome::Aquatic},
        {"ice",         Biome::Ice},
        {"fungal",      Biome::Fungal},
        {"crystal",     Biome::Crystal},
        {"corroded",    Biome::Corroded},
        {"alien_crystalline", Biome::AlienCrystalline},
        {"alien_organic",     Biome::AlienOrganic},
        {"alien_geometric",   Biome::AlienGeometric},
        {"alien_void",        Biome::AlienVoid},
        {"alien_light",       Biome::AlienLight},
        {"scarred_scorched",  Biome::ScarredScorched},
        {"scarred_glassed",   Biome::ScarredGlassed},
        {"station",           Biome::Station},
    };
    for (const auto& [n, b] : table) {
        if (name == n) { out = b; return true; }
    }
    return false;
}

} // namespace astra
```

**Note on Swamp:** The current `Biome` enum doesn't have a `Swamp` value — swamp terrain maps to `Biome::Aquatic` in `detail_biome_for_terrain()`. We should consider adding `Biome::Swamp` to the enum in this task so swamp gets its own profile. If adding to the enum is too disruptive (colors, rendering), we can defer and alias "swamp" to Aquatic in the parse table for now. Check `tilemap.h` and `biome_colors()` before deciding.

- [ ] **Step 2: Check if adding Biome::Swamp is safe**

Search for all switch statements on `Biome` to see what needs updating:

Run: `grep -rn "case Biome::" src/ include/ | grep -c ""`

If there are fewer than ~10 switch sites, add `Biome::Swamp` to the enum, add a case in `biome_colors()`, and add cases in any other switches. If there are many, defer and alias swamp to aquatic.

- [ ] **Step 3: Commit**

```bash
git add src/generators/biome_profiles.cpp
git commit -m "feat: define BiomeProfile for all 19 biomes"
```

---

### Task 6: Terrain Compositor

**Files:**
- Create: `include/astra/terrain_compositor.h`
- Create: `src/generators/terrain_compositor.cpp`

- [ ] **Step 1: Create the compositor header**

```cpp
#pragma once

#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/tilemap.h"

namespace astra {

// Merge terrain channels into a TileMap.
// Phase 1: elevation only (wall/floor).
// Phase 2+: adds moisture (water) and structure (overrides).
void composite_terrain(TileMap& map, const TerrainChannels& channels,
                       const BiomeProfile& prof);

} // namespace astra
```

- [ ] **Step 2: Create the compositor implementation**

```cpp
#include "astra/terrain_compositor.h"

namespace astra {

void composite_terrain(TileMap& map, const TerrainChannels& channels,
                       const BiomeProfile& prof) {
    int w = channels.width;
    int h = channels.height;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float elev = channels.elev(x, y);

            // Phase 3: structure overrides (when implemented)
            // StructureMask s = channels.struc(x, y);
            // if (s == StructureMask::Wall)  { map.set(x, y, Tile::Wall);  continue; }
            // if (s == StructureMask::Floor) { map.set(x, y, Tile::Floor); continue; }
            // if (s == StructureMask::Water) { map.set(x, y, Tile::Water); continue; }

            // Phase 2: moisture → water (when implemented)
            // float moist = channels.moist(x, y);
            // if (moist > prof.water_threshold && elev < prof.flood_level) {
            //     map.set(x, y, Tile::Water);
            //     continue;
            // }

            // Phase 1: elevation only
            if (elev > prof.wall_threshold) {
                map.set(x, y, Tile::Wall);
            } else {
                map.set(x, y, Tile::Floor);
            }
        }
    }
}

} // namespace astra
```

- [ ] **Step 3: Commit**

```bash
git add include/astra/terrain_compositor.h src/generators/terrain_compositor.cpp
git commit -m "feat: add terrain compositor (elevation-only for Phase 1)"
```

---

### Task 7: DetailMapGeneratorV2

**Files:**
- Create: `src/generators/detail_map_generator_v2.cpp`

- [ ] **Step 1: Create the v2 generator**

```cpp
#include "astra/map_generator.h"
#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/terrain_compositor.h"
#include "astra/noise.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <cmath>

namespace astra {

static constexpr int bleed_margin = 20;

class DetailMapGeneratorV2 : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& /*rng*/) override {}
    void place_features(std::mt19937& /*rng*/) override {}
    void assign_regions(std::mt19937& rng) override;
    void generate_backdrop(unsigned /*seed*/) override {}

private:
    void apply_neighbor_bleed(TerrainChannels& channels, std::mt19937& rng);
};

void DetailMapGeneratorV2::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();
    const auto& prof = biome_profile(props_->biome);

    // Create channels
    TerrainChannels channels(w, h);

    // Layer 1: Elevation
    if (prof.elevation_fn) {
        prof.elevation_fn(channels.elevation.data(), w, h, rng, prof);
    }

    // Apply neighbor bleed at overworld tile edges
    apply_neighbor_bleed(channels, rng);

    // Composite channels into TileMap
    composite_terrain(*map_, channels, prof);
}

void DetailMapGeneratorV2::apply_neighbor_bleed(TerrainChannels& channels,
                                                 std::mt19937& rng) {
    int w = channels.width;
    int h = channels.height;

    struct Edge {
        Tile neighbor_tile;
        // For each cell, returns distance from this edge (0 = at edge)
        // and whether the cell is within the bleed margin.
    };

    // Check each direction for a different biome neighbor
    struct NeighborInfo {
        Tile tile;
        bool valid;
    };
    NeighborInfo neighbors[4] = {
        { props_->detail_neighbor_n, props_->detail_neighbor_n != Tile::Empty },
        { props_->detail_neighbor_s, props_->detail_neighbor_s != Tile::Empty },
        { props_->detail_neighbor_w, props_->detail_neighbor_w != Tile::Empty },
        { props_->detail_neighbor_e, props_->detail_neighbor_e != Tile::Empty },
    };

    for (int dir = 0; dir < 4; ++dir) {
        if (!neighbors[dir].valid) continue;

        Biome nb = detail_biome_for_terrain(neighbors[dir].tile, props_->biome);
        const auto& neighbor_prof = biome_profile(nb);

        // Skip if same biome — no bleed needed
        if (nb == props_->biome) continue;

        // Generate neighbor's elevation for the margin strip
        unsigned neighbor_seed = rng() ^ (dir * 7919u);
        std::vector<float> neighbor_elev(w * h, 0.0f);
        if (neighbor_prof.elevation_fn) {
            std::mt19937 nrng(neighbor_seed);
            neighbor_prof.elevation_fn(neighbor_elev.data(), w, h, nrng, neighbor_prof);
        }

        // Blend
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int dist = 0;
                switch (dir) {
                    case 0: dist = y; break;                  // north: distance from top
                    case 1: dist = (h - 1) - y; break;       // south: distance from bottom
                    case 2: dist = x; break;                  // west: distance from left
                    case 3: dist = (w - 1) - x; break;       // east: distance from right
                }
                if (dist >= bleed_margin) continue;

                float t = 1.0f - static_cast<float>(dist) / bleed_margin;
                t = t * t; // quadratic falloff — strong only right at edge

                int idx = y * w + x;
                channels.elevation[idx] = channels.elevation[idx] * (1.0f - t)
                                        + neighbor_elev[idx] * t;
            }
        }
    }
}

void DetailMapGeneratorV2::assign_regions(std::mt19937& /*rng*/) {
    Region surface;
    surface.name = "Surface";
    surface.flavor = RoomFlavor::Bridge;
    surface.lit = true;
    int rid = map_->add_region(surface);
    for (int y = 0; y < map_->height(); ++y) {
        for (int x = 0; x < map_->width(); ++x) {
            map_->set_region(x, y, rid);
        }
    }
}

std::unique_ptr<MapGenerator> make_detail_map_generator_v2() {
    return std::make_unique<DetailMapGeneratorV2>();
}

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add src/generators/detail_map_generator_v2.cpp
git commit -m "feat: add DetailMapGeneratorV2 with elevation + neighbor bleed"
```

---

### Task 8: Build Integration

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add new source files to CMakeLists.txt**

After line 39 (`src/generators/detail_map_generator.cpp`), add:

```cmake
    src/generators/biome_profiles.cpp
    src/generators/elevation_strategies.cpp
    src/generators/terrain_compositor.cpp
    src/generators/detail_map_generator_v2.cpp
```

- [ ] **Step 2: Verify build compiles**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds with no errors. If there are linker errors about missing symbols, check forward declarations match the implementations.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add v2 detail map generator sources"
```

---

### Task 9: Dev Test Tool

**Files:**
- Modify: `include/astra/game.h` (add declaration)
- Modify: `src/game.cpp` (add implementation)
- Modify: `src/dev_console.cpp` (add command dispatch)

- [ ] **Step 1: Add declaration to game.h**

After line 136 (`void dev_command_kill_hostiles();`), add:

```cpp
    void dev_command_biome_test(Biome biome, int layer);
```

- [ ] **Step 2: Add implementation to game.cpp**

After the `dev_warp_stamp_test()` function (after line 358), add:

```cpp
// Forward declare v2 generator factory
std::unique_ptr<MapGenerator> make_detail_map_generator_v2();

void Game::dev_command_biome_test(Biome biome, int layer) {
    (void)layer; // Phase 1: only elevation exists, layer param reserved for future
    animations_.clear();
    unsigned seed = static_cast<unsigned>(std::time(nullptr));

    auto props = default_properties(MapType::DetailMap);
    props.biome = biome;
    props.width = 360;
    props.height = 150;
    props.light_bias = 100;

    world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
    auto gen = make_detail_map_generator_v2();
    gen->generate(world_.map(), props, seed);
    world_.map().set_biome(biome);
    world_.map().set_location_name("[DEV] Biome Test: " + biome_profile(biome).name);

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();

    world_.visibility() = VisibilityMap(props.width, props.height);
    recompute_fov();
    compute_camera();
    world_.current_region() = -1;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    check_region_change();
}
```

- [ ] **Step 3: Add command dispatch to dev_console.cpp**

Add `#include "astra/biome_profile.h"` to the includes at the top of the file.

After the `warp` command block (after line 180), add:

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
        if (args.size() >= 3) {
            try { layer = std::stoi(args[2]); } catch (...) {
                log("Invalid layer: " + args[2]);
                return;
            }
        }
        game.dev_command_biome_test(biome, layer);
        log("Biome test: " + args[1] + " (360x150, layer " + std::to_string(layer) + ")");
    }
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Clean build, no errors.

- [ ] **Step 5: Smoke test**

Run: `./build/astra-dev`

1. Start a dev game
2. Open console with `~`
3. Type `biome_test grassland`
4. Verify: a 360x150 map appears, mostly floor with rare wall peaks
5. Type `biome_test rocky`
6. Verify: more wall outcrops, rougher terrain
7. Type `biome_test crystal`
8. Verify: ridge-like wall lines (different character from rocky)
9. Type `biome_test aquatic`
10. Verify: nearly all floor, almost no walls

- [ ] **Step 6: Commit**

```bash
git add include/astra/game.h src/game.cpp src/dev_console.cpp
git commit -m "feat: add biome_test dev command for visual terrain inspection"
```

---

### Task 10: Visual Tuning Pass

**Files:**
- Modify: `src/generators/biome_profiles.cpp` (parameter adjustments)

- [ ] **Step 1: Test every biome and record results**

Run the game, open console, test each biome:

```
biome_test grassland
biome_test forest
biome_test jungle
biome_test sandy
biome_test rocky
biome_test volcanic
biome_test aquatic
biome_test ice
biome_test fungal
biome_test crystal
biome_test corroded
biome_test alien_crystalline
biome_test alien_organic
biome_test alien_geometric
biome_test alien_void
biome_test alien_light
biome_test scarred_scorched
biome_test scarred_glassed
```

For each biome, check:
- Is it mostly open (85-95% floor)?
- Does it look distinct from other biomes?
- Are walls forming interesting shapes (not random speckle)?

- [ ] **Step 2: Adjust parameters as needed**

Tune `elevation_frequency`, `elevation_octaves`, `wall_threshold`, and amplitude multipliers in the strategy implementations. Common adjustments:
- Too many walls → raise `wall_threshold` or lower amplitude multiplier
- Too uniform → increase `elevation_frequency` or add octaves
- Ridgeline too dense → lower `elevation_frequency`

- [ ] **Step 3: Commit tuning changes**

```bash
git add src/generators/biome_profiles.cpp src/generators/elevation_strategies.cpp
git commit -m "tune: adjust elevation parameters after visual testing"
```

---

## Verification

After all tasks:

1. **Build:** `cmake -B build -DDEV=ON && cmake --build build` — clean, no warnings
2. **Every biome testable:** `biome_test <name>` works for all 19 biomes
3. **Open-first:** Grassland/Sandy/Aquatic/Ice are 90%+ floor. Rocky/Volcanic are 75%+ floor. No biome is a wall maze.
4. **Distinct:** Crystal and Rocky look different. Flat biomes (Aquatic/Ice) are clearly different from gentle (Grassland/Forest).
5. **Old generator untouched:** `warp random`, `warp stamp ruins`, normal gameplay all work identically — the old DetailMapGenerator is still in place.
6. **360x150 maps:** The dev test generates full-sized maps, not 120x50.
