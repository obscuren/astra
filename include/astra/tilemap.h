#pragma once

#include "astra/renderer.h"

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
    Water,
    Ice,
    Fixture,
};

inline char tile_glyph(Tile t) {
    switch (t) {
        case Tile::Floor:  return '.';
        case Tile::Wall:   return '#';
        case Tile::Portal: return '>';
        case Tile::Water:  return '~';
        case Tile::Ice:    return '~';
        default:           return ' ';
    }
}

enum class MapType : uint8_t {
    SpaceStation,
    DerelictStation,
    Nebula,
    Rocky,
    Lava,
    Asteroid,
};

enum class Biome : uint8_t {
    Station,
    Rocky,
    Volcanic,
    Ice,
    Sandy,
    Aquatic,
    Fungal,
    Crystal,
    Corroded,
};

struct BiomeColors {
    Color wall;
    Color floor;
    Color water;
    Color remembered;
};

BiomeColors biome_colors(Biome b);

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
    // Derelict station
    DerelictBay,
    HullBreach,
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

// --- Fixture system ---

enum class FixtureType : uint8_t {
    // Decorative (impassable, no interaction)
    Table,          // 'o'  — cantina tables, command tables
    Console,        // '#'  — computer terminals (decorative)
    Crate,          // '='  — cargo crates
    Bunk,           // '='  — sleeping bunks
    Rack,           // '|'  — weapon/supply racks (decorative)
    Conduit,        // '%'  — engineering pipes
    ShuttleClamp,   // '='  — docking bay clamps
    Shelf,          // '['  — storage shelving
    Viewport,       // '"'  — observatory window

    // Walkable (floor-like, no interaction)
    Stool,          // 'o'  — bar stools, chairs
    Debris,         // ','  — floor clutter

    // Interactable (impassable, player presses 'e' adjacent)
    HealPod,        // '+'  — medbay healing pod, cooldown-based full heal
    FoodTerminal,   // '$'  — buy food: eat-here or take-away
    WeaponDisplay,  // '/'  — browse/buy weapons
    RepairBench,    // '%'  — repair gear (future)
    SupplyLocker,   // '&'  — search for random loot (future)
    StarChart,      // '*'  — observatory lore terminal (future)
    RestPod,        // '='  — crew quarters rest (advance ticks, full heal)
};

struct FixtureData {
    FixtureType type = FixtureType::Table;
    char glyph = '?';
    Color color = Color::White;
    bool passable = false;
    bool interactable = false;
    int cooldown = 0;           // ticks until reusable (0 = no cooldown, -1 = one-time)
    int last_used_tick = -1;    // world_tick when last used (-1 = never)
};

FixtureData make_fixture(FixtureType type);

// --- Room features ---

enum class RoomFeature : uint16_t {
    None           = 0,
    Healing        = 1 << 0,
    Rest           = 1 << 1,
    FoodShop       = 1 << 2,
    WeaponShop     = 1 << 3,
    QuestGiver     = 1 << 4,
    LoreSource     = 1 << 5,
    Repair         = 1 << 6,
    Storage        = 1 << 7,
};

inline RoomFeature operator|(RoomFeature a, RoomFeature b) {
    return static_cast<RoomFeature>(
        static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline RoomFeature operator&(RoomFeature a, RoomFeature b) {
    return static_cast<RoomFeature>(
        static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
inline bool has_feature(RoomFeature set, RoomFeature flag) {
    return (static_cast<uint16_t>(set) & static_cast<uint16_t>(flag)) != 0;
}

RoomFeature default_features(RoomFlavor flavor);

struct Region {
    RegionType type = RegionType::Room;
    bool lit = false;
    RoomFlavor flavor = RoomFlavor::EmptyRoom;
    RoomFeature features = RoomFeature::None;
    std::string name;           // e.g. "Storage Bay 7"
    std::string enter_message;  // shown when player enters
};

class TileMap {
public:
    TileMap() = default;
    TileMap(int width, int height, MapType type = MapType::SpaceStation);

    MapType map_type() const { return map_type_; }
    Biome biome() const { return biome_; }
    void set_biome(Biome b) { biome_ = b; }
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

    // Fixture accessors
    int fixture_id(int x, int y) const;
    const FixtureData& fixture(int id) const { return fixtures_[id]; }
    FixtureData& fixture_mut(int id) { return fixtures_[id]; }
    int add_fixture(int x, int y, FixtureData fd);
    int fixture_count() const { return static_cast<int>(fixtures_.size()); }
    const std::vector<FixtureData>& fixtures_vec() const { return fixtures_; }
    const std::vector<int>& fixture_ids() const { return fixture_ids_; }

    // Hub flag
    bool is_hub() const { return hub_; }
    void set_hub(bool h) { hub_ = h; }

    // Const accessors for serialization
    const std::vector<Tile>& tiles() const { return tiles_; }
    const std::vector<int>& region_ids() const { return region_ids_; }
    const std::vector<Region>& regions_vec() const { return regions_; }
    const std::vector<char>& backdrop_data() const { return backdrop_; }

    // Bulk load from deserialized data
    void load_from(int w, int h, MapType type, Biome biome, std::string location,
                   std::vector<Tile> tiles, std::vector<int> rids,
                   std::vector<Region> regions, std::vector<char> backdrop);
    void load_fixtures(std::vector<FixtureData> fixtures, std::vector<int> fixture_ids);

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

    // Find a floor tile in a specific region by ID.
    bool find_open_spot_in_region(int region_id, int& out_x, int& out_y,
                                  const std::vector<std::pair<int,int>>& exclude = {},
                                  std::mt19937* rng = nullptr) const;

private:
    MapType map_type_ = MapType::SpaceStation;
    Biome biome_ = Biome::Station;
    int width_ = 0;
    int height_ = 0;
    std::string location_name_ = "Unknown";
    bool hub_ = false;
    std::vector<Tile> tiles_;
    std::vector<char> backdrop_;
    std::vector<int> region_ids_;
    std::vector<Region> regions_;
    std::vector<FixtureData> fixtures_;
    std::vector<int> fixture_ids_;  // parallel to tiles_, -1 if no fixture
};

} // namespace astra
