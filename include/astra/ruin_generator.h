#pragma once

#include "astra/ruin_types.h"
#include "astra/tilemap.h"
#include "astra/terrain_channels.h"
#include "astra/map_properties.h"

#include <random>
#include <string>

namespace astra {

class RuinGenerator {
public:
    Rect generate(TileMap& map, const TerrainChannels& channels,
                  const MapProperties& props, std::mt19937& rng,
                  const std::string& civ_name = "") const;

private:
    void place_room_furniture(TileMap& map, const RuinRoom& room,
                              const CivConfig& civ,
                              std::mt19937& rng) const;

    void apply_edge_continuity(TileMap& map, const RuinPlan& plan,
                               const MapProperties& props) const;
};

} // namespace astra
