#pragma once

#include <cstdint>

#include "astra/grid_sector.h"

namespace astra {

struct DeadImplantGenInput {
    uint32_t seed            = 0;
    int      faction_id      = 0;   // for future palette / flavor (unused in v1)
    int      npc_threat_tier = 1;   // weak influence on size
};

// Generate a per-corpse dead-implant sector. Small (4x4 to 8x8 walkable
// interior). Reward distribution per Spec 1 §9.3:
//   ~60% empty, ~30% 1 reward, ~10% 2 rewards (hard cap at 2).
// Reward types use existing GridTile enum values as v1 placeholders;
// Spec 2 will replace with proper Cache / Schematic / Sigil tile types.
GridSector gen_dead_implant_sector(const DeadImplantGenInput& in);

}  // namespace astra
