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

GridSector make_player_deep_grid_base() {
    GridSector s;
    s.w = 30;
    s.h = 20;
    s.tiles.assign(static_cast<size_t>(s.w * s.h), GridTile::Floor);

    auto set = [&](int x, int y, GridTile t) {
        if (x >= 0 && x < s.w && y >= 0 && y < s.h)
            s.tiles[static_cast<size_t>(y * s.w + x)] = t;
    };

    // Outer firewall border.
    for (int x = 0; x < s.w; ++x) {
        set(x, 0,        GridTile::Firewall);
        set(x, s.h - 1,  GridTile::Firewall);
    }
    for (int y = 0; y < s.h; ++y) {
        set(0,       y, GridTile::Firewall);
        set(s.w - 1, y, GridTile::Firewall);
    }

    // Northern utility room divider with one doorway.
    for (int x = 1; x < s.w - 1; ++x) set(x, 5, GridTile::Firewall);
    set(8, 5, GridTile::Floor);

    // Northern fixtures (DataNode glyph reused for stash terminal, sig rack,
    // AI contacts; EncryptedFile glyph for the lore vault).
    set(4,  3, GridTile::DataNode);       // stash terminal
    set(15, 3, GridTile::DataNode);       // signature program rack
    set(25, 3, GridTile::EncryptedFile);  // lore vault interface

    // Southern AI contact spots.
    set(5,  12, GridTile::DataNode);
    set(20, 14, GridTile::DataNode);

    // Exit back to regional darknet.
    set(15, 18, GridTile::ExitNode);

    s.spawn_x = 15;
    s.spawn_y = 17;
    return s;
}

} // namespace astra
