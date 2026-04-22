# Archive Dungeon Migration — Kickoff Prompt

> **Purpose:** Start a fresh Claude Code session with the full context needed to migrate the Conclave Archive off the legacy ruin-based generator onto the new layered dungeon pipeline. Also reshape the Archive layout completely — the current layout was shipped as a stopgap under the old system and is intentionally being rebuilt as part of this migration.
>
> **Precondition:** Slice 1 of the dungeon generator overhaul has landed. The pipeline (backdrop / layout / connectivity / overlay / decoration / fixtures) is in `src/dungeon/` and `generate_dungeon_level(...)` already routes through it for every recipe *except* Conclave Archive, which still runs the legacy `RuinGenerator`-based body behind a `kind_tag == "conclave_archive"` branch.

---

## Paste this into the new session to start

> I want to migrate the Conclave Archive dungeon onto the new layered pipeline and completely rework its layout. Read `docs/plans/2026-04-21-archive-dungeon-migration-kickoff.md` in full and then run `superpowers:brainstorming` on the Archive-specific design. Don't touch code yet.

---

## Background — what slice 1 delivered

The dungeon generator was rebuilt as a **six-layer pipeline** driven by a data-driven `DungeonStyle` config. Every dungeon goes through the same orchestrator; styles select which layer variants run.

### Pipeline (already live)

1. **Backdrop** — fills every cell with an impassable, opaque tile and sets `Biome::Dungeon` so untouched cells render as a block character instead of starfield.
2. **Layout** — dispatches on `style.layout` into a carver (BSP rooms, open cave, tunnel cave, derelict station BSP, ruin stamps).
3. **Connectivity** — if `style.connectivity_required`, validates reachability; hookpoint for corridor pathfinding in non-BSP layouts.
4. **Overlays** — environment overlays whitelisted per style (BattleScarred / Infested / Flooded / Vacuum). Requested via `DungeonLevelSpec.overlays`; filtered against `style.allowed_overlays`.
5. **Decoration** — debris / flora / furniture, dispatched by `style.decoration_pack`. Uses civ palette. Wraps the extracted `apply_decay(map, civ, intensity, rng)` helper from `ruin_decay.cpp`.
6. **Fixtures** — stairs (6.i, via `stairs_strategy`), quest fixtures from `spec.fixtures` (6.ii, placement hints: `back_chamber`, `center`, `entry_room`, `""`), and style-required fixtures (6.iii — deferred, this is where altars land during the Archive migration).

### Per-layer RNG discipline (documented in `include/astra/dungeon/pipeline.h`)

Each layer derives its own `std::mt19937` from the level seed via XOR mixing so adding or removing an overlay never reshuffles decoration or fixture placement. Keep this discipline when adding new layer bodies.

### What's on `DungeonStyle` today

`include/astra/dungeon/dungeon_style.h`:
```cpp
struct DungeonStyle {
    StyleId         id;
    const char*     debug_name;
    std::string     backdrop_material;        // "rock","sand","plating","cavern_floor"
    LayoutKind      layout;
    StairsStrategy  stairs_strategy;          // EntryExitRooms / FurthestPair / CorridorEndpoints
    std::vector<OverlayKind> allowed_overlays;
    std::string     decoration_pack;          // "ruin_debris","cave_flora","station_scrap","natural_minimal"
    bool            connectivity_required;
};
```

`StyleId` has reserved slots for `PrecursorRuin`, `OpenCave`, `TunnelCave`, `DerelictStation` — **this migration registers the first of these (`PrecursorRuin`).**

### What's on `DungeonLevelSpec` today

`include/astra/dungeon_recipe.h`:
```cpp
struct DungeonLevelSpec {
    dungeon::StyleId              style_id    = dungeon::StyleId::SimpleRoomsAndCorridors;
    std::string                   civ_name    = "Precursor";
    int                           decay_level = 2;
    int                           enemy_tier  = 1;
    std::vector<std::string>      npc_roles;
    std::vector<PlannedFixture>   fixtures;
    std::vector<dungeon::OverlayKind> overlays;
    bool is_side_branch = false;
    bool is_boss_level  = false;
};
```

All fields are persisted (save-file version 38). No schema changes required for the Archive migration *unless* we introduce `required_fixtures` — see below.

### What's on `CivConfig` today

