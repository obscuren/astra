#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace astra {

enum class Tile : uint8_t {
    Empty,
    Floor,
    Wall,
    Portal,
};

inline char tile_glyph(Tile t) {
    switch (t) {
        case Tile::Floor:  return '.';
        case Tile::Wall:   return '#';
        case Tile::Portal: return '>';
        default:           return ' ';
    }
}

enum class MapType : uint8_t {
    SpaceStation,
    Nebula,
    Rocky,
    Lava,
};

enum class RegionType : uint8_t {
    Room,
    Corridor,
};

enum class RoomFlavor : uint8_t {
    // Space station
    EmptyRoom,
    Cantina,
    StorageBay,
    CrewQuarters,
    Medbay,
    Engineering,
    CommandCenter,
    CargoHold,
    Armory,
    Observatory,
    // Rocky / asteroid
    CavernEmpty,
    CavernMushroom,
    CavernCrystal,
    CavernPool,
    MinedOut,
    CollapseZone,
    // Corridors (all map types)
    CorridorPlain,
    CorridorDimLit,
    CorridorMaintenance,
    CorridorDamaged,
};

struct Region {
    RegionType type = RegionType::Room;
    bool lit = false;
    RoomFlavor flavor = RoomFlavor::EmptyRoom;
    std::string name;           // e.g. "Storage Bay 7"
    std::string enter_message;  // shown when player enters
};

class TileMap {
public:
    TileMap() = default;
    TileMap(int width, int height, MapType type = MapType::SpaceStation);

    MapType map_type() const { return map_type_; }
    char backdrop(int x, int y) const;

    Tile get(int x, int y) const;
    void set(int x, int y, Tile t);
    bool passable(int x, int y) const;
    bool opaque(int x, int y) const;

    // Region queries
    int region_id(int x, int y) const;
    int region_count() const { return static_cast<int>(regions_.size()); }
    const Region& region(int id) const { return regions_[id]; }

    // Public setters for map generators
    void set_region(int x, int y, int rid);
    int add_region(Region reg);
    void update_region(int id, const Region& reg);
    void set_backdrop(int x, int y, char c);
    void clear_all();

    int width() const { return width_; }
    int height() const { return height_; }

    // Location name
    const std::string& location_name() const { return location_name_; }
    void set_location_name(const std::string& name) { location_name_ = name; }

    // Const accessors for serialization
    const std::vector<Tile>& tiles() const { return tiles_; }
    const std::vector<int>& region_ids() const { return region_ids_; }
    const std::vector<Region>& regions_vec() const { return regions_; }
    const std::vector<char>& backdrop_data() const { return backdrop_; }

    // Bulk load from deserialized data
    void load_from(int w, int h, MapType type, std::string location,
                   std::vector<Tile> tiles, std::vector<int> rids,
                   std::vector<Region> regions, std::vector<char> backdrop);

    // Returns a valid floor position for spawning
    void find_open_spot(int& out_x, int& out_y) const;

    // Find a floor tile in the same region as (near_x, near_y), avoiding exclude list.
    // Pass an rng to pick randomly; nullptr picks the first match.
    bool find_open_spot_near(int near_x, int near_y, int& out_x, int& out_y,
                             const std::vector<std::pair<int,int>>& exclude = {},
                             std::mt19937* rng = nullptr) const;

    // Find a floor tile in a different room region than (avoid_x, avoid_y).
    bool find_open_spot_other_room(int avoid_x, int avoid_y, int& out_x, int& out_y,
                                   const std::vector<std::pair<int,int>>& exclude = {},
                                   std::mt19937* rng = nullptr) const;

private:
    MapType map_type_ = MapType::SpaceStation;
    int width_ = 0;
    int height_ = 0;
    std::string location_name_ = "Unknown";
    std::vector<Tile> tiles_;
    std::vector<char> backdrop_;
    std::vector<int> region_ids_;
    std::vector<Region> regions_;
};

} // namespace astra
