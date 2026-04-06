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

// ── Lore annotation for a star system ──
struct LoreAnnotation {
    int lore_tier = 0;                  // 0=mundane, 1=touched, 2=significant, 3=pivotal
    std::vector<int> ruin_civ_ids;      // which civilizations left ruins here (indices into WorldLore)
    std::string primary_civ_name;       // dominant civilization's short name
    bool has_megastructure = false;
    bool beacon = false;                // Sgr A* beacon node
    bool battle_site = false;
    bool weapon_test_site = false;
    bool plague_origin = false;
    bool terraformed = false;
    int terraformed_by_civ = -1;        // civ index
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
    LoreAnnotation lore;                // populated by apply_lore_to_galaxy()
};

struct NavigationData {
    uint32_t current_system_id = 0;
    int current_body_index = -1;  // index into current system's bodies (-1 = none)
    int current_moon_index = -1;  // moon index within body (-1 = on body itself)
    bool at_station = true;       // true if docked at the system's station
    bool on_ship = false;         // true if aboard the player's starship
    std::vector<StarSystem> systems;
    int navi_range = 1;
};

// Generate the full galaxy from a seed
NavigationData generate_galaxy(unsigned game_seed);

// Map simulation lore data onto real star systems.
// Each LoreSystemData is matched to the nearest real system by position.
struct WorldLore; // forward declaration
void apply_lore_to_galaxy(NavigationData& nav, const WorldLore& lore);

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
