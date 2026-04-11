# Cave Entrance POI — Design Spec

**Date:** 2026-04-11
**Status:** Draft
**Scope:** Phase 6 of `2026-04-07-detail-map-generation-v2-design.md` — implement the Cave Entrance POI stamp.

## Context

Phase 6 of the detail map v2 rewrite introduces POIs. Settlements, Ruins, Outposts, and Crashed Ships are implemented; Cave Entrance, Landing Pad, Beacon, and Megastructure are still stubbed in `poi_phase.cpp`. This spec covers the Cave Entrance stamp only.

The legacy `detail_map_generator.cpp:784-854` had a single cave entrance (16×12 rocky outcrop with a noise-shaped ellipse, 3-wide cave mouth from the south, 6–10 scattered boulders, `Tile::Portal` at center, 3–5 Debris fixtures). The v2 rewrite keeps the natural-cave feel but expands it into three lore-weighted variants with biome-aware placement that embeds cave mouths into real cliff faces.

Unlike other POIs, cave entrance's **primary purpose is dungeon access**. The generator's job is to stamp a visually distinct "go down here" marker on the surface; the dungeon below is handled by existing `TunnelCaveGenerator` / `OpenCaveGenerator`. Every cave entrance therefore has a mandatory `Tile::Portal`.

## Concept

A cave entrance is a **dungeon portal in the wilderness** with three visual forms scaling by lore tier:

- **Natural cave** — a dark cave mouth carved into a cliff face. Organic, weathered, the default.
- **Abandoned mine** — squared-off pit with wooden supports and mine cart rails. Suggests someone dug here and left.
- **Ancient excavation** — stone plaza around a central pit with descending steps. Precursor-built, imposing.

### Visual signature per variant

- **Natural cave**: rocky outcrop with a dark cave mouth opening directly into an existing cliff wall. Debris at the mouth. Boulders scattered around.
- **Abandoned mine**: a squared `StructuralWall` pit at a cliff face with wooden beams framing it, a short run of mine cart rails leading out into the open, 2 Crates + 1 Conduit dropped near the entrance.
- **Ancient excavation**: a large rectangular stone plaza with 4 corner pillar fragments, a central pit in the middle where stone steps descend, 1 Console + 1 BookCabinet inside (ancient terminals), scattered Debris.

### Placement constraint

Natural caves and abandoned mines are embedded in cliff walls — their mouths open into solid rock. This restricts them to biomes that actually have cliffs: **Mountains, Rocky, Volcanic**. Ancient excavations don't need a cliff backing and can spawn in any biome.

## Decisions Summary

| Decision | Choice | Rationale |
|---|---|---|
| Concept | Three lore-weighted variants | Variety at different lore tiers without scope creep |
| Variant scaling | Variant-sized footprints (16×12 / 22×16 / 30×22) | Larger lore = more grandeur |
| Biome filter | Cave/mine restricted to Mountains/Rocky/Volcanic; excavation any biome | Cave mouths need real cliffs to open into |
| Non-cliff biomes | Fail gracefully (no POI) if lore roll picks cave/mine AND not in a cliff biome | Low-lore forests don't get cave entrances; consistent with "cliffs needed" rule |
| Cliff detection | Scan the scored footprint for `Tile::Wall` adjacent to `Tile::Floor` | Reuses `PlacementScorer` + post-scan; no new scoring infra |
| Mouth orientation | Derived from cliff hit direction (Floor → mouth side) | Mouth always opens away from the cliff |
| Portal | Always present (mandatory) | Cave entrance's whole purpose is dungeon access |
| Architecture | New `CaveEntranceGenerator` parallel to `RuinGenerator` / `CrashedShipGenerator` | Stamps directly on map; no `SettlementPlan` involved |
| Dev command | `biome_test <biome> cave [natural|mine|excavation]` | Exercises all variants during development |

---

## 1. Architecture

### New files

| File | Responsibility |
|---|---|
| `include/astra/cave_entrance_types.h` | `CaveVariant` enum (None, NaturalCave, AbandonedMine, AncientExcavation), `CaveVariantSpec` struct (footprint, fixture template), optional `CliffHit` helper struct. |
| `include/astra/cave_entrance_generator.h` | `CaveEntranceGenerator` class with one public `generate()` method. |
| `src/generators/cave_entrance_generator.cpp` | Full generator: biome filter + lore roll, cliff detection, variant-specific stamping, portal placement. |

### Modified files

| File | Change |
|---|---|
| `include/astra/map_properties.h` | Add `std::string detail_cave_variant` dev override field. |
| `src/generators/poi_phase.cpp` | Remove `OW_CaveEntrance` from the stub list; dispatch to `CaveEntranceGenerator`. |
| `CMakeLists.txt` | Add `src/generators/cave_entrance_generator.cpp` to `ASTRA_SOURCES`. |
| `src/dev_console.cpp` | Parse `cave` / `cave_entrance` + variant name args in `biome_test`. |
| `src/game.cpp` | `dev_command_biome_test()` branch for cave entrance. |
| `docs/roadmap.md` | Check the Cave Entrance POI box; add follow-ups. |

