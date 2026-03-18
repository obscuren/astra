#pragma once

#include "astra/celestial_body.h"
#include "astra/renderer.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace astra {

enum class StarClass : uint8_t {
    ClassM,  // Red dwarf (most common)
    ClassK,  // Orange
    ClassG,  // Yellow (Sol-like)
    ClassF,  // Yellow-white
    ClassA,  // White
    ClassB,  // Blue-white
    ClassO,  // Blue (rarest)
};

struct StationInfo {
    std::string name;
    bool derelict = false;
};

struct StarSystem {
    uint32_t id = 0;
    std::string name;
    StarClass star_class = StarClass::ClassM;
    bool binary = false;
    bool has_station = false;
    StationInfo station;
    int planet_count = 0;
    int asteroid_belts = 0;
    int danger_level = 1;
    float gx = 0.0f;
    float gy = 0.0f;
    bool discovered = false;
    std::vector<CelestialBody> bodies;
    bool bodies_generated = false;
};

struct NavigationData {
    uint32_t current_system_id = 0;
    int current_body_index = -1;  // index into current system's bodies (-1 = none)
    bool at_station = true;       // true if docked at the system's station
    std::vector<StarSystem> systems;
    int navi_range = 1;
};

// Generate the full galaxy from a seed
NavigationData generate_galaxy(unsigned game_seed);

// Fill in a system's properties from its seed and position
void generate_system(StarSystem& sys, uint32_t seed, float gx, float gy);

// Generate a sci-fi system name
std::string generate_system_name(std::mt19937& rng);

// Generate a station name
std::string generate_station_name(std::mt19937& rng);

// Generate celestial bodies for a system (lazy, idempotent)
void generate_system_bodies(StarSystem& sys);

// Display helpers
const char* star_class_name(StarClass sc);
char star_class_glyph(StarClass sc);
Color star_class_color(StarClass sc);

// Euclidean distance between two systems
float system_distance(const StarSystem& a, const StarSystem& b);

// Mark systems within radius of source as discovered
void discover_nearby(NavigationData& nav, uint32_t system_id, float radius);

} // namespace astra
