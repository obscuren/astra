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