`include/astra/ruin_types.h` — unchanged from pre-slice-1 except for:
- `int backdrop_tint` — color for `Biome::Dungeon` block-char rendering.

The Precursor civ entry is already registered in `src/generators/ruin_civ_configs.cpp` and is what every Archive level uses today.

### The Archive bridge we're about to remove

`src/generators/dungeon_level.cpp`:
```cpp
void generate_dungeon_level(...) {
    // TODO[archive-migration]: remove once PrecursorRuin lands.
    if (recipe.kind_tag == "conclave_archive") {
        old_impl::generate_archive_level_legacy(map, recipe, depth, seed, entered_from);
        return;
    }
    ...
    dungeon::run(map, style, civ, spec, ctx);
}
```

The entire `old_impl::` namespace (port of the pre-pipeline body, plus `find_fixture_xy`, `collect_region_open`, `region_centroid`, `place_planned_fixtures`) exists solely to keep the Archive running on the legacy code. **After this slice, `old_impl::` is deleted.**

### The current Archive (what's being replaced)

`src/dungeon/conclave_archive.cpp`, factory `build_conclave_archive_levels()`. Each level is a `DungeonLevelSpec` with:
- `civ_name = "Precursor"`
- `decay_level` per depth (typically increasing)
- `fixtures` — quest altars and terminals via `QuestFixtureDef` with `placement_hint = "back_chamber"` etc.
- `kind_tag = "conclave_archive"` on the parent recipe.

The *visible* problems in the current Archive (all root-caused in the kickoff spec `docs/plans/2026-04-21-dungeon-generator-overhaul-kickoff.md`):
- Rooms often disconnected or non-existent because `RuinGenerator` builds a ruin footprint, not a playable dungeon.
- No region tagging → distance-based placement fallbacks everywhere.
- `Tile::Empty` bleeds through as starfield.
- Decay, decoration, and environment effects all fused into one `RuinGenerator` call.

The new pipeline fixes the structural bugs. **This migration also rethinks the Archive as a deliberate dungeon** — not a decayed ruin, but a sealed pre-Collapse vault with deliberate chambers, corridors, and thematic staging.

---

## The ask

Two things in the same slice:

### 1. Register a `PrecursorRuin` style

Data-driven entry in `src/dungeon/style_configs.cpp`:
```cpp
const DungeonStyle kPrecursorRuin = [] {
    DungeonStyle s;
    s.id                   = StyleId::PrecursorRuin;
    s.debug_name           = "precursor_ruin";
    s.backdrop_material    = "rock";
    s.layout               = LayoutKind::???;         // open question
    s.stairs_strategy      = StairsStrategy::EntryExitRooms;
    s.allowed_overlays     = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack      = "precursor_vault";        // NEW pack — see below
    s.connectivity_required = true;
    return s;
}();
```

Register in `parse_style_id` too so `:dungen precursor_ruin Precursor` works for smoke testing.

### 2. Redesign the Archive layout itself

The current Archive was shipped as "whatever `RuinGenerator` outputs, decorated with altars." The new Archive should feel like a **sealed Precursor vault** — intentional chambers, a sense of descent, clear thematic staging (ritual hall, reliquary, inner sanctum, etc.). Layout, chamber sizes, and connection topology are all in scope.

Open design questions the brainstorm should resolve:
- **Layout kind.** BSP rooms? Hand-assembled modules? A new `PrecursorVault` layout with explicit "nave + side chapels + sanctum" structure? BSP is cheapest; modules give the most authored feel.
- **Chamber count and size per depth.** Current recipe has N levels (check `build_conclave_archive_levels()`); each should have a distinct spatial identity.
- **Corridor style.** Broad processional halls? Narrow service passages? Both alternating?
- **Named chambers / tagged regions.** Do we add a `RegionType::Sanctum` or is region tagging still generic + we convey flavor through decoration?
- **Required fixtures catalog.** The Archive needs altars and consoles placed *by the style*, not by quest recipes. This is the deferred-from-slice-1 layer 6.iii — the migration likely adds a `std::vector<RequiredFixture> required_fixtures;` field to `DungeonStyle` and a small type enum (`Altar`, `Console`, `Plinth`, `Reliquary`, ...).
- **Decoration pack.** `"precursor_vault"` — how does it differ from the generic `"ruin_debris"`? Different decay curve? Glyph/rune scatter? Preserved furniture vs rubble?

