#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class PerimeterBuilder {
public:
    void build(TileMap& map, const SettlementPlan& plan, std::mt19937& rng) const;
};

} // namespace astra
