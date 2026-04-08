# Flora & Mineral Ground Fixtures — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move Tier 2/3 floor decorations from renderer to fixture data, add 8 new ground resource FixtureTypes, and implement per-biome flora strategies with noise-based patch placement.

**Architecture:** Add FloraStrategy function pointer to BiomeProfile (same pattern as ElevationStrategy). Each biome defines a strategy that uses Perlin noise (fbm from noise.h) to place flora/mineral fixtures in terrain-aware patches. Renderer's resolve_floor() loses Tier 2/3, resolve_fixture() gains 8 new cases. Settlement clearing automatically handles the new types.

**Tech Stack:** C++20, `cmake -B build -DDEV=ON && cmake --build build`

**Spec:** `docs/superpowers/specs/2026-04-08-flora-mineral-fixtures-design.md`

---

## File Structure

```
Modified:
  include/astra/tilemap.h                   — 8 new FixtureType values
  src/tilemap.cpp                           — make_fixture() cases for 8 types
  include/astra/biome_profile.h             — FloraStrategy type alias, field in BiomeProfile
  src/generators/biome_profiles.cpp         — assign flora_fn per biome
  src/generators/detail_map_generator_v2.cpp — call flora_fn after scatter
  src/terminal_theme.cpp                    — remove Tier 2/3 from resolve_floor(),
                                              add 8 fixture cases in resolve_fixture(),
                                              add 8 cases in fixture_glyph()
  src/generators/exterior_decorator.cpp     — clear new types in settlements
  src/game_rendering.cpp                    — fixture names + descriptions
  src/game_interaction.cpp                  — bump messages
  src/map_editor.cpp                        — fixture names

Create:
  src/generators/flora_strategies.cpp       — all per-biome flora strategy functions
  CMakeLists.txt                            — add flora_strategies.cpp
```

---

### Task 1: New FixtureTypes

Add 8 ground resource fixture types.

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: `src/tilemap.cpp`

- [ ] **Step 1: Add fixture type enum values**

In `include/astra/tilemap.h`, add after `Planter` in the FixtureType enum:

```cpp
    // Ground resources (Phase 6 — flora/mineral fixtures)
    FloraFlower,        // passable wildflower — Botany
    FloraHerb,          // passable herb/fern — Botany
    FloraMushroom,      // passable mushroom — Botany
    FloraGrass,         // passable tall grass/reeds — Botany/Fiber
    FloraLichen,        // passable lichen/moss — Botany
    MineralOre,         // passable ore deposit — Mining
    MineralCrystal,     // passable crystal shard — Mining
    ScrapComponent,     // passable salvage scrap — Salvaging
```

- [ ] **Step 2: Add make_fixture cases**

In `src/tilemap.cpp`, in `make_fixture()`, add cases for all 8 (all passable, non-interactable, no vision blocking):

```cpp
        case FixtureType::FloraFlower:
        case FixtureType::FloraHerb:
        case FixtureType::FloraMushroom:
        case FixtureType::FloraGrass:
        case FixtureType::FloraLichen:
        case FixtureType::MineralOre:
        case FixtureType::MineralCrystal:
        case FixtureType::ScrapComponent:
            fd.passable = true; fd.interactable = false; break;
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Compile with -Wswitch warnings in terminal_theme.cpp (expected, fixed in later tasks).

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp
git commit -m "feat: add 8 ground resource fixture types (flora, mineral, scrap)"
```

---

### Task 2: FloraStrategy in BiomeProfile

Add the strategy type and field.

**Files:**
- Modify: `include/astra/biome_profile.h`

- [ ] **Step 1: Add FloraStrategy type alias and field**

In `include/astra/biome_profile.h`, add after the StructureStrategy type alias:

```cpp
using FloraStrategy = void(*)(TileMap& map, int w, int h,
                               std::mt19937& rng,
                               const float* elevation,
                               const float* moisture,
                               const BiomeProfile& prof);
```

Add to BiomeProfile struct, after the scatter vector:

```cpp
    // Layer 5: Flora / ground resources
    FloraStrategy flora_fn = nullptr;
```

The include for TileMap is already present via `#include "astra/tilemap.h"`.

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. No biome assigns flora_fn yet — it's nullptr by default.

