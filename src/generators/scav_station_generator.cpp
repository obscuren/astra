#include "astra/scav_station_generator.h"
#include "astra/map_generator.h"
#include "astra/station_type.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra {

// =========================================================================
// Scav Station Generator
//
// A smaller, rougher station than NormalHub. Uses a 2x3 grid (2 columns,
// 3 rows) on a 60x40 map, giving 5 fixed rooms + 1 random room.
//
// Room roster:
//   (0,0) DockingBay      — EmptyRoom flavor
//   (1,0) Mess Hall       — Cantina flavor
//   (0,1) Scrap Yard      — MaintenanceAccess flavor (utility/salvage feel)
//   (1,1) Keeper's Nook   — CrewQuarters flavor (small)
//   (0,2) Bunk Room       — CrewQuarters flavor
// =========================================================================

struct ScavRoomDef {
    RoomFlavor flavor;
    int col, row;
};

static constexpr ScavRoomDef scav_rooms[] = {
    {RoomFlavor::EmptyRoom,          0, 0},  // Docking Bay
    {RoomFlavor::Cantina,            1, 0},  // Mess Hall
    {RoomFlavor::MaintenanceAccess,  0, 1},  // Scrap Yard
    {RoomFlavor::CrewQuarters,       1, 1},  // Keeper's Nook
    {RoomFlavor::CrewQuarters,       0, 2},  // Bunk Room
};
static constexpr int scav_room_count = 5;

// =========================================================================
// Doorway-safe fixture placement (mirrors hub_station_generator pattern)
// =========================================================================

struct ScavRoomContext {
    int ix1, iy1, ix2, iy2;
    int wx1, wy1, wx2, wy2;
    std::vector<bool> blocked;
    int iw, ih;
    TileMap* map;

    ScavRoomContext(TileMap& m, const MapGenerator::RoomRect& r)
        : ix1(r.x1 + 1), iy1(r.y1 + 1), ix2(r.x2 - 1), iy2(r.y2 - 1),
          wx1(r.x1), wy1(r.y1), wx2(r.x2), wy2(r.y2),
          iw(r.x2 - r.x1 - 1), ih(r.y2 - r.y1 - 1), map(&m) {
        if (iw <= 0 || ih <= 0) { iw = ih = 0; return; }
        blocked.resize(iw * ih, false);

        auto mark_door_zone = [&](int door_ix, int door_iy) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int bx = door_ix + dx;
                    int by = door_iy + dy;
                    if (bx >= 0 && bx < iw && by >= 0 && by < ih)
                        blocked[by * iw + bx] = true;
                }
            }
        };

        for (int x = r.x1; x <= r.x2; ++x)
            if (m.get(x, r.y1) == Tile::Floor) mark_door_zone(x - ix1, 0);
        for (int x = r.x1; x <= r.x2; ++x)
            if (m.get(x, r.y2) == Tile::Floor) mark_door_zone(x - ix1, ih - 1);
        for (int y = r.y1; y <= r.y2; ++y)
            if (m.get(r.x1, y) == Tile::Floor) mark_door_zone(0, y - iy1);
        for (int y = r.y1; y <= r.y2; ++y)
            if (m.get(r.x2, y) == Tile::Floor) mark_door_zone(iw - 1, y - iy1);
    }

    bool is_door_zone(int x, int y) const {
        int lx = x - ix1, ly = y - iy1;
        if (lx < 0 || lx >= iw || ly < 0 || ly >= ih) return true;
        return blocked[ly * iw + lx];
    }

    bool place(int x, int y, FixtureData fd) {
        if (x < ix1 || x > ix2 || y < iy1 || y > iy2) return false;
        if (map->get(x, y) != Tile::Floor) return false;
        if (!fd.passable && is_door_zone(x, y)) return false;
        map->add_fixture(x, y, fd);
        return true;
    }

    bool too_small() const { return iw < 3 || ih < 3; }
};

// =========================================================================
// Per-flavor furnishing (reuses hub logic for these flavors)
// =========================================================================

static void furnish_docking_bay(ScavRoomContext& ctx) {
    if (ctx.too_small()) return;
    for (int x = ctx.ix1; x <= ctx.ix2; ++x)
        ctx.place(x, ctx.iy2, make_fixture(FixtureType::ShuttleClamp));
    ctx.place(ctx.ix1,     ctx.iy1,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1 + 1, ctx.iy1,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1,     ctx.iy1 + 1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2,     ctx.iy1,     make_fixture(FixtureType::Crate));
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    int mid_y = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(mid_x, mid_y, make_fixture(FixtureType::ShipTerminal));
}

static void furnish_mess_hall(ScavRoomContext& ctx) {
    if (ctx.too_small()) return;
    int bar_y = ctx.iy1 + (ctx.iy2 - ctx.iy1) / 3;
    for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; ++x)
        ctx.place(x, bar_y, make_fixture(FixtureType::Table));
    if (bar_y + 1 <= ctx.iy2) {
        for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2)
            ctx.place(x, bar_y + 1, make_fixture(FixtureType::Stool));
    }
    if (bar_y - 1 >= ctx.iy1) {
        int mid_x = (ctx.ix1 + ctx.ix2) / 2;
        ctx.place(mid_x, bar_y - 1, make_fixture(FixtureType::FoodTerminal));
    }
}

