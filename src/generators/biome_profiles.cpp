#include "astra/biome_profile.h"

#include <array>
#include <utility>

namespace astra {

// Forward-declare elevation strategies (defined in elevation_strategies.cpp)
void elevation_gentle(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_rugged(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_flat(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);
void elevation_ridgeline(float* grid, int w, int h, std::mt19937& rng, const BiomeProfile& prof);

const BiomeProfile& biome_profile(Biome b) {
    // Natural biomes
    static const BiomeProfile grassland {
        "Grassland",
        elevation_gentle,
        0.02f, 3, 0.92f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile forest {
        "Forest",
        elevation_gentle,
        0.03f, 4, 0.88f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile jungle {
        "Jungle",
        elevation_gentle,
        0.035f, 4, 0.86f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile sandy {
        "Sandy",
        elevation_gentle,
        0.015f, 3, 0.95f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile rocky {
        "Rocky",
        elevation_rugged,
        0.04f, 5, 0.78f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile volcanic {
        "Volcanic",
        elevation_rugged,
        0.05f, 5, 0.75f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile aquatic {
        "Aquatic",
        elevation_flat,
        0.01f, 2, 0.98f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile ice {
        "Ice",
        elevation_flat,
        0.015f, 2, 0.96f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile fungal {
        "Fungal",
        elevation_gentle,
        0.025f, 4, 0.90f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile crystal {
        "Crystal",
        elevation_ridgeline,
        0.035f, 4, 0.82f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile corroded {
        "Corroded",
        elevation_rugged,
        0.04f, 4, 0.80f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };

    // Alien biomes
    static const BiomeProfile alien_crystalline {
        "AlienCrystalline",
        elevation_ridgeline,
        0.04f, 4, 0.80f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile alien_organic {
        "AlienOrganic",
        elevation_gentle,
        0.03f, 4, 0.88f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile alien_geometric {
        "AlienGeometric",
        elevation_flat,
        0.02f, 2, 0.96f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile alien_void {
        "AlienVoid",
        elevation_flat,
        0.02f, 3, 0.94f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile alien_light {
        "AlienLight",
        elevation_flat,
        0.015f, 2, 0.97f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };

    // Scar biomes
    static const BiomeProfile scarred_scorched {
        "ScarredScorched",
        elevation_rugged,
        0.045f, 4, 0.80f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };
    static const BiomeProfile scarred_glassed {
        "ScarredGlassed",
        elevation_flat,
        0.02f, 3, 0.94f,
        nullptr, 0.04f, 0.7f, 0.4f,
        nullptr, 0.5f,
        {}
    };

    // Station (fallback)
    static const BiomeProfile station {
        "Station",
        elevation_flat,
        0.02f, 2, 0.98f,
        nullptr, 0.04f, 0.7f, 0.4f,
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
    static const std::array<std::pair<const char*, Biome>, 20> table {{
        {"grassland",        Biome::Grassland},
        {"forest",           Biome::Forest},
        {"jungle",           Biome::Jungle},
        {"sandy",            Biome::Sandy},
        {"rocky",            Biome::Rocky},
        {"volcanic",         Biome::Volcanic},
        {"aquatic",          Biome::Aquatic},
        {"swamp",            Biome::Aquatic},
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
