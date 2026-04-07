# Detail Map Generation v2 — Phase 2: Moisture Layer

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the moisture channel with 5 strategies producing organic water shapes — pools, rivers, coastlines, and lava channels.

**Architecture:** Moisture strategies write to the moisture float grid, reading the elevation grid for placement. The compositor gains water tile handling. Each biome gets a moisture strategy assignment with tuned parameters.

**Tech Stack:** C++20, value noise fBm

**Spec:** `docs/superpowers/specs/2026-04-07-detail-map-generation-v2-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/generators/moisture_strategies.cpp` | Create | 5 moisture strategy implementations |
| `src/generators/biome_profiles.cpp` | Modify | Assign moisture_fn + tune parameters for all 19 biomes |
| `src/generators/terrain_compositor.cpp` | Modify | Add water tile compositing |
| `src/generators/detail_map_generator_v2.cpp` | Modify | Call moisture strategy in generate_layout |
| `CMakeLists.txt` | Modify | Add moisture_strategies.cpp |

---

### Task 1: Moisture Strategies

**Files:**
- Create: `src/generators/moisture_strategies.cpp`

- [ ] **Step 1: Create moisture_strategies.cpp with all 5 strategies**

All functions in `namespace astra`, matching `MoistureStrategy` signature:
```cpp
void(*)(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof)
```

**`moisture_none`** — No-op. Grid stays zero-initialized.

**`moisture_pools`** — Organic blob-shaped pools in low elevation:
1. Pick 6-14 seed points biased toward low elevation (sample 5 random positions, pick lowest)
2. Each pool gets radius 4-16 tiles
3. For each cell near a pool center: compute distance, apply radial falloff, distort shape with `fbm(x, y, pool_seed, prof.moisture_frequency, 3)` 
4. Combine pools via max (not additive)
5. Gate by elevation: fade moisture to zero where `elevation > flood_level`
6. Clamp to [0, 1]

**`moisture_river`** — Sinuous water band:
1. Choose axis (70% horizontal, 30% vertical)
2. Centerline: `base_y + A*sin(x*freq) + B*sin(x*freq2+phase)` with noise perturbation
3. Width: 3-6 tiles base, varied by noise along length
4. Soft bank edges (2-tile gradient)
5. Bias centerline toward low elevation

**`moisture_coastline`** — Large water body with irregular shore:
1. Pick random edge (N/S/E/W)
2. Shoreline curve: `base_depth + fbm * amplitude` (35-50% water coverage)
3. Add peninsula/inlet features via ridge_noise
4. 4-tile soft shore gradient
5. Scatter 3-5 small tidal pools near shore on land side

**`moisture_channels`** — Branching liquid channels:
1. 1-2 source points at random edges
2. Random walk toward opposite edge with sinuosity from noise
3. Branch every 20-40 steps at 30-60 degrees, length 15-40 tiles
4. Sub-branches with 30% probability
5. Channel width 2-3 tiles (primary), 1-2 (branches), soft edges
6. Bias walk toward low elevation

- [ ] **Step 2: Commit**

```bash
git add src/generators/moisture_strategies.cpp
git commit -m "feat: implement 5 moisture strategies"
```

---

### Task 2: Update Biome Profiles

**Files:**
- Modify: `src/generators/biome_profiles.cpp`

- [ ] **Step 1: Add forward declarations for moisture strategies**

After the elevation strategy forward declarations, add:
```cpp
void moisture_none(float*, int, int, std::mt19937&, const float*, const BiomeProfile&);
void moisture_pools(float*, int, int, std::mt19937&, const float*, const BiomeProfile&);
void moisture_river(float*, int, int, std::mt19937&, const float*, const BiomeProfile&);
void moisture_coastline(float*, int, int, std::mt19937&, const float*, const BiomeProfile&);
void moisture_channels(float*, int, int, std::mt19937&, const float*, const BiomeProfile&);
```

- [ ] **Step 2: Update all 19 biome profiles**

Replace `nullptr` moisture_fn with the correct strategy and tune parameters:

