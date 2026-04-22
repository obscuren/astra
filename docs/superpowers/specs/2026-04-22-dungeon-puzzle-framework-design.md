# Dungeon Puzzle Framework — Design

**Date:** 2026-04-22
**Status:** Draft, awaiting user review
**Related:** `docs/plans/2026-04-23-dungeon-puzzle-framework-kickoff.md`

---

## Goal

Add a reusable puzzle framework as a new pipeline layer (layer 7) in the dungeon generator. Ship one concrete puzzle kind: `SealedStairsDown`, which on Archive L1 seals the terminal room containing `stairs_dn` at generation time and places a wall-attached Precursor button elsewhere on the level. Pressing the button unlocks the seal, emits a flavor log line, and marks the puzzle solved. Solved state persists across save/load and across level revisits.

Bundled with the framework: a generic **proximity-trigger** feature for fixtures, surfacing short flavor lines when the player comes within a fixture-configurable radius. Used in this slice to make the Precursor button glint when approached, and to make the post-unlock stairs whisper a Nova-aligned line. Also bundled: a new `FixtureType::StairsDownPrecursor` variant (violet glyph) swapped in on `stairs_dn` after the seal breaks. (Stairs are already fixtures in this codebase, so no tile-level proximity plumbing is needed.)

The framework is designed so future puzzle kinds (pressure plates, rune alignment, crystal resonance, dark chambers) plug in without another architectural pass.

---

## Narrative

The Conclave thinks they control the Archive ruins. They don't. The Precursors left defensive mechanisms that reassert themselves. `SealedStairsDown` is the first concrete expression of that theme: the player reaches L1, finds the route to the deeper Archive cut off by Precursor stonework, and must locate a recessed Precursor stud elsewhere on the level to break the seal.

On unlock: *"You hear a faint rumbling in the distance, rock scraping against rock. With a sudden thud it stops, the floor shakes slightly."*

As the player moves, the button and (post-unlock) the stairs whisper their presence:

- Approaching the button: *"A button flashes faintly."*
- Approaching the unsealed stairs: *"The stairs pulse faintly with a familiar violet light. Not Conclave work."*

The stairs themselves are re-rendered in Nova violet after the unlock, a visual cue that the Precursor mechanism — not the Conclave — owns this route down.

---

## Architecture

### Pipeline layer 7 — puzzles

A new layer in `dungeon::run(...)`, executed strictly after all layer-6 sub-layers. It has its own RNG sub-seed:

```cpp
auto rng_puz = sub(ctx.seed, 0x5EA1EDEDu);  // "sealeded"
```

The layer iterates `style.required_puzzles`, filters by `depth_mask`, and dispatches each entry to a per-kind resolver. Resolvers mutate the `TileMap` (seal tiles, place button fixture) and append a `PuzzleState` record to `map.puzzles`.

The layer is part of the generator. When `dungen` returns, the dungeon is already sealed + buttoned. Runtime code only handles the *unlock* event, not initial setup.

### Style config

`DungeonStyle` gains a catalog field parallel to `required_fixtures`:

```cpp
struct RequiredPuzzle {
    PuzzleKind kind;
    uint32_t   depth_mask;   // which depths this puzzle runs on
};

struct DungeonStyle {
    // ...existing fields...
    std::vector<RequiredPuzzle> required_puzzles;
};
```

`kPrecursorRuin` registers:

```cpp
s.required_puzzles = {
    { PuzzleKind::SealedStairsDown, depth_mask_bit(1) },
};
```

### PuzzleKind enum

```cpp
enum class PuzzleKind : uint8_t {
    SealedStairsDown = 0,
    // Future: SealedChamber, PressurePlateSequence, RuneAlignment, ...
};
```

### PuzzleState (persisted)

Lives on `TileMap` as `std::vector<PuzzleState> puzzles`. Kind-tagged, with a payload sized to the largest kind (variant in C++20 — `std::variant` inside the struct, or a flat struct carrying union-like fields; final choice made in implementation plan, both serialize the same way):

```cpp
struct PuzzleState {
    uint16_t     id;                // unique within the level (1-based; 0 means unlinked)
    PuzzleKind   kind;
    bool         solved = false;

    // Kind-specific payload. For SealedStairsDown:
    std::vector<std::pair<int,int>> sealed_tiles;   // tiles swapped floor→StructuralWall
    std::pair<int,int>              button_pos { -1, -1 };
    std::pair<int,int>              stairs_pos { -1, -1 };   // cached stairs_dn for unlock
    // (Future kinds add their own payload fields or move to std::variant.)
};
```

