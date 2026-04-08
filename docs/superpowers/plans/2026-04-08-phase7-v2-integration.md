# Phase 7: v2 Generator Integration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the v1 detail map generator with v2 in normal gameplay. One 360x150 map per overworld tile, viewport-based zone transitions, overworld ruins rendered as baroque pipe characters.

**Architecture:** The 360x150 map is always fully loaded. The 3x3 zone grid (120x50 each) is a viewport concept — transitions within a tile just move the camera. Crossing an overworld tile boundary saves/loads a full 360x150 map. LocationKey drops zone_x/zone_y. Unimplemented POI types generate as terrain-only (stubbed).

**Tech Stack:** C++20, existing TileMap/WorldManager/MapGenerator infrastructure.

---

## File Structure

### Modified Files

| File | Changes |
|------|---------|
| `src/map_generator.cpp` | Change default DetailMap dimensions to 360x150; switch factory to v2 |
| `src/game_world.cpp` | Rewrite `build_detail_props` (no zone-based neighbor sampling), `enter_detail_map` (single map), `transition_detail_edge` (viewport transitions within tile, full map gen on tile boundary), `exit_dungeon_to_detail`, `enter_lost_detail`, `save_current_location` |
| `include/astra/world_manager.h` | Change LocationKey to drop zone_x/zone_y; add viewport zone tracking |
| `src/generators/poi_phase.cpp` | Stub unimplemented POI types (return empty rect instead of skipping) |
| `src/terminal_theme.cpp` | Overworld ruin tiles: baroque pipe glyphs with neighbor connections |
| `src/map_renderer.cpp` | Overworld ruins: encode neighbor ruin mask into seed for pipe rendering |
| `src/game.cpp` | Remove v2 forward declaration (now in factory); update biome_test to match |

---

## Task 1: Switch Factory and Default Dimensions

**Files:**
- Modify: `src/map_generator.cpp:355-400`

- [ ] **Step 1: Change default DetailMap dimensions**

In `default_properties(MapType type)` at line 362-363, change:
```cpp
    p.width = 360;
    p.height = 150;
```

- [ ] **Step 2: Switch factory to v2**

At the top of `map_generator.cpp`, add forward declaration:
```cpp
std::unique_ptr<MapGenerator> make_detail_map_generator_v2();
```

At line 399-400, change the DetailMap case:
```cpp
    case MapType::DetailMap:
        return make_detail_map_generator_v2();
```

- [ ] **Step 3: Clean up game.cpp biome_test**

In `src/game.cpp`, remove the forward declaration of `make_detail_map_generator_v2` (line 428) and the explicit dimension override (lines 443-444, `props.width = 360; props.height = 150;` — now default). Change line 474 from `make_detail_map_generator_v2()` to `create_generator(MapType::DetailMap)`.

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Verify biome_test still works**

Run the game, open dev console: `biome_test forest ruins baroque 0.2`
Expected: Same as before (v2 generator, 360x150).

- [ ] **Step 6: Commit**

```bash
git add src/map_generator.cpp src/game.cpp
git commit -m "feat(phase7): switch detail map factory to v2, default 360x150"
```

---

## Task 2: Simplify LocationKey (Drop Zone Coordinates)

**Files:**
- Modify: `include/astra/world_manager.h:20-23`
- Modify: `src/game_world.cpp` (all LocationKey construction sites)

- [ ] **Step 1: Update LocationKey typedef**

In `world_manager.h`, change the key to 7 elements (drop zone_x, zone_y):
```cpp
// LocationKey: {system_id, body_index, moon_index, is_station, ow_x, ow_y, depth}
using LocationKey = std::tuple<uint32_t, int, int, bool, int, int, int>;
```

Keep `zones_per_tile = 3` (still used for viewport math).

Update the sentinel keys:
```cpp
static inline const LocationKey ship_key = {0, -2, -1, false, -1, -1, 0};
static inline const LocationKey maintenance_key = {0, -3, -1, false, -1, -1, 0};
```

- [ ] **Step 2: Update all LocationKey construction sites in game_world.cpp**

Search for every place a LocationKey tuple is constructed and remove the last two elements (zone_x, zone_y). These are at approximately:
- `save_current_location` (line ~30): detail map key
- `enter_detail_map` (line ~510): detail_key
- `exit_dungeon_to_detail` (line ~805): detail_key
- `transition_detail_edge` (line ~915): new location key
- `enter_lost_detail` (line ~1509): detail_key

Each currently looks like:
```cpp
LocationKey key = {system_id, body_index, moon_index, false, ow_x, ow_y, depth, zone_x, zone_y};
```

Change to:
```cpp
LocationKey key = {system_id, body_index, moon_index, false, ow_x, ow_y, depth};
```

Also check `game_world.cpp` for any other LocationKey constructions (station key, overworld key, dungeon key) — those don't have zone coords but the tuple size changed so they need updating if they relied on implicit zeros.

- [ ] **Step 3: Build and fix any compilation errors**

