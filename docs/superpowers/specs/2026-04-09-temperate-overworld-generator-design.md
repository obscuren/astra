# Temperate Planet Overworld Generator — Design Spec

**Date:** 2026-04-09
**Status:** Draft

## Context

The current overworld generator uses per-tile noise thresholds to assign biomes, producing salt-and-pepper terrain where biomes mix at the tile level. Temperate planets (Earth-like) should have large coherent biome regions — a visible forest patch, a distinct desert area, a mountain range — like the Caves of Qud overworld.

We built the `OverworldGeneratorBase` class hierarchy to support body-type-specific generators. This is the first dedicated subclass.

## Goal

A `TemperateOverworldGenerator` that produces large coherent biome regions using Voronoi tessellation, with rivers that flow from mountains along region boundaries.

## Design Decisions

- **Voronoi region-based classification** — 15-25 seed points, each assigned a biome from local elevation/moisture, tiles belong to nearest seed
- **Combined A+B river generation** — rivers start at mountains following elevation, bend toward Voronoi boundaries as they descend, form lakes in basins
- **Factory receives MapProperties** — so it can dispatch the right generator subclass
- **Reuse Default POI logic** — temperate generator inherits POI placement from a shared utility

---

## 1. Voronoi Terrain Classification

### Seed Placement

1. Generate 15-25 random seed points across the map
2. Each seed reads elevation and moisture noise at its position
3. Assign biome to each seed using these thresholds:

| Condition | Biome |
|-----------|-------|
| elevation > 0.72 | Mountains |
| elevation < 0.25 | Lake |
| elevation < 0.35, moisture > 0.5 | Swamp |
| moisture > 0.6 | Forest |
| moisture > 0.3 | Plains |
| else | Desert |

4. For each map tile, find the nearest seed (Euclidean distance) and assign that seed's biome

### Special Cases

- **Lake seeds** produce `OW_Lake` tiles directly — the entire Voronoi cell becomes water
- **Mountain seeds** produce `OW_Mountains` — the entire cell becomes mountain range
- Tiles with extreme elevation (> 0.72) become Mountains regardless of their Voronoi assignment (elevation override)
- Tiles with very low elevation (< 0.25) become Lake regardless of Voronoi assignment

### Edge Smoothing

At Voronoi cell boundaries (where the distance difference between the two nearest seeds is < 2 tiles), use local elevation/moisture noise to probabilistically pick between the two biomes. This creates irregular natural-looking borders instead of sharp geometric lines.

---

## 2. River Generation (Combined A+B)

### Source Selection

Same as current: scan for tiles with elevation 0.55-0.72 that are adjacent to mountain tiles. Shuffle and pick 3-6 sources (scaled by mountain coverage).

### Path Algorithm

Each step picks the next tile by blending two forces:

```
score(neighbor) = downhill_weight * (current_elev - neighbor_elev)
               + boundary_weight * (1.0 / distance_to_voronoi_boundary)
```

- `downhill_weight` = `elevation * 2.0` (strong at high elevation, weakens as river descends)
- `boundary_weight` = `(1.0 - elevation) * 1.5` (weak at high elevation, strong in lowlands)

Pick the neighbor with the highest score. Max 100 steps per river.

### Lake Formation

When a river reaches a basin (no neighbor scores positive — no downhill and no boundary pull), flood fill a small area (3-8 tiles) as `OW_Lake`. The flood fills the lowest-elevation connected tiles from the current position.

### River Count

`3 + mountain_seed_count / 3`, clamped to 3-6. More mountains = more rivers.

---

## 3. Factory Changes

### Updated Signature

```cpp
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props);
```

### Dispatch Logic

```cpp
if (props.body_type == BodyType::Terrestrial &&
    props.body_temperature == Temperature::Temperate &&
    (props.body_atmosphere == Atmosphere::Standard ||
     props.body_atmosphere == Atmosphere::Dense)) {
    return std::make_unique<TemperateOverworldGenerator>();
}
return std::make_unique<DefaultOverworldGenerator>();
```

### Call Site Update

`create_generator(MapType)` gains access to props. The overworld case calls `make_overworld_generator(props)`.

---

## 4. POI Placement

The temperate generator reuses the Default generator's POI placement logic. The `place_pois` method from `DefaultOverworldGenerator` is extracted into a free function that both generators can call:

```cpp
void place_default_pois(TileMap* map, const MapProperties* props, std::mt19937& rng);
```

The temperate generator's `place_pois()` simply calls this function.

---

## 5. Files

| File | Action | Details |
|------|--------|---------|
| `src/generators/temperate_overworld_generator.cpp` | Create | Voronoi classification + A+B rivers |
| `src/generators/default_overworld_generator.cpp` | Modify | Extract POI logic to shared function |
| `include/astra/overworld_generator.h` | Modify | Update factory signature, declare shared POI function |
| `src/generators/overworld_generator_base.cpp` | Modify | Update factory call if needed |
| `src/map_generator.cpp` | Modify | Pass props to overworld factory |
| `CMakeLists.txt` | Modify | Add temperate generator source |

---

## 6. What This Does NOT Change

- Other body types (rocky, asteroid, etc.) — still use DefaultOverworldGenerator
- Non-temperate terrestrial bodies — still use DefaultOverworldGenerator
- Detail map generation — unchanged, driven by overworld tile type
- POI types and stamps — unchanged, same placement logic
- Lore overlays — shared, applied by base class
- Connectivity and landing pad — shared, applied by base class

---

## 7. Out of Scope

- Climate bands / latitude-based biome variation
- Continent/ocean structure (large water bodies with coastlines)
- New overworld tile types
- New POI types for temperate worlds
- Custom detail map biome profiles for temperate planets
