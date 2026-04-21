# Stage 4 — Conclave Archive on Io (Design)

**Date:** 2026-04-21
**Arc:** The Stellar Signal (main storyline) — Stage 4 closer
**Scope:** Land the Conclave Archive as a playable multi-level Precursor
ruin on Io, built on top of a new reusable multi-level dungeon generator.
Completes the roadmap *"Stage 4 — Conclave Archive (Io)"* item.

**Canonical narrative source:** `/Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md`
(Stage 4 & Stage 5 sections). Nova's crystal audio log is the Stage 5
opening beat — lines 292–343 of that document.

---

## Goal

After the Siege quest accepts, Nova tells the player to find her hidden
fragment in the Conclave Archive on Io. This spec delivers:

1. A reusable **multi-level dungeon generator** (linear descent with
   optional side branches; `DungeonRecipe` configures civ aesthetic,
   level count, enemy tiers, planned fixtures per level).
2. The **Conclave Archive**: three-level Precursor ruin on Io, with
   Conclave surface patrols, scaled enemy density per level, and an
   Archon Sentinel boss in the crystal vault.
3. A **Nova resonance crystal** at the deepest level that, on
   interaction, plays Nova's final message (Stage 5 opener) and
   completes the Siege quest. Crystal-log text is already written by
   the user in the canonical narrative file.
4. Clean handoff: Siege completion clears the `tha_lockdown` flag (wired
   in the 2026-04-21 lockdown slice), closing Stage 4. The three-ending
   branching is **Stage 5 scope** — explicitly out of scope here.

---

## Section 1 — Multi-level dungeon infrastructure (reusable)

### `DungeonRecipe` and per-level specs

New public types in `include/astra/dungeon_recipe.h`:

```cpp
struct PlannedFixture {
    std::string quest_fixture_id;   // registered in QuestFixtureDef registry
    // Placement hint. "back_chamber" / "center" / "random" — consumed by
    // the per-level generator. Empty string = random.
    std::string placement_hint;
};

struct DungeonLevelSpec {
    CivAesthetic aesthetic = CivAesthetic::Precursor;
    DecayLevel   decay     = DecayLevel::Heavy;
    int          enemy_tier = 1;           // 1..3 (scales enemy density + archetype pool)
    std::vector<std::string>    npc_roles; // e.g. {"Conclave Sentry", "Conclave Sentry"}
    std::vector<PlannedFixture> fixtures;  // e.g. nova_resonance_crystal on deepest level
    bool is_side_branch = false;           // decoration only in this slice
    bool is_boss_level  = false;           // suppresses StairsDown generation
};

struct DungeonRecipe {
    LocationKey root;                      // depth-0 key (Io overworld for the Archive)
    std::string kind_tag;                  // "conclave_archive", ...
    int         level_count = 1;           // if 0 at registration, resolved to random in [3..5]
    std::vector<DungeonLevelSpec> levels;  // size == level_count after resolve
};
```

Registry lives on `WorldManager`:

```cpp
std::unordered_map<LocationKey, DungeonRecipe, LocationKeyHash>& dungeon_recipes();
const DungeonRecipe* find_dungeon_recipe(const LocationKey& root) const;
```

Recipes are registered at quest-accept time (Archive: by Siege
`on_accepted`) or POI-generation time (future). Persisted in saves in a
new `DUNGEON_RECIPES` tagged section; save format version bumps.

### `LocationKey.depth` activation

`LocationKey` already carries a `depth` field and the hash already mixes
it, so cache keys are depth-aware with no struct change. Meaning:

- Surface / overworld key: `{sys, body, moon, is_station, -1, -1, 0}`.
- Dungeon level N: same `(sys, body, moon)` with `depth = N`,
  `is_station = false`, `-1/-1` for the other fields.

Per-level maps persist through the existing `LocationState` cache
keyed by the full LocationKey — no changes required to the cache.

### Stairs fixtures

`FixtureType::StairsDown`, `FixtureType::StairsUp`, and
`FixtureType::DungeonHatch` are already declared. This slice adds
real interaction handlers (`src/game_interaction.cpp`).

**Descent (`StairsDown` or `DungeonHatch` at depth 0):**
1. Look up the active `DungeonRecipe` for the surface's root key (for
   `DungeonHatch`) or for the current dungeon's root (for `StairsDown`
   at depth ≥ 1). Bail silently if no recipe registered (safety for
   arbitrary stairs in non-recipe dungeons later).
