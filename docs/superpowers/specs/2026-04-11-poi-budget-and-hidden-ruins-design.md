# POI Budget, Hidden Ruins, and Anchor-Hinted Placement

**Date:** 2026-04-11
**Status:** Design
**Related roadmap entries:**
- `docs/roadmap.md` — "Layered POI site selection" (this spec is its implementation)
- `docs/roadmap.md` — "Ship scanner component" (future consumer of the budget data)
- `docs/roadmap.md` — "Lore fragment system" (future — shares the journal discovery substrate)

## Problem

POI placement today runs in two disconnected stages:

1. **Overworld stamping** (`place_default_pois` in `src/generators/default_overworld_generator.cpp`) — sprinkles small POI stamps tile-by-tile. Each category (settlements, caves, ruins, crashed ships, outposts) is placed in an independent greedy pass. The only spacing constraint is an 8-tile Manhattan gate. There is no terrain awareness beyond "not mountain/lake/river" and no cross-category coordination. Variant decisions (ruin civ, ship class, cave variant) are deferred to the detail-map side.
2. **Detail-map site selection** (`PlacementScorer` in `src/generators/placement_scorer.cpp`) — when the player enters a POI tile, the scorer finds a footprint *inside* the detail zone. It hard-rejects any region with more than 15% wall tiles, which is why `CaveEntranceGenerator` and `CrashedShipGenerator` bypass it entirely. When the scorer fails, the overworld tile silently becomes an empty zone because it was already committed upstream.

This causes three concrete problems:

- **Lore can't guarantee anything.** A tier-3 ancient-civ planet has no mechanism to *ensure* it contains an excavation or a Precursor ruin — each POI is an independent roll.
- **Every generator reinvents site picking.** `CaveEntranceGenerator::find_cliff_edge_global()` and `CrashedShipGenerator`'s own placement routine exist because `PlacementScorer` is too settlement-shaped. Stage-1 terrain information (why the overworld tile was chosen) is thrown away by the time stage 2 runs.
- **No pre-visit visibility.** The player can't see what a planet contains without walking every tile. There is no substrate for a ship scanner, no substrate for the existing "Archaeology skill effects" roadmap items, and no way for the dev console to summarize a planet's composition.

A secondary goal: provide a substrate for **hidden ruins** — ruins that exist in the world data but are not rendered on the overworld until the player walks onto their tile, at which point a Journal entry is created and the tile swaps to its true form.

## Design

### Two-layer model

**Layer 1 — the budget (PoiBudget).** Computed deterministically once per planet from `(planet_seed, body_props, lore_snapshot)`. Lists exactly how many of each POI kind the planet will host, with variants pre-rolled. Stored with the overworld map, saved/loaded, accessible to UI and dev tools.

**Layer 2 — placement requests.** The overworld generator expands the budget into a list of `PoiRequest` entries, each carrying its kind, variant, terrain requirements, and priority. A single placement pass walks the overworld, scores candidate tiles against each request's requirements, and assigns greedily in priority order. When a tile is chosen, the pass records an **anchor hint** on the tile — a small struct describing *why* the tile was picked (e.g., "cliff edge at offset +3,+5") so stage-2 detail-map generators can stamp without rescanning.

Stage 2 — the detail-map POI phase — is **not rewritten**. Existing generators keep their stamping logic. The only change is that they read the anchor hint from `MapProperties` and use it as their starting point, bypassing (or supplementing) `PlacementScorer` on their own terms. `PlacementScorer` itself is untouched.

### Ruin tiers: visible and hidden

Ruins become a two-tier resource driven by lore tier:

- **Visible ruins** — placed on the overworld with the normal stamp glyph. Player can see and navigate to them immediately.
- **Hidden ruins** — placed in the overworld map's data but rendered as the underlying biome tile until the player steps onto them. On the step-onto event, the tile swaps to its true `OW_Ruins` form, a Journal entry is created (category `Discovery`), and a message-log line flashes.

Hidden ruins are stored in a generic `hidden_pois` list on the overworld map (not ruin-specific — the mechanism should be reusable for buried wrecks, sealed vaults, etc., though only ruins will be hidden in this first pass). The discovery event fires when the player's movement resolves onto a tile with an undiscovered entry.

### Journal integration

The Journal system (`include/astra/journal.h`) already exists with a `JournalCategory::Discovery` value. A discovery creates a single `JournalEntry`:

