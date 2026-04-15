#include "astra/body_presets.h"

namespace astra {

CelestialBody make_landable_asteroid(std::string name) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::LandableAsteroid;
    b.atmosphere = Atmosphere::None;
    b.temperature = Temperature::Cold;
    b.size = 1;
    b.moons = 0;
    b.orbital_distance = 1.0f;
    b.landable = true;
    b.explored = false;
    b.has_dungeon = false;
    b.danger_level = 1;
    b.day_length = 200;
    return b;
}

CelestialBody make_scar_planet(std::string name, Biome scar_biome) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::Terrestrial;
    b.atmosphere = Atmosphere::Thin;
    b.temperature = Temperature::Hot;
    b.size = 3;
    b.moons = 0;
    b.orbital_distance = 1.0f;
    b.landable = true;
    b.explored = false;
    b.has_dungeon = false;
    b.danger_level = 4;
    b.day_length = 200;
    b.biome_override = scar_biome;
    return b;
}

} // namespace astra
