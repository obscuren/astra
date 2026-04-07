#pragma once

#include "astra/biome_profile.h"
#include <vector>

namespace astra {

struct TerrainChannels {
    int width = 0;
    int height = 0;
    std::vector<float> elevation;
    std::vector<float> moisture;
    std::vector<StructureMask> structure;

    TerrainChannels() = default;

    TerrainChannels(int w, int h)
        : width(w), height(h),
          elevation(w * h, 0.0f),
          moisture(w * h, 0.0f),
          structure(w * h, StructureMask::None) {}

    float  elev(int x, int y) const { return elevation[y * width + x]; }
    float& elev(int x, int y)       { return elevation[y * width + x]; }

    float  moist(int x, int y) const { return moisture[y * width + x]; }
    float& moist(int x, int y)       { return moisture[y * width + x]; }

    StructureMask  struc(int x, int y) const { return structure[y * width + x]; }
    StructureMask& struc(int x, int y)       { return structure[y * width + x]; }
};

} // namespace astra