static void furnish_scrap_yard(ScavRoomContext& ctx) {
    if (ctx.too_small()) return;
    // Conduits and crates scattered around — salvage feel
    for (int x = ctx.ix1; x <= ctx.ix2; x += 3)
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Conduit));
    ctx.place(ctx.ix2,     ctx.iy2,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2 - 1, ctx.iy1,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1,     ctx.iy2 - 1, make_fixture(FixtureType::Crate));
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::RepairBench));
}

static void furnish_crew_quarters(ScavRoomContext& ctx) {
    if (ctx.too_small()) return;
    for (int y = ctx.iy1; y <= ctx.iy2; y += 2) {
        ctx.place(ctx.ix1, y, make_fixture(FixtureType::Bunk));
        ctx.place(ctx.ix2, y, make_fixture(FixtureType::Bunk));
    }
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    ctx.place(mid_x, ctx.iy2, make_fixture(FixtureType::RestPod));
}

static void furnish_scav_room(TileMap& map, RoomFlavor flavor,
                               const MapGenerator::RoomRect& r) {
    ScavRoomContext ctx(map, r);
    if (ctx.iw <= 0 || ctx.ih <= 0) return;

    switch (flavor) {
        case RoomFlavor::EmptyRoom:         furnish_docking_bay(ctx);  break;
        case RoomFlavor::Cantina:           furnish_mess_hall(ctx);    break;
        case RoomFlavor::MaintenanceAccess: furnish_scrap_yard(ctx);   break;
        case RoomFlavor::CrewQuarters:      furnish_crew_quarters(ctx);break;
        default: break;
    }
}

// =========================================================================
// ScavStationGenerator class
// =========================================================================

class ScavStationGenerator : public MapGenerator {
public:
    explicit ScavStationGenerator(StationContext ctx) : ctx_(std::move(ctx)) {}

protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;

private:
    void safe_corridor_h(int x1, int x2, int y, int crid);
    void safe_corridor_v(int y1, int y2, int x, int crid);

    StationContext ctx_;
};

static bool is_room_floor_scav(const TileMap* map, int x, int y, int room_rid) {
    return map->get(x, y) == Tile::Floor && map->region_id(x, y) == room_rid;
}

void ScavStationGenerator::safe_corridor_h(int x1, int x2, int y, int crid) {
    int lo = std::min(x1, x2);
    int hi = std::max(x1, x2);
    for (int x = lo; x <= hi; ++x) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;
        if (t == Tile::Floor && in_room) continue;
        if (t == Tile::Wall && in_room) {
            bool crossing = is_room_floor_scav(map_, x - 1, y, rid) ||
                            is_room_floor_scav(map_, x + 1, y, rid);
            if (crossing) map_->set(x, y, Tile::Floor);
            continue;
        }
        map_->set(x, y, Tile::Floor);
        map_->set_region(x, y, crid);
        if (map_->get(x, y - 1) == Tile::Empty) {
            map_->set(x, y - 1, Tile::Wall);
            map_->set_region(x, y - 1, crid);
        }
        if (map_->get(x, y + 1) == Tile::Empty) {
            map_->set(x, y + 1, Tile::Wall);
            map_->set_region(x, y + 1, crid);
        }
    }
}

void ScavStationGenerator::safe_corridor_v(int y1, int y2, int x, int crid) {
    int lo = std::min(y1, y2);
    int hi = std::max(y1, y2);
    for (int y = lo; y <= hi; ++y) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;
        if (t == Tile::Floor && in_room) continue;
        if (t == Tile::Wall && in_room) {
            bool crossing = is_room_floor_scav(map_, x, y - 1, rid) ||
                            is_room_floor_scav(map_, x, y + 1, rid);
            if (crossing) map_->set(x, y, Tile::Floor);
            continue;
        }
        map_->set(x, y, Tile::Floor);
        map_->set_region(x, y, crid);
        if (map_->get(x - 1, y) == Tile::Empty) {
            map_->set(x - 1, y, Tile::Wall);
            map_->set_region(x - 1, y, crid);
        }
        if (map_->get(x + 1, y) == Tile::Empty) {
            map_->set(x + 1, y, Tile::Wall);
            map_->set_region(x + 1, y, crid);
        }
    }
}