### Reused as-is (no changes)

- `PlacementScorer` — picks the site. Footprint varies per variant.
- No settlement pipeline stages used — cave entrance stamps the map directly.

### Class interface

```cpp
class CaveEntranceGenerator {
public:
    // Returns placement footprint on success, empty Rect on failure
    // (no valid variant for biome, no cliff found, placement rejected).
    Rect generate(TileMap& map,
                  const TerrainChannels& channels,
                  const MapProperties& props,
                  std::mt19937& rng) const;
};
```

Matches the `RuinGenerator` / `CrashedShipGenerator` pattern: single public method, all variant-specific logic private to the .cpp.

---

## 2. Variant Specifications

| Spec | Natural Cave | Abandoned Mine | Ancient Excavation |
|---|---|---|---|
| **Footprint** | 16×12 | 22×16 | 30×22 |
| **Biome filter** | Mountains / Rocky / Volcanic only | Mountains / Rocky / Volcanic only | Any biome |
| **Requires cliff** | Yes | Yes | No |
| **Wall material** | `Tile::Wall` (natural rock) | `StructuralWall` + glyph 2 (wood supports), glyph 0 (metal rails) | `StructuralWall` + glyph 1 (stone) |
| **Orientation** | Derived from cliff hit | Derived from cliff hit | Fixed/random |
| **Fixtures** | 3–5 Debris near mouth | 2 Crate + 1 Conduit + 3–5 Debris | 1 Console + 1 BookCabinet + 2–4 Debris |
| **Portal** | At mouth interior | Inside the pit | At the bottom of the visible steps |

### `CaveVariantSpec` struct

```cpp
struct CaveVariantSpec {
    CaveVariant variant;
    const char* name;

    int foot_w, foot_h;                 // placement footprint
    bool requires_cliff;                // biome-filtered if true
    int debris_min, debris_max;         // scatter near mouth
    std::vector<FixtureType> fixtures;  // extra fixtures beyond debris
};
```

Three constants at file scope: `kSpecNaturalCave`, `kSpecAbandonedMine`, `kSpecAncientExcavation`.

---

## 3. Variant Selection

### Lore-weighted + biome-filtered roll

```cpp
CaveVariant pick_cave_variant(int lore_tier, Biome b, std::mt19937& rng) {
    bool cliff_biome = (b == Biome::Mountains || b == Biome::Rocky ||
                        b == Biome::Volcanic);
    int r = static_cast<int>(rng() % 100);

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

`CaveVariant::None` means "fail this POI" — the generator returns an empty `Rect`. In practice this only happens when a cave entrance tile lands in a low-lore flat biome.

### Dev override

If `props.detail_cave_variant` is non-empty, parse the string:

- `"natural"` → `NaturalCave`
- `"mine"` → `AbandonedMine`
- `"excavation"` → `AncientExcavation`

The override skips the biome filter for selection (so you can force a mine in a Forest biome for testing), but **cliff detection still applies** — if the resulting natural/mine variant can't find a cliff, the POI fails.

---

## 4. Cliff Detection

For Natural Cave and Abandoned Mine, after `PlacementScorer` picks a site, the generator scans the footprint interior for a cliff edge — an existing `Tile::Wall` tile with at least one 4-neighbor `Tile::Floor`.

### Algorithm

```cpp
struct CliffHit {
    int wall_x, wall_y;         // the wall tile (where the mouth opens INTO)
    int floor_x, floor_y;       // the adjacent floor tile (outside the mouth)
    enum class Facing { N, S, E, W } mouth_facing;
};

