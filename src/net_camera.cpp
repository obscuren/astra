#include "astra/net_camera.h"

#include <algorithm>

namespace astra {

void NetCamera::follow(int avatar_x, int avatar_y, int sector_w, int sector_h) {
    if (sector_w <= viewport_w && sector_h <= viewport_h) {
        cam_x = 0;
        cam_y = 0;
        return;
    }

    // Phase 5 S7e: cap effective deadzone so the "stable" region
    // [dz, viewport - dz] is always non-empty. The original code used
    // deadzone_margin directly; when 2 * margin > viewport, the
    // region inverts and follow() oscillates between the two follow
    // branches every render frame. Capping at (viewport - 1) / 2
    // guarantees 2 * dz <= viewport - 1.
    const int dz_x = std::min(deadzone_margin,
                              std::max(0, (viewport_w - 1) / 2));
    const int dz_y = std::min(deadzone_margin,
                              std::max(0, (viewport_h - 1) / 2));

    int rel_x = avatar_x - cam_x;
    if (rel_x < dz_x) {
        cam_x = std::max(0, avatar_x - dz_x);
    } else if (rel_x > viewport_w - dz_x) {
        cam_x = std::min(sector_w - viewport_w,
                         avatar_x - (viewport_w - dz_x));
    }
    cam_x = std::clamp(cam_x, 0, std::max(0, sector_w - viewport_w));

    int rel_y = avatar_y - cam_y;
    if (rel_y < dz_y) {
        cam_y = std::max(0, avatar_y - dz_y);
    } else if (rel_y > viewport_h - dz_y) {
        cam_y = std::min(sector_h - viewport_h,
                         avatar_y - (viewport_h - dz_y));
    }
    cam_y = std::clamp(cam_y, 0, std::max(0, sector_h - viewport_h));
}

} // namespace astra
