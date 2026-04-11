#pragma once

#include "astra/map_properties.h"
#include "astra/rect.h"
#include "astra/terrain_channels.h"
#include "astra/tilemap.h"

#include <random>

namespace astra {

// Generates a crashed ship POI: picks a ship class (EscapePod, Freighter,
// Corvette) from lore weights, picks a random 4-way orientation, scores a
// placement site, then stamps the long scorched skid mark, hull wreckage,
// interior bulkheads, hull breaches, debris field, and room fixtures
// directly onto the map. Roughly 20% of crashes include a dungeon portal.
//
// Parallel to RuinGenerator — owns the entire stamping pipeline; no
// SettlementPlan involved.
class CrashedShipGenerator {
public:
    // Returns the placement footprint on success, or an empty Rect if the
    // site couldn't be placed (e.g. Aquatic biome, or PlacementScorer
    // failed to find room).
    Rect generate(TileMap& map,
                  const TerrainChannels& channels,
                  const MapProperties& props,
                  std::mt19937& rng) const;
};

} // namespace astra