### 3. Flip Archive recipes and delete the bridge

- Update `build_conclave_archive_levels()` so each `DungeonLevelSpec` sets `style_id = StyleId::PrecursorRuin`.
- Move any fixture types that are really "style-required, not quest-specific" out of `PlannedFixture` and into the style's `required_fixtures`.
- **Delete the `kind_tag == "conclave_archive"` branch** in `src/generators/dungeon_level.cpp`.
- **Delete the `old_impl::` namespace** and all its helpers.
- **Delete `src/generators/dungeon_level.cpp`'s remaining legacy code** — the only things that survive are `dungeon_level_seed`, `find_stairs_up`, `find_stairs_down`, and the new pipeline front door.

---

## Relevant files (read these)

### Already-written pipeline code (don't modify — extend)

- `include/astra/dungeon/dungeon_style.h` — `DungeonStyle`, enums, registry API.
- `include/astra/dungeon/level_context.h` — `LevelContext` POD.
- `include/astra/dungeon/pipeline.h` — `dungeon::run(...)` + RNG sub-seed docs.
- `include/astra/dungeon/{backdrop,layout,connectivity,overlay,decoration,fixtures}.h`
- `src/dungeon/style_configs.cpp` — add `PrecursorRuin` entry here.
- `src/dungeon/pipeline.cpp` — orchestrator.
- `src/dungeon/layout.cpp` — extend with new `LayoutKind` variant if the design calls for it.
- `src/dungeon/decoration.cpp` — add `precursor_vault` pack handler.
- `src/dungeon/fixtures.cpp` — extend 6.iii if `required_fixtures` is introduced.

### Legacy code to delete

- `src/generators/dungeon_level.cpp::old_impl::` namespace (the entire verbatim-port block).
- The `kind_tag == "conclave_archive"` branch in `generate_dungeon_level(...)`.

### Archive quest code (update, don't delete)

