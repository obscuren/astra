# Crashed Ship POI — Design Spec

**Date:** 2026-04-10
**Status:** Draft
**Scope:** Phase 6 of `2026-04-07-detail-map-generation-v2-design.md` — implement the Crashed Ship POI stamp.

## Context

Phase 6 of the detail map v2 rewrite introduces POIs. Settlements, Ruins, and Outposts are implemented; Crashed Ship, Cave Entrance, Landing Pad, Beacon, and Megastructure are still stubbed in `poi_phase.cpp`. This spec covers the Crashed Ship stamp only.

The legacy `detail_map_generator.cpp:1525-1696` had a single fixed crashed ship (20×6 fuselage, east-west orientation, 8-12 tile skid, 30×20 debris field, 2 bulkheads, Console + Crates + Conduit + Portal). The v2 rewrite keeps the core concept but expands it into three classes with richer variety and a stronger visual signature around the skid mark.

## Concept

A crashed ship is a **wrecked hull sitting on the surface** surrounded by scattered debris, with a **long scorched skid mark** leading to it showing where the ship plowed into the ground. It's an empty loot wreck — no survivors, no creatures inside — the player walks in, scavenges crates, maybe reads a console log (lore later), walks out. Enemies come from the biome, not the ship itself.

### Visual signature

- **Long scorched skid mark** behind the ship, cutting through whatever terrain/scatter was there (trees, grass, minerals, debris — all cleared). The skid is the defining visual.
- **Hull wreckage** — ~75% breached plating, tapered nose/stern, 2-5 hull breaches letting the interior show through.
- **Debris field** of small wall fragments scattered in a radius around the ship.
- **Interior rooms** separated by partial bulkheads — scavengeable crates, consoles, conduits.

### Three ship classes

| Class | Size | Feel |
|---|---|---|
| **EscapePod** | Tiny (10×6) | Emergency cabin, short skid, minimal loot. The body of an unfortunate pilot. |
| **Freighter** | Medium (24×8) | Cargo hauler, 3 rooms, medium skid, crates-heavy loot. |
| **Corvette** | Large (36×10) | Military/civilian corvette, 5 rooms, long skid, richest loot. |

## Decisions Summary

| Decision | Choice | Rationale |
|---|---|---|
| Purpose | Empty loot wreck (no survivors/enemies) | Start simple; enemies come from biome |
| Class count | 3 (EscapePod / Freighter / Corvette) | More variety than one ship, still bounded scope |
| Class selection | Lore-tier weighted | World progression gets meaning (tier 2 areas had real traffic) |
| Orientation | Random 4-way (N/S/E/W) | Every wreck feels different |
| Dungeon portal | ~20% chance per stamp | Rare "this one cracked something open" variant |
| Biome validity | All except Aquatic | Aquatic ships read weird in ASCII |
| Architecture | New `CrashedShipGenerator` class parallel to `RuinGenerator` | Clean isolation; no `SettlementPlan` overhead |
| Skid marks | Long (14-40 tiles), clears all fixtures/scatter in path | The defining visual; player sees it from far away |
| NPCs | None | Empty wrecks per purpose decision |
| Dev command | `biome_test <biome> ship [pod|freighter|corvette]` | Exercises all classes during development |

---

## 1. Architecture

### New files

| File | Responsibility |
|---|---|
| `include/astra/crashed_ship_types.h` | `ShipClass` enum, `ShipOrientation` enum, `ShipClassSpec` struct (hull dimensions, room/bulkhead positions, skid range, debris radius, fixture template). |
| `include/astra/crashed_ship_generator.h` | `CrashedShipGenerator` class with one public `generate()` method. |
| `src/generators/crashed_ship_generator.cpp` | Full generator: class selection, orientation, placement, skid, hull, bulkheads, breaches, debris, fixtures, portal. Private helpers for orientation rotation and per-class layout. |

### Modified files

| File | Change |
|---|---|
| `include/astra/map_properties.h` | Add `std::string detail_crashed_ship_class` override field. |
| `src/generators/poi_phase.cpp` | Remove `OW_CrashedShip` from the stub list; dispatch to `CrashedShipGenerator`. |
| `CMakeLists.txt` | Add `src/generators/crashed_ship_generator.cpp` to `ASTRA_SOURCES`. |
| `src/dev_console.cpp` | Parse `ship` / `crashed_ship` + class name args in `biome_test`. |
| `src/game.cpp` | `dev_command_biome_test()` outpost-style branch for crashed ship. |
| `include/astra/game.h` | No signature change (existing `poi_style` parameter is repurposed for ship class name). |
| `docs/roadmap.md` | Check the Crashed Ship POI box; add follow-up entries. |

