#pragma once

#include <cstdint>
#include <utility>

namespace astra::dungeon {

// Mutable scratch passed through layers.
// layer 2 writes entry_region_id / exit_region_id
// layer 6 writes stairs_up / stairs_dn
struct LevelContext {
    int                 depth         = 1;
    uint32_t            seed          = 0;
    std::pair<int,int>  entered_from  {-1, -1};   // descended-from coord on child map
    int                 entry_region_id = -1;
    int                 exit_region_id  = -1;
    std::pair<int,int>  stairs_up     {-1, -1};
    std::pair<int,int>  stairs_dn     {-1, -1};
};

} // namespace astra::dungeon
