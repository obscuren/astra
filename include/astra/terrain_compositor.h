#pragma once

#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/tilemap.h"

namespace astra {

void composite_terrain(TileMap& map, const TerrainChannels& channels,
                       const BiomeProfile& prof);

} // namespace astra
