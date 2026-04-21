# Dungeon Generator Overhaul — Design

> **Status:** Design approved 2026-04-21. Implementation plan pending (`superpowers:writing-plans` follow-up).
> **Kickoff:** `docs/plans/2026-04-21-dungeon-generator-overhaul-kickoff.md`
> **Scope:** Slice 1 — land the new pipeline with one smoke-test style. Archive migration is a follow-up slice.

---

## Goal

Replace the ad-hoc `generate_dungeon_level(...)` body (currently a thin wrapper around `RuinGenerator`) with a reusable **six-layer pipeline** that handles every dungeon style Astra will want: Precursor ruins, open caves, tunnel caves, derelict stations, flooded temples, etc. Each layer is a small, independently testable unit; each style is a data-driven config that selects which layer variants to run.

Public entry point is unchanged:

```cpp
void generate_dungeon_level(TileMap&, const DungeonRecipe&, int depth,
                            uint32_t seed, std::pair<int,int> entered_from);
```

No caller changes required.

---

## Architecture

### Code layout

```
include/astra/dungeon/
  dungeon_style.h      — DungeonStyle, StyleId, LayoutKind, OverlayKind, StairsStrategy
  level_context.h      — LevelContext POD
  pipeline.h           — dungeon::run(...)
  backdrop.h           — layer 1
  layout.h             — layer 2
  connectivity.h       — layer 3
  overlay.h            — layer 4
  decoration.h         — layer 5
  fixtures.h           — layer 6

src/dungeon/
  style_configs.cpp    — DungeonStyle registry (analog of ruin_civ_configs.cpp)
  pipeline.cpp         — orchestrator, <50 lines
  backdrop.cpp
  layout.cpp
  connectivity.cpp
  overlay.cpp
  decoration.cpp
  fixtures.cpp
```

`src/generators/dungeon_level.cpp` shrinks to a wrapper: resolve `spec.style_id` → `DungeonStyle`, resolve `spec.civ_name` → `CivConfig`, build a `LevelContext`, call `dungeon::run(...)`.

### Data flow

```
DungeonRecipe.levels[depth-1]  (DungeonLevelSpec)
     │
     ├── style_id        ──► DungeonStyle          (style_configs.cpp)
     ├── civ_name        ──► CivConfig             (ruin_civ_configs.cpp)
     └── overlays, fixtures, npc_roles, decay_level, …
                                 │
                                 ▼
                  dungeon::run(map, style, civ, spec, ctx, rng)
                                 │
           ┌─────────────┬───────┴──────┬──────────────┬────────────┬──────────┐
           ▼             ▼              ▼              ▼            ▼          ▼
       backdrop       layout      connectivity     overlay     decoration  fixtures
       (fill rock)   (carve)    (pathfind rooms)  (flooded)   (debris)  (stairs/quest)
```

### Design axes

- **Style** (structural): backdrop material, layout generator, allowed overlays, decoration pack. `DungeonStyle` is a pure data struct.
- **Civ** (aesthetic): palette, furniture prefs, backdrop tint. `CivConfig` from `ruin_civ_configs.cpp` — unchanged. Every dungeon has a civ; caves use a new `"Natural"` sentinel civ.
- **Spec** (per-level): depth, decay level, overlays requested, quest fixtures, NPC roles. `DungeonLevelSpec` — gains two fields, everything else unchanged.

Style and civ are orthogonal: a ruin style works with Precursor, Monolithic, Crystal; a cave style works with Natural or (future) Fungal.

### Platform isolation

All new code is pure game logic — `TileMap`, enums, `std::mt19937`. No renderer includes. No `ASTRA_HAS_SDL` ifdefs. Visual decisions flow through tile enums + `CivConfig` palette + `terminal_theme.cpp`.

---

## Layer contract

### `DungeonStyle` struct

