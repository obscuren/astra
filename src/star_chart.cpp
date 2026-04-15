#include "astra/star_chart.h"
#include "astra/lore_types.h"
#include "astra/station_roll.h"
#include "astra/time_of_day.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace astra {

// ---------------------------------------------------------------------------
// Name generation
// ---------------------------------------------------------------------------

static const char* prefixes[] = {
    "Arc", "Nyx", "Vox", "Kel", "Zar", "Ori", "Hel", "Tor",
    "Lum", "Cyr", "Fen", "Dra", "Syl", "Ven", "Kor", "Ash",
    "Rix", "Pho", "Zan", "Ira", "Myr", "Cal", "Eos", "Tyn",
    "Ald", "Bel", "Cas", "Del", "Exa", "Gal", "Hex", "Ion",
    "Jov", "Kha", "Lys", "Mag", "Neb", "Oph", "Pyr", "Qui",
};

static const char* suffixes[] = {
    "tani", "ios", "ara", "eon", "ium", "is", "ax", "on",
    "ael", "oth", "une", "ix", "us", "al", "or", "en",
    "an", "ari", "eth", "ine", "ova", "ux", "yr", "os",
};

static const char* designations[] = {
    "Prime", "II", "III", "IV", "V", "VI", "VII",
    "Alpha", "Beta", "Gamma", "Delta",
};

std::string generate_system_name(std::mt19937& rng) {
    std::uniform_int_distribution<int> prefix_dist(0, 39);
    std::uniform_int_distribution<int> suffix_dist(0, 23);
    std::uniform_int_distribution<int> desig_dist(0, 10);
    std::uniform_int_distribution<int> style(0, 3);

    int s = style(rng);
    if (s == 0) {
        // "Prefix-Number"
        std::uniform_int_distribution<int> num(2, 99);
        return std::string(prefixes[prefix_dist(rng)]) + "-" + std::to_string(num(rng));
    } else if (s == 1) {
        // "PrefixSuffix"
        return std::string(prefixes[prefix_dist(rng)]) + suffixes[suffix_dist(rng)];
    } else if (s == 2) {
        // "PrefixSuffix Designation"
        return std::string(prefixes[prefix_dist(rng)]) + suffixes[suffix_dist(rng)]
               + " " + designations[desig_dist(rng)];
    } else {
        // "Prefix-Number Designation"
        std::uniform_int_distribution<int> num(1, 47);
        return std::string(prefixes[prefix_dist(rng)]) + "-" + std::to_string(num(rng))
               + " " + designations[desig_dist(rng)];
    }
}

// ---------------------------------------------------------------------------
// Station name generation
// ---------------------------------------------------------------------------

static const char* station_prefixes[] = {
    "Waystation", "Outpost", "Relay", "Station", "Hub",
    "Beacon", "Port", "Depot", "Anchorage", "Haven",
};

static const char* station_name_parts[] = {
    "Orion", "Cygnus", "Vega", "Altair", "Rigel", "Lyra",
    "Draco", "Corvus", "Aquila", "Hydra", "Pavo", "Indus",
    "Mensa", "Pyxis", "Norma", "Ara", "Crux", "Lupus",
};

std::string generate_station_name(std::mt19937& rng) {
    std::uniform_int_distribution<int> prefix_dist(0, 9);
    std::uniform_int_distribution<int> name_dist(0, 17);
    std::uniform_int_distribution<int> style(0, 3);
    std::uniform_int_distribution<int> num_dist(1, 99);

    int s = style(rng);
    if (s == 0) {
        // "Waystation Orion"
        return std::string(station_prefixes[prefix_dist(rng)]) + " "
               + station_name_parts[name_dist(rng)];
    } else if (s == 1) {
        // "Orion Relay"
        return std::string(station_name_parts[name_dist(rng)]) + " "
               + station_prefixes[prefix_dist(rng)];
    } else if (s == 2) {
        // "Station Orion-7"
        return std::string(station_prefixes[prefix_dist(rng)]) + " "
               + station_name_parts[name_dist(rng)] + "-"
               + std::to_string(num_dist(rng));
    } else {
        // "Orion-42 Outpost"
        return std::string(station_name_parts[name_dist(rng)]) + "-"
               + std::to_string(num_dist(rng)) + " "
               + station_prefixes[prefix_dist(rng)];
    }
}

