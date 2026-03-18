#include "astra/tilemap.h"

#include <algorithm>
#include <random>

namespace astra {

TileMap::TileMap(int width, int height, MapType type)
    : map_type_(type), width_(width), height_(height),
      tiles_(width * height, Tile::Empty),
      backdrop_(width * height, '\0'),
      region_ids_(width * height, -1) {}

char TileMap::backdrop(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return '\0';
    return backdrop_[y * width_ + x];
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
    Tile t = get(x, y);
    return t == Tile::Floor || t == Tile::Portal;
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

int TileMap::add_region(Region reg) {
    int id = static_cast<int>(regions_.size());
    regions_.push_back(std::move(reg));
    return id;
}

void TileMap::update_region(int id, const Region& reg) {
    if (id >= 0 && id < static_cast<int>(regions_.size())) {
        regions_[id] = reg;
    }
}

void TileMap::set_backdrop(int x, int y, char c) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        backdrop_[y * width_ + x] = c;
    }
}

void TileMap::clear_all() {
    std::fill(tiles_.begin(), tiles_.end(), Tile::Empty);
    std::fill(backdrop_.begin(), backdrop_.end(), '\0');
    std::fill(region_ids_.begin(), region_ids_.end(), -1);
    regions_.clear();
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

bool TileMap::find_open_spot_near(int near_x, int near_y, int& out_x, int& out_y,
                                  const std::vector<std::pair<int,int>>& exclude,
                                  std::mt19937* rng) const {
    int rid = region_id(near_x, near_y);
    if (rid < 0) return false;

    // Collect all valid candidates in the region
    std::vector<std::pair<int,int>> candidates;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (region_id(x, y) != rid) continue;
            if (get(x, y) != Tile::Floor) continue;

            bool excluded = false;
            for (const auto& [ex, ey] : exclude) {
                if (x == ex && y == ey) { excluded = true; break; }
            }
            if (excluded) continue;

            // Fast path: return first match when no rng
            if (!rng) {
                out_x = x;
                out_y = y;
                return true;
            }
            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    auto [cx, cy] = candidates[dist(*rng)];
    out_x = cx;
    out_y = cy;
    return true;
}

bool TileMap::find_open_spot_other_room(int avoid_x, int avoid_y, int& out_x, int& out_y,
                                        const std::vector<std::pair<int,int>>& exclude,
                                        std::mt19937* rng) const {
    int avoid_rid = region_id(avoid_x, avoid_y);

    std::vector<std::pair<int,int>> candidates;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (get(x, y) != Tile::Floor) continue;
            int rid = region_id(x, y);
            if (rid < 0 || rid == avoid_rid) continue;
            if (regions_[rid].type != RegionType::Room) continue;

            bool excluded = false;
            for (const auto& [ex, ey] : exclude) {
                if (x == ex && y == ey) { excluded = true; break; }
            }
            if (excluded) continue;

            if (!rng) {
                out_x = x;
                out_y = y;
                return true;
            }
            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    auto [cx, cy] = candidates[dist(*rng)];
    out_x = cx;
    out_y = cy;
    return true;
}

void TileMap::load_from(int w, int h, MapType type, std::string location,
                        std::vector<Tile> tiles, std::vector<int> rids,
                        std::vector<Region> regions, std::vector<char> backdrop) {
    width_ = w;
    height_ = h;
    map_type_ = type;
    location_name_ = std::move(location);
    tiles_ = std::move(tiles);
    region_ids_ = std::move(rids);
    regions_ = std::move(regions);
    backdrop_ = std::move(backdrop);
}

} // namespace astra