| Biome | moisture_fn | frequency | water_threshold | flood_level |
|-------|------------|-----------|-----------------|-------------|
| Grassland | river | 0.03 | 0.85 | 0.4 |
| Forest | pools | 0.04 | 0.65 | 0.45 |
| Jungle | pools | 0.05 | 0.55 | 0.5 |
| Sandy | none | 0.04 | 0.7 | 0.4 |
| Rocky | none | 0.04 | 0.7 | 0.4 |
| Volcanic | channels | 0.03 | 0.6 | 0.6 |
| Aquatic | coastline | 0.03 | 0.5 | 0.95 |
| Ice | pools | 0.035 | 0.7 | 0.45 |
| Fungal | pools | 0.04 | 0.6 | 0.45 |
| Crystal | pools | 0.04 | 0.85 | 0.35 |
| Corroded | none | 0.04 | 0.7 | 0.4 |
| AlienCrystalline | pools | 0.05 | 0.65 | 0.45 |
| AlienOrganic | pools | 0.045 | 0.55 | 0.5 |
| AlienGeometric | none | 0.04 | 0.7 | 0.4 |
| AlienVoid | pools | 0.03 | 0.7 | 0.5 |
| AlienLight | pools | 0.035 | 0.6 | 0.5 |
| ScarredScorched | none | 0.04 | 0.7 | 0.4 |
| ScarredGlassed | none | 0.04 | 0.7 | 0.4 |
| Station | none | 0.04 | 0.7 | 0.4 |

- [ ] **Step 3: Commit**

```bash
git add src/generators/biome_profiles.cpp
git commit -m "feat: assign moisture strategies to all biome profiles"
```

---

### Task 3: Update Compositor + Generator + CMake

**Files:**
- Modify: `src/generators/terrain_compositor.cpp`
- Modify: `src/generators/detail_map_generator_v2.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Update compositor to handle water**

In the compositing loop, add moisture check before elevation check:

```cpp
float elev = channels.elev(x, y);
float moist = channels.moist(x, y);

if (moist > prof.water_threshold && elev < prof.flood_level) {
    map.set(x, y, Tile::Water);
} else if (elev > prof.wall_threshold) {
    map.set(x, y, Tile::Wall);
} else {
    map.set(x, y, Tile::Floor);
}
```

- [ ] **Step 2: Update generator to call moisture strategy**

In `generate_layout()`, after the elevation call and before `apply_neighbor_bleed`:

```cpp
if (prof.moisture_fn) {
    prof.moisture_fn(channels.moisture.data(), w, h, rng,
                     channels.elevation.data(), prof);
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

After `src/generators/elevation_strategies.cpp`, add:
```
    src/generators/moisture_strategies.cpp
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/generators/terrain_compositor.cpp src/generators/detail_map_generator_v2.cpp CMakeLists.txt
git commit -m "feat: wire moisture layer into compositor and generator"
```

---

### Task 4: Visual Verification

- [ ] **Step 1: Test all biomes with water**

```
biome_test aquatic      → coastline with large water body
biome_test forest       → organic pools in clearings
biome_test jungle       → aggressive pools, lots of water
biome_test volcanic     → branching lava channels
biome_test grassland    → rare sinuous river
biome_test ice          → frozen pools
biome_test fungal       → bio-pools
biome_test crystal      → rare small pools
biome_test alien_organic → pulsing pools
```

- [ ] **Step 2: Test biomes without water**

```
biome_test sandy        → no water
biome_test rocky        → no water
biome_test alien_geometric → no water
biome_test scarred_scorched → no water
```

- [ ] **Step 3: Tune parameters as needed**

- [ ] **Step 4: Commit tuning**

```bash
git commit -am "tune: adjust moisture parameters after visual testing"
```

---

## Verification

1. **Build:** clean compile with `-DDEV=ON`
2. **Water biomes:** organic pool/river/coastline/channel shapes, NOT noise speckle
3. **Dry biomes:** zero water tiles
4. **Aquatic:** ~35-50% water coverage with irregular shoreline
5. **Volcanic:** branching channels cutting through rock
6. **Old generator:** untouched, normal gameplay works
