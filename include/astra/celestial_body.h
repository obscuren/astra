#pragma once

#include "astra/renderer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class BodyType : uint8_t {
    Rocky,
    GasGiant,
    IceGiant,
    Terrestrial,
    DwarfPlanet,
    AsteroidBelt,
};

enum class Atmosphere : uint8_t {
    None,
    Thin,
    Standard,
    Dense,
    Toxic,
    Reducing,
};

enum class Temperature : uint8_t {
    Frozen,
    Cold,
    Temperate,
    Hot,
    Scorching,
};

enum class Resource : uint16_t {
    None       = 0,
    Metals     = 1 << 0,
    RareMetals = 1 << 1,
    Water      = 1 << 2,
    Fuel       = 1 << 3,
    Organics   = 1 << 4,
    Crystals   = 1 << 5,
    Radioactive= 1 << 6,
    Gas        = 1 << 7,
};

inline Resource operator|(Resource a, Resource b) {
    return static_cast<Resource>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline bool has_resource(uint16_t field, Resource r) {
    return (field & static_cast<uint16_t>(r)) != 0;
}

struct CelestialBody {
    std::string name;
    BodyType type = BodyType::Rocky;
    Atmosphere atmosphere = Atmosphere::None;
    Temperature temperature = Temperature::Cold;
    uint16_t resources = 0;
    uint8_t size = 1;
    uint8_t moons = 0;
    std::vector<std::string> moon_names;
    float orbital_distance = 0.0f;
    bool landable = false;
    bool explored = false;
    bool has_dungeon = false;
    int danger_level = 1;
};

// Display helpers
char body_type_glyph(BodyType type);
Color body_type_color(BodyType type);
const char* body_type_name(BodyType type);
const char* atmosphere_name(Atmosphere atmo);
const char* temperature_name(Temperature temp);

} // namespace astra
