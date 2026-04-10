#pragma once

#include "astra/settlement_types.h"
#include "astra/terrain_channels.h"

#include <random>

namespace astra {

// Plans a small fenced outpost: one main building inside a palisade fence,
// 2-4 hand-stamped tents outside the fence, and 1-2 campfire clusters. Reuses
// the settlement downstream stages (BuildingGenerator, PathRouter,
// PerimeterBuilder, ExteriorDecorator) by emitting a SettlementPlan. The
// post_stamp() method runs after those stages to hand-stamp tents, place
// campfires, and apply biome-specific fence glyph overrides.
class OutpostPlanner {
public:
    // Populate a SettlementPlan with terrain clear, the main building spec,
    // fence perimeter spec, and gate-to-door / gate-to-edge path specs.
    SettlementPlan plan(const PlacementResult& placement,
                        const TerrainChannels& channels,
                        const TileMap& map,
                        const MapProperties& props,
                        std::mt19937& rng) const;

    // Hand-stamp tents and campfires around the fence and apply biome
    // fence glyph overrides. Call after BuildingGenerator / PathRouter /
    // PerimeterBuilder / ExteriorDecorator have run.
    void post_stamp(TileMap& map, const SettlementPlan& plan,
                    Biome biome, std::mt19937& rng) const;
};

} // namespace astra
