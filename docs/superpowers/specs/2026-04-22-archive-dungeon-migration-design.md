# Archive Dungeon Migration — Design Spec

> **Status:** design approved 2026-04-22. Implementation plan to follow via `superpowers:writing-plans`.
>
> **Slice:** migrate the Conclave Archive off the legacy `RuinGenerator` body onto the new layered dungeon pipeline, *and* completely redesign the Archive's spatial identity as a sealed Precursor vault. Also lands pipeline layer 6.iii (style-required fixtures) — this is its first real consumer.

---

## 1. Context

Slice 1 of the dungeon generator overhaul shipped the six-layer pipeline (backdrop / layout / connectivity / overlay / decoration / fixtures) in `src/dungeon/`. Every recipe routes through `dungeon::run(...)` **except** the Conclave Archive, which is still handled by the legacy `old_impl::generate_archive_level_legacy(...)` body behind a `kind_tag == "conclave_archive"` branch in `src/generators/dungeon_level.cpp`.

This slice:
1. Registers `StyleId::PrecursorRuin` with a new `LayoutKind::PrecursorVault`, a new `"precursor_vault"` decoration pack, and a populated `required_fixtures` catalog.
2. Redesigns the Archive's three levels with distinct spatial identities.
3. Flips Archive recipes to the new style and deletes the legacy bridge + `old_impl::` namespace.

The Archive is Astra's first authored quest dungeon under the new pipeline and the first test case for pipeline layer 6.iii. Design decisions here set the shape of future Precursor styles (tombs, outposts) and the wider style-required-fixture system.

---

## 2. New style: `StyleId::PrecursorRuin`

Registered in `src/dungeon/style_configs.cpp`:

```cpp
const DungeonStyle kPrecursorRuin = [] {
    DungeonStyle s;
    s.id                    = StyleId::PrecursorRuin;
    s.debug_name            = "precursor_ruin";
    s.backdrop_material     = "rock";
    s.layout                = LayoutKind::PrecursorVault;         // new
    s.stairs_strategy       = StairsStrategy::EntryExitRooms;
    s.allowed_overlays      = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack       = "precursor_vault";                  // new
    s.connectivity_required = true;
    s.required_fixtures     = kPrecursorRuinRequiredFixtures;     // see §5
    return s;
}();
```

Also registered in `parse_style_id(...)` so `:dungen precursor_ruin Precursor` works as the primary smoke test.

Style-level allowlist is *capability*, not *current use*. `Infested` is allowed for future Precursor sites overrun by fauna; the Archive recipe never requests it.

---

## 3. New layout: `LayoutKind::PrecursorVault`

Dispatches on `LevelContext.depth` and produces three authored topologies. Each topology returns a room graph with at least one *terminal chamber* tagged so `required_fixtures` can target it via `PlacementSlot::SanctumCenter`.

### 3.1 L1 — Outer Ruin (depth 1)

Fractured entry complex. The Conclave has pushed this far and stalled.

- Entry room → short processional corridor (**1-wide**, broken by rubble segments).
- 4–6 collapsed side rooms of varied sizes off the processional.
- Terminal chamber at far end (modest, not grand) — hosts nothing on L1 for Archive, but is the shape the layout always produces.
- StairsDown in the terminal chamber.

Reads: breached, held, battle-scarred friendly.

### 3.2 L2 — Inner Sanctum (depth 2)

Sacred intact interior. Precursor defenses reasserted and drove the Conclave back.

- Entry room → central **nave** (**3-wide**, straight, long).
- Symmetric side chapels (1-wide branches off the nave), 2–4 per side.
- Terminal chamber at the nave's far end.
- StairsDown in the terminal chamber.

Reads: sacred processional + intimate chapels.

### 3.3 L3 — Crystal Vault (depth 3, boss level)

Minimalist, processional, terminal.

- Entry antechamber → **3-wide ceremonial approach corridor** → single dominant vault room.
- No side branches.
- Vault room is the terminal chamber; hosts Plinth + boss.
- No StairsDown (boss level).

