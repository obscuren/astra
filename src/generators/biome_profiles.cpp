#include "astra/biome_profile.h"

#include <array>
#include <utility>

namespace astra {

// Forward-declare elevation strategies (defined in elevation_strategies.cpp)
void elevation_gentle(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_rugged(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_flat(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_ridgeline(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);

// Forward-declare moisture strategies (defined in moisture_strategies.cpp)
void moisture_none(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);
void moisture_pools(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);
void moisture_river(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);
void moisture_coastline(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);
void moisture_channels(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);
void moisture_marsh(float* grid, int w, int h, std::mt19937& rng, const float* elevation, const BiomeProfile& prof);

// Forward-declare structure strategies (defined in structure_strategies.cpp)
void structure_none(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
void structure_cliffs(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
void structure_islands(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
void structure_formations(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
void structure_craters(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);
void structure_mountains(StructureMask* grid, int w, int h, std::mt19937& rng, const float* elevation, const float* moisture, const BiomeProfile& prof);

const BiomeProfile& biome_profile(Biome b) {
    // Natural biomes
    static const BiomeProfile grassland {
        "Grassland",
        elevation_gentle,
        0.02f, 3, 0.92f,
        moisture_river, 0.03f, 0.85f, 0.4f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.03f, false}},
        flora_grassland
    };
    static const BiomeProfile forest {
        "Forest",
        elevation_gentle,
        0.03f, 4, 0.88f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.25f, true}},
        flora_forest
    };
    static const BiomeProfile jungle {
        "Jungle",
        elevation_gentle,
        0.035f, 4, 0.86f,
        moisture_river, 0.03f, 0.7f, 0.5f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.35f, true}},
        flora_jungle
    };
    static const BiomeProfile sandy {
        "Sandy",
        elevation_gentle,
        0.015f, 3, 0.95f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.02f, false}},
        flora_sandy
    };
    static const BiomeProfile rocky {
        "Rocky",
        elevation_rugged,
        0.04f, 5, 0.78f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_formations, 0.4f,
        {{FixtureType::NaturalObstacle, 0.05f, false}},
        flora_rocky
    };
    static const BiomeProfile volcanic {
        "Volcanic",
        elevation_rugged,
        0.05f, 5, 0.75f,
        moisture_channels, 0.03f, 0.6f, 0.6f,
        structure_craters, 0.5f,
        {{FixtureType::NaturalObstacle, 0.04f, false}},
        flora_volcanic
    };
    static const BiomeProfile aquatic {
        "Aquatic",
        elevation_flat,
        0.01f, 2, 0.98f,
        moisture_none, 0.04f, 0.7f, 0.4f,  // parked: needs neighbor-aware redesign in Phase 7
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.02f, false}},
        flora_aquatic
    };
    static const BiomeProfile ice {
        "Ice",
        elevation_flat,
        0.015f, 2, 0.96f,
        moisture_none, 0.035f, 0.7f, 0.45f,
        structure_formations, 0.4f,
        {{FixtureType::NaturalObstacle, 0.03f, false}},
        flora_ice
    };
    static const BiomeProfile fungal {
        "Fungal",
        elevation_gentle,
        0.025f, 4, 0.90f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.20f, true}},
        flora_fungal
    };
    static const BiomeProfile crystal {
        "Crystal",
        elevation_ridgeline,
        0.035f, 4, 0.82f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_formations, 0.5f,
        {{FixtureType::NaturalObstacle, 0.06f, false}},
        flora_crystal
    };
    static const BiomeProfile corroded {
        "Corroded",
        elevation_rugged,
        0.04f, 4, 0.80f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.3f,
        {{FixtureType::NaturalObstacle, 0.04f, false}},
        flora_corroded
    };

    static const BiomeProfile marsh {
        "Marsh",
        elevation_flat,
        0.02f, 2, 0.97f,
        moisture_marsh, 0.025f, 0.48f, 0.8f,
        structure_islands, 0.5f,
        {{FixtureType::NaturalObstacle, 0.08f, false}},
        flora_marsh
    };

    static const BiomeProfile mountains {
        "Mountains",
        elevation_ridgeline,
        0.04f, 5, 0.92f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_mountains, 0.6f,
        {{FixtureType::NaturalObstacle, 0.02f, false},
         {FixtureType::MineralOre, 0.01f, false},
         {FixtureType::MineralCrystal, 0.005f, false}},
        nullptr
    };

    // Alien biomes
    static const BiomeProfile alien_crystalline {
        "AlienCrystalline",
        elevation_ridgeline,
        0.04f, 4, 0.80f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_formations, 0.6f,
        {{FixtureType::NaturalObstacle, 0.06f, false}},
        flora_alien
    };
    static const BiomeProfile alien_organic {
        "AlienOrganic",
        elevation_gentle,
        0.03f, 4, 0.88f,
        moisture_pools, 0.045f, 0.65f, 0.5f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.20f, true}},
        flora_alien
    };
    static const BiomeProfile alien_geometric {
        "AlienGeometric",
        elevation_flat,
        0.02f, 2, 0.96f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_formations, 0.7f,
        {{FixtureType::NaturalObstacle, 0.03f, false}},
        flora_alien
    };
    static const BiomeProfile alien_void {
        "AlienVoid",
        elevation_flat,
        0.02f, 3, 0.94f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.4f,
        {{FixtureType::NaturalObstacle, 0.02f, false}},
        flora_alien
    };
    static const BiomeProfile alien_light {
        "AlienLight",
        elevation_flat,
        0.015f, 2, 0.97f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.0f,
        {{FixtureType::NaturalObstacle, 0.04f, false}},
        flora_alien
    };

    // Scar biomes
    static const BiomeProfile scarred_scorched {
        "ScarredScorched",
        elevation_rugged,
        0.045f, 4, 0.80f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_none, 0.3f,
        {{FixtureType::NaturalObstacle, 0.06f, false}},
        flora_scarred
    };
    static const BiomeProfile scarred_glassed {
        "ScarredGlassed",
        elevation_flat,
        0.02f, 3, 0.94f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        structure_formations, 0.6f,
        {{FixtureType::NaturalObstacle, 0.03f, false}},
        flora_scarred
    };

    // Station (fallback)
    static const BiomeProfile station {
        "Station",
        elevation_flat,
        0.02f, 2, 0.98f,
        moisture_none, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };

    switch (b) {
        case Biome::Grassland:         return grassland;
        case Biome::Forest:            return forest;
        case Biome::Jungle:            return jungle;
        case Biome::Sandy:             return sandy;
        case Biome::Rocky:             return rocky;
        case Biome::Volcanic:          return volcanic;
        case Biome::Aquatic:           return aquatic;
        case Biome::Ice:               return ice;
        case Biome::Fungal:            return fungal;
        case Biome::Crystal:           return crystal;
        case Biome::Corroded:          return corroded;
        case Biome::Marsh:             return marsh;
        case Biome::Mountains:         return mountains;
        case Biome::AlienCrystalline:  return alien_crystalline;
        case Biome::AlienOrganic:      return alien_organic;
        case Biome::AlienGeometric:    return alien_geometric;
        case Biome::AlienVoid:         return alien_void;
        case Biome::AlienLight:        return alien_light;
        case Biome::ScarredScorched:   return scarred_scorched;
        case Biome::ScarredGlassed:    return scarred_glassed;
        case Biome::Station:           return station;
    }
    return station; // unreachable fallback
}

