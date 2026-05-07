#include "astra/anchor.h"

#include <algorithm>

#include "astra/grid_sector.h"
#include "astra/world_manager.h"

namespace astra {

AnchorProjection make_anchor_projection(const GridSector& sec,
                                        const WorldManager& world) {
    AnchorProjection p;
    p.site_w = sec.w;
    p.site_h = sec.h;

    // RW extent: take the active map's dimensions. If accessible.
    // Fall back to sane defaults if we can't introspect.
    const auto& map = world.map();
    p.rw_origin_x = 0;
    p.rw_origin_y = 0;
    p.rw_extent_x = map.width()  > 0 ? map.width()  : 60;
    p.rw_extent_y = map.height() > 0 ? map.height() : 50;

    return p;
}

void project_rw_to_site(const AnchorProjection& p, int rwx, int rwy,
                        int& sx, int& sy) {
    int dx = rwx - p.rw_origin_x;
    int dy = rwy - p.rw_origin_y;
    int extent_x = std::max(1, p.rw_extent_x);
    int extent_y = std::max(1, p.rw_extent_y);
    sx = (dx * p.site_w) / extent_x;
    sy = (dy * p.site_h) / extent_y;
    sx = std::clamp(sx, 0, std::max(0, p.site_w - 1));
    sy = std::clamp(sy, 0, std::max(0, p.site_h - 1));
}

bool nudge_to_passable(const GridSector& sector, int& sx, int& sy,
                       int max_radius) {
    if (sector.passable(sx, sy)) return true;

    int best_dx = 0, best_dy = 0;
    int best_d = 1 << 30;
    for (int rad = 1; rad <= max_radius && best_d == (1 << 30); ++rad) {
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != rad) continue;
                int tx = sx + dx, ty = sy + dy;
                if (!sector.passable(tx, ty)) continue;
                int d = std::abs(dx) + std::abs(dy);
                if (d < best_d) {
                    best_d = d;
                    best_dx = dx;
                    best_dy = dy;
                }
            }
        }
    }
    if (best_d == (1 << 30)) return false;
    sx += best_dx;
    sy += best_dy;
    return true;
}

}  // namespace astra