- [ ] **Step 3: Commit**

```bash
git add include/astra/biome_profile.h
git commit -m "feat: add FloraStrategy type alias and field to BiomeProfile"
```

---

### Task 3: Flora Strategies Implementation

Implement all per-biome flora strategy functions.

**Files:**
- Create: `src/generators/flora_strategies.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create flora_strategies.cpp**

This file contains all flora strategy functions. Each uses `fbm()` from `noise.h` for patch-based placement and/or uniform random for sparse placement. All follow the same pattern: iterate floor tiles, check noise/random threshold, place fixture if no existing fixture.

Key patterns used across strategies:
- **Noise patches**: `fbm(x, y, seed, frequency) > threshold` → place fixture
- **Sparse random**: `rng() % 100 < density_pct` → place fixture
- **Near-tree clustering**: check if NaturalObstacle exists within N tiles
- **Moisture-aware**: check `moisture[y*w+x] > threshold` before placing

Each strategy gets a unique noise seed derived from `rng()` so different flora layers don't correlate.

Implement these strategies:
- `flora_grassland` — FloraFlower (noise 0.08, thresh 0.55), FloraGrass (noise 0.05, thresh 0.45), FloraHerb (2% sparse)
- `flora_forest` — FloraMushroom (near trees, 30%), FloraHerb (noise 0.06, thresh 0.6), FloraGrass (3% sparse far from trees)
- `flora_jungle` — FloraFlower (noise 0.07, thresh 0.5), FloraGrass (8% uniform), FloraHerb (2% sparse)
- `flora_rocky` — MineralOre (noise 0.1, thresh 0.65), FloraLichen (3% sparse)
- `flora_volcanic` — MineralOre (noise 0.08, thresh 0.6, only where elevation > 0.5), ScrapComponent (1% sparse)
- `flora_fungal` — FloraMushroom (noise 0.06, thresh 0.4), FloraHerb (2% sparse)
- `flora_ice` — MineralCrystal (noise 0.09, thresh 0.6), FloraLichen (2% sparse)
- `flora_marsh` — FloraGrass (where moisture > 0.3, 15% density), FloraHerb (4% sparse), FloraFlower (2% sparse)
- `flora_crystal` — MineralCrystal (noise 0.07, thresh 0.45)
- `flora_corroded` — ScrapComponent (noise 0.08, thresh 0.55), MineralOre (2% sparse)
- `flora_sandy` — FloraGrass (1% sparse), MineralOre (2% sparse)
- `flora_scarred` — ScrapComponent (noise 0.07, thresh 0.5), MineralOre (1% sparse)
- `flora_aquatic` — FloraGrass (5% near water), MineralCrystal (1% sparse)
- `flora_alien` — MineralCrystal + FloraMushroom mixed (noise 0.06, thresh 0.5)

All functions share the same signature: `void(TileMap&, int, int, std::mt19937&, const float*, const float*, const BiomeProfile&)`.

Common helper at the top of the file:

```cpp
static void place_flora(TileMap& map, int x, int y, FixtureType type) {
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    if (map.get(x, y) != Tile::Floor) return;
    if (map.fixture_id(x, y) >= 0) return;
    FixtureData fd;
    fd.type = type;
    fd.passable = true;
    fd.interactable = false;
    fd.blocks_vision = false;
    map.add_fixture(x, y, fd);
}
```

Declare all strategy functions in a header block at the top (or use forward declarations) so `biome_profiles.cpp` can reference them. Since the existing strategies (elevation, moisture, structure) are declared at the top of their respective `.cpp` files and referenced directly from `biome_profiles.cpp`, follow the same pattern — declare the flora functions in `biome_profile.h` or at the top of `biome_profiles.cpp`.

Best approach: declare them in `biome_profile.h` after the FloraStrategy type alias:

```cpp
// Flora strategy functions (defined in flora_strategies.cpp)
void flora_grassland(TileMap& map, int w, int h, std::mt19937& rng,
                     const float* elevation, const float* moisture,
                     const BiomeProfile& prof);
void flora_forest(TileMap& map, int w, int h, std::mt19937& rng,
                  const float* elevation, const float* moisture,
                  const BiomeProfile& prof);