Solved-flag is authoritative. When `solved == true`, the puzzle layer's runtime unlock path has already run (on a previous game session) and the map is in its post-unlock state — no re-application on load.

### FixtureData extension

```cpp
struct FixtureData {
    // ...existing fields...
    uint16_t    puzzle_id         = 0;   // 0 = not linked to a puzzle
    std::string proximity_message;       // empty = no proximity trigger
    uint8_t     proximity_radius  = 0;   // 0 = disabled; Chebyshev distance
};
```

New fixture type:

```cpp
FixtureType::PrecursorButton
```

- Glyph `◘`, gold (matches Precursor palette)
- Passable false, interactable true, cooldown 0 (one-time via solved-flag, not via cooldown)
- `proximity_message = "A button flashes faintly."`, `proximity_radius = 4`
- Name: *"Precursor stud"*, description: *"A recessed Precursor stud, dimly lit."*

### Fixture variant — `StairsDownPrecursor`

A new entry in `FixtureType`:

```cpp
enum class FixtureType : uint8_t {
    // ...existing...
    StairsDownPrecursor,   // descend-stairs variant with Nova violet glyph
};
```

`terminal_theme.cpp` adds a row mirroring `StairsDown`'s glyph (`>`) but with `Color::BrightMagenta` (Nova violet, matching crystal/resonance palette). Every code site that currently treats `FixtureType::StairsDown` as "descend here" must also treat `StairsDownPrecursor` the same way — see task breakdown for the 13 identified sites.

`make_fixture(StairsDownPrecursor)` defaults the same as `StairsDown` (passable false, interactable true) plus the proximity fields (`proximity_message = "The stairs pulse faintly with a familiar violet light. Not Conclave work."`, `proximity_radius = 4`).

### Proximity triggers

Fixture-only. Every `FixtureData` with `proximity_radius > 0` and non-empty `proximity_message` is eligible.

**Firing loop:** a small pass at the end of `Game::update()`. It maintains an ephemeral set of "currently inside radius" fixture ids. Each tick:

1. Iterate fixtures whose position is within a bounding box of size `max_radius × 2 + 1` around the player.
2. For each eligible fixture, compute Chebyshev distance from player to fixture position.
3. If `dist <= radius` and id not in set: log the message, insert id into set.
4. If `dist > radius` and id in set: remove id from set (re-arms the trigger).

The ephemeral set lives on `Game`, not serialised. After load, the set is empty, so re-entering a radius fires the message again — acceptable and consistent with the "once per radius reveal" decision. The fixture's `proximity_message` and `proximity_radius` themselves ARE persisted (they're part of `FixtureData`), so the trigger survives save/load.

Cost: O(fixtures-in-bounding-box). On a 120×60 level with a few dozen fixtures total, this is trivial.

### Resolver for `SealedStairsDown`

1. Identify the terminal room by `ctx.sanctum_region_id` (L1 authored carver tags it).
2. Find doorway tiles: walkable tiles inside `sanctum_box` whose 4-neighbours include at least one walkable tile *outside* `sanctum_box`. These are the seams where the room connects to the rest of the level.
3. For each doorway seam, replace the walkable tile on the *entry side* (outside `sanctum_box`) with `Tile::StructuralWall`. Record these positions in `sealed_tiles`. (Sealing the outside tile, not the inside, prevents the player from spawning inside a sealed room if they ever start there.)
4. Place the button using the existing `PlacementSlot::WallAttached` resolver, scoped to regions excluding `entry_region_id` and `sanctum_region_id`. Fallback chain: wall-attached in side room → wall-attached anywhere non-entry/non-terminal → open floor anywhere non-entry/non-terminal. The last fallback guarantees generation never fails.
5. Add the button as a `FixtureData { type = PrecursorButton, puzzle_id = <assigned> }`.
6. Assign `id` = next unused id on the level (starts at 1), append a `PuzzleState` to `map.puzzles`.

### Runtime unlock path

`src/dialog_manager.cpp :: interact_fixture` gains a case for `FixtureType::PrecursorButton`:

```cpp
case FixtureType::PrecursorButton:
    astra::dungeon::puzzles::on_button_pressed(game, fixture.puzzle_id);
    break;
```

`puzzles::on_button_pressed(game, id)`:

1. Look up the `PuzzleState` by id in the current level's `TileMap::puzzles`. If not found or already solved: return silently.
2. Dispatch on `kind`. For `SealedStairsDown`:
   - For each position in `PuzzleState::sealed_tiles`, swap `StructuralWall` → `Tile::Floor` (the dungeon-interior default).
   - At `PuzzleState::stairs_pos`, remove the `StairsDown` fixture and add a `StairsDownPrecursor` fixture in its place. The replacement fixture carries the proximity message by virtue of `make_fixture(StairsDownPrecursor)` defaults.
   - Mark `solved = true`.
   - `log("You hear a faint rumbling in the distance, rock scraping against rock. With a sudden thud it stops, the floor shakes slightly.")`

(`stairs_pos` is cached onto `PuzzleState` at generation time because `LevelContext` is pipeline-scratch and not available at runtime.)

The button fixture itself remains in place and interactable, but the resolver short-circuits on `solved`. (Polish option later: swap its glyph/color to indicate "used," but out of scope.)

### Save/load

- `SAVE_FILE_VERSION` bumps 40 → 41.
- `TileMap::puzzles` is serialized alongside `fixtures_`. For each record: id, kind, solved, sealed_tiles (length + pairs), button_pos, stairs_pos.
- `FixtureData` gains `puzzle_id` (u16), `proximity_message` (string), `proximity_radius` (u8) — all serialized in the existing per-fixture write/read block.
- `FixtureType::StairsDownPrecursor` is a new enum value; per-fixture `type` is already serialized as a u8.
- Ephemeral in-radius set on `Game` is NOT serialised — on load, re-entering triggers re-fires the message.
- Old saves rejected per `feedback_no_backcompat_pre_ship`.

### LevelContext

No new persistent fields. `LevelContext` remains pipeline-scratch. Puzzle state lives on the persisted `TileMap`.

### Dev surface

- **`:solve`** — dev command, marks every unsolved puzzle on the current level solved by invoking its unlock path (so sealed tiles are actually swapped back and the message fires). Useful for iterating without hunting the button.
- **`dumpmap`** — extended to print puzzle state: id, kind, solved, sealed-tile count, button position.

---

## Data flow summary

```
seed ──► dungeon::run
          ├─ layer 1 backdrop
          ├─ layer 2 layout          ──► ctx.sanctum_region_id, sanctum_box, entry_region_id
          ├─ layer 3 connectivity
          ├─ layer 4 overlays
          ├─ layer 5 decoration
          ├─ layer 6.i  stairs       ──► ctx.stairs_up, ctx.stairs_dn
          ├─ layer 6.iii required_fixtures
          ├─ layer 6.ii quest fixtures
          └─ layer 7 puzzles         ──► map.puzzles, map.fixtures (button), map tiles (seal)

runtime:
    button interact ──► puzzles::on_button_pressed(id)
                         └─► swap sealed tiles to floor, set solved=true, log message
```

---

## Files touched

### New

- `include/astra/dungeon/puzzles.h` — public API: `apply_puzzles(ctx, map, style)`, `on_button_pressed(game, id)`, kind enum, `RequiredPuzzle` struct.
- `src/dungeon/puzzles.cpp` — dispatch + resolvers + runtime unlock.

### Extended

- `include/astra/dungeon/dungeon_style.h` — `required_puzzles` field.
- `include/astra/dungeon/pipeline.h` / `src/dungeon/pipeline.cpp` — wire layer 7.
- `include/astra/tilemap.h` — `FixtureType::PrecursorButton`, `FixtureType::StairsDownPrecursor`, `FixtureData::{puzzle_id, proximity_message, proximity_radius}`, `TileMap::puzzles` accessors.
- `src/tilemap.cpp` — `make_fixture(PrecursorButton)` and `make_fixture(StairsDownPrecursor)` defaults.
- `src/terminal_theme.cpp` — glyph/color rows for `PrecursorButton` (gold `◘`) and `StairsDownPrecursor` (Nova violet `>`).
- `src/game.cpp` / `src/game_rendering.cpp` — proximity-trigger pass at end of `Game::update()`, ephemeral in-radius set member on `Game`.
- `src/dialog_manager.cpp` — interaction dispatch case for `PrecursorButton`; `StairsDownPrecursor` reuses `StairsDown` descent logic.
- **13 sites that currently treat `FixtureType::StairsDown` specially** must accept `StairsDownPrecursor` as equivalent for descent/rendering/editor/dumpmap: `src/main.cpp:82`, `src/tilemap.cpp:444`, `src/generators/dungeon_level.cpp:54`, `src/dialog_manager.cpp:426`, `src/dev_console.cpp:933`, `src/dev_console.cpp:1006`, `src/map_editor.cpp:299`, `src/game.cpp:701`, `src/game_input.cpp:403`, `src/dungeon/fixtures.cpp:289`, `src/dungeon/fixtures.cpp:319`, plus the theme/make_fixture sites already listed.
- `src/game_rendering.cpp` — name + description for the button.
- `src/map_editor.cpp` — editor palette entry.
- `src/dungeon/style_configs.cpp` — `required_puzzles` on `kPrecursorRuin`.
- `include/astra/save_file.h` — `SAVE_FILE_VERSION` 41.
- `src/save_file.cpp` — serialize puzzles + `puzzle_id`.
- `src/game.cpp` / `src/dev_console.cpp` — `:solve` dev command.
- `src/main.cpp` + `dev_command_dumpmap` — include puzzle state in dump.
- `docs/roadmap.md` — check off the framework.
- `docs/formulas.md` — document any tuning knobs (there are none substantial for this slice, but note the wall-attached fallback chain).

