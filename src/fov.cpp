#include "astra/fov.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

#include <cmath>

namespace astra {

bool OpacityProbe::opaque(int x, int y) const {
    if (map && map->opaque(x, y)) return true;
    if (extra_opaque) {
        uint64_t key = (uint64_t(uint32_t(x)) << 32) | uint32_t(y);
        if (extra_opaque->count(key)) return true;
    }
    return false;
}

// Multipliers for transforming coordinates in each octant.
static constexpr int octant_transform[8][4] = {
    { 1,  0,  0,  1},
    { 0,  1,  1,  0},
    { 0, -1,  1,  0},
    {-1,  0,  0,  1},
    {-1,  0,  0, -1},
    { 0, -1, -1,  0},
    { 0,  1, -1,  0},
    { 1,  0,  0, -1},
};

static void cast_light(const OpacityProbe& probe, VisibilityMap& vis,
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

            if (dx * dx + dy * dy <= radius_sq) {
                vis.set_visible(map_x, map_y);
            }

            if (blocked) {
                if (probe.opaque(map_x, map_y)) {
                    new_start = r_slope;
                } else {
                    blocked = false;
                    start_slope = new_start;
                }
            } else {
                if (probe.opaque(map_x, map_y) && j < radius) {
                    blocked = true;
                    cast_light(probe, vis, ox, oy, radius,
                               j + 1, start_slope, l_slope,
                               xx, xy, yx, yy);
                    new_start = r_slope;
                }
            }
        }

        if (blocked) break;
    }
}

void compute_fov(const OpacityProbe& probe, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius) {
    vis.clear_visible();
    vis.set_visible(origin_x, origin_y);

    for (int oct = 0; oct < 8; ++oct) {
        cast_light(probe, vis, origin_x, origin_y, radius,
                   1, 1.0f, 0.0f,
                   octant_transform[oct][0], octant_transform[oct][1],
                   octant_transform[oct][2], octant_transform[oct][3]);
    }
}

static void cast_light_lit(const OpacityProbe& probe, VisibilityMap& vis,
                            int ox, int oy, int radius,
                            int row, float start_slope, float end_slope,
                            int xx, int xy, int yx, int yy,
                            const std::vector<LightSource>& lights) {
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

            if (dx * dx + dy * dy <= radius_sq) {
                for (const auto& ls : lights) {
                    int ldx = map_x - ls.x, ldy = map_y - ls.y;
                    if (ldx * ldx + ldy * ldy <= ls.radius * ls.radius) {
                        vis.set_visible(map_x, map_y);
                        break;
                    }
                }
            }

            if (blocked) {
                if (probe.opaque(map_x, map_y)) {
                    new_start = r_slope;
                } else {
                    blocked = false;
                    start_slope = new_start;
                }
            } else {
                if (probe.opaque(map_x, map_y) && j < radius) {
                    blocked = true;
                    cast_light_lit(probe, vis, ox, oy, radius,
                                    j + 1, start_slope, l_slope,
                                    xx, xy, yx, yy, lights);
                    new_start = r_slope;
                }
            }
        }

        if (blocked) break;
    }
}

void compute_fov_lit(const OpacityProbe& probe, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights) {
    int max_radius = 0;
    for (const auto& ls : lights) {
        int dx = ls.x - player_x, dy = ls.y - player_y;
        int dist = static_cast<int>(std::sqrt(dx * dx + dy * dy)) + 1;
        int extended = dist + ls.radius;
        if (extended > max_radius) max_radius = extended;
    }

    for (int oct = 0; oct < 8; ++oct) {
        cast_light_lit(probe, vis, player_x, player_y, max_radius,
                       1, 1.0f, 0.0f,
                       octant_transform[oct][0], octant_transform[oct][1],
                       octant_transform[oct][2], octant_transform[oct][3],
                       lights);
    }
}

} // namespace astra
