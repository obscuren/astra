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

    generate_flavors(rng);
    generate_backdrop(seed);
}

// --- Room / corridor flavor tables ---

struct FlavorEntry {
    RoomFlavor flavor;
    const char* name;
    const char* enter_message;
};

static constexpr FlavorEntry station_room_flavors[] = {
    {RoomFlavor::EmptyRoom,     "Empty Compartment",
        "An empty compartment. Dust motes drift in the recycled air."},
    {RoomFlavor::Cantina,       "Cantina",
        "The faint smell of synth-brew lingers. A cantina, long since abandoned."},
    {RoomFlavor::StorageBay,    "Storage Bay",
        "Crates and containers line the walls. A storage bay, mostly picked clean."},
    {RoomFlavor::CrewQuarters,  "Crew Quarters",
        "Bunks are bolted to the walls. Someone lived here once."},
    {RoomFlavor::Medbay,        "Medbay",
        "Medical equipment hums on standby. The air smells faintly of antiseptic."},
    {RoomFlavor::Engineering,   "Engineering Bay",
        "Conduits and junction boxes crowd every surface. The station's guts."},
    {RoomFlavor::CommandCenter,  "Command Center",
        "Consoles flicker with residual power. This was the nerve center."},
    {RoomFlavor::CargoHold,     "Cargo Hold",
        "A cavernous hold. Magnetic clamps line the deck, most of them empty."},
    {RoomFlavor::Armory,        "Armory",
        "Weapon racks stand in rows, most stripped bare. A few lockers remain sealed."},
    {RoomFlavor::Observatory,   "Observatory",
        "A viewport dominates the far wall. Jupiter's swirling storms fill the view."},
};

static constexpr FlavorEntry station_corridor_flavors[] = {
    {RoomFlavor::CorridorPlain,       "Corridor",
        "A standard station corridor. Overhead lights cast a sterile glow."},
    {RoomFlavor::CorridorDimLit,      "Dim Corridor",
        "The lights here flicker and buzz. Shadows pool in the corners."},
    {RoomFlavor::CorridorMaintenance, "Maintenance Shaft",
        "Exposed piping and cable runs. A maintenance access corridor."},
    {RoomFlavor::CorridorDamaged,     "Damaged Corridor",
        "Scorch marks streak the walls. Something violent happened here."},
};

static constexpr FlavorEntry rocky_room_flavors[] = {
    {RoomFlavor::CavernEmpty,    "Empty Cavern",
        "A rough-hewn cavern. Dripping water echoes off the stone."},
    {RoomFlavor::CavernMushroom, "Fungal Cavern",
        "Bioluminescent fungi cling to the walls, casting a sickly green glow."},
    {RoomFlavor::CavernCrystal,  "Crystal Grotto",
        "Crystalline formations jut from every surface, refracting your light."},
    {RoomFlavor::CavernPool,     "Subterranean Pool",
        "A still pool of dark water fills the center. Something glints beneath."},
    {RoomFlavor::MinedOut,       "Mined-Out Chamber",
        "Pick marks scar the walls. Whatever was here has been extracted."},
    {RoomFlavor::CollapseZone,   "Unstable Cavern",
        "Rubble litters the floor. The ceiling looks ready to give."},
};

static constexpr FlavorEntry rocky_corridor_flavors[] = {
    {RoomFlavor::CorridorPlain,       "Tunnel",
        "A narrow tunnel through the rock. Your footsteps echo ahead."},
    {RoomFlavor::CorridorDimLit,      "Dark Passage",
        "The darkness presses close. You can barely see a few meters ahead."},
    {RoomFlavor::CorridorMaintenance, "Bore Shaft",
        "Machine-cut walls. An old mining bore shaft."},
    {RoomFlavor::CorridorDamaged,     "Collapsed Passage",
        "Fallen rock narrows the path. You squeeze through carefully."},
};

template <std::size_t N>
static const FlavorEntry& pick_flavor(const FlavorEntry (&table)[N], std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    return table[dist(rng)];
}

void TileMap::generate_flavors(std::mt19937& rng) {
    bool first_room = true;
    for (auto& reg : regions_) {
        const FlavorEntry* entry = nullptr;

        if (reg.type == RegionType::Room) {
            // First room is the spawn point — give it a special flavor
            if (first_room && map_type_ == MapType::SpaceStation) {
                first_room = false;
                reg.flavor = RoomFlavor::EmptyRoom;
                reg.name = "Docking Bay";
                reg.enter_message = "The main docking bay. Shuttle clamps line the deck "
                                    "and the hum of life support fills the air.";
                continue;
            }
            first_room = false;

            switch (map_type_) {
                case MapType::SpaceStation:
                    entry = &pick_flavor(station_room_flavors, rng);
                    break;
                case MapType::Rocky:
                case MapType::Lava:
                    entry = &pick_flavor(rocky_room_flavors, rng);
                    break;
                case MapType::Nebula:
                    // Reuse rocky for now until nebula flavors are added
                    entry = &pick_flavor(rocky_room_flavors, rng);
                    break;
            }
        } else {
            switch (map_type_) {
                case MapType::SpaceStation:
                    entry = &pick_flavor(station_corridor_flavors, rng);
                    break;
                case MapType::Rocky:
                case MapType::Lava:
                case MapType::Nebula:
                    entry = &pick_flavor(rocky_corridor_flavors, rng);
                    break;
            }
        }

        if (entry) {
            reg.flavor = entry->flavor;
            reg.name = entry->name;
            reg.enter_message = entry->enter_message;
        }
    }
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
