# Outpost POI — Design Spec

**Date:** 2026-04-10
**Status:** Draft
**Scope:** Phase 6 of `2026-04-07-detail-map-generation-v2-design.md` — implement the Outpost POI stamp.

## Context

Phase 6 of the detail map v2 rewrite introduces POIs. Settlements and Ruins are implemented; Outpost, Crashed Ship, Cave Entrance, Landing Pad, Beacon, and Megastructure are still stubbed in `poi_phase.cpp`. This spec covers the Outpost stamp only.

The legacy `detail_map_generator.cpp:1698–1936` had an outpost generator (military compound with perimeter, guard towers, main building + shed, N/S gates). That generator is not reused — we rebuild on top of the v2 architecture (`PlacementScorer` → `*Planner` → `BuildingGenerator` → `PathRouter` → `PerimeterBuilder` → `ExteriorDecorator`).

## Concept

An outpost is a **small but alive** fenced fort with **one** main building inside the fence and **2–4 tent shelters outside** the fence forming a traveler/camp ring. It exists for trade, shelter, quest handoff, and future hostile encounters. Flavor variants (forward base, refuge, scoundrel hideout, traveler rest) are out of scope for this iteration — one generic outpost type ships first, variants layer on top later.

### Visual signature

- Clean rectangular palisade fence, ~70% coverage (gaps visible), one main gate.
- Single ~10×7 building centered inside.
- 2–4 small tent shelters scattered in the outer ring around the fence.
- Campfire clusters (`CampStove` + `Bench`) near tents.

This reads distinctly from settlements (which have multiple buildings in a grid) and from ruins (which are dead megastructure wall networks).

## Decisions Summary

| Decision | Choice | Rationale |
|---|---|---|
| Variants | Single generic type | Start simple; retrofit variants via data-driven kinds later |
| Architecture | New `OutpostPlanner` class parallel to `SettlementPlanner` | Matches `RuinGenerator` separation; reuses downstream infra |
| Shape | Rectangular fence | Clean fort silhouette; reliable stamping |
| Total footprint | 40×30 (25×18 fenced core + ~7 tile outer tent ring) | Smaller than settlements, fits all biomes |
| Fence material | Biome-dependent glyph override | Free visual variety without civ coupling |
| Fence coverage | ~70% (`CivStyle.decay = 0.3f`) | Makeshift feel, reuses existing perimeter logic |
| Gate count | 1 (random side) | Reinforces fort feel; simpler path routing |
| Main building interior | `FurniturePalette` via new `BuildingType::OutpostMain` | Consistent with settlement infra |
| Civ theming | `select_civ_style(props)` like settlements | Free lore reactivity, near-zero extra code |
| Biome validity | All biomes including Aquatic | `PlacementScorer` rejects bad sites |
| Dungeon portal | None (roadmap note for future ~20% chance) | Outposts are surface POIs for now |

---

## 1. Architecture

### New files

| File | Responsibility |
|---|---|
| `include/astra/outpost_planner.h` | `OutpostPlanner` class interface, exposing `plan()` and `post_stamp()`. No new plan type — outposts emit `SettlementPlan` so downstream stages don't branch. |
| `src/generators/outpost_planner.cpp` | Plans the outpost: fence rect, main building rect with door side, gate placement, paths. Returns a `SettlementPlan` via `plan()`. Also hand-stamps tents, campfires, and fence glyph overrides via `post_stamp()`. Private biome-material lookup lives inside this file. |

### Modified files

| File | Change |
|---|---|
| `include/astra/settlement_types.h` | Add `BuildingType::OutpostMain` enum value. |
| `src/generators/settlement_planner.cpp` | Add `size_range()` case for `OutpostMain` (fixed 10×7). |
| `src/generators/furniture_palettes.cpp` | Add palette case for `OutpostMain` (console anchor, table+bench, bunks, crates, campstove). |
| `src/generators/poi_phase.cpp` | Remove `OW_Outpost` from the stub list; add dispatch branch to `OutpostPlanner`. |
| `CMakeLists.txt` | Add `src/generators/outpost_planner.cpp` to `ASTRA_SOURCES`. |
| `docs/roadmap.md` | Check the Outpost POI box; add follow-up entries. |

