# Flora & Mineral Ground Fixtures — Design Spec

Move Tier 2/3 floor decorations from renderer-side visuals to proper fixture data entities. Add per-biome flora strategies for terrain-aware patch placement. Future-proof for trade skill harvesting.

## Problem

Floor decorations (flowers, mushrooms, gravel, etc.) are currently generated at render time by `resolve_floor()` using position seed. They exist only in the renderer — not in map data. This means:
- Settlements can't clear them (they're not fixtures)
- Both renderers (terminal + SDL) must duplicate the logic
- No path to making them interactive (harvesting, alchemy)

## Solution

Replace Tier 2/3 renderer decorations with data-driven fixtures placed by per-biome flora strategies. Tier 1 (subtle ground texture: `,` `` ` `` at ~15%) stays in the renderer.

## New Fixture Types

Eight types covering three future trade skill categories:

| FixtureType | Biomes | Future Skill | Visual Examples |
|-------------|--------|--------------|-----------------|
| `FloraFlower` | Grassland, Jungle, Marsh | Botany | ✿ ✶ • wildflowers in varied colors |
| `FloraHerb` | Forest, Marsh, Grassland | Botany | ƒ τ small green/brown plants |
| `FloraMushroom` | Fungal, Forest | Botany | Φ mushrooms in biome colors |
| `FloraGrass` | Grassland, Marsh, Jungle | Botany/Fiber | " τ tall grass, reeds |
| `FloraLichen` | Rocky, Ice | Botany | · ° subtle colored patches |
| `MineralOre` | Rocky, Volcanic | Mining | ite ore chunks, slag |
| `MineralCrystal` | Crystal, Ice | Mining | ◇ ◆ crystal shards |
| `ScrapComponent` | Corroded, Scarred | Salvaging | ⚙ % ≠ metal scraps, circuitry |

All types: passable = true, interactable = false, blocks_vision = false.

## Flora Strategy

New function pointer in BiomeProfile:

```cpp
using FloraStrategy = void(*)(TileMap& map, int w, int h,
                               std::mt19937& rng,
                               const float* elevation,
                               const float* moisture,
                               const BiomeProfile& prof);
```

Called after scatter (NaturalObstacle) placement, before connectivity. Receives elevation and moisture channels for terrain-aware decisions.

### Per-Biome Strategies

**Grassland** (`flora_grassland`):
- FloraFlower: Perlin noise patches, frequency ~0.08, threshold 0.55. Dense colorful clusters.
- FloraGrass: separate noise layer, frequency ~0.05, threshold 0.45. Larger sweeps of tall grass.
- FloraHerb: sparse random, ~2% density. Scattered throughout.

**Forest** (`flora_forest`):
- FloraMushroom: cluster near NaturalObstacle fixtures (near trees). 30% chance within 2 tiles of a tree.
- FloraHerb: Perlin noise patches, frequency ~0.06, threshold 0.6. Fern clusters.
- FloraGrass: sparse, ~3% in open areas (tiles far from trees).

**Jungle** (`flora_jungle`):
- FloraFlower: noise patches, dense. Frequency ~0.07, threshold 0.5.
- FloraGrass: dense reeds/ferns, ~8% uniform.
- FloraHerb: sparse, ~2%.

**Rocky** (`flora_rocky`):
- MineralOre: noise patches, frequency ~0.1, threshold 0.65. Vein-like clusters.
- FloraLichen: sparse uniform, ~3%.

**Volcanic** (`flora_volcanic`):
- MineralOre: noise patches near high elevation (lava edge), frequency ~0.08, threshold 0.6.
- ScrapComponent: very sparse, ~1% (ancient debris).

**Fungal** (`flora_fungal`):
- FloraMushroom: dense, noise patches frequency ~0.06, threshold 0.4. Primary ground cover.
- FloraHerb: sparse spore-related, ~2%.

**Ice** (`flora_ice`):
- MineralCrystal: noise patches, frequency ~0.09, threshold 0.6.
- FloraLichen: sparse, ~2%.

**Marsh** (`flora_marsh`):
- FloraGrass: dense near water (uses moisture channel). Where moisture > 0.3, density ~15%.
- FloraHerb: moderate, ~4%.
- FloraFlower: sparse, ~2%.

**Crystal** (`flora_crystal`):
- MineralCrystal: dense noise patches, frequency ~0.07, threshold 0.45.

**Corroded** (`flora_corroded`):
- ScrapComponent: noise patches, frequency ~0.08, threshold 0.55. Junk piles.
- MineralOre: sparse, ~2%.

**Sandy** (`flora_sandy`):
- FloraGrass: very sparse, ~1%. Hardy scrub.
- MineralOre: sparse uniform, ~2%. Surface minerals.

**Scarred (Scorched/Glassed)** (`flora_scarred`):
- ScrapComponent: moderate, noise patches. Debris fields.
- MineralOre: sparse near craters.

**Aquatic** (`flora_aquatic`):
- FloraGrass: seaweed/kelp near water, ~5%.
- MineralCrystal: sparse, ~1%. Sea glass.

**Alien biomes** (`flora_alien`):
- Use the alien architecture type to determine fixture mix. Crystalline aliens → MineralCrystal, Organic → FloraMushroom/FloraHerb, etc. Noise-patched.

## Renderer Changes

### Remove from resolve_floor()
Delete Tier 2 (roll < 5, biome-specific colored decorations) and Tier 3 (roll < 1, rare decorations) from `resolve_floor()`. Keep only:
- Tier 1: basic scatter (~15%) — subtle `,` `` ` `` `.` in dim biome color
- Base: plain `.` in floor color (~85%)

### Add to resolve_fixture()
Add cases for all 8 new FixtureTypes. Each resolves glyph/color from biome + seed, same pattern as NaturalObstacle:

```cpp
case FixtureType::FloraFlower: {
    switch (biome) {
        case Biome::Grassland: {
            static const ResolvedVisual variants[] = {
                // same visuals currently in Tier 2/3 for grassland flowers
            };
            vis = variants[seed % N]; break;
        }
        // ... other biomes
    }
    break;
}
```

Move the existing Tier 2/3 visual variants directly into these fixture cases — same glyphs, same colors, just driven by fixture data instead of floor seed.

## Settlement Integration

No changes needed. The ExteriorDecorator already clears `NaturalObstacle` and `ShoreDebris` within the settlement footprint. Add the 8 new types to the same clearing check:

```cpp
if (ft == FixtureType::NaturalObstacle || ft == FixtureType::ShoreDebris
    || ft == FixtureType::FloraFlower || ft == FixtureType::FloraHerb
    || ft == FixtureType::FloraMushroom || ft == FixtureType::FloraGrass
    || ft == FixtureType::FloraLichen || ft == FixtureType::MineralOre
    || ft == FixtureType::MineralCrystal || ft == FixtureType::ScrapComponent)
```

Or simpler: clear any passable non-interactable fixture (ground resources are all passable + non-interactable).

## File Structure

```
Modified:
  include/astra/tilemap.h                — 8 new FixtureType values
  src/tilemap.cpp                        — make_fixture() cases
  include/astra/biome_profile.h          — FloraStrategy type alias
  src/generators/biome_profiles.cpp      — assign flora_fn per biome
  src/generators/detail_map_generator_v2.cpp — call flora_fn after scatter
  src/terminal_theme.cpp                 — remove Tier 2/3 from resolve_floor,
                                           add 8 fixture cases to resolve_fixture
  src/generators/exterior_decorator.cpp  — clear new fixture types in settlements
  src/game_rendering.cpp                 — fixture names/descriptions
  src/game_interaction.cpp               — bump messages
  src/map_editor.cpp                     — fixture names

Create:
  src/generators/flora_strategies.cpp    — all per-biome flora strategy functions
```