---

## Open-question resolutions (from kickoff)

| # | Question | Decision |
|---|----------|----------|
| 1 | Seal mechanism | **(a)** Physical wall — swap doorway tiles to `StructuralWall`. Breaks LoS into terminal room. |
| 2 | State persistence | **(a)** `std::vector<PuzzleState>` on `TileMap`. Save version 40→41. |
| 3 | Button placement | **(c)** `PlacementSlot::WallAttached`, scoped to non-entry/non-terminal regions, with two-step fallback. |
| 4 | Glyph / hint | `◘` gold, distinct from flavor. Player knows it's interactable; mystery is what it does. |
| 5 | Re-seal on revisit | **(a)** Stays open once solved. |
| 6 | Message delivery | `log()` line. |
| 7 | Layer ordering | Layer 7, after all of layer 6. Sub-seed `0x5EA1EDEDu`. Puzzle is part of generator, not a post-gen patch. |
| 8 | Dev console | `:solve` + `dumpmap` extension ship with this slice. |
| 9 | Puzzle ID | Explicit `uint16_t id`, stored on both `PuzzleState` and `FixtureData`. |
| A | Proximity trigger model | Chebyshev radius, per-fixture tunable. |
| B | Repeat / once | Ephemeral "currently in radius" set; re-arms on exit. |
| C | Declaration | `FixtureData` fields for fixture-bound; `TileMap::tile_proximity_triggers` for tile-bound. |
| D | Post-unlock stairs colour | New `FixtureType::StairsDownPrecursor` variant, Nova violet `>`. (Stairs are already fixtures — the kickoff's tile-vs-fixture question was based on an incorrect premise.) |
| E | Tile vs fixture for stairs | Moot — stairs are already fixtures. Fixture-level proximity covers both the button and the stairs. |

---

## Explicitly out of scope

- Additional puzzle kinds beyond `SealedStairsDown`.
- Puzzle graphs / chained puzzles.
- Cross-level puzzles.
- Quest-driven puzzles (those go through `spec.fixtures`).
- Timed / turn-limited puzzles.
- Polish: visual change on the button after it's used.

---

## Definition of done

- Layer 7 runs in `src/dungeon/puzzles.cpp`, dispatched from `pipeline.cpp`.
- `DungeonStyle::required_puzzles` exists; `kPrecursorRuin` registers `SealedStairsDown` on L1.
- `FixtureType::PrecursorButton` wired end-to-end (glyph, factory, name, description, editor, interaction).
- `:dungen precursor_ruin Precursor` on L1 produces a sealed terminal room and a button; pressing the button unlocks stairs_down with the flavor message.
- Archive quest playthrough via Io entry reproduces the same behavior.
- Save/load mid-L1 preserves puzzle state (sealed if unsolved, unlocked if solved).
- `SAVE_FILE_VERSION` bumped to 41; old saves rejected.
- `:solve` dev command and `dumpmap` extension functional.
- Approaching the button (radius 4) logs *"A button flashes faintly."*; leaving and re-entering radius re-fires.
- After unlock, the stairs_dn fixture is `StairsDownPrecursor` (Nova violet `>`, descends identically), and approaching it (radius 4) logs *"The stairs pulse faintly with a familiar violet light. Not Conclave work."*
- `cmake --build build -DDEV=ON -j` produces zero new warnings.
- `docs/roadmap.md` updated.