The tuple size change will cause compile errors at every site that constructs a LocationKey with the wrong number of elements. Fix each one.

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add include/astra/world_manager.h src/game_world.cpp
git commit -m "feat(phase7): simplify LocationKey — one entry per overworld tile"
```

---

## Task 3: Rewrite build_detail_props for Full-Tile Generation

**Files:**
- Modify: `src/game_world.cpp` — `build_detail_props` function (~line 423)

- [ ] **Step 1: Remove zone-based neighbor sampling**

The current function only samples N/S/E/W neighbors at the outer edges of the 3x3 zone grid. With a full 360x150 map, neighbors are always the adjacent overworld tiles. Remove the zone_x/zone_y conditional checks (lines 457-468).

Replace with unconditional neighbor sampling:
```cpp
// Sample all 4 overworld neighbors for edge blending
auto get_ow = [&](int x, int y) -> Tile {
    if (x < 0 || x >= ow_map.width() || y < 0 || y >= ow_map.height())
        return Tile::Empty;
    return ow_map.get(x, y);
};
props.detail_neighbor_n = get_ow(ow_x, ow_y - 1);
props.detail_neighbor_s = get_ow(ow_x, ow_y + 1);
props.detail_neighbor_w = get_ow(ow_x - 1, ow_y);
props.detail_neighbor_e = get_ow(ow_x + 1, ow_y);
```

- [ ] **Step 2: Remove center-zone-only POI check**

Currently POIs only generate when `zx == 1 && zy == 1`. With a full map, POIs always generate. Remove the zone check (lines 470-471) — just check the overworld tile type directly.

- [ ] **Step 3: Set zone_x/zone_y to 0**

The v2 generator still reads `props.zone_x` and `props.zone_y` for seed computation. Set both to 0 (no zone variation — one seed per tile).

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(phase7): build_detail_props generates full-tile props, no zone grid"
```

---

## Task 4: Rewrite enter_detail_map for Single 360x150 Map

**Files:**
- Modify: `src/game_world.cpp` — `enter_detail_map` function (~line 499)

- [ ] **Step 1: Rewrite enter_detail_map**

Key changes:
- Set zone to (1,1) initially (player starts in center viewport section)
- LocationKey without zone_x/zone_y
- Generate 360x150 map via factory (which now returns v2)
- Place player in the center of the center zone section (i.e., around x=180, y=75)
- Use `spawn_settlement_npcs_v2` instead of v1 spawner
- Seed computation: remove zone_x/zone_y mixing (one seed per overworld tile)

The cache check stays the same — if we've visited this tile before, restore the cached 360x150 map.

- [ ] **Step 2: Update NPC spawning to v2**

Replace `spawn_settlement_npcs(...)` call with `spawn_settlement_npcs_v2(...)`. The v2 spawner takes different parameters — check the signature in `npc_spawner.h` line 38.

- [ ] **Step 3: Build and test**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

Start a new game, land on a planet, enter a detail map from the overworld. Should see a 360x150 map with v2 terrain. The viewport should show a 120x50 section centered on the player.

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(phase7): enter_detail_map generates single 360x150 v2 map"
```

---

## Task 5: Viewport-Based Zone Transitions

**Files:**
- Modify: `src/game_world.cpp` — `transition_detail_edge` function (~line 843)

- [ ] **Step 1: Rewrite transition_detail_edge**

The function currently saves and loads separate maps per zone. Rewrite it to:

**Intra-tile transitions** (moving between zones within the same 360x150 map):
- Calculate new zone from delta
- If still within 0..2 on both axes: just update `zone_x_/zone_y_`, place player at opposite edge of the new zone section, recompute camera. No save/load/generate. The map stays loaded.
- Player position conversion: if moving east from zone (0,y), player.x goes from ~119 to ~120 (crossing from section 0 to section 1 within the same map). Actually simpler: player.x = `new_zone_x * 120 + (dx > 0 ? 0 : 119)`, player.y preserved.

**Cross-tile transitions** (crossing to a different overworld tile):
- Same as before but with the new single-map approach: save current 360x150, generate/restore neighbor's 360x150, place player at matching edge.
- New zone wraps: e.g., going east from zone (2,y) → zone (0,y) of the next overworld tile. Player.x = 0 + a few tiles.

- [ ] **Step 2: Handle edge cases**

- Diagonal movement (dx != 0 && dy != 0): currently handled, keep working
- Overworld bounds check: keep the existing bounds/passability check against cached overworld
- Time cost: keep 5 for intra-tile, 15 for cross-tile

- [ ] **Step 3: Build and test**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

Test: enter a detail map, walk to the edge of the 120x50 viewport. Should transition to the adjacent section of the same 360x150 map (no loading screen, instant). Walk to the edge of the overworld tile boundary — should load a new map.

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(phase7): viewport-based zone transitions within single map"
```

---

## Task 6: Update exit_dungeon_to_detail and enter_lost_detail

**Files:**
- Modify: `src/game_world.cpp` — both functions

