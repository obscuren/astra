#pragma once

#include "astra/rect.h"
#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"
#include "astra/map_properties.h"

#include <random>

namespace astra {

// Run POI generation. Returns the settlement footprint rect (valid if a
// settlement was placed, zero-sized otherwise).
Rect poi_phase(TileMap& map, const TerrainChannels& channels,
               const MapProperties& props, std::mt19937& rng);

} // namespace astra
