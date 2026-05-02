#include "astra/deep_grid_sector.h"

namespace astra {

GridSector make_deep_grid_base() {
    GridSector sec;
    sec.w = 60;
    sec.h = 40;
    sec.tiles.assign(static_cast<size_t>(60) * 40, GridTile::Floor);

    auto stamp = [&](int x, int y, GridTile t) {
        if (x >= 0 && y >= 0 && x < 60 && y < 40) {
            sec.tiles[y * 60 + x] = t;
        }
    };

    // Outer perimeter — structural Wall (not Firewall — the deep-Grid is
    // inside-only; player can't escape through the outer wall).
    for (int x = 0; x < 60; ++x) { stamp(x, 0, GridTile::Wall); stamp(x, 39, GridTile::Wall); }
    for (int y = 0; y < 40; ++y) { stamp(0, y, GridTile::Wall); stamp(59, y, GridTile::Wall); }

    // Anchor region: cols 1-13, rows 1-12. Bounded on east by structural
    // wall at x=13 with a 2-cell open doorway in the middle.
    for (int y = 1; y <= 12; ++y) stamp(13, y, GridTile::Wall);
    stamp(13, 6, GridTile::Floor);   // east doorway top
    stamp(13, 7, GridTile::Floor);   // east doorway bottom
    // Anchor south wall at y=12 separating from the bottom expanse.
    for (int x = 1; x <= 13; ++x) stamp(x, 12, GridTile::Wall);

    // Anchor lore-archive DataNode — Your.Anchor v1 visual marker. Place
    // at (3, 4) — visible from the spawn point but not on it.
    stamp(3, 4, GridTile::DataNode);
    sec.spawn_x = 6;
    sec.spawn_y = 6;

    // Atlas region: cols 14-44, rows 1-30. The big open area that Cut 3
    // Task 31 will populate with WarpAnchor tiles. Atlas south wall at
    // y=30 keeps the bottom of the deep-Grid unused for Plan 7.
    for (int x = 14; x <= 44; ++x) stamp(x, 30, GridTile::Wall);
    // Vertical wall between Atlas and Frontier with a tier-2 Firewall
    // breach point.
    for (int y = 1; y <= 30; ++y) stamp(45, y, GridTile::Firewall);

    // Frontier region: cols 46-58, rows 1-30. All Floor, but only reachable
    // by breaching the firewall at x=45. Plan 7 will subdivide this with
    // additional firewall partitions and content.
    // (The Floor is already filled by the initial fill above.)

    // Bottom corridor (rows 31-38) is leftover floor — kept open for
    // future expansion. Doors from Anchor / Atlas / Frontier into this
    // corridor are not punched in Cut 3; the space is reserved.

    return sec;
}

} // namespace astra
