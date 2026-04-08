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

} // namespace astra
