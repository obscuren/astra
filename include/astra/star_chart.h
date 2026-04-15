#pragma once

#include "astra/celestial_body.h"
#include "astra/lore_types.h"
#include "astra/renderer.h"
#include "astra/station_type.h"

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
    StationType type = StationType::NormalHub;
    StationSpecialty specialty = StationSpecialty::Generic;
    uint64_t keeper_seed = 0;
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
    int scar_count = 0;
    int primary_civ_index = -1;
    Architecture primary_civ_architecture = Architecture::Geometric;
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

struct CustomSystemSpec {
    std::string name;
    float gx = 0.0f;
    float gy = 0.0f;
    StarClass star_class = StarClass::ClassG;
    bool discovered = true;
    bool binary = false;
    bool has_station = false;
    LoreAnnotation lore = {};
    std::vector<CelestialBody> bodies;        // empty = lazy procedural
};

struct NavigationData {
    uint32_t current_system_id = 0;
    int current_body_index = -1;  // index into current system's bodies (-1 = none)
    int current_moon_index = -1;  // moon index within body (-1 = on body itself)
    bool at_station = true;       // true if docked at the system's station
    bool on_ship = false;         // true if aboard the player's starship
    std::vector<StarSystem> systems;
    int navi_range = 1;
    uint32_t next_custom_system_id = 0x80000000u;
};

// Create a custom system and append it to nav.systems. Returns the allocated id.
// IDs come from nav.next_custom_system_id (starts at 0x80000000); the counter
// survives save/load. If spec.bodies is non-empty, bodies are moved in and
// bodies_generated is set to true. Empty spec.bodies leaves bodies_generated
// false so the lazy generator runs on first access.
uint32_t add_custom_system(NavigationData& nav, CustomSystemSpec spec);

// Set discovered=true for the system with this id. Returns false if not found.
bool reveal_system(NavigationData& nav, uint32_t system_id);

// Symmetry helper; sets discovered=false.
bool hide_system(NavigationData& nav, uint32_t system_id);

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
