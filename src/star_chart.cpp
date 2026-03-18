#include "astra/star_chart.h"

#include <algorithm>
#include <cmath>

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

static void generate_sol_bodies(StarSystem& sys) {
    using R = Resource;
    auto r = [](Resource a, Resource b) { return static_cast<uint16_t>(a | b); };
    auto r1 = [](Resource a) { return static_cast<uint16_t>(a); };

    sys.bodies = {
        {"Mercury",       BodyType::Rocky,        Atmosphere::None,     Temperature::Scorching, r1(R::Metals),              2, 0, 0.39f,  true,  false, false, 2},
        {"Venus",         BodyType::Rocky,        Atmosphere::Toxic,    Temperature::Scorching, r(R::Metals, R::RareMetals),5, 0, 0.72f,  true,  false, false, 4},
        {"Earth",         BodyType::Terrestrial,  Atmosphere::Standard, Temperature::Temperate, r(R::Water, R::Organics),   5, 1, 1.0f,   true,  false, false, 1},
        {"Mars",          BodyType::Rocky,        Atmosphere::Thin,     Temperature::Cold,      r(R::Metals, R::Water),     3, 2, 1.52f,  true,  false, true,  2},
        {"Asteroid Belt", BodyType::AsteroidBelt, Atmosphere::None,     Temperature::Cold,      r(R::Metals, R::RareMetals),1, 0, 2.7f,   false, false, false, 3},
        {"Jupiter",       BodyType::GasGiant,     Atmosphere::Reducing, Temperature::Frozen,    r(R::Gas, R::Fuel),         10,4, 5.2f,   false, false, false, 3},
        {"Saturn",        BodyType::GasGiant,     Atmosphere::Reducing, Temperature::Frozen,    r(R::Gas, R::Fuel),         9, 3, 9.5f,   false, false, false, 3},
        {"Uranus",        BodyType::IceGiant,     Atmosphere::Reducing, Temperature::Frozen,    r1(R::Gas),                 7, 2, 19.2f,  false, false, false, 2},
        {"Neptune",       BodyType::IceGiant,     Atmosphere::Reducing, Temperature::Frozen,    r1(R::Gas),                 7, 1, 30.0f,  false, false, false, 2},
        {"Pluto",         BodyType::DwarfPlanet,  Atmosphere::Thin,     Temperature::Frozen,    r(R::Water, R::Crystals),   1, 1, 39.5f,  true,  false, true,  1},
        {"Kuiper Belt",   BodyType::AsteroidBelt, Atmosphere::None,     Temperature::Frozen,    r(R::Metals, R::Crystals),  1, 0, 45.0f,  false, false, false, 2},
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

    std::mt19937 rng(sys.id ^ 0x504C4E54u);

    // Habitable zone bounds based on star class
    float hz_inner, hz_outer;
    switch (sys.star_class) {
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

} // namespace astra
