#include "astra/map_generator.h"

#include <cstdlib>

namespace astra {

// =========================================================================
// Starship Generator — fixed-layout personal starship
// =========================================================================
// 4 rooms connected linearly left-to-right:
//   Cockpit (region 0) → Command Center (region 1) → Mess Hall (region 2) → Quarters (region 3)
// Corridors connect adjacent room centers horizontally.

class StarshipGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;
};

// Room definitions: {x1, y1, width, height}
struct ShipRoom {
    int x, y, w, h;
};

static constexpr ShipRoom ship_rooms[] = {
    { 2,  6, 8,  6},   // Cockpit (region 0, spawn)
    {12,  5, 12, 8},   // Command Center (region 1)
    {26,  6, 10, 6},   // Mess Hall (region 2)
    {38,  5, 10, 8},   // Quarters (region 3)
};
static constexpr int ship_room_count = 4;

void StarshipGenerator::generate_layout(std::mt19937& /*rng*/) {
    for (int i = 0; i < ship_room_count; ++i) {
        const auto& sr = ship_rooms[i];
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true;
        int rid = map_->add_region(reg);

        RoomRect room{sr.x, sr.y, sr.x + sr.w - 1, sr.y + sr.h - 1};
        carve_rect(room.x1, room.y1, room.x2, room.y2, rid);
        rooms_.push_back(room);
    }
}

void StarshipGenerator::connect_rooms(std::mt19937& /*rng*/) {
    // Connect adjacent rooms with horizontal corridors
    for (int i = 0; i < ship_room_count - 1; ++i) {
        int cy = (rooms_[i].y1 + rooms_[i].y2) / 2;
        int x1 = rooms_[i].x2;
        int x2 = rooms_[i + 1].x1;

        Region creg;
        creg.type = RegionType::Corridor;
        creg.lit = true;
        int crid = map_->add_region(creg);

        carve_corridor_h(x1, x2, cy, crid);
    }
}

// Safe fixture placement: skip impassable fixtures near doorways.
// A doorway is a floor tile on the wall perimeter of a room (carved by corridors).
using RoomRect = MapGenerator::RoomRect;

static bool is_near_doorway(const TileMap& map, const RoomRect& r, int x, int y) {
    // Check all 4 wall edges for floor tiles (doorways) within 1 tile of (x,y)
    for (int wx = r.x1; wx <= r.x2; ++wx) {
        if (map.get(wx, r.y1) == Tile::Floor && std::abs(wx - x) <= 1 && std::abs(r.y1 - y) <= 1)
            return true;
        if (map.get(wx, r.y2) == Tile::Floor && std::abs(wx - x) <= 1 && std::abs(r.y2 - y) <= 1)
            return true;
    }
    for (int wy = r.y1; wy <= r.y2; ++wy) {
        if (map.get(r.x1, wy) == Tile::Floor && std::abs(r.x1 - x) <= 1 && std::abs(wy - y) <= 1)
            return true;
        if (map.get(r.x2, wy) == Tile::Floor && std::abs(r.x2 - x) <= 1 && std::abs(wy - y) <= 1)
            return true;
    }
    return false;
}

static bool safe_place(TileMap& map, const RoomRect& r, int x, int y, FixtureData fd) {
    if (map.get(x, y) != Tile::Floor) return false;
    if (!fd.passable && is_near_doorway(map, r, x, y)) return false;
    map.add_fixture(x, y, fd);
    return true;
}

void StarshipGenerator::place_features(std::mt19937& /*rng*/) {
    // --- Cockpit (room 0) ---
    {
        const auto& r = rooms_[0];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1;
        // Viewports along north wall
        for (int x = ix1; x <= ix2; ++x) {
            safe_place(*map_, r, x, iy1, make_fixture(FixtureType::Viewport));
        }
        // Consoles in front of viewports
        if (iy1 + 1 <= r.y2 - 1) {
            for (int x = ix1; x <= ix2; x += 2) {
                safe_place(*map_, r, x, iy1 + 1, make_fixture(FixtureType::Console));
            }
        }
    }

    // --- Command Center (room 1) ---
    {
        const auto& r = rooms_[1];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;
        int cx = (ix1 + ix2) / 2;
        int cy = (iy1 + iy2) / 2;
        // StarChart in center
        safe_place(*map_, r, cx, cy, make_fixture(FixtureType::StarChart));
        // Console row below
        if (cy + 1 <= iy2) {
            for (int x = cx - 1; x <= cx + 1; ++x) {
                if (x >= ix1 && x <= ix2) {
                    safe_place(*map_, r, x, cy + 1, make_fixture(FixtureType::Console));
                }
            }
        }
    }

    // --- Mess Hall (room 2) ---
    {
        const auto& r = rooms_[2];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;
        int cx = (ix1 + ix2) / 2;
        // Table in center
        safe_place(*map_, r, cx, (iy1 + iy2) / 2, make_fixture(FixtureType::Table));
        // Stools around table
        if (cx - 1 >= ix1)
            safe_place(*map_, r, cx - 1, (iy1 + iy2) / 2, make_fixture(FixtureType::Stool));
        if (cx + 1 <= ix2)
            safe_place(*map_, r, cx + 1, (iy1 + iy2) / 2, make_fixture(FixtureType::Stool));
        // FoodTerminal on north wall
        safe_place(*map_, r, cx, iy1, make_fixture(FixtureType::FoodTerminal));
    }

    // --- Quarters (room 3) ---
    {
        const auto& r = rooms_[3];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;
        // Bunks along walls
        for (int y = iy1; y <= iy2; y += 2) {
            safe_place(*map_, r, ix1, y, make_fixture(FixtureType::Bunk));
            safe_place(*map_, r, ix2, y, make_fixture(FixtureType::Bunk));
        }
        // RestPod at the far end
        int cx = (ix1 + ix2) / 2;
        safe_place(*map_, r, cx, iy2, make_fixture(FixtureType::RestPod));
    }
}

void StarshipGenerator::assign_regions(std::mt19937& /*rng*/) {
    struct ShipRoomInfo {
        RoomFlavor flavor;
        const char* name;
        const char* enter_message;
    };

    static const ShipRoomInfo room_info[] = {
        {RoomFlavor::ShipCockpit, "Cockpit",
            "The cockpit. Stars drift beyond the viewport, navigation consoles glow softly."},
        {RoomFlavor::ShipCommandCenter, "Command Center",
            "The command center. A star chart terminal dominates the room."},
        {RoomFlavor::ShipMessHall, "Mess Hall",
            "The mess hall. A small table and food terminal — comforts of home."},
        {RoomFlavor::ShipQuarters, "Sleeping Quarters",
            "The sleeping quarters. Bunks line the walls. A rest pod hums at the far end."},
    };

    static const ShipRoomInfo corridor_info = {
        RoomFlavor::CorridorPlain, "Ship Corridor",
        "A narrow corridor connecting the ship's compartments."
    };

    int count = map_->region_count();
    int room_index = 0;

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);
        if (reg.type == RegionType::Room && room_index < ship_room_count) {
            const auto& info = room_info[room_index];
            reg.flavor = info.flavor;
            reg.name = info.name;
            reg.enter_message = info.enter_message;
            reg.features = default_features(reg.flavor);
            ++room_index;
        } else {
            reg.flavor = corridor_info.flavor;
            reg.name = corridor_info.name;
            reg.enter_message = corridor_info.enter_message;
        }
        map_->update_region(i, reg);
    }
}

std::unique_ptr<MapGenerator> make_starship_generator() {
    return std::make_unique<StarshipGenerator>();
}

} // namespace astra
