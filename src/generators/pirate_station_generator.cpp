#include "astra/pirate_station_generator.h"
#include "astra/map_generator.h"
#include "astra/station_type.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra {

// =========================================================================
// Pirate Station Generator
//
// A rough, dangerous station controlled by pirates. Uses a 2x3 grid on a
// 60x40 map, giving 6 fixed rooms.
//
// Room roster:
//   (0,0) DockingBay          — EmptyRoom flavor
//   (1,0) Pirate Den          — Cantina flavor
//   (0,1) Brig                — CrewQuarters flavor
//   (1,1) Captain's Quarters  — CrewQuarters flavor (pirate captain spawn site)
//   (0,2) Loot Stash          — StorageBay flavor
//   (1,2) Black Market        — MaintenanceAccess flavor
// =========================================================================

struct PirateRoomDef {
    RoomFlavor flavor;
    int col, row;
};

static constexpr PirateRoomDef pirate_rooms[] = {
    {RoomFlavor::EmptyRoom,          0, 0},  // Docking Bay
    {RoomFlavor::Cantina,            1, 0},  // Pirate Den
    {RoomFlavor::CrewQuarters,       0, 1},  // Brig
    {RoomFlavor::CrewQuarters,       1, 1},  // Captain's Quarters
    {RoomFlavor::StorageBay,         0, 2},  // Loot Stash
    {RoomFlavor::MaintenanceAccess,  1, 2},  // Black Market
};
static constexpr int pirate_room_count = 6;

// =========================================================================
// Doorway-safe fixture placement (mirrors scav_station_generator pattern)
// =========================================================================

struct PirateRoomContext {
    int ix1, iy1, ix2, iy2;
    int wx1, wy1, wx2, wy2;
    std::vector<bool> blocked;
    int iw, ih;
    TileMap* map;

    PirateRoomContext(TileMap& m, const MapGenerator::RoomRect& r)
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
// Per-flavor furnishing
// =========================================================================

static void furnish_pirate_docking_bay(PirateRoomContext& ctx) {
    if (ctx.too_small()) return;
    for (int x = ctx.ix1; x <= ctx.ix2; ++x)
        ctx.place(x, ctx.iy2, make_fixture(FixtureType::ShuttleClamp));
    ctx.place(ctx.ix1,     ctx.iy1,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1 + 1, ctx.iy1,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2,     ctx.iy1,     make_fixture(FixtureType::Crate));
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    int mid_y = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(mid_x, mid_y, make_fixture(FixtureType::ShipTerminal));
}

static void furnish_pirate_den(PirateRoomContext& ctx) {
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

static void furnish_brig(PirateRoomContext& ctx) {
    if (ctx.too_small()) return;
    // Bunks on the walls — cramped holding cell feel
    for (int y = ctx.iy1; y <= ctx.iy2; y += 2) {
        ctx.place(ctx.ix1, y, make_fixture(FixtureType::Bunk));
        ctx.place(ctx.ix2, y, make_fixture(FixtureType::Bunk));
    }
}

static void furnish_captains_quarters(PirateRoomContext& ctx) {
    if (ctx.too_small()) return;
    // Rest pod + crates suggest a commandeered living space
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    ctx.place(mid_x, ctx.iy1, make_fixture(FixtureType::RestPod));
    ctx.place(ctx.ix2, ctx.iy2,     make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2, ctx.iy2 - 1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1, ctx.iy2,     make_fixture(FixtureType::ShipTerminal));
}

static void furnish_loot_stash(PirateRoomContext& ctx) {
    if (ctx.too_small()) return;
    // Dense crate stacking — stolen cargo feel
    for (int x = ctx.ix1; x <= ctx.ix2; x += 2)
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Crate));
    for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2)
        ctx.place(x, ctx.iy2, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2, ctx.iy1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1, ctx.iy2, make_fixture(FixtureType::Crate));
}

static void furnish_black_market(PirateRoomContext& ctx) {
    if (ctx.too_small()) return;
    // Repair bench + conduits — back-room fixer feel
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::RepairBench));
    for (int x = ctx.ix1; x <= ctx.ix2; x += 3)
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Conduit));
    ctx.place(ctx.ix2, ctx.iy2, make_fixture(FixtureType::Crate));
}

static void furnish_pirate_room(TileMap& map, RoomFlavor flavor, int room_index,
                                 const MapGenerator::RoomRect& r) {
    PirateRoomContext ctx(map, r);
    if (ctx.iw <= 0 || ctx.ih <= 0) return;

    switch (flavor) {
        case RoomFlavor::EmptyRoom:
            furnish_pirate_docking_bay(ctx);
            break;
        case RoomFlavor::Cantina:
            furnish_pirate_den(ctx);
            break;
        case RoomFlavor::CrewQuarters:
            // First CrewQuarters = Brig (index 2), second = Captain's Quarters (index 3)
            if (room_index == 2)
                furnish_brig(ctx);
            else
                furnish_captains_quarters(ctx);
            break;
        case RoomFlavor::StorageBay:
            furnish_loot_stash(ctx);
            break;
        case RoomFlavor::MaintenanceAccess:
            furnish_black_market(ctx);
            break;
        default:
            break;
    }
}

