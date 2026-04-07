# Structured Furniture Placement & Settlement NPCs — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace random-scatter furniture with rule-based structured placement, and add variety-driven NPC spawning that scales with settlement size and civ style.

**Architecture:** FurnitureGroup replaces FurnitureEntry with a PlacementRule enum that drives six distinct placement algorithms (Anchor, TableSet, WallShelf, WallUniform, Corner, Center). The building generator's Step 6 is rewritten to process groups by rule. NPC spawning is extended with two new types (Scavenger, Prospector) and a new v2 spawner with civ-style-influenced role selection.

**Tech Stack:** C++20, `cmake -B build -DDEV=ON && cmake --build build`

**Spec:** `docs/superpowers/specs/2026-04-07-furniture-placement-and-npcs-design.md`

---

## File Structure

```
Modified:
  include/astra/settlement_types.h     — replace FurnitureEntry with PlacementRule + FurnitureGroup
  src/generators/furniture_palettes.cpp — rewrite palettes using FurnitureGroup
  src/generators/building_generator.cpp — rewrite Step 6 with rule-based placement
  src/terminal_theme.cpp               — bench glyph ║, shelf rendering
  include/astra/npc.h                  — add Scavenger, Prospector to NpcRole enum
  include/astra/npc_defs.h             — declare build_scavenger(), build_prospector()
  include/astra/npc_spawner.h          — declare spawn_settlement_npcs_v2()
  src/npc_spawner.cpp                  — implement spawn_settlement_npcs_v2()
  src/game.cpp                         — wire NPC spawning into biome_test

Create:
  src/npcs/scavenger.cpp               — build_scavenger() implementation
  src/npcs/prospector.cpp              — build_prospector() implementation

Modify:
  CMakeLists.txt                       — add new .cpp files
```

---

### Task 1: Replace FurnitureEntry with FurnitureGroup

Update the types header to use the new group-based system.

**Files:**
- Modify: `include/astra/settlement_types.h`

- [ ] **Step 1: Replace FurnitureEntry and FurniturePalette**

In `include/astra/settlement_types.h`, replace the existing FurnitureEntry, FurniturePalette, and `furniture_palette` declaration (lines 72-89) with:

```cpp
// --- Furniture ---

enum class PlacementRule : uint8_t {
    Anchor,       // prominent position (back wall center), placed first
    TableSet,     // table + bench on each side, rows in center
    WallShelf,    // 3-tile shelf structure against walls
    WallUniform,  // distributed evenly along all walls
    Corner,       // one per corner, skip near doors
    Center,       // free-standing in open floor
};

struct FurnitureGroup {
    PlacementRule rule = PlacementRule::Center;
    FixtureType primary   = FixtureType::Table;
    FixtureType secondary = FixtureType::Bench; // paired item (bench for TableSet, item for Shelf)
    int min_count = 1;
    int max_count = 1;
    float frequency = 1.0f;  // probability this group appears at all
};

struct FurniturePalette {
    std::vector<FurnitureGroup> groups;
};

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style);
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Compile errors in `furniture_palettes.cpp` and `building_generator.cpp` (they still use old FurnitureEntry). This is expected — we fix them in Tasks 2 and 3.

- [ ] **Step 3: Commit**

```bash
git add include/astra/settlement_types.h
git commit -m "feat: replace FurnitureEntry with PlacementRule + FurnitureGroup"
```

---

### Task 2: Rewrite Furniture Palettes

Replace flat entries with grouped palettes using PlacementRule.

**Files:**
- Modify: `src/generators/furniture_palettes.cpp`

- [ ] **Step 1: Rewrite all palettes**

Replace the entire content of `furniture_palettes.cpp`:

```cpp
#include "astra/settlement_types.h"

