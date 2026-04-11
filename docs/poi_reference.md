# POI Generator Reference

Living catalog of every Point-of-Interest generator in Astra's detail map
pipeline. Each POI follows the same template: a variant specifications
table, a selection function, a stamp pipeline, and per-variant stamp
logic. Kept alongside the code so new POIs can be added by copy/paste and
tuned in one place.

All POIs dispatch from `src/generators/poi_phase.cpp` via `Tile::OW_*`
values set on the overworld map, and reuse the shared placement infra:
`PlacementScorer` for site selection, `TerrainChannels` for biome data,
`MapProperties` for lore context.

| POI | File | Dispatch tile |
|---|---|---|
| Settlement | `src/generators/settlement_planner.cpp` | `Tile::OW_Settlement` |
| Ruins | `src/generators/ruin_generator.cpp` | `Tile::OW_Ruins` |
| Outpost | `src/generators/outpost_planner.cpp` | `Tile::OW_Outpost` |
| Crashed Ship | `src/generators/crashed_ship_generator.cpp` | `Tile::OW_CrashedShip` |
| Cave Entrance | `src/generators/cave_entrance_generator.cpp` | `Tile::OW_CaveEntrance` |

---

## 1. Settlement

Civilization hub with multiple buildings connected by paths, optional
perimeter wall, terrain mods to level the site. Planned by
`SettlementPlanner`, stamped by shared downstream stages
(`BuildingGenerator`, `PathRouter`, `PerimeterBuilder`,
`ExteriorDecorator`).

### Variant specifications

Civ style selected by `select_civ_style(props)` from lore, biome
determines size category. No enum variants — the civ style + size
category drives the result.

| Spec | Frontier | Advanced | Ruined |
|---|---|---|---|
| **Trigger** | low lore | `lore_tier ≥ 2` or `alien_strength > 0.3` | `lore_plague_origin = true` |
| **Wall tile** | StructuralWall | StructuralWall | Wall (decayed) |
| **Floor tile** | IndoorFloor | IndoorFloor | Floor |
| **Lighting** | Torch | HoloLight | Torch |
| **Storage** | Crate | Locker | Crate |
| **Cooking** | CampStove | FoodTerminal | Debris |
| **Knowledge** | BookCabinet | DataTerminal | Debris |
| **Display** | Rack | WeaponDisplay | Debris |
| **Perimeter decay** | 0.0 | 0.0 (with 3×3 corner towers) | 0.35 |

### Size categories (footprint)

| Size | Trigger | Footprint | Building count |
|---|---|---|---|
| **Small** | harsh biomes (Volcanic/Ice/Scarred/Corroded/Crystal) or `lore_tier = 0` | 70×45 | 3–5 |
| **Medium** | default | 100×60 | 5–8 |
| **Large** | lush biomes (Forest/Jungle/Grassland/Marsh) + `lore_tier ≥ 2` | 130×80 | 8–12 |

### Placement pipeline

1. **Determine size category + footprint** from biome + lore tier.
2. **PlacementScorer.score()** with the chosen footprint. On failure, return empty rect.
3. **Plan terrain mods** — level the center area, raise bluff for elevated anchors, clear structure masks inside the footprint.
4. **Place anchor buildings** — MainHall at center, Distillery at waterfront anchor, Lookout at elevated anchor.
5. **Place Market near center** (if building count allows).
6. **Grow remaining buildings** — weighted roll (60% Dwelling, 20% Workshop, 20% Storage) via `find_growth_candidates` which scores sites around existing buildings.
7. **Plan paths** — door-to-nearest-door edges between buildings, plus a 4-wide winding entry path from center to the nearest map edge.
8. **Detect bridges** — scan water crossings from the center to each map edge, add `BridgeSpec` entries.
9. **Decide perimeter** — walled if `lore_tier ≥ 2` or biome is hostile (Volcanic / ScarredScorched / ScarredGlassed / Corroded).
10. **Downstream stages** run via `poi_phase.cpp`: `BuildingGenerator` → `PathRouter` → `PerimeterBuilder` → `ExteriorDecorator`.

### Building types

Anchor buildings, market, and organic growth produce a mix of:
`MainHall`, `Market`, `Dwelling`, `Distillery`, `Lookout`, `Workshop`,
`Storage`. Size ranges per type in `settlement_planner.cpp:size_range()`.
Interior furniture comes from `furniture_palette(type, style)` in
`furniture_palettes.cpp`.

