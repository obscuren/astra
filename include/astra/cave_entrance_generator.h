#pragma once

#include "astra/map_properties.h"
#include "astra/rect.h"
#include "astra/terrain_channels.h"
#include "astra/tilemap.h"

#include <random>

namespace astra {

// Generates a cave entrance POI: dungeon portal on the surface with
// three lore-weighted variants (natural cave, abandoned mine, ancient
// excavation). Cave and mine variants embed into existing cliff walls
// found via a post-placement scan; ancient excavation sits on flatter
// ground and is the only variant allowed in non-cliff biomes.
//
// Parallel to RuinGenerator / CrashedShipGenerator — owns the entire
// stamping pipeline, stamps directly on the map without going through
// SettlementPlan.
class CaveEntranceGenerator {
public:
    // Returns the placement footprint on success, or an empty Rect if
    // the site couldn't be placed (no variant for the biome, no cliff
    // found, or PlacementScorer failed).
    Rect generate(TileMap& map,
                  const TerrainChannels& channels,
                  const MapProperties& props,
                  std::mt19937& rng) const;
};

} // namespace astra