// ... etc for all strategies
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `src/generators/flora_strategies.cpp` to the source list.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile. Strategies exist but aren't called yet.

- [ ] **Step 4: Commit**

```bash
git add src/generators/flora_strategies.cpp include/astra/biome_profile.h CMakeLists.txt
git commit -m "feat: implement per-biome flora strategies with noise-based patches"
```

---

### Task 4: Assign Flora Strategies to Biomes

Wire each biome profile to its flora strategy.

**Files:**
- Modify: `src/generators/biome_profiles.cpp`

- [ ] **Step 1: Assign flora_fn for each biome**

In `src/generators/biome_profiles.cpp`, for each biome profile definition, set the `flora_fn` field:

```cpp
// Grassland
prof.flora_fn = flora_grassland;

// Forest
prof.flora_fn = flora_forest;

// Jungle
prof.flora_fn = flora_jungle;

// Rocky
prof.flora_fn = flora_rocky;

// Volcanic
prof.flora_fn = flora_volcanic;

// Fungal
prof.flora_fn = flora_fungal;

// Ice
prof.flora_fn = flora_ice;

// Marsh
prof.flora_fn = flora_marsh;

// Crystal
prof.flora_fn = flora_crystal;

// Corroded
prof.flora_fn = flora_corroded;

// Sandy
prof.flora_fn = flora_sandy;

// ScarredScorched, ScarredGlassed
prof.flora_fn = flora_scarred;

// Aquatic
prof.flora_fn = flora_aquatic;

// All alien biomes
prof.flora_fn = flora_alien;
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 3: Commit**

```bash
git add src/generators/biome_profiles.cpp
git commit -m "feat: assign flora strategies to all biome profiles"
```

---

### Task 5: Call Flora Strategy from V2 Generator

Add the flora phase call after scatter, before connectivity.

**Files:**
- Modify: `src/generators/detail_map_generator_v2.cpp`

- [ ] **Step 1: Add flora call in place_features()**

In `detail_map_generator_v2.cpp`, in `place_features()`, after the riparian lush zone block (Layer B) and before the POI Phase block, add:

```cpp
    // --- Flora Phase (ground resources) ---
    if (prof.flora_fn) {
        prof.flora_fn(*map_, w, h, rng,
                      channels_.elevation.data(),
                      channels_.moisture.data(),
                      prof);
    }
```

- [ ] **Step 2: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: `biome_test grassland` — flora fixtures should now be placed (but invisible until renderer is updated in Task 6). You can verify by looking for extra Fixture tiles on the map.

- [ ] **Step 3: Commit**

```bash
git add src/generators/detail_map_generator_v2.cpp
git commit -m "feat: call flora strategy from v2 generator after scatter"
```

---

### Task 6: Renderer — Remove Tier 2/3, Add Fixture Cases

The big renderer change: remove decoration generation from floor rendering, add fixture rendering for 8 new types.

**Files:**
- Modify: `src/terminal_theme.cpp`

- [ ] **Step 1: Strip Tier 2 and Tier 3 from resolve_floor()**

In `resolve_floor()` (around lines 389-563), remove the Tier 3 block (`if (roll < 1)`) and the Tier 2 block (`if (roll < 5)`). Keep only:
- The Tier 1 basic scatter block (`if (roll >= s.threshold)` returns plain floor, else returns scatter glyph)
- The station early return

After this change, `resolve_floor()` should be roughly:

```cpp
static ResolvedVisual resolve_floor(uint8_t seed, Biome biome, Color floor_color, Color remembered_color) {
    if (biome == Biome::Station) {
        return {'.', nullptr, floor_color, Color::Default};
    }

    int roll = seed % 100;
    int variant = (seed >> 4) % 8;

    // Basic scatter in dim color (~15% density)
    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:    s = {15, ",:`",  3}; break;
        case Biome::Volcanic: s = {15, ",';" , 3}; break;
        case Biome::Ice:      s = {12, "'`,",  3}; break;
        case Biome::Sandy:    s = {15, ",`:",  3}; break;
        case Biome::Aquatic:  s = {10, ",:",   2}; break;
        case Biome::Fungal:   s = {15, "\",'", 3}; break;
        case Biome::Crystal:  s = {15, "*'`",  3}; break;
        case Biome::Corroded: s = {15, ",:;",  3}; break;
        case Biome::Forest:   s = {15, "\",'", 3}; break;
        case Biome::Grassland:s = {15, ",`.",  3}; break;
        case Biome::Jungle:   s = {15, "\",'", 3}; break;
        case Biome::Marsh:    s = {15, "\",~", 3}; break;
        default: return {'.', nullptr, floor_color, Color::Default};
    }

    if (roll >= s.threshold) {
        return {'.', nullptr, floor_color, Color::Default};
    }
    char scatter = s.glyphs[variant % s.count];
    return {scatter, nullptr, remembered_color, Color::Default};
}
```

- [ ] **Step 2: Add fixture rendering for 8 new types**

In `resolve_fixture()`, add cases for each new type. Move the visual variants from the old Tier 2/3 code into these cases. Each type switches on biome for biome-specific visuals:

**FloraFlower** — colorful flowers per biome (the old Tier 2 grassland flowers, jungle flowers, etc.)
**FloraHerb** — green/brown plants (ƒ τ in green/brown)
**FloraMushroom** — mushrooms in biome colors (Φ in green/magenta)
**FloraGrass** — tall grass, reeds (" τ in green)
**FloraLichen** — subtle patches (· ° in gray/cyan)
**MineralOre** — ore chunks (, ` in gray/red/brown)
**MineralCrystal** — crystal shards (◇ ◆ in magenta/cyan)
**ScrapComponent** — metal scraps (⚙ % in gray/orange)