---

## 2. Ruins

Ancient megastructure walls spanning the entire detail map. BSP-driven
wall network with gradient/sectoral decay and per-civilization visual
themes. Reaches the map edges so neighboring ruin tiles connect
seamlessly.

### Variant specifications (civilizations)

| Spec | Monolithic | Baroque | Crystal | Industrial |
|---|---|---|---|---|
| **Feel** | Massive geometric blocks | Ornate gilded halls | Bright angular lattices | Rusted heavy machinery |
| **Wall glyphs** | `█ ▓ ░` | `╔ ═ ╗ ║ ╬ ╠ ╣` | `┌ ─ ┐ │ ┼ ├ ┤` | `▓ █ ░` |
| **Accent glyphs** | `▪ ■` | `◆ ◇` | `◈ ◎ ◉` | `⊞ ⊠` |
| **Color primary** | 250 (near-white) | 178 (warm gold) | 51 (bright cyan) | 166 (corroded orange) |
| **Color secondary** | 245 (light gray) | 124 (deep red) | 231 (white) | 124 (rust red) |
| **Wall thickness bias** | 1.3 | 0.8 | 0.6 | 1.1 |
| **Split regularity** | 0.3 (organic) | 0.8 (geometric) | 0.5 | 0.6 |
| **Preferred rooms** | GreatHall×2, Temple, Vault | Archive×2, Temple, Observatory | Observatory×2, Archive, Vault | Vault×2, Workshop, Storage |
| **Architecture** | Geometric | Crystalline | LightWoven | VoidCarved |

Selection is by dev override (`props.detail_ruin_civ`) or by
`props.lore_civ_architecture` mapping, or random. See
`ruin_generator.cpp:select_civ()`.

### Placement pipeline

