# Settlement POI System — Design Spec

Phase 6 of detail map generation v2. First POI type: settlements.

## Overview

Settlements are terrain-aware, walkable villages placed on the unified 360×150 detail map. They consist of buildings with interiors, connecting paths, optional perimeter walls, and bridges over water. The system is built as composable components that future POI types (ruins, outposts, etc.) can reuse.

## Architecture

Settlements run as a POI phase in the v2 orchestrator, after scatter and before connectivity:

```
DetailMapGeneratorV2::place_features()
  └── existing scatter code
  └── poi_phase(tilemap, channels, props, rng)
        ├── PlacementScorer     — finds best location + terrain anchors
        ├── SettlementPlanner   — decides what to build, sculpts terrain
        ├── BuildingGenerator   — places buildings with interiors
        ├── PathRouter          — connects buildings, edges, bridges
        ├── PerimeterBuilder    — optional walled enclosure
        └── ExteriorDecorator   — lamps, benches, scatter clearing
```

Data flows through a `SettlementPlan` struct — the planner builds a full blueprint before anything writes to the TileMap. Each subsequent component reads the plan and executes its part.

## Component Details

### 1. PlacementScorer

Finds the best region on the map and catalogs terrain features within it.

**Scoring (candidate regions scanned in chunks):**

| Factor | Weight | Rule |
|--------|--------|------|
| Flatness | Highest | Low elevation variance in region |
| Water proximity | Bonus | Near water but not on it |
| Elevated ground | Bonus | Presence of higher terrain nearby |
| Edge margin | Hard constraint | Minimum ~15 tiles from map edges |
| Structure clear | Hard constraint | Not on top of cliff formations, craters |

**Anchor discovery:** After selecting the best region, the scorer catalogs terrain features and produces 1–3 anchors:

- **Waterfront anchor** — water edge detected within region → distillery, dock
- **Elevated anchor** — higher ground present → lookout
- **Center anchor** — largest flat clearing → main hall, plaza (always present)

**Output:** `PlacementResult{origin, footprint_bounds, vector<Anchor>}` where `Anchor{position, type}`.

### 2. SettlementPlanner

The brain of the system. Takes anchors, lore fields, and biome context to produce a full blueprint.

**Process:**

1. **Size determination** — based on biome + lore context:
   - Small (3–5 buildings): harsh biomes, low lore_tier
   - Medium (5–8 buildings): temperate biomes, moderate lore_tier
   - Large (8–12 buildings): lush biomes, high lore_tier

2. **CivStyle selection** — from `lore_tier` and `lore_alien_architecture` in MapProperties. Hook for future blended styles.

3. **Terrain feature check** — scan anchors: use natural terrain where it exists, sculpt where it doesn't:
   - Need elevated ground for lookout but none exists? Raise a bluff (modify elevation channel + place wall tiles as cliff face).
   - Need flat plaza but terrain is bumpy? Level it (flatten elevation in area).
   - Waterfront exists naturally? Claim it. No sculpting needed.
   - Principle: **use what nature gave us first, create what's missing.**

4. **Building assignment:**
   - Assign key buildings to terrain anchors (waterfront → distillery, elevated → lookout)
   - Place main hall at center anchor
   - Place market/trader near center
   - Grow remaining buildings outward — each new building picks a position adjacent to existing ones, offset by 2–3 tiles for path gaps
   - Growth direction influenced by terrain (avoid water, walls)

5. **Perimeter decision** — walled if `lore_tier >= 2` or biome is hostile (volcanic, scarred). 1–2 gated entries aligned with entry paths.

6. **Bridge detection** — if any path connection must cross water, mark bridge locations at narrowest crossing points.

**Output:** `SettlementPlan` struct containing:
- `origin`, `footprint_bounds`
- `civ_style` (CivStyle reference)
- `buildings` — vector of `BuildingSpec{type, position, size, shape, anchor_type}`
- `paths` — vector of path connection pairs
- `bridges` — vector of `BridgeSpec{start, end, width}`
- `perimeter` — optional `PerimeterSpec{bounds, gate_positions}`
- `terrain_mods` — vector of terrain modifications to apply before building

### 3. CivStyle

Data-driven style definitions. Three initial styles, extensible to more.

```cpp
struct CivStyle {
    std::string name;

    // Walls & floors — glyphs stored as uint8_t glyph override IDs
    // (resolved to actual characters by the renderer)
    uint8_t wall_glyph;
    uint8_t window_glyph;
    uint8_t door_glyph;
    Tile interior_floor;
    Tile path_tile;

    // Fixtures
    FixtureType lighting;     // Torch / Lamp / HoloLight
    FixtureType storage;      // Crate / Locker
    FixtureType furniture;    // Bench / Chair

    // Perimeter
    uint8_t perimeter_glyph;
    uint8_t gate_glyph;

    // Bridge
    uint8_t bridge_floor;
    uint8_t bridge_rail;

    // Furniture role resolution
    FurniturePalette resolve_palette(BuildingType type) const;
};
```

