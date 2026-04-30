#include "astra/grid_sector.h"

#include "astra/grid_regional_generator.h"

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
        || t == GridTile::ExitNode
        || t == GridTile::EncryptedFile;
}

GridSector gen_subnet_sector(uint32_t seed, int security_tier) {
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
    return s;
}

GridSector gen_regional_sector(uint32_t seed, int security_tier) {
    return grid_regional_generator::generate(seed, security_tier);
}

} // namespace astra
