#pragma once

#include "astra/celestial_body.h"

#include <string>

namespace astra {

// A single landable asteroid. Distinct chart glyph ('*') from
// AsteroidBelt ('~'). Used as the anchor body for quests that place
// a fixture on the asteroid's detail map (e.g. Nova's beacon).
CelestialBody make_landable_asteroid(std::string name);

// A terrestrial planet with a forced scarred biome (glassed by default,
// or scorched). Used by quests that place content on a war-ravaged world.
CelestialBody make_scar_planet(std::string name,
                               Biome scar_biome = Biome::ScarredGlassed);

} // namespace astra