```cpp
enum class StyleId : uint8_t {
    SimpleRoomsAndCorridors,   // slice 1 smoke-test
    PrecursorRuin,             // follow-up slice (Archive migration)
    OpenCave,
    TunnelCave,
    DerelictStation,
};

enum class LayoutKind : uint8_t {
    BSPRooms, OpenCave, TunnelCave, DerelictStationBSP, RuinStamps,
};

enum class OverlayKind : uint8_t {
    None, BattleScarred, Infested, Flooded, Vacuum,
};

enum class StairsStrategy : uint8_t {
    EntryExitRooms,     // distinct regions (BSP, multi-room ruin)
    FurthestPair,       // one big region, stairs at max-distance pair (open cave)
    CorridorEndpoints,  // corridor-only, stairs at spine extrema (tunnel cave)
};

struct DungeonStyle {
    StyleId         id;
    const char*     debug_name;               // "simple_rooms", shown in :dungen
    std::string     backdrop_material;        // "rock","sand","plating","cavern_floor"
    LayoutKind      layout;
    StairsStrategy  stairs_strategy;
    std::vector<OverlayKind> allowed_overlays;
    std::string     decoration_pack;          // "ruin_debris","cave_flora","station_scrap","natural_minimal"
    bool            connectivity_required;    // false for caves, true for BSP
};
```

Registry: `const DungeonStyle& style_config(StyleId);` in `style_configs.cpp`.

### `LevelContext`

```cpp
struct LevelContext {
    int                 depth;
    uint32_t            seed;
    std::pair<int,int>  entered_from;       // from descend_stairs
    int                 entry_region_id = -1; // layer 2 writes, layer 6 reads
    int                 exit_region_id  = -1;
    std::pair<int,int>  stairs_up{-1,-1};    // layer 6 writes
    std::pair<int,int>  stairs_dn{-1,-1};
};
```

### Layer signatures

All in `astra::dungeon` namespace; all free functions.

```cpp
void apply_backdrop     (TileMap&, const DungeonStyle&, const CivConfig&, std::mt19937&);
void apply_layout       (TileMap&, const DungeonStyle&, const CivConfig&, LevelContext&, std::mt19937&);
void apply_connectivity (TileMap&, const DungeonStyle&, LevelContext&, std::mt19937&);
void apply_overlays     (TileMap&, const DungeonStyle&, const DungeonLevelSpec&, std::mt19937&);
void apply_decoration   (TileMap&, const DungeonStyle&, const CivConfig&, std::mt19937&);
void apply_fixtures     (TileMap&, const DungeonStyle&, const DungeonLevelSpec&, LevelContext&, std::mt19937&);
```

Each layer is its own `.cpp` file, ≤~200 lines. Orchestrator calls them in order.

### RNG discipline

Each layer derives a named sub-seed from the base seed:

```
backdrop      seed ^ 0xBDBDBDBDu
layout        seed ^ 0x1A1A1A1Au
connectivity  seed ^ 0xC0FFEE00u
overlays      seed ^ 0x0FEB0FEBu
decoration    seed ^ 0xDEC02011u
fixtures      seed ^ 0xF12F12F1u
```

Prevents any one layer's RNG draws from shifting outputs of another. Adding an overlay won't reshuffle decoration placement.

### Per-layer responsibilities

1. **backdrop** — write an impassable, opaque tile (`Tile::Wall` for all materials in slice 1) to every cell; `map.set_biome(Biome::Dungeon)`.
2. **layout** — dispatch on `style.layout` into existing primitives (`BspGenerator`, `OpenCaveGenerator`, `TunnelCaveGenerator`, `DerelictStationGenerator`, `RuinGenerator`). **Post-condition:** `map.region_count() >= 1` with tagged regions. Writes `ctx.entry_region_id` / `ctx.exit_region_id`.
3. **connectivity** — if `style.connectivity_required`, ensure all rooms reachable via `PathRouter`. No-op otherwise.
4. **overlays** — apply `spec.overlays` filtered against `style.allowed_overlays`; wraps `ruin_stamps.cpp` primitives (`apply_battle_scarred`, `apply_infested`, `apply_flooded`). `Vacuum` is a no-op stub in slice 1.
5. **decoration** — debris, flora, furniture; dispatches on `style.decoration_pack`. Wraps an extracted `apply_decay(map, civ, intensity, rng)` helper from `ruin_decay.cpp`.
6. **fixtures** — stairs (sub-pass 6.i, via `stairs_strategy`), then quest fixtures (6.ii, from `spec.fixtures`), then style-required fixtures (6.iii, schema only in slice 1).