namespace astra {

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style) {
    FurniturePalette pal;
    //                              rule,             primary,              secondary,           min,max, freq

    switch (type) {
        case BuildingType::MainHall:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::TableSet,    FixtureType::Table,   FixtureType::Bench,   2, 4, 1.0f},
                {PlacementRule::WallShelf,   style.knowledge,      FixtureType::Shelf,   2, 3, 0.7f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   2, 4, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 2, 0.5f},
            };
            break;

        case BuildingType::Market:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Table,   FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   4, 6, 1.0f},
                {PlacementRule::WallShelf,   FixtureType::Shelf,   FixtureType::Shelf,   2, 3, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 3, 0.9f},
            };
            break;

        case BuildingType::Dwelling:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Bunk,    FixtureType::Bunk,    1, 1, 1.0f},
                {PlacementRule::TableSet,    FixtureType::Table,   FixtureType::Bench,   1, 1, 0.8f},
                {PlacementRule::WallUniform, style.cooking,        FixtureType::Table,   1, 1, 0.8f},
                {PlacementRule::WallUniform, style.knowledge,      FixtureType::Table,   1, 1, 0.3f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 2, 0.6f},
            };
            break;

        case BuildingType::Distillery:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   3, 5, 1.0f},
                {PlacementRule::Center,      FixtureType::Table,   FixtureType::Table,   1, 1, 0.6f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 4, 0.8f},
            };
            break;

        case BuildingType::Lookout:
            pal.groups = {
                {PlacementRule::Anchor,      style.knowledge,      FixtureType::Table,   1, 1, 0.9f},
                {PlacementRule::Center,      style.seating,        FixtureType::Table,   1, 1, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 1, 0.4f},
            };
            break;

        case BuildingType::Workshop:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Table,   FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   2, 3, 0.8f},
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   1, 2, 0.5f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 3, 0.7f},
            };
            break;

        case BuildingType::Storage:
            pal.groups = {
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   3, 4, 1.0f},
                {PlacementRule::WallUniform, FixtureType::Shelf,   FixtureType::Table,   3, 5, 0.7f},
                {PlacementRule::WallUniform, style.storage,        FixtureType::Table,   2, 4, 0.8f},
            };
            break;
    }

    return pal;
}

} // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Still fails on `building_generator.cpp` (Task 3 fixes it). `furniture_palettes.cpp` should compile.

- [ ] **Step 3: Commit**

```bash
git add src/generators/furniture_palettes.cpp
git commit -m "feat: rewrite furniture palettes with FurnitureGroup and PlacementRule"
```

---

### Task 3: Rewrite Building Generator Furnishing (Rule-Based Placement)

Replace the entire Step 6 in `building_generator.cpp` with rule-based placement algorithms.

**Files:**
- Modify: `src/generators/building_generator.cpp`

- [ ] **Step 1: Replace Step 6 in building_generator.cpp**

Replace everything from `// --- Step 6: Interior furnishing ---` to the end of the `generate()` function (before the closing `}`) with the new rule-based placement system. The new code implements:

1. **Interior tile collection** — categorize all walkable interior tiles as wall-adjacent, corner, or center. Detect narrow passages (< 4 wide) and mark as no-furniture zones.

2. **Find back wall center** — the wall opposite the primary door. This is where Anchor items go.

3. **Placement functions per rule:**
   - `place_anchor()` — find the interior tile on the back wall at the center. Place the primary fixture there.
   - `place_table_sets()` — find center tiles (not wall-adjacent), place table then bench on each side (east+west or north+south depending on room orientation). Space sets 2 tiles apart.
   - `place_wall_shelves()` — walk perimeter in order. Place 3-tile shelf structures (primary, secondary, primary) at regular intervals (every 5+ tiles). Shelf orientation follows wall (vertical on E/W walls, horizontal on N/S walls). Skip near doors/corners.
   - `place_wall_uniform()` — walk perimeter, place primary fixture at even intervals. Skip near doors, corners, and existing fixtures. Distribute across all four walls.
   - `place_corner()` — find the 4 interior corners (1 tile in from each rect corner). Skip corners within 2 tiles of a door. Place one item per corner.
   - `place_center()` — place on any available center tile (not wall-adjacent).

