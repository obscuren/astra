# Dungeon Generator Overhaul — Kickoff Prompt

> **Purpose:** Start a fresh Claude Code session with enough context to run a brainstorm on redesigning Astra's dungeon generation around a layered pipeline. The current dungeon system shipped with the Conclave Archive quest (Stage 4) but layout quality is poor and the code path is quest-specific rather than reusable.

---

## Paste this into the new session to start

> I want to brainstorm a dungeon generator overhaul for Astra. Read `docs/plans/2026-04-21-dungeon-generator-overhaul-kickoff.md` in full and then run the `superpowers:brainstorming` skill on the design. Don't touch code yet.

---

## The ask

Replace the current ad-hoc dungeon generator (`src/generators/dungeon_level.cpp`, built on top of `RuinGenerator`) with a **layered multi-pass pipeline** that handles every dungeon style we'll want — Precursor ruins, caves, stations under attack, flooded temples, etc. The current Archive dungeons stay functional for now; migrating them to the new system is a **follow-up**.

### The 6-layer pipeline the user wants

Each dungeon style is a stack of passes. Each pass is a small, testable unit.

1. **Backdrop** — fill the map with the base material for this dungeon style.
   _Rock for underground ruins, sand for desert catacombs, station plating for derelicts, cavern floor for open caves, etc. This is what the player sees in "empty" space where no interior has been carved. Today all dungeons fall back to `Tile::Empty` which renders as vacuum starfield — visually wrong for underground._

2. **Layout** — carve the dungeon's topology on top of the backdrop.
   _Style-specific: BSP rooms + corridors, cellular-automata caves, hand-assembled modules, etc. Each layout generator declares what it needs (connectivity guarantees, corridor widths, room-tagging)._