### Reused as-is (no changes)

- `PlacementScorer` — picks the site. Footprint passed in varies per class + orientation (e.g. corvette needs the worst-case 60×40 to account for the long skid and debris ring).
- No settlement pipeline stages used — `CrashedShipGenerator` stamps the map directly.

---

## 2. Class Specifications

All hull coordinates are in **ship-local "east-facing" frame** (nose at +dx). Orientation rotates these through a `rotate()` helper before stamping.

**Coordinate convention:** ship-local dx ranges over `[-hull_len/2, hull_len/2 - 1]` (an even-length hull). Local dy ranges over `[-body_half_h, +body_half_h]`. All room extents below are **inclusive** dx ranges.

### EscapePod

- **Hull:** ship_len=10, body_half_h=3 (10 × 7 bounding box)
- **dx range:** [-5, +4]
- **Taper:** nose narrows over last 3 tiles (half-h 3 → 1); stern is flat
- **Hull coverage:** 0.75
- **Rooms:** 1 — cabin [-5, +4] (single cabin, no bulkheads)
- **Skid length:** 14–18 tiles
- **Debris radius:** 8 tiles
- **Debris fragment count:** 3–5
- **Breach count:** 2 (2-tile wide openings)
- **Fixtures (by room):**
  - Cabin: 1 Console, 1 Bunk, 1 Crate

### Freighter

- **Hull:** ship_len=24, body_half_h=4 (24 × 9 bounding box)
- **dx range:** [-12, +11]
- **Taper:** nose narrows over last 4 tiles (half-h 4 → 1); stern narrows slightly (half-h 4 → 3)
- **Hull coverage:** 0.75
- **Rooms:** 3
  - Engine bay: [-12, -5]
  - Cargo hold: [-3, +3]
  - Cockpit: [+5, +11]
- **Bulkheads at:** dx = -4, dx = +4 (partial `StructuralWall` with center-row gap)
- **Skid length:** 22–28 tiles
- **Debris radius:** 14 tiles
- **Debris fragment count:** 8–15
- **Breach count:** 3–4 (2–3 tile wide openings)
- **Fixtures (by room, ordered stern→nose):**
  - Engine bay: 1 Conduit, 1 Rack
  - Cargo hold: 3–5 Crates
  - Cockpit: 1 Console

### Corvette

- **Hull:** ship_len=36, body_half_h=5 (36 × 11 bounding box)
- **dx range:** [-18, +17]
- **Taper:** nose narrows over last 5 tiles (half-h 5 → 1); stern narrows slightly (half-h 5 → 4)
- **Hull coverage:** 0.75
- **Rooms:** 5
  - Engine bay: [-18, -12]
  - Cargo hold: [-10, -5]
  - Mess: [-3, +1]
  - Quarters: [+3, +9]
  - Cockpit: [+11, +17]
- **Bulkheads at:** dx = -11, -4, +2, +10 (partial `StructuralWall` with center-row gap)
- **Skid length:** 32–40 tiles
- **Debris radius:** 20 tiles
- **Debris fragment count:** 12–20
- **Breach count:** 4–5 (2–3 tile wide openings)
- **Fixtures (by room, ordered stern→nose):**
  - Engine bay: 1 Conduit, 1 Rack
  - Cargo hold: 3–5 Crates
  - Mess: 1 Table, 2 Bench
  - Quarters: 2–3 Bunks
  - Cockpit: 2 Console

### `ShipClassSpec` struct

```cpp
struct RoomExtent { int dx_min, dx_max; };

struct ShipClassSpec {
    ShipClass class_id;
    const char* name;

    // Hull
    int hull_len;              // full length (even)
    int body_half_h;           // widest half-height
    int nose_taper_len;        // tiles over which nose narrows
    int stern_taper;           // tiles by which stern narrows (0 = flat)
    float hull_coverage;       // 0.75f — fraction of edge tiles kept as wall

    // Rooms
    std::vector<RoomExtent> rooms;        // ordered stern→nose
    std::vector<int> bulkhead_dx;         // local-x positions of bulkheads

    // Skid
    int skid_min, skid_max;    // length range in tiles

    // Debris
    int debris_radius;
    int debris_min, debris_max; // fragment count range

    // Breaches
    int breach_min, breach_max;

    // Fixtures per room index (matches rooms[])
    std::vector<std::vector<FixtureType>> fixtures_by_room;
};
```