Note: Glyph fields use the TileMap's existing `glyph_override` system (`uint8_t` IDs mapped to display characters by the renderer). This avoids multi-byte char issues with the current Cell struct. Exact glyph assignments determined during implementation.

**Initial styles:**

| | Frontier | Advanced | Ruined |
|---|---|---|---|
| Walls | Wood/metal `#` | Smooth panels `█` | Crumbling `▒` |
| Windows | `▪` | `◻` | broken/missing |
| Lighting | Torch | Holo-lamp | Broken/none |
| Interior floor | Packed dirt | Metal grating | Overgrown floor |
| Path | Dirt path | Paved | Cracked/partial |
| Perimeter | Wooden palisade | Energy fence | Gaps, partial walls (30–40% coverage) |
| Bridge | Wood planks + rope | Metal + railings | Broken planks, gaps |

Style selection logic: `lore_tier < 2` → Frontier, `lore_alien_architecture > 0` → Advanced, contextual flags (abandoned, ancient) → Ruined. Designed so future lore-blended styles slot in without changing the generator.

### 4. BuildingGenerator

Places individual buildings on the TileMap given a `BuildingSpec`.

**Shape generation:**

1. Start with a primary rectangle sized by building type:
   - Dwelling: 4×3 to 6×5
   - Market/trader: 6×4 to 8×6
   - Main hall: 8×6 to 12×8
   - Distillery: 6×5 to 8×6
   - Lookout: 3×3 to 5×4
   - Workshop/storage: 5×4 to 7×5

2. Optionally attach 1–2 extensions to create composite shapes:
   - Wing (side extension) → L-shape
   - Back room → T-shape
   - Porch (1-tile deep, open front) facing nearest path
   - Alcove (small bump-out)
   - Extensions chosen to make architectural sense: porch faces path, back room extends away from center

**Construction order:**

1. **Clear area** — remove existing fixtures within building footprint + 1-tile margin
2. **Floor** — fill interior with CivStyle `interior_floor`
3. **Walls** — perimeter of shape gets wall tiles (CivStyle `wall_glyph`)
4. **Doors** — 1–2 per building. Primary door faces nearest path/plaza. Never on corners. Secondary door on larger buildings (back entrance).
5. **Windows** — along walls, spaced every 3–4 wall tiles. Not adjacent to doors, not on corners. CivStyle `window_glyph`.
6. **Interior furnishing** — using the furniture palette system (see below)

### 5. Furniture Palette System

Data-driven, extensible furnishing for building interiors.

```cpp
struct FurnitureEntry {
    FixtureType type;
    float frequency;        // probability of appearing
    bool wall_adjacent;     // must be placed against a wall
    bool needs_clearance;   // needs open tile in front for interaction
    bool prefers_corner;    // prefers corner placement
};

struct FurniturePalette {
    std::vector<FurnitureEntry> entries;
};
```

**Placement rules:**
- **Wall-adjacent** — cabinets, racks, displays, stoves: placed against walls
- **Center-viable** — tables, chairs, workbenches: can go in open floor space
- **Needs clearance** — stoves, displays, consoles: at least one open tile in front
- **Corner pieces** — storage crates, cabinets: prefer corner positions

**CivStyle resolves generic roles to concrete fixtures:**

| Role | Frontier | Advanced | Ruined |
|------|----------|----------|--------|
| Cooking | CampStove | FoodTerminal | BrokenStove |
| Storage | Crate | Locker | BrokenCrate |
| Seating | Bench | Chair | Debris |
| Knowledge | BookCabinet | DataTerminal | BrokenTerminal |
| Work surface | Table | Console | BrokenTable |
| Display | Rack | WeaponDisplay | BrokenRack |

**Per-building-type palettes (examples):**
- **Dwelling:** cooking (0.8), seating (0.7), storage (0.6), knowledge (0.3)
- **Market:** display (0.9), storage (0.8), seating (0.3)
- **Main hall:** seating (0.9, multiple), work surface (0.7), knowledge (0.5), display (0.4)
- **Distillery:** work surface (0.9), storage (0.8), cooking (0.4)
- **Lookout:** knowledge (0.6), seating (0.5) — window-heavy walls, minimal furniture

New fixture types and building types added by extending palettes. No generator changes needed.

### 6. PathRouter

Connects buildings to each other and to the map edge.

**Path types:**
- **Main path** (2-wide): from settlement entrance to central area (plaza/main hall)
- **Branch paths** (1-wide): from individual buildings to the main path
- **Entry path** (2-wide): from settlement to nearest map edge, so player can find it

**Routing:**
- Grid-aligned L-shaped connections (pick shorter L of two options)
- Path tile uses CivStyle `path_tile` — visually distinct from surrounding terrain
- Each building's primary door connects to the nearest existing path segment

**Bridge generation** — when a path must cross water:

```
    O════O        ← support pillars (wall tiles)
~~~~║····║~~~~    ← bridge floor (CivStyle bridge_floor)
~~~~║····║~~~~       + railings (CivStyle bridge_rail)
~~~~║····║~~~~
    O════O
```

- Find narrowest water crossing point
- Bridge width matches path width (1 or 2 tiles)
- Railings on both sides
- Support pillars (wall tiles) at each end
- Bridge floor is passable

### 7. PerimeterBuilder

Optional walled enclosure around the settlement.

**Construction:**
- Calculate bounding rectangle around all buildings + 3–4 tile padding
- Place wall ring using CivStyle `perimeter_glyph`
- 1–2 gate openings aligned with entry paths
- Gates use CivStyle `gate_glyph`, are passable

**Style variations:**
- **Frontier:** full wooden palisade, simple gate openings
- **Advanced:** full wall, optional corner towers (small 2×2 rooms with lighting)
- **Ruined:** partial wall with 30–40% coverage, gaps, missing sections

### 8. ExteriorDecorator

Final polish pass after all structural work.

**Decorations:**
- **Lighting** — CivStyle lamps along paths every 5–6 tiles, flanking building doors, at gate entries
- **Furniture** — benches near plazas, planters near buildings on lush biomes
- **Signage** — fixture near market/trader entrance
- **Scatter clearing** — remove existing scatter fixtures within settlement footprint + small margin
- **Transition zone** — at settlement edges, replace dense vegetation with stumps/cleared area for natural blending with surrounding terrain

## Settlement Sizing by Context

| Size | Buildings | When |
|------|-----------|------|
| Small (3–5) | Main hall, market, 1–3 dwellings | Harsh biomes (volcanic, scarred, barren), low lore_tier |
| Medium (5–8) | + workshop, distillery/lookout | Temperate biomes, moderate lore_tier |
| Large (8–12) | + extra dwellings, storage, specialized | Lush biomes (forest, jungle), high lore_tier |

Mega-cities (multi-tile settlements) are out of scope for this phase. Single-tile only.

## Terrain Sculpting

Settlements modify terrain to fit their needs, but prefer natural terrain first.

**Sculpting operations (applied before building placement):**
- **Level** — flatten elevation in a rectangular area (for plaza, building sites)
- **Raise bluff** — increase elevation + place wall tiles as cliff face (for lookout)
- **Cut bank** — flatten water edge area (for dock/waterfront buildings)
- **Clear** — remove structure masks (walls, formations) in footprint

**Principle:** Scan terrain features first. Use natural features as anchors. Only sculpt what's missing. A settlement on varied terrain will look more organic than one on flat plains because more natural anchors exist and less sculpting is needed.

## Integration Points

**Input from overworld:**
- `MapProperties::detail_has_poi` — whether this tile has a POI
- `MapProperties::detail_poi_type` — `Tile::OW_Settlement`
- `MapProperties::lore_tier`, `lore_alien_architecture` — drive CivStyle selection
- Biome type from `detail_terrain` — influences settlement size and scatter clearing

**Input from terrain channels:**
- Elevation grid — flatness scoring, elevated anchor detection
- Moisture grid — water proximity scoring, waterfront anchor detection
- Structure grid — avoid placing on formations/craters

**Output to TileMap:**
- Wall tiles, floor tiles, path tiles, bridge tiles
- Fixtures (doors, windows, furniture, lighting, decorations)
- Cleared scatter within settlement footprint

**Connectivity phase** runs after POI placement — ensures all settlement interiors connect to the walkable map via BFS flood-fill safety net.

## New Types Required

**FixtureTypes to add:**
- `CampStove`, `FoodTerminal`
- `Locker`, `BookCabinet`, `DataTerminal`
- `Bench`, `Chair`
- `Lamp`, `HoloLight`
- `Gate`
- `BridgeRail`

**Tile types potentially needed:**
- Path tiles per CivStyle (or use glyph overrides on existing Floor tile)

## File Structure

```
include/astra/
  civ_style.h            — CivStyle struct, style definitions
  settlement_plan.h      — SettlementPlan, BuildingSpec, Anchor, etc.
  furniture_palette.h    — FurnitureEntry, FurniturePalette
  placement_scorer.h     — PlacementScorer
  building_generator.h   — BuildingGenerator

src/generators/
  civ_styles.cpp         — three initial style definitions
  furniture_palettes.cpp — palettes per building type
  placement_scorer.cpp   — scoring + anchor discovery
  settlement_planner.cpp — plan assembly + terrain sculpting
  building_generator.cpp — shape gen, walls, doors, windows, furnishing
  path_router.cpp        — path routing + bridge generation
  perimeter_builder.cpp  — optional walled enclosure
  exterior_decorator.cpp — lamps, benches, scatter clearing
  poi_phase.cpp          — orchestrator, called from v2 generator
```

## Dev Testing

Extend `biome_test <biome>` in dev console to accept a POI flag: `biome_test <biome> settlement`. Generates a 360×150 v2 map with a settlement placed on it. Allows rapid iteration on placement, building shapes, and decoration.