- [ ] **Step 1: Update exit_dungeon_to_detail**

- LocationKey without zone coords
- If not cached, generate full 360x150 via factory
- Place player in the zone section they entered from (use saved zone_x/zone_y to compute position within the big map)

- [ ] **Step 2: Update enter_lost_detail**

- Random zone still picks a random section (0-2, 0-2) for the viewport
- LocationKey without zone coords
- Generate full 360x150 if not cached
- Place player at random position within the selected zone section

- [ ] **Step 3: Build and test**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(phase7): update dungeon exit and lost detail for single-map approach"
```

---

## Task 7: Stub Unimplemented POI Types

**Files:**
- Modify: `src/generators/poi_phase.cpp`

- [ ] **Step 1: Add stub branches for all POI types**

Currently poi_phase.cpp handles OW_Ruins and OW_Settlement. Add explicit no-op branches for the others so they don't fall through unexpectedly:

```cpp
if (props.detail_poi_type == Tile::OW_Ruins) {
    // ... existing ruin generator
}

if (props.detail_poi_type == Tile::OW_Settlement) {
    // ... existing settlement code
}

// Stubbed POI types — generate terrain only, POI comes later
if (props.detail_poi_type == Tile::OW_CrashedShip ||
    props.detail_poi_type == Tile::OW_Outpost ||
    props.detail_poi_type == Tile::OW_CaveEntrance ||
    props.detail_poi_type == Tile::OW_Landing ||
    props.detail_poi_type == Tile::OW_Beacon ||
    props.detail_poi_type == Tile::OW_Megastructure) {
    return {};  // terrain-only for now
}
```

- [ ] **Step 2: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add src/generators/poi_phase.cpp
git commit -m "feat(phase7): stub unimplemented POI types in v2 poi_phase"
```

---

## Task 8: Overworld Ruin Pipe Rendering

**Files:**
- Modify: `src/map_renderer.cpp` — overworld rendering path
- Modify: `src/terminal_theme.cpp` — OW_Ruins glyph resolution

- [ ] **Step 1: Encode ruin neighbor mask in map_renderer.cpp**

In the overworld rendering path (around line 80-117), when rendering an `OW_Ruins` tile, check the 4 adjacent tiles on the overworld map. Encode which neighbors are also `OW_Ruins` into the seed byte (same N=1,S=2,E=4,W=8 bitmask approach as detail map pipe walls).

```cpp
if (tile_at == Tile::OW_Ruins) {
    uint8_t nb = 0;
    if (my > 0 && rc.world.map().get(mx, my-1) == Tile::OW_Ruins) nb |= 0x01;
    if (my+1 < rc.world.map().height() && rc.world.map().get(mx, my+1) == Tile::OW_Ruins) nb |= 0x02;
    if (mx+1 < rc.world.map().width() && rc.world.map().get(mx+1, my) == Tile::OW_Ruins) nb |= 0x04;
    if (mx-1 >= 0 && rc.world.map().get(mx-1, my) == Tile::OW_Ruins) nb |= 0x08;
    desc.seed = nb;  // bits 0-3 = neighbor mask
}
```

- [ ] **Step 2: Render OW_Ruins as baroque pipes in terminal_theme.cpp**

Replace the current OW_Ruins glyph handler (lines 218-226) with neighbor-aware baroque pipe rendering. Use the same `baroque_conn[]` table from the detail map ruin rendering:

```cpp
case Tile::OW_Ruins: {
    static const char* baroque_conn[] = {
        // same 16-entry table as detail map baroque pipes
        // 0=■, 1=║, 2=║, 3=║, 4=═, 5=╚, 6=╔, 7=╠,
        // 8=═, 9=╝, 10=╗, 11=╣, 12=═, 13=╩, 14=╦, 15=╬
    };
    uint8_t nb = seed & 0x0F;
    const char* utf8 = baroque_conn[nb];
    Color c = remembered ? bc.remembered : static_cast<Color>(178);  // warm gold
    return {'#', utf8, c, Color::Default};
}
```

- [ ] **Step 3: Build and test**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

Start a game, navigate to a system with ruins on the overworld. Ruin tiles should display as connected baroque pipe characters in warm gold.

- [ ] **Step 4: Commit**

```bash
git add src/map_renderer.cpp src/terminal_theme.cpp
git commit -m "feat(phase7): overworld ruins render as connected baroque pipe characters"
```

---

## Notes

### What This Plan Does NOT Cover

- **Neighbor edge data caching** — the spec mentions reading cached neighbor edges for seamless terrain blending at overworld tile boundaries. The v2 generator already handles this via `detail_neighbor_n/s/e/w` in MapProperties, which `build_detail_props` populates from the overworld. No additional caching mechanism is needed.
- **Deleting the v1 generator** — kept as fallback. Can be removed once v2 is stable in gameplay.
- **Save/load serialization changes** — LocationKey change affects serialization. If save files exist with the old 9-tuple key, they'll be incompatible. This is acceptable during development.
