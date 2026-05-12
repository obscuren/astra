#include "astra/grid_sector.h"

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
    if (t == GridTile::Door) return !is_locked_door(x, y);
    return t == GridTile::Floor
        || t == GridTile::DataNode
        || t == GridTile::ExitNode
        || t == GridTile::EncryptedFile
        || t == GridTile::Connector
        || t == GridTile::WarpAnchor;
}

bool GridSector::is_locked_door(int x, int y) const {
    return locked_doors.count({x, y}) != 0;
}

void GridSector::unlock_door(int x, int y) {
    locked_doors.erase({x, y});
}

} // namespace astra
