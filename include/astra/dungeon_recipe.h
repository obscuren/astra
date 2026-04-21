#pragma once

#include "astra/location_key.h"

#include <string>
#include <vector>

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