// ---------------------------------------------------------------------------
// System generation from seed + position
// ---------------------------------------------------------------------------

void generate_system(StarSystem& sys, uint32_t seed, float gx, float gy) {
    std::mt19937 rng(seed);
    sys.id = seed;
    sys.gx = gx;
    sys.gy = gy;
    sys.name = generate_system_name(rng);

    // Star class: weighted toward M/K
    std::uniform_int_distribution<int> class_roll(0, 99);
    int r = class_roll(rng);
    if (r < 40)       sys.star_class = StarClass::ClassM;
    else if (r < 70)  sys.star_class = StarClass::ClassK;
    else if (r < 82)  sys.star_class = StarClass::ClassG;
    else if (r < 90)  sys.star_class = StarClass::ClassF;
    else if (r < 95)  sys.star_class = StarClass::ClassA;
    else if (r < 98)  sys.star_class = StarClass::ClassB;
    else               sys.star_class = StarClass::ClassO;

    // Binary: ~30%
    std::uniform_int_distribution<int> binary_roll(0, 99);
    sys.binary = binary_roll(rng) < 30;

    // Station: ~80%
    std::uniform_int_distribution<int> station_roll(0, 99);
    sys.has_station = station_roll(rng) < 80;

    if (sys.has_station) {
        // Station seed derived from system seed (same as sys.id) for reproducibility.
        // Task 14's distribution test should use this same derivation.
        uint64_t station_seed = static_cast<uint64_t>(seed);
        sys.station.type = roll_station_type(station_seed);
        sys.station.specialty = (sys.station.type == StationType::NormalHub)
            ? roll_station_specialty(station_seed)
            : StationSpecialty::Generic;
        sys.station.keeper_seed = derive_keeper_seed(station_seed);
        sys.station.name = generate_station_name(rng);
    }

    // Planets and belts
    std::uniform_int_distribution<int> planet_dist(0, 8);
    std::uniform_int_distribution<int> belt_dist(0, 3);
    sys.planet_count = planet_dist(rng);
    sys.asteroid_belts = belt_dist(rng);

    // Danger level: scales with proximity to core (0,0)
    float dist = std::sqrt(gx * gx + gy * gy);
    float danger_base = 10.0f * (1.0f - dist / 220.0f);
    if (danger_base < 1.0f) danger_base = 1.0f;
    if (danger_base > 10.0f) danger_base = 10.0f;
    std::uniform_int_distribution<int> danger_jitter(-1, 1);
    sys.danger_level = static_cast<int>(danger_base) + danger_jitter(rng);
    if (sys.danger_level < 1) sys.danger_level = 1;
    if (sys.danger_level > 10) sys.danger_level = 10;
}

// ---------------------------------------------------------------------------
// Celestial body generation
// ---------------------------------------------------------------------------

static const char* roman_numerals[] = {
    "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X",
    "XI", "XII", "XIII", "XIV", "XV",
};

// ---------------------------------------------------------------------------
// Moon name generation
// ---------------------------------------------------------------------------

static const char* moon_prefixes[] = {
    "Ath", "Cal", "Dor", "Ery", "Gal", "Hel", "Ith", "Kyr",
    "Lar", "Myr", "Nyx", "Obi", "Pel", "Rha", "Syr", "Thal",
    "Umi", "Val", "Xen", "Zor",
};

static const char* moon_suffixes[] = {
    "os", "is", "on", "us", "ax", "en", "ar", "ia",
    "ys", "or", "ix", "al", "um", "as", "el", "an",
};

static std::vector<std::string> generate_moon_names(const std::string& body_name,
                                                     int count, std::mt19937& rng) {
    std::vector<std::string> names;
    std::uniform_int_distribution<int> prefix_dist(0, 19);
    std::uniform_int_distribution<int> suffix_dist(0, 15);

    for (int i = 0; i < count; ++i) {
        std::string name = std::string(moon_prefixes[prefix_dist(rng)])
                         + moon_suffixes[suffix_dist(rng)];
        names.push_back(std::move(name));
    }
    return names;
}

