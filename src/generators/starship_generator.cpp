#include "astra/map_generator.h"

#include <cstdlib>

namespace astra {

// =========================================================================
// Starship Generator — fixed-layout personal starship
// =========================================================================
// 4 rooms centered on a 50x20 map, laid out along the x-axis.
// The Cockpit (region 0, spawn) sits at the east end — its "nose" faces space
// through viewports integrated into the east wall. Rooms are in array order
// east→west; connect_rooms links adjacent entries.
//
//   Cockpit (0, east) → Command Center (1) → Mess Hall (2) → Quarters (3, west)

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

// Ship is centered on the 50x20 map. All rooms share cy=9 so the corridor
// threads cleanly through the whole vessel. The cockpit is 7 tall (pilot
// bench stack); other rooms are 6 or 8. Total span: x=2..47.
static constexpr ShipRoom ship_rooms[] = {
    {40, 6,  8, 7},   // Cockpit (region 0, spawn) — east end, viewports on east wall
    {26, 6, 12, 8},   // Command Center (region 1)
    {14, 7, 10, 6},   // Mess Hall (region 2)
    { 2, 6, 10, 8},   // Quarters (region 3) — west end
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
    // Rooms are laid out east-to-west in the array, so each pair's corridor
    // runs from room[i]'s WEST wall (x1) to room[i+1]'s EAST wall (x2).
    for (int i = 0; i < ship_room_count - 1; ++i) {
        int cy = (rooms_[i].y1 + rooms_[i].y2) / 2;
        int x1 = rooms_[i].x1;
        int x2 = rooms_[i + 1].x2;

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
    // --- Cockpit (room 0, east nose) ---
    // Layout (8x7 room, interior 6x5):
    //   ████████
    //   █....╘╬░    TerminalCornerTop · Console · Viewport
    //   █....o╬░    Chair             · Console · Viewport
    //   ...o.╞╬░    corridor doorway (west), side Chair, TerminalSide · Console
    //   █....o╠░    Chair             · TerminalJunction · Viewport
    //   █▣...╒╬░    ARIA . . . TerminalCornerBot · Console · Viewport
    //   ████████
    //
    // Chairs (Stool) are passable. Terminal variants are impassable console
    // box-drawing glyphs forming a continuous column.
    {
        const auto& r = rooms_[0];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;

        // East wall: Viewports replace each wall tile.
        for (int y = iy1; y <= iy2; ++y) {
            if (map_->get(r.x2, y) != Tile::Wall) continue;
            map_->add_fixture(r.x2, y, make_fixture(FixtureType::Viewport));
        }

        // Terminal column at ix2 (one west of viewports) — amber.
        // Rows aligned with stools (iy1+1 and iy1+3) use TerminalJunction (╠),
        // everything else uses TerminalCenter (╬) so the column reads as a
        // continuous run of active controls.
        for (int y = iy1; y <= iy2; ++y) {
            FixtureType ft = (y == iy1 + 1 || y == iy1 + 3)
                                 ? FixtureType::TerminalJunction
                                 : FixtureType::TerminalCenter;
            safe_place(*map_, r, ix2, y, make_fixture(ft));
        }

        // Terminal bench column at ix2-1 — box-drawing chars at rows 0/2/4
        // of the interior, passable Chairs (Stool) at rows 1/3.
        int bench_x = ix2 - 1;
        safe_place(*map_, r, bench_x, iy1,     make_fixture(FixtureType::TerminalCornerTop));
        safe_place(*map_, r, bench_x, iy1 + 1, make_fixture(FixtureType::Stool));
        safe_place(*map_, r, bench_x, iy1 + 2, make_fixture(FixtureType::TerminalSide));
        safe_place(*map_, r, bench_x, iy1 + 3, make_fixture(FixtureType::Stool));
        safe_place(*map_, r, bench_x, iy2,     make_fixture(FixtureType::TerminalCornerBot));

        // Side chair near the corridor entry (col 3 in the drawing).
        safe_place(*map_, r, ix1 + 2, iy1 + 2, make_fixture(FixtureType::Stool));

        // ARIA (CommandTerminal) in the SW interior corner.
        safe_place(*map_, r, ix1, iy2, make_fixture(FixtureType::CommandTerminal));
    }

    // --- Command Center (room 1) ---
    {
        const auto& r = rooms_[1];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;
        int cx = (ix1 + ix2) / 2;
        int cy = (iy1 + iy2) / 2;
        // StarChart projector — 3 tiles ( * ) centered on the room.
        safe_place(*map_, r, cx - 1, cy, make_fixture(FixtureType::StarChartL));
        safe_place(*map_, r, cx,     cy, make_fixture(FixtureType::StarChart));
        safe_place(*map_, r, cx + 1, cy, make_fixture(FixtureType::StarChartR));
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
    // Layout (10x6 room):
    //   ██████████
    //   █.......$█   FoodTerminal in NE interior corner
    //   ..........   corridor passage (doorways on east & west)
    //   █...║¤║..█   booth: Bench | Table | Bench
    //   █o..║¤║..█   Kitchen in SW interior corner, booth continues
    //   ██████████
    {
        const auto& r = rooms_[2];
        int ix1 = r.x1 + 1, iy1 = r.y1 + 1, ix2 = r.x2 - 1, iy2 = r.y2 - 1;

        // FoodTerminal in the NE interior corner
        safe_place(*map_, r, ix2, iy1, make_fixture(FixtureType::FoodTerminal));

        // Booth — bench / table / bench — spanning the two south rows.
        // Positioned ~center of the interior so the corridor row above stays
        // clear of obstructions.
        int booth_x1 = ix1 + 3;  // left bench column
        int booth_x2 = ix1 + 5;  // right bench column
        int booth_tx = ix1 + 4;  // table column (center)
        for (int y = iy2 - 1; y <= iy2; ++y) {
            safe_place(*map_, r, booth_x1, y, make_fixture(FixtureType::Bench));
            safe_place(*map_, r, booth_tx, y, make_fixture(FixtureType::Table));
            safe_place(*map_, r, booth_x2, y, make_fixture(FixtureType::Bench));
        }

        // Kitchen in the SW interior corner (interactable, future cooking).
        safe_place(*map_, r, ix1, iy2, make_fixture(FixtureType::Kitchen));
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
