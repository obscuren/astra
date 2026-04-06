# Detail Map Overhaul — Design Spec

**Date:** 2026-04-06
**Status:** Draft
**Depends on:** Terrain shaping from lore (merged)

## Overview

Full overhaul of the detail map generator to produce terrain that feels like a zoomed-in version of the overworld tile. Each biome has distinct movement feel and terrain character through a composable terrain feature system. Alien, scar, and colossal landmark terrain are built on the same foundation.

### Design Decisions

- **Representational terrain** — detail maps represent what the overworld tile looks like at ground level (mountains have ridges, forests have canopy)
- **Distinct movement per biome** — mountains force narrow passes, forests have winding paths, swamps have island hopping
- **Visual elevation only** — tile appearance suggests height (cliffs, ledges) without mechanical elevation system
- **Water passable with health cost** — existing behavior preserved; water shapes terrain through risk, not hard barriers
- **Composable terrain features** — biomes are defined as compositions of reusable terrain feature algorithms
- **Colossal landmarks** — beacon/megastructure span 3-5 zones with interconnecting structures
- **Three sub-projects** — base natural terrain, alien terrain, scar+landmarks (this spec covers all three but implementation is phased)

---

## 1. Terrain Feature System

### Feature Interface

A terrain feature is a function that takes the in-progress TileMap, an RNG, a noise field, and a config struct, then carves/places geometry.

```cpp
struct FeatureConfig {
    float intensity;     // 0.0-1.0 — how dominant this feature is
    Tile wall_tile;      // what tile to use for walls
    Tile floor_tile;     // what tile for carved space
    Tile water_tile;     // what tile for water features
};

// All features share this signature
using TerrainFeatureFn = void(*)(TileMap* map, std::mt19937& rng,
                                  const float* noise, int w, int h,
                                  const FeatureConfig& cfg);
```

### Feature Library

| Feature | Algorithm | Used by |
|---------|-----------|---------|
| `RidgeWalk` | Random walk + perpendicular wall extrusion, creates ridges with narrow passes | Mountains, Rocky, AlienCrystalline |
| `CliffBand` | Horizontal/vertical wall bands with noise edges, creates cliff faces | Mountains, Rocky |
| `DenseCanopy` | High-density noise walls with winding organic paths carved through | Forest, Jungle, AlienOrganic |
| `Clearing` | Circular/irregular open areas within dense terrain | Forest, Jungle, AlienGeometric, AlienLight |
| `IslandArchipelago` | Flood-fill water with noise-shaped land masses | Swamp, Aquatic, AlienOrganic |
| `DuneField` | Rolling sine-wave wall bands with gaps between | Desert, Sandy, ScarredScorched |
| `CraterBowl` | Radial distance-based depression with rim walls | Crater, Rocky, ScarredGlassed |
| `PoolCluster` | Scattered water pools with noise edges | Swamp, Ice, Fungal, all alien |
| `LavaChannels` | Branching water (lava) channels cutting through rock | Volcanic |
| `CrystalFormation` | Angular wall clusters with geometric patterns | Crystal, AlienCrystalline |
| `GeometricRuins` | Grid-aligned wall segments, perfect rectangles, etched floor | AlienGeometric |
| `OrganicGrowth` | Blob-shaped wall clusters with tendrils, pulsing pools | AlienOrganic |
| `VoidFissures` | Long thin wall cracks with dark pools at intersections | AlienVoid |
| `LightPillars` | Scattered column formations with open space between | AlienLight |
| `DebrisField` | Scattered wall chunks with irregular shapes | ScarredScorched, battle sites |
| `GlassedGround` | Dense wall field (fused terrain), impassable zones | ScarredGlassed |
| `FungalSprawl` | Organic noise walls with mushroom-cap shapes | Fungal, Swamp |
| `IceFormations` | Jagged wall clusters + ice tile pools | Ice |
| `NarrowPass` | Guaranteed walkable corridor through dense terrain | All (connectivity guarantee) |

Features are reusable across biomes with different configs (tile types, intensity). A `RidgeWalk` in a mountain biome uses grey wall tiles; in AlienCrystalline it uses cyan crystal walls.

---

## 2. Biome Definitions

Each biome maps to a composition of features plus base parameters.