// ---------------------------------------------------------------------------
// Sol bodies
// ---------------------------------------------------------------------------

static void generate_sol_bodies(StarSystem& sys) {
    using R = Resource;
    auto r = [](Resource a, Resource b) { return static_cast<uint16_t>(a | b); };
    auto r1 = [](Resource a) { return static_cast<uint16_t>(a); };

    auto body = [](const char* name, BodyType type, Atmosphere atmo, Temperature temp,
                   uint16_t res, uint8_t sz, uint8_t moons, std::vector<std::string> mnames,
                   float dist, bool land, bool explored, bool dungeon, int danger) {
        CelestialBody b;
        b.name = name;
        b.type = type;
        b.atmosphere = atmo;
        b.temperature = temp;
        b.resources = res;
        b.size = sz;
        b.moons = moons;
        b.moon_names = std::move(mnames);
        b.orbital_distance = dist;
        b.landable = land;
        b.explored = explored;
        b.has_dungeon = dungeon;
        b.danger_level = danger;
        return b;
    };

    sys.bodies = {
        body("Mercury",       BodyType::Rocky,        Atmosphere::None,     Temperature::Scorching, r1(R::Metals),              2, 0, {}, 0.39f,  true,  false, false, 2),
        body("Venus",         BodyType::Rocky,        Atmosphere::Toxic,    Temperature::Scorching, r(R::Metals, R::RareMetals),5, 0, {}, 0.72f,  true,  false, false, 4),
        body("Earth",         BodyType::Terrestrial,  Atmosphere::Standard, Temperature::Temperate, r(R::Water, R::Organics),   5, 1, {"Luna"}, 1.0f,   true,  false, false, 1),
        body("Mars",          BodyType::Rocky,        Atmosphere::Thin,     Temperature::Cold,      r(R::Metals, R::Water),     3, 2, {"Phobos", "Deimos"}, 1.52f,  true,  false, true,  2),
        body("Asteroid Belt", BodyType::AsteroidBelt, Atmosphere::None,     Temperature::Cold,      r(R::Metals, R::RareMetals),1, 0, {}, 2.7f,   false, false, false, 3),
        body("Jupiter",       BodyType::GasGiant,     Atmosphere::Reducing, Temperature::Frozen,    r(R::Gas, R::Fuel),         10,4, {"Io", "Europa", "Ganymede", "Callisto"}, 5.2f,   false, false, false, 3),
        body("Saturn",        BodyType::GasGiant,     Atmosphere::Reducing, Temperature::Frozen,    r(R::Gas, R::Fuel),         9, 3, {"Titan", "Enceladus", "Mimas"}, 9.5f,   false, false, false, 3),
        body("Uranus",        BodyType::IceGiant,     Atmosphere::Reducing, Temperature::Frozen,    r1(R::Gas),                 7, 2, {"Titania", "Oberon"}, 19.2f,  false, false, false, 2),
        body("Neptune",       BodyType::IceGiant,     Atmosphere::Reducing, Temperature::Frozen,    r1(R::Gas),                 7, 1, {"Triton"}, 30.0f,  false, false, false, 2),
        body("Pluto",         BodyType::DwarfPlanet,  Atmosphere::Thin,     Temperature::Frozen,    r(R::Water, R::Crystals),   1, 1, {"Charon"}, 39.5f,  true,  false, true,  1),
        body("Kuiper Belt",   BodyType::AsteroidBelt, Atmosphere::None,     Temperature::Frozen,    r(R::Metals, R::Crystals),  1, 0, {}, 45.0f,  false, false, false, 2),
    };
}

