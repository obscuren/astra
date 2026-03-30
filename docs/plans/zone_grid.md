# Plan: 3x3 Zone Grid Per Overworld Tile

## Context

Each overworld tile currently maps to a single 80x50 detail map. Walking off the edge transitions to the next overworld tile's detail map. This makes overworld tiles feel small — a single screen.

**Goal:** Each overworld tile becomes a **3x3 grid of zones**, each 120x50. Entering an overworld tile drops you in the center zone (1,1). Walking off-screen transitions to an adjacent zone within the same tile (Zelda-style). Walking off the 3x3 grid's outer edge transitions to the neighboring overworld tile's corresponding edge zone.

This makes each overworld tile 9x larger in explorable area (360x150 effective) while keeping the Zelda screen-flip feel.

---

## Phase 1: Data Model — Zone Tracking

### `include/astra/world_manager.h`

**Add zone position tracking:**
```cpp
int& zone_x() { return zone_x_; }  // 0, 1, 2 within current overworld tile
int& zone_y() { return zone_y_; }
```
Private members: `int zone_x_ = 1; int zone_y_ = 1;` (default to center)

**Extend LocationKey** — add 2 more ints for zone coordinates:
```cpp
using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int, int, int>;
//                              sysid  body moon flag  ow_x ow_y depth zone_x zone_y
```

Update static keys:
```cpp
static inline const LocationKey ship_key = {0, -2, -1, false, -1, -1, 0, -1, -1};
static inline const LocationKey maintenance_key = {0, -3, -1, false, -1, -1, 0, -1, -1};
```

### All LocationKey construction sites (~24 occurrences across 5 files)
Every `LocationKey{...}` literal gains two trailing ints. Most detail map keys use `zone_x_, zone_y_`. Overworld/station/ship keys use `-1, -1`.

**Files to update:**
- `src/game_world.cpp` — 19 occurrences (enter_detail_map, transition_detail_edge, exit_detail_to_overworld, save_current_location, enter_dungeon_from_detail, etc.)
- `src/save_file.cpp` — 2 occurrences (save/load location cache)
- `src/dialog_manager.cpp` — 1 occurrence
- `src/dev_console.cpp` — 1 occurrence
- `src/quests/missing_hauler.cpp` — 1 occurrence

---

## Phase 2: Zone Size Change

### `src/map_generator.cpp` — `default_properties()`
Change DetailMap defaults:
```cpp
case MapType::DetailMap:
    p.width = 120;   // was 80
    p.height = 50;   // stays 50
    break;
```

---

## Phase 3: Zone Transition Logic

### `src/game_world.cpp` — Rewrite `transition_detail_edge()`

Current behavior: walking off edge → next overworld tile.
New behavior: walking off edge → next zone within same tile, OR next overworld tile if at grid boundary.

```
Zone grid for overworld tile (5,3):
  (0,0) (1,0) (2,0)
  (0,1) (1,1) (2,1)   ← player enters at (1,1)
  (0,2) (1,2) (2,2)
```

**New logic:**
```cpp
void Game::transition_detail_edge(int dx, int dy) {
    int new_zx = world_.zone_x() + dx;
    int new_zy = world_.zone_y() + dy;

    // Still within the 3x3 grid? → intra-tile zone transition
    if (new_zx >= 0 && new_zx < 3 && new_zy >= 0 && new_zy < 3) {
        save_current_location();
        world_.zone_x() = new_zx;
        world_.zone_y() = new_zy;
        // Build/restore detail map for new zone
        // LocationKey uses (ow_x, ow_y, depth=0, zone_x, zone_y)
        // Place player at opposite edge, preserving other axis
        // No overworld passability check needed (same tile)
        // Lighter time cost (5 ticks vs 15 for overworld transition)
        return;
    }

    // Crossed the 3x3 boundary → overworld tile transition
    // Zone wraps to the corresponding edge of the neighbor tile
    // while PRESERVING the perpendicular zone coordinate.
    //
    // Examples:
    //   tile(5,3) zone(2,1) walk east  → tile(6,3) zone(0,1) — same zone row
    //   tile(5,3) zone(1,0) walk north → tile(5,2) zone(1,2) — same zone col
    //   tile(5,3) zone(0,2) walk south → tile(5,4) zone(0,0) — same zone col
    //   tile(5,3) zone(0,1) walk west  → tile(4,3) zone(2,1) — same zone row
    //
    int ow_dx = 0, ow_dy = 0;
    if (new_zx < 0)  { ow_dx = -1; new_zx = 2; }  // wrap to east edge of western neighbor
    if (new_zx >= 3) { ow_dx = 1;  new_zx = 0; }  // wrap to west edge of eastern neighbor
    if (new_zy < 0)  { ow_dy = -1; new_zy = 2; }  // wrap to south edge of northern neighbor
    if (new_zy >= 3) { ow_dy = 1;  new_zy = 0; }  // wrap to north edge of southern neighbor
    // new_zx/new_zy now hold the ENTRY zone in the neighbor tile
    // The perpendicular axis is unchanged (zone row/col preserved)

    // Check overworld bounds & passability (existing logic)
    // Update overworld_x/y += ow_dx/ow_dy
    // Set zone_x = new_zx, zone_y = new_zy
    // Build/restore detail map for the new zone in the new overworld tile
    // Place player at opposite edge, preserving other axis position
    // Full time cost (15 ticks)
}
```

