# Conclave Archive on Io Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land a playable multi-level Precursor ruin on Io — the Conclave Archive — built on a new reusable multi-level dungeon generator, closing the Siege quest and clearing the THA lockdown.

**Architecture:** A new `DungeonRecipe` type describes a per-dungeon spec (civ aesthetic, level count, per-level enemy tiers, planned fixtures). Recipes are registered per root `LocationKey` at quest-accept time and persisted in saves. `LocationKey.depth` (already in the tuple) identifies each level; map caching is unchanged. A new `generate_dungeon_level` entry point dispatches to the existing `ruin_generator` with the level's spec, places one `StairsUp` + one `StairsDown` (or only `StairsUp` on boss levels), and places planned fixtures. `DungeonHatch` + `StairsUp/Down` fixture handlers are extended with recipe lookup + bidirectional linkage (ascending returns the player to the exact tile they used to descend). The Archive is the first consumer: registered by `StellarSignalSiegeQuest::on_accepted`, three levels, Precursor civ aesthetic, Archon Sentinel boss on L3, Nova's resonance crystal on L3. Crystal interaction completes the Siege quest, whose `on_completed` already clears `tha_lockdown`.

**Tech Stack:** C++20, existing Astra headers (`include/astra/`), CMake. No new third-party dependencies. **No test framework** — Astra has none; validation is `cmake --build build -j` + in-game dev-mode smoke tests at the task boundaries that introduce user-visible behavior.

**Spec:** `docs/superpowers/specs/2026-04-21-conclave-archive-design.md`

---

## Task 1: DungeonRecipe header + struct definitions

**Files:**
- Create: `include/astra/dungeon_recipe.h`

- [ ] **Step 1: Write `include/astra/dungeon_recipe.h`**

```cpp
#pragma once

#include "astra/world_manager.h"  // LocationKey (std::tuple)
#include "astra/tilemap.h"        // FixtureType (future use)

#include <string>
#include <vector>
#include <functional>

namespace astra {

// Fixture the generator must place on a given level. placement_hint is
// a free-form string consumed by generate_dungeon_level:
//   "back_chamber" — deepest non-entry room
//   "center"       — map center region
//   ""             — random open room
struct PlannedFixture {
    std::string quest_fixture_id;
    std::string placement_hint;
};

// Per-level configuration. The generator dispatches on civ_name into
// civ_config_by_name (see ruin_civ_configs.cpp).
struct DungeonLevelSpec {
    std::string              civ_name    = "Precursor";
    int                      decay_level = 2;   // 0..3, mirrors ruin_generator scale
    int                      enemy_tier  = 1;   // 1..3 (informational)
    std::vector<std::string> npc_roles;         // names resolved via create_npc_by_role
    std::vector<PlannedFixture> fixtures;
    bool is_side_branch = false;                // decoration only for this slice
    bool is_boss_level  = false;                // suppresses StairsDown generation
};

// A registered multi-level dungeon. root is the top-level surface key
// (depth 0). Each dungeon level is LocationKey{..., depth = N + 1} —
// the level index into levels[] is depth - 1.
struct DungeonRecipe {
    LocationKey root;
    std::string kind_tag;                       // e.g. "conclave_archive"
    int         level_count = 1;
    std::vector<DungeonLevelSpec> levels;
};

} // namespace astra
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build (no existing code uses the header yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon_recipe.h
git commit -m "feat(dungeon): add DungeonRecipe structs"
```

---

## Task 2: WorldManager dungeon-recipes registry

**Files:**
- Modify: `include/astra/world_manager.h`
- Modify: `src/world_manager.cpp`

- [ ] **Step 1: Add forward-declared struct + registry to `include/astra/world_manager.h`**

Add at the top of the file (after the existing `LocationKey` + `QuestLocationMeta` declarations but before `class WorldManager`):

```cpp
// Forward-declared; full definition in dungeon_recipe.h.
struct DungeonRecipe;
```

Inside `class WorldManager`, in the `public:` section (near `quest_locations()` accessors):

```cpp
std::unordered_map<LocationKey, DungeonRecipe, LocationKeyHash>& dungeon_recipes() { return dungeon_recipes_; }
const std::unordered_map<LocationKey, DungeonRecipe, LocationKeyHash>& dungeon_recipes() const { return dungeon_recipes_; }
const DungeonRecipe* find_dungeon_recipe(const LocationKey& root) const;
```

In the `private:` section (next to `quest_locations_`):

```cpp
std::unordered_map<LocationKey, DungeonRecipe, LocationKeyHash> dungeon_recipes_;
```

If a `LocationKeyHash` specialization doesn't already exist (search `grep -n LocationKeyHash include/astra/world_manager.h`), add one right after `LocationKey`'s using declaration:

```cpp
struct LocationKeyHash {
    std::size_t operator()(const LocationKey& k) const noexcept {
        // std::tuple has no std::hash, so we fold the fields manually.
        auto h = [](auto v) { return std::hash<decltype(v)>{}(v); };
        std::size_t r = 0;
        auto mix = [&](std::size_t x) { r ^= x + 0x9e3779b9 + (r << 6) + (r >> 2); };
        mix(h(std::get<0>(k)));
        mix(h(std::get<1>(k)));
        mix(h(std::get<2>(k)));
        mix(h(static_cast<int>(std::get<3>(k))));
        mix(h(std::get<4>(k)));
        mix(h(std::get<5>(k)));
        mix(h(std::get<6>(k)));
        return r;
    }
};
```

If one already exists (for `quest_locations_`), reuse it.

- [ ] **Step 2: Implement `find_dungeon_recipe` in `src/world_manager.cpp`**

```cpp
#include "astra/dungeon_recipe.h"

// ... existing code ...

const DungeonRecipe* WorldManager::find_dungeon_recipe(const LocationKey& root) const {
    auto it = dungeon_recipes_.find(root);
    return (it == dungeon_recipes_.end()) ? nullptr : &it->second;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/world_manager.h src/world_manager.cpp
git commit -m "feat(world): add dungeon_recipes registry on WorldManager"
```