Reads: ceremonial final walk.

### 3.4 Shared layout contract

All three sub-layouts:
- Produce a connected room graph (layer 3 validates this anyway).
- Tag exactly one terminal chamber for `SanctumCenter` placement.
- Tag chapel-shaped rooms for `ChapelCenter` placement (L2 only; other depths: no chapels = no chapel-slot fixtures placed).
- Expose room rects to the connectivity + fixture layers via the usual `LevelContext` channels.

---

## 4. New decoration pack: `"precursor_vault"` (tiles-only)

Handler in `src/dungeon/decoration.cpp`. Paints tiles only; all decorative *fixtures* (pillars, braziers) come from `required_fixtures`.

- **Floor rune scatter.** Sparse, tinted in the civ palette. Flavor tiles via the existing semantic decoration layer.
- **Wall runes.** Sparse rune tiles on interior chamber walls.
- **Rubble density** tied to `decay_level`:
  - Decay 3 (L1): heavy rubble, collapsed segments in corridor, scorch-friendly substrate.
  - Decay 2 (L2): light dust, intact furnishings feel.
  - Decay 0 (L3): near-zero scatter — only faint rune dust.
- **Explicitly excluded:** flora, scrap, industrial debris. Those belong to `cave_flora` / `station_scrap`.
- **Overlay interaction:** none for v1. `BattleScarred` paints its own scorch/damage; no doubling up.

---

## 5. Pipeline layer 6.iii — style-required fixtures

### 5.1 New types

```cpp
enum class FixtureKind {
    Plinth,
    Altar,
    Inscription,
    Pillar,
    ResonancePillar,
    Brazier,
};

enum class PlacementSlot {
    SanctumCenter,    // the single terminal chamber's center
    ChapelCenter,     // centers of chapel-tagged rooms
    EachRoomOnce,     // one per non-terminal room
    WallAttached,     // attached to an interior wall of any room
    FlankPair,        // two copies flanking a target (plinth/altar); target resolved per-call
};

struct IntRange { int min; int max; };

struct RequiredFixture {
    FixtureKind   kind;
    PlacementSlot where;
    IntRange      count;        // per level
    uint32_t      depth_mask;   // bit 0 = L1, bit 1 = L2, ...
};
```

Added to `include/astra/dungeon/dungeon_style.h`:

```cpp
std::vector<RequiredFixture> required_fixtures;
```

### 5.2 Layer 6.iii implementation

In `src/dungeon/fixtures.cpp`, runs **before** 6.ii (quest-fixture placement) so 6.ii can resolve placement hints against placed required fixtures.

For each `RequiredFixture` whose `depth_mask` includes the current depth:
1. Sample `count` via the 6.iii RNG sub-seed. `count` is interpreted *per placement site* for slot types that iterate sites (`ChapelCenter`, `EachRoomOnce`, `FlankPair`), and as a total for slot types that don't (`SanctumCenter`, `WallAttached`).
2. Resolve `where` against tagged rooms / walls in `LevelContext` to produce candidate positions.
3. Spawn `FixtureKind` instances at those positions. Record each placed location keyed by `FixtureKind` in `LevelContext::placed_required_fixtures` so 6.ii and later `FlankPair` entries can look them up.
4. **`FlankPair` targeting.** A `FlankPair` entry requires a *target* fixture to flank. Target resolution rule: for each chamber that has at least one previously-placed `Plinth` or `Altar` (in that priority order), spawn two copies of `FixtureKind` on opposite sides of it. If a chamber has multiple targets (e.g. L2 chapel with two altars), flank each. `count` controls per-target duplication but defaults to `{2,2}` meaning literal "one each side." Ordering of `required_fixtures` entries is load-bearing: Plinth/Altar entries must appear before any `FlankPair` that depends on them, and this ordering is documented in the header comment. Multiple `FlankPair` entries can target the same Plinth/Altar (e.g. L3 plinth gets both `ResonancePillar` and `Brazier` flank pairs); each pair picks its own opposing axis so they don't overlap.