2. Compute target key: `target = current_key` with `depth = current_depth + 1`.
3. `save_current_location()` (existing helper).
4. Generate-or-restore target level:
   - If `location_cache().count(target)`, `restore_location(target)`.
   - Else call `generate_dungeon_level(map, recipe, target.depth, seed)`.
5. Place player at the matching `StairsUp` fixture on the new level.
   Each level stores a `meta` pair `(entered_from_x, entered_from_y)`
   — deterministic, seeded — that the generator uses to place the
   level's `StairsUp`. Descending writes these from the previous
   level's `StairsDown` position so the round-trip is exact.

**Ascent (`StairsUp`):**
1. If `current_depth == 0`, no-op (surface exit is via the overworld
   cave/hatch tile, not StairsUp).
2. If `current_depth >= 1`, compute `target` = `current_key` with
   `depth = current_depth - 1`, `save_current_location()`,
   `restore_location(target)`. The previous level is always cached
   (we just came from it).
3. Spawn player at the `StairsDown` position recorded in the previous
   level's map state — the exact tile they used to descend. This is
   the bidirectional-linkage requirement.

**Surface entry via DungeonHatch:**
- Hatch is a fixture placed by the POI generator on the overworld.
- Its interaction calls the descent path above with `target.depth = 1`.
- Exiting level 1 via `StairsUp` returns the player to the Io overworld
  at the hatch's saved `(x, y)` — since level 1 is at depth 1, ascent
  resolves to depth 0 = the Io moon overworld.

### Per-level seed

```cpp
uint32_t level_seed(uint32_t world_seed, const LocationKey& key) {
    return world_seed
         ^ hash_location_key_without_depth(key)
         ^ (static_cast<uint32_t>(key.depth) * 6271u);
}
```

Deterministic: identical dungeon content across save/load.

### `generate_dungeon_level(...)`

```cpp
void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed);
```

New file `src/generators/dungeon_level.cpp`. Dispatches to the existing
`ruin_generator` with the level's `DungeonLevelSpec.aesthetic` and
`decay`. Afterwards:

- Places `StairsUp` at the seeded entry point (first corridor
  intersection from map spawn, or a stored `entered_from_{x,y}` when
  descending).
- Places `StairsDown` at the center of the last room in the generator's
  room list — unless the level is `is_boss_level`, in which case no
  `StairsDown` is placed.
- Spawns NPCs from `DungeonLevelSpec.npc_roles` using the existing
  `create_npc_by_role` path; distributes them across rooms excluding
  the stairs rooms.
- Places planned fixtures via `QuestFixtureDef` registry using
  `PlannedFixture.placement_hint` (e.g. `"back_chamber"` → deepest
  non-entry room).

### Save / load

- New `DUNGEON_RECIPES` tagged section in `save_file.cpp` — map of
  `LocationKey → DungeonRecipe`. Bump `SAVE_FILE_VERSION`.
- Existing `LocationState` cache already stores maps by full
  `LocationKey` including depth; **no changes required**.

### Explicit out-of-scope for the infrastructure

- Branching graphs (sibling stairs to multiple child levels). Recipes
  have `is_side_branch` but all stairs in this slice are strictly
  parent↔child linear.
- Hot reload / in-game recipe editor.
- Multi-stairs per level beyond `StairsUp + StairsDown`.
- Dungeon exits via portals / teleporters.

---

## Section 2 — Conclave Archive content

### Io overworld reshape (depth 0)

`StellarSignalSiegeQuest::on_accepted` writes
`QuestLocationMeta` for the Io overworld key
`{1, 5, 0, false, -1, -1, 0}`:

- `poi_stamp_type = PoiType::PrecursorArchive` — new POI variant
  (`src/poi/precursor_archive_poi.cpp`). Deterministic stamp: a
  small Precursor ruin exterior with a central
  `FixtureType::DungeonHatch` whose `quest_fixture_id` is
  `"conclave_archive_entrance"`. The hatch interaction triggers
  descent to depth 1.
- `npc_roles = { "Conclave Sentry" × 3 }` — surface patrols exterior
  to the ruin. Stage-4 hostility scenario's regular ambush does not
  fire here (Sol is not a Conclave-controlled system).
- Quest marker `!` flows through the existing `quest_locations_`
  path — unchanged.

### Level 1 — Outer Ruin

- **Aesthetic:** Precursor (new civ config, Section 2's new config
  block). Decay: `BattleScarred` (fought-over, collapsed sections).