Three constants (`kSpecEscapePod`, `kSpecFreighter`, `kSpecCorvette`) hold the per-class data at file scope in `crashed_ship_generator.cpp`.

---

## 3. Class Selection & Orientation

### Lore-weighted class roll

```cpp
ShipClass pick_ship_class(int lore_tier, std::mt19937& rng) {
    int r = rng() % 100;
    switch (lore_tier) {
        case 0:  // 70 / 25 / 5
            if (r < 70) return ShipClass::EscapePod;
            if (r < 95) return ShipClass::Freighter;
            return ShipClass::Corvette;
        case 1:  // 30 / 55 / 15
            if (r < 30) return ShipClass::EscapePod;
            if (r < 85) return ShipClass::Freighter;
            return ShipClass::Corvette;
        default: // tier 2+: 10 / 40 / 50
            if (r < 10) return ShipClass::EscapePod;
            if (r < 50) return ShipClass::Freighter;
            return ShipClass::Corvette;
    }
}
```

If `props.detail_crashed_ship_class` is non-empty, skip the roll and parse the override ("pod", "freighter", "corvette").

### Orientation rotation

```cpp
enum class ShipOrientation { East, West, South, North };

// Rotate a ship-local (dx, dy) to world-offset.
std::pair<int,int> rotate(int dx, int dy, ShipOrientation o) {
    switch (o) {
        case ShipOrientation::East:  return { dx,  dy};
        case ShipOrientation::West:  return {-dx,  dy};
        case ShipOrientation::South: return {-dy,  dx};
        case ShipOrientation::North: return { dy, -dx};
    }
    return {dx, dy};
}
```

All stamping code walks ship-local coordinates (dx from -ship_len/2 to +ship_len/2, dy from -body_half_h to +body_half_h), rotates each one to world offsets via `rotate()`, then adds the center point to get the world tile.

**Skid direction:** The skid is always on the negative-dx side in ship-local (behind the stern). The rotation naturally places it in the correct world direction for each orientation.

---

## 4. Stamp Pipeline

`CrashedShipGenerator::generate(TileMap&, const TerrainChannels&, const MapProperties&, std::mt19937&) → Rect` runs:

1. **Reject Aquatic biome** — return empty `Rect{}`.
2. **Pick class** — either parse `detail_crashed_ship_class` override or roll lore-weighted class.
3. **Pick orientation** — uniform over {N, S, E, W}.
4. **Compute footprint size** (for `PlacementScorer`) — the footprint must fit hull + skid in the ship-axis direction and hull width + a margin in the perpendicular direction. The debris ring extends outside the footprint where needed and is clamped at stamp time (out-of-bounds fragments are skipped). This keeps the footprint small enough that corvettes still fit on the 360×150 map.

   **Ship-axis length** = hull_len + skid_max (worst case skid).
   **Perpendicular width** = hull bounding box height + 6 tiles of margin on each side (for debris + breach clearance).
   
   | Class | Axis length | Perp width |
   |---|---|---|
   | EscapePod | 10 + 18 = 28 | 7 + 12 = 19 |
   | Freighter | 24 + 28 = 52 | 9 + 12 = 21 |
   | Corvette | 36 + 40 = 76 | 11 + 12 = 23 |
   
   After orientation rotation, East/West → `foot_w=axis, foot_h=perp`; North/South → `foot_w=perp, foot_h=axis` (swap).
