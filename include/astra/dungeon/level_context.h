#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra::dungeon {

// Mutable scratch passed through layers.
// - layer 2 writes entry/exit region ids, sanctum_region_id, chapel_region_ids, rooms.
// - layer 6.iii writes placed_required_fixtures.
// - layer 6.i/6.ii read placed_required_fixtures.
struct LevelContext {
    int                              depth             = 1;
    uint32_t                         seed              = 0;
    std::pair<int,int>               entered_from      {-1, -1};
    int                              entry_region_id   = -1;
    int                              exit_region_id    = -1;
    std::pair<int,int>               stairs_up         {-1, -1};
    std::pair<int,int>               stairs_dn         {-1, -1};

    // Layout chamber tagging (PrecursorVault populates these).
    int                              sanctum_region_id = -1;       // terminal chamber
    std::vector<int>                 chapel_region_ids;            // symmetric side chapels

    // Authored-layout stair placement hints. If set (>= 0), the stairs
    // placer picks the open cell closest to these points — this constrains
    // stairs to specific rooms even when the whole map is one connected
    // component. Layouts that don't care leave them -1.
    std::pair<int,int>               entry_pref        {-1, -1};
    std::pair<int,int>               exit_pref         {-1, -1};

    // Layer 6.iii output: fixture-kind -> list of placed tile positions.
    std::unordered_map<int, std::vector<std::pair<int,int>>>
                                     placed_required_fixtures;
};

inline int kind_key(FixtureKind k) { return static_cast<int>(k); }

} // namespace astra::dungeon