---

## Task 3: Save/load `DUNGEON_RECIPES` tagged section

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump `SAVE_FILE_VERSION`**

Find `SAVE_FILE_VERSION` in `include/astra/save_file.h` and bump it by 1 (note the new value, e.g. `SAVE_FILE_VERSION = 36`).

Add to `SaveData`:

```cpp
std::unordered_map<LocationKey, DungeonRecipe, LocationKeyHash> dungeon_recipes;
```

Include `astra/dungeon_recipe.h` at the top.

- [ ] **Step 2: Write recipes in the save path (`src/save_file.cpp`)**

In the function that serializes world state, after the `world_flags` block, insert a new tagged section. Use the existing `begin_section` / `end_section` helpers:

```cpp
{
    auto size_pos = w.begin_section("DREC");  // DungeonRecipes
    w.write_u32(static_cast<uint32_t>(data.dungeon_recipes.size()));
    for (const auto& [root, recipe] : data.dungeon_recipes) {
        // Root LocationKey
        w.write_u32(std::get<0>(root));
        w.write_i32(std::get<1>(root));
        w.write_i32(std::get<2>(root));
        w.write_u8(std::get<3>(root) ? 1 : 0);
        w.write_i32(std::get<4>(root));
        w.write_i32(std::get<5>(root));
        w.write_i32(std::get<6>(root));

        w.write_string(recipe.kind_tag);
        w.write_u32(static_cast<uint32_t>(recipe.level_count));
        w.write_u32(static_cast<uint32_t>(recipe.levels.size()));
        for (const auto& lvl : recipe.levels) {
            w.write_string(lvl.civ_name);
            w.write_i32(lvl.decay_level);
            w.write_i32(lvl.enemy_tier);
            w.write_u8(lvl.is_side_branch ? 1 : 0);
            w.write_u8(lvl.is_boss_level  ? 1 : 0);
            w.write_u32(static_cast<uint32_t>(lvl.npc_roles.size()));
            for (const auto& role : lvl.npc_roles) w.write_string(role);
            w.write_u32(static_cast<uint32_t>(lvl.fixtures.size()));
            for (const auto& fx : lvl.fixtures) {
                w.write_string(fx.quest_fixture_id);
                w.write_string(fx.placement_hint);
            }
        }
    }
    w.end_section(size_pos);
}
```

- [ ] **Step 3: Read in load path, gated on version**

In the load function, after the existing `world_flags` read block:

```cpp
if (data.version >= SAVE_FILE_VERSION_WITH_DUNGEON_RECIPES) {
    // Tag already consumed by the section-dispatch loop, or read here
    // if this version adds its own read call. Mirror the exact pattern
    // used for world_flags in this file.
    uint32_t n = r.read_u32();
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t sys = r.read_u32();
        int body    = r.read_i32();
        int moon    = r.read_i32();
        bool is_st  = r.read_u8() != 0;
        int ow_x    = r.read_i32();
        int ow_y    = r.read_i32();
        int depth   = r.read_i32();
        LocationKey root{sys, body, moon, is_st, ow_x, ow_y, depth};

        DungeonRecipe recipe;
        recipe.root        = root;
        recipe.kind_tag    = r.read_string();
        recipe.level_count = static_cast<int>(r.read_u32());
        uint32_t lc = r.read_u32();
        recipe.levels.reserve(lc);
        for (uint32_t j = 0; j < lc; ++j) {
            DungeonLevelSpec lvl;
            lvl.civ_name       = r.read_string();
            lvl.decay_level    = r.read_i32();
            lvl.enemy_tier     = r.read_i32();
            lvl.is_side_branch = r.read_u8() != 0;
            lvl.is_boss_level  = r.read_u8() != 0;
            uint32_t rc = r.read_u32();
            for (uint32_t k = 0; k < rc; ++k) lvl.npc_roles.push_back(r.read_string());
            uint32_t fc = r.read_u32();
            for (uint32_t k = 0; k < fc; ++k) {
                PlannedFixture fx;
                fx.quest_fixture_id = r.read_string();
                fx.placement_hint   = r.read_string();
                lvl.fixtures.push_back(std::move(fx));
            }
            recipe.levels.push_back(std::move(lvl));
        }
        data.dungeon_recipes[root] = std::move(recipe);
    }
}
```

Define `SAVE_FILE_VERSION_WITH_DUNGEON_RECIPES` as the version you bumped to in Step 1.

In the function that loads `WorldManager` from `SaveData`, add:

```cpp
world.dungeon_recipes() = std::move(data.dungeon_recipes);
```

And in the function that writes `SaveData` from `WorldManager`:

```cpp
data.dungeon_recipes = world.dungeon_recipes();
```

Mirror the exact idiom used for `world_flags` in the same file — match naming and placement.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Smoke-test save/load**