### Reused as-is (no changes)

- `PlacementScorer` — picks the 40×30 site on the detail map.
- `BuildingGenerator` — stamps the main building only. Tents are **not** routed through `BuildingGenerator` because a 3×2 footprint has zero interior tiles (all 6 tiles are on the shape edge), so wall/floor separation and window placement don't work at that size.
- `PerimeterBuilder` — stamps the fence. Respects `CivStyle.decay` for coverage, applies organic noise offsets (-1/+1 tiles) to the wall line, and handles the gate as a 3×3 opening zone. Avoids the auto 3×3 corner towers by using a `CivStyle.name != "Advanced"`.
- `PathRouter` — draws the gate-to-door path and the gate-to-wilderness outward stub.
- `ExteriorDecorator` — runs on the footprint; outpost-specific campfire fixtures are added directly to the plan as explicit placements so the decorator doesn't have to know about outposts.

### Hand-stamped tents

`OutpostPlanner` stamps tents directly on the `TileMap` as a post-step of its own (not via a `BuildingSpec`). Each tent is a 3×2 footprint:

- 5 perimeter tiles become `Tile::Wall` with a biome-specific glyph override (tent canvas color).
- 1 perimeter tile on the side facing the fence becomes `Tile::Floor` with a `FixtureType::Door` — the tent entrance.
- The single interior tile is `Tile::Floor`.

Because tents are hand-stamped, no `BuildingType::Tent` enum, no tent size range, no tent furniture palette case. This keeps the change set smaller and avoids polluting the furniture/size infrastructure with a special case it wasn't designed for.

---

## 2. Layout & Stamp Pipeline

### Footprint

- Total footprint: **40×30** on the 360×150 detail map.
- Fenced core: **25×18** centered inside the footprint.
- Outer ring: ~7 tiles on each side — tent and campfire scatter zone.
- Main building: **10×7** centered inside the fence.

### Stamp sequence

`OutpostPlanner` has two phases:

1. **Plan phase** — `plan()` populates a `SettlementPlan` with terrain mods, the main building spec, the perimeter spec, and path specs. This is what gets passed back to `poi_phase.cpp` to drive the standard downstream stages.
2. **Post-stamp phase** — after `poi_phase.cpp` runs `BuildingGenerator` / `PathRouter` / `PerimeterBuilder` / `ExteriorDecorator`, the planner's `post_stamp()` method runs to hand-stamp the tents, place exterior campfire clusters, and apply fence glyph overrides. `SettlementPlan` has no generic "loose fixture" vector, so these operations live in a dedicated method on the planner rather than being shoehorned into the plan.

### Elements of the `SettlementPlan` (plan phase)

1. **Terrain clear** — one `TerrainMod::Clear` over the full 40×30 footprint. Removes natural walls and structure mask, lets the compositor show floor underneath. No elevation changes.
2. **Main building** — one `BuildingSpec { type=OutpostMain, 10×7, centered }`. Door side chosen to face whichever gate side was picked. Orientation sets the `BuildingShape::door_positions` on the correct wall.
3. **Fence perimeter** — one `PerimeterSpec { bounds = fenced core rect, gate_positions = [one gate] }`. Gate side is chosen randomly (N/S/E/W); gate center is placed at the mid-point of that side. `CivStyle.decay` is set to `0.3f` for ~70% coverage and the style name is kept non-"Advanced" to skip auto corner towers.
4. **Paths** — two `PathSpec` entries:
   - Gate center → main building door (3-wide indoor floor). `PathRouter` handles L-shaped routing.
   - Gate center → nearest footprint edge, ~6 tiles outward (3-wide). Connects the outpost to the surrounding wilderness visually.

### Post-stamp operations (post_stamp phase)

5. **Hand-stamped tents** — 2–4 tents placed in the outer ring directly on the `TileMap`. Constraints:
   - Minimum 2-tile gap from the fence on all sides.
   - Never overlap each other or existing structures (rejection sampling, up to 10 attempts per tent).
   - Avoid the gate outward path corridor.
   - Door on the side facing the fence (inward).
   - Each tent footprint is 3×2: 5 perimeter tiles become `Tile::Wall` with a biome glyph override, 1 perimeter tile becomes a floor + `FixtureType::Door`, the 1 interior tile becomes `Tile::Floor`.