- **enemy_tier:** 1.
- **NPC spawns:** 4 `Conclave Sentry` (existing archetype).
- **Flavor fixtures:** 2 `QuestFixture` datapads via
  `QuestFixtureDef` (Inscription style, short Precursor-flavored
  placeholder text — not quest-bound). Separate from Nova's crystal.
- **Stairs:** `StairsUp` (returns to Io surface at hatch position)
  and `StairsDown`.

### Level 2 — Inner Sanctum

- **Aesthetic:** Precursor, **Moderate** decay (preserved chambers).
- **enemy_tier:** 2.
- **NPC spawns:** 3 `Heavy Conclave Sentry` (new archetype,
  Section 2's new NPC block) + 2 `Archon Remnant` (existing,
  Stage 2 Echo 1). The Archons are beginning to wake as Precursor
  defenses reassert — matches canon line *"a Precursor ruin they
  think they control"*.
- **Flavor fixtures:** 1 Inscription fixture with Precursor lore hint
  (placeholder text about cycles; player-written later).
- **Stairs:** `StairsUp` + `StairsDown`.

### Level 3 — Crystal Vault (boss level)

- **Aesthetic:** Precursor, `None` decay (pristine — Conclave never
  breached this layer).
- **enemy_tier:** 3.
- **NPC spawns:** 1 `Archon Sentinel` (existing, Stage 2 Echo 3
  miniboss — reuse unchanged) + 2 `Heavy Conclave Sentry` in the
  approach corridor (dead before reaching the vault — the Conclave
  has been chipping at this and failing). The boss guards the vault
  proper.
- **Mandatory fixture:** `nova_resonance_crystal`. `PlannedFixture`
  with `placement_hint = "back_chamber"`. Placed by the per-level
  generator in the vault's deepest room.
- **Flavor fixture:** 1 Inscription placeholder.
- **Stairs:** `StairsUp` only. No `StairsDown` (boss level).

### New Precursor civ aesthetic

`src/generators/civ_config_precursor.cpp` (new, plus registration in
`civ_aesthetics.h`):

- **Palette:** deep violet base + Stellari-resonance cyan accents
  (matching `color 135` already used for Conclave Sentry glyph and
  beacon fixtures from Stage 3).
- **Wall glyphs:** existing wall set, Precursor tint.
- **Furniture preference:** `pillar`, `inscription`, `altar`.
  Occasional non-interactive decorative `ResonancePillar` fixture
  (new `FixtureType::ResonancePillar`, `~` glyph, color 135, no
  interaction handler — renderer-only).
- **Decay stamps:** `BattleScarred` enabled; `Flooded` disabled;
  `Infested` disabled.

### New enemy archetype: Heavy Conclave Sentry

`src/npcs/heavy_conclave_sentry.cpp` (new). Mirrors
`src/npcs/conclave_sentry.cpp` with higher stats:

- Glyph `S`, color 135 (same as standard Sentry).
- Role name: `"Heavy Conclave Sentry"`.
- `max_hp = 50 + level * 5`, `base_damage = 9 + level / 2`,
  `defense_value = 10 + level / 2`, `armor_value = 6`,
  `xp_award = 70`. Plasma damage type.
- Hostile, aggressive, standard melee+ranged behavior (same as
  Sentry, higher numbers).

### Nova's resonance crystal (quest fixture)

Registered via `QuestFixtureDef` registry:

```cpp
QuestFixtureDef nova_crystal;
nova_crystal.id         = "nova_resonance_crystal";
nova_crystal.glyph      = '*';
nova_crystal.color      = 135;  // Stellari resonance cyan
nova_crystal.prompt     = "A small Stellari-resonance crystal hums on "
                          "a Precursor pedestal. Activate it?";
nova_crystal.log_style  = PlaybackStyle::AudioLog;
nova_crystal.log_title  = "STELLARI RESONANCE CRYSTAL — FINAL LOG";
nova_crystal.log_lines  = { /* TODO: paste from
                              nova-arc-the-stellar-signal.md
                              lines 292–343 (Nova's crystal monologue) */ };
nova_crystal.cooldown   = -1;  // one-shot
```

Interaction flow (standard `QuestFixture` path, already plumbed):
1. Prompt → confirm.
2. PlaybackViewer opens with the audio log.
3. On completion/dismiss, `QuestManager::on_fixture_interacted(
   "nova_resonance_crystal")` ticks Siege's second objective.
4. Siege `all_objectives_complete()` → quest completes via the
   existing `travel_to_destination`-style drain loop running after
   fixture interactions.
