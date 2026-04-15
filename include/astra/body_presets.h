#pragma once

#include "astra/celestial_body.h"

#include <string>

namespace astra {

// A bare asteroid intended to host a single quest fixture on its detail map.
// Used by Nova Stage 3's beacon system.
CelestialBody make_asteroid_orbit(std::string name);

} // namespace astra