4. **Walkability verify** — BFS from each door position. If any interior tile is unreachable, remove the last-placed furniture piece and retry. Simple: just ensure all interior floor tiles are reachable from any door.

The full implementation should be self-contained within the building_generator.cpp file as static helper functions called from `generate()`.

Key constraints:
- Never place on a tile that already has a fixture (`map.fixture_id(x,y) >= 0`)
- Never place on door positions or tiles adjacent to doors (inside)
- Never place in narrow passage zones
- `map.add_fixture(x, y, make_fixture(type))` to place
- Walk perimeter using the existing `walk_perimeter()` helper
- Back wall detection: the door is on one wall edge, back wall is the opposite edge of the primary rect

- [ ] **Step 2: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Then test: `biome_test grassland settlement`

Expected: Buildings have structured furniture — tables with benches in center, items along walls, storage in corners.

- [ ] **Step 3: Commit**

```bash
git add src/generators/building_generator.cpp
git commit -m "feat: rule-based furniture placement with structured layouts"
```

---

### Task 4: Update Bench and Shelf Rendering

Change bench glyph to `║` and ensure shelves render correctly.

**Files:**
- Modify: `src/terminal_theme.cpp`

- [ ] **Step 1: Update bench rendering**

In `src/terminal_theme.cpp`, find the `FixtureType::Bench` case in `resolve_fixture` (around line 972) and change:

```cpp
        case FixtureType::Bench:
            vis = {'=', "\xe2\x95\x91", static_cast<Color>(137), Color::Default}; break; // ║ tan bench
```

The UTF-8 for `║` is `\xe2\x95\x91`.

Also update the `fixture_glyph` function (around line 1426):

```cpp
        case FixtureType::Bench:           return '|';
```

- [ ] **Step 2: Verify shelf rendering**

Shelves (`FixtureType::Shelf`) should already render with `╔` in dark gray. The item slot on shelves uses `~` — this is handled by a future interactive shelf feature. For now, the middle tile of a 3-tile shelf structure is just another Shelf fixture (no special rendering needed yet).

- [ ] **Step 3: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test: `biome_test grassland settlement` — benches should show as `║` in tan next to tables.

- [ ] **Step 4: Commit**

```bash
git add src/terminal_theme.cpp
git commit -m "feat: bench renders as ║ glyph"
```

---

### Task 5: New NPC Types — Scavenger and Prospector

Create two new NPC builder functions.

**Files:**
- Modify: `include/astra/npc.h` — add enum values
- Modify: `include/astra/npc_defs.h` — declare builders
- Create: `src/npcs/scavenger.cpp`
- Create: `src/npcs/prospector.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add NpcRole values**

In `include/astra/npc.h`, add before the closing `};` of the NpcRole enum:

```cpp
    Scavenger,
    Prospector,
```

- [ ] **Step 2: Declare builders**

In `include/astra/npc_defs.h`, add after the `build_random_civilian` declaration:

```cpp
// Settlement NPC builders
Npc build_scavenger(Race race, std::mt19937& rng);
Npc build_prospector(Race race, std::mt19937& rng);
```

- [ ] **Step 3: Create scavenger.cpp**

Create `src/npcs/scavenger.cpp` following the pattern of `src/npcs/drifter.cpp`:

```cpp
#include "astra/npc_defs.h"