Run: `./build/astra-dev`
- Start a new game.
- Save, quit, reload.
- Expected: no crash, load succeeds, main menu or resumed game renders normally.
- (No recipes exist yet — this just verifies the empty-list round-trip.)

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): persist dungeon recipes (DREC section)"
```

---

## Task 4: Precursor civ aesthetic config

**Files:**
- Modify: `src/generators/ruin_civ_configs.cpp`

- [ ] **Step 1: Add `civ_config_precursor()` factory**

Locate `civ_config_crystal()` in `src/generators/ruin_civ_configs.cpp`. Below it, add:

```cpp
CivConfig civ_config_precursor() {
    CivConfig c;
    c.name = "Precursor";
    c.civ_index = CIV_PRECURSOR;  // add this constant; see Step 2
    // Use the same box-drawing wall set as Crystal.
    c.wall_glyphs = {"\xe2\x94\x8c", "\xe2\x94\x80", "\xe2\x94\x90",
                     "\xe2\x94\x82", "\xe2\x94\xbc", "\xe2\x94\x9c",
                     "\xe2\x94\xa4"};
    // Stellari resonance cyan primary, violet secondary.
    c.color_primary   = 135;   // Stellari resonance cyan
    c.color_secondary = 93;    // deep violet
    c.accent_glyphs   = {"\xe2\x97\x86", "\xe2\x97\x88", "*"};
    // Larger, sparser rooms: Precursor architecture preference.
    c.min_room_size = 4;
    c.max_room_size = 9;
    c.corridor_width = 1;
    return c;
}
```

(Mirror whichever additional fields the existing `CivConfig` struct carries — if `min_room_size` doesn't exist, skip it; only set fields that the struct defines. Check `include/astra/generators/ruin_config.h` or similar for the exact field list. If in doubt, copy `civ_config_crystal()` verbatim and change only `name`, `civ_index`, and `color_primary`/`secondary`.)

- [ ] **Step 2: Add `CIV_PRECURSOR` constant**

Find where `CIV_CRYSTAL` is declared (same file or a nearby header). Add:

```cpp
constexpr int CIV_PRECURSOR = CIV_CRYSTAL + 1;   // use the next available index
```

(Match whatever convention is used. If these are an enum, add a new enumerator; if `constexpr int`, use the next value.)

- [ ] **Step 3: Route "Precursor" through `civ_config_by_name`**

In the same file, find `civ_config_by_name(const std::string& name)`. Add a branch:

```cpp
if (name == "Precursor") return civ_config_precursor();
```

- [ ] **Step 4: Declare the factory**

If there's a header exposing `civ_config_crystal()` publicly (e.g. `include/astra/ruin_civ_configs.h`), add a matching declaration for `civ_config_precursor()`. If not, leave it internal.

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add src/generators/ruin_civ_configs.cpp <header-if-touched>
git commit -m "feat(generators): add Precursor civ aesthetic"
```

---

## Task 5: Heavy Conclave Sentry archetype

**Files:**
- Modify: `include/astra/npc.h`
- Modify: `include/astra/npc_defs.h`
- Create: `src/npcs/heavy_conclave_sentry.cpp`
- Modify: `src/npc.cpp`
- Modify: `src/dev_console.cpp`
- Modify: `src/terminal_theme.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add enumerator to `include/astra/npc.h`**

In the `NpcRole` enum (already contains `ConclaveSentry`), add right after:

```cpp
HeavyConclaveSentry,
```

- [ ] **Step 2: Declare the factory in `include/astra/npc_defs.h`**

Find where `build_conclave_sentry` is declared; add below it:

```cpp
Npc build_heavy_conclave_sentry(std::mt19937& rng);
```

- [ ] **Step 3: Write `src/npcs/heavy_conclave_sentry.cpp`**

Mirror `src/npcs/conclave_sentry.cpp` verbatim, adjusting only numbers and role strings:

```cpp
#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_heavy_conclave_sentry(std::mt19937& /*rng*/) {
    Npc npc;
    npc.race        = Race::Stellari;
    npc.npc_role    = NpcRole::HeavyConclaveSentry;
    npc.role        = "Heavy Conclave Sentry";
    npc.name        = "Heavy Conclave Sentry";
    npc.hp          = 55;
    npc.max_hp      = 55;
    npc.faction     = Faction_StellariConclave;
    npc.quickness   = 105;
    npc.base_xp     = 90;
    npc.base_damage = 5;
    npc.dv          = 11;
    npc.av          = 7;
    npc.damage_dice = Dice::make(1, 10);
    npc.damage_type = DamageType::Plasma;
    npc.type_affinity = {-1, 3, 0, 0, 0};
    return npc;
}

} // namespace astra
```

- [ ] **Step 4: Dispatch in `src/npc.cpp`**

In `create_npc`'s switch, next to `case NpcRole::ConclaveSentry:`:

```cpp
case NpcRole::HeavyConclaveSentry: return build_heavy_conclave_sentry(rng);
```

In `create_npc_by_role`'s if-chain, next to `"Conclave Sentry"`:

```cpp
if (role_name == "Heavy Conclave Sentry")
    return create_npc(NpcRole::HeavyConclaveSentry, Race::Stellari, rng);
```

- [ ] **Step 5: Dev console mapping**

In `src/dev_console.cpp`, next to `archon_remnant` / `conclave_sentry` mappings in the spawn command block:

```cpp
else if (role_arg == "heavy_conclave_sentry") role_name = "Heavy Conclave Sentry";
```

- [ ] **Step 6: Glyph mapping in `src/terminal_theme.cpp`**

In the `npc_glyph` switch, add:

```cpp
case NpcRole::HeavyConclaveSentry: return 'H';
```

- [ ] **Step 7: Add to `CMakeLists.txt`**

Add `src/npcs/heavy_conclave_sentry.cpp` to `ASTRA_SOURCES` (sorted next to `src/npcs/conclave_sentry.cpp`).

- [ ] **Step 8: Build + dev-console smoke**

Run: `cmake --build build -j && ./build/astra-dev`
In-game: open dev console, run `spawn heavy_conclave_sentry`. Expected: `H`-glyph hostile spawns in view. Kill it; confirm plasma damage, ~55 HP, higher-than-standard-Sentry feel.

- [ ] **Step 9: Commit**

```bash
git add include/astra/npc.h include/astra/npc_defs.h src/npcs/heavy_conclave_sentry.cpp \
        src/npc.cpp src/dev_console.cpp src/terminal_theme.cpp CMakeLists.txt
