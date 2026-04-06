#pragma once

#include "astra/lore_types.h"
#include "astra/map_properties.h"
#include "astra/world_constants.h"

#include <cstdint>
#include <vector>

namespace astra {

enum class LandmarkType : uint8_t {
    None = 0,
    Beacon,
    Megastructure,
};

struct LoreInfluenceMap {
    int width  = 0;
    int height = 0;
    std::vector<float> alien_strength;   // 0.0-1.0
    std::vector<float> scar_intensity;   // 0.0-1.0
    std::vector<LandmarkType> landmark;  // per-cell

    bool empty() const { return width == 0; }

    float alien_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return alien_strength[y * width + x];
    }
    float scar_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return scar_intensity[y * width + x];
    }
    LandmarkType landmark_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return LandmarkType::None;
        return landmark[y * width + x];
    }
};

// Generate a lore influence map for an overworld surface.
// Returns an empty map if no lore features apply.
LoreInfluenceMap generate_lore_influence(
    const MapProperties& props,
    int map_width, int map_height,
    unsigned seed);

}  // namespace astra