6. **Campfire fixture clusters** — 1–2 explicit `{CampStove, Bench, Bench}` triples placed at hand-picked positions near tents in the outer ring, directly on the map. Simple fixture placement, no pathing.
7. **Fence glyph override** — walks the fence tiles inside the perimeter bounds and calls `map.set_glyph_override(x, y, glyph_id)` using the biome material lookup. This skips tiles that are floors (gaps) or gates.

### Path to door orientation

The door side of the main building depends on the gate side:

- Gate N → door N (top of building)
- Gate S → door S (bottom)
- Gate E → door E (right wall)
- Gate W → door W (left wall)

The door tile is placed at the building wall mid-point on the chosen side. `PathRouter` routes between gate and door.

---

## 3. Data: BuildingTypes, Palettes, Fence Materials

### New `BuildingType` enum value

In `include/astra/settlement_types.h`:

```cpp
enum class BuildingType : uint8_t {
    MainHall,
    Market,
    Dwelling,
    Distillery,
    Lookout,
    Workshop,
    Storage,

    // Ruin types
    Temple, Vault, GreatHall, Archive, Observatory,

    // Outpost main building
    OutpostMain,
};
```

### Size range

In `src/generators/settlement_planner.cpp`:

```cpp
case BuildingType::OutpostMain: return {10, 10, 7, 7};
```

### Furniture palette

In `src/generators/furniture_palettes.cpp`:

```cpp
case BuildingType::OutpostMain:
    pal.groups = {
        {PlacementRule::Anchor,      FixtureType::Console,  FixtureType::Table, 1, 1, 1.0f},  // quest giver anchor
        {PlacementRule::TableSet,    FixtureType::Table,    FixtureType::Bench, 1, 1, 1.0f},  // trader + seating
        {PlacementRule::WallUniform, FixtureType::Bunk,     FixtureType::Table, 1, 2, 1.0f},  // resting
        {PlacementRule::Corner,      style.storage,         FixtureType::Table, 1, 2, 1.0f},  // loot/quartermaster
        {PlacementRule::WallUniform, style.cooking,         FixtureType::Table, 1, 1, 0.8f},  // campstove
    };
    break;
```

The `style.storage` and `style.cooking` entries come from `CivStyle` (already selected via `select_civ_style(props)` in `OutpostPlanner::plan()`), so an outpost in a high-lore zone automatically uses advanced fixtures, a ruined-civ outpost uses decayed ones, etc.

This palette satisfies `spawn_outpost_npcs()`'s expected anchors (Console, Bunk, Crate) in `src/npc_spawner.cpp`, so NPC spawning works without changes.

### Biome fence material lookup

New helper in `src/generators/outpost_planner.cpp`:

```cpp
struct FenceMaterial { int glyph_id; };

FenceMaterial fence_material_for_biome(Biome b) {
    switch (b) {
        case Biome::Forest:
        case Biome::Jungle:
        case Biome::Grassland:      return {2};  // wood
        case Biome::Rocky:
        case Biome::Mountains:      return {1};  // stacked stone (concrete slot)
        case Biome::Sandy:
        case Biome::Volcanic:       return {3};  // salvage plating
        case Biome::Ice:
        case Biome::Crystal:        return {0};  // ice/metal blocks
        case Biome::Swamp:
        case Biome::Marsh:
        case Biome::Fungal:         return {2};  // wood
        case Biome::Aquatic:        return {3};  // salvage (floating platform feel)
        case Biome::ScarredScorched:
        case Biome::ScarredGlassed: return {3};  // salvage
        default:                    return {2};
    }
}
```

Glyph IDs 0–3 correspond to the existing `set_glyph_override()` slot convention used by the legacy outpost code at `detail_map_generator.cpp:1840` (metal / concrete / wood / salvage). Applied as a post-pass on fence tiles after `PerimeterBuilder` runs.

### Outpost CivStyle construction

`OutpostPlanner::plan()` calls `select_civ_style(props)` to get the base civ style, then copies it and overrides:

```cpp
CivStyle style = select_civ_style(props);
style.decay = 0.3f;         // ~70% fence coverage
style.perimeter_wall = Tile::Wall;  // use basic Wall so glyph override post-pass looks right
// Name stays non-"Advanced" (frontier/ruined/scoundrel etc.) to skip PerimeterBuilder's auto corner towers.
// If select_civ_style returned "Advanced", we overwrite the name to "Frontier" for outpost purposes.
if (style.name == "Advanced") style.name = "Frontier";
```

This gives us a makeshift-looking fence regardless of lore tier while preserving interior civ theming for the main building.

---

## 4. Placement & Scoring

`OutpostPlanner::plan()` receives a `PlacementResult` from `PlacementScorer` (called in `poi_phase.cpp` before the planner, exactly as for settlements). The scorer is asked for a 40×30 footprint; it rejects sites that don't clear terrain. If placement fails, `poi_phase` returns an empty rect (no POI stamped) — matches settlement behavior.

No new scoring logic is needed. Aquatic biome outposts succeed when the scorer finds a dry island within its search; otherwise they simply don't spawn.

---

## 5. Integration Flow in `poi_phase.cpp`

```cpp
if (props.detail_poi_type == Tile::OW_Outpost) {
    int foot_w = 40, foot_h = 30;

    PlacementScorer scorer;
    auto placement = scorer.score(channels, map, foot_w, foot_h);
    if (!placement.valid) return {};

    TerrainChannels mutable_channels = channels;

    OutpostPlanner planner;
    auto plan = planner.plan(placement, mutable_channels, map, props, rng);

    // Apply terrain mods (Clear only for outposts)
    for (const auto& mod : plan.terrain_mods) { /* same loop as settlement branch */ }

    BuildingGenerator builder;
    for (const auto& spec : plan.buildings) {
        builder.generate(map, spec, plan.style, rng);
    }

    PathRouter router;
    router.route(map, plan);

    PerimeterBuilder perimeter;
    perimeter.build(map, plan, rng);

    ExteriorDecorator decorator;
    decorator.decorate(map, plan, rng);

    // Post-stamp: tents, campfires, fence glyph overrides
    planner.post_stamp(map, plan, props.biome, rng);

    return placement.footprint;
}
```

The block mirrors the settlement branch exactly except for the footprint size, planner class, and the `post_stamp()` call at the end.

---

## 6. Dev Testing

The existing dev command `dev_command_warp_stamp(Tile::OW_Outpost)` already exists and is wired into the dialog POI stamp test (`dialog_manager.cpp:735–807`). No dev tooling changes needed — selecting "Outpost" from the POI test menu will exercise `OutpostPlanner` once wired.

---

## 7. Testing & Verification

Manual verification checklist (no automated tests for POI visuals):

- [ ] `biome_test` / warp-stamp menu produces an outpost in every biome.
- [ ] Fence reads as ~70% coverage with biome-appropriate material.
- [ ] Main building has one door facing the gate; `PathRouter` connects them.
- [ ] 2–4 tents appear outside the fence in the outer ring, not overlapping.
- [ ] `CampStove` + `Bench` clusters appear near tents.
- [ ] Main building interior shows Console, Table+Bench, Bunks, Crates per palette.
- [ ] `spawn_outpost_npcs()` successfully finds the expected fixture anchors.
- [ ] Outpost does not overlap with adjacent biome transitions or water edges visibly.
- [ ] Aquatic outposts either spawn on a dry patch or don't spawn at all (no ocean fences).

---

## 8. Follow-ups (Roadmap)

Added to `docs/roadmap.md`:

- [x] Outpost POI generator (fenced fort, single building, exterior tents)
- [ ] Optional dungeon portal under outposts (~20% chance) — rare "sits on top of something" variant
- [ ] Outpost kind variants (forward base / refuge / scoundrel hideout / traveler camp) driven by seed or lore
- [ ] Outpost reputation/hostility system — hostile outposts spawn combatants instead of traders

## 9. Out of Scope

- NPC spawning logic beyond anchoring (already exists in `spawn_outpost_npcs`).
- Outpost variants and their visual/NPC differences.
- Reputation, hostility, and combat encounters at outposts.
- Underground dungeon generation beneath outposts.
- Custom outpost-specific fixture types (e.g., watchtower, barbed wire, fire pit).
- Multi-overworld-tile outposts.