- **category:** `Discovery`
- **title:** `"Ruin: Precursor-A Outpost — Karn-3 IV"`
- **technical:** `"System: Karn-3  •  Body: IV  •  Coords: (142, 88)  •  Civ: Precursor-A  •  Lore Tier: 2"`
- **personal:** commander's-log flavor text, procedurally assembled from civ + tier + biome templates
- **timestamp:** formatted from `world_tick`

Each Discovery entry also carries enough data to render a **live map preview** at journal render time: `(system_id, body_index, overworld_x, overworld_y)` plus a stable location name string captured at discovery time (so the title and technical text render correctly even if navigation state moves on). At display time, the journal panel draws a fixed-size window of the overworld map around those coordinates using whatever renderer is active. This stays backend-agnostic for the eventual SDL port — we are not snapshotting rendered cells, only storing coordinates and re-rendering live.

The center tile is highlighted in the preview so the player can spot the location at a glance.

### Budget visibility in UI

The ship scanner component that will eventually gate budget visibility is **out of scope for this spec**. For now, the star-chart planet info panel displays the full budget unconditionally, framed as output from a "basic built-in ship scanner". When the real scanner component ships, it becomes an upgrade that reveals additional data (the hidden/uncharted split), and the basic display continues to work without UI rework.

## Data model

### `PoiBudget`

Lives in a new `include/astra/poi_budget.h`.

```cpp
namespace astra {

enum class RuinFormation : uint8_t {
    Solo,
    Connected,
};

enum class ShipClass : uint8_t;    // already defined in crashed_ship_types.h
enum class CaveVariant : uint8_t;  // already defined in cave_entrance_types.h

struct RuinRequest {
    std::string civ;             // civilization key ("precursor_a", etc.)
    RuinFormation formation = RuinFormation::Solo;
    bool hidden = false;         // true = added to hidden_pois, not overworld stamps
};

struct ShipRequest {
    ShipClass klass;             // pod / freighter / corvette
};

struct PoiBudget {
    // Per-kind counts
    int settlements = 0;
    int outposts = 0;

    struct CaveCounts {
        int natural = 0;
        int mine = 0;
        int excavation = 0;
    } caves;

    // Per-item request lists (variant pre-rolled)
    std::vector<RuinRequest> ruins;   // includes both visible and hidden
    std::vector<ShipRequest> ships;

    // Future (stubbed, not placed by this spec)
    int beacons = 0;
    int megastructures = 0;
};

// Roll a budget from planet context. Deterministic given the seed.
PoiBudget roll_poi_budget(const MapProperties& props, std::mt19937& rng);

// Human-readable summary for dev console / planet info panel.
std::string format_poi_budget(const PoiBudget& budget);

} // namespace astra
```

The budget is stored as a field on `MapProperties` and copied into the overworld `TileMap` (via a new `poi_budget()` accessor) so it survives past `MapProperties` lifetime.

### `PoiRequest` and placement

Not a persisted type — only exists during overworld generation. Lives alongside the placement pass in `src/generators/poi_placement.cpp` (new).

```cpp
enum class PoiPriority : uint8_t { Required, Normal, Opportunistic };

struct PoiTerrainRequirements {
    bool needs_cliff = false;         // cave mine / natural cave
    bool needs_flat = false;          // settlement / outpost / crashed ship
    bool needs_water_adjacent = false;// (future — currently unused)
    std::vector<Biome> allowed_biomes;// empty = any biome
    int min_spacing = 8;              // tiles to other placed POIs
};

struct PoiRequest {
    Tile poi_tile;                    // OW_Ruins, OW_Outpost, etc.
    PoiTerrainRequirements reqs;
    PoiPriority priority = PoiPriority::Normal;
    // Variant payload carried through to the anchor hint
    std::string ruin_civ;             // ruins only
    RuinFormation ruin_formation = RuinFormation::Solo;
    bool ruin_hidden = false;
    ShipClass ship_class;             // ships only
    CaveVariant cave_variant;         // caves only
};
```

### `PoiAnchorHint`

Stored on the overworld tile (see next section) and carried into `MapProperties` when `build_detail_props()` constructs the detail-map context.

The budget runs at **overworld scale** — one cell per planet tile — not detail-map scale. When placement picks a tile, the "reason" for the pick is expressed in terms of *which neighbouring overworld tile made the pick valid*. Stage-2 generators then use this reason to know which edge of the freshly-generated detail map to stamp against. For a cliff-requiring cave, the hint says "mountain is to the north" and the cave generator stamps against the detail map's north edge (which inherits cliff walls from the neighbour).

