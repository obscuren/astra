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
};

MapProperties default_properties(MapType type);

} // namespace astra