git commit -m "feat(npc): add Heavy Conclave Sentry archetype"
```

---

## Task 6: `ResonancePillar` decorative fixture + nova_resonance_crystal + archive entrance fixture registration

**Files:**
- Modify: `include/astra/tilemap.h` (FixtureType enum)
- Modify: `src/terminal_theme.cpp` (fixture_glyph)
- Modify: `src/game.cpp` (register quest fixtures at startup)

- [ ] **Step 1: Add `ResonancePillar` to `FixtureType` enum**

In `include/astra/tilemap.h`, find the `FixtureType` enum (around line 328). Add next to other decorative fixtures:

```cpp
ResonancePillar,  // '~' — non-interactive Precursor decoration
```

- [ ] **Step 2: Add glyph mapping**

In `src/terminal_theme.cpp`, `fixture_glyph` switch:

```cpp
case FixtureType::ResonancePillar: return '~';
```

Also, if a `fixture_color` or equivalent exists that defaults to fallbacks, add:

```cpp
case FixtureType::ResonancePillar: return 135;  // Stellari resonance cyan
```

(If color is only handled per-instance via `FixtureData`, skip — renderer will use the global default; color gets applied when placed.)

- [ ] **Step 3: Register `nova_resonance_crystal` at startup**

Find where `register_quest_fixture` is called for the stellar-signal echoes (search `grep -n register_quest_fixture src/quests`). Add a new registration site in `StellarSignalSiegeQuest`'s `register_fixtures` override, or alongside the echoes registry, depending on which pattern is idiomatic. Prefer the same pattern as the echoes.

Open `src/quests/stellar_signal_siege.cpp` and add a `register_fixtures` override:

```cpp
void register_fixtures() override {
    register_quest_fixture({
        "conclave_archive_entrance",
        'v', 135, "Descend into the Conclave Archive",
        "You drop through the hatch into the ruin below.",
        "", {}
    });
    register_quest_fixture({
        "nova_resonance_crystal",
        '*', 135,
        "A small Stellari-resonance crystal hums on a Precursor pedestal. Activate it?",
        "The crystal lights up. Nova's voice fills the chamber.",
        "STELLARI RESONANCE CRYSTAL - FINAL LOG",
        {
            // TODO: paste Nova's Stage 5 monologue verbatim from
            // /Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md
            // lines 292-343. Left as a placeholder so the user owns
            // the narrative content.
            "(placeholder - paste crystal log lines here)",
        },
    });
}
```

If `register_fixtures` is not a `StoryQuest` virtual, follow whatever pattern stellar_signal_echoes uses — it's the template.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/tilemap.h src/terminal_theme.cpp src/quests/stellar_signal_siege.cpp
git commit -m "feat(fixtures): register Archive entrance + crystal quest fixtures; add ResonancePillar"
```

---

## Task 7: `generate_dungeon_level` entry point

**Files:**
- Create: `include/astra/dungeon_level_generator.h`
- Create: `src/generators/dungeon_level.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write `include/astra/dungeon_level_generator.h`**

```cpp
#pragma once

#include "astra/dungeon_recipe.h"
#include "astra/tilemap.h"

#include <cstdint>

namespace astra {

// Generate a single dungeon level into `map` according to recipe.levels[depth-1].
// Places one StairsUp at the seeded entry position and one StairsDown at the
// last-room center (unless the level is_boss_level). Spawns NPCs from
// level spec. Places planned fixtures (QuestFixtures keyed by quest_fixture_id).
//
// `entered_from` is the position to place the StairsUp on (the down-stairs
// tile on the parent level, so the round-trip is exact). Use {-1, -1} for
// "generator picks a deterministic default" (first room center).
void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int,int> entered_from);

// Find the (x,y) of the single StairsUp fixture on `map`. Returns {-1,-1}
// if none found.
std::pair<int,int> find_stairs_up(const TileMap& map);

// Find the (x,y) of the single StairsDown fixture on `map`. Returns
// {-1,-1} if none found.
std::pair<int,int> find_stairs_down(const TileMap& map);

// Deterministic per-level seed derived from the world seed and the
// level's LocationKey.
uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& level_key);

} // namespace astra
```

- [ ] **Step 2: Write `src/generators/dungeon_level.cpp`**

```cpp
#include "astra/dungeon_level_generator.h"
#include "astra/map_generator.h"   // create_generator()
#include "astra/map_properties.h"
#include "astra/quest_fixture.h"
#include "astra/tile_props.h"

#include <random>

namespace astra {

uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& k) {
    uint32_t s = world_seed;
    s ^= static_cast<uint32_t>(std::get<0>(k)) * 73856093u;
    s ^= static_cast<uint32_t>(std::get<1>(k)) * 19349663u;
    s ^= static_cast<uint32_t>(std::get<2>(k)) * 83492791u;
    s ^= static_cast<uint32_t>(std::get<6>(k)) * 6271u;        // depth
    return s;
}

std::pair<int,int> find_stairs_up(const TileMap& m) {
    for (int i = 0; i < m.fixture_count(); ++i) {
        const auto& f = m.fixture(i);
        if (f.type == FixtureType::StairsUp) return {f.x, f.y};
    }
    return {-1, -1};
}

std::pair<int,int> find_stairs_down(const TileMap& m) {
    for (int i = 0; i < m.fixture_count(); ++i) {
        const auto& f = m.fixture(i);
        if (f.type == FixtureType::StairsDown) return {f.x, f.y};
    }
    return {-1, -1};
}

static void place_planned_fixtures(TileMap& map,
                                   const DungeonLevelSpec& spec,
                                   std::mt19937& rng) {
    for (const auto& pf : spec.fixtures) {
        int fx = -1, fy = -1;

        // Rooms are stored on TileMap as regions. For "back_chamber"
        // pick the last region's center.
        if (pf.placement_hint == "back_chamber") {
            int rc = map.region_count();
            for (int r = rc - 1; r >= 0 && fx < 0; --r) {
                const auto& region = map.region(r);
                int cx = region.x + region.w / 2;
                int cy = region.y + region.h / 2;
                if (map.passable(cx, cy)) { fx = cx; fy = cy; }
            }
        }
        // Fallback: random passable tile.
        if (fx < 0) {
            std::vector<std::pair<int,int>> open;
            for (int y = 0; y < map.height(); ++y)
                for (int x = 0; x < map.width(); ++x)
                    if (map.passable(x, y)) open.push_back({x, y});
            if (open.empty()) continue;
            std::uniform_int_distribution<size_t> d(0, open.size() - 1);
            auto p = open[d(rng)];
            fx = p.first; fy = p.second;
        }

        FixtureData fd;
        fd.type = FixtureType::QuestFixture;
        fd.interactable = true;
        fd.quest_fixture_id = pf.quest_fixture_id;
        fd.cooldown = -1;
        map.add_fixture(fx, fy, fd);
    }
}