---

## Backdrop & rendering

Belt-and-braces: solid tile fill **and** biome flag.

### Solid tile fill

Layer 1 writes `Tile::Wall` to every cell. The `backdrop_material` string on `DungeonStyle` is informational for slice 1 — future slices may introduce `Tile::Rock`, `Tile::Plating`, etc. Visual differentiation today comes from civ palette, not tile identity.

### `Biome::Dungeon`

- New enum value in `Biome`.
- Layer 1 always calls `map.set_biome(Biome::Dungeon)` (replaces the `Biome::Corroded` the current generator sets).
- `terminal_theme.cpp` is updated: when `biome == Biome::Dungeon`, render `Tile::Empty` as `░` (light block), not the starfield glyph. Civ palette tints the block color.
- Safety net: catches any stray `Tile::Empty` introduced by downstream layers (decay holes, overlay carve-outs) so the starfield-bleed regression cannot return.

### Civ-aware backdrop palette

`CivConfig` gains a `backdrop_tint` field (color reference). Natural civ = neutral grey; Precursor = existing Precursor palette; Industrial = rust; etc. Renderer already resolves ruin tiles via civ palette — we extend the same path.

### `Biome::Corroded` sweep

Before implementation: grep for `Biome::Corroded` usages. Replacing it with `Biome::Dungeon` in `dungeon_level.cpp` must not affect other callers. If any exist, introduce `Biome::Dungeon` alongside, don't replace `Corroded` globally.

---

## Overlays, decoration & civ

### Overlays (layer 4)

`DungeonLevelSpec` gains a requested-overlays field:

```cpp
struct DungeonLevelSpec {
    StyleId                     style_id    = StyleId::SimpleRoomsAndCorridors;  // NEW
    std::string                 civ_name    = "Precursor";
    int                         decay_level = 2;
    int                         enemy_tier  = 1;
    std::vector<std::string>    npc_roles;
    std::vector<PlannedFixture> fixtures;
    std::vector<OverlayKind>    overlays;                                         // NEW
    bool                        is_side_branch = false;
    bool                        is_boss_level  = false;
};
```

