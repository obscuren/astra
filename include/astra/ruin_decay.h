#pragma once

#include "astra/tilemap.h"
#include "astra/settlement_types.h"
#include "astra/ruin_types.h"

#include <cstdint>
#include <random>

namespace astra {

// Ruin wall tinting flag -- bit 1 of custom_flags_
static constexpr uint8_t CF_RUIN_TINT = 0x02;

// Civilization index in bits 2-4 of custom_flags_ (0-7, shifted left by 2)
static constexpr uint8_t CF_CIV_SHIFT = 2;
static constexpr uint8_t CF_CIV_MASK  = 0x1C;  // bits 2-4

// Sanctum flag -- bit 5 of custom_flags_. Applied to walls + floors within
// a dungeon's sanctum chamber so the renderer can paint them with gold-
// crystal "ancient but preserved" visuals instead of the generic dungeon
// palette.
static constexpr uint8_t CF_SANCTUM = 0x20;

inline void set_ruin_civ(TileMap& map, int x, int y, int civ_index) {
    uint8_t flags = map.get_custom_flags(x, y);
    flags = (flags & ~CF_CIV_MASK) | (static_cast<uint8_t>(civ_index & 0x07) << CF_CIV_SHIFT);
    map.set_custom_flags_byte(x, y, flags);
}

inline int get_ruin_civ(uint8_t flags) {
    return (flags & CF_CIV_MASK) >> CF_CIV_SHIFT;
}

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

// Apply ruin decay (stain tiles, crumble walls) in-place.
// Intensity is 0.0..1.0; 0 = pristine, 1 = heavy damage.
// Safe to call on any tile grid — stays inside passable/wall cells.
void apply_decay(TileMap& map, const CivConfig& civ,
                 float intensity, std::mt19937& rng);

} // namespace astra