### 5.3 Archive's `required_fixtures` set

```cpp
const std::vector<RequiredFixture> kPrecursorRuinRequiredFixtures = {
    // L3 vault: central plinth (hosts the quest crystal via recipe override).
    { FixtureKind::Plinth,          PlacementSlot::SanctumCenter, {1,1}, depth_mask_bit(3) },
    { FixtureKind::ResonancePillar, PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },

    // L2 chapels: altar per chapel, braziers flanking.
    { FixtureKind::Altar,           PlacementSlot::ChapelCenter,  {1,2}, depth_mask_bit(2) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(2) },
    { FixtureKind::Pillar,          PlacementSlot::EachRoomOnce,  {0,2}, depth_mask_bit(2) | depth_mask_bit(3) },

    // Inscriptions: flavor text, all depths.
    { FixtureKind::Inscription,     PlacementSlot::WallAttached,  {1,2}, depth_mask_all(3) },
};
```

(`depth_mask_bit(n)` and `depth_mask_all(n)` are small helpers added in the same header.)

### 5.4 Inscription text

Inscription flavor text previously lived as `QuestFixtureDef` entries per recipe. It moves to a civ-level pool:

```cpp
struct CivConfig {
    // ... existing fields ...
    std::vector<std::string> inscription_text_pool;
};
```

Layer 6.iii draws one entry per placed `Inscription` fixture using its sub-seeded RNG. Precursor civ gets a short curated pool (5–10 entries) in `src/generators/ruin_civ_configs.cpp`.

---

## 6. Quest-fixture reshuffle

### 6.1 What stays on the recipe (`DungeonLevelSpec::fixtures`)

Only `nova_resonance_crystal` on L3.

### 6.2 New placement hint: `"required_plinth"`

Layer 6.ii resolves this against placed `FixtureKind::Plinth` locations from `LevelContext`. The quest fixture is spawned *on* the plinth tile (it visually sits on the plinth; gameplay-wise, interaction happens on that tile).

If no plinth was placed (misconfiguration), log a pipeline error and fall back to `"back_chamber"` resolution. Production styles should never hit this fallback.

### 6.3 What drops from the recipe entirely

The 1–2 flavor `Inscription` `QuestFixtureDef` datapads per level referenced in the original Archive quest spec. These become `FixtureKind::Inscription` required fixtures; their text comes from Precursor `inscription_text_pool`.

---

## 7. Archive recipe (`build_conclave_archive_levels`)

```cpp
// L1 — Outer Ruin
l1.style_id   = StyleId::PrecursorRuin;
l1.civ_name   = "Precursor";
l1.decay_level = 3;
l1.enemy_tier = 1;
l1.overlays   = { OverlayKind::BattleScarred };
l1.npc_roles  = { /* unchanged */ };

// L2 — Inner Sanctum
l2.style_id   = StyleId::PrecursorRuin;
l2.civ_name   = "Precursor";
l2.decay_level = 2;
l2.enemy_tier = 2;
l2.overlays   = { OverlayKind::BattleScarred };  // fighting-stopped-here feeling
l2.npc_roles  = { /* unchanged */ };

// L3 — Crystal Vault
l3.style_id     = StyleId::PrecursorRuin;
l3.civ_name     = "Precursor";
l3.decay_level  = 0;
l3.enemy_tier   = 3;
l3.is_boss_level = true;
l3.overlays     = {};  // pristine
l3.fixtures     = { PlannedFixture{ "nova_resonance_crystal", "required_plinth" } };
l3.npc_roles    = { /* unchanged, including Archon Sentinel boss */ };
```

NPC rosters per depth are unchanged from the current code.

---

## 8. Legacy code deletion

Removed in this slice:
- `kind_tag == "conclave_archive"` branch in `src/generators/dungeon_level.cpp::generate_dungeon_level(...)`.
- Entire `old_impl::` namespace in `src/generators/dungeon_level.cpp`:
  - `generate_archive_level_legacy(...)`
  - `find_fixture_xy(...)`
  - `collect_region_open(...)`
  - `region_centroid(...)`
  - `place_planned_fixtures(...)`
