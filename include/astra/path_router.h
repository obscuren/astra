#pragma once

#include "astra/settlement_types.h"

namespace astra {

class PathRouter {
public:
    void route(TileMap& map, const SettlementPlan& plan) const;
};

} // namespace astra