```cpp
struct FeatureSpec {
    TerrainFeatureFn fn;
    FeatureConfig config;
};

struct BiomeDefinition {
    float base_wall_density;    // background noise wall level
    float base_water_density;   // background noise water level
    std::vector<FeatureSpec> features;  // ordered: structural first, detail second
};

BiomeDefinition biome_definition(Biome b);
```

### Natural Biomes

| Biome | Wall | Water | Features | Feel |
|-------|------|-------|----------|------|
| Mountains | 0.4 | 0.0 | RidgeWalk, CliffBand, NarrowPass | Tight passes between towering ridges |
| Rocky | 0.3 | 0.0 | CliffBand, CraterBowl(small), NarrowPass | Broken rocky terrain with outcrops |
| Forest | 0.15 | 0.05 | DenseCanopy, Clearing, PoolCluster(small) | Winding paths through dense trees |
| Jungle | 0.25 | 0.1 | DenseCanopy(high), Clearing(small), PoolCluster | Thick vegetation with hidden pools |
| Grassland | 0.05 | 0.02 | DuneField(low), Clearing(large) | Mostly open with gentle rolling terrain |
| Sandy | 0.08 | 0.0 | DuneField, CraterBowl(rare) | Sand dunes with occasional outcrops |
| Ice | 0.1 | 0.15 | IceFormations, PoolCluster(ice) | Jagged ice walls, frozen pools |
| Volcanic | 0.2 | 0.3 | LavaChannels, CliffBand | Rock walls with lava channels |
| Aquatic | 0.05 | 0.5 | IslandArchipelago | Islands surrounded by water |
| Swamp | 0.1 | 0.4 | IslandArchipelago, PoolCluster, FungalSprawl(low) | Soggy islands with shallow crossings |
| Fungal | 0.2 | 0.1 | FungalSprawl, PoolCluster, Clearing | Mushroom formations with bio-pools |
| Crystal | 0.15 | 0.05 | CrystalFormation, Clearing | Angular crystal outcrops |

### Alien Biomes

| Biome | Features | Feel |
|-------|----------|------|
| AlienCrystalline | CrystalFormation(high), RidgeWalk(angular), PoolCluster(prismatic) | Crystal ridges with glowing pools |
| AlienOrganic | OrganicGrowth, IslandArchipelago(bio-pools), DenseCanopy(tendril) | Fleshy walls, pulsing pools, tendril corridors |
| AlienGeometric | GeometricRuins, Clearing(grid-aligned) | Perfect geometric walls, grid floors, open plazas |
| AlienVoid | VoidFissures, PoolCluster(dark), Clearing(sparse) | Cracks in dark stone, void pools, eerie emptiness |
| AlienLight | LightPillars, Clearing(large), PoolCluster(luminous) | Open terrain with glowing columns and light pools |

### Scar Biomes

| Biome | Features | Feel |
|-------|----------|------|
| ScarredScorched | DebrisField, CraterBowl(shallow), DuneField(ash) | Charred debris across blackened dunes |
| ScarredGlassed | CraterBowl(deep), GlassedGround, DebrisField(sparse) | Massive crater with fused-glass walls |

---

## 3. Generation Flow

Replaces the current noise-threshold approach with a phased pipeline.

**Phase 1: Base noise field**
Generate fBm elevation/moisture noise (same as current). Store as raw float arrays. Don't classify into tiles yet — features read these values.

**Phase 2: Feature composition**
Look up `BiomeDefinition` for the tile's biome. Run each feature in order:
1. Structural features first (RidgeWalk, DenseCanopy, IslandArchipelago) — define major terrain
2. Detail features second (PoolCluster, Clearing) — carve into structural output
3. `NarrowPass` last — guarantee a walkable corridor

