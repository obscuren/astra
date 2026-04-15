#include "astra/celestial_body.h"
#include "astra/time_of_day.h"

namespace astra {

char body_type_glyph(BodyType type) {
    switch (type) {
        case BodyType::Rocky:        return 'o';
        case BodyType::GasGiant:     return 'O';
        case BodyType::IceGiant:     return 'O';
        case BodyType::Terrestrial:  return 'o';
        case BodyType::DwarfPlanet:  return '.';
        case BodyType::AsteroidBelt: return '~';
        case BodyType::LandableAsteroid: return '*';
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
        case BodyType::LandableAsteroid: return Color::White;
    }
    return Color::White;
}

Color body_display_color(const CelestialBody& body) {
    // Mars-like: cold rocky with thin atmo = rust red
    if (body.type == BodyType::Rocky &&
        body.temperature == Temperature::Cold &&
        body.atmosphere == Atmosphere::Thin) {
        return static_cast<Color>(166);  // rust red
    }
    // Scorching rocky = reddish orange (Mercury/Venus-like)
    if (body.type == BodyType::Rocky && body.temperature == Temperature::Scorching) {
        return static_cast<Color>(208);  // orange
    }
    return body_type_color(body.type);
}

const char* body_type_name(BodyType type) {
    switch (type) {
        case BodyType::Rocky:        return "Rocky";
        case BodyType::GasGiant:     return "Gas Giant";
        case BodyType::IceGiant:     return "Ice Giant";
        case BodyType::Terrestrial:  return "Terrestrial";
        case BodyType::DwarfPlanet:  return "Dwarf Planet";
        case BodyType::AsteroidBelt: return "Asteroid Belt";
        case BodyType::LandableAsteroid: return "Landable Asteroid";
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

Biome determine_biome(BodyType type, Atmosphere atmo, Temperature temp, unsigned seed) {
    // Deterministic pick helper
    auto pick = [&](std::initializer_list<Biome> options) -> Biome {
        int n = static_cast<int>(options.size());
        if (n <= 1) return *options.begin();
        return *(options.begin() + (seed % n));
    };

    switch (type) {
        case BodyType::Rocky:
            // Cold rocky with thin atmo = Mars-like (rust-colored sand)
            if (temp == Temperature::Cold && atmo == Atmosphere::Thin)
                return Biome::Sandy;
            if (temp == Temperature::Frozen || temp == Temperature::Cold)
                return Biome::Ice;
            if (temp == Temperature::Scorching)
                return Biome::Volcanic;
            if (temp == Temperature::Hot)
                return Biome::Sandy;
            // Temperate
            if (atmo == Atmosphere::Thin)
                return Biome::Sandy; // mars-like
            return Biome::Rocky;

        case BodyType::Terrestrial:
            if (atmo == Atmosphere::Toxic)
                return Biome::Corroded;
            if (atmo == Atmosphere::Reducing)
                return Biome::Fungal;
            if (temp == Temperature::Frozen || temp == Temperature::Cold)
                return Biome::Ice;
            if (temp == Temperature::Scorching)
                return Biome::Volcanic;
            if (temp == Temperature::Hot) {
                if (atmo == Atmosphere::Dense)
                    return Biome::Jungle;
                return Biome::Sandy;
            }
            // Temperate
            if (atmo == Atmosphere::Dense)
                return pick({Biome::Aquatic, Biome::Jungle});
            // Standard or Thin temperate
            return pick({Biome::Fungal, Biome::Aquatic, Biome::Forest});

        case BodyType::DwarfPlanet:
            if (temp == Temperature::Frozen)
                return pick({Biome::Ice, Biome::Crystal});
            return Biome::Rocky;

        case BodyType::AsteroidBelt:
            return pick({Biome::Rocky, Biome::Crystal});

        case BodyType::LandableAsteroid:
            return pick({Biome::Rocky, Biome::Crystal});

        default:
            return Biome::Rocky;
    }
}

CelestialBody generate_moon_body(const CelestialBody& parent, int moon_index, unsigned seed) {
    unsigned h = seed ^ (static_cast<unsigned>(moon_index) * 6271u + 997u);
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    int roll = static_cast<int>(h % 100);

    CelestialBody moon;
    moon.name = (moon_index < static_cast<int>(parent.moon_names.size()))
                ? parent.moon_names[moon_index]
                : parent.name + " Moon " + std::to_string(moon_index + 1);
    moon.landable = true;
    moon.size = 1 + (h % 3);
    moon.moons = 0;
    moon.has_dungeon = ((h >> 8) % 100) < 60;
    moon.danger_level = std::max(1, parent.danger_level + static_cast<int>((h >> 4) % 3) - 1);

    if (parent.type == BodyType::GasGiant || parent.type == BodyType::IceGiant) {
        if (roll < 40) {
            // Frozen rocky (most common, like Europa/Ganymede)
            moon.type = BodyType::Rocky;
            moon.atmosphere = Atmosphere::None;
            moon.temperature = Temperature::Frozen;
        } else if (roll < 60) {
            // Tidally heated (Io-like)
            moon.type = BodyType::Rocky;
            moon.atmosphere = Atmosphere::Thin;
            moon.temperature = Temperature::Scorching;
        } else if (roll < 80) {
            // Cold rocky with thin atmosphere
            moon.type = BodyType::Rocky;
            moon.atmosphere = Atmosphere::Thin;
            moon.temperature = Temperature::Cold;
        } else {
            // Titan-like
            moon.type = BodyType::Terrestrial;
            moon.atmosphere = Atmosphere::Standard;
            moon.temperature = Temperature::Cold;
        }
    } else if (parent.type == BodyType::DwarfPlanet) {
        moon.type = BodyType::Rocky;
        moon.atmosphere = Atmosphere::None;
        moon.temperature = Temperature::Frozen;
    } else {
        // Rocky/Terrestrial parent — small rocky moon
        moon.type = BodyType::Rocky;
        moon.atmosphere = Atmosphere::None;
        // Same or colder temperature
        if (parent.temperature == Temperature::Frozen || parent.temperature == Temperature::Cold) {
            moon.temperature = parent.temperature;
        } else {
            // Roll between parent temp and one step colder
            int t = static_cast<int>(parent.temperature);
            int cooler = std::max(0, t - 1);
            moon.temperature = static_cast<Temperature>(cooler + static_cast<int>((h >> 12) % 2));
        }
    }

    moon.day_length = derive_moon_day_length(static_cast<int>(parent.type));
    return moon;
}

} // namespace astra