Layer 4 behavior:
- For each requested overlay, check against `style.allowed_overlays`. If not allowed, log a warning in dev mode and skip (don't hard-fail — recipes stay portable across styles during iteration).
- Apply in list order. Dispatch to `ruin_stamps.cpp` primitives.
- Overlays mutate tiles only — no fixtures (those are layer 6).

### Decoration (layer 5)

Inputs: `style.decoration_pack` (what kind of stuff), `CivConfig` (how it looks), `spec.decay_level` (how broken).

- Decoration packs are tables in `style_configs.cpp`: furniture kinds, density, decay curve.
- Ruin-style packs wrap an extracted `apply_decay(map, civ, intensity, rng)` helper factored out of `ruin_decay.cpp`. The existing `RuinGenerator` decay call stays intact (used by overworld POIs); the new helper is a parallel entry point for the dungeon pipeline.
- Cave packs use `civ_config.allowed_flora` + mushroom stamps. Natural civ gets a curated sparse-moss pack.
- Respects already-placed tiles: no double-stamping, no overwriting overlay water.

### Civ integration recap

| Layer | Reads civ? | How |
|-------|-----------|-----|
| 1 backdrop | yes | `civ.backdrop_tint` for color |
| 2 layout | no | structural only |
| 3 connectivity | no | structural only |
| 4 overlays | no | water doesn't care who built the place |
| 5 decoration | yes | `civ.furniture_kinds`, `civ.decoration_palette` |
| 6 fixtures | yes | altar/terminal glyph flavor |

### New `"Natural"` civ

Registered in `ruin_civ_configs.cpp` — neutral grey palette, empty furniture list, allows only flora/mushroom stamps. Required for cave styles in future slices.

---

## Fixtures, stairs & region tagging

### Prerequisite — all layouts tag regions

The current bug (`RuinGenerator` doesn't tag regions → region-aware helpers silently fail → distance-based fallbacks) is fixed at the source.

After layer 2:
- **Invariant:** `map.region_count() >= 1` with tagged regions.
- Layouts that don't naturally produce regions (open cave, tunnel cave, ruin stamps) call a shared post-pass utility.
- `region.type` is set meaningfully: `RegionType::Room`, `RegionType::Corridor`, or `RegionType::Cave` (add this value if not already present).
- Layer 2 writes `ctx.entry_region_id` and `ctx.exit_region_id`. For single-region layouts these are equal — stairs strategy handles it.

### Region-tagging utility

`src/generators/room_identifier.cpp` gains a public:

```cpp
// Flood-fill each connected passable component and tag it as one region
// with the given default type. Safety-net for layouts that don't tag.
void tag_connected_components(TileMap&, RegionType default_type);
```

Layouts that need it call it post-carve. Open cave uses `Cave`; ruin-stamp layout uses `Room` (needed for Archive migration in the follow-up slice); tunnel cave uses `Corridor`.

### Layer 6 sub-passes

**6.i Stairs** — dispatched by `style.stairs_strategy`:

- `EntryExitRooms`: preferred path. StairsUp placed in `ctx.entry_region_id`, preferring `ctx.entered_from` if valid and unoccupied. StairsDown placed in `ctx.exit_region_id`. Boss levels suppress StairsDown.
- `FurthestPair`: one big region (open cave). StairsUp near `entered_from`; StairsDown at farthest passable cell.
- `CorridorEndpoints`: corridor-only (tunnel cave). Walk the longest corridor spine; stairs at its two extrema.

**6.ii Quest fixtures** — existing `place_planned_fixtures` logic migrated here and cleaned up. Hints:

- `"back_chamber"` — room region with max distance from `ctx.entry_region_id`, excluding entry/exit.
- `"center"` — region containing map center.
- `"entry_room"` — new: the entry region, not occupied by StairsUp.
- `""` — random open room.
- Fallback: any passable, fixture-free tile.

All hints work across every style because regions are always tagged now.

**6.iii Required fixtures** — style-level altar/terminal/trap placement. **Deferred:** no `required_fixtures` field is added to `DungeonStyle` in slice 1; the sub-pass exists in the pipeline code as a no-op call site marked `TODO: Archive migration`. Adding the field + type catalog happens in the Archive-migration slice where it's actually needed.

---

## Slice 1 scope

### Deliverables

1. `include/astra/dungeon/` headers (7 files).
2. `src/dungeon/` sources: `pipeline.cpp` + six layer files + `style_configs.cpp`.
3. Exactly one style: **`SimpleRoomsAndCorridors`** — BSP rooms, compatible with any civ, `BattleScarred`/`Infested` overlays allowed, `ruin_debris` decoration pack.
4. One new civ: **`"Natural"`** in `ruin_civ_configs.cpp`.
5. `Biome::Dungeon` enum value + terminal theme rendering of `Tile::Empty` as `░` when biome is `Dungeon`.
6. `CivConfig::backdrop_tint` field.
7. Region-tagging utility `tag_connected_components(...)` in `room_identifier.cpp`.
8. Extracted `apply_decay(map, civ, intensity, rng)` helper exposed from `ruin_decay.cpp`.
9. Rewrite of `generate_dungeon_level(...)` as a thin wrapper calling `dungeon::run(...)` — except for the Archive branch below.
10. Dev-console command: `:dungen <style_id> [civ_name]` — generates a dungeon in a scratch map and drops the player into it; for smoke-testing without a recipe.
11. `DungeonLevelSpec` gains `StyleId style_id` + `std::vector<OverlayKind> overlays`.
12. `SAVE_FILE_VERSION` bump. `DREC` section extended:
    - `style_id` (u8)
    - `overlays_len` (u8) + `overlays` (u8[overlays_len])
    - Reader uses version gate — old saves default `style_id = SimpleRoomsAndCorridors`, `overlays = {}`.

### Archive compatibility bridge

Archive recipes today are the sole producer of `DungeonRecipe`s. Auto-assigning `SimpleRoomsAndCorridors` on load would visibly change their look, so slice 1 keeps Archive on the old generator via a per-recipe kind-tag branch:

```cpp
void generate_dungeon_level(...) {
    if (recipe.kind_tag == "conclave_archive") {
        // TODO: remove with Archive migration (separate slice)
        old_generate_dungeon_level_impl(...);   // current body, kept verbatim
        return;
    }
    dungeon::run(...);
}
```

This is the one temporary special-case. The follow-up slice removes the branch after the `PrecursorRuin` style is ready and Archive recipes are updated to use it.

### Explicitly NOT in slice 1

- Conclave Archive migration (separate spec).
- `PrecursorRuin`, `OpenCave`, `TunnelCave`, `DerelictStation` styles (schema supports them; only `SimpleRoomsAndCorridors` ships).
- New `Tile` enum values (`Tile::Rock`, `Tile::Plating`, etc.).
- `Vacuum` overlay implementation.
- Required-fixture type catalog (altar/terminal/trap) — schema only.
- `poi_phase.cpp`'s `OW_PrecursorArchive` overworld POI path — untouched; still calls `RuinGenerator` directly.
- Hidden/secret rooms.
- Procedural recipe generation.
- Multi-level branching graphs.

---

## Testing

No test framework. Validation is `cmake --build build -DDEV=ON -j` + dev-mode smoke.

- `:dungen SimpleRoomsAndCorridors Natural` — generates, renders. Eyeball: connected rooms, both stairs placed, block-char backdrop covers untouched cells, no starfield bleed.
- `:dungen SimpleRoomsAndCorridors Precursor` — same layout, Precursor color tint.
- Descend stairs into Conclave Archive — behavior unchanged (old generator branch).
- `cmake --build build -j`: zero new warnings.

---

## Risks & mitigations

| Risk | Mitigation |
|------|-----------|
| Adding `Biome::Dungeon` breaks `switch (biome)` statements elsewhere | Grep for `Biome::` switches before edit; add `default:` coverage where missing. |
| `room_identifier` post-pass changes region numbering for existing non-dungeon callers | Helper is a new, opt-in function; no existing caller changes behavior. |
| Save-version bump invalidates in-flight playtest saves | Read path is version-gated with defaults; call out in commit message for user awareness. |
| Archive kind-tag branch lingers past follow-up slice | Strong `TODO: remove with Archive migration` at the branch point; follow-up spec named now. |
| Per-layer RNG subseed discipline drifts as layers are added | Document in `pipeline.h` header comment; enforce in code review. |
| `apply_decay` extraction changes behavior of the existing `RuinGenerator` decay path | Extract as a new entry point; keep the existing call signature intact and call the helper from inside it. |

---

## Out of scope (carried over from kickoff)

- Stage 5 Stellar Signal arc.
- Archon Remnants galaxy-wide spawn boost.
- Hidden / secret rooms.
- Procedural dungeon recipes.
- Multi-level branching dungeon graphs.
- Dungeon re-entry / respawn economy.

---

## Follow-up slices (not this spec)

1. **Archive migration.** Build `PrecursorRuin` style; flip Conclave Archive recipes to it; delete the kind-tag branch; delete the old `generate_dungeon_level` body.
2. **Cave styles.** `OpenCave` + `TunnelCave` with Natural civ. Enables cave POIs.
3. **Derelict station style.** Bridges the existing `DerelictStationGenerator` into the pipeline.
4. **Required fixtures catalog.** Altars, terminals, traps as typed fixtures (moves Conclave altars out of quest-fixture hack into structured layer 6.iii).
