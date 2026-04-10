#pragma once

#include "astra/celestial_body.h"
#include "astra/edge_strip.h"
#include "astra/lore_types.h"
#include "astra/tilemap.h"

#include <cstdint>
#include <optional>
#include <string>

namespace astra {

struct LoreInfluenceMap;  // forward declaration

enum class Environment : uint8_t {
    Station,
    Derelict,
    Cave,
    Surface,
    Temple,
    Facility,
};

enum class Climate : uint8_t {
    Temperate,
    Volcanic,
    Frozen,
    Irradiated,
    Vacuum,
};

struct MapProperties {
    Environment environment = Environment::Station;
    Climate climate = Climate::Temperate;
    Biome biome = Biome::Station;
    int difficulty = 1;       // 1-10, drives NPC levels & density
    int loot_tier = 1;        // 1-5, controls item quality ceiling
    int light_bias = 60;      // 0-100, % of rooms that start lit
    bool has_backdrop = false; // starfield/nebula backdrop
    int room_count_min = 5;
    int room_count_max = 12;
    int width = 120;
    int height = 60;

    // Overworld generation context (only used for MapType::Overworld)
    BodyType body_type = BodyType::Rocky;
    Atmosphere body_atmosphere = Atmosphere::None;
    Temperature body_temperature = Temperature::Cold;
    bool body_has_dungeon = false;
    int body_danger_level = 1;

    // Lore-driven overworld context (populated from LoreAnnotation)
    int lore_tier = 0;              // 0-3: affects POI density
    bool lore_battle_site = false;  // extra crashed ships, debris
    bool lore_weapon_test = false;  // scarred terrain, craters
    bool lore_megastructure = false;// orbital structure POI
    bool lore_beacon = false;       // Sgr A* beacon POI
    bool lore_terraformed = false;  // altered biome
    bool lore_plague_origin = false;// abandoned settlements
    int lore_scar_count = 0;
    Architecture lore_civ_architecture = Architecture::Geometric;
    int lore_primary_civ_index = -1;

    // Lore influence map (set before overworld generation, nullptr if no lore)
    const LoreInfluenceMap* lore_influence = nullptr;

    // Detail map lore context (populated from influence map at overworld cell)
    float lore_alien_strength = 0.0f;
    Architecture lore_alien_architecture = Architecture::Geometric;
    float lore_scar_intensity = 0.0f;

    // Zone position within the 3x3 grid (for shared edge seeding)
    int zone_x = 1;
    int zone_y = 1;
    int overworld_x = 0;
    int overworld_y = 0;

    // Detail map generation context (only used for MapType::DetailMap)
    Tile detail_terrain = Tile::OW_Plains;
    Tile detail_neighbor_n = Tile::Empty;
    Tile detail_neighbor_s = Tile::Empty;
    Tile detail_neighbor_e = Tile::Empty;
    Tile detail_neighbor_w = Tile::Empty;

    // Cached neighbor edge strips for seamless detail map transitions
    std::optional<EdgeStrip> edge_strip_n;
    std::optional<EdgeStrip> edge_strip_s;
    std::optional<EdgeStrip> edge_strip_e;
    std::optional<EdgeStrip> edge_strip_w;

    bool detail_has_poi = false;
    Tile detail_poi_type = Tile::Empty;
    bool detail_is_custom = false;  // hand-crafted zone (skip generator)
    std::string detail_ruin_civ;   // dev-mode: force specific ruin civilization
    float detail_ruin_decay = -1.0f;  // dev-mode: 0=pristine, 1=max decay, -1=random
};

MapProperties default_properties(MapType type);

// Map an overworld terrain tile to the appropriate detail map biome
inline Biome detail_biome_for_terrain(Tile terrain, Biome planet_biome) {
    switch (terrain) {
        case Tile::OW_Forest:    return Biome::Forest;
        case Tile::OW_Plains:    return Biome::Grassland;
        case Tile::OW_Desert:    return Biome::Sandy;
        case Tile::OW_IceField:  return Biome::Ice;
        case Tile::OW_LavaFlow:  return Biome::Volcanic;
        case Tile::OW_Swamp:     return Biome::Marsh;
        case Tile::OW_Barren:    return Biome::Rocky;
        case Tile::OW_Fungal:    return Biome::Fungal;
        case Tile::OW_Mountains: return Biome::Mountains;
        case Tile::OW_Crater:    return Biome::Rocky;
        case Tile::OW_River:     return Biome::Aquatic;
        case Tile::OW_Lake:      return Biome::Aquatic;
        case Tile::OW_Beacon:    return Biome::Rocky;
        case Tile::OW_Megastructure: return Biome::Rocky;
        case Tile::OW_AlienTerrain: return Biome::Grassland;
        case Tile::OW_ScorchedEarth: return Biome::Sandy;
        case Tile::OW_GlassedCrater: return Biome::Rocky;
        default:                 return planet_biome;
    }
}

} // namespace astra