```cpp
enum class AnchorDirection : uint8_t {
    None,
    North,       // neighbour to the north triggered the match
    South,
    East,
    West,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
};

enum class AnchorReason : uint8_t {
    None,
    CliffAdjacent,      // adjacent overworld tile is mountain/crater
    WaterAdjacent,      // adjacent overworld tile is lake/river
    Flat,               // tile passed flatness filter with no directional trigger
    Open,               // no terrain requirement, took best-scored open tile
};

struct PoiAnchorHint {
    bool valid = false;
    AnchorReason reason = AnchorReason::None;
    AnchorDirection direction = AnchorDirection::None;  // meaningful for Cliff/Water
    // Variant echo so stage-2 generators can read without reparsing props strings.
    CaveVariant cave_variant = CaveVariant::None;
    ShipClass ship_class = ShipClass::EscapePod;
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};
```

### `HiddenPoi`

Lives on the overworld `TileMap`.

```cpp
struct HiddenPoi {
    int x = 0;
    int y = 0;
    Tile underlying_tile = Tile::OW_Plains;  // what to render until discovered
    Tile real_tile = Tile::OW_Ruins;         // what the tile becomes on discovery
    bool discovered = false;
    // Discovery payload (used when firing the journal event)
    std::string ruin_civ;
    RuinFormation ruin_formation = RuinFormation::Solo;
};
```

The overworld `TileMap` gets:

```cpp
std::vector<HiddenPoi>& hidden_pois();
const std::vector<HiddenPoi>& hidden_pois() const;
const HiddenPoi* find_hidden_poi(int x, int y) const;  // undiscovered lookup
```

Render code queries `find_hidden_poi` and substitutes `underlying_tile` when the entry exists and is undiscovered. Movement code queries it on step-onto and, if undiscovered, flips `discovered`, fires the Journal event, and emits a message.

### `JournalEntry` additions

A new Discovery-only payload, stored on the entry but only populated for `category == Discovery`.

```cpp
struct JournalEntry {
    // ... existing fields ...

    // Populated for Discovery entries. Used to render a live map preview.
    bool has_discovery_location = false;
    int discovery_system_id = 0;
    int discovery_body_index = 0;
    int discovery_moon_index = -1;
    int discovery_overworld_x = 0;
    int discovery_overworld_y = 0;
    std::string discovery_location_name;   // e.g. "Karn-3 IV"
};
```

A helper `make_discovery_journal_entry(...)` assembles entries from the ruin data + current world state + a flavor-text pool.

## Placement pass — how it actually runs

Invoked from `DefaultOverworldGenerator::place_pois` (and equivalent subclasses), replacing `place_default_pois`.

```
1. Expand budget into a vector<PoiRequest>.
   - Ruins: one request per RuinRequest entry.
   - Caves: one request per natural/mine/excavation slot.
   - Ships: one request per ShipRequest.
   - Settlements/outposts: one request per count.
2. Partition requests by priority: Required first, then Normal, then Opportunistic.
3. Build a terrain score cache over the overworld map:
     - For each tile: biome, adjacency to cliff (mountain-adjacent),
       adjacency to water, flatness (local neighbourhood variance).
   This is O(w*h), done once per overworld generation.
4. For each request in priority order:
     a. Filter candidates by reqs (biome, cliff, flat, water).
     b. Filter out tiles within min_spacing of any already-placed POI.
     c. Score remaining candidates — lower is better — by a weighted sum of
        (distance from map centre, terrain match, openness).
     d. Take the best, compute the PoiAnchorHint offset (e.g. find the actual
        cliff tile adjacent to the chosen tile), stamp the overworld tile, and
        store the hint keyed by (overworld_x, overworld_y).
     e. If the request is flagged hidden, append to hidden_pois instead of
        stamping the overworld tile; the underlying tile is captured from what
        the overworld already has at that coord.
5. Log failures. Required failures emit a dev-mode warning and are silently
   accepted (the placement pass does not retry with relaxed constraints or
   fall back to a cheaper slot — accepting the miss is preferable to a map
   with a POI in an obviously wrong place). Normal and Opportunistic
   failures are silent.
```

The anchor hint storage lives on the overworld `TileMap` as:

```cpp
std::unordered_map<uint64_t, PoiAnchorHint> poi_anchor_hints;  // key = y*width + x
```

Read by `Game::build_detail_props()` and copied into `MapProperties::detail_poi_anchor`.

### Stage-2 integration

`MapProperties` gains one field:

```cpp
PoiAnchorHint detail_poi_anchor;
```

Each existing POI generator gets one change:

- **`CaveEntranceGenerator`** — if `props.detail_poi_anchor.valid` and `reason == CliffAdjacent`, stamp against the detail-map edge indicated by `direction` (e.g., direction North → scan the top edge of the detail map for a wall tile). This replaces `find_cliff_edge_global` with a bounded edge scan. The `cave_variant` comes from the hint, overriding the string parse. If the hint is invalid or no wall is found on the hinted edge, fall back to `find_cliff_edge_global` exactly as today.
- **`CrashedShipGenerator`** — if the hint is valid, take `ship_class` from the hint. Skid direction biases toward the hint's `direction` when present. Site roll remains local.
- **`RuinGenerator`** — read `ruin_civ` from the hint if present, falling back to the existing props string.
- **`SettlementPlanner` / `OutpostPlanner`** — optional hint; if absent (old saves, hand-crafted zones), fall back to `PlacementScorer::score` exactly as today. This gives backwards compatibility for free.

No generator is rewritten. Each gets a small "hint first, scorer fallback" prologue.

## Roll rules — how the budget is decided

The budget roll lives in `roll_poi_budget`. It reads existing fields on `MapProperties` (body type, atmosphere, temperature, lore tier, lore flags) and produces counts.

Sketch of the rules, which map directly onto what `place_default_pois` does today but becomes explicit:

| Kind | Habitable | Marginal | Airless (non-asteroid) | Asteroid |
|---|---|---|---|---|
| Settlements | 3 base, +lore | 0–1 (40%) | 0 | 0 |
| Outposts | 1–2 (30% chance, 70% @ tier 2+) | same | 0–1 | 0 |
| Caves (natural) | 2–5 if dungeon | same | same | 0–2 |
| Caves (mine) | +1 if lore tier ≥ 2 | same | same | 0 |
| Caves (excavation) | +1 if lore tier ≥ 3 | same | — | — |
| Ruins | lore-tier driven: 1–2 / 3–5 / 6–10 / 8–14 | same | sparse | sparse |
| Hidden ruins | `hidden_ratio = clamp(lore_tier * 0.25, 0, 0.6)` of ruin count | same | — | — |
| Crashed ships | 1–3 (20% + danger×10%); 3–7 if battle site | same | same (pod/freighter only) | pod only |
| Beacons | 0 (future) | — | — | — |
| Megastructures | 0 (future) | — | — | — |

Required priority applies to:
- Tier-3 ancient-civ worlds: at least one excavation must place.
- `lore_battle_site` worlds: at least one crashed ship must place.
- Tier-3 worlds: at least half of hidden ruins must place.

Everything else is Normal priority.

### Ruin variant rolling

For each ruin slot, roll civ (weighted by lore primary civ), formation (solo / connected — connected adds a second ruin request within close spacing of the first), and hidden flag (from `hidden_ratio`).

## Planet info panel

The star chart planet info panel (already rendering tier/biome/lore details) gains a new "Ship Scanner" section at the bottom, always visible:

```
SCANNER REPORT
──────────────
  Settlements   3
  Outposts      1
  Ruins         8  (Precursor-A x5, Unknown x3)
  Caves         5  (natural x3, mine x1, excavation x1)
  Crashed Ships 3
```

This reads directly from the overworld `TileMap::poi_budget()`. No scanner component gate in this pass — the fiction is "your ship has a basic built-in sensor suite". The real scanner component ships later and extends this display with a "visible / uncharted" split.

## Dev console

A new `budget` command dumps the current planet's PoiBudget in full, including the hidden-ruin list and the anchor-hint map, for debugging placement failures.

A new `discoveries` command dumps the player's Discovery journal entries.

## Save format

Bumping save version (current v22 → v23). Three additions:

- `PoiBudget` serialised on each overworld `TileMap`.
- `std::vector<HiddenPoi>` serialised on each overworld `TileMap`.
- Per-tile `PoiAnchorHint` map serialised on each overworld `TileMap`.
- `JournalEntry` adds discovery location fields (read as zero / `has_discovery_location = false` for older entries).

Old v22 saves load without a budget. On load, a reconstruction pass walks each overworld `TileMap`, counts placed POI tiles (`OW_Settlement`, `OW_Ruins`, etc.), and synthesises a `PoiBudget` that *describes* what's already on the map. This keeps the scanner report functional on legacy saves. Variant data is unknown for reconstructed budgets (ruin civ shows as "Unknown", ship class as "Unknown"). The anchor-hint map is empty for legacy saves — stage-2 generators fall back to their pre-spec behaviour, which is exactly what they did in v22. Hidden ruins do not exist in legacy saves; they only appear on worlds generated at v23 or later.

