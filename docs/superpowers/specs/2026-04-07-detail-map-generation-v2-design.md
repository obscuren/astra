# Detail Map Generation v2 — Design Spec

**Date:** 2026-04-07
**Status:** Draft
**Supersedes:** 2026-04-06-detail-map-overhaul-design.md

## Context

The current detail map generator (`detail_map_generator.cpp`, 2273 lines) produces terrain by thresholding fBm noise into walls and floors. This creates claustrophobic, maze-like maps where biomes differ only in wall density. Mountains, forests, and deserts all use the same algorithm with different threshold values, resulting in maps that feel same-y and have too many obstructions.

We're replacing this with a layered channel architecture inspired by Caves of Qud's terrain approach: **floor is the default**, walls are rare and intentional, and biome identity comes from structural features and scatter density. This is a clean-slate rewrite — no code reuse from the existing generator.

## Design Decisions

- **Open-first terrain** — floor is the common case (85-95% of tiles). Walls are intentional terrain features, not noise artifacts.
- **Layered channel architecture** — elevation, moisture, and structure produce separate data channels composited into the final TileMap. Scatter, connectivity, and POI paint directly onto the composed result.
- **BiomeProfile + strategy functions** — each biome is configured by a struct of tunable parameters plus function pointers for fundamentally different algorithms. Strategies are reusable across biomes.
- **Unified 360×150 map** — one overworld tile generates one large map. The 3×3 zone grid is a viewport/gameplay concept, not a generation boundary. No more edge-seed stitching between zones.
- **Neighbor bleed** — at overworld tile boundaries, biome parameters lerp toward the neighbor's profile. Deterministic seeds ensure neighboring maps match at edges.
- **Cached maps** — generated detail maps are cached. Neighbors read cached edge data for seamless transitions.
- **Incremental implementation** — each layer is built and visually verified across all biomes before the next layer begins.

---

## 1. Architecture

### Generation Pipeline

```
BiomeProfile (parameters + strategy functions)
    │
    ▼
┌─────────────────────────────────────────┐
│ Phase 1 — Channel Generation            │
│                                         │
│  Layer 1: Elevation   → float[360×150]  │
│  Layer 2: Moisture    → float[360×150]  │
│  Layer 3: Structure   → enum[360×150]   │
│                                         │
│         ▼ Compositor ▼                  │
│  Merge channels → TileMap               │
└─────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────┐
│ Phase 2 — Direct Painting               │
│                                         │
│  Layer 4: Scatter      → fixtures       │
│  Layer 5: Connectivity → path carving   │
│  Layer 6: POI          → structures     │
└─────────────────────────────────────────┘
```

### Compositor Rules

For each cell (x, y), in priority order:

1. If `structure[x][y]` != None → use structure's tile type (Wall/Floor/Water)
2. If `moisture[x][y]` > water_threshold AND `elevation[x][y]` < flood_level → **Water**
3. If `elevation[x][y]` > wall_threshold → **Wall** (rare, only extreme peaks)
4. Otherwise → **Floor** (the common case)

### Map Sizing

- One overworld tile = one TileMap of 360×150
- 3×3 zone grid (each 120×50) is a viewport concept — player transitions between zones zelda-style
- Zone grid is used for POI placement logic (e.g., center zone gets primary POI) but not for generation boundaries

---

## 2. BiomeProfile System

### Structure

```cpp
struct BiomeProfile {
    // Layer 1: Elevation
    ElevationStrategy elevation_fn;
    float elevation_frequency = 0.03f;
    int   elevation_octaves  = 4;
    float wall_threshold     = 0.85f;

    // Layer 2: Moisture
    MoistureStrategy moisture_fn;
    float moisture_frequency = 0.04f;
    float water_threshold    = 0.7f;
    float flood_level        = 0.4f;

    // Layer 3: Structure
    StructureStrategy structure_fn;
    float structure_intensity = 0.5f;

    // Layer 4: Scatter
    std::vector<ScatterEntry> scatter;
};

// Strategy function signatures
using ElevationStrategy = void(*)(float* grid, int w, int h,
                                   std::mt19937& rng,
                                   const BiomeProfile& prof);

using MoistureStrategy = void(*)(float* grid, int w, int h,
                                  std::mt19937& rng,
                                  const float* elevation,
                                  const BiomeProfile& prof);

using StructureStrategy = void(*)(StructureMask* grid, int w, int h,
                                   std::mt19937& rng,
                                   const float* elevation,
                                   const float* moisture,
                                   const BiomeProfile& prof);
```

### Elevation Strategies

