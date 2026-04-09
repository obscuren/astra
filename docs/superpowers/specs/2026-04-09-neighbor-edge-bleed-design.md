# Neighbor Edge Bleed — Design Spec

**Date:** 2026-04-09
**Status:** Draft
**Parent:** Detail Map Generation v2 (Phase 5: Connectivity)

## Context

The v2 detail map generator produces 360×150 maps per overworld tile. Each map is generated independently with only the neighbor's overworld tile *type* (e.g., `OW_Mountains`) passed as context. The current `apply_neighbor_bleed()` regenerates the neighbor's elevation noise from its biome profile and lerps it into a 20-tile margin — a parameter-level approximation that never reads actual generated terrain.

The result: rivers, ridges, ruins, paths, and structures all hard-stop at tile boundaries. Walking from one overworld tile to the next feels like entering a completely separate world.

## Goal

When a detail map generates next to an already-cached neighbor, the generator reads the neighbor's actual edge tiles and forces the new map's border to match. Terrain features flow naturally across overworld tile boundaries — rivers connect, ridges continue, ruins extend.

## Design Decisions

- **Two-phase blending** — 5 tiles verbatim stamp + 15 tiles weighted transition
- **Tiles + fixtures + glyph overrides + custom flags** — full visual fidelity at edges
- **Extract on read** — no separate edge strip storage; read directly from cached TileMap when needed
- **Procedural fallback** — when no cached neighbor exists, fall back to current biome-profile noise blending
- **POI wins** — POI stamps overwrite bleed zone tiles where they overlap

---

## 1. Edge Strip Data

An edge strip is a slice of a cached neighbor's TileMap, extracted on-demand when building properties for an adjacent tile.

### Cell Data

For each cell in the strip:

| Field | Type | Description |
|-------|------|-------------|
| `tile` | `Tile` | The tile enum (Floor, Wall, Water, Path, StructuralWall, IndoorFloor, etc.) |
| `fixture` | `FixtureData` | Fixture data if present; `std::nullopt` if no fixture |
| `glyph_override` | `uint8_t` | Glyph override index (0 = no override) |
| `custom_flags` | `uint8_t` | Bitwise custom flags |

### Strip Dimensions

- **Depth:** 20 cells (5 verbatim + 15 blend)
- **Length:** 360 cells for north/south edges, 150 cells for east/west edges
- **Storage:** `std::vector<EdgeStripCell>` of size `depth × length`, row-major where row 0 is the shared boundary line and row 19 is deepest into the neighbor

### Direction Mapping

When tile B is **north** of tile A:
- B needs A's **north** edge strip (A's top 20 rows, stored top-to-bottom: row 0 of A = row 0 of strip)
- B applies it to its **south** border (B's bottom 20 rows, mapped bottom-to-top: strip row 0 → B's last row)

Wait — let me be precise. The shared boundary is where the two maps touch:

| B's direction from A | A's edge to extract | A's rows/cols | B applies to |
|----------------------|--------------------|--------------:|--------------|
| B is south of A | A's south edge | A's rows `[h-20, h)` | B's rows `[0, 20)` |
| B is north of A | A's north edge | A's rows `[0, 20)` | B's rows `[h-20, h)` |
| B is east of A | A's east edge | A's cols `[w-20, w)` | B's cols `[0, 20)` |
| B is west of A | A's west edge | A's cols `[0, 20)` | B's cols `[w-20, w)` |

In all cases, strip index 0 = the boundary line (the row/col closest to B), strip index 19 = deepest into A.

---

## 2. Extraction (Read-Time)

### Where

In `Game::build_detail_props(int ow_x, int ow_y)`, after setting `detail_neighbor_n/s/e/w`.

### How

For each cardinal direction:

1. Compute the neighbor's `LocationKey`: same `{system_id, body_index, moon_index, false}` but with `(ow_x + dx, ow_y + dy)` for the overworld coordinates, depth 0.
2. Look up `world_.location_cache()` for that key.
3. If found, extract the edge strip from the cached `LocationState`'s `TileMap`:
   - Read the relevant 20 rows/cols of tiles, fixture IDs + fixture data, glyph overrides, and custom flags.
   - Store into an `EdgeStrip` struct.
4. Attach the strip to `MapProperties` (new optional field per direction).

### MapProperties Changes

```cpp
struct EdgeStripCell {
    Tile tile = Tile::Empty;
    std::optional<FixtureData> fixture;
    uint8_t glyph_override = 0;
    uint8_t custom_flags = 0;
};

struct EdgeStrip {
    int length = 0;    // 360 or 150
    int depth = 20;
    std::vector<EdgeStripCell> cells;  // [depth * length]

    const EdgeStripCell& at(int depth_idx, int along_idx) const {
        return cells[depth_idx * length + along_idx];
    }
};
```

New fields on `MapProperties`:

```cpp
std::optional<EdgeStrip> edge_strip_n;
std::optional<EdgeStrip> edge_strip_s;
std::optional<EdgeStrip> edge_strip_e;
std::optional<EdgeStrip> edge_strip_w;
```

---

## 3. Application (Two-Phase Blending)

### Pipeline Position

`apply_neighbor_bleed()` runs after `composite_terrain()` in `generate_layout()`. This means the tile's own terrain is fully generated before edge strips are applied.

