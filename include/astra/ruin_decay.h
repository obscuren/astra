#pragma once

#include "astra/tilemap.h"
#include "astra/settlement_types.h"

#include <random>

namespace astra {

// Ruin wall tinting flag -- bit 1 of custom_flags_
static constexpr uint8_t CF_RUIN_TINT = 0x02;

struct DecayContext {
    float age_decay = 0.5f;       // wall removal probability (extra, beyond CivStyle)
    bool battle_scarred = false;  // future
    int blast_direction = -1;     // future
    bool seismic = false;         // future

    // Gradient decay: edges decay more, interior preserved
    bool use_gradient = false;
    Rect gradient_footprint;       // used to compute distance-from-edge

    // Sectoral variance: per-sector random multiplier
    bool use_sectoral = false;
    float sectoral_variance = 0.3f;  // max +/- deviation from gradient
};

class RuinDecay {
public:
    void apply(TileMap& map, const Rect& footprint,
               const DecayContext& ctx, Biome biome,
               std::mt19937& rng) const;
};

} // namespace astra
