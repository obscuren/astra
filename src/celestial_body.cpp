#include "astra/celestial_body.h"

namespace astra {

char body_type_glyph(BodyType type) {
    switch (type) {
        case BodyType::Rocky:        return 'o';
        case BodyType::GasGiant:     return 'O';
        case BodyType::IceGiant:     return 'O';
        case BodyType::Terrestrial:  return 'o';
        case BodyType::DwarfPlanet:  return '.';
        case BodyType::AsteroidBelt: return '~';
    }
    return '?';
}

Color body_type_color(BodyType type) {
    switch (type) {
        case BodyType::Rocky:        return Color::DarkGray;
        case BodyType::GasGiant:     return Color::Yellow;
        case BodyType::IceGiant:     return Color::Cyan;
        case BodyType::Terrestrial:  return Color::Green;
        case BodyType::DwarfPlanet:  return Color::DarkGray;
        case BodyType::AsteroidBelt: return Color::White;
    }
    return Color::White;
}

const char* body_type_name(BodyType type) {
    switch (type) {
        case BodyType::Rocky:        return "Rocky";
        case BodyType::GasGiant:     return "Gas Giant";
        case BodyType::IceGiant:     return "Ice Giant";
        case BodyType::Terrestrial:  return "Terrestrial";
        case BodyType::DwarfPlanet:  return "Dwarf Planet";
        case BodyType::AsteroidBelt: return "Asteroid Belt";
    }
    return "Unknown";
}

const char* atmosphere_name(Atmosphere atmo) {
    switch (atmo) {
        case Atmosphere::None:     return "None";
        case Atmosphere::Thin:     return "Thin";
        case Atmosphere::Standard: return "Standard";
        case Atmosphere::Dense:    return "Dense";
        case Atmosphere::Toxic:    return "Toxic";
        case Atmosphere::Reducing: return "Reducing";
    }
    return "Unknown";
}

const char* temperature_name(Temperature temp) {
    switch (temp) {
        case Temperature::Frozen:    return "Frozen";
        case Temperature::Cold:      return "Cold";
        case Temperature::Temperate: return "Temperate";
        case Temperature::Hot:       return "Hot";
        case Temperature::Scorching: return "Scorching";
    }
    return "Unknown";
}

} // namespace astra