- Any helpers used only by `old_impl::`.

Surviving in `src/generators/dungeon_level.cpp`: `dungeon_level_seed`, `find_stairs_up`, `find_stairs_down`, and the pipeline front door `generate_dungeon_level(...)`.

`DungeonRecipe::kind_tag` field itself: keep if any other consumer reads it (narrative metadata, quest logic); otherwise remove in a follow-up — not scoped here.

---

## 9. Save / backward compatibility — none

Per the no-backcompat-pre-ship policy:
- `SAVE_FILE_VERSION` bumps **38 → 39**.
- Saves at version < 39 are rejected at load with a clear error message.
- No migration code, no read gates, no preserved legacy rendering path.

The `DungeonLevelSpec` schema is unchanged by this slice (the new `required_fixtures` lives on `DungeonStyle`, which is an in-code registry, not persisted per-recipe). The version bump is justified by the structural shift: persisted Archive `DetailMap`s from the legacy generator can no longer be produced and reading them back would give the player a layout that no longer matches the quest's expectations.

---

## 10. Design constraints (carry-through from slice 1)

- C++20, `namespace astra` (pipeline code in `namespace astra::dungeon`), header-first, no new third-party deps.
- Platform-isolated: generator code is game logic; no renderer includes, no ifdefs.
- No test framework. Validation is `cmake --build build -DDEV=ON -j` + in-game smoke testing via `:dungen` + full Archive quest playthrough.
- Per-layer RNG discipline preserved: layer 6.iii gets its own `std::mt19937` sub-seeded from the level seed, distinct from 6.i (stairs) and 6.ii (quest fixtures).
- `LocationKey` unchanged.

---

## 11. Definition of done

- `StyleId::PrecursorRuin` registered in `src/dungeon/style_configs.cpp` and parseable by `:dungen`.
- `:dungen precursor_ruin Precursor` generates a visibly Archive-themed level at each depth.
- `LayoutKind::PrecursorVault` implemented with three per-depth topologies; each level reads distinctly from the minimap.
- `"precursor_vault"` decoration pack implemented (tiles-only, decay-curve-driven).
- Pipeline layer 6.iii (`required_fixtures`) implemented, runs before 6.ii, exposes placed locations via `LevelContext`.
- `FixtureKind` catalog (Plinth, Altar, Inscription, Pillar, ResonancePillar, Brazier) wired to renderable fixtures.
- `PlacementSlot` resolver supports `SanctumCenter`, `ChapelCenter`, `EachRoomOnce`, `WallAttached`, `FlankPair`.
- `CivConfig::inscription_text_pool` added; Precursor entry populated with curated text.
- `build_conclave_archive_levels()` sets `style_id = StyleId::PrecursorRuin` on all 3 levels; only `nova_resonance_crystal` remains as a recipe fixture, using `"required_plinth"` placement hint.
- Legacy `old_impl::` namespace and `kind_tag == "conclave_archive"` bridge fully deleted.
- `SAVE_FILE_VERSION` bumped to 39; saves < 39 rejected at load.
- Playing through the Conclave Archive quest from Io entry → crystal interaction → completion works end-to-end. Boss fight, stair traversal, NPC spawns, and all fixture interactions correct.
- `cmake --build build -DDEV=ON -j` produces zero new warnings.
- `docs/roadmap.md` checks off the Archive migration item.

---

## 12. Explicitly out of scope

- Other `StyleId` slots (`OpenCave`, `TunnelCave`, `DerelictStation`) — each their own future slice.
- Hidden / secret rooms in the Archive.
- Tile enum expansion (`Tile::Rock`, `Tile::Plating`) — style still fills with `Tile::Wall`, differentiation remains palette-driven.
- Chamber-library / ASCII stamp system — explicitly rejected in brainstorm in favor of authored carver topologies.
- Procedural dungeon recipes; multi-level branching graphs.
- Save migration for pre-39 saves — by policy, not happening.