std::optional<CliffHit> find_cliff_edge(const TileMap& map, const Rect& foot) {
    auto in_bounds = [&](int x, int y) {
        return x >= 0 && x < map.width() && y >= 0 && y < map.height();
    };
    static constexpr int dxs[] = { 0,  0, -1, 1};
    static constexpr int dys[] = {-1,  1,  0, 0};

    for (int y = foot.y; y < foot.y + foot.h; ++y) {
        for (int x = foot.x; x < foot.x + foot.w; ++x) {
            if (!in_bounds(x, y)) continue;
            if (map.get(x, y) != Tile::Wall) continue;

            // Must be part of a cliff cluster (not an isolated rock).
            int wall_neighbors = 0;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dxs[d], ny = y + dys[d];
                if (in_bounds(nx, ny) && map.get(nx, ny) == Tile::Wall)
                    ++wall_neighbors;
            }
            if (wall_neighbors < 2) continue;

            // Must have at least one Floor neighbor (that's the mouth side).
            for (int d = 0; d < 4; ++d) {
                int nx = x + dxs[d], ny = y + dys[d];
                if (!in_bounds(nx, ny)) continue;
                if (map.get(nx, ny) != Tile::Floor) continue;

                CliffHit hit;
                hit.wall_x = x; hit.wall_y = y;
                hit.floor_x = nx; hit.floor_y = ny;
                // Floor direction relative to wall determines mouth facing.
                if (dys[d] < 0) hit.mouth_facing = CliffHit::Facing::N;
                else if (dys[d] > 0) hit.mouth_facing = CliffHit::Facing::S;
                else if (dxs[d] < 0) hit.mouth_facing = CliffHit::Facing::W;
                else hit.mouth_facing = CliffHit::Facing::E;
                return hit;
            }
        }
    }
    return std::nullopt;
}
```

Returns the **first** valid cliff edge found, which biases toward the top-left of the footprint. This is acceptable — the cliff is just "somewhere in the area", and random orientation of the main map already provides visual variety.

If no cliff edge is found, the generator returns an empty `Rect` — consistent with fail-gracefully policy.

### Ancient Excavation — no cliff needed

Ancient excavation skips cliff detection entirely. It stamps on the center of the placement footprint regardless of surrounding terrain.

---

## 5. Stamp Pipeline

`CaveEntranceGenerator::generate()` runs:

1. **Pick variant** — dev override or lore-weighted/biome-filtered roll. If `None`, return empty rect.
2. **Score placement** — `PlacementScorer::score()` with the variant's footprint. If invalid → empty rect.
3. **Cliff detection** (NaturalCave, AbandonedMine only) — scan the footprint for a cliff edge. If none → empty rect. For AncientExcavation, skip and use the footprint center.
4. **Stamp variant-specific content** (see Section 6).
5. **Place fixtures** per the variant's fixture list.
6. **Place Portal** at the variant-specific position.
7. Return the placement footprint.

---

## 6. Variant Stamp Logic

### Natural Cave (16×12)

- Stamp a noise-shaped rocky outcrop extending from the cliff hit into the footprint. Use the legacy ellipse-with-noise approach: distance-from-center normalized by half-extents (8, 6), with a noise term, threshold ~0.85, edge-weathered at dist > 0.6 with 80% wall probability.
- Carve a 3-wide cave mouth corridor from the outer edge of the outcrop to the cliff wall. The corridor walks from the mouth-facing side of the outcrop to `(wall_x, wall_y)`.
- Place `Tile::Portal` at `(wall_x, wall_y)` — the cave mouth interior, one step into the cliff wall from the detected cliff hit.
- Scatter 3–5 `Debris` fixtures in a small radius outside the mouth (on the `floor_x/floor_y` side).
- Place 6–10 small boulders around the outcrop as 1–2 tile `Tile::Wall` clusters, rejection-sampled to avoid the outcrop interior and the cave mouth corridor.

### Abandoned Mine (22×16)

- Stamp a rectangular pit ~8×6 at the cliff face, with the long edge parallel to the cliff. The pit's interior is cleared to `Tile::Floor`; the perimeter is `StructuralWall` with glyph 0 (metal).
- Wooden support beams: 2–4 additional `StructuralWall` tiles with glyph 2 (wood) flanking the pit entrance on the cliff-facing side. These visually read as beams bracing the mine mouth.
- Short mine cart rails: a 6-tile line of `Tile::Floor` leading out from the pit entrance in the direction **away** from the cliff (using `mouth_facing`). Hint the rails via a `Conduit` fixture every 2 tiles along the line.
- Place `Tile::Portal` inside the pit (at `(wall_x, wall_y)`, one step into the cliff wall).
- Place 2 `Crate` fixtures + 1 `Conduit` fixture inside/beside the pit on random floor tiles.
- Scatter 3–5 `Debris` fixtures around the pit entrance.

### Ancient Excavation (30×22)

- Stamp a stone plaza: rectangular `StructuralWall` outline ~14×10, centered on the footprint, with glyph 1 (stone/concrete). Interior cleared to `Tile::IndoorFloor`. ~70% perimeter coverage to look weathered.
- Four corner pillar fragments: 2×2 `StructuralWall` clusters at each corner of the plaza, with glyph 1, also at 70% coverage.
- Central pit: a 5×3 cutout in the middle of the plaza where stone steps descend. The pit tiles are set to `Tile::IndoorFloor` to hint at "there's an ancient stone floor below this level."
- Place `Tile::Portal` at the center of the pit.
- Place 1 `Console` + 1 `BookCabinet` fixture inside the plaza on random interior floor tiles (ancient precursor terminals left behind).
- Scatter 2–4 `Debris` fixtures within the plaza.
- Place 6–10 boulders scattered outside the plaza (natural terrain reclaiming the site).

---

## 7. MapProperties Addition

Add to `include/astra/map_properties.h`:

```cpp
// Dev override: force a specific cave entrance variant.
// Empty = auto (lore-weighted + biome-filtered).
// Values: "natural", "mine", "excavation".
std::string detail_cave_variant;
```

The generator reads this once at the top of `generate()`:

```cpp
CaveVariant variant;
if (props.detail_cave_variant == "natural") {
    variant = CaveVariant::NaturalCave;
} else if (props.detail_cave_variant == "mine") {
    variant = CaveVariant::AbandonedMine;
} else if (props.detail_cave_variant == "excavation") {
    variant = CaveVariant::AncientExcavation;
} else {
    variant = pick_cave_variant(props.lore_tier, props.biome, rng);
}
if (variant == CaveVariant::None) return {};
```

---

## 8. Dev Console Integration

### `src/dev_console.cpp`

Extend the `biome_test` arg parser:

```cpp
} else if (args[i] == "cave" || args[i] == "cave_entrance") {
    poi_type = "cave";
} else if (args[i] == "natural" || args[i] == "mine" || args[i] == "excavation") {
    if (poi_type.empty()) poi_type = "cave";
    poi_style = args[i];
}
```

Extend the help text:

```
biome_test <biome> ... [cave [natural|mine|excavation]]
  cave: dungeon entrance — natural cave, abandoned mine, or ancient excavation
