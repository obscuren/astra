#pragma once

#include "astra/celestial_body.h"
#include "astra/tilemap.h"

#include <cstdint>

namespace astra {

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

    // Detail map generation context (only used for MapType::DetailMap)
    Tile detail_terrain = Tile::OW_Plains;
    Tile detail_neighbor_n = Tile::Empty;
    Tile detail_neighbor_s = Tile::Empty;
    Tile detail_neighbor_e = Tile::Empty;
    Tile detail_neighbor_w = Tile::Empty;
    bool detail_has_poi = false;
    Tile detail_poi_type = Tile::Empty;
    bool detail_is_custom = false;  // hand-crafted zone (skip generator)
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
        case Tile::OW_Swamp:     return Biome::Aquatic;
        case Tile::OW_Fungal:    return Biome::Fungal;
        case Tile::OW_Mountains: return Biome::Rocky;
        case Tile::OW_Crater:    return Biome::Rocky;
        case Tile::OW_River:     return Biome::Aquatic;
        case Tile::OW_Lake:      return Biome::Aquatic;
        default:                 return planet_biome;
    }
}

} // namespace astra