Features write directly to the TileMap. Each feature respects existing tiles where appropriate (a PoolCluster won't overwrite a wall placed by RidgeWalk unless configured to).

**Phase 3: Edge blending**
Shared edge seeding (already implemented). Smooths zone boundaries. Runs after features so edge terrain matches adjacent zones.

**Phase 4: Connectivity guarantee**
`ensure_connectivity()` as safety net. With good feature design, this should rarely need to carve.

**Phase 5: Scatter + POI**
`scatter_biome_features()` places fixtures. Then POI handlers run if applicable. Landmark POIs delegate to `LandmarkGenerator`.

---

## 4. Colossal Landmarks

Beacon and megastructure span 3-5 zones of the 3x3 detail grid. A `LandmarkGenerator` receives zone position and generates the appropriate section.

### Zone Layout

```
Beacon:                         Megastructure:
  [approach] [corridor] [approach]    [exterior] [ wing  ] [exterior]
  [corridor] [  spire ] [corridor]    [  wing  ] [ core  ] [  wing  ]
  [approach] [corridor] [approach]    [exterior] [ wing  ] [exterior]
```

### Zone Types

**Center (1,1):** Main structure — beacon spire hall or megastructure reactor core. Large interior, key fixtures, portal/interaction point.

**Cardinal zones (0,1), (1,0), (1,2), (2,1):** Corridors/wings connecting to center. For beacon: approach paths with pylons. For megastructure: functional wings (storage, machinery, collapsed sections).

**Corner zones (0,0), (0,2), (2,0), (2,2):** Exterior approach terrain. Normal biome features with the structure visible at inner edges. Debris, foundation rubble, cleared ground.

### Connectivity

Each non-corner zone guarantees open corridors at edges facing adjacent structure zones:
- Center: corridors on all 4 sides
- Cardinal: corridors toward center + adjacent cardinals (2-3 exits)
- Corners: natural terrain, structure bleeds in at inner edges

### Architecture Theming

Wall tiles, floor tiles, and fixture types selected from `civ_aesthetics.h` based on the civilization's Architecture type. A Crystalline beacon has prismatic walls; a Geometric megastructure has grid-aligned walls.

### Integration

The detail map generator checks for `OW_Beacon` or `OW_Megastructure` tile type. If present, it delegates to `LandmarkGenerator` instead of normal terrain feature composition. Corner zones still use normal terrain features with structure elements bleeding in from inner edges.

---

## 5. File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/generators/terrain_features.h` | Create | Feature function declarations, BiomeDefinition, FeatureSpec, biome_definition() |
| `src/generators/terrain_features.cpp` | Create | All terrain feature implementations (~19 features) |
| `src/generators/landmark_generator.h` | Create | LandmarkGenerator class, zone-aware beacon/megastructure generation |
| `src/generators/landmark_generator.cpp` | Create | Landmark generation implementations |
| `src/generators/detail_map_generator.cpp` | Rewrite | Orchestration only: phases 1-5, delegates to features and landmarks |

### Size Estimates

- `terrain_features.cpp`: ~800-1200 lines (19 features × ~50-60 lines each)
- `landmark_generator.cpp`: ~400-600 lines (2 landmarks × 3 zone types × ~80 lines)
- `detail_map_generator.cpp`: ~500-700 lines (down from 2273, now orchestration only)

---

## 6. Implementation Phases

### Phase 1: Base Natural Terrain
- Create terrain feature system (struct, function signature, biome definitions)
- Implement 10 core features: RidgeWalk, CliffBand, DenseCanopy, Clearing, IslandArchipelago, DuneField, CraterBowl, PoolCluster, LavaChannels, NarrowPass
- Wire up all 12 natural biomes
- Restructure detail_map_generator.cpp to use new pipeline
- Preserve edge blending and POI handlers

### Phase 2: Alien Terrain
- Implement 5 alien features: CrystalFormation, GeometricRuins, OrganicGrowth, VoidFissures, LightPillars
- Plus FungalSprawl, IceFormations (also used by natural biomes)
- Wire up 5 alien biome definitions

### Phase 3: Scar Terrain + Colossal Landmarks
- Implement 2 scar features: DebrisField, GlassedGround
- Wire up 2 scar biome definitions
- Create LandmarkGenerator with zone-aware beacon and megastructure
- Architecture theming for landmarks

---

## 7. Out of Scope

- Mechanical elevation system (deferred — visual only for now)
- Movement cost for water (keep existing health cost)
- River V2 overhaul (separate task, but features like PoolCluster and LavaChannels improve water handling)
- Plague terrain (can be added as a biome definition + features later)
- New overworld tile types or overworld generation changes
- SDL renderer updates