5. `StellarSignalSiegeQuest::on_completed` runs — clears
   `tha_lockdown` (already wired).

---

## Section 3 — Quest wiring

### Siege quest object restructure

`src/quests/stellar_signal_siege.cpp`:

```cpp
q.objectives = {
    { ObjectiveType::GoToLocation,
      "Land on Io", 1, 0, "Io" },
    { ObjectiveType::InteractFixture,
      "Recover Nova's fragment from the Conclave Archive",
      1, 0, "nova_resonance_crystal" },
};
q.reward.xp = 400;
q.reward.credits = 500;
q.journal_on_complete =
    "Played Nova's final message. Heard her three choices. THA's "
    "comms are open again — the Conclave pulled back. I think they "
    "didn't expect anyone to reach the vault. Nova's voice is still "
    "in my head.";
```

### `on_accepted` extends with recipe registration

```cpp
void on_accepted(Game& game) override {
    // --- existing ---
    LocationKey k{1, 5, 0, false, -1, -1, 0};
    QuestLocationMeta meta;
    meta.quest_id       = QUEST_ID_SIEGE;
    meta.quest_title    = "They Came For Her";
    meta.target_system_id = 1;
    meta.target_body_index = 5;
    meta.target_moon_index = 0;

    // --- new ---
    meta.poi_stamp_type = PoiType::PrecursorArchive;
    meta.npc_roles      = { "Conclave Sentry", "Conclave Sentry",
                            "Conclave Sentry" };
    game.world().quest_locations()[k] = std::move(meta);

    // Register Conclave Archive dungeon recipe.
    DungeonRecipe recipe;
    recipe.root        = k;
    recipe.kind_tag    = "conclave_archive";
    recipe.level_count = 3;
    recipe.levels      = build_conclave_archive_levels(); // helper in
                                                          // src/dungeon/
                                                          //   conclave_archive.cpp
    game.world().dungeon_recipes()[k] = std::move(recipe);

    // ARIA panic transmission (existing, unchanged).
    open_transmission(game, ...);
}

void on_completed(Game& game) override {
    set_world_flag(game, "tha_lockdown", false);
}
```

### Arrival on Io ticks first objective

`QuestManager::on_location_entered("Io")` is already called from
`travel_to_destination`'s `TravelToBody` branch (line ~1451 in current
tree, passes the moon name through). First objective completes on
player landing. No code changes in game_world for this — the existing
location-name matcher just works once the Siege objective target_id
is `"Io"`.

### Crystal interaction ticks second objective

Standard `QuestFixture` fixture_id path: `on_fixture_interacted(
"nova_resonance_crystal")` increments the
`InteractFixture` objective. Existing drain loop closes the quest.

---

## Flow (end-to-end)

1. Siege quest accepted (from the Return quest cascade, wired in the
   2026-04-21 lockdown slice). `on_accepted`:
   - Registers Io `QuestLocationMeta` with POI stamp + sentry patrols.
   - Registers DungeonRecipe for the 3-level Archive.
   - Opens ARIA panic transmission.
   - Queues Siege popup with Nova's "they came for me" text.
2. Player warps to Sol, navigates to Jupiter → Io. Landing completes
   Siege objective 1 ("Land on Io").
3. Io overworld: Conclave Sentry patrols visible; Precursor Archive
   POI deterministically placed with a `*` quest marker and a
   DungeonHatch fixture at center.
4. Player fights through surface patrols; interacts with hatch →
   descent to L1.
5. L1: Conclave Sentries cleared; find StairsDown; descend to L2.
6. L2: Heavy Sentries + awakening Archon Remnants; descend to L3.
7. L3: Archon Sentinel boss encounter (dice combat showcase — high AV
   forces STR penetration play); crystal fixture in the back chamber.