void generate_system_bodies(StarSystem& sys) {
    if (sys.bodies_generated) return;
    sys.bodies_generated = true;

    // Sol — hardcoded real planets
    if (sys.id == 1) {
        generate_sol_bodies(sys);
        return;
    }

    // Sgr A* — supermassive black hole, no bodies
    if (sys.id == 0) return;

    // Neutron stars: no habitable zone, no procedural planets.
    // Custom systems pre-fill bodies; the idempotence guard at the top
    // prevents this branch from clobbering them.
    if (sys.star_class == StarClass::Neutron) {
        return;
    }

    std::mt19937 rng(sys.id ^ 0x504C4E54u);

    // Habitable zone bounds based on star class
    float hz_inner, hz_outer;
    switch (sys.star_class) {
        case StarClass::Neutron: // unreachable due to early return above, but silences -Wswitch
        case StarClass::ClassM: hz_inner = 0.1f;  hz_outer = 0.4f;  break;
        case StarClass::ClassK: hz_inner = 0.4f;  hz_outer = 0.8f;  break;
        case StarClass::ClassG: hz_inner = 0.8f;  hz_outer = 1.5f;  break;
        case StarClass::ClassF: hz_inner = 1.0f;  hz_outer = 2.0f;  break;
        case StarClass::ClassA: hz_inner = 1.5f;  hz_outer = 3.0f;  break;
        case StarClass::ClassB: hz_inner = 3.0f;  hz_outer = 8.0f;  break;
        case StarClass::ClassO: hz_inner = 5.0f;  hz_outer = 15.0f; break;
    }

    int total_bodies = sys.planet_count + sys.asteroid_belts;
    if (total_bodies <= 0) return;

    std::uniform_real_distribution<float> spacing_dist(1.4f, 2.2f);
    std::uniform_int_distribution<int> size_dist(1, 10);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<int> moon_rocky(0, 2);
    std::uniform_int_distribution<int> moon_gas(1, 6);

    float distance = 0.2f + hz_inner * 0.3f; // start near inner zone
    int planet_num = 0;
    int belt_num = 0;
    int belts_placed = 0;

    for (int i = 0; i < total_bodies; ++i) {
        CelestialBody body;
        body.orbital_distance = distance;

        // Place asteroid belts between inner and outer zones
        if (belts_placed < sys.asteroid_belts &&
            distance > hz_outer && distance < hz_outer * 3.0f &&
            percent(rng) < 50) {
            body.type = BodyType::AsteroidBelt;
            body.name = sys.name + " Belt";
            if (belts_placed > 0) body.name += " " + std::to_string(belts_placed + 1);
            body.atmosphere = Atmosphere::None;
            body.temperature = (distance < hz_inner) ? Temperature::Hot : Temperature::Cold;
            body.size = 1;
            body.moons = 0;
            body.landable = false;
            body.resources = static_cast<uint16_t>(Resource::Metals);
            if (percent(rng) < 30) body.resources |= static_cast<uint16_t>(Resource::RareMetals);
            body.danger_level = std::max(1, sys.danger_level - 1);
            ++belts_placed;
        } else {
            // Planet
            if (planet_num < 15) {
                body.name = sys.name + " " + roman_numerals[planet_num];
            } else {
                body.name = sys.name + " " + std::to_string(planet_num + 1);
            }
            ++planet_num;

            // Determine type based on zone
            if (distance < hz_inner) {
                // Inner zone — rocky, hot
                body.type = BodyType::Rocky;
                body.temperature = (distance < hz_inner * 0.5f) ? Temperature::Scorching : Temperature::Hot;
                body.atmosphere = (percent(rng) < 30) ? Atmosphere::Thin : Atmosphere::None;
                body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(1, 5)(rng));
                body.moons = static_cast<uint8_t>(moon_rocky(rng));
                body.landable = true;
                body.resources = static_cast<uint16_t>(Resource::Metals);
                if (percent(rng) < 20) body.resources |= static_cast<uint16_t>(Resource::RareMetals);
            } else if (distance >= hz_inner && distance <= hz_outer) {
                // Habitable zone
                if (percent(rng) < 40) {
                    body.type = BodyType::Terrestrial;
                    body.atmosphere = Atmosphere::Standard;
                    body.resources = static_cast<uint16_t>(Resource::Water | Resource::Organics);
                    if (percent(rng) < 30) body.resources |= static_cast<uint16_t>(Resource::Metals);
                } else {
                    body.type = BodyType::Rocky;
                    body.atmosphere = (percent(rng) < 50) ? Atmosphere::Thin : Atmosphere::None;
                    body.resources = static_cast<uint16_t>(Resource::Metals);
                    if (percent(rng) < 25) body.resources |= static_cast<uint16_t>(Resource::Water);
                }
                body.temperature = Temperature::Temperate;
                body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(2, 7)(rng));
                body.moons = static_cast<uint8_t>(moon_rocky(rng));
                body.landable = true;
            } else if (distance <= hz_outer * 5.0f) {
                // Outer zone — gas/ice giants
                if (percent(rng) < 60) {
                    body.type = BodyType::GasGiant;
                    body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(7, 10)(rng));
                    body.resources = static_cast<uint16_t>(Resource::Gas | Resource::Fuel);
                } else {
                    body.type = BodyType::IceGiant;
                    body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(5, 8)(rng));
                    body.resources = static_cast<uint16_t>(Resource::Gas);
                }
                body.atmosphere = Atmosphere::Reducing;
                body.temperature = Temperature::Frozen;
                body.moons = static_cast<uint8_t>(moon_gas(rng));
                body.landable = false;
            } else {
                // Far zone — dwarf planets or ice giants
                if (percent(rng) < 70) {
                    body.type = BodyType::DwarfPlanet;
                    body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(1, 3)(rng));
                    body.landable = true;
                    body.resources = static_cast<uint16_t>(Resource::Water);
                    if (percent(rng) < 20) body.resources |= static_cast<uint16_t>(Resource::Crystals);
                } else {
                    body.type = BodyType::IceGiant;
                    body.size = static_cast<uint8_t>(std::uniform_int_distribution<int>(4, 7)(rng));
                    body.landable = false;
                    body.resources = static_cast<uint16_t>(Resource::Gas);
                }
                body.atmosphere = (body.type == BodyType::DwarfPlanet) ? Atmosphere::Thin : Atmosphere::Reducing;
                body.temperature = Temperature::Frozen;
                body.moons = (body.type == BodyType::DwarfPlanet) ?
                    static_cast<uint8_t>(moon_rocky(rng)) : static_cast<uint8_t>(moon_gas(rng));
            }

            // Dungeon on landable bodies with some probability
            body.has_dungeon = body.landable && (percent(rng) < 40);
            body.danger_level = std::max(1, sys.danger_level + std::uniform_int_distribution<int>(-2, 2)(rng));
            if (body.danger_level > 10) body.danger_level = 10;

            // Generate moon names
            if (body.moons > 0) {
                body.moon_names = generate_moon_names(body.name, body.moons, rng);
            }

            body.day_length = derive_day_length(
                static_cast<int>(body.type), body.size, body.orbital_distance);
        }

        sys.bodies.push_back(std::move(body));
        distance *= spacing_dist(rng);
    }

    // Place remaining belts if we didn't get them all
    while (belts_placed < sys.asteroid_belts) {
        CelestialBody belt;
        belt.type = BodyType::AsteroidBelt;
        belt.name = sys.name + " Belt";
        if (belts_placed > 0) belt.name += " " + std::to_string(belts_placed + 1);
        belt.orbital_distance = distance;
        belt.atmosphere = Atmosphere::None;
        belt.temperature = Temperature::Frozen;
        belt.size = 1;
        belt.landable = false;
        belt.resources = static_cast<uint16_t>(Resource::Metals);
        belt.danger_level = std::max(1, sys.danger_level - 1);
        sys.bodies.push_back(std::move(belt));
        ++belts_placed;
        distance *= spacing_dist(rng);
    }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

