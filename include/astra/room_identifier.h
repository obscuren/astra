#pragma once

#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <random>

namespace astra {

class RoomIdentifier {
public:
    void identify(const TileMap& map, RuinPlan& plan,
                  std::mt19937& rng) const;

private:
    BuildingType theme_for_geometry(const RuinRoom& room,
                                   const CivConfig& civ,
                                   std::mt19937& rng) const;
};

// Flood-fill every connected passable component that is not already
// inside a tagged region, and register each as a new region with the
// given default type. Safety-net for layouts that don't tag regions.
void tag_connected_components(TileMap& map, RegionType default_type);

} // namespace astra
