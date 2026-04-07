#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"

namespace astra {

class PlacementScorer {
public:
    PlacementResult score(const TerrainChannels& channels,
                          const TileMap& map,
                          int footprint_w, int footprint_h,
                          int edge_margin = 15) const;
};

} // namespace astra