void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int,int> entered_from) {
    if (depth < 1 || depth > static_cast<int>(recipe.levels.size())) return;
    const auto& spec = recipe.levels[depth - 1];

    // Mirror existing dungeon entry pattern (src/game_world.cpp:925-928):
    //   world_.map() = TileMap(w, h, type);
    //   auto gen = create_generator(type);
    //   gen->generate(world_.map(), props, seed);
    //
    // The caller has already assigned the TileMap; we only invoke the
    // generator here. The caller owns the map lifetime.
    MapProperties props = default_properties(MapType::Dungeon);
    props.civ_name = spec.civ_name;
    props.decay_level = spec.decay_level;
    // If MapProperties has additional fields the existing ruin entry
    // path sets (decay, lore_alien_architecture, etc.), mirror those
    // from the existing dungeon-entry call site in game_world.cpp — do
    // NOT invent fields. Copy whichever ones are set there.

    auto gen = create_generator(MapType::Dungeon);
    gen->generate(map, props, seed);

    // Place StairsUp at entered_from if passable; otherwise at the
    // first region center.
    int ux, uy;
    if (entered_from.first >= 0 &&
        map.passable(entered_from.first, entered_from.second)) {
        ux = entered_from.first; uy = entered_from.second;
    } else if (map.region_count() > 0) {
        const auto& r0 = map.region(0);
        ux = r0.x + r0.w / 2; uy = r0.y + r0.h / 2;
    } else {
        ux = map.width() / 2; uy = map.height() / 2;
    }
    {
        FixtureData f; f.type = FixtureType::StairsUp; f.interactable = true;
        map.add_fixture(ux, uy, f);
    }

    // Place StairsDown at the last region's center unless boss level.
    if (!spec.is_boss_level) {
        int rc = map.region_count();
        if (rc > 1) {
            const auto& rn = map.region(rc - 1);
            int dx = rn.x + rn.w / 2, dy = rn.y + rn.h / 2;
            if (map.passable(dx, dy) && (dx != ux || dy != uy)) {
                FixtureData f; f.type = FixtureType::StairsDown; f.interactable = true;
                map.add_fixture(dx, dy, f);
            }
        }
    }

    // Planned fixtures.
    std::mt19937 rng(seed ^ 0xA5A5u);
    place_planned_fixtures(map, spec, rng);

    // NPC spawning is NOT done here — generate_dungeon_level only
    // produces the map. The caller (Game::descend_stairs) spawns NPCs
    // into world_.npcs() after the map is assigned to world_.map().
}

} // namespace astra
```

**Note:** API names used match Astra's actual TileMap (`passable`, `region`, `region_count`, `fixture_count`, `fixture`, `add_fixture`). Generator is invoked via `create_generator(MapType::Dungeon)` matching the precedent at `src/game_world.cpp:925-928`. NPC spawning is caller-owned (matches how existing dungeon entry uses `world_.npcs().push_back`).

- [ ] **Step 3: Add to `CMakeLists.txt`**

Add `src/generators/dungeon_level.cpp` to `ASTRA_SOURCES` next to other generator files.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build. Compile errors around TileMap methods indicate need to adjust member names — fix per real API (do not skip; pattern must match).

- [ ] **Step 5: Commit**

```bash
git add include/astra/dungeon_level_generator.h src/generators/dungeon_level.cpp CMakeLists.txt
git commit -m "feat(dungeon): add generate_dungeon_level entry point"
```

---

## Task 8: `Game::descend_stairs` / `Game::ascend_stairs` helpers

**Files:**
- Modify: `include/astra/game.h`
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Declare helpers in `include/astra/game.h`**

In `class Game`, `public:` section, near the existing warp/travel helpers:

```cpp
// Descend into a recipe-backed dungeon level. `from_fixture_pos` is the
// (x,y) of the StairsDown / DungeonHatch the player interacted with.
// Saves current location, generates-or-restores the target depth, and
// places the player at the matching StairsUp. Plays a log line.
void descend_stairs(std::pair<int,int> from_fixture_pos);

// Ascend to the parent level of the current dungeon. On depth 1, exits
// to the surface (depth 0) at the saved DungeonHatch position. Places
// the player at the matching StairsDown on the parent level.
void ascend_stairs();
```

- [ ] **Step 2: Implement helpers in `src/game_world.cpp`**

**Pattern for building a `LocationKey` from current state.** Astra has no `current_location_key()` accessor — call sites build the key manually from `world_.navigation()`. This helper encapsulates that and adds a depth field. Add near the top of the file (after the existing anonymous-namespace helpers):

```cpp
namespace {
astra::LocationKey make_location_key(const astra::Navigation& nav,
                                     int depth) {
    return astra::LocationKey{
        nav.current_system_id,
        nav.current_body_index,
        nav.current_moon_index,
        nav.at_station,
        /*ow_x*/ -1, /*ow_y*/ -1,
        depth
    };
}
} // namespace
```

(Verify field names on `Navigation` — `current_system_id`, `current_body_index`, `current_moon_index`, `at_station` are all referenced elsewhere in the file. If any name differs, match what `travel_to_destination` already uses — those are the authoritative names.)

Add the helpers near `travel_to_destination`:

```cpp
#include "astra/dungeon_level_generator.h"
#include "astra/dungeon_recipe.h"

static astra::LocationKey with_depth(const astra::LocationKey& k, int d) {
    astra::LocationKey out = k;
    std::get<6>(out) = d;
    return out;
}