namespace astra {

Npc build_scavenger(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Scavenger;
    npc.role = "Scavenger";
    npc.hp = 10;
    npc.max_hp = 10;
    npc.disposition = Disposition::Neutral;
    npc.quickness = 45;
    npc.base_xp = 12;
    npc.base_damage = 2;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "What? I found it first. Back off.",
        {
            {
                "Everything here is salvage. The ones who built this "
                "place are long gone. No point letting good tech rot.",
                {
                    {"Find anything valuable?", 1},
                    {"I'll leave you to it.", -1},
                },
            },
            {
                "Bits and pieces. Some circuitry, a power cell or two. "
                "Nothing that'll make me rich, but enough to keep moving.",
                {
                    {"Where do you sell it?", 2},
                    {"Good luck.", -1},
                },
            },
            {
                "Anywhere there's a station with a market. Some places "
                "pay better than others. You learn the routes.",
                {
                    {"Thanks for the tip.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
```

- [ ] **Step 4: Create prospector.cpp**

Create `src/npcs/prospector.cpp`:

```cpp
#include "astra/npc_defs.h"

namespace astra {

Npc build_prospector(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Prospector;
    npc.role = "Prospector";
    npc.hp = 12;
    npc.max_hp = 12;
    npc.disposition = Disposition::Neutral;
    npc.quickness = 40;
    npc.base_xp = 15;
    npc.base_damage = 2;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "See these rocks? There's wealth in them.",
        {
            {
                "Been surveying this region for weeks. The mineral "
                "deposits here are unlike anything in the outer systems. "
                "Dense veins of raw alloy, just waiting to be extracted.",
                {
                    {"What kind of minerals?", 1},
                    {"Sounds like hard work.", -1},
                },
            },
            {
                "Titanium composites, mostly. Some trace iridium. "
                "The real prize would be a zeronium pocket, but those "
                "are one in a million.",
                {
                    {"What's zeronium worth?", 2},
                    {"Interesting. Good luck.", -1},
                },
            },
            {
                "Enough to buy a new ship and retire to the inner "
                "systems. That's the dream, anyway. Most of us just "
                "find enough to keep our gear running.",
                {
                    {"Hope you strike it rich.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
```

- [ ] **Step 5: Add to CMakeLists.txt**

Add `src/npcs/scavenger.cpp` and `src/npcs/prospector.cpp` to the source list.

- [ ] **Step 6: Handle new NpcRole in renderer**

Search for switches on `NpcRole` in `src/terminal_theme.cpp` and `src/game_rendering.cpp`. Add cases for `Scavenger` and `Prospector`:

In the NPC glyph function (terminal_theme.cpp), add:
```cpp
        case NpcRole::Scavenger:  return 'S';
        case NpcRole::Prospector: return 'P';
```

In any NPC name/description function (game_rendering.cpp), add appropriate entries.

- [ ] **Step 7: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile, no unhandled enum warnings.

- [ ] **Step 8: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h src/npcs/scavenger.cpp \
  src/npcs/prospector.cpp CMakeLists.txt src/terminal_theme.cpp src/game_rendering.cpp
git commit -m "feat: add Scavenger and Prospector NPC types"
```

---

### Task 6: Settlement NPC Spawner V2

Rewrite the settlement NPC spawner with civ-style-influenced role selection and scaling.

**Files:**
- Modify: `include/astra/npc_spawner.h`
- Modify: `src/npc_spawner.cpp`

- [ ] **Step 1: Declare new spawner**

In `include/astra/npc_spawner.h`, add after existing declarations:

```cpp
// V2 settlement NPC spawner — civ-style influenced, scales with settlement size
void spawn_settlement_npcs_v2(TileMap& map, std::vector<Npc>& npcs,
                               int player_x, int player_y,
                               std::mt19937& rng, const Player* player,
                               int size_category,    // 0=small, 1=medium, 2=large
                               const CivStyle& style,
                               Biome biome);
```

This requires including `settlement_types.h` in the header (for CivStyle).

- [ ] **Step 2: Implement spawn_settlement_npcs_v2**

Add to `src/npc_spawner.cpp`. The function:

1. Determines total NPC target: small=4-6, medium=7-10, large=11-15. Ruined halves the target.
2. Places fixed roles: Leader near Console, Trader near Table.
3. Determines optional role count: small=1-2, medium=2-4, large=3-5.
4. Builds a weighted pool of optional roles based on civ style name ("Frontier"/"Advanced"/"Ruined") and biome (Prospector only on Rocky/Volcanic/Sandy). Rolls each independently at its percentage chance.
5. Places optional NPCs near relevant fixtures (engineer near Conduit, medic near HealPod, astronomer in general settlement area).
6. Fills remaining slots with residents: weighted mix of civilian (60%), drifter (20%), settler (10%), refugee (10%). Settler and refugee use `build_civilian` with modified role string.
7. For ruined: leader uses `build_scavenger` instead of `build_commander`, residents are mostly drifters/scavengers.

Use existing helpers: `find_fixture_pos()`, `find_floor_near()`, `pick_friendly_race()`.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add include/astra/npc_spawner.h src/npc_spawner.cpp
git commit -m "feat: add spawn_settlement_npcs_v2 with civ-style roles and scaling"
```

---

### Task 7: Wire NPC Spawning into Dev Console

Call the new spawner from `dev_command_biome_test` when testing settlements.

**Files:**
- Modify: `src/game.cpp`

- [ ] **Step 1: Add NPC spawning after map generation**

In `src/game.cpp`, in `dev_command_biome_test()`, after the existing `world_.npcs().clear()` line, add NPC spawning when settlement is active:

```cpp
    world_.npcs().clear();
    world_.ground_items().clear();

    if (settlement) {
        // Spawn settlement NPCs using v2 spawner
        std::mt19937 npc_rng(seed ^ 0xNPC5u);
        CivStyle style = select_civ_style(props);
        // Determine size category from props (same logic as poi_phase)
        int size_cat = 0;
        bool harsh = (biome == Biome::Volcanic || biome == Biome::ScarredScorched
                   || biome == Biome::ScarredGlassed || biome == Biome::Ice);
        bool lush = (biome == Biome::Forest || biome == Biome::Jungle
                  || biome == Biome::Grassland || biome == Biome::Marsh);
        if (harsh || props.lore_tier == 0) size_cat = 0;
        else if (lush && props.lore_tier >= 2) size_cat = 2;
        else size_cat = 1;

        spawn_settlement_npcs_v2(world_.map(), world_.npcs(),
                                  player_.x, player_.y, npc_rng, &player_,
                                  size_cat, style, biome);
    }
```

Add required includes at the top of game.cpp:
```cpp
#include "astra/npc_spawner.h"
#include "astra/settlement_types.h"
```

- [ ] **Step 2: Build and test**

Run: `cmake -B build -DDEV=ON && cmake --build build`
Test all three styles:
```
biome_test grassland settlement frontier
biome_test grassland settlement advanced
biome_test grassland settlement ruined
```

Expected: NPCs appear in settlements. Frontier has leader + merchant + civilians. Advanced has more specialist NPCs. Ruined has scavengers and drifters.

- [ ] **Step 3: Commit**

```bash
git add src/game.cpp
git commit -m "feat: wire settlement NPC spawning into biome_test command"
```

---

### Task 8: Visual Testing and Tuning

Test the complete system across biomes and styles.

**Files:** None new — testing and tuning.

- [ ] **Step 1: Test furniture placement across building types**

Run `biome_test grassland settlement` multiple times. Walk into each building and verify:
- MainHall: Console at back wall, tables with benches in center, shelves on walls, storage in corners
- Market: Counter at back, racks along walls, storage in corners
- Dwelling: Bunk at back, table+bench, stove on wall
- Check no furniture blocks doorways or creates 1-wide chokepoints

- [ ] **Step 2: Test NPC variety**

Run each style 3-4 times:
```
biome_test grassland settlement frontier
biome_test rocky settlement advanced
biome_test sandy settlement ruined
```

Verify: NPCs are present, different roles appear, ruined has scavengers, advanced has more specialists.

- [ ] **Step 3: Test biome-specific NPCs**

```
biome_test rocky settlement frontier    — should sometimes have Prospector
biome_test forest settlement frontier   — should NOT have Prospector
```

- [ ] **Step 4: Tune and fix**

Adjust furniture counts, NPC probabilities, and placement spacing as needed.

- [ ] **Step 5: Commit fixes**

```bash
git add -u
git commit -m "fix: tune furniture placement and NPC spawning after visual testing"
```
