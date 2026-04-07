#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"
#include "astra/map_properties.h"

#include <random>

namespace astra {

void poi_phase(TileMap& map, const TerrainChannels& channels,
               const MapProperties& props, std::mt19937& rng);

} // namespace astra
