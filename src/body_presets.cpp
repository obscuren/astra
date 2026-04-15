#include "astra/body_presets.h"

namespace astra {

CelestialBody make_asteroid_orbit(std::string name) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::AsteroidBelt;
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

} // namespace astra