| Strategy | Algorithm | Used by |
|----------|-----------|---------|
| `elevation_gentle` | Low-amplitude fBm. Smooth rolling terrain. Walls only at rare peaks. | Grassland, Sandy, Forest, Fungal |
| `elevation_rugged` | Higher amplitude, sharper peaks. More wall outcrops but still mostly open. | Rocky, Volcanic |
| `elevation_flat` | Near-zero amplitude. Almost entirely floor. | Aquatic, Swamp, Ice |
| `elevation_ridgeline` | Ridge noise — sharp creases forming natural cliff lines. | Mountains, Crystal |

### Moisture Strategies

| Strategy | Algorithm | Used by |
|----------|-----------|---------|
| `moisture_none` | No water generation. | Sandy, Rocky, Mountains |
| `moisture_pools` | Organic blob-shaped pools in low elevation areas. | Swamp, Forest, Fungal, Ice, Crystal (rare) |
| `moisture_river` | Sinuous band of water following low elevation. | Grassland (rare), Jungle |
| `moisture_coastline` | Large water body with irregular shoreline. | Aquatic |
| `moisture_channels` | Branching channels carved through terrain. For lava/acid. | Volcanic |

### Structure Strategies

| Strategy | Algorithm | Used by |
|----------|-----------|---------|
| `structure_none` | No structural features. Terrain from elevation + moisture only. | Grassland, Sandy |
| `structure_cliffs` | Diagonal/horizontal cliff bands with noise-eroded edges. | Mountains, Rocky |
| `structure_canopy` | Dense tree-wall clusters with winding clearings between. | Forest, Jungle (dense) |
| `structure_islands` | Raises floor platforms in water areas. Archipelago/stepping stones. | Swamp, Aquatic |
| `structure_formations` | Angular/geometric wall clusters. Crystal outcrops, ice spires. | Crystal, Ice |
| `structure_craters` | Radial crater rims with depressed interiors. | Volcanic, Rocky |
| `structure_organic` | Blob-shaped wall clusters with tendrils. Mushroom caps, fungal growths. | Fungal |

### Natural Biome Profiles

| Biome | Elevation | Moisture | Structure | Feel |
|-------|-----------|----------|-----------|------|
| Grassland | gentle | river (rare) | none | Wide open plains, occasional stream |
| Forest | gentle | pools | canopy | Winding paths through dense trees, hidden pools |
| Jungle | gentle | pools + river | canopy (dense) | Thick vegetation, more water, tighter paths |
| Sandy | gentle | none | none | Open dunes, sparse features, scatter-defined |
| Rocky | rugged | none | cliffs | Broken outcrops, cliff faces, rubble |
| Mountains | ridgeline | none | cliffs | Ridge walls, narrow passes, cliff bands |
| Volcanic | rugged | channels | craters | Lava channels through rock, crater rims |
| Aquatic | flat | coastline | islands | Island archipelago in open water |
| Swamp | flat | pools | islands | Soggy ground, water pools, raised platforms |
| Ice | flat | pools | formations | Ice spires, frozen pools, open tundra |
| Fungal | gentle | pools | organic | Mushroom formations, bio-pools, spore clusters |
| Crystal | ridgeline | pools (rare) | formations | Angular crystal outcrops, prismatic pools |

### Alien Biome Profiles

| Biome | Elevation | Moisture | Structure | Feel |
|-------|-----------|----------|-----------|------|
| AlienCrystalline | ridgeline | pools | formations (angular) | Crystal ridges with glowing pools |
| AlienOrganic | gentle | pools | organic (dense) | Fleshy walls, pulsing pools, tendril corridors |
| AlienGeometric | flat | none | formations (grid-aligned) | Perfect geometric walls, grid floors, open plazas |
| AlienVoid | flat | pools (dark) | cliffs (fissures) | Cracks in dark stone, void pools, eerie emptiness |
| AlienLight | flat | pools (luminous) | none | Open terrain with glowing columns (via scatter) |

### Scar Biome Profiles

| Biome | Elevation | Moisture | Structure | Feel |
|-------|-----------|----------|-----------|------|
| ScarredScorched | rugged | none | none | Charred debris across blackened ground (scatter-heavy) |
| ScarredGlassed | flat | none | formations (fused) | Massive fused-glass walls, impassable zones |

---

## 3. Neighbor Bleed

At overworld tile boundaries, the generation lerps between the current biome's profile parameters and the neighboring tile's biome profile.

### How it works