// =========================================================================
// PirateStationGenerator class
// =========================================================================

class PirateStationGenerator : public MapGenerator {
public:
    explicit PirateStationGenerator(StationContext ctx) : ctx_(std::move(ctx)) {}

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

static bool is_room_floor_pirate(const TileMap* map, int x, int y, int room_rid) {
    return map->get(x, y) == Tile::Floor && map->region_id(x, y) == room_rid;
}

void PirateStationGenerator::safe_corridor_h(int x1, int x2, int y, int crid) {
    int lo = std::min(x1, x2);
    int hi = std::max(x1, x2);
    for (int x = lo; x <= hi; ++x) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;
        if (t == Tile::Floor && in_room) continue;
        if (t == Tile::Wall && in_room) {
            bool crossing = is_room_floor_pirate(map_, x - 1, y, rid) ||
                            is_room_floor_pirate(map_, x + 1, y, rid);
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

void PirateStationGenerator::safe_corridor_v(int y1, int y2, int x, int crid) {
    int lo = std::min(y1, y2);
    int hi = std::max(y1, y2);
    for (int y = lo; y <= hi; ++y) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;
        if (t == Tile::Floor && in_room) continue;
        if (t == Tile::Wall && in_room) {
            bool crossing = is_room_floor_pirate(map_, x, y - 1, rid) ||
                            is_room_floor_pirate(map_, x, y + 1, rid);
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

void PirateStationGenerator::generate_layout(std::mt19937& rng) {
    // 6 fixed rooms on a 2x3 grid within a 60x40 map.
    int map_w = map_->width();
    int map_h = map_->height();

    std::uniform_int_distribution<int> w_dist(8, 11);
    std::uniform_int_distribution<int> h_dist(6, 9);

    // Grid: 2 columns, 3 rows
    int col_width  = map_w / 2;
    int row_height = map_h / 3;

    for (int i = 0; i < pirate_room_count; ++i) {
        int rw  = w_dist(rng);
        int rh  = h_dist(rng);
        int col = pirate_rooms[i].col;
        int row = pirate_rooms[i].row;

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

void PirateStationGenerator::connect_rooms(std::mt19937& rng) {
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

void PirateStationGenerator::place_features(std::mt19937& /*rng*/) {
    for (int i = 0; i < pirate_room_count && i < static_cast<int>(rooms_.size()); ++i) {
        furnish_pirate_room(*map_, pirate_rooms[i].flavor, i, rooms_[i]);
    }
}

void PirateStationGenerator::assign_regions(std::mt19937& /*rng*/) {
    struct FlavorInfo {
        RoomFlavor flavor;
        const char* name;
        const char* enter_message;
    };

    // Two CrewQuarters rooms have distinct names; we track them by index.
    static const FlavorInfo pirate_flavor_table[] = {
        {RoomFlavor::EmptyRoom,
            "Docking Bay",
            "A battered docking bay. Stolen ships crowd the clamps."},
        {RoomFlavor::Cantina,
            "Pirate Den",
            "Smoke, shouting, and spilled liquor. The pirates call this home."},
        {RoomFlavor::CrewQuarters,
            "Brig",
            "Crude bunks serve as a holding cell. The locks look functional."},
        {RoomFlavor::StorageBay,
            "Loot Stash",
            "Crates packed floor to ceiling. Most carry no manifest."},
        {RoomFlavor::MaintenanceAccess,
            "Black Market",
            "A back-room Fixer operates here. No questions asked, no receipts given."},
    };

    // Captain's Quarters is the second CrewQuarters room.
    static const FlavorInfo captains_quarters_info = {
        RoomFlavor::CrewQuarters,
        "Captain's Quarters",
        "Sparse but private. Whoever commands this station sleeps here."
    };

    static const FlavorInfo corridor_info = {
        RoomFlavor::CorridorPlain,
        "Station Corridor",
        "Grimy corridors. Graffiti covers the walls in three different languages."
    };

    auto find_info = [&](RoomFlavor f) -> const FlavorInfo* {
        for (const auto& info : pirate_flavor_table)
            if (info.flavor == f) return &info;
        return nullptr;
    };

    int count      = map_->region_count();
    int room_index = 0;
    int crew_quarters_seen = 0;

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);

        if (reg.type == RegionType::Room) {
            if (room_index < pirate_room_count) {
                RoomFlavor f = pirate_rooms[room_index].flavor;

                if (f == RoomFlavor::CrewQuarters) {
                    // First = Brig, second = Captain's Quarters
                    if (crew_quarters_seen == 0) {
                        const FlavorInfo* brig = find_info(f);
                        if (brig) {
                            reg.flavor        = brig->flavor;
                            reg.name          = brig->name;
                            reg.enter_message = brig->enter_message;
                        }
                    } else {
                        reg.flavor        = captains_quarters_info.flavor;
                        reg.name          = captains_quarters_info.name;
                        reg.enter_message = captains_quarters_info.enter_message;
                    }
                    ++crew_quarters_seen;
                } else if (const auto* info = find_info(f)) {
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

std::unique_ptr<MapGenerator> make_pirate_station_generator(const StationContext& ctx) {
    return std::make_unique<PirateStationGenerator>(ctx);
}

} // namespace astra
