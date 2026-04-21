# Dungeon Generator Factory Refactor — Follow-up Plan

**Date:** 2026-04-21
**Status:** Deferred from the Conclave Archive slice (2026-04-21).
**Trigger:** The workaround in commit `df7893d` (`src/generators/dungeon_level.cpp`) bypasses the abstract `create_generator()` factory and instantiates `RuinGenerator` directly so that `spec.civ_name` can reach `select_civ`. This is correct for the Archive, but it couples `generate_dungeon_level` to a concrete generator class and can't generalise to other dungeon kinds (e.g. station-style dungeons, cave-style dungeons) that a future `DungeonRecipe` might need.

---

## Problem

`include/astra/map_generator.h` defines an abstract `MapGenerator` interface:

```cpp
class MapGenerator {
public:
    virtual ~MapGenerator() = default;
    virtual void generate(TileMap& map, const MapProperties& props,
                          uint32_t seed) const = 0;
};
```

`src/map_generator.cpp` exposes a factory:

```cpp
std::unique_ptr<MapGenerator> create_generator(MapType type);
std::unique_ptr<MapGenerator> create_generator(MapType type,
                                               const MapProperties& props);
```

Concrete implementations (`derelict_station_generator.cpp`, `starship_generator.cpp`, `open_cave_generator.cpp`, etc.) each extend `MapGenerator::generate(map, props, seed)`. Their own config/tuning comes from fields on `MapProperties`.

**But** `RuinGenerator` (in `src/generators/ruin_generator.cpp`) is a special case:

- It's not registered in `create_generator`.
- Its `generate` signature is **different**:
  ```cpp
  Rect RuinGenerator::generate(TileMap& map,
                               const TerrainChannels& channels,
                               const MapProperties& props,
                               std::mt19937& rng,
                               const std::string& civ_name = "") const;
  ```
- It takes `TerrainChannels`, a `std::mt19937&` instead of `uint32_t seed`, and a civ-name string.

Today it's invoked from two places:

1. `src/generators/poi_phase.cpp:21` — as a POI substamp inside a detail map.
2. `src/generators/dungeon_level.cpp` (new, Archive slice) — for stand-alone dungeon levels.

Neither path flows through `create_generator`. That's the coupling problem.

### Why the current Archive workaround works (for now)

`generate_dungeon_level(..., const DungeonRecipe&, int depth, uint32_t seed, ...)` reads `spec.civ_name` from the recipe and calls `RuinGenerator` directly. The Archive always wants a ruin, so this is fine for the slice. But any future recipe with `civ_name = ""` (or a non-ruin dungeon kind) forces `dungeon_level.cpp` to branch on "what kind of generator should I use?" — which is exactly what the abstract factory should own.

---

## Goal

Make `create_generator` (or a more flexible successor) route civ-name / recipe hints to any concrete generator without callers needing to know the concrete class. Callers pass configuration via `MapProperties` (or a small recipe-hint struct); the factory owns dispatch.

---

## Design sketch

Two tracks, pick whichever fits:

### Track A — Push everything through `MapProperties`

- Promote `TerrainChannels` from a `RuinGenerator::generate` parameter to a `MapProperties` member (optional, empty by default). Ruin generator reads `props.terrain_channels` instead of a separate arg.
- Replace `RuinGenerator::generate(map, channels, props, rng, civ_name)` with the abstract `MapGenerator::generate(map, props, seed)` signature. `civ_name` comes from `props.detail_ruin_civ` (already exists) or a new `props.recipe_civ_name`. The internal `std::mt19937` is built from `seed` inside `generate`.
- Register `RuinGenerator` with `create_generator(MapType::Dungeon)` — adding a new `MapType::Dungeon` enumerator distinct from `MapType::DerelictStation`. (Alternatively: route `MapType::DerelictStation` to ruin when a recipe hint is present and derelict otherwise.)
- `generate_dungeon_level` replaces the direct `RuinGenerator` call with `create_generator(MapType::Dungeon)->generate(map, props_with_civ_and_channels, seed)`.

**Pros:** no new abstraction surface. Uses the interface that already exists.
**Cons:** `MapProperties` continues to grow as a catch-all. `TerrainChannels` being optional on a struct that's passed by-value is a mild cost (empty channels are cheap but non-zero).

