#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace astra {

class TileMap;
class VisibilityMap;

// Pluggable opacity check for FOV. Holds a TileMap pointer (terrain + fixtures)
// and an optional set of "extra opaque" tiles (e.g., active smoke clouds).
// Built once per FOV call; passed by const ref into shadowcasting.
//
// Key encoding for extra_opaque: (uint64_t(uint32_t(x)) << 32) | uint32_t(y).
struct OpacityProbe {
    const TileMap* map = nullptr;
    const std::unordered_set<uint64_t>* extra_opaque = nullptr;

    bool opaque(int x, int y) const;
};

// Compute field of view using recursive shadowcasting.
// Clears current visibility, then marks tiles visible from (origin_x, origin_y)
// within the given radius. Walls (and any tile flagged opaque by the probe)
// block line of sight.
void compute_fov(const OpacityProbe& probe, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius);

// A light source that extends the player's FOV in its direction.
struct LightSource {
    int x, y;
    int radius;
};

// Extend FOV from player toward light sources (additive — does not clear).
void compute_fov_lit(const OpacityProbe& probe, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights);

} // namespace astra
