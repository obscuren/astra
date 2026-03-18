#pragma once

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
    int difficulty = 1;       // 1-10, drives NPC levels & density
    int loot_tier = 1;        // 1-5, controls item quality ceiling
    int light_bias = 60;      // 0-100, % of rooms that start lit
    bool has_backdrop = false; // starfield/nebula backdrop
    int room_count_min = 5;
    int room_count_max = 12;
    int width = 120;
    int height = 60;
};

MapProperties default_properties(MapType type);

} // namespace astra
