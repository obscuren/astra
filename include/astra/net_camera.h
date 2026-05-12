#pragma once

namespace astra {

// Follows the avatar inside a Grid sector with a deadzone, so movement near
// the centre never scrolls but approaching an edge does. When the sector
// fits inside the viewport, the camera locks to (0, 0) and behaves like
// the legacy fixed-origin renderer.
struct NetCamera {
    int viewport_w = 60;
    int viewport_h = 22;
    int cam_x = 0;
    int cam_y = 0;
    int deadzone_margin = 12;

    void follow(int avatar_x, int avatar_y, int sector_w, int sector_h);
};

} // namespace astra
