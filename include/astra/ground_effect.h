#pragma once

#include <cstdint>
#include <unordered_set>

namespace astra {

class Game;

enum class GroundEffectKind : uint8_t {
    Smoke = 0,
    // Reserved: Acid, Ice, Fire — adding a kind requires only an enum value,
    // a row in the GroundEffectDef table, optional per-step gameplay hook,
    // and a render entry. No save schema change.
};

struct GroundEffect {
    GroundEffectKind kind = GroundEffectKind::Smoke;
    int x = 0;
    int y = 0;
    int ttl = 0;            // ticks remaining; entry erased when ttl <= 0
    uint16_t origin_id = 0; // optional grouping: which detonation produced this
};

struct GroundEffectDef {
    int  radius;          // Chebyshev half-width (5×5 = 2)
    int  center_ttl;      // initial TTL at impact tile
    int  ring_falloff;    // TTL subtracted per Chebyshev ring
    bool blocks_vision;   // smoke=true; future ice=false
};

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k);

// Stamp a new ground-effect patch at (x, y). Walks (2*radius+1)² square,
// skips tiles whose Bresenham wall-LOS from impact is blocked. Per-tile
// TTL = max(1, center_ttl − ring * ring_falloff). On overlap with existing
// entry of same kind on the tile, TTL = max(new, existing).
void stamp_ground_effect(Game& game, GroundEffectKind kind, int x, int y);

// Run once per world tick: TTL--, erase expired.
void tick_ground_effects(Game& game);

// Build packed (x,y) hash-set of tiles whose active ground effect has
// blocks_vision=true. Used by FOV. Key = (uint64_t(uint32_t(x)) << 32) | uint32_t(y).
std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game& game);

} // namespace astra