### `src/game_world.cpp` — Update `enter_detail_map()`

- Set `zone_x_ = 1; zone_y_ = 1;` (center zone)
- LocationKey includes zone coordinates
- Seed generation incorporates zone position:
  ```cpp
  ^ (static_cast<unsigned>(zone_x) * 4517u)
  ^ (static_cast<unsigned>(zone_y) * 5381u)
  ```

### `src/game_world.cpp` — Update `exit_detail_to_overworld()`
- Reset `zone_x_ = 1; zone_y_ = 1;`

### `src/game_world.cpp` — Update `build_detail_props()`
- Add zone_x, zone_y parameters
- Neighbor sampling changes: only sample overworld neighbors at grid edges
  - Zone (0,y) west neighbor = overworld tile to the west
  - Zone (1,y) west neighbor = same overworld tile (internal edge — no terrain change)
  - Zone (2,y) east neighbor = overworld tile to the east
- Interior zones have no neighbor blending (all same terrain)

### `src/game_world.cpp` — Update `enter_dungeon_from_detail()`
- Must preserve zone_x/zone_y so exiting dungeon returns to correct zone

---

## Phase 4: Save/Load

### `src/save_file.cpp`
- Version 16: serialize `zone_x_`, `zone_y_` in world section
- LocationKey serialization gains 2 extra ints per entry
- Backward compat: version <16 defaults zone to (1,1) and appends (-1,-1) to old keys

### `include/astra/save_file.h`
- Bump `version = 16`

---

## Phase 5: Detail Map Generator Adjustments

### `src/generators/detail_map_generator.cpp`
- Width changes from 80 to 120 (via MapProperties, no hardcoded values)
- Edge blending only at overworld-boundary edges (zones at grid edges 0 or 2)
- Interior zone edges: continuous terrain, no blending needed
- POI placement: only in center zone (1,1) by default — settlements, ruins, etc. appear there
  - Could later extend POIs across multiple zones

### Props additions (`include/astra/map_properties.h`)
```cpp
int zone_x = 1;  // which zone in the 3x3 grid
int zone_y = 1;
```
Used by the generator to decide edge blending behavior.

---

## Files Modified

| File | Changes |
|------|---------|
| `include/astra/world_manager.h` | Add zone_x_, zone_y_ members + accessors; extend LocationKey tuple |
| `include/astra/map_properties.h` | Add zone_x, zone_y fields |
| `src/game_world.cpp` | Rewrite transition_detail_edge(); update enter_detail_map(), exit_detail_to_overworld(), build_detail_props(), enter_dungeon_from_detail(), save_current_location() |
| `src/game_interaction.cpp` | No change (try_move edge detection unchanged) |
| `src/generators/detail_map_generator.cpp` | Edge blending conditional on zone position |
| `src/map_generator.cpp` | Default detail map width 80→120 |
| `src/save_file.cpp` | Version 16: zone persistence + extended LocationKey |
| `include/astra/save_file.h` | Bump version |
| `src/dialog_manager.cpp` | Update 1 LocationKey literal |
| `src/dev_console.cpp` | Update 1 LocationKey literal |
| `src/quests/missing_hauler.cpp` | Update 1 LocationKey literal |

---

## Verification

1. Build compiles with `-DDEV=ON`
2. Enter overworld tile → spawns in center zone (1,1) of 120x50 map
3. Walk east off screen → transitions to zone (2,1), player appears on west edge at same Y
4. Walk east again off zone (2,1) → transitions to NEXT overworld tile's zone (0,1)
5. Walk west from zone (0,1) → returns to previous overworld tile's zone (2,1)
6. Walk north/south transitions work the same way
7. Exit to overworld (`<`) from any zone → returns to overworld
8. Re-enter same tile → center zone (1,1) again
9. Previously visited zones are cached and restored (NPCs, items persist)
10. Save/load preserves zone position and all cached zones
11. Dungeon entry from detail map → exit returns to correct zone
12. POIs (settlements, ruins) appear in center zone
13. Edge blending only at overworld-boundary zone edges