3. **Connectivity** — if the layout style requires it, pathfind between rooms and carve corridors to guarantee reachability.
   _Open caves typically skip this (they're already connected by generator design). BSP dungeons and multi-room ruins require it._

4. **Environment overlay** — large-feature passes that have to make sense for the style.
   _Waterlogged rooms, moisture / humidity zones, lava chambers, vacuum breaches, magical wards, collapsed sections. "Flooded" doesn't belong in a desert catacomb; "lava" doesn't belong in an ice cave. Each style whitelists which overlays are valid._

5. **Variation / decoration** — surface dressing: debris, flora, furniture.
   _Scrap piles and sparks for wrecked stations, plants and mushrooms for caves, inscriptions and altars for Precursor, skeletons / corpse scatter where historically appropriate._

6. **Fixtures** — named placements the game mechanic depends on.
   - **6.i** Quest fixtures (registered via `QuestFixtureDef`, positioned by `placement_hint`).
   - **6.ii** Stairs (up, down) — per-level, guaranteed reachable, positioned per style (back chamber, deepest room, etc.).
   - **6.iii** Any other required fixtures (altars, terminals, traps).

---

## Current state (what exists today)

### Recipe + registry (keep; may be extended)
- `include/astra/dungeon_recipe.h` — `PlannedFixture`, `DungeonLevelSpec`, `DungeonRecipe` structs.
- `WorldManager::dungeon_recipes_` — `std::map<LocationKey, DungeonRecipe>` registry. Accessors `dungeon_recipes()` and `find_dungeon_recipe()`.
- `SaveData::dungeon_recipes` and `save_file.cpp` `DREC` tagged section — full serialization.
- `NavigationData::current_depth` — int field tracking dungeon depth; serialized; anchors save/restore keys.

**Likely changes:** `DungeonLevelSpec` grows a `style` / `kind` field (enum?) that selects the layered-pipeline stack. `civ_name` and `decay_level` become inputs to specific passes rather than baked into one generator call.

### Current generator (replace)
- `src/generators/dungeon_level.cpp`'s `generate_dungeon_level(...)` is the function to replace. It currently:
  - Pre-fills with `Tile::Wall` (backdrop, but crude — `Tile::Empty` escapes in many places).
  - Calls `RuinGenerator` directly with the level's `civ_name` / `decay_level` — this is the layout + decay all fused together.
  - Places StairsUp at `entered_from` or first room region.
  - Places StairsDown in the furthest room if not a boss level.
  - Places planned fixtures via `place_planned_fixtures`.

**Problems:**
- Layout quality is poor (ruins aren't dungeons — rooms often disconnected or non-existent; corridors are broken by design).
- No region tagging by `RuinGenerator`, so every region-aware helper silently fails — the whole game had to work around this (`find_open_spot_far_from`, distance-based placement).
- Backdrop handling is partial — `Tile::Empty` tiles that the generator didn't touch render as starfield unless biome is set carefully.
- No support for environment overlays (flooded, etc.).
- Variation / decoration is bundled into `RuinGenerator`'s decay pass, not separable.

### Call sites to keep working
- `src/game_world.cpp`'s `Game::descend_stairs()` — creates a fresh TileMap, calls the generator, spawns NPCs into `world_.npcs()` afterwards.
- `src/game_world.cpp`'s `Game::ascend_stairs()` — restore-only, doesn't call the generator.
- `src/generators/poi_phase.cpp`'s `OW_PrecursorArchive` branch — uses `RuinGenerator` directly for the surface detail-map ruin (the overworld POI that leads into the dungeon). This is a **separate** path — the "detail map" is not a dungeon level; it's an overworld POI. May or may not want to share infrastructure with the dungeon pipeline.

### Rendering issue flagged by user
Starfield backdrop currently renders any `Tile::Empty` tile when `biome == Biome::Station`. Dungeons set biome to `Biome::Corroded` in the generator but the TileMap's default-constructed biome stays `Station`, so the starfield bleeds through on un-filled cells. Two possible fixes:
- Always `map.set_biome(...)` after constructing a dungeon map.
- Introduce `Biome::Dungeon` and render `Tile::Empty` as a block character (`░`/`▒`/`▓`) when biome is underground.

The user prefers "fill in all 'empty' space with the block character, indicating we're underground". **This is a hard requirement** for the new system.

---

## Reference files

### Layout primitives available
- `src/generators/bsp_generator.cpp` — classic binary-space-partitioning rooms. Used internally by `RuinGenerator` today.
- `src/generators/tunnel_cave_generator.cpp` — tunnel caves.
- `src/generators/open_cave_generator.cpp` — open-area caves (cellular automata).
- `src/generators/derelict_station_generator.cpp` — BSP + station aesthetic + connectivity.
- `src/generators/hub_station_generator.cpp` — multi-zone station (THA).
- `src/generators/room_identifier.cpp` — post-pass that tags regions.
- `src/generators/path_router.cpp` — corridor pathfinding between rooms.

### Ruin / decay helpers (salvage for layer 5 / civ decoration)
- `src/generators/ruin_generator.cpp` — the current hammer. Useful pieces: `civ_config_by_name`, `ruin_decay` pass, `select_stamps`.
- `src/generators/ruin_civ_configs.cpp` — `CivConfig` definitions (Monolithic, Baroque, Crystal, Industrial, Precursor). Palette + furniture preferences per civ. **Keep.**
- `src/generators/ruin_decay.cpp` — decay intensity application. **Keep as layer 5 primitive.**
- `src/generators/ruin_stamps.cpp` — BattleScarred / Infested / Flooded stamps. **Keep as layer 4 primitive.**

### Quest fixture + recipe infrastructure
- `include/astra/quest_fixture.h` / `src/quest_fixture.cpp` — `QuestFixtureDef` registry.
- `include/astra/dungeon_recipe.h` — recipe types (to be extended).
- `src/dungeon/conclave_archive.cpp` — first recipe; factory function `build_conclave_archive_levels()`.

### Related specs / plans (prior iterations)
- `docs/superpowers/specs/2026-04-21-conclave-archive-design.md` — Stage 4 Conclave Archive design doc.
- `docs/superpowers/plans/2026-04-21-conclave-archive.md` — implementation plan (executed).
- `docs/plans/2026-04-21-dungeon-generator-factory.md` — earlier, smaller follow-up idea (abstract factory cleanup); will be **superseded** by this overhaul.

---

## Design constraints

- **No test framework in Astra.** Validation is `cmake --build build -j` + in-game dev-mode smoke testing. TDD is not the workflow.
- **C++20, header-first, `namespace astra`.** No new third-party deps.
- **Platform-isolated.** Generator code is game logic — must not include renderer or platform headers. All visual decisions flow through `TileMap` tile enums + `CivConfig` palette + renderer tile resolution in `terminal_theme.cpp`.
- **LocationKey is `std::tuple<uint32_t, int, int, bool, int, int, int>`** — `{system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}`. Dungeon levels use `depth >= 1`. Do not introduce a new key shape.
- **Recipes are persisted.** Any new fields added to `DungeonLevelSpec` / `DungeonRecipe` must bump `SAVE_FILE_VERSION` in `include/astra/save_file.h` and extend the `DREC` section write/read in `src/save_file.cpp`.

---

## Questions to explore in the brainstorm

1. **Layer dispatch contract.** Is each layer a pure function `(TileMap&, const LevelContext&, std::mt19937&)` or a composable object? How does a style select which layers to run?
2. **Style definition.** Is a "style" an enum plus a config struct? A class hierarchy? A registered builder function?
3. **Environment overlay whitelisting.** How does a style declare compatible overlays? (e.g. cave allows Flooded, disallows Vacuum; station allows Vacuum, disallows Flooded).
4. **Backdrop mechanism.** Raw tile fill vs. a renderer-level backdrop flag (like `RF_Starfield` but for underground)? The user explicitly wants the block-char underground look, so likely both.
5. **Connectivity guarantee.** Is it a separate pass that runs only for BSP-style layouts, or a post-condition on the layout pass?
6. **Stairs placement.** Does it belong in layer 6 (fixtures) or does the layout layer reserve "entry" and "exit" rooms?
7. **Migration strategy.** Current Archive dungeons use the old generator. Do we migrate them in the same slice or ship the new system with a dummy recipe, then migrate Archive separately?
8. **Naming.** New generator class name — `DungeonGenerator`, `InteriorGenerator`, `LayeredDungeonGenerator`? User already said keep it **generic**, not Archive-specific.

---

## Session plan (suggested)

1. Start with `superpowers:brainstorming`.
2. Explore the layer contract and style definition first — these are the load-bearing decisions.
3. Pick 2–3 concrete styles as design targets (Precursor ruin, open cave, derelict station) and walk each through the 6 layers as a sanity check.
4. Write the spec to `docs/superpowers/specs/YYYY-MM-DD-dungeon-generator-design.md`.
5. Follow with `superpowers:writing-plans` for a task breakdown.
6. Execute via subagent-driven development.

Archive migration is a **separate spec** after the new system lands.

---

## Out of scope for this slice

- **Stage 5 Stellar Signal arc** (three endings, Nova core extraction).
- **Archon Remnants galaxy-wide spawn boost** (separate Stage 4 polish).
- **Hidden / secret rooms.** Can layer in later.
- **Procedural dungeon recipes** (random generation of a `DungeonRecipe` from a POI budget). Out of scope until more than one hand-authored recipe exists.
- **Multi-level dungeons with branching graphs.** Current scope is strictly linear (parent ↔ child).
- **Dungeon re-entry / respawn economy.** Each level is deterministic per seed + depth; cleared levels stay cleared via save cache.
