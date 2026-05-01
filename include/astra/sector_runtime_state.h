#pragma once

#include "astra/grid_sector.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace astra {

// Single tile mutation: a delta against the seed-regenerated base sector.
// Used to persist player actions (cracked firewall → Floor, looted DataNode → Floor,
// decrypted EncryptedFile → Floor, cracked DeepGridGateway → Floor) so they survive
// save/load and mid-jack-in sector traversal.
struct SectorMutation {
    uint8_t  x = 0;
    uint8_t  y = 0;
    GridTile new_tile = GridTile::Floor;
};

// Per-sector runtime overlay applied after seed-regen.
struct SectorRuntimeState {
    std::vector<SectorMutation>                       mutations;
    std::vector<std::pair<uint8_t, uint8_t>>          killed_ice;
};

// Apply mutations as overlay onto the regenerated base sector.
// Bounds-checks coordinates; out-of-range mutations are silently skipped
// (they were valid at the time of recording but a future generator change
// might shrink the sector — defensive).
void apply_mutations(GridSector& sector, const SectorRuntimeState& state);

} // namespace astra