const char* star_class_name(StarClass sc) {
    switch (sc) {
        case StarClass::ClassM: return "M (Red Dwarf)";
        case StarClass::ClassK: return "K (Orange Star)";
        case StarClass::ClassG: return "G (Yellow Star)";
        case StarClass::ClassF: return "F (Yellow-White)";
        case StarClass::ClassA: return "A (White Star)";
        case StarClass::ClassB: return "B (Blue-White)";
        case StarClass::ClassO: return "O (Blue Giant)";
        case StarClass::Neutron: return "Neutron (Pulsar Remnant)";
    }
    return "Unknown";
}

char star_class_glyph(StarClass sc) {
    switch (sc) {
        case StarClass::ClassO:
        case StarClass::ClassB: return '*';
        case StarClass::ClassA:
        case StarClass::ClassF: return '*';
        case StarClass::ClassG: return '*';
        case StarClass::ClassK: return '*';
        case StarClass::ClassM: return '.';
        case StarClass::Neutron: return '+';
    }
    return '.';
}

Color star_class_color(StarClass sc) {
    switch (sc) {
        case StarClass::ClassM: return Color::Red;
        case StarClass::ClassK: return static_cast<Color>(208); // orange
        case StarClass::ClassG: return Color::Yellow;
        case StarClass::ClassF: return Color::White;
        case StarClass::ClassA: return Color::White;
        case StarClass::ClassB: return Color::Cyan;
        case StarClass::ClassO: return Color::Blue;
        case StarClass::Neutron: return Color::BrightWhite;
    }
    return Color::White;
}