```
generate_layout()
  ├── elevation_fn()
  ├── moisture_fn()
  ├── structure_fn()
  ├── composite_terrain()
  └── apply_neighbor_bleed()    ← reworked
connect_rooms()
place_features()
  ├── scatter / flora
  └── POI phase                  ← POI overwrites bleed if needed
```

### Phase 1: Verbatim Stamp (depth 0–4)

For each cell in the outermost 5 rows/cols:
- Copy tile type from strip → `map_->set(x, y, cell.tile)`
- If strip cell has a fixture, add it → `map_->add_fixture(x, y, cell.fixture.value())`
- Copy glyph override → `map_->set_glyph_override(x, y, cell.glyph_override)`
- Copy custom flags → `map_->set_custom_flags_byte(x, y, cell.custom_flags)`

This is an unconditional overwrite. Whatever the generator produced for these cells is replaced.

### Phase 2: Weighted Blend (depth 5–19)

For each cell in the blend zone:

1. Compute blend weight: `t = 1.0 - ((depth - 5) / 15.0)`, so `t` ranges from 1.0 at depth 5 to 0.0 at depth 19. Apply quadratic falloff: `t = t * t`.

2. **Walls and structural tiles** (Wall, StructuralWall, IndoorFloor, Path): if the strip cell is one of these and the generated cell is Floor, stamp the strip cell with probability `t`. This extends walls and structures inward with decreasing density.

3. **Water**: if the strip cell is Water and the generated cell is Floor, stamp it with probability `t`. Rivers and pools extend inward.

4. **Floor**: strip Floor cells don't overwrite — floor is the default and the generated terrain likely already has floor here.

5. **Fixtures**: if the strip cell has a fixture and the generated cell has no fixture, place it with probability `t`.

6. **Glyph overrides**: copy from strip with probability `t` (only where tile was also stamped).

The RNG for probability checks uses a deterministic seed derived from the overworld coordinates + direction, so results are reproducible.

### Phase 3: Procedural Fallback

If no edge strip exists for a direction (neighbor not cached), fall back to the current behavior: regenerate the neighbor's elevation from its biome profile and lerp it into the margin. This code is unchanged from the existing `apply_neighbor_bleed()`.

---

## 4. Corner Handling

Where two edge strips overlap (e.g., north + east neighbor both cached), the 5×5 verbatim corner and the blend triangle both receive data from two strips.

**Resolution:** process strips in order: north, south, east, west. Later strips overwrite earlier ones. For the verbatim zone this means east/west edges win over north/south in corners. This is arbitrary but deterministic.

In practice, corners are small (5×5 = 25 cells for verbatim, ~15×15 for blend overlap) and both strips came from real terrain that was itself generated coherently, so conflicts are visually minor.

---

## 5. Interaction with Other Systems

### POI Phase

POI stamps run after `apply_neighbor_bleed()` in the pipeline. A POI that overlaps the bleed zone unconditionally overwrites bleed-stamped tiles. This means:
- A settlement in tile B can build its structures right up to the edge
- The bleed zone provides terrain context around the POI, not constraints on it

### Scatter and Flora

Scatter and flora run in `place_features()`, after bleed. They will naturally decorate the bleed zone with biome-appropriate fixtures. Scatter skips cells that already have fixtures (including those stamped from the edge strip), so neighbor fixtures are preserved.

### FOV and Visibility

No changes. The bleed zone contains normal tiles that the FOV system processes as usual.

### Save/Load

No changes to serialization. Edge strips are transient — extracted on-read from cached TileMaps, never persisted. When a save file is loaded, the location cache is restored with full TileMaps, and edge strip extraction works the same way.

---

## 6. File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `include/astra/edge_strip.h` | Create | `EdgeStripCell` and `EdgeStrip` structs, extraction function declaration |
| `src/generators/edge_strip.cpp` | Create | `extract_edge_strip()` implementation — reads a TileMap edge into an EdgeStrip |
| `include/astra/map_properties.h` | Modify | Add 4 `std::optional<EdgeStrip>` fields |
| `src/game_world.cpp` | Modify | `build_detail_props()` — extract edge strips from cached neighbors |
| `src/generators/detail_map_generator_v2.cpp` | Modify | Rework `apply_neighbor_bleed()` — two-phase stamp + blend with fallback |

---

## 7. Visual Example

```
Tile A (Mountains/Rocky)         Tile B (Forest) — generating
cached, has ridges + water       reads A's east edge strip

    A's east edge (20 cols)       B's west edge (20 cols)
    ────────────────────────      ────────────────────────
    ...###..~~~~..##..####  →  →  ####..##..~~~~..###...........
    ...####.~~~~..###.####  →  →  ####.###..~~~~.####...........
    ...##...~~~~..##..####  →  →  ####..##..~~~~..##............
         ↑                        ↑         ↑         ↑
    A's actual tiles         verbatim   blended    B's own
    (cached)                 (0-4)     (5-19)     generation
```

The river (`~~~~`) flows continuously from A into B. The ridge walls (`####`) extend inward with decreasing density. Beyond the blend zone, B's forest generation takes over naturally.

---

## 8. Out of Scope

- Diagonal neighbor bleed (only cardinal directions)
- Multi-tile POI edge coordination (handled by existing POI group system)
- Edge strip persistence in save files (transient, extracted from cached TileMaps)
- SDL renderer changes
- Changes to the overworld generator
