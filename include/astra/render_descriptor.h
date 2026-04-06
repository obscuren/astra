#pragma once

#include <cstdint>
#include "astra/tilemap.h"  // Biome
#include "astra/item.h"     // Rarity

namespace astra {

enum class RenderCategory : uint8_t {
    Tile,
    Fixture,
    Npc,
    Item,
    Effect,
    Player,
};

enum RenderFlag : uint8_t {
    RF_None         = 0,
    RF_Remembered   = 1 << 0,
    RF_Hostile      = 1 << 1,
    RF_Damaged      = 1 << 2,
    RF_Lit          = 1 << 3,
    RF_Interactable = 1 << 4,
    RF_Equipped     = 1 << 5,
    RF_Starfield    = 1 << 6,  // tile is in a starfield zone (Station Empty tiles)
    RF_Open         = 1 << 7,  // door is open
};

struct RenderDescriptor {
    RenderCategory category = RenderCategory::Tile;
    uint16_t type_id  = 0;
    uint8_t  seed     = 0;
    uint8_t  flags    = RF_None;
    Biome    biome    = Biome::Station;
    Rarity   rarity   = Rarity::Common;
};

enum class AnimationType : uint8_t {
    ConsoleBlink,
    WaterShimmer,
    ViewportShimmer,
    TorchFlicker,
    DamageFlash,
    HealPulse,
    Projectile,
    LevelUp,
    AlienPulse, ScarSmolder, BeaconGlow, MegastructureShift,
};

// Deterministic position hash for visual variation.
// Same (x,y) always produces the same seed.
inline uint8_t position_seed(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    return static_cast<uint8_t>(h >> 24);
}

// Encode structural wall material in top 2 bits of seed, variation in bottom 6.
inline uint8_t encode_wall_seed(uint8_t material, int x, int y) {
    uint8_t base = position_seed(x, y) & 0x3F;
    return static_cast<uint8_t>((material << 6) | base);
}

inline uint8_t decode_wall_material(uint8_t seed) {
    return seed >> 6;
}

} // namespace astra
