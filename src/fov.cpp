#include "astra/fov.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

namespace astra {

// Multipliers for transforming coordinates in each octant.
// Each octant is defined by 4 values: xx, xy, yx, yy
// which transform (row, col) -> (dx, dy) as:
//   dx = col * xx + row * xy
//   dy = col * yx + row * yy
static constexpr int octant_transform[8][4] = {
    { 1,  0,  0,  1},  // octant 0
    { 0,  1,  1,  0},  // octant 1
    { 0, -1,  1,  0},  // octant 2
    {-1,  0,  0,  1},  // octant 3
    {-1,  0,  0, -1},  // octant 4
    { 0, -1, -1,  0},  // octant 5
    { 0,  1, -1,  0},  // octant 6
    { 1,  0,  0, -1},  // octant 7
};

static void cast_light(const TileMap& map, VisibilityMap& vis,
                        int ox, int oy, int radius,
                        int row, float start_slope, float end_slope,
                        int xx, int xy, int yx, int yy) {
    if (start_slope < end_slope) return;

    int radius_sq = radius * radius;

    for (int j = row; j <= radius; ++j) {
        int dx = -j - 1;
        int dy = -j;
        bool blocked = false;
        float new_start = start_slope;

        for (dx = dx + 1; dx <= 0; ++dx) {
            int map_x = ox + dx * xx + dy * xy;
            int map_y = oy + dx * yx + dy * yy;

            float l_slope = (dx - 0.5f) / (dy + 0.5f);
            float r_slope = (dx + 0.5f) / (dy - 0.5f);

            if (start_slope < r_slope) continue;
            if (end_slope > l_slope) break;

            // Check if within circular radius
            if (dx * dx + dy * dy <= radius_sq) {
                vis.set_visible(map_x, map_y);
            }

            if (blocked) {
                if (map.opaque(map_x, map_y)) {
                    new_start = r_slope;
                } else {
                    blocked = false;
                    start_slope = new_start;
                }
            } else {
                if (map.opaque(map_x, map_y) && j < radius) {
                    blocked = true;
                    cast_light(map, vis, ox, oy, radius,
                               j + 1, start_slope, l_slope,
                               xx, xy, yx, yy);
                    new_start = r_slope;
                }
            }
        }

        if (blocked) break;
    }
}

void compute_fov(const TileMap& map, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius) {
    vis.clear_visible();

    // Origin is always visible
    vis.set_visible(origin_x, origin_y);

    for (int oct = 0; oct < 8; ++oct) {
        cast_light(map, vis, origin_x, origin_y, radius,
                   1, 1.0f, 0.0f,
                   octant_transform[oct][0], octant_transform[oct][1],
                   octant_transform[oct][2], octant_transform[oct][3]);
    }
}

} // namespace astra
