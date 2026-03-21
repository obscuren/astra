# Plan: Overhaul Detail Map POI Stamps

## Context

All 6 POI stamps in `detail_map_generator.cpp` `place_features()` are tiny (~10x10 at most) on an 80x50 detail map. They feel underwhelming — especially Ruins which is just three 4x4 blocks. We're tackling them one by one, starting with Ruins.

---

## TODO Tracker

- [x] **1. Ruins** — largest overhaul needed
- [x] **2. Crashed Ship** — needs longer fuselage, debris field, breached sections
- [x] **3. Outpost** — bare 5x5 box, needs rooms, crates, defensive perimeter
- [x] **4. Cave Entrance** — 3x3 box, needs rocky outcrop, scattered boulders
- [x] **5. Settlement** — decent but could use multiple buildings, paths, market area
- [ ] **6. Landing Pad** — functional but plain, could use pad markings, perimeter lights

---

## Step 1: Ruins Overhaul (DONE)

### Problem
- Three tiny 4x4 blocks with random gaps + 7x3 corridor = ~15x6 footprint on 80x50 map
- `OW_Ruins` terrain has no entry in `terrain_wall_density()`, defaults to 0.10 — almost no walls in surrounding terrain
- No fixtures, no variety, no sense of scale

### File: `src/generators/detail_map_generator.cpp`

### Changes

**A. Increase base wall density for ruins terrain**
In `terrain_wall_density()`, added:
```
case Tile::OW_Ruins: return 0.20f;
```

**B. Replaced the `OW_Ruins` case in `place_features()`**
New generation algorithm:

1. **Central complex** (~20x16): A main building footprint with 3-5 rooms of varying sizes (4x4 to 8x6). Rooms connected by doorways. Walls placed with ~70% probability (crumbling effect).

2. **Outer walls / perimeter**: Partial perimeter wall around the complex (~24x20) with large random gaps (collapsed sections). Wall probability ~40%.

3. **Extending corridors**: 2-3 corridors radiating outward from the complex toward map edges, 3-wide, with crumbling walls (probability ~50%). Length 8-15 tiles.

4. **Scattered wall fragments**: 4-8 small wall clusters (2x2 to 3x3) placed randomly within ~15 tiles of center, ~50% wall probability. Represents collapsed outbuildings.

5. **Fixtures**: Debris fixtures scattered in rooms. A Console fixture in one room (ancient terminal). A Crate in another.

6. **Portal**: `>` portal in one of the interior rooms for dungeon access.

---

## Step 2: Crashed Ship (DONE)

### Problem
- Diamond-shaped hull (~9x5) with hollow interior — too small and symmetrical
- Single debris fixture at center, no sense of impact or wreckage

### Changes

**A. Wall density for crashed ship terrain**
In `terrain_wall_density()`, added:
```
case Tile::OW_CrashedShip: return 0.15f;
```

**B. Replaced the `OW_CrashedShip` case in `place_features()`**
New generation algorithm:

1. **Main fuselage (~20x6)**: Elongated east-west hull with tapered nose (narrowing over last 4 tiles) and slightly narrowed stern. Hull walls at ~75% probability for breached plating effect. Slight diagonal offset (crash skid) randomized per seed.

2. **3 interior sections**: Cockpit (front, around dx +4 to +10), mid-section (center, dx -4 to +4), engine bay (rear, dx -10 to -4). Separated by partial internal bulkheads with ~60% wall probability and center gaps for passage.

3. **2-3 breach points**: Random hull wall sections cleared to Floor in 2-3 tile runs, creating "torn open" visual effect on top or bottom hull edge.

4. **Impact gouge**: 3-wide floor trench extending 8-12 tiles behind the stern (west). Side tiles have ~40% wall probability for churned rubble berms.

5. **Debris field (~30x20)**: 8-15 small wall fragments (1x1 to 2x2) scattered within ~15 tiles of center at 60% wall probability. 6-10 Debris fixtures (passable decorative ',') scattered on floor tiles near the ship.

6. **Fixtures**: Console in cockpit, 1-2 Crates in mid-section, Conduit in engine bay, scattered Debris decorations in debris field.

7. **Portal**: `>` in mid-section for dungeon access.

---

## Step 3: Outpost (DONE)

