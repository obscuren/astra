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
    // Overworld terrain
    OW_Plains,
    OW_Mountains,
    OW_Crater,
    OW_IceField,
    OW_LavaFlow,
    OW_Desert,
    OW_Fungal,
    OW_Forest,
    OW_River,
    OW_Lake,
    OW_Swamp,
    // Overworld POIs
    OW_CaveEntrance,
    OW_Ruins,
    OW_Settlement,
    OW_CrashedShip,
    OW_Outpost,
    // Overworld special
    OW_Landing,
};

inline char tile_glyph(Tile t) {
    switch (t) {
        case Tile::Floor:          return '.';
        case Tile::Wall:           return '#';
        case Tile::Portal:         return '>';
        case Tile::Water:          return '~';
        case Tile::Ice:            return '~';
        case Tile::OW_Plains:      return '.';
        case Tile::OW_Mountains:   return '^';
        case Tile::OW_Crater:      return 'o';
        case Tile::OW_IceField:    return '~';
        case Tile::OW_LavaFlow:    return '~';
        case Tile::OW_Desert:      return '.';
        case Tile::OW_Fungal:      return '"';
        case Tile::OW_Forest:      return 'T';
        case Tile::OW_River:       return '~';
        case Tile::OW_Lake:        return '~';
        case Tile::OW_Swamp:       return '"';
        case Tile::OW_CaveEntrance:return '>';
        case Tile::OW_Ruins:       return '#';
        case Tile::OW_Settlement:  return '*';
        case Tile::OW_CrashedShip: return '%';
        case Tile::OW_Outpost:     return '+';
        case Tile::OW_Landing:     return '=';
        default:                   return ' ';
    }
}

// Position-varied UTF-8 glyphs for overworld tiles (CP437-inspired).
// Returns a null-terminated UTF-8 string occupying one terminal cell.
inline const char* overworld_glyph(Tile t, int x, int y) {
    // Deterministic position hash for glyph variation
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);

    switch (t) {
        case Tile::OW_Mountains: {
            static const char* g[] = {
                "\xe2\x96\xb2",  // ▲
                "\xe2\x88\xa9",  // ∩
                "^",
                "\xce\x93",      // Γ
                "\xe2\x96\xb2",  // ▲
            };
            return g[h % 5];
        }
        case Tile::OW_Forest: {
            static const char* g[] = {
                "\xe2\x99\xa0",  // ♠
                "\xce\xa6",      // Φ
                "\xc6\x92",      // ƒ
            };
            return g[h % 3];
        }
        case Tile::OW_Plains: {
            static const char* g[] = {
                "\xc2\xb7",      // ·
                ".",
                ",",
            };
            return g[h % 3];
        }
        case Tile::OW_Desert: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xc2\xb7",      // ·
                ".",
            };
            return g[h % 3];
        }
        case Tile::OW_Lake: {
            return "\xe2\x89\x88"; // ≈
        }
        case Tile::OW_River: {
            static const char* g[] = {
                "\xe2\x89\x88",  // ≈
                "~",
            };
            return g[h % 2];
        }
        case Tile::OW_Swamp: {
            static const char* g[] = {
                "\xcf\x84",      // τ
                "\"",
                ",",
            };
            return g[h % 3];
        }
        case Tile::OW_Fungal: {
            static const char* g[] = {
                "\xce\xa6",      // Φ
                "\"",
                "\xcf\x84",      // τ
            };
            return g[h % 3];
        }
        case Tile::OW_IceField: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xc2\xb7",      // ·
                "'",
            };
            return g[h % 3];
        }
        case Tile::OW_LavaFlow: {
            static const char* g[] = {
                "\xe2\x89\x88",  // ≈
                "~",
            };
            return g[h % 2];
        }
        case Tile::OW_Crater: {
            static const char* g[] = {
                "o",
                "\xc2\xb0",      // °
            };
            return g[h % 2];
        }
        case Tile::OW_CaveEntrance: {
            static const char* g[] = {
                "\xe2\x96\xbc",  // ▼
                "\xce\x98",      // Θ
            };
            return g[h % 2];
        }
        case Tile::OW_Ruins: {
            static const char* g[] = {
                "\xcf\x80",      // π
                "\xce\xa9",      // Ω
                "\xc2\xa7",      // §
                "\xce\xa3",      // Σ
            };
            return g[h % 4];
        }
        case Tile::OW_Settlement:  return "\xe2\x99\xa6"; // ♦
        case Tile::OW_CrashedShip: {
            static const char* g[] = {
                "%",
                "\xc2\xa4",      // ¤
            };
            return g[h % 2];
        }
        case Tile::OW_Outpost:     return "+";
        case Tile::OW_Landing:     return "\xe2\x89\xa1"; // ≡
        default:                   return " ";
    }
}

enum class MapType : uint8_t {
    SpaceStation,
    DerelictStation,
    Nebula,
    Rocky,
    Lava,
    Asteroid,
    Starship,
    Overworld,
    DetailMap,
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

// Biome-specific UTF-8 wall glyph for dungeon/station rendering.
inline const char* dungeon_wall_glyph(Biome biome, int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);

    switch (biome) {
        case Biome::Station:
            return "\xe2\x96\x88";  // █
        case Biome::Rocky: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xe2\x96\x91",  // ░
                "#",
            };
            return g[h % 3];
        }
        case Biome::Volcanic: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
            };
            return g[h % 3];
        }
        case Biome::Ice: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xe2\x96\x91",  // ░
                "#",
            };
            return g[h % 3];
        }
        case Biome::Sandy: {
            static const char* g[] = {
                "\xe2\x96\x92",  // ▒
                "\xe2\x96\x91",  // ░
                "#",
            };
            return g[h % 3];
        }
        case Biome::Aquatic: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
            };
            return g[h % 2];
        }
        case Biome::Fungal: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "#",
            };
            return g[h % 3];
        }
        case Biome::Crystal: {
            static const char* g[] = {
                "\xe2\x97\x86",  // ◆
                "\xe2\x97\x87",  // ◇
            };
            return g[h % 2];
        }
        case Biome::Corroded: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "#",
                "\xe2\x96\x92",  // ▒
            };
            return g[h % 3];
        }
        default:
            return "#";
    }
}

// UTF-8 water/liquid glyph for dungeon rendering.
inline const char* dungeon_water_glyph(Biome biome, int x, int y) {
    (void)biome;
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);

    static const char* g[] = {
        "\xe2\x89\x88",  // ≈
        "\xe2\x89\x88",  // ≈
        "~",
    };
    return g[h % 3];
}

// UTF-8 portal/stairs glyph for dungeon rendering.
inline const char* dungeon_portal_glyph() {
    return "\xe2\x96\xbc";  // ▼
}

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
    // Starship interiors
    ShipCockpit,
    ShipCommandCenter,
    ShipMessHall,
    ShipQuarters,
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
    ShipTerminal,   // '>'  — board your starship
};

struct FixtureData {
    FixtureType type = FixtureType::Table;
    char glyph = '?';
    const char* utf8_glyph = nullptr;  // UTF-8 override (non-null = use instead of glyph)
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

    // Glyph override layer (for stamp system)
    uint8_t glyph_override(int x, int y) const;
    void set_glyph_override(int x, int y, uint8_t idx);
    const std::vector<uint8_t>& glyph_overrides() const { return glyph_override_; }
    void load_glyph_overrides(std::vector<uint8_t> overrides);

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
    std::vector<uint8_t> glyph_override_;  // parallel to tiles_, 0 = no override
};

} // namespace astra
