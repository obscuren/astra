#pragma once

#include <cmath>
#include <cstdint>

namespace astra {

enum class FactionTerritory : uint8_t {
    Unclaimed        = 0,
    StellariConclave = 1,
    TerranFederation = 2,
    KrethMiningGuild = 3,
    VeldraniAccord   = 4,
};

// Three-octave sine domain warp. Perturbs galaxy coords before nearest-capital
// lookup so territorial borders become organic blobs instead of perfect circles.
// Amplitudes sum to ~7 units — small enough that territories stay close to
// their true influence radius (no long bleed), big enough for organic edges.
inline void warp_galaxy_coord(float& gx, float& gy) {
    const float ax = std::sin(gy * 0.035f) * 3.5f
                   + std::sin(gy * 0.090f + 1.7f) * 2.0f
                   + std::sin(gy * 0.230f + 3.4f) * 1.5f;
    const float ay = std::cos(gx * 0.035f) * 3.5f
                   + std::cos(gx * 0.090f + 2.3f) * 2.0f
                   + std::cos(gx * 0.230f + 4.1f) * 1.5f;
    gx += ax;
    gy += ay;
}

} // namespace astra