Use `seed % N` for variant selection within each biome, same pattern as NaturalObstacle.

- [ ] **Step 3: Add fixture_glyph cases**

In `fixture_glyph()`, add ASCII fallback glyphs:

```cpp
        case FixtureType::FloraFlower:     return '*';
        case FixtureType::FloraHerb:       return ',';
        case FixtureType::FloraMushroom:   return 'o';
        case FixtureType::FloraGrass:      return '"';
        case FixtureType::FloraLichen:     return '.';
        case FixtureType::MineralOre:      return ',';
        case FixtureType::MineralCrystal:  return '*';
        case FixtureType::ScrapComponent:  return '%';
```

- [ ] **Step 4: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: `biome_test grassland` — should see flower patches (data-driven) instead of uniform scatter.
Test: `biome_test rocky` — should see mineral ore veins.
Test: `biome_test grassland settlement` — settlement area should be clear of flora.

- [ ] **Step 5: Commit**

```bash
git add src/terminal_theme.cpp
git commit -m "feat: move floor decorations to fixture rendering, strip Tier 2/3 from resolve_floor"
```

---

### Task 7: Settlement Clearing + UI Support

Update ExteriorDecorator clearing and add fixture names/descriptions/bump messages.

**Files:**
- Modify: `src/generators/exterior_decorator.cpp`
- Modify: `src/game_rendering.cpp`
- Modify: `src/game_interaction.cpp`
- Modify: `src/map_editor.cpp`

- [ ] **Step 1: Update clearing in ExteriorDecorator**

In `src/generators/exterior_decorator.cpp`, replace the fixture type check in the clearing loop with a simpler approach — clear any passable non-interactable fixture:

```cpp
            if (fid < 0) continue;
            const auto& f = map.fixture(fid);
            if (f.passable && !f.interactable) {
                map.remove_fixture(x, y);
            }
```

This covers NaturalObstacle, ShoreDebris, and all 8 new types without listing them individually.

- [ ] **Step 2: Add fixture names in game_rendering.cpp**

In the `fixture_type_name` function, add:

```cpp
        case FixtureType::FloraFlower:     return "Wildflower";
        case FixtureType::FloraHerb:       return "Herb";
        case FixtureType::FloraMushroom:   return "Mushroom";
        case FixtureType::FloraGrass:      return "Tall Grass";
        case FixtureType::FloraLichen:     return "Lichen";
        case FixtureType::MineralOre:      return "Ore Deposit";
        case FixtureType::MineralCrystal:  return "Crystal Shard";
        case FixtureType::ScrapComponent:  return "Salvage";
```

In the `fixture_type_desc` function, add:

