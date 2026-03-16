#include "astra/tilemap.h"

#include <algorithm>
#include <random>

namespace astra {

TileMap::TileMap(int width, int height, MapType type)
    : map_type_(type), width_(width), height_(height),
      tiles_(width * height, Tile::Empty),
      backdrop_(width * height, '\0'),
      region_ids_(width * height, -1) {}

void TileMap::generate(unsigned seed) {
    std::fill(tiles_.begin(), tiles_.end(), Tile::Empty);
    std::fill(region_ids_.begin(), region_ids_.end(), -1);
    regions_.clear();
    std::mt19937 rng(seed);

    struct RoomRect { int x1, y1, x2, y2; };
    std::vector<RoomRect> rooms;

    int max_rooms = 8;
    int min_size = 4;
    int max_size = std::min(width_ / 3, height_ / 3);
    if (max_size < min_size + 1) max_size = min_size + 1;

    std::uniform_int_distribution<int> size_dist(min_size, max_size);
    std::uniform_int_distribution<int> lit_chance(0, 99);

    for (int i = 0; i < max_rooms * 4 && static_cast<int>(rooms.size()) < max_rooms; ++i) {
        // +2 for wall border on each side
        int w = size_dist(rng);
        int h = size_dist(rng);
        int total_w = w + 2;
        int total_h = h + 2;
        if (total_w >= width_ || total_h >= height_) continue;

        std::uniform_int_distribution<int> x_dist(0, width_ - total_w);
        std::uniform_int_distribution<int> y_dist(0, height_ - total_h);
        int x = x_dist(rng);
        int y = y_dist(rng);

        // Room bounds (walls included)
        RoomRect candidate{x, y, x + total_w - 1, y + total_h - 1};

        // Check overlap with existing rooms (with 1-tile gap)
        bool overlaps = false;
        for (const auto& r : rooms) {
            if (candidate.x1 - 1 <= r.x2 && candidate.x2 + 1 >= r.x1 &&
                candidate.y1 - 1 <= r.y2 && candidate.y2 + 1 >= r.y1) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        // Create region for this room (~60% lit)
        int rid = static_cast<int>(regions_.size());
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = lit_chance(rng) < 60;
        regions_.push_back(reg);

        // Place walls around the room border, floors inside
        for (int ry = candidate.y1; ry <= candidate.y2; ++ry) {
            for (int rx = candidate.x1; rx <= candidate.x2; ++rx) {
                if (ry == candidate.y1 || ry == candidate.y2 ||
                    rx == candidate.x1 || rx == candidate.x2) {
                    set(rx, ry, Tile::Wall);
                    set_region(rx, ry, rid);
                } else {
                    set(rx, ry, Tile::Floor);
                    set_region(rx, ry, rid);
                }
            }
        }

        // Connect to previous room with corridor
        if (!rooms.empty()) {
            int cx1 = (rooms.back().x1 + rooms.back().x2) / 2;
            int cy1 = (rooms.back().y1 + rooms.back().y2) / 2;
            int cx2 = (candidate.x1 + candidate.x2) / 2;
            int cy2 = (candidate.y1 + candidate.y2) / 2;

            // Create corridor region (unlit by default)
            int crid = static_cast<int>(regions_.size());
            Region creg;
            creg.type = RegionType::Corridor;
            creg.lit = false;
            regions_.push_back(creg);

            if (rng() % 2 == 0) {
                carve_corridor_h(cx1, cx2, cy1, crid);
                carve_corridor_v(cy1, cy2, cx2, crid);
            } else {
                carve_corridor_v(cy1, cy2, cx1, crid);
                carve_corridor_h(cx1, cx2, cy2, crid);
            }
        }

        rooms.push_back(candidate);
    }

    // Ensure at least one room exists
    if (rooms.empty()) {
        int rid = static_cast<int>(regions_.size());
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true;
        regions_.push_back(reg);

        for (int ry = 1; ry < height_ - 1; ++ry) {
            for (int rx = 1; rx < width_ - 1; ++rx) {
                if (ry == 1 || ry == height_ - 2 || rx == 1 || rx == width_ - 2) {
                    set(rx, ry, Tile::Wall);
                } else {
                    set(rx, ry, Tile::Floor);
                }
                set_region(rx, ry, rid);
            }
        }
    }

    generate_backdrop(seed);
}

char TileMap::backdrop(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return '\0';
    return backdrop_[y * width_ + x];
}

void TileMap::generate_backdrop(unsigned seed) {
    std::fill(backdrop_.begin(), backdrop_.end(), '\0');

    if (map_type_ != MapType::SpaceStation) return;

    // Scatter stars on empty tiles
    std::mt19937 rng(seed ^ 0xBACDu);
    std::uniform_int_distribution<int> chance(0, 99);
    std::uniform_int_distribution<int> star_type(0, 9);

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (get(x, y) != Tile::Empty) continue;

            int roll = chance(rng);
            if (roll < 3) {
                // 3% chance of a star
                int st = star_type(rng);
                if (st < 6)       backdrop_[y * width_ + x] = '.';
                else if (st < 9)  backdrop_[y * width_ + x] = '*';
                else              backdrop_[y * width_ + x] = '+';
            }
        }
    }
}

Tile TileMap::get(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return Tile::Empty;
    return tiles_[y * width_ + x];
}

void TileMap::set(int x, int y, Tile t) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        tiles_[y * width_ + x] = t;
    }
}

bool TileMap::passable(int x, int y) const {
    return get(x, y) == Tile::Floor;
}

bool TileMap::opaque(int x, int y) const {
    Tile t = get(x, y);
    return t == Tile::Wall || t == Tile::Empty;
}

int TileMap::region_id(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
    return region_ids_[y * width_ + x];
}

void TileMap::set_region(int x, int y, int rid) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        region_ids_[y * width_ + x] = rid;
    }
}

void TileMap::find_open_spot(int& out_x, int& out_y) const {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (get(x, y) == Tile::Floor) {
                out_x = x;
                out_y = y;
                return;
            }
        }
    }
    out_x = width_ / 2;
    out_y = height_ / 2;
}

void TileMap::carve_room(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            set(x, y, Tile::Floor);
        }
    }
}

void TileMap::carve_corridor_h(int x1, int x2, int y, int rid) {
    int lo = std::min(x1, x2);
    int hi = std::max(x1, x2);
    for (int x = lo; x <= hi; ++x) {
        // Floor in the center, walls on sides
        set(x, y, Tile::Floor);
        set_region(x, y, rid);
        if (get(x, y - 1) == Tile::Empty) {
            set(x, y - 1, Tile::Wall);
            set_region(x, y - 1, rid);
        }
        if (get(x, y + 1) == Tile::Empty) {
            set(x, y + 1, Tile::Wall);
            set_region(x, y + 1, rid);
        }
    }
}

void TileMap::carve_corridor_v(int y1, int y2, int x, int rid) {
    int lo = std::min(y1, y2);
    int hi = std::max(y1, y2);
    for (int y = lo; y <= hi; ++y) {
        set(x, y, Tile::Floor);
        set_region(x, y, rid);
        if (get(x - 1, y) == Tile::Empty) {
            set(x - 1, y, Tile::Wall);
            set_region(x - 1, y, rid);
        }
        if (get(x + 1, y) == Tile::Empty) {
            set(x + 1, y, Tile::Wall);
            set_region(x + 1, y, rid);
        }
    }
}

} // namespace astra