## Files touched

**New:**
- `include/astra/poi_budget.h` — budget + helpers
- `src/poi_budget.cpp` — roll + format
- `include/astra/poi_placement.h` — placement pass + anchor hint + request types
- `src/generators/poi_placement.cpp` — the unified placement pass
- `include/astra/hidden_poi.h` — `HiddenPoi` struct

**Modified:**
- `include/astra/map_properties.h` — add `detail_poi_anchor`, carry budget reference
- `include/astra/tilemap.h` — `hidden_pois()` accessor, `poi_anchor_hints` map, `poi_budget()` accessor
- `src/tilemap.cpp` — storage for the above
- `src/generators/default_overworld_generator.cpp` — replace `place_default_pois` body with a call into `poi_placement.cpp`; the entry point keeps its name and signature for other overworld subclasses
- `src/generators/overworld_generator_base.cpp` — call `roll_poi_budget` during generation, store on map
- `src/generators/cave_entrance_generator.cpp` — honour anchor hint; keep global scan as fallback
- `src/generators/crashed_ship_generator.cpp` — honour anchor hint
- `src/generators/ruin_generator.cpp` — honour anchor hint (civ only)
- `src/generators/poi_phase.cpp` — route anchor hint into generators
- `src/game_world.cpp` — `build_detail_props()` copies anchor hint; step-onto event checks `hidden_pois` and fires Journal event
- `include/astra/journal.h` — Discovery location fields on `JournalEntry`, `make_discovery_journal_entry` helper
- `src/journal.cpp` — implementation + flavor-text pools
- `src/character_screen.cpp` — render live map preview for Discovery entries
- `src/star_chart_viewer.cpp` / planet info panel — scanner report section reading from `poi_budget()`
- `src/dev_console.cpp` — `budget` and `discoveries` commands
- `include/astra/save_file.h` / `src/save_file.cpp` / `src/save_system.cpp` — version bump, serialize new fields
- `docs/roadmap.md` — mark "Layered POI site selection" done; note hidden ruins + budget substrate

## Out of scope

Explicitly not in this spec (each is its own future feature):

- **Ship scanner component.** The budget is always visible via a "basic built-in scanner" fiction. Real scanner tiers come later.
- **`PlacementScorer` rewrite.** It stays as-is. Settlement and outpost generators keep using it. Cave and crashed-ship generators bypass it via the anchor hint, same as today.
- **Journal codex UI.** Discovery entries surface in the existing journal tab. No new panel.
- **Lore fragment items** (data crystals, memory engrams). The journal discovery path is shared substrate for that feature, but lore fragments are not placed by this spec.
- **Archaeology skill effects.** Beacon Sense, Ruin Reader, etc. will later modify scanner output, but are not wired here.
- **Beacon and Megastructure POIs.** Budget tracks them but the count stays 0. Parked roadmap items.
- **Buried / hidden crashed ships, sealed vaults.** The `HiddenPoi` mechanism is generic to support them later, but no non-ruin hidden POIs are generated in this pass.
- **Connected-ruin placement.** The `RuinFormation::Connected` value is recorded on each ruin request for future use, but in this pass both `Solo` and `Connected` produce one ruin each with standard spacing. Real multi-site linked ruins (with tighter spacing and a shared backstory) come later.

## Success criteria

- Generating any planet twice with the same seed produces the same `PoiBudget` and the same placed POIs.
- A tier-3 ancient-civ planet generates with at least one excavation, one crashed ship (if battle site), and at least half of its hidden ruins — verified by dev console `budget` on several seeds.
- Walking onto a hidden ruin tile swaps the tile, emits `"Ruin discovered — …"` to the message log, creates a `JournalCategory::Discovery` entry with the correct system/body/coords/location name, and the entry renders a live map preview in the journal tab centred on the discovered tile.
- Saving and loading a planet preserves budget, hidden ruins (including discovered state), anchor hints, and journal discovery entries.
- `CaveEntranceGenerator` with a valid anchor hint stamps its entrance at the cliff tile indicated by the hint without scanning the whole map.
- `CrashedShipGenerator` with a valid anchor hint stamps the ship's hull centered on the hint without its own site roll.
- `SettlementPlanner` and `OutpostPlanner` continue to work on old saves (no hint present) via the `PlacementScorer` fallback.
- Star chart planet info panel shows a scanner report section sourced from `poi_budget()`.
- Dev console `budget` prints the current planet's full budget; `discoveries` prints the player's Discovery journal entries.
