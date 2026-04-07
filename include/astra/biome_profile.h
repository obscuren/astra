#pragma once

#include "astra/tilemap.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace astra {

// Forward-declare so strategy aliases can reference it.
struct BiomeProfile;

// --- Strategy type aliases ---

using ElevationStrategy = void(*)(float* grid, int w, int h,
                                  std::mt19937& rng,
                                  const BiomeProfile& prof);

using MoistureStrategy  = void(*)(float* grid, int w, int h,
                                  std::mt19937& rng,
                                  const float* elevation,
                                  const BiomeProfile& prof);

enum class StructureMask : uint8_t { None, Wall, Floor, Water };

using StructureStrategy = void(*)(StructureMask* grid, int w, int h,
                                  std::mt19937& rng,
                                  const float* elevation,
                                  const float* moisture,
                                  const BiomeProfile& prof);

// --- Scatter configuration ---

struct ScatterEntry {
    FixtureType type = FixtureType::NaturalObstacle;
    float density = 0.0f;
    bool blocks_vision = false;
};

// --- Biome profile ---

struct BiomeProfile {
    std::string name;

    // Layer 1: Elevation
    ElevationStrategy elevation_fn = nullptr;
    float elevation_frequency = 0.03f;
    int   elevation_octaves   = 4;
    float wall_threshold      = 0.85f;

    // Layer 2: Moisture (Phase 2)
    MoistureStrategy moisture_fn = nullptr;
    float moisture_frequency = 0.04f;
    float water_threshold    = 0.7f;
    float flood_level        = 0.4f;

    // Layer 3: Structure (Phase 3)
    StructureStrategy structure_fn = nullptr;
    float structure_intensity = 0.5f;

    // Layer 4: Scatter (Phase 4)
    std::vector<ScatterEntry> scatter;
};

// --- Lookup functions ---

const BiomeProfile& biome_profile(Biome b);
bool parse_biome(const std::string& name, Biome& out);

} // namespace astra