1. **Select civilization** from dev override / lore / random.
2. **Footprint = full map** (ruins span edge to edge — they're megastructures).
3. **Roll decay modifier** (0.3–1.0 random, or dev override).
4. **BSP wall network** — `BspGenerator` recursively subdivides the footprint with thickness/regularity from the civ config.
5. **Room identification** — `RoomIdentifier` flood-fills the enclosed spaces and assigns `BuildingType` themes from the civ's `preferred_rooms`.
6. **Furnish rooms** — per-room furniture palettes with ruined fixture substitutions (most roles → `Debris`).
7. **Decay pass** — `RuinDecay` with gradient (distance-from-edge) + sectoral variance, breaks walls into `Debris` and rubble.
8. **Post-processing stamps** — battle-scarred, infested, flooded, excavated (applied when decay modifier > 0.3).
9. **Edge continuity** — if neighboring overworld tiles are also ruins, stamp edge stubs using a shared seed so wall networks connect across tiles.

### Notes

- Ruins are the only POI that always fills the full detail map.
- Multi-tile ruins use shared-seed edge strips (`edge_n/s/e/w` flags from `MapProperties`) for seamless continuity.
- Fixture types within rooms are decayed substitutions of settlement civ-styles, keyed by the civ config's `preferred_rooms`.

---

## 3. Outpost

Small fenced fort with one main building inside a biome-themed palisade
and 2–4 hand-stamped tents in the surrounding ring. Planned by
`OutpostPlanner`, reuses settlement downstream stages (`BuildingGenerator`,
`PathRouter`, `PerimeterBuilder`, `ExteriorDecorator`) by emitting a
`SettlementPlan`. Tents + campfires + fence glyphs apply in a
`post_stamp()` phase.

### Variant specifications

Single generic type. Variants (forward base / refuge / scoundrel hideout
/ traveler camp) are a roadmap follow-up.

| Spec | Value |
|---|---|
| **Total footprint** | 44×32 |
| **Fenced core** | 30×22 |
| **Main building** | 13×9 (`BuildingType::OutpostMain`) |
| **Fence coverage** | ~70% (CivStyle.decay = 0.3) |
| **Fence style name** | "Frontier" (avoids PerimeterBuilder's auto 3×3 corner towers) |
| **Gates** | 1 main gate, random side (N/S/E/W) |
| **Tents** | 2–4 hand-stamped 3×2 mini-structures outside the fence |
| **Campfires** | 1–2 CampStove+Bench+Bench clusters near tents |
| **Biome validity** | All biomes including Aquatic (PlacementScorer handles terrain fit) |

### Biome fence material

Fence walls get a `set_glyph_override()` post-pass keyed by biome:

| Biome | Material | Glyph slot |
|---|---|---|
| Forest / Jungle / Grassland / Marsh / Fungal | wood palisade | 2 |
| Rocky / Mountains | stacked stone | 1 |
| Sandy / Volcanic / Aquatic / Scarred* | salvage plating | 3 |
| Ice / Crystal | metal / ice blocks | 0 |

### Stamp pipeline

1. **Footprint 44×32** passed to `PlacementScorer`.
2. **OutpostPlanner.plan()** builds a `SettlementPlan`:
   - `TerrainMod::Clear` over the full footprint.
   - Main building `BuildingSpec` (`OutpostMain`, 13×9, door faces the gate side).
   - `PerimeterSpec` for the fenced core with one gate at the middle of the chosen side.
   - Two `PathSpec` entries: gate → main building door (1-wide), gate → nearest map edge (3-wide outward).
3. **Downstream stages** run via `poi_phase.cpp`: `BuildingGenerator` → `PathRouter` → `PerimeterBuilder` → `ExteriorDecorator`.
4. **OutpostPlanner.post_stamp()** runs after:
   - Fence glyph override post-pass — walks a 3-tile band on each edge of the fence bounds, applying the biome material glyph to every `Tile::Wall` tile (handles PerimeterBuilder's ±1 organic noise offsets).
   - Tent placement — rejection sampling (up to 60 attempts) for 2–4 tents in the outer ring. Each 3×2 tent stamps 5 perimeter walls with biome glyph + 1 floor-door on the fence-facing side.
   - Campfire clusters — 1–2 `{CampStove, Bench, Bench}` triples placed near the first tents, offset outward from the fence center.

### Main building fixtures (`OutpostMain` palette)

| Placement rule | Fixture | Count | Probability |
|---|---|---|---|
| Anchor | Console | 1 | 1.0 |
| TableSet | Table + Bench | 1 pair | 1.0 |
| WallUniform | Bunk | 1–2 | 1.0 |
| Corner | Crate | 1–2 | 1.0 |
| WallUniform | CampStove | 1 | 0.8 |

Hardcoded (not `style.storage`/`style.cooking`) so `spawn_outpost_npcs`
can always find Console/Bunk/Crate for NPC anchoring regardless of civ
style.

---

## 4. Crashed Ship

Wrecked hull sitting on the surface with a long scorched skid mark
behind it. Three ship classes selected by lore tier with random 4-way
orientation. Empty loot wreck — no inhabitants. Stamped directly on the
map by `CrashedShipGenerator` without going through `SettlementPlan`.

### Variant specifications

| Spec | EscapePod | Freighter | Corvette |
|---|---|---|---|
| **Hull size** | 10 × 7 | 24 × 9 | 36 × 11 |
| **Body half-height** | 3 | 4 | 5 |
| **Nose taper length** | 0 (flat blunt) | 1 | 1 |
| **Stern taper** | 0 | 0 | 0 |
| **Hull coverage** | 75% | 75% | 75% |
| **Rooms** | 1 (cabin) | 3 (engine / cargo / cockpit) | 5 (engine / cargo / mess / quarters / cockpit) |
| **Bulkheads (dx)** | — | -4, +4 | -11, -4, +2, +10 |
| **Engine nacelles** | 1 tile | 2 tiles | 2 tiles |
| **Wing span / width** | 0 / 0 | 1 / 2 | 2 / 3 |
| **Skid length** | 14–18 | 22–28 | 32–40 |
| **Debris radius** | 8 | 14 | 20 |
| **Debris fragment count** | 3–5 | 8–15 | 12–20 |
| **Breach count** | 2 | 3–4 | 4–5 |
| **Fixtures** | 1 Console, 1 Bunk, 1 Crate | 1 Conduit + 1 Rack (engine); 5 Crate (cargo); 1 Console (cockpit) | 1 Conduit + 1 Rack (engine); 5 Crate (cargo); 1 Table + 2 Bench (mess); 3 Bunk (quarters); 2 Console (cockpit) |
| **Footprint axis** | 28 | 52 | 76 |
| **Footprint perp** | 19 | 21 | 23 |

### Lore-weighted selection

```cpp
ShipClass pick_ship_class(int lore_tier, std::mt19937& rng) {
    int r = static_cast<int>(rng() % 100);
    if (lore_tier <= 0) {            // 70 / 25 / 5
        if (r < 70) return ShipClass::EscapePod;
        if (r < 95) return ShipClass::Freighter;
        return ShipClass::Corvette;
    }
    if (lore_tier == 1) {            // 30 / 55 / 15
        if (r < 30) return ShipClass::EscapePod;
        if (r < 85) return ShipClass::Freighter;
        return ShipClass::Corvette;
    }
    if (r < 10) return ShipClass::EscapePod;   // tier 2+: 10 / 40 / 50
    if (r < 50) return ShipClass::Freighter;
    return ShipClass::Corvette;
}
```

Dev override: `props.detail_crashed_ship_class` = `"pod"` / `"freighter"` / `"corvette"` skips the roll.

### Orientation

4-way uniform: `{East, West, South, North}`. A `rotate()` helper maps
ship-local `(dx, dy)` to world offsets for every stamp operation so the
same layout logic works in all directions:

```cpp
std::pair<int,int> rotate(int dx, int dy, ShipOrientation o) {
    switch (o) {
        case ShipOrientation::East:  return { dx,  dy};
        case ShipOrientation::West:  return {-dx,  dy};
        case ShipOrientation::South: return {-dy,  dx};  // CW
        case ShipOrientation::North: return { dy, -dx};  // CCW
    }
}
```

### Biome filter

Aquatic is skipped entirely (`return empty Rect`). All other biomes valid
— `PlacementScorer` rejects unfit sites.

### Stamp pipeline

1. **Reject Aquatic biome.**
2. **Pick class** (dev override or lore-weighted roll).
3. **Pick orientation** uniform over {N, S, E, W}.
4. **Compute footprint** (axis × perp) and rotate to world dimensions.
5. **PlacementScorer.score()** with the rotated footprint.
6. **Compute hull center** offset inside the footprint so the skid has room behind the stern (`footprint edge + skid_max + hull_len/2` on the stern side).
7. **Stamp skid mark** (BEFORE hull — hull overwrites the stern end):
   - Walk ship-local dx from `-half - skid_length` to `-half - 1`.
   - fbm-noise perpendicular offset ±2 tiles for gentle curvature.
   - 3-wide band: center row always scorched, side rows 80% probability (gaps).
   - **Every tile gets `map.remove_fixture()` called** — the skid plows through flora, grass, minerals, debris.
   - Scorched tiles become `Tile::IndoorFloor` (renderer tints it as burnt ground).
   - Flanking rubble: 30% chance of `Tile::Wall` fragments one tile outside the band.
8. **Stamp hull** — tapered `hull_half_h(dx)`, 75% `StructuralWall` + glyph 0 (metal) on edges (breached 25%), `IndoorFloor` interior.
9. **Stamp bulkheads** — partial `StructuralWall` at each `bulkhead_dx`, center row gap for passage, 60% per-tile coverage.
10. **Stamp breaches** — `breach_min..breach_max` random hull-edge openings, 2–3 tiles wide, replacing walls with `IndoorFloor`.
11. **Stamp engine nacelles** — `nacelle_len` tiles behind the stern on dy = ±body_half_h, 80% coverage, metal glyph.
12. **Stamp wings** — perpendicular stubs of `wing_span` tiles × `wing_width` tiles along dx = 0, 85% coverage, metal glyph.
13. **Stamp debris field** — random wall fragments within `debris_radius`, rejection-sampled to avoid hull and skid band.
14. **Place fixtures** per room — shuffle interior floor candidates, drop fixtures in order from `fixtures_by_room[i]`.
15. **Dungeon portal roll** — 20% chance, placed on an interior floor tile in the middle room.

---

## 5. Cave Entrance

Dungeon entry point with three lore-weighted variants. Primary purpose:
place a `Tile::Portal` that leads into an underground dungeon when the
player interacts with it. Cave/mine variants embed into cliff walls;
ancient excavation sits on flatter ground.

### Variant specifications

| Spec | Natural Cave | Abandoned Mine | Ancient Excavation |
|---|---|---|---|
| **Footprint** | 16×12 | 22×16 | 30×22 |
| **Biome filter** | Mountains / Rocky / Volcanic only | Mountains / Rocky / Volcanic only | Any biome |
| **Requires cliff** | Yes | Yes | No |
| **Wall material** | `Tile::Wall` (natural rock) | `StructuralWall` + glyph 2 (wood supports), glyph 0 (metal rails) | `StructuralWall` + glyph 1 (stone) |
| **Orientation** | Derived from cliff hit | Derived from cliff hit | Fixed/random |
| **Fixtures** | 3–5 Debris near mouth | 2 Crate + 1 Conduit + 3–5 Debris | 1 Console + 1 BookCabinet + 2–4 Debris |
| **Portal** | At mouth interior | Inside the pit | At the bottom of the visible steps |

### Lore-weighted selection

```cpp
CaveVariant pick_cave_variant(int lore_tier, Biome b, std::mt19937& rng) {
    bool cliff_biome = (b == Biome::Mountains || b == Biome::Rocky ||
                        b == Biome::Volcanic);
    int r = rng() % 100;

    if (!cliff_biome) {
        // Flat biomes: only ancient excavation can spawn, and only in higher lore.
        if (lore_tier < 1) return CaveVariant::None;  // signal "no POI"
        return CaveVariant::AncientExcavation;
    }

    // Cliff biomes: full roll, lore-weighted.
    switch (lore_tier) {
        case 0: // 80/15/5
            if (r < 80) return CaveVariant::NaturalCave;
            if (r < 95) return CaveVariant::AbandonedMine;
            return CaveVariant::AncientExcavation;
        case 1: // 50/35/15
            if (r < 50) return CaveVariant::NaturalCave;
            if (r < 85) return CaveVariant::AbandonedMine;
            return CaveVariant::AncientExcavation;
        default: // 20/30/50
            if (r < 20) return CaveVariant::NaturalCave;
            if (r < 50) return CaveVariant::AbandonedMine;
            return CaveVariant::AncientExcavation;
    }
}
```

`CaveVariant::None` means "fail this POI" — generator returns empty rect.

### Cliff detection (for NaturalCave / AbandonedMine)

After `PlacementScorer` picks a site, the generator scans the footprint
interior for a cliff edge — an existing `Tile::Wall` tile with at least
one 4-neighbor `Tile::Floor`. The algorithm:

```cpp
struct CliffHit {
    int wall_x, wall_y;         // the wall tile (where the mouth opens INTO)
    int floor_x, floor_y;       // the adjacent floor tile (outside the mouth)
    GateSide mouth_facing;      // which way the mouth opens (N/S/E/W)
};

std::optional<CliffHit> find_cliff_edge(const TileMap& map, const Rect& foot) {
    // Walk all tiles in the footprint. For each Wall tile, check 4-neighbors.
    // The first Wall tile with a Floor neighbor where the Wall has at least
    // 2 adjacent Wall tiles (so it's part of a cliff, not an isolated rock)
    // is returned. If none found, return nullopt.
}
```

If no cliff edge is found → generator returns empty rect (consistent
with fail-gracefully policy).

**Mouth facing** is determined by which direction the Floor sits
relative to the Wall:
- Floor north of wall → mouth faces North (ship-local: cliff is south of mouth)
- etc.

### Stamp pipeline

1. **Pick variant** — dev override or lore-weighted roll. If `None`, return empty rect.
2. **Score placement** — `PlacementScorer::score()` with the variant's footprint. If invalid → empty rect.
3. **Cliff detection** (NaturalCave, AbandonedMine only) — scan the footprint for a cliff edge. If none → empty rect. For AncientExcavation, skip this step and use the footprint center directly.
4. **Stamp variant-specific content** (see below).
5. **Place fixtures** per the variant's fixture list.
6. **Place Portal** at the variant-specific position.
7. Return the placement footprint.

### Variant stamp logic

**NaturalCave (16×12):**
- Stamp a noise-shaped rocky outcrop extending from the cliff into the footprint (legacy-style ellipse with noise).
- Carve a 3-wide cave mouth corridor from the outer edge of the outcrop to the cliff wall (where the cliff hit was detected).
- Place Portal at the cave mouth interior (1 tile into the wall side).
- Scatter 3–5 Debris fixtures in a small radius outside the mouth.
- Small boulders (1–2 tile Wall clusters) around the outcrop, 6–10 of them.

**AbandonedMine (22×16):**
- Stamp a squared-off pit: rectangular `StructuralWall` outline ~8×6 at the cliff face, interior cleared.
- Wooden support beams: 2–4 `StructuralWall` tiles with glyph 2 (wood) flanking the pit entrance.
- Short mine cart rails: a 6-tile line of `Floor` tiles leading out from the pit entrance in the direction away from the cliff. Rails are visually hinted by a `Conduit` fixture every 2 tiles.
- Place Portal inside the pit.
- 2 Crates + 1 Conduit inside/beside the pit.
- 3–5 Debris fixtures scattered around.

**AncientExcavation (30×22):**
- Stamp a stone plaza: rectangular `StructuralWall` outline ~14×10 with a clear interior floor.
- 4 corner pillar fragments: 2×2 `StructuralWall` clusters with glyph 1 (concrete/stone) at each corner of the plaza (weathered, ~70% coverage).
- Central pit: 5×3 cutout in the middle where stone steps descend. The pit tiles use `Tile::IndoorFloor` to hint at "ancient stone floor below."
- Place Portal at the center of the pit.
- 1 Console + 1 BookCabinet inside the plaza (ancient terminals).
- 2–4 Debris fixtures scattered within the plaza.
- 6–10 boulders scattered outside the plaza (overgrown with surrounding terrain).

---

## Template for new POIs

When adding a new POI generator, copy this skeleton and fill in the
specifics. Section numbering above is purely for reading order — nothing
else depends on it.

```
## N. <POI Name>

<One-paragraph description: what it is, where it's stamped, which
class owns the logic, what downstream stages it reuses or bypasses.>

### Variant specifications

<Table with one column per variant, rows for footprint, biome filter,
fixtures, etc. Or a single "Single type" column if no variants.>

### Selection logic

<Code block showing how the variant is picked — lore-weighted roll,
biome filter, dev override handling.>

### Placement / detection

<Any special placement logic: cliff detection, water requirements,
orientation derivation, etc. Skip this section if it's just
PlacementScorer.>

### Stamp pipeline

<Numbered steps showing what gets stamped in order. Include the
downstream stages called via poi_phase.cpp if any.>

### Variant stamp logic

<Per-variant specifics: shapes, fixture placements, visual details.>
```

---

## Shared Infrastructure

All POIs rely on:

- **`PlacementScorer`** (`src/generators/placement_scorer.cpp`) — scores a footprint against the `TerrainChannels` and `TileMap` to find a viable site. Returns a `PlacementResult` with the chosen footprint + anchors, or `valid = false`.
- **`TerrainChannels`** (`include/astra/terrain_channels.h`) — elevation + moisture + structure mask + flora grids. POIs read `struc(x,y)` to check for water/wall structure.
- **`MapProperties`** (`include/astra/map_properties.h`) — biome, lore tier/alien strength/plague flags, neighbor POI tiles for edge continuity, dev overrides.
- **`set_glyph_override(x, y, idx)`** on `TileMap` — 2-bit glyph slot for renderer material theming. Slot assignments: 0 = metal, 1 = concrete/stone, 2 = wood, 3 = salvage.

Downstream stages used by settlement + outpost (but bypassed by ruins /
crashed ship / cave entrance):

- **`BuildingGenerator`** — stamps a `BuildingSpec` with walls/floor/doors/windows and runs `furniture_palette(type, style)` for interior fixtures.
- **`PathRouter`** — carves `PathSpec` entries as L-shaped narrow paths or wide winding paths with noise offsets.
- **`PerimeterBuilder`** — stamps a `PerimeterSpec` fence/wall with `CivStyle.decay` for coverage gaps and organic noise offsets.
- **`ExteriorDecorator`** — path lighting, scatter cleanup, lamps along detected `Tile::Path` tiles.

---

## Dev Console

All POIs can be tested via the dev console command `biome_test`:

```
biome_test <biome> [settlement [frontier|advanced|ruined]]
                   [ruins [monolithic|baroque|crystal|industrial] [connected]]
                   [outpost]
                   [ship [pod|freighter|corvette]]
                   [cave [natural|mine|excavation]]
```

The command regenerates a 360×150 detail map with the chosen biome and
stamps the selected POI at the center. NPC spawning fires for
settlement/outpost; crashed ship + cave entrance stay empty.