- `src/dungeon/conclave_archive.cpp::build_conclave_archive_levels()` — set `style_id`, possibly restructure `fixtures` vs `required_fixtures`.
- `include/astra/dungeon/conclave_archive.h`
- `include/astra/quest_fixture.h` — `QuestFixtureDef` registry. Altars and terminals that the quest interacts with stay here (they're still quest fixtures, just *placed* by the style rather than the recipe).

### Design docs for reference

- `docs/superpowers/specs/2026-04-21-dungeon-generator-design.md` — the pipeline design spec. Section "Follow-up slices (not this spec)" names this slice.
- `docs/superpowers/plans/2026-04-21-dungeon-generator-overhaul.md` — slice 1 plan. Useful for tone/granularity of the plan you'll write.
- `docs/superpowers/specs/2026-04-21-conclave-archive-design.md` — original Archive quest design (Stage 4).
- `docs/superpowers/plans/2026-04-21-conclave-archive.md` — implementation plan for the Archive quest. Helpful for understanding *what must keep working* (quest beats, NPC spawns, fixture interactions).

### Save-file / persistence

- `include/astra/save_file.h` line 66: `version = 38` — slice 1 bumped this.
- `src/save_file.cpp` `write_dungeon_recipes_section` / `read_dungeon_recipes_section` — if this migration adds `required_fixtures` to `DungeonStyle`, that field is **not** persisted per-recipe (styles are registered, not saved). No save bump needed for style-internal additions. **Only bump version if `DungeonLevelSpec` itself gains a field.**

---

## Design constraints (same as slice 1, repeated for a cold session)

- C++20, `namespace astra` (pipeline code in `namespace astra::dungeon`), header-first (`#pragma once`), no new third-party deps.
- Platform-isolated: generator code is game logic; no renderer includes, no ifdefs.
- No test framework. Validation is `cmake --build build -DDEV=ON -j` + in-game smoke testing.
- `:dungen precursor_ruin Precursor` must work end-to-end as the primary smoke test (the dev command already exists from slice 1).
- `LocationKey` is unchanged: `std::tuple<uint32_t, int, int, bool, int, int, int>`. Dungeon levels use `depth >= 1`.
- If `DungeonLevelSpec` gains any field, bump `SAVE_FILE_VERSION` (currently 38 → 39) and extend the `DREC` read/write with a version gate.

---

## Questions to explore in the brainstorm

1. **Layout approach.** BSP rooms with Precursor-biased room sizes? Hand-assembled modules (like ASCII stamps) for hallmark chambers + BSP filler? A new `PrecursorVault` layout kind with explicit nave/chapel/sanctum structure?
2. **Per-depth identity.** If the Archive is N levels deep, how do the levels differ spatially and thematically? (e.g. L1 = entrance vestibule, L2 = ritual hall, L3 = reliquary, L4 = inner sanctum.)
3. **Required-fixtures catalog.** What fixture types does the Archive actually need placed by the style? Altars? Consoles? Plinths? Braziers? Does this justify the layer 6.iii schema addition now, or can we keep everything in `spec.fixtures` with new placement hints?
4. **Chamber-scale vs tile-scale authoring.** Do we need a "chamber library" (pre-authored small rectangles with decoration patterns) or is per-tile decoration sufficient?
5. **Decoration pack design.** What makes `"precursor_vault"` distinct from `"ruin_debris"`? Glyph stamps? Different decay curve? Preserved ritual objects?
6. **Connectivity style.** Do Archive corridors need to be pathfound (wider, processional), or is BSP-natural connectivity fine?
7. **Overlays.** Should `Flooded` be allowed (lower sanctum with cistern)? `BattleScarred` (evidence of the Collapse)? `Infested` (later Conclave occupation)?
8. **Quest-fixture reshuffle.** Which current `PlannedFixture` entries should become style-required (placed by style) versus stay as quest fixtures (placed by recipe)? Criterion: does the quest logic *name* the fixture, or does the style *always* need a fixture of that shape there?
9. **Backwards compat for saves mid-quest.** If a player is mid-Archive when this slice lands, do we:
   - Reset the dungeon (regenerate with the new layout on next descent),
   - Preserve existing per-level state (player keeps progress, layout changes only on fresh levels),
   - Block the upgrade until the player leaves the Archive?
   Pick one. Save-version gating is the mechanism.

---

## Session plan (suggested)

1. Start with `superpowers:brainstorming`.
2. Walk through the questions above — question 1 (layout approach) and question 3 (required-fixtures catalog) are load-bearing; everything else flows from those.
3. If `required_fixtures` is added to `DungeonStyle`, decide on the type catalog (`FixtureKind::Altar`, `Console`, etc.) in the same brainstorm.
4. Draft an Archive-specific chamber/decoration spec — what does each depth look like?
5. Write the spec to `docs/superpowers/specs/YYYY-MM-DD-archive-dungeon-migration-design.md`.
6. Follow with `superpowers:writing-plans` for a task breakdown.
7. Execute via subagent-driven development.

---

## Explicitly out of scope for this slice

- New dungeon styles beyond `PrecursorRuin` (no `OpenCave`, `TunnelCave`, `DerelictStation` in this slice — those are their own future slices).
- Stage 5 Stellar Signal arc.
- Procedural dungeon recipes.
- Multi-level dungeon branching graphs.
- Hidden / secret rooms in the Archive (can layer in later on top of the new style).
- Tile enum expansion (`Tile::Rock`, `Tile::Plating` as distinct tiles). Style still fills with `Tile::Wall`; differentiation is palette-driven.

---

## Definition of done

- `StyleId::PrecursorRuin` is registered in `src/dungeon/style_configs.cpp`.
- `:dungen precursor_ruin Precursor` in dev console generates a visibly Archive-themed level.
- All Conclave Archive recipe levels have `style_id = StyleId::PrecursorRuin` and look *better* than the current Archive — the redesign, not just a port.
- The `kind_tag == "conclave_archive"` bridge is gone from `generate_dungeon_level(...)`.
- The `old_impl::` namespace in `src/generators/dungeon_level.cpp` is gone.
- Playing through the Conclave Archive quest from entry to completion works end-to-end — all quest fixtures reachable, all NPC spawns correct, no softlocks.
- Old saves either (a) load cleanly and regenerate the Archive with the new style on next descent, or (b) present a clear prompt to the player — whichever choice the brainstorm produces.
- `cmake --build build -DDEV=ON -j` produces zero new warnings.
- `docs/roadmap.md` updated to check off the Archive migration item.