5. **Call PlacementScorer** with the rotated footprint. If invalid → return empty rect.
6. **Compute hull center point** from the placement footprint, offset so the skid has room behind the stern. For an east-facing ship the hull center sits at `footprint.x + skid_max + hull_len/2` (x) and `footprint.y + perp/2` (y). Other orientations shift accordingly — whichever side the stern points toward, the hull is pushed to the opposite edge of the footprint to leave skid_max tiles of room on the stern side.
7. **Stamp skid mark** (BEFORE hull — hull stamping overwrites any skid tile that ends up under the stern):
   - Roll skid length in `[spec.skid_min, spec.skid_max]`.
   - Walk from `dx = -hull_len/2 - skid_length` to `dx = -hull_len/2`, step +1.
   - For each position, compute noise offset perpendicular `offset = (fbm(i * 0.15) - 0.5) * 4` → range roughly -2..+2.
   - Stamp a 3-wide band: center row always scorched; side rows scorched with 80% probability (gaps).
   - For every stamped tile:
     - `map.remove_fixture(wx, wy)` — clear flora / minerals / debris scatter in the path.
     - `map.set(wx, wy, Tile::IndoorFloor)` — use IndoorFloor as the scorched floor marker (renderer already tints it differently from Floor; if this looks wrong in testing we add a dedicated scorched slot).
   - For each edge of the band, ~30% chance of a flanking `Tile::Wall` rubble fragment (one tile outside the band).
   - The skid also clears fixtures in its 3-tile band regardless of whether each tile got stamped (so flanking fixtures are gone too).
8. **Stamp hull** — for each local (dx, dy):
   - `hull_half_h(dx)` computes tapered half-height.
   - Walls on the boundary (`dy == ±hh || dx == ±ship_half`) with `hull_coverage` probability; otherwise the edge tile is `IndoorFloor` (breached plating).
   - Interior tiles are always `IndoorFloor`.
   - Hull walls are `Tile::StructuralWall` with `glyph_override = 0` (metal slot).
   - `map.remove_fixture()` on every hull tile before stamping (kills any scatter underneath).
9. **Stamp bulkheads** — for each `bulkhead_dx` in the spec, walk dy from -(body_half_h - 1) to +(body_half_h - 1). Place `StructuralWall + glyph_override(0)` with 60% probability except at `dy == 0` (center gap for passage).
10. **Stamp breaches** — roll breach count in `[breach_min, breach_max]`. For each breach, pick random dx within `[-ship_half + 2, +ship_half - 3]`, pick top or bottom hull edge, clear 2-3 consecutive tiles by setting them to `IndoorFloor`.
11. **Stamp debris field** — roll fragment count in `[debris_min, debris_max]`. For each fragment, pick random offset within `±debris_radius` from center. Skip if the tile is inside the hull bounding box, on the skid band, or already structural. Place a 1-2 tile `Tile::Wall` cluster with 60% per-tile probability. `map.remove_fixture()` on the tile first.
12. **Place fixtures** — for each room in `spec.rooms`:
    - Derive the room's local-dx range from `rooms[i]`.
    - Find interior floor tiles within that range.
    - For each fixture type in `fixtures_by_room[i]`, pick a random unoccupied interior floor tile, set it to `Tile::Fixture`, call `map.add_fixture()`.
13. **Dungeon portal roll** — 20% chance (`rng() % 5 == 0`). If yes, place `Tile::Portal` on a random interior floor tile in the middle room (index `rooms.size() / 2`).
14. Return the `placement.footprint` rect.

### Skid environment clearing (detail)

The skid mark is the **defining visual feature**. The clearing logic is intentional:

```cpp
// For each tile (wx, wy) in the skid band (including edges):
if (!in_bounds(wx, wy)) continue;
// Clear any scatter (flora, minerals, debris, grass)
if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
// Scorch the ground (use IndoorFloor as scorch marker)
Tile current = map.get(wx, wy);
if (current != Tile::Water) {  // don't overwrite water
    map.set(wx, wy, Tile::IndoorFloor);
}
```

The ship "plowed through" — the tree was not there before, and the skid is now a visible burn scar cutting through the biome. This reads as crystal-clear visual evidence of the crash from 30 tiles away.

---

## 5. MapProperties Addition

Add to `include/astra/map_properties.h`:

```cpp
// Dev override: force a specific crashed ship class.
// Empty string = auto (lore-weighted). Values: "pod", "freighter", "corvette".
std::string detail_crashed_ship_class;
```

The generator reads this once at the top of `generate()`:

```cpp
ShipClass klass;
if (props.detail_crashed_ship_class == "pod") {
    klass = ShipClass::EscapePod;
} else if (props.detail_crashed_ship_class == "freighter") {
    klass = ShipClass::Freighter;
} else if (props.detail_crashed_ship_class == "corvette") {
    klass = ShipClass::Corvette;
} else {
    klass = pick_ship_class(props.lore_tier, rng);
}
```

