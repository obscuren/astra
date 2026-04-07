#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class BuildingGenerator {
public:
    void generate(TileMap& map,
                  const BuildingSpec& spec,
                  const CivStyle& style,
                  std::mt19937& rng) const;
};

} // namespace astra