```

Extend the success message:

```cpp
} else if (poi_type == "cave") {
    msg += " + cave entrance";
    if (!poi_style.empty()) msg += " (" + poi_style + ")";
}
```

### `src/game.cpp` `dev_command_biome_test()`

```cpp
} else if (poi_type == "cave") {
    props.detail_has_poi = true;
    props.detail_poi_type = Tile::OW_CaveEntrance;
    props.detail_cave_variant = poi_style;  // "" / natural / mine / excavation
    props.lore_tier = 1;
}
```

Location name:

```cpp
} else if (poi_type == "cave") {
    loc_name += " + Cave Entrance";
    if (!poi_style.empty()) {
        std::string s = poi_style;
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        loc_name += " (" + s + ")";
    }
}
```

No NPC spawning — cave entrances are empty surface POIs.

---

## 9. `poi_phase.cpp` Dispatch

Remove `OW_CaveEntrance` from the stub list and add:

```cpp
if (props.detail_poi_type == Tile::OW_CaveEntrance) {
    CaveEntranceGenerator cave_gen;
    return cave_gen.generate(map, channels, props, rng);
}
```

The generator owns everything — no terrain mods, no building generator calls, no downstream stages. It stamps the map directly and returns the placement footprint.

---

## 10. Testing & Verification

Manual verification checklist:

- [ ] `biome_test mountains cave natural` produces a cave mouth embedded in a cliff wall.
- [ ] `biome_test mountains cave mine` produces a mine pit with wooden supports and mine cart rails leading out.
- [ ] `biome_test mountains cave excavation` produces a stone plaza with 4 corner pillars and a central pit.
- [ ] `biome_test mountains cave` picks a variant via lore weighting.
- [ ] `biome_test grassland cave natural` fails (no POI) — no cliffs in grassland.
- [ ] `biome_test grassland cave excavation` succeeds — excavation doesn't need cliffs.
- [ ] `biome_test grassland cave` with `lore_tier=0` fails (no POI for low-lore flat biomes).
- [ ] `Tile::Portal` is placed in every successful variant and descends to a dungeon when used.
- [ ] `find_cliff_edge` correctly orients the mouth facing — natural cave tested with cliffs on all 4 sides of the footprint.

---

## 11. Follow-ups (Roadmap)

Added to `docs/roadmap.md`:

- [x] Cave Entrance POI generator (natural cave / abandoned mine / ancient excavation variants, cliff-embedded placement)
- [ ] **Cave entrance dungeon theming** — dungeon content matches the variant (natural cave → tunnel cave generator, mine → structured mine layout, excavation → ancient temple layout)
- [ ] **Additional cave variants** — flooded cave, sealed vault entrance, collapsed shaft
- [ ] **Ice cave variants** — frozen entrances with crystal fixtures in Ice biome
- [ ] **Cave entrance persistent state** — track whether the player has cleared the dungeon below

---

## 12. Out of Scope

- Dungeon content below the portal (handled by existing `TunnelCaveGenerator` / `OpenCaveGenerator`).
- Enemy spawning near the entrance — caves/mines are quiet surface POIs.
- Persistent "already discovered" flags.
- Cave variants beyond the three (natural / mine / excavation).
- Biome-specific cave material theming beyond the built-in variant differences.
- Flooding, collapse, or other hazards near the entrance.
- Readable content on the ancient excavation's Console / BookCabinet fixtures.