---

## 6. Dev Command Integration

### `src/dev_console.cpp`

Extend the `biome_test` arg parser:

```cpp
} else if (args[i] == "ship" || args[i] == "crashed_ship") {
    poi_type = "ship";
} else if (args[i] == "pod" || args[i] == "freighter" || args[i] == "corvette") {
    if (poi_type.empty()) poi_type = "ship";
    poi_style = args[i];
}
```

Extend the success message:

```cpp
} else if (poi_type == "ship") {
    msg += " + crashed ship";
    if (!poi_style.empty()) msg += " (" + poi_style + ")";
}
```

Extend the help text:

```
biome_test <biome> ... [ship [pod|freighter|corvette]]
  ship: crashed ship wreck; class optional (auto = lore-weighted)
```

### `src/game.cpp` `dev_command_biome_test()`

Add a branch matching the outpost/settlement pattern:

```cpp
} else if (poi_type == "ship") {
    props.detail_has_poi = true;
    props.detail_poi_type = Tile::OW_CrashedShip;
    props.detail_crashed_ship_class = poi_style;  // "" / pod / freighter / corvette
    props.lore_tier = 1;  // mid-tier default
}
```

Location name:

```cpp
} else if (poi_type == "ship") {
    loc_name += " + Crashed Ship";
    if (!poi_style.empty()) {
        // Capitalize first letter
        std::string s = poi_style;
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        loc_name += " (" + s + ")";
    }
}
```

No NPC spawning — crashed ships are empty wrecks. The block simply doesn't call any `spawn_*_npcs()` function.

---

## 7. Testing & Verification

Manual verification checklist (no automated tests for POI visuals):

- [ ] `biome_test grassland ship pod` produces a tiny wreck with a short clear skid.
- [ ] `biome_test grassland ship freighter` produces a medium wreck with 3 rooms and medium skid.
- [ ] `biome_test grassland ship corvette` produces a large wreck with 5 rooms and a long skid.
- [ ] `biome_test grassland ship` picks a class via lore weighting (tier 1 = freighter mostly).
- [ ] Ship orientation varies across regenerations (N/S/E/W).
- [ ] Skid mark clearly cuts through trees, grass, flora fixtures — no scatter remains in the skid band.
- [ ] Hull has visible breaches (2-5 per class), ~75% plating coverage.
- [ ] Crates appear in the cargo hold room of freighter/corvette.
- [ ] Console appears in the cockpit.
- [ ] Bunks appear in corvette quarters.
- [ ] Debris fragments scatter around the ship.
- [ ] `biome_test aquatic ship` returns no POI (empty footprint).
- [ ] All other biomes produce ships successfully.
- [ ] Portal appears ~20% of the time in the middle room.

---

## 8. Follow-ups (Roadmap)

Added to `docs/roadmap.md`:

- [x] Crashed Ship POI generator (EscapePod / Freighter / Corvette classes, skid marks, debris fields)
- [ ] **Crashed ship dungeon theming** — when the 20% portal hits, the underground dungeon should be wreck-themed (buried hull sections, more debris, possibly xenomorph nests)
- [ ] **Crashed ship kind variants** — pirate / civilian / military / alien flavors via scatter + fixture palette substitutions
- [ ] **Crashed ship lore logs** — readable captain's log fixture on the cockpit console
- [ ] **Aquatic crashed ships** — partially submerged hull rendering
- [ ] **Haunted wrecks** — optional creature spawning inside wrecks (ties to world threat level)

---

## 9. Out of Scope

- NPC/creature spawning inside the wreck (empty wrecks for first pass).
- Aquatic crashed ships (deferred — ships on water read weird in ASCII).
- Dungeon content when the portal hits (uses whatever default dungeon generator runs).
- Readable lore log content on the cockpit console (just a Console fixture, no text).
- Crashed ship kind variants (pirate / civilian / military / alien).
- Diagonal (8-direction) ship orientation.
- Escape pod trails next to larger wrecks.
- Exterior scatter around the ship beyond the debris field (craters, footprints, etc.).
- Save/load persistence of which rooms the player has entered / looted.