### Problem
- Bare 5x5 box with a door, nothing inside

### Changes

**A. Wall density for outpost terrain**
In `terrain_wall_density()`, added:
```
case Tile::OW_Outpost: return 0.18f;
```

**B. Replaced the `OW_Outpost` case in `place_features()`**
New generation algorithm:

1. **Defensive perimeter (~24x18)**: Outer wall ring at ~60% probability (weathered fortification). 3-wide gate gaps on north and south sides.

2. **Guard towers (4 corners)**: 3x3 solid wall blocks at each perimeter corner with 1x1 interior floor (lookout position).

3. **Main building (10x8, centered)**: Walls at ~85% probability (well-maintained). South doorway. Internal dividing wall splits into command room (west, Console fixture + Portal) and barracks (east, Bunk fixture).

4. **Storage shed (6x4, northeast)**: Walls at ~75% probability. West doorway facing courtyard. 2 Crate fixtures and 1 Rack fixture inside.

5. **Courtyard paths**: 3-wide floor paths connecting north/south gates to main building and main building to shed. Natural biome terrain preserved between paths and structures.

6. **Debris**: 3-5 Debris decorations scattered on floor tiles within the perimeter.

---

## Step 4: Cave Entrance (DONE)

### Problem
- 3x3 wall box with portal — barely visible

### Changes

**A. Wall density for cave terrain**
In `terrain_wall_density()`, added:
```
case Tile::OW_CaveEntrance: return 0.25f;
```

**B. Replaced the `OW_CaveEntrance` case in `place_features()`**
New generation algorithm:

1. **Rocky outcrop (~16x12)**: Elliptical wall formation using distance-from-center with noise perturbation. Edge weathering at ~80% wall probability, solid deeper inside. Threshold 0.85 with noise offset.

2. **Cave mouth approach from south**: 3-wide corridor narrowing to 1-wide as it approaches the center portal. Creates a natural funnel effect.

3. **Scattered boulders (6-10)**: 1x1 or 2x2 wall clusters placed outside the main outcrop (distance > 0.9) at 50% probability.

4. **Portal**: At center for dungeon access.

5. **Debris decorations (3-5)**: Scattered on floor tiles near the entrance approach.

---

## Step 5: Settlement (DONE)

### Problem
- Single 7x5 building — doesn't feel like a settlement

### Changes

**A. Wall density for settlement terrain**
In `terrain_wall_density()`, added:
```
case Tile::OW_Settlement: return 0.10f;
```
Low density — settlements are cleared areas.

**B. Replaced the `OW_Settlement` case in `place_features()`**
New generation algorithm:

1. **Central plaza (~8x6)**: Pure floor rectangle centered on cx, cy. 3-5 Stool fixtures scattered inside (outdoor seating).

2. **Main hall (10x6, north of plaza)**: Walls at ~90% probability. South doorway facing plaza. Console fixture (admin terminal) and Portal for dungeon access.

3. **Market building (8x5, east of plaza)**: Walls at ~85% probability. West doorway facing plaza. 2 Crate fixtures and 1 Table fixture (merchant counter).

4. **Dwelling buildings (2-3 small ~5x4)**: Fixed positions west/south of plaza. Walls at ~80% probability. Doorways facing nearest path. Each contains 1 Bunk fixture.

5. **Pathways**: 2-wide floor paths radiating from plaza — north (to hall), east (to market), south and west (entry roads toward map edges).

6. **Market stalls (2-3, open-air)**: Table + Crate combos south of plaza on floor tiles.

7. **Debris (3-5)**: Scattered on floor tiles throughout the settlement area.

---

## Step 6: Landing Pad (TODO)

### Problem
- Flat 7x5 floor area with single fixture — plain

### Planned Changes
- Pad markings (wall tiles forming landing pad outline)
- Perimeter lights (fixtures around edges)
- Control tower (small walled structure nearby)
- Fuel/supply area with crates
- ShipTerminal fixture retained

---

## Verification

1. `cmake -B build -DDEV=ON && cmake --build build`
2. Warp to a planet, find POI tile on overworld
3. Enter `>` — stamp should be substantially larger with visible structure
4. Enter same location with same seed — layout identical (deterministic)
5. Different seeds produce different layouts
