#include "astra/net_camera.h"

#include <algorithm>

namespace astra {

void NetCamera::follow(int avatar_x, int avatar_y, int sector_w, int sector_h) {
    if (sector_w <= viewport_w && sector_h <= viewport_h) {
        cam_x = 0;
        cam_y = 0;
        return;
    }

    int rel_x = avatar_x - cam_x;
    if (rel_x < deadzone_margin) {
        cam_x = std::max(0, avatar_x - deadzone_margin);
    } else if (rel_x > viewport_w - deadzone_margin) {
        cam_x = std::min(sector_w - viewport_w,
                         avatar_x - (viewport_w - deadzone_margin));
    }
    cam_x = std::clamp(cam_x, 0, std::max(0, sector_w - viewport_w));

    int rel_y = avatar_y - cam_y;
    if (rel_y < deadzone_margin) {
        cam_y = std::max(0, avatar_y - deadzone_margin);
    } else if (rel_y > viewport_h - deadzone_margin) {
        cam_y = std::min(sector_h - viewport_h,
                         avatar_y - (viewport_h - deadzone_margin));
    }
    cam_y = std::clamp(cam_y, 0, std::max(0, sector_h - viewport_h));
}

} // namespace astra
