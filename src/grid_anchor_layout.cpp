#include "astra/grid_sector.h"

namespace astra {

GridSector make_consciousness_anchor_sector() {
    GridSector s;
    s.w = 14;
    s.h = 10;
    s.tiles.assign(static_cast<size_t>(s.w * s.h), GridTile::Wall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    // Diamond-ring of firewalls forming an open shrine around a central
    // DataNode. The DataNode is the lore-archive interface (Plan 4).
    s.set(6, 3, GridTile::Firewall);
    s.set(8, 3, GridTile::Firewall);
    s.set(5, 4, GridTile::Firewall);
    s.set(9, 4, GridTile::Firewall);
    s.set(5, 5, GridTile::Firewall);
    s.set(9, 5, GridTile::Firewall);
    s.set(6, 6, GridTile::Firewall);
    s.set(8, 6, GridTile::Firewall);
    s.set(7, 4, GridTile::DataNode);   // lore archive

    s.spawn_x = 1;
    s.spawn_y = s.h / 2;
    s.set(s.w - 2, s.h / 2, GridTile::ExitNode);
    return s;
}

} // namespace astra
