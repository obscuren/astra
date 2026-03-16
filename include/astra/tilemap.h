#pragma once

#include <cstdint>
#include <vector>

namespace astra {

enum class Tile : uint8_t {
    Empty,
    Floor,
    Wall,
};

inline char tile_glyph(Tile t) {
    switch (t) {
        case Tile::Floor: return '.';
        case Tile::Wall:  return '#';
        default:          return ' ';
    }
}

enum class RegionType : uint8_t {
    Room,
    Corridor,
};

struct Region {
    RegionType type = RegionType::Room;
    bool lit = false;
};

class TileMap {
public:
    TileMap() = default;
    TileMap(int width, int height);

    void generate(unsigned seed);

    Tile get(int x, int y) const;
    void set(int x, int y, Tile t);
    bool passable(int x, int y) const;
    bool opaque(int x, int y) const;

    // Region queries
    int region_id(int x, int y) const;
    int region_count() const { return static_cast<int>(regions_.size()); }
    const Region& region(int id) const { return regions_[id]; }

    int width() const { return width_; }
    int height() const { return height_; }

    // Returns a valid floor position for spawning
    void find_open_spot(int& out_x, int& out_y) const;

private:
    void carve_room(int x1, int y1, int x2, int y2);
    void carve_corridor_h(int x1, int x2, int y, int rid);
    void carve_corridor_v(int y1, int y2, int x, int rid);
    void set_region(int x, int y, int rid);

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
    std::vector<int> region_ids_;
    std::vector<Region> regions_;
};

} // namespace astra
