#pragma once

#include "astra/grid_sector.h"
#include "astra/lan.h"

namespace astra {

class GridNetwork;

struct LanSizeParams {
    int office_count = 0;
    int ring_count   = 0;
    int width        = 0;
    int height       = 0;
};

// Per-spec §5 size formula. Independent of LanMetadata so callers can
// preview dimensions before committing.
LanSizeParams compute_lan_size(int n_nodes);

// Generate a procedural LAN sector for the given LAN. Returns an all-Floor
// sector for now (Task 20 stub); Tasks 21-24 add the firewall ring, office
// rooms, connectors, and gateway tiles.
//
// `net` is needed so the generator can look up Subnet GridNodeIds when it
// stamps `⌬` Gateway tiles (each tile carries a target_node_id that lets
// breach.exe / mid-jack-in traversal resolve which device the tile leads to).
GridSector generate_lan_sector(const LanMetadata& meta, const GridNetwork& net);

} // namespace astra
