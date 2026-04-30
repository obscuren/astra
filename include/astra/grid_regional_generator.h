#pragma once

#include "astra/grid_sector.h"

#include <cstdint>

namespace astra::grid_regional_generator {

// Generates a regional darknet sector — 40×24, 4–8 firewall-bordered rooms
// connected by floor doorways, decorated with one ExitNode plus a handful of
// EncryptedFiles, DataNodes, and (sometimes) a deep-Grid Gateway. Style stays
// in the regional palette (firewall ▓ + floor ░).
GridSector generate(uint32_t seed, int security_tier,
                    int min_rooms = 4, int max_rooms = 8);

} // namespace astra::grid_regional_generator
