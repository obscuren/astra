#pragma once

#include "astra/settlement_types.h"

#include <random>

namespace astra {

class ExteriorDecorator {
public:
    void decorate(TileMap& map, const SettlementPlan& plan, std::mt19937& rng) const;
};

} // namespace astra
