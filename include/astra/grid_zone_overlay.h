#pragma once

#include "astra/grid_camera.h"
#include "astra/grid_sector.h"
#include "astra/renderer.h"

namespace astra::grid_zone_overlay {

// Draw faint dashed perimeter + banner per zone, between the floor pass
// and the content pass. Camera-aware: zones outside the visible playfield
// are skipped. Single-zone LANs are silently skipped (no banner clutter).
void draw(Renderer& r,
          const GridSector& sec,
          const GridCamera& cam,
          int playfield_x, int playfield_y,
          int playfield_w, int playfield_h);

} // namespace astra::grid_zone_overlay
