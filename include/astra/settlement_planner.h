#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"

#include <random>

namespace astra {

class SettlementPlanner {
public:
    SettlementPlan plan(const PlacementResult& placement,
                        const TerrainChannels& channels,
                        const TileMap& map,
                        const MapProperties& props,
                        std::mt19937& rng) const;
};

} // namespace astra