### Track B — Add a `DungeonRecipe`-aware factory

- Introduce `create_dungeon_generator(const DungeonLevelSpec&)` in `map_generator.h`. It returns `unique_ptr<MapGenerator>` and internally dispatches on `spec.civ_name` or a new `spec.dungeon_kind` enum:
  ```cpp
  enum class DungeonKind : uint8_t { Ruin, DerelictStation, Cave };
  ```
- `DungeonLevelSpec` grows a `DungeonKind kind = DungeonKind::Ruin;` field.
- `generate_dungeon_level` calls `create_dungeon_generator(spec)->generate(map, props, seed)` and no longer hard-codes `RuinGenerator`.
- The ruin path inside this factory still has to do the `TerrainChannels` + `civ_name` wiring — but that wiring is isolated behind the factory, not leaking into callers.

**Pros:** explicit vocabulary. The recipe tells the factory the kind; the factory owns concrete-class knowledge.
**Cons:** adds a second factory shape alongside the existing `create_generator`.

### Recommended approach

**Track A.** Fewer moving parts, and the `MapProperties` fields for `civ_name` / `ruin_decay` already exist — they just need to be consumed by the ruin generator's standard `generate(map, props, seed)` override instead of the current custom signature. `TerrainChannels` as an optional field is a small nudge in return for reunifying the generator interface.

Track B's `DungeonKind` enum can still be added later if we start differentiating dungeon kinds and the recipe needs richer dispatch.

---

## Affected files (estimated)

**Track A:**

- `include/astra/map_properties.h` — add `TerrainChannels terrain_channels;` (optional), and confirm `detail_ruin_civ` / `detail_ruin_decay` semantics.
- `include/astra/ruin_generator.h` — change `generate` signature to match `MapGenerator` interface; keep `RuinGenerator : public MapGenerator`.
- `src/generators/ruin_generator.cpp` — adapt body: build `std::mt19937` from `seed`; read channels from props; read civ from `props.detail_ruin_civ`; all existing behavior preserved.
- `src/map_generator.cpp` — register `RuinGenerator` under a new or existing `MapType`. Decide whether to introduce `MapType::Dungeon` or reuse `MapType::DerelictStation` with a recipe-kind hint.
- `src/generators/dungeon_level.cpp` — revert the direct `RuinGenerator` instantiation; go back to `create_generator(...)->generate(map, props, seed)`.
- `src/generators/poi_phase.cpp` — same change to the existing call site (the in-detail-map ruin substamp). Currently it does `RuinGenerator ruin_gen; ruin_gen.generate(...)` inline.

**Track B (if preferred later):** add `create_dungeon_generator` + `DungeonKind` + spec field.

---

## Test plan

No test framework — validation is build + in-game smoke:

1. `cmake --build build -j` clean.
2. Conclave Archive still renders with Precursor aesthetic (cyan + violet walls) across all three levels, with decay gradient L1 heavy → L3 pristine.
3. `OW_Ruins` POIs on overworlds (natural, quest-independent) still render and enter correctly — `poi_phase.cpp`'s ruin substamp path is the second caller.
4. Missing Hauler dungeon still loads (that quest uses `OW_CaveEntrance`, routed through the cave generator — should be unaffected, but confirm).
5. Save/load round-trip of an Archive in progress: descent, ascent, fixture state all preserved.

---

## References

- Current workaround: `src/generators/dungeon_level.cpp` (commit `df7893d`).
- Existing factory: `src/map_generator.cpp:501` (`create_generator`).
- Existing `MapGenerator` interface: `include/astra/map_generator.h`.
- Existing ruin generator: `include/astra/ruin_generator.h`, `src/generators/ruin_generator.cpp`.
- Existing in-detail ruin substamp caller: `src/generators/poi_phase.cpp:20-26`.
- Recipe/spec types: `include/astra/dungeon_recipe.h`.

---

## Out of scope

- Adding new concrete dungeon generators (caves, stations) to the recipe system — that can wait until a second `DungeonRecipe` consumer appears.
- POI-budget-driven multi-level dungeons (Archive is quest-seeded; generalising to non-quest recipes is a separate feature).
- Per-level aesthetic transitions within the same dungeon (Archive already works via per-level `civ_name`).
