#pragma once

namespace astra {

class TileMap;
class VisibilityMap;

// Compute field of view using recursive shadowcasting.
// Clears current visibility, then marks tiles visible from (origin_x, origin_y)
// within the given radius. Walls block line of sight.
void compute_fov(const TileMap& map, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius);

} // namespace astra