float system_distance(const StarSystem& a, const StarSystem& b) {
    float dx = a.gx - b.gx;
    float dy = a.gy - b.gy;
    return std::sqrt(dx * dx + dy * dy);
}

void discover_nearby(NavigationData& nav, uint32_t system_id, float radius) {
    const StarSystem* source = nullptr;
    for (const auto& s : nav.systems) {
        if (s.id == system_id) { source = &s; break; }
    }
    if (!source) return;

    float r2 = radius * radius;
    for (auto& s : nav.systems) {
        float dx = s.gx - source->gx;
        float dy = s.gy - source->gy;
        if (dx * dx + dy * dy <= r2) {
            s.discovered = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Galaxy generation — 4 spiral arms + bulge + halo
// ---------------------------------------------------------------------------

// Place a system along a logarithmic spiral arm.
// To avoid clustering near the core (where r is small), we distribute
// systems evenly by *radius* and solve for the corresponding theta.
static void place_arm_systems(std::vector<StarSystem>& systems,
                              std::mt19937& rng, unsigned game_seed,
                              float arm_offset, int count,
                              float a, float b,
                              float r_min, float r_max,
                              float scatter) {
    std::uniform_real_distribution<float> r_dist(r_min, r_max);
    std::uniform_real_distribution<float> scatter_dist(-scatter, scatter);

    for (int i = 0; i < count; ++i) {
        float r = r_dist(rng);
        // Invert r = a * exp(b * theta) → theta = ln(r/a) / b
        float theta = std::log(r / a) / b;

        float gx = r * std::cos(theta + arm_offset) + scatter_dist(rng);
        float gy = r * std::sin(theta + arm_offset) + scatter_dist(rng);

        // Clamp to galaxy radius
        float dist = std::sqrt(gx * gx + gy * gy);
        if (dist > 210.0f) continue;

        uint32_t seed = game_seed ^ static_cast<uint32_t>(systems.size() + 1) * 2654435761u;
        StarSystem sys;
        generate_system(sys, seed, gx, gy);
        systems.push_back(std::move(sys));
    }
}

static void place_bulge_systems(std::vector<StarSystem>& systems,
                                std::mt19937& rng, unsigned game_seed,
                                int count, float max_radius) {
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.2832f);
    std::uniform_real_distribution<float> radius_dist(0.0f, 1.0f);

    for (int i = 0; i < count; ++i) {
        float angle = angle_dist(rng);
        // sqrt gives uniform area distribution (not center-biased)
        float r = max_radius * std::sqrt(radius_dist(rng));

        float gx = r * std::cos(angle);
        float gy = r * std::sin(angle);

        uint32_t seed = game_seed ^ static_cast<uint32_t>(systems.size() + 1) * 2654435761u;
        StarSystem sys;
        generate_system(sys, seed, gx, gy);
        systems.push_back(std::move(sys));
    }
}

NavigationData generate_galaxy(unsigned game_seed) {
    NavigationData nav;
    std::mt19937 rng(game_seed ^ 0x47414C41u); // "GALA"

    // Spiral parameters
    float a = 5.0f;     // inner radius scale
    float b = 0.18f;    // spiral tightness
    float r_min = 20.0f;  // arms start outside the bulge
    float r_max = 200.0f; // galaxy edge
    float scatter = 10.0f;

    // 4 arms, each offset by pi/2
    // Sagittarius (Sol's arm), Perseus, Norma-Outer, Scutum-Centaurus
    float arm_offsets[] = { 0.0f, 1.5708f, 3.1416f, 4.7124f };
    int systems_per_arm = 210;

    for (int arm = 0; arm < 4; ++arm) {
        place_arm_systems(nav.systems, rng, game_seed,
                          arm_offsets[arm], systems_per_arm,
                          a, b, r_min, r_max, scatter);
    }

    // Central bulge — sparse, spread across inner region
    place_bulge_systems(nav.systems, rng, game_seed, 40, 50.0f);

    // Halo — sparse, scattered throughout
    std::uniform_real_distribution<float> halo_angle(0.0f, 6.2832f);
    std::uniform_real_distribution<float> halo_radius(30.0f, 200.0f);
    for (int i = 0; i < 50; ++i) {
        float angle = halo_angle(rng);
        float r = halo_radius(rng);
        float gx = r * std::cos(angle);
        float gy = r * std::sin(angle);

        uint32_t seed = game_seed ^ static_cast<uint32_t>(nav.systems.size() + 1) * 2654435761u;
        StarSystem sys;
        generate_system(sys, seed, gx, gy);
        nav.systems.push_back(std::move(sys));
    }

    // Sgr A* — special system at center
    {
        StarSystem sgr;
        sgr.id = 0;
        sgr.name = "Sgr A*";
        sgr.star_class = StarClass::ClassO;
        sgr.binary = false;
        sgr.has_station = false;
        sgr.planet_count = 0;
        sgr.asteroid_belts = 0;
        sgr.danger_level = 10;
        sgr.gx = 0.0f;
        sgr.gy = 0.0f;
        sgr.discovered = true;
        nav.systems.push_back(sgr);
    }

    // Sol — hardcoded in Sagittarius Arm
    {
        StarSystem sol;
        sol.id = 1;
        sol.name = "Sol";
        sol.star_class = StarClass::ClassG;
        sol.binary = false;
        sol.has_station = true;
        sol.station.name = "The Heavens Above";
        sol.station.type = StationType::NormalHub;
        sol.station.specialty = StationSpecialty::Generic;
        sol.planet_count = 8;
        sol.asteroid_belts = 1;
        sol.danger_level = 1;
        sol.gx = 180.0f;
        sol.gy = 0.0f;
        sol.discovered = true;
        nav.systems.push_back(sol);
    }

    nav.current_system_id = 1; // Sol

    // Discover Sol's nearest neighbors
    discover_nearby(nav, 1, 20.0f);

    return nav;
}

// ── apply_lore_to_galaxy ───────────────────────────────────────────────────
// Maps each simulated lore system to the nearest real star system by position.
// Transfers ruins, scars, beacons, megastructures, tiers onto real systems.

void apply_lore_to_galaxy(NavigationData& nav, const WorldLore& lore) {
    if (!lore.generated || lore.sim_systems.empty() || nav.systems.empty())
        return;

    // For each lore system, find the nearest real system and apply annotations
    std::unordered_set<size_t> claimed; // prevent double-mapping

    for (const auto& ls : lore.sim_systems) {
        float best_dist = 1e18f;
        size_t best_idx = 0;

        for (size_t i = 0; i < nav.systems.size(); ++i) {
            if (claimed.count(i)) continue;
            // Skip Sgr A* (id=0) and Sol (id=1) — keep them special
            if (nav.systems[i].id <= 1) continue;

            float dx = nav.systems[i].gx - ls.gx;
            float dy = nav.systems[i].gy - ls.gy;
            float d = dx * dx + dy * dy;
            if (d < best_dist) {
                best_dist = d;
                best_idx = i;
            }
        }

        if (best_dist > 1e17f) continue; // no system found

        claimed.insert(best_idx);
        auto& sys = nav.systems[best_idx];

        // Apply lore annotation
        sys.lore.lore_tier = ls.lore_tier;
        sys.lore.ruin_civ_ids = ls.ruin_civ_ids;
        sys.lore.has_megastructure = ls.has_megastructure;
        sys.lore.beacon = ls.beacon;
        sys.lore.battle_site = ls.battle_site;
        sys.lore.weapon_test_site = ls.weapon_test_site;
        sys.lore.plague_origin = ls.plague_origin;
        sys.lore.terraformed = ls.terraformed;
        sys.lore.terraformed_by_civ = ls.terraformed_by;
        sys.lore.scar_count = ls.scar_count;
        sys.lore.primary_civ_index = ls.primary_civ_index;
        if (ls.primary_civ_index >= 0 &&
            ls.primary_civ_index < static_cast<int>(lore.civilizations.size())) {
            sys.lore.primary_civ_architecture =
                lore.civilizations[ls.primary_civ_index].architecture;
        }

        // Set primary civilization name from the most recent ruin layer
        if (!ls.ruin_civ_ids.empty()) {
            int last_civ = ls.ruin_civ_ids.back();
            if (last_civ >= 0 && last_civ < static_cast<int>(lore.civilizations.size())) {
                sys.lore.primary_civ_name = lore.civilizations[last_civ].short_name;
            }
        }
        if (ls.megastructure_builder >= 0 &&
            ls.megastructure_builder < static_cast<int>(lore.civilizations.size())) {
            sys.lore.primary_civ_name = lore.civilizations[ls.megastructure_builder].short_name;
        }

        // Tier 3 systems get higher danger (ancient guardians, etc.)
        if (ls.lore_tier >= 3 && sys.danger_level < 8) {
            sys.danger_level = std::max(sys.danger_level, 7);
        } else if (ls.lore_tier >= 2 && sys.danger_level < 5) {
            sys.danger_level = std::max(sys.danger_level, 4);
        }
    }
}

// ---------------------------------------------------------------------------
// Custom system management
// ---------------------------------------------------------------------------

uint32_t add_custom_system(NavigationData& nav, CustomSystemSpec spec) {
    // Allocate an id, stepping past any collision (extremely unlikely).
    uint32_t id = nav.next_custom_system_id;
    while (std::any_of(nav.systems.begin(), nav.systems.end(),
                       [id](const StarSystem& s){ return s.id == id; })) {
        ++id;
    }
    nav.next_custom_system_id = id + 1;

    StarSystem sys;
    sys.id = id;
    sys.name = std::move(spec.name);
    sys.star_class = spec.star_class;
    sys.binary = spec.binary;
    sys.has_station = spec.has_station;
    sys.station = spec.station;
    sys.gx = spec.gx;
    sys.gy = spec.gy;
    sys.discovered = spec.discovered;
    sys.lore = std::move(spec.lore);
    sys.planet_count = 0;
    sys.asteroid_belts = 0;
    sys.danger_level = 1;

    if (!spec.bodies.empty()) {
        sys.bodies = std::move(spec.bodies);
        sys.bodies_generated = true;
    } else {
        sys.bodies_generated = false;
    }

    nav.systems.push_back(std::move(sys));
    return id;
}

static bool set_discovered(NavigationData& nav, uint32_t system_id, bool value) {
    for (auto& s : nav.systems) {
        if (s.id == system_id) {
            s.discovered = value;
            return true;
        }
    }
    return false;
}

bool reveal_system(NavigationData& nav, uint32_t system_id) {
    return set_discovered(nav, system_id, true);
}

bool hide_system(NavigationData& nav, uint32_t system_id) {
    return set_discovered(nav, system_id, false);
}

std::optional<std::pair<float, float>>
pick_coords_near(const NavigationData& nav, uint32_t ref_system_id,
                 float min_dist, float max_dist, std::mt19937& rng,
                 int max_attempts) {
    const StarSystem* ref = nullptr;
    for (const auto& s : nav.systems) {
        if (s.id == ref_system_id) { ref = &s; break; }
    }
    if (!ref) return std::nullopt;

    std::uniform_real_distribution<float> dr(min_dist, max_dist);
    std::uniform_real_distribution<float> dtheta(0.0f, 6.2831853f);

    // Reject any candidate too close to an existing system.
    constexpr float kMinSep = 0.25f;
    for (int i = 0; i < max_attempts; ++i) {
        float r = dr(rng);
        float t = dtheta(rng);
        float gx = ref->gx + r * std::cos(t);
        float gy = ref->gy + r * std::sin(t);

        bool collides = false;
        for (const auto& s : nav.systems) {
            float dx = s.gx - gx;
            float dy = s.gy - gy;
            if (dx * dx + dy * dy < kMinSep * kMinSep) { collides = true; break; }
        }
        if (!collides) return std::make_pair(gx, gy);
    }
    return std::nullopt;
}

} // namespace astra