8. Interact crystal → audio log plays (Nova's Stage 5 monologue) →
   Siege objective 2 completes → quest completes → `tha_lockdown`
   cleared → journal `on_complete` text posts.
9. Player can now return to THA and dock (normal flow — the Lockdown
   intercept sees `tha_lockdown == false`).
10. Save/load round-trip verified at each stage.

**Stage 5 is explicitly not triggered by this slice.** The three-ending
choice branching lives in a separate spec.

---

## Files touched

**New files:**
- `include/astra/dungeon_recipe.h` — structs, registry accessors.
- `src/dungeon_recipe.cpp` — accessors, serialization helpers.
- `src/dungeon/conclave_archive.cpp` — `build_conclave_archive_levels()`
  factory.
- `src/generators/dungeon_level.cpp` — `generate_dungeon_level()`.
- `src/generators/civ_config_precursor.cpp` — Precursor civ aesthetic.
- `src/poi/precursor_archive_poi.cpp` — POI stamp (surface ruin +
  DungeonHatch placement).
- `src/npcs/heavy_conclave_sentry.cpp` — new enemy archetype.

**Modified files:**
- `include/astra/world_manager.h` — `dungeon_recipes_` map, accessors.
- `src/world_manager.cpp` — accessor implementations.
- `include/astra/save_file.h` / `src/save_file.cpp` — `DUNGEON_RECIPES`
  tagged section, version bump.
- `src/game_interaction.cpp` — StairsUp / StairsDown / DungeonHatch
  handlers.
- `src/game_world.cpp` — depth-aware travel/restoration paths (minimal:
  locate callers of `restore_location` / `save_current_location` and
  confirm they're depth-agnostic through LocationKey already).
- `src/quests/stellar_signal_siege.cpp` — new objectives, recipe
  registration, journal_on_complete, reward.
- `include/astra/fixture_defs.h` / fixture registry — register
  `nova_resonance_crystal`, `conclave_archive_entrance` (hatch
  identifier), and decorative `ResonancePillar`.
- `include/astra/npc_defs.h` — add `HeavyConclaveSentry` role name.
- `src/npc_spawner.cpp` — dispatch to new archetype.
- `include/astra/civ_aesthetics.h` — add `CivAesthetic::Precursor`.
- `CMakeLists.txt` — new source files.
- `docs/roadmap.md` — tick *Stage 4 — Conclave Archive (Io)*.

---

## Testing

Manual smoke test, in dev mode:

1. Start a game, advance through Stage 1 → 3 via dev console
   (`quest finish story_stellar_signal_{hook,echoes,beacon}`).
2. Accept Probe, warp to Conclave space → Probe auto-completes →
   Return popup → Accept.
3. Warp back to Sol → Scenario completes Return → Siege accepts →
   ARIA panic transmission → Siege popup with Nova's text → Accept.
4. Travel to THA → Automated Response denial (lockdown still set).
5. Travel to Io. Confirm:
   - Conclave Sentry patrols on Io surface.
   - Quest marker `!` at Io on star chart.
   - Precursor Archive POI exists at Io's center with a visible
     DungeonHatch fixture.
   - Siege objective 1 "Land on Io" ticks to complete.
6. Interact hatch → descend to L1. Confirm player spawns at
   `StairsUp` position.
7. Ascend from L1 `StairsUp` → returns to Io surface at hatch
   position (round-trip).
8. Re-enter, descend all the way to L3.
9. At each level, descend/ascend round-trip and verify spawn
   positions match the matching stairs (deterministic bidirectional
   linkage).
10. Kill Archon Sentinel; interact crystal; confirm:
    - Audio log plays (user-provided text, confirms placeholder
      correctly points at the canonical file).
    - Siege completes on dismissal.
    - `tha_lockdown` clears.
    - Journal `on_complete` entry posts.
11. Warp to Sol, travel to THA → normal docking, full map load.
12. Save/load at multiple points:
    - Mid-L2, mid-descent: reload lands in L2 with preserved state.
    - After Siege accept before reaching Io: recipe persists; Io
      still has POI/patrols; descent still works.
    - After crystal interaction: flag cleared state persists.

Edge cases:
- Leaving Io mid-dungeon via star chart (if possible from depth 0):
  re-entering restarts at surface. Each cached level map persists.
- Attempting `StairsUp` at depth 0 (surface): no-op, confirmed.
- Dying in the Archive: save-on-death semantics unchanged; resumes
  from last save.

---

## Out of scope

Tracked in roadmap for later slices:

- **Stage 5 — The Long Way Home** (three endings, Nova core
  extraction, branching save state, NG+ hooks).
- Archon Remnants galaxy-wide spawn boost.
- Precursor Linguist skill integration with Archive inscriptions.
- Reward tuning / legendary accessory balance pass.
- Branching multi-level dungeon graphs (sibling sub-levels).
- POI Budget integration for arbitrary (non-quest-seeded) multi-level
  ruins. The Archive is quest-seeded; future Precursor ruins via
  layered POI placement are a separate feature.
- Precursor Linguist-gated Archive doors, puzzles, lock mechanics.
