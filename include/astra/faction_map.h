#pragma once

#include <cstdint>
#include <vector>

namespace astra {

constexpr int kFactionMapWidth  = 256;
constexpr int kFactionMapHeight = 256;

enum class FactionTerritory : uint8_t {
    Unclaimed        = 0,
    StellariConclave = 1,
    TerranFederation = 2,
    KrethMiningGuild = 3,
    VeldraniAccord   = 4,
};

struct FactionMap {
    std::vector<FactionTerritory> cells;
    float gx_min = 0.0f;
    float gx_max = 0.0f;
    float gy_min = 0.0f;
    float gy_max = 0.0f;
    bool empty() const { return cells.empty(); }
};

} // namespace astra
