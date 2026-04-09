# Mountains Biome — Design Spec

**Date:** 2026-04-09
**Status:** Draft

## Context

Mountains (`OW_Mountains`) are currently impassable on the overworld — the player cannot enter them. They map to `Biome::Rocky`, which produces scattered rock formations, not mountain terrain. Mountains need their own biome with dramatic ridgelines, cliff walls, and narrow passes.

## Goal

Add a dedicated Mountains biome with three neighbor-driven structure variants (passes, gradient, plateau), make mountain tiles traversable on the overworld, and integrate with the existing edge bleed system.

## Design Decisions

- **Three structure variants in one strategy function** — neighbor count determines which variant generates
- **Sparse and barren with mineral rewards** — rocks and ore deposits, no flora
- **Normal movement speed** — walls and narrow passes are the obstacle, not tick cost
- **No new overworld tile types** — `OW_Mountains` stays as-is, just becomes passable

---

## 1. Biome Enum

Add `Mountains` to the `Biome` enum in `tilemap.h`, after `Marsh`:

```cpp
Marsh,
Mountains,
// Alien terrain...
```

## 2. Passability

Remove `OW_Mountains` from the impassable terrain checks in `game_world.cpp`:

- `enter_detail_map()`: remove from the early-return comment/case that blocks entry
- `transition_detail_edge()`: remove from the `if (dest_tile == Tile::OW_Mountains || ...)` block

Add an entry message for mountains: `"You ascend into the mountains."`

## 3. Biome Mapping

In `detail_biome_for_terrain()` (`map_properties.h`):

```cpp
case Tile::OW_Mountains: return Biome::Mountains;
```

(`OW_Crater` stays mapped to `Biome::Rocky`.)

## 4. BiomeProfile

```
Mountains:
  name:                "Mountains"
  elevation_fn:        elevation_ridgeline
  elevation_frequency: 0.04
  elevation_octaves:   5
  wall_threshold:      0.72
  moisture_fn:         moisture_none
  moisture_frequency:  0.04
  water_threshold:     0.7
  flood_level:         0.4
  structure_fn:        structure_mountains
  structure_intensity: 0.6
  scatter:             [{NaturalObstacle, 0.02, false},
                        {MineralOre, 0.01, false},
                        {MineralCrystal, 0.005, false}]
  flora_fn:            nullptr
```

## 5. Structure Strategy: structure_mountains

A single strategy function that reads the neighbor overworld tiles from the generation context to count adjacent mountain tiles, then branches into one of three variants.

### Neighbor Counting

The strategy function receives the `BiomeProfile` which doesn't carry neighbor data. Instead, the neighbor count is computed from `MapProperties` before calling the strategy. A new field on `BiomeProfile` carries this context:

```cpp
int mountain_neighbor_count = 0;  // set by generator before calling structure_fn
```

The v2 generator sets this field by counting how many of `detail_neighbor_n/s/e/w` are `OW_Mountains` before invoking the structure strategy.

### Variant Selection

| Mountain neighbors | Variant | Wall coverage | Feel |
|---|---|---|---|
| 0-1 | **Passes** | 60-70% | Narrow winding corridors between cliff walls. Isolated peak or range edge. |
| 2 | **Gradient** | 40-50% | Heavy walls near non-mountain edges, opening up toward center. Range boundary. |
| 3-4 | **Plateau** | 20-30% | Open highland with ridgeline bands cutting across. Deep interior of range. |

### Passes Variant (0-1 mountain neighbors)

Algorithm:
1. Start with all cells as `StructureMask::Wall`
2. Use high-frequency Perlin noise to carve winding corridors (threshold ~0.35)
3. Widen corridors slightly (1-2 cells) with a second pass
4. Result: maze-like passages through dense cliff walls

### Gradient Variant (2 mountain neighbors)

Algorithm:
1. Start with all cells as `StructureMask::None`
2. For each edge that borders a non-mountain tile, place a wall band 30-40 tiles deep with noise-eroded edges
3. Center region stays open
4. Result: heavy walls at biome transitions, alpine meadow in the middle

The non-mountain edges are determined from `detail_neighbor_n/s/e/w` — any neighbor that is NOT `OW_Mountains` gets a wall band.

### Plateau Variant (3-4 mountain neighbors)

Algorithm:
1. Start with all cells as `StructureMask::None`
2. Generate 3-5 diagonal ridgeline bands using ridge noise (sharp creases)
3. Bands are 2-4 cells wide, cutting across the map at varying angles
4. Result: open terrain with dramatic ridge walls, easy to navigate but visually striking

## 6. Biome Colors

```cpp
case Biome::Mountains:
    return {
        Color{140, 140, 140},  // wall: gray stone
        Color{100, 90, 80},    // floor: dark rocky ground
        Color{100, 90, 80},    // water: n/a (moisture_none)
        Color{70, 65, 60},     // remembered: dim stone
    };
```

## 7. Edge Bleed Integration

Mountains use `elevation_ridgeline` which produces dramatic elevation peaks. The existing edge bleed system (both cached and synthetic strips) naturally propagates mountain walls into neighboring tiles. No additional work needed — the ridgeline terrain is wall-heavy enough to create visible bleed through the standard two-phase blending.

## 8. Files Touched

| File | Action | Change |
|------|--------|--------|
| `include/astra/tilemap.h` | Modify | Add `Mountains` to `Biome` enum |
| `include/astra/map_properties.h` | Modify | `detail_biome_for_terrain`: `OW_Mountains` → `Mountains` |
| `include/astra/biome_profile.h` | Modify | Declare `structure_mountains`, add `mountain_neighbor_count` field |
| `src/game_world.cpp` | Modify | Remove `OW_Mountains` from impassable checks, add entry message |
| `src/generators/biome_profiles.cpp` | Modify | Add mountains profile, add to switch |
| `src/generators/structure_strategies.cpp` | Modify | Implement `structure_mountains` (3 variants) |
| `src/generators/detail_map_generator_v2.cpp` | Modify | Set `mountain_neighbor_count` before calling structure strategy |
| `src/tilemap.cpp` | Modify | Add `Biome::Mountains` to `biome_colors()` |

## 9. Out of Scope

- Movement speed penalties for mountains (future overworld mechanic)
- Mountain-specific NPCs or encounters
- Snow/ice at high elevations (future weather system)
- Cave entrances within mountain terrain
- New overworld tile types (e.g., foothills, peaks)