bool parse_biome(const std::string& name, Biome& out) {
    static const std::array<std::pair<const char*, Biome>, 23> table {{
        {"grassland",        Biome::Grassland},
        {"forest",           Biome::Forest},
        {"jungle",           Biome::Jungle},
        {"sandy",            Biome::Sandy},
        {"rocky",            Biome::Rocky},
        {"volcanic",         Biome::Volcanic},
        {"aquatic",          Biome::Aquatic},
        {"swamp",            Biome::Marsh},
        {"marsh",            Biome::Marsh},
        {"ice",              Biome::Ice},
        {"fungal",           Biome::Fungal},
        {"crystal",          Biome::Crystal},
        {"corroded",         Biome::Corroded},
        {"alien_crystalline", Biome::AlienCrystalline},
        {"alien_organic",    Biome::AlienOrganic},
        {"alien_geometric",  Biome::AlienGeometric},
        {"alien_void",       Biome::AlienVoid},
        {"alien_light",      Biome::AlienLight},
        {"scarred_scorched", Biome::ScarredScorched},
        {"scarred_glassed",  Biome::ScarredGlassed},
        {"mountains",        Biome::Mountains},
        {"station",          Biome::Station},
    }};

    for (const auto& [key, biome] : table) {
        if (name == key) {
            out = biome;
            return true;
        }
    }
    return false;
}

} // namespace astra
