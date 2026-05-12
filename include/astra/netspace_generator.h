#pragma once

#include "astra/netspace.h"

namespace astra {

// Phase 0 stub. Produces a deterministic 14x8 walkable room with a
// jack-in tile at (1, 4) and an exit tile at (12, 4). No ICE, no
// payloads, no entities. Phase 1+ replaces this with per-target
// grammars (gen_door_netspace, gen_vending_netspace, ...).
Netspace gen_empty_netspace(const TargetDescriptor& desc);

}  // namespace astra
