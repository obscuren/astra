#pragma once

#include "astra/tilemap.h"

#include <cstdint>
#include <vector>

namespace astra {

enum class CaveVariant : uint8_t {
    None,               // fail-gracefully sentinel
    NaturalCave,
    AbandonedMine,
    AncientExcavation,
};

enum class CaveFacing : uint8_t { North, South, East, West };

// Result of scanning a placement footprint for a cliff edge.
struct CliffHit {
    int wall_x;
    int wall_y;
    int floor_x;
    int floor_y;
    CaveFacing mouth_facing;
};

struct CaveVariantSpec {
    CaveVariant variant;
    const char* name;

    int foot_w;
    int foot_h;
    bool requires_cliff;

    int debris_min;
    int debris_max;

    // Extra fixtures beyond debris (placed inside the entrance interior).
    std::vector<FixtureType> fixtures;
};

} // namespace astra