void Game::descend_stairs(std::pair<int,int> from_fixture_pos) {
    auto& nav = world_.navigation();
    LocationKey cur_key = make_location_key(nav, nav.current_depth);
    int cur_depth = nav.current_depth;
    LocationKey root = with_depth(cur_key, 0);

    const DungeonRecipe* recipe = world_.find_dungeon_recipe(root);
    if (!recipe) {
        log("Nothing happens.");
        return;
    }
    int next_depth = cur_depth + 1;
    if (next_depth > recipe->level_count) {
        log("The stairs end here.");
        return;
    }

    save_current_location();

    LocationKey target = with_depth(cur_key, next_depth);
    if (world_.location_cache().count(target)) {
        restore_location(target);
    } else {
        auto props = default_properties(MapType::Dungeon);
        world_.map() = TileMap(props.width, props.height, MapType::Dungeon);

        uint32_t seed = dungeon_level_seed(world_.seed(), target);
        generate_dungeon_level(world_.map(), *recipe, next_depth, seed,
                               from_fixture_pos);
        world_.map().set_location_name(recipe->kind_tag);

        world_.npcs().clear();
        world_.ground_items().clear();

        // Spawn NPCs from the level spec using the same pattern as
        // existing dungeon entry code (world_.npcs().push_back).
        const auto& spec = recipe->levels[next_depth - 1];
        std::mt19937 npc_rng(seed ^ 0x5A5Au);
        std::vector<std::pair<int,int>> occupied;
        for (const auto& role : spec.npc_roles) {
            int nx, ny;
            if (!world_.map().find_open_spot_other_room(
                    world_.map().width() / 2, world_.map().height() / 2,
                    nx, ny, occupied, &npc_rng))
                continue;
            Npc n = create_npc_by_role(role, npc_rng);
            n.x = nx; n.y = ny;
            occupied.push_back({nx, ny});
            world_.npcs().push_back(std::move(n));
        }

        // Place player at the matching StairsUp.
        auto up = find_stairs_up(world_.map());
        if (up.first >= 0) { player_.x = up.first; player_.y = up.second; }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    nav.current_depth = next_depth;
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You descend.");
}

void Game::ascend_stairs() {
    auto& nav = world_.navigation();
    int cur_depth = nav.current_depth;
    if (cur_depth <= 0) return;   // at surface — should never fire

    save_current_location();

    LocationKey target = make_location_key(nav, cur_depth - 1);
    if (world_.location_cache().count(target)) {
        restore_location(target);
    } else {
        // Parent level should always be cached (we just came from it).
        // Log and abort cleanly if not.
        log("You climb, but find no way back.");
        return;
    }

    // Place player at the matching StairsDown (or DungeonHatch if we
    // just emerged to depth 0).
    std::pair<int,int> spawn = {-1, -1};
    if (cur_depth - 1 == 0) {
        for (int i = 0; i < world_.map().fixture_count(); ++i) {
            const auto& f = world_.map().fixture(i);
            if (f.type == FixtureType::DungeonHatch) {
                spawn = {f.x, f.y}; break;
            }
        }
    } else {
        spawn = find_stairs_down(world_.map());
    }
    if (spawn.first >= 0) { player_.x = spawn.first; player_.y = spawn.second; }

    nav.current_depth = cur_depth - 1;
    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You climb back up.");
}
```

**Required Navigation extension:** `current_depth` is a new integer field on `Navigation`. Add it:

- In `include/astra/world_manager.h` (the `struct Navigation` definition — grep for `current_body_index` to locate it), add:

  ```cpp
  int current_depth = 0;   // 0 = surface; >=1 = dungeon level
  ```

- In `src/save_file.cpp`, wherever `Navigation` fields are serialized (grep for `current_body_index`), add `write_i32(nav.current_depth)` on save and `nav.current_depth = r.read_i32()` on load — gated on the save-version you bumped in Task 3. For older saves, leave `current_depth = 0`.

- Wherever navigation state is reset (e.g. warp-to-system, land-on-body, travel_to_destination's state mutations), set `current_depth = 0` alongside the other resets. Grep `at_station = ` for the call sites — at each, add `nav.current_depth = 0;` right below. This guarantees the field is zero whenever the player is on a surface/overworld, not stuck at some dungeon depth from a previous location.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/game.h src/game_world.cpp
git commit -m "feat(dungeon): add Game::descend_stairs and ascend_stairs helpers"
```

---

## Task 9: Wire fixture interaction for StairsUp / StairsDown / DungeonHatch

**Files:**
- Modify: `src/dialog_manager.cpp`

- [ ] **Step 1: Extend `interact_fixture` switch**

Locate `DialogManager::interact_fixture` (around `src/dialog_manager.cpp:296`). Modify the existing cases:

```cpp
case FixtureType::StairsUp: {
    // Recipe-backed dungeon? If so, ascend via recipe logic.
    if (game.world().navigation().current_depth > 0) {
        game.ascend_stairs();
        break;
    }
    // Fall through to pre-existing behavior.
    if (game.world().map().location_name() == "Maintenance Tunnels") {
        game.exit_maintenance_tunnels();
    } else {
        game.exit_dungeon_to_detail();
    }
    break;
}
case FixtureType::StairsDown: {
    game.descend_stairs({f.x, f.y});
    break;
}
case FixtureType::DungeonHatch: {
    // Recipe-backed hatch on the surface? Route into the dungeon.
    const auto& nav = game.world().navigation();
    if (nav.current_depth == 0) {
        LocationKey root{
            nav.current_system_id,
            nav.current_body_index,
            nav.current_moon_index,
            nav.at_station,
            -1, -1, 0
        };
        if (game.world().find_dungeon_recipe(root)) {
            game.descend_stairs({f.x, f.y});
            break;
        }
    }
    // Pre-existing maintenance-tunnel fallthrough:
    if (game.quests().has_active_quest("story_getting_airborne")) {
        game.enter_maintenance_tunnels();
    } else {
        game.log("Maintenance Tunnels -- Currently Under Maintenance.");
    }
    break;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/dialog_manager.cpp
git commit -m "feat(dungeon): wire stairs fixture interactions to recipe descent/ascent"
```

---

## Task 10: Precursor Archive POI stamp

**Files:**
- Modify: `include/astra/tilemap.h` (Tile enum — add `OW_PrecursorArchive` if not present)
- Modify: `src/overworld_stamps.cpp`
- Modify: `src/poi/` placement dispatch (grep to locate)

- [ ] **Step 1: Add `Tile::OW_PrecursorArchive` enumerator**

In `include/astra/tilemap.h`, find the `Tile` enum (near `OW_Outpost`, `OW_CrashedShip`). Add:

```cpp
OW_PrecursorArchive,
```

- [ ] **Step 2: Define the stamp in `src/overworld_stamps.cpp`**

Mirror `crashed_ship_stamps` style:

```cpp
static const StampCell precursor_archive_3x3[] = {
    // 3x3 small ruin. Walls on outer ring, hatch at center. Adjust
    // glyphs to match the existing palette (Tile::Wall vs ruin decals).
    {0,0, Tile::Wall},  {1,0, Tile::Wall},  {2,0, Tile::Wall},
    {0,1, Tile::Wall},  {1,1, Tile::OW_Ruins /*passable center*/}, {2,1, Tile::Wall},
    {0,2, Tile::Wall},  {1,2, Tile::Wall},  {2,2, Tile::Wall},
};
const StampDef precursor_archive_stamps[] = {
    {3, 3, precursor_archive_3x3, 9},
};
const int precursor_archive_stamp_count = 1;
```

The actual hatch fixture is placed programmatically in Step 3 because stamps only paint tiles.

- [ ] **Step 3: Wire POI placement**

Find the file that routes `QuestLocationMeta.poi_type` into stamp painting — search:

```
grep -n 'OW_Outpost\|OW_CrashedShip' src/poi src/generators src/game_world.cpp
```

In the dispatch switch that maps a `poi_type` to a stamp set, add:

```cpp
case Tile::OW_PrecursorArchive: {
    place_stamp(map, x, y, precursor_archive_stamps, precursor_archive_stamp_count, rng);
    // Put the DungeonHatch fixture at the stamp's center (x+1, y+1 for 3x3).
    FixtureData f; f.type = FixtureType::DungeonHatch; f.interactable = true;
    f.quest_fixture_id = "conclave_archive_entrance";
    map.add_fixture(x + 1, y + 1, f);
    break;
}
```

Match the existing case structure. Placement position — `x, y` is the stamp anchor.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/tilemap.h src/overworld_stamps.cpp <poi-dispatch-file>
git commit -m "feat(poi): add Precursor Archive POI stamp + DungeonHatch placement"
```

---

## Task 11: `build_conclave_archive_levels()` factory

**Files:**
- Create: `include/astra/dungeon/conclave_archive.h`
- Create: `src/dungeon/conclave_archive.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write `include/astra/dungeon/conclave_archive.h`**

```cpp
#pragma once

#include "astra/dungeon_recipe.h"
#include <vector>

namespace astra {

// Three-level Precursor ruin. Level 1 Conclave Sentries, Level 2
// Heavy Sentries + Archon Remnants, Level 3 Archon Sentinel boss +
// Nova's resonance crystal.
std::vector<DungeonLevelSpec> build_conclave_archive_levels();

} // namespace astra
```

- [ ] **Step 2: Write `src/dungeon/conclave_archive.cpp`**

```cpp
#include "astra/dungeon/conclave_archive.h"

namespace astra {

std::vector<DungeonLevelSpec> build_conclave_archive_levels() {
    std::vector<DungeonLevelSpec> out;
    out.reserve(3);

    {
        DungeonLevelSpec l1;
        l1.civ_name    = "Precursor";
        l1.decay_level = 3;   // heavy / battle-scarred
        l1.enemy_tier  = 1;
        l1.npc_roles   = {"Conclave Sentry", "Conclave Sentry",
                          "Conclave Sentry", "Conclave Sentry"};
        out.push_back(std::move(l1));
    }
    {
        DungeonLevelSpec l2;
        l2.civ_name    = "Precursor";
        l2.decay_level = 2;   // moderate
        l2.enemy_tier  = 2;
        l2.npc_roles   = {"Heavy Conclave Sentry", "Heavy Conclave Sentry",
                          "Heavy Conclave Sentry",
                          "Archon Remnant", "Archon Remnant"};
        out.push_back(std::move(l2));
    }
    {
        DungeonLevelSpec l3;
        l3.civ_name    = "Precursor";
        l3.decay_level = 0;   // pristine
        l3.enemy_tier  = 3;
        l3.is_boss_level = true;
        l3.npc_roles   = {"Heavy Conclave Sentry", "Heavy Conclave Sentry",
                          "Archon Sentinel"};
        l3.fixtures    = {
            PlannedFixture{ "nova_resonance_crystal", "back_chamber" },
        };
        out.push_back(std::move(l3));
    }

    return out;
}

} // namespace astra
```

- [ ] **Step 3: Add to `CMakeLists.txt`**

Add `src/dungeon/conclave_archive.cpp` to `ASTRA_SOURCES`.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/dungeon/conclave_archive.h src/dungeon/conclave_archive.cpp CMakeLists.txt
git commit -m "feat(archive): add build_conclave_archive_levels factory"
```

---

## Task 12: Siege quest wiring — objectives, recipe registration, rewards

**Files:**
- Modify: `src/quests/stellar_signal_siege.cpp`

- [ ] **Step 1: Rewrite objectives, on_accepted, add rewards and journal_on_complete**

Replace `create_quest` body with:

```cpp
Quest create_quest() override {
    Quest q;
    q.id = QUEST_ID_SIEGE;
    q.title = "They Came For Her";
    q.description = /* existing Nova monologue — unchanged */;
    q.giver_npc = "Stellar Engineer";
    q.is_story = true;
    q.objectives = {
        { ObjectiveType::GoToLocation,
          "Land on Io", 1, 0, "Io" },
        { ObjectiveType::InteractFixture,
          "Recover Nova's fragment from the Conclave Archive",
          1, 0, "nova_resonance_crystal" },
    };
    q.reward.xp      = 400;
    q.reward.credits = 500;
    q.journal_on_accept =
        /* existing text — unchanged */;
    q.journal_on_complete =
        "Played Nova's final message. Heard her three choices. "
        "THA's comms are open again - the Conclave pulled back. "
        "I think they didn't expect anyone to reach the vault. "
        "Nova's voice is still in my head.";
    return q;
}
```

Extend `on_accepted`:

```cpp
#include "astra/dungeon/conclave_archive.h"
// ... (existing includes)

void on_accepted(Game& game) override {
    // --- existing star-chart marker ---
    LocationKey k{1, 5, 0, false, -1, -1, 0};
    QuestLocationMeta meta;
    meta.quest_id        = QUEST_ID_SIEGE;
    meta.quest_title     = "They Came For Her";
    meta.target_system_id = 1;
    meta.target_body_index = 5;
    meta.target_moon_index = 0;

    // --- new: POI + surface patrols ---
    meta.poi_type  = Tile::OW_PrecursorArchive;
    meta.npc_roles = {"Conclave Sentry", "Conclave Sentry", "Conclave Sentry"};

    game.world().quest_locations()[k] = std::move(meta);

    // --- new: register Conclave Archive recipe ---
    DungeonRecipe recipe;
    recipe.root        = k;
    recipe.kind_tag    = "conclave_archive";
    recipe.level_count = 3;
    recipe.levels      = build_conclave_archive_levels();
    game.world().dungeon_recipes()[k] = std::move(recipe);

    // --- existing: ARIA panic transmission (unchanged) ---
    open_transmission(game, /* existing lines */);
}
```

(Preserve the existing `on_completed` override that clears `tha_lockdown` — just ensure Steps 6's register_fixtures is also present. Do not delete anything already there.)

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/quests/stellar_signal_siege.cpp
git commit -m "feat(stellar-signal): Siege quest wires Archive POI, recipe, objectives, rewards"
```

---

## Task 13: Roadmap update

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Tick the Stage 4 Conclave Archive line**

Open `docs/roadmap.md`, find:

```
- [ ] **Stage 4 — Conclave Archive (Io)** — multi-level Precursor ruin, Stellari-resonance crystal fixture
```

Replace with:

```
- [x] **Stage 4 — Conclave Archive (Io)** — multi-level Precursor ruin, Archon Sentinel boss, Stellari-resonance crystal fixture; reusable DungeonRecipe generator landed (2026-04-21)
```

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): tick Stage 4 Conclave Archive"
```

---

## Task 14: End-to-end smoke test

**Files:** none — manual verification only.

- [ ] **Step 1: Build clean**

Run: `cmake --build build -j`
Expected: clean build, zero new warnings (pre-existing warnings OK).

- [ ] **Step 2: Full flow in dev mode**

Run: `./build/astra-dev`

1. `quest finish story_stellar_signal_hook`
2. `quest finish story_stellar_signal_echoes`
3. `quest finish story_stellar_signal_beacon`
4. Accept `story_stellar_signal_conclave_probe` via NPC offer.
5. Warp into any Conclave-controlled system → Conclave warning transmission plays → Return popup → Accept.
6. Warp to Sol → ARIA panic transmission → Siege popup (Nova text) → Accept.
7. Star chart → THA → `t` → Automated Response transmission, no dock (lockdown intercept working).
8. Star chart → Jupiter → Io moon → `t` → land on Io. Siege objective 1 completes.
9. Io surface: expect Conclave Sentry patrols and a small Precursor ruin near center with a visible hatch glyph (`v`, cyan color 135). Fight to the hatch; interact.
10. L1: spawn at `<` StairsUp. Fight 4 Conclave Sentries. Find `>` StairsDown. Interact.
11. L2: spawn at `<` StairsUp. Verify ascending from here returns to L1 at the exact StairsDown tile you used. Descend again. Fight Heavy Sentries + Archon Remnants. Descend.
12. L3: spawn at `<` StairsUp. Fight to the back chamber. Kill Archon Sentinel. Interact `*` crystal fixture.
13. Audio log plays (placeholder text, acceptable for now). On dismiss: Siege completes, XP + credits posted, `journal_on_complete` written, THA lockdown clears.
14. Star chart → THA → `t` → normal docking succeeds (lockdown cleared).

Save/load round-trip:
- Save mid-L2. Quit. Relaunch, Load. Resume in L2 with preserved state (npcs, fixture interaction history, stairs).
- Save on Io surface. Reload. Recipe still registered (verify by descending through the hatch).

- [ ] **Step 2: If any step fails**

Stop. Fix the regression. Do not proceed to the next task batch.

- [ ] **Step 3: Commit** (no files to commit for this task — it's a verification gate).

---

## Final Verification Checklist

Before declaring the slice done:

- [ ] Clean build on default target (`cmake --build build -j`)
- [ ] `./build/astra-dev` launches without error
- [ ] Dev console spawn works for `heavy_conclave_sentry`
- [ ] Siege objective 1 ("Land on Io") ticks on Io arrival
- [ ] Precursor Archive POI + DungeonHatch visible on Io surface
- [ ] L1 → L2 → L3 descent chain works
- [ ] Ascent from every level lands at the matching parent stair (bidirectional linkage)
- [ ] Archon Sentinel boss spawns on L3
- [ ] Crystal fixture plays audio log (placeholder text acceptable; user fills verbatim later)
- [ ] Crystal interaction completes Siege quest
- [ ] Siege completion clears `tha_lockdown` (verify by docking THA)
- [ ] Save/load mid-dungeon round-trips correctly
- [ ] Roadmap ticked

---

## Open questions to resolve during or after this slice

1. **Civ-config field list** — when adding `civ_config_precursor()`, match the exact field list in `CivConfig`. If fields differ from what this plan shows, drop the unknown ones and add a note in the commit message.
2. **TileMap API naming** — the stair-finder and fixture-placement code in Task 7/8 uses presumed method names. Adjust per real API; the structure doesn't change.
3. **Recipe kind_tag as location name** — using `"conclave_archive"` as the level's location name in `set_location_name` is a stopgap. A proper per-level name ("Outer Ruin", "Inner Sanctum", "Crystal Vault") is a polish pass — not blocking.
4. **Placeholder crystal audio** — ships with placeholder; user replaces with verbatim lines from canonical narrative file post-merge.
5. **Heavy Sentry glyph `H`** — may conflict with something; if so, switch to another upper letter; no functional impact.