void ScavStationGenerator::generate_layout(std::mt19937& rng) {
    // 5 fixed rooms on a 2x3 grid within a 60x40 map.
    int map_w = map_->width();
    int map_h = map_->height();

    // Smaller rooms than hub: 8-11 wide, 6-9 tall (including walls)
    std::uniform_int_distribution<int> w_dist(8, 11);
    std::uniform_int_distribution<int> h_dist(6, 9);

    // Grid: 2 columns, 3 rows
    int col_width  = map_w / 2;
    int row_height = map_h / 3;

    for (int i = 0; i < scav_room_count; ++i) {
        int rw  = w_dist(rng);
        int rh  = h_dist(rng);
        int col = scav_rooms[i].col;
        int row = scav_rooms[i].row;

        int cell_x = col * col_width + 2;
        int cell_y = row * row_height + 2;
        int max_x  = (col + 1) * col_width - rw - 2;
        int max_y  = (row + 1) * row_height - rh - 2;
        if (max_x <= cell_x) max_x = cell_x;
        if (max_y <= cell_y) max_y = cell_y;

        std::uniform_int_distribution<int> x_dist(cell_x, max_x);
        std::uniform_int_distribution<int> y_dist(cell_y, max_y);
        int x = x_dist(rng);
        int y = y_dist(rng);

        if (x + rw >= map_w) x = map_w - rw - 1;
        if (y + rh >= map_h) y = map_h - rh - 1;
        if (x < 1) x = 1;
        if (y < 1) y = 1;

        Region reg;
        reg.type = RegionType::Room;
        reg.lit  = true;
        int rid  = map_->add_region(reg);

        RoomRect room{x, y, x + rw - 1, y + rh - 1};
        carve_rect(room.x1, room.y1, room.x2, room.y2, rid);
        rooms_.push_back(room);
    }
}

void ScavStationGenerator::connect_rooms(std::mt19937& rng) {
    for (size_t i = 1; i < rooms_.size(); ++i) {
        int cx1 = (rooms_[i - 1].x1 + rooms_[i - 1].x2) / 2;
        int cy1 = (rooms_[i - 1].y1 + rooms_[i - 1].y2) / 2;
        int cx2 = (rooms_[i].x1 + rooms_[i].x2) / 2;
        int cy2 = (rooms_[i].y1 + rooms_[i].y2) / 2;

        Region creg;
        creg.type = RegionType::Corridor;
        creg.lit  = false;
        int crid  = map_->add_region(creg);

        if (rng() % 2 == 0) {
            safe_corridor_h(cx1, cx2, cy1, crid);
            safe_corridor_v(cy1, cy2, cx2, crid);
        } else {
            safe_corridor_v(cy1, cy2, cx1, crid);
            safe_corridor_h(cx1, cx2, cy2, crid);
        }
    }
}

void ScavStationGenerator::place_features(std::mt19937& /*rng*/) {
    for (int i = 0; i < scav_room_count && i < static_cast<int>(rooms_.size()); ++i) {
        furnish_scav_room(*map_, scav_rooms[i].flavor, rooms_[i]);
    }
}

void ScavStationGenerator::assign_regions(std::mt19937& /*rng*/) {
    struct FlavorInfo {
        RoomFlavor flavor;
        const char* name;
        const char* enter_message;
    };

    static const FlavorInfo scav_flavor_table[] = {
        {RoomFlavor::EmptyRoom,
            "Docking Bay",
            "A grimy docking bay. Shuttle clamps line the deck, most caked in rust."},
        {RoomFlavor::Cantina,
            "Mess Hall",
            "A cramped mess hall that smells of recycled protein and stale air."},
        {RoomFlavor::MaintenanceAccess,
            "Scrap Yard",
            "Salvaged parts and broken components fill every corner. "
            "A repair bench sits buried under debris."},
        {RoomFlavor::CrewQuarters,
            "Crew Quarters",
            "Bunks line the walls in crooked rows. Someone left a light on."},
    };

    static const FlavorInfo corridor_info = {
        RoomFlavor::CorridorPlain,
        "Station Corridor",
        "A dim corridor. Cables hang loose from the ceiling panels."
    };

    auto find_info = [&](RoomFlavor f) -> const FlavorInfo* {
        for (const auto& info : scav_flavor_table)
            if (info.flavor == f) return &info;
        return nullptr;
    };

    int count      = map_->region_count();
    int room_index = 0;

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);

        if (reg.type == RegionType::Room) {
            if (room_index < scav_room_count) {
                RoomFlavor f = scav_rooms[room_index].flavor;
                if (const auto* info = find_info(f)) {
                    reg.flavor        = info->flavor;
                    reg.name          = info->name;
                    reg.enter_message = info->enter_message;
                } else {
                    reg.flavor = f;
                    reg.name   = "Unknown Room";
                    reg.enter_message = "";
                }
            }
            reg.features = default_features(reg.flavor);
            ++room_index;
        } else {
            reg.flavor        = corridor_info.flavor;
            reg.name          = corridor_info.name;
            reg.enter_message = corridor_info.enter_message;
        }

        map_->update_region(i, reg);
    }

    map_->set_hub(true);
    map_->set_tha(false);
}

std::unique_ptr<MapGenerator> make_scav_station_generator(const StationContext& ctx) {
    return std::make_unique<ScavStationGenerator>(ctx);
}

} // namespace astra