1. For each cell within the bleed margin (20 tiles from the edge), compute a blend weight `t` from 0.0 (interior) to 1.0 (edge).
2. Lerp float parameters: `effective_param = lerp(current.param, neighbor.param, t)`
3. Strategy functions: use the current biome's strategy, but with blended parameters. This means a forest next to a desert keeps the forest's structure algorithm but with reduced intensity near the edge.
4. Deterministic seed: the noise field for the bleed margin uses a seed derived from both overworld tile coordinates, so the neighbor's bleed region produces matching values.

### Cached neighbor edges

- When a detail map is generated, its edge strips (outermost 20 columns/rows) are stored in the cache.
- When a neighboring tile generates, it reads the cached edge strip and uses it as the ground truth for its bleed margin, ensuring pixel-perfect matching.
- If no neighbor is cached yet, both tiles use the deterministic seed approach — they'll match regardless of generation order.

---

## 4. POI System

### POIContext

```cpp
struct POIContext {
    OverworldTile poi_type;     // OW_Settlement, OW_Ruins, OW_CrashedShip, etc.
    Architecture architecture;  // civ architecture style
    float alien_strength;       // lore alien influence
    float scar_intensity;       // lore scar influence
    bool is_hidden;             // discovered on entry vs visible on overworld
    int poi_x, poi_y;           // stamp position on the 360×150 map
    int poi_w, poi_h;           // stamp dimensions
};
```

### Visible vs Hidden POIs

- **Visible POIs** — marked on the overworld map with an icon. Player knows they're entering a settlement/ruins/etc.
- **Hidden POIs** — overworld tile looks like normal terrain. The tile has a hidden POI flag set at overworld generation time (deterministic, seed-based). When the player first enters the tile, they discover it: "You've discovered ancient ruins of the Crystalline civilization."
- Both types generate through the same POI layer with the same POIContext. The only difference is the discovery trigger and overworld display.

### POI Placement

POIs stamp onto the unified 360×150 map at a position and size. The zone grid is irrelevant to POI placement — a ruin can straddle zone boundaries naturally.

### POI Terrain Integration

POIs are not isolated stamps — they connect to surrounding terrain. Ruins have pipes and corridors reaching outward, settlements have paths leading into the wilderness, outposts have cleared perimeters. The POI layer reads the composed terrain and generates connective elements that bridge the structure into its environment.

### Multi-Overworld-Tile POIs

Ruins and megastructures can span multiple overworld tiles:

- At overworld generation, multi-tile POIs are placed as a cluster of overworld tiles sharing a POI group ID.
- Each tile in the cluster knows its position within the POI (e.g., "north wing of the ruins").
- When a tile generates, it stamps its section of the POI. Edge data is cached.
- Neighboring tiles in the same POI group read cached edges to connect structures seamlessly.
- Only ruins and megastructures support multi-tile. Other POI types fit within a single tile.

---

## 5. File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/biome_profile.h` | Create | BiomeProfile struct, strategy type aliases, `biome_profile()` lookup |
| `src/generators/biome_profiles.cpp` | Create | All biome profile definitions (natural, alien, scar) |
| `src/generators/elevation_strategies.cpp` | Create | 4 elevation strategy implementations |
| `src/generators/moisture_strategies.cpp` | Create | 5 moisture strategy implementations |
| `src/generators/structure_strategies.cpp` | Create | 7 structure strategy implementations |
| `src/generators/terrain_compositor.cpp` | Create | Channel → TileMap compositor |
| `src/generators/detail_map_generator_v2.cpp` | Create | New generator: orchestrates layers 1-6 |
| `include/astra/poi_context.h` | Create | POIContext struct |
| `src/generators/poi_stamps.cpp` | Create | POI stamp handlers (settlement, ruins, outpost, etc.) |
| `src/generators/detail_map_generator.cpp` | Delete | Old monolithic generator (after v2 is complete) |

---

## 6. Dev Testing Tool

A dev-mode command for visual inspection of biome generation at each layer stage:

```
biome_test <biome> [layer]
```

- `biome_test grassland 1` — elevation channel only
- `biome_test grassland 2` — elevation + moisture
- `biome_test grassland 3` — elevation + moisture + structure (composited)
- `biome_test grassland 4` — full with scatter
- `biome_test grassland` — full pipeline (all layers)
- `biome_test all 1` — cycle through every biome at layer 1

Generates a 360×150 map and displays it in the game viewport. Used during development to tune each layer before building the next.

---

## 7. Implementation Phases

Each phase is a commit checkpoint. Build layer → visually test all biomes → fix → commit → next.

