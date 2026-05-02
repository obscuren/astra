#include "astra/grid_sector.h"

#include "astra/tilemap.h"   // FixtureType — Cut 2.6 device avatar source

#include <random>

namespace astra {

GridTile GridSector::at(int x, int y) const {
    if (!in_bounds(x, y)) return GridTile::Wall;
    return tiles[static_cast<size_t>(y * w + x)];
}

void GridSector::set(int x, int y, GridTile t) {
    if (!in_bounds(x, y)) return;
    tiles[static_cast<size_t>(y * w + x)] = t;
}

bool GridSector::in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < w && y < h;
}

bool GridSector::passable(int x, int y) const {
    GridTile t = at(x, y);
    return t == GridTile::Floor
        || t == GridTile::DataNode
        || t == GridTile::Gateway
        || t == GridTile::DeepGridGateway
        || t == GridTile::ExitNode
        || t == GridTile::EncryptedFile
        || t == GridTile::Connector
        || t == GridTile::WarpAnchor;
}

GridSector gen_subnet_sector(uint32_t seed, int security_tier) {
    return gen_subnet_sector(seed, security_tier, FixtureType::Console);
}

GridSector gen_subnet_sector(uint32_t seed, int security_tier, FixtureType source_type) {
    std::mt19937 rng(seed);
    GridSector s;
    s.w = 8;
    s.h = 8;
    s.tiles.assign(static_cast<size_t>(s.w * s.h), GridTile::Wall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    s.spawn_x = 1;
    s.spawn_y = s.h - 2;
    s.set(s.w - 2, 1, GridTile::ExitNode);
    s.set(s.w / 2, s.h / 2, GridTile::DataNode);
    if (security_tier >= 2) {
        std::uniform_int_distribution<int> fx_dist(2, s.w - 3);
        std::uniform_int_distribution<int> fy_dist(2, s.h - 3);
        s.set(fx_dist(rng), fy_dist(rng), GridTile::Firewall);
    }

    // Plan 5 Cut 2.6: stamp a wall-mounted device-avatar on the north wall
    // mid-x. The renderer picks the glyph from source_fixture_type.
    s.source_fixture_type = source_type;
    int ax = s.w / 2;
    int ay = 0;
    if (s.in_bounds(ax, ay) && s.at(ax, ay) == GridTile::Wall) {
        s.set(ax, ay, GridTile::DeviceAvatar);
    } else {
        // Fallback: scan the top row for the first Wall tile.
        for (int x = 0; x < s.w; ++x) {
            if (s.at(x, 0) == GridTile::Wall) {
                s.set(x, 0, GridTile::DeviceAvatar);
                break;
            }
        }
    }
    return s;
}

namespace {

// Helper: stamp a horizontal interior wall along row `y` with a doorway at
// column `door_x`. Skips the outer firewall border so corners stay intact.
void stamp_hwall(GridSector& s, int y, int door_x) {
    for (int x = 1; x < s.w - 1; ++x) {
        if (x == door_x) continue;
        s.set(x, y, GridTile::Firewall);
    }
}

void stamp_vwall(GridSector& s, int x, int door_y) {
    for (int y = 1; y < s.h - 1; ++y) {
        if (y == door_y) continue;
        s.set(x, y, GridTile::Firewall);
    }
}

bool place_random_floor(GridSector& s, GridTile t, std::mt19937& rng) {
    std::uniform_int_distribution<int> dx(1, s.w - 2);
    std::uniform_int_distribution<int> dy(1, s.h - 2);
    for (int tries = 0; tries < 60; ++tries) {
        int x = dx(rng), y = dy(rng);
        if (s.at(x, y) == GridTile::Floor) {
            s.set(x, y, t);
            return true;
        }
    }
    return false;
}

} // namespace

GridSector gen_regional_sector(uint32_t seed, int security_tier) {
    std::mt19937 rng(seed);

    // 28×14 — comfortable inside the 60×22 grid viewport, large enough for
    // 3-4 compartments without scrolling.
    GridSector s;
    s.w = 28;
    s.h = 14;
    s.tiles.assign(static_cast<size_t>(s.w * s.h), GridTile::Firewall);
    for (int y = 1; y < s.h - 1; ++y)
        for (int x = 1; x < s.w - 1; ++x)
            s.set(x, y, GridTile::Floor);

    // Seeded variant — three hand-shaped layouts pick deterministically.
    enum class Variant { VBisect, HBisect, TeeRight };
    Variant variant = static_cast<Variant>(rng() % 3);

    std::uniform_int_distribution<int> door_yd(2, s.h - 3);
    std::uniform_int_distribution<int> door_xd(2, s.w - 3);

    switch (variant) {
        case Variant::VBisect: {
            int x = s.w / 2;
            stamp_vwall(s, x, door_yd(rng));
            break;
        }
        case Variant::HBisect: {
            int y = s.h / 2;
            stamp_hwall(s, y, door_xd(rng));
            break;
        }
        case Variant::TeeRight: {
            int x_main = s.w / 2;
            int y_arm  = s.h / 2;
            stamp_vwall(s, x_main, door_yd(rng));
            // Horizontal arm to the right of the vertical, leaving the main
            // doorway intact.
            for (int x = x_main + 1; x < s.w - 1; ++x) {
                s.set(x, y_arm, GridTile::Firewall);
            }
            int arm_door_x = std::uniform_int_distribution<int>(
                                 x_main + 2, s.w - 3)(rng);
            s.set(arm_door_x, y_arm, GridTile::Floor);
            break;
        }
    }

    // Spawn at the bottom-left corner; ExitNode at the top-right corner so
    // the player sees an obvious target across the sector.
    s.spawn_x = 2;
    s.spawn_y = s.h - 3;
    if (s.at(s.spawn_x, s.spawn_y) != GridTile::Floor) {
        s.set(s.spawn_x, s.spawn_y, GridTile::Floor);
    }
    s.set(s.w - 3, 2, GridTile::ExitNode);

    // Decorate. Counts mirror what the spec wants (1-4 EncryptedFile, 0-2
    // DataNode, 50% Gateway). Tier 3 adds an extra encrypted file.
    int n_enc = 1 + static_cast<int>(rng() % 4);
    for (int i = 0; i < n_enc; ++i) place_random_floor(s, GridTile::EncryptedFile, rng);
    int n_data = static_cast<int>(rng() % 3);
    for (int i = 0; i < n_data; ++i) place_random_floor(s, GridTile::DataNode, rng);
    if ((rng() & 1u) == 0) place_random_floor(s, GridTile::Gateway, rng);

    if (security_tier >= 3) {
        place_random_floor(s, GridTile::EncryptedFile, rng);
    }

    return s;
}

} // namespace astra