```cpp
        case FixtureType::FloraFlower:     return "Colorful wildflowers sway in the breeze.";
        case FixtureType::FloraHerb:       return "A patch of useful-looking herbs.";
        case FixtureType::FloraMushroom:   return "Mushrooms cluster in the damp ground.";
        case FixtureType::FloraGrass:      return "Tall grass rustles as you pass.";
        case FixtureType::FloraLichen:     return "Hardy lichen clings to the surface.";
        case FixtureType::MineralOre:      return "Raw mineral ore exposed at the surface.";
        case FixtureType::MineralCrystal:  return "A crystalline formation juts from the ground.";
        case FixtureType::ScrapComponent:  return "Salvageable scrap metal and circuitry.";
```

- [ ] **Step 3: Add bump messages in game_interaction.cpp**

These are passable fixtures so players walk over them, not bump into them. Add walk-over messages in the walk-over section (around line 97):

```cpp
                    case FixtureType::FloraFlower:  log("You step through wildflowers."); break;
                    case FixtureType::FloraHerb:    log("Herbs crunch underfoot."); break;
                    case FixtureType::FloraMushroom: log("You step past mushrooms."); break;
                    case FixtureType::FloraGrass:   log("Tall grass brushes your legs."); break;
                    case FixtureType::FloraLichen:  break; // silent — too subtle
                    case FixtureType::MineralOre:   log("You step over exposed ore."); break;
                    case FixtureType::MineralCrystal: log("Crystal fragments crunch underfoot."); break;
                    case FixtureType::ScrapComponent: log("You step over scattered scrap."); break;
```

- [ ] **Step 4: Add map editor names**

In `src/map_editor.cpp`, in the fixture name switch, add:

```cpp
        case FixtureType::FloraFlower:     return "Flora: Flower";
        case FixtureType::FloraHerb:       return "Flora: Herb";
        case FixtureType::FloraMushroom:   return "Flora: Mushroom";
        case FixtureType::FloraGrass:      return "Flora: Grass";
        case FixtureType::FloraLichen:     return "Flora: Lichen";
        case FixtureType::MineralOre:      return "Mineral: Ore";
        case FixtureType::MineralCrystal:  return "Mineral: Crystal";
        case FixtureType::ScrapComponent:  return "Scrap Component";
```

- [ ] **Step 5: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: `biome_test grassland settlement` — settlement should be clean of flora.
Test: walk over flowers — should see walk-over messages.

- [ ] **Step 6: Commit**

```bash
git add src/generators/exterior_decorator.cpp src/game_rendering.cpp \
  src/game_interaction.cpp src/map_editor.cpp
git commit -m "feat: settlement clearing for new fixtures, names, descriptions, walk-over messages"
```

---

### Task 8: Visual Testing and Tuning

Test flora placement across all biomes and tune noise parameters.

**Files:** None new — testing and tuning.

- [ ] **Step 1: Test each biome**

```
biome_test grassland       — flower patches + tall grass sweeps
biome_test forest          — mushrooms near trees, fern clusters
biome_test rocky           — mineral ore veins
biome_test volcanic        — ore near lava, sparse scrap
biome_test fungal          — dense mushroom patches
biome_test ice             — crystal shards, lichen
biome_test marsh           — dense reeds near water
biome_test crystal         — crystal formations
biome_test corroded        — scrap piles
biome_test sandy           — sparse scrub + minerals
biome_test scarred_scorched — debris fields
```

Verify: flora appears in patches (not uniform), visually distinct from ground, readable.

- [ ] **Step 2: Test settlement clearing**

```
biome_test grassland settlement
biome_test forest settlement
```

Verify: settlement area clear of flora, surrounding terrain has flora.

- [ ] **Step 3: Tune parameters**

Adjust noise frequencies, thresholds, and density percentages in `flora_strategies.cpp` as needed. Key things to watch:
- Too dense? Raise threshold or lower density %
- Too sparse? Lower threshold or raise density %
- Too uniform? Lower noise frequency for bigger patches
- Patches too large? Raise noise frequency

- [ ] **Step 4: Commit fixes**

```bash
git add -u
git commit -m "fix: tune flora placement parameters after visual testing"
```
