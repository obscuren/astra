#pragma once

#include <vector>

namespace astra {

class TileMap;
class VisibilityMap;

// Compute field of view using recursive shadowcasting.
// Clears current visibility, then marks tiles visible from (origin_x, origin_y)
// within the given radius. Walls block line of sight.
void compute_fov(const TileMap& map, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius);

// A light source that extends the player's FOV in its direction.
struct LightSource {
    int x, y;
    int radius;
};

// Extend FOV from player toward light sources. For each light, runs
// shadowcasting from the player with extended radius, but only marks
// tiles visible if they're within light_radius of the light source.
// Does not clear existing visibility (additive).
void compute_fov_lit(const TileMap& map, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights);

} // namespace astra
