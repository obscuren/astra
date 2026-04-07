# Structured Furniture Placement & Settlement NPCs — Design Spec

Replaces the current random-scatter furniture system with rule-based placement. Adds variety-driven NPC spawning that scales with settlement size and civ style. Shared system reusable across all POI types.

## Part 1: Furniture Placement

### Placement Rules

Each furniture item follows one of six placement rules:

#### 1. TableSet
Place a table with a bench/seat on each side as a single unit. Multiple sets arranged in rows with 2-tile gaps between rows. Placed in the center area of the room.

```
║o║  ║o║
║o║  ║o║
║o║  ║o║

║ = bench   o = table
```

#### 2. WallUniform
Distribute items evenly along ALL walls (not clustered on one side). Walk the full perimeter, place items at regular intervals. Skip within 2 tiles of doors and corners.

```
####+#####+#####
# |    |    | #
#              #
#              #
# |    |    | #
################

| = rack/display   + = door
```

#### 3. WallShelf
3-tile shelf structures against walls. Orientation follows the wall direction. Middle tile is an item slot (future: lootable).

Against east/west walls (vertical):
```
#║   ← shelf (brown)
#~   ← item slot (colored)
#║   ← shelf (brown)
```

Against north/south walls (horizontal):
```
######
 ═~═
```

Skip near doors and corners. Space shelves at least 4 tiles apart.

#### 4. Corner
One bulky item per corner (storage crates, barrels). Skip corners within 2 tiles of a door.

```
#####+##########
#=           =#
#              #
#              #
#=           =#
################

= = storage in corners
```

#### 5. Anchor
NPC-critical fixtures placed first at prominent positions. Console/terminal goes at the back wall center (opposite the primary door). These establish the room's purpose.

#### 6. Center
Free-standing items in the open floor area. Used for standalone furniture that doesn't pair with anything (standalone stool, debris in ruined buildings).

### Furniture Groups

Palettes define groups instead of flat entries:

```cpp
enum class PlacementRule : uint8_t {
    TableSet,
    WallUniform,
    WallShelf,
    Corner,
    Anchor,
    Center,
};

struct FurnitureGroup {
    PlacementRule rule;
    FixtureType primary;        // main item (table, shelf, console)
    FixtureType secondary;      // paired item (bench for TableSet)
    int min_count = 1;
    int max_count = 1;
    float frequency = 1.0f;     // probability this group appears at all
};
```

### Per-Building Palettes

**MainHall:**
- Anchor: Console (1) — back wall center
- TableSet: Table + Bench (2-4 sets)
- WallShelf: BookCabinet (2-3)
- WallUniform: Display racks (2-4)
- Corner: Storage (1-2)

**Market:**
- Anchor: Table (1) — merchant counter, back wall center
- WallUniform: Display racks (4-6)
- WallShelf: Shelf (2-3)
- Corner: Storage (2-3)

**Dwelling:**
- Anchor: Bunk (1) — against back wall
- TableSet: Table + Bench (1 set)
- WallUniform: CampStove (1), BookCabinet (1)
- Corner: Storage (1-2)

**Distillery:**
- Anchor: Console (1) — control panel
- WallUniform: Conduit (3-5)
- Center: Table (1) — work surface
- Corner: Storage (2-4)

**Lookout:**
- Anchor: Knowledge fixture (1) — scope/terminal
- Center: Seating (1)
- Corner: Storage (1)

**Workshop:**
- Anchor: Table (1) — main workbench, center
- WallUniform: Display racks (2-3), Conduit (1-2)
- Corner: Storage (2-3)

**Storage:**
- Corner: Storage (4) — all corners
- WallUniform: Shelf (3-5)
- WallUniform: Storage (2-4)

### Placement Algorithm

Process groups in this order:

1. **Narrow passage detection** — scan room for any interior section narrower than 4 tiles. Mark those tiles as no-furniture zones.
2. **Anchors** — place NPC-critical fixtures at prominent positions (back wall center).
3. **TableSets** — place table+bench units in center area, in rows, 2-tile gaps.
4. **WallShelves** — walk perimeter, place 3-tile structures at regular intervals.
5. **WallUniform** — walk perimeter, fill remaining wall slots with even spacing.
6. **Corner** — one item per available corner.
7. **Center** — remaining items in open floor.
8. **Walkability verify** — BFS from each door to room center. Remove any furniture that blocks a 2-wide path.

### Bench Rendering

Benches use `║` (vertical double pipe) glyph in brown/tan. This replaces the current `═` rendering for `FixtureType::Bench`.

### Shelf Rendering

Shelf tiles use `║` (vertical) or `═` (horizontal) depending on wall orientation. Item slot tiles use `~` colored by item type. Interactive shelf looting is a future feature (see `docs/ideas/interactive_shelves.md`).

## Part 2: Settlement NPC Spawning

### Fixed Roles (always present)

| Role | Builder | Placed Near |
|------|---------|-------------|
| Leader | `build_commander()` | Console (Anchor in MainHall) |
| Trader | `build_merchant()` or `build_food_merchant()` or `build_arms_dealer()` (random) | Table (Anchor in Market) |

### Optional Roles (civ-style influenced)

Each role is rolled independently. Larger settlements roll more optional slots:
- Small: 1-2 optional roles
- Medium: 2-4 optional roles  
- Large: 3-5 optional roles

| Role | Frontier | Advanced | Ruined |
|------|----------|----------|--------|
| Medic | 40% | 60% | — |
| Engineer | 20% | 60% | 30% |
| Astronomer | 10% | 50% | — |
| Arms dealer | 30% | 40% | 50% |
| Food merchant | 50% | 30% | 20% |
| Drifter | 30% | 10% | 60% |
| Scavenger | — | — | 70% |
| Prospector* | 20% | 10% | 10% |

*Prospector only spawns on rocky, volcanic, or sandy biomes.

### Residents

Fill remaining NPC count to the target:
- Small: 4-6 total NPCs
- Medium: 7-10 total NPCs
- Large: 11-15 total NPCs

Resident types (weighted random):
- **Civilian** — generic, random friendly race, random name
- **Drifter** — wanderer, passing through
- **Settler** — civilian variant with frontier-flavored name/dialog
- **Refugee** — civilian variant with displaced flavor

Residents are placed in dwellings (near bunks), on paths, or wandering the settlement exterior.

### Ruined Settlement Special Rules

- 50% fewer NPCs overall (halve the target count)
- Leader is replaced by a scavenger boss (uses `build_scavenger()` — new builder needed)
- No medic, no astronomer
- Most residents are drifters and scavengers

### New NPC Builders Required

- `build_scavenger(Race, rng)` — scavenger NPC, has scavenged goods in inventory, found in ruined settlements
- `build_prospector(Race, rng)` — prospector NPC, has minerals/ore, found on mineral-rich worlds

### Placement Logic

NPCs are placed after furniture generation:
1. Find fixture positions (Console → leader, Market table → trader)
2. Roll optional roles based on civ style and size
3. Place optional NPCs near relevant fixtures or in appropriate buildings (medic near HealPod, engineer near Conduit, astronomer in Lookout)
4. Fill remaining slots with residents, distributed across dwellings and paths
5. Wire into `dev_command_biome_test` so settlements spawn NPCs during testing

## File Structure

```
Modified:
  include/astra/settlement_types.h    — PlacementRule enum, FurnitureGroup struct
  src/generators/furniture_palettes.cpp — rewrite palettes using FurnitureGroup
  src/generators/building_generator.cpp — rewrite furnishing step with rule-based placement
  src/tilemap.cpp                      — bench glyph change (if needed)
  src/terminal_theme.cpp               — bench rendering ║, shelf rendering
  src/npc_spawner.cpp                  — new spawn_settlement_npcs_v2()
  include/astra/npc_defs.h             — build_scavenger(), build_prospector()
  src/npc_defs.cpp                     — scavenger/prospector implementations
  src/game.cpp                         — wire NPC spawning into biome_test
```