### Phase 1: Foundation + Elevation Layer
- Create `BiomeProfile` struct + all 12 natural biome profiles (strategies assigned, params tuned later)
- Create channel data structures (float grid, enum grid for structure mask)
- Implement 4 elevation strategies: `elevation_gentle`, `elevation_rugged`, `elevation_flat`, `elevation_ridgeline`
- Implement compositor (elevation-only mode)
- Implement neighbor bleed (lerp biome params at overworld tile edges)
- Build dev test tool (`biome_test`)
- Wire into new generator class
- **Verify:** each biome produces distinct, mostly-open elevation terrain

### Phase 2: Moisture Layer
- Implement 5 moisture strategies: `moisture_none`, `moisture_pools`, `moisture_river`, `moisture_coastline`, `moisture_channels`
- Update compositor to handle water tiles
- Moisture reads elevation channel for flood level placement
- **Verify:** water shapes are organic and biome-appropriate, no random noise speckle

### Phase 3: Structure Layer
- Implement 7 structure strategies: `structure_none`, `structure_cliffs`, `structure_canopy`, `structure_islands`, `structure_formations`, `structure_craters`, `structure_organic`
- Update compositor to handle structure overrides
- Structure reads elevation + moisture channels
- **Verify:** biomes feel distinct — mountains have cliffs, forests have canopy, swamps have islands

### Phase 4: Scatter Layer
- Define scatter palettes per biome (fixture types, densities, stamp patterns)
- Place NaturalObstacle fixtures on floor tiles
- Scatter density and type is the final biome identity layer
- **Verify:** maps feel alive — dense vegetation in forests, sparse cacti in desert, mushrooms in fungal

### Phase 5: Connectivity Layer
- Flood-fill connectivity check across the 360×150 map
- Minimal 1-wide path carving where isolated pockets exist
- Should rarely trigger with open-first design
- **Verify:** all biomes are fully connected, no isolated pockets, no heavy-handed corridor carving

### Phase 6: POI Layer
- Create `POIContext` struct
- Port/rewrite POI stamp handlers (settlement, ruins, outpost, crashed ship, beacon, megastructure, landing, cave entrance)
- Hidden POI discovery system (overworld flag → detail map reveal → player message)
- Multi-overworld-tile POI support for ruins and megastructures
- **Verify:** POIs generate correctly across all biomes, civ architecture theming applied

### Phase 7: Map Caching + Integration
- Cache generated 360×150 maps per overworld tile
- Neighbor map edge data cached for seamless transitions
- Wire viewport navigation (zelda-style zone transitions within cached map)
- Replace old DetailMapGenerator with v2
- **Verify:** seamless zone transitions, neighbor bleed works, cache hit/miss correct

### Phase 8: Alien Biome Profiles
- Define 5 alien biome profiles using existing strategies + new params
- Add alien-specific strategy variants where needed (e.g., geometric grid-aligned formations)
- **Verify:** alien biomes feel distinct from natural biomes and from each other

### Phase 9: Scar Biome Profiles + Colossal Landmarks
- Define 2 scar biome profiles
- Create LandmarkGenerator for beacon and megastructure (multi-zone, architecture-themed)
- **Verify:** scar terrain looks devastated, landmarks span zones correctly

### Phase 10: World Persistence
- Serialize/deserialize cached detail maps
- Persist POI discovery state (hidden POIs stay discovered)
- Persist any detail map modifications (player actions, combat damage)
- **Verify:** save/load round-trips correctly, discovered POIs persist

---

## 8. Opaque Fixtures (Vision-Blocking Scatter)

Some scatter fixtures represent objects too large to see past (tall trees, dense vegetation, large mushroom caps, crystal spires) but that the player can still walk through. These are **passable but vision-blocking** — they affect FOV calculation but not movement.

### Implementation

- `NaturalObstacle` fixtures gain a `blocks_vision` flag
- The FOV system treats `blocks_vision` fixtures the same as walls for line-of-sight
- The movement system ignores the flag — player walks through freely
- Biome scatter palettes specify which fixtures block vision:
  - **Forest/Jungle:** tall trees, dense canopy clusters → blocks vision
  - **Fungal:** large mushroom caps → blocks vision
  - **Crystal/Ice:** tall crystal/ice spires → blocks vision
  - **Grassland:** short grass, small rocks → does NOT block vision
  - **Sandy:** cacti, small dunes → does NOT block vision

This creates tactical terrain variety — forests and jungles limit visibility while remaining navigable, open plains and deserts let you see far.

---

## 9. Out of Scope

- Mechanical elevation system (visual only for now)
- Movement cost changes for water (keep existing health cost)
- New overworld tile types or overworld generation changes
- SDL renderer updates
- Dungeon/underworld generation (separate system)
