#include "astra/map_generator.h"

#include <vector>

namespace astra {

// =========================================================================
// Doorway-safe fixture placement
// =========================================================================
// Corridors punch through room walls, creating doorway tiles on the room's
// interior perimeter. Placing an impassable fixture on or next to a doorway
// blocks the only path in/out. We detect doorways, build a 1-tile buffer
// around each one, and all furnish functions use safe_place() which checks
// this buffer before placing any impassable fixture.

struct RoomContext {
    int ix1, iy1, ix2, iy2; // interior bounds (walls excluded)
    int wx1, wy1, wx2, wy2; // full room bounds (including walls)
    std::vector<bool> blocked; // (ix2-ix1+1) * (iy2-iy1+1) bitmap
    int iw, ih;
    TileMap* map;

    RoomContext(TileMap& m, const MapGenerator::RoomRect& r)
        : ix1(r.x1 + 1), iy1(r.y1 + 1), ix2(r.x2 - 1), iy2(r.y2 - 1),
          wx1(r.x1), wy1(r.y1), wx2(r.x2), wy2(r.y2),
          iw(r.x2 - r.x1 - 1), ih(r.y2 - r.y1 - 1), map(&m) {
        if (iw <= 0 || ih <= 0) { iw = ih = 0; return; }
        blocked.resize(iw * ih, false);

        // Find doorways: floor tiles on the wall perimeter of the room rect.
        // These are wall tiles that got carved into floor by corridor carving.
        // Check all 4 wall edges of the room.
        auto mark_door_zone = [&](int door_ix, int door_iy) {
            // Mark the doorway tile + all neighbors (1-tile buffer) as blocked
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int bx = door_ix + dx;
                    int by = door_iy + dy;
                    if (bx >= 0 && bx < iw && by >= 0 && by < ih) {
                        blocked[by * iw + bx] = true;
                    }
                }
            }
        };

        // North wall (y = r.y1): doorways are floor tiles on this line
        for (int x = r.x1; x <= r.x2; ++x) {
            if (m.get(x, r.y1) == Tile::Floor) {
                mark_door_zone(x - ix1, 0);
            }
        }
        // South wall (y = r.y2)
        for (int x = r.x1; x <= r.x2; ++x) {
            if (m.get(x, r.y2) == Tile::Floor) {
                mark_door_zone(x - ix1, ih - 1);
            }
        }
        // West wall (x = r.x1)
        for (int y = r.y1; y <= r.y2; ++y) {
            if (m.get(r.x1, y) == Tile::Floor) {
                mark_door_zone(0, y - iy1);
            }
        }
        // East wall (x = r.x2)
        for (int y = r.y1; y <= r.y2; ++y) {
            if (m.get(r.x2, y) == Tile::Floor) {
                mark_door_zone(iw - 1, y - iy1);
            }
        }
    }

    // Is this interior coordinate in the doorway buffer zone?
    bool is_door_zone(int x, int y) const {
        int lx = x - ix1, ly = y - iy1;
        if (lx < 0 || lx >= iw || ly < 0 || ly >= ih) return true;
        return blocked[ly * iw + lx];
    }

    // Safe placement: only place an impassable fixture if not in door zone.
    // Passable fixtures (stools, debris) can go anywhere.
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
// Per-flavor furnishing functions
// =========================================================================

static void furnish_docking_bay(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // ShuttleClamps along the south wall
    for (int x = ctx.ix1; x <= ctx.ix2; ++x) {
        ctx.place(x, ctx.iy2, make_fixture(FixtureType::ShuttleClamp));
    }

    // Crate clusters in corners
    ctx.place(ctx.ix1, ctx.iy1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1 + 1, ctx.iy1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix1, ctx.iy1 + 1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2, ctx.iy1, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2 - 1, ctx.iy1, make_fixture(FixtureType::Crate));

    // ShipTerminal near center of docking bay
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    int mid_y = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(mid_x, mid_y, make_fixture(FixtureType::ShipTerminal));
}

static void furnish_cantina(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Bar counter row across upper third
    int bar_y = ctx.iy1 + (ctx.iy2 - ctx.iy1) / 3;
    for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; ++x) {
        ctx.place(x, bar_y, make_fixture(FixtureType::Table));
    }

    // Stools on the player side (one row below bar) — passable, always safe
    if (bar_y + 1 <= ctx.iy2) {
        for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2) {
            ctx.place(x, bar_y + 1, make_fixture(FixtureType::Stool));
        }
    }

    // FoodTerminal on wall behind counter
    if (bar_y - 1 >= ctx.iy1) {
        int mid_x = (ctx.ix1 + ctx.ix2) / 2;
        ctx.place(mid_x, bar_y - 1, make_fixture(FixtureType::FoodTerminal));
    }

    // Scattered tables in lower area
    int table_y = bar_y + 3;
    if (table_y <= ctx.iy2 - 1) {
        for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 3) {
            ctx.place(x, table_y, make_fixture(FixtureType::Table));
        }
    }
}

static void furnish_storage_bay(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Rows of shelves creating aisles — skip edges to leave path
    for (int y = ctx.iy1 + 1; y <= ctx.iy2 - 1; y += 3) {
        for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2) {
            ctx.place(x, y, make_fixture(FixtureType::Shelf));
        }
    }

    // SupplyLocker on back wall
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    ctx.place(mid_x, ctx.iy1, make_fixture(FixtureType::SupplyLocker));
}

static void furnish_crew_quarters(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Bunks along walls, leaving aisle in the middle
    for (int y = ctx.iy1; y <= ctx.iy2; y += 2) {
        ctx.place(ctx.ix1, y, make_fixture(FixtureType::Bunk));
        ctx.place(ctx.ix2, y, make_fixture(FixtureType::Bunk));
    }

    // RestPod at the far end
    int mid_x = (ctx.ix1 + ctx.ix2) / 2;
    ctx.place(mid_x, ctx.iy2, make_fixture(FixtureType::RestPod));
}

static void furnish_medbay(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // HealPods along the left wall, evenly spaced
    int pod_count = 0;
    for (int y = ctx.iy1; y <= ctx.iy2; y += 3) {
        if (pod_count >= 3) break;
        if (ctx.place(ctx.ix1, y, make_fixture(FixtureType::HealPod))) {
            ++pod_count;
        }
    }

    // Console in the center
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::Console));
}

static void furnish_engineering(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Conduits along north and south walls, spaced out
    for (int x = ctx.ix1; x <= ctx.ix2; x += 3) {
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Conduit));
        ctx.place(x, ctx.iy2, make_fixture(FixtureType::Conduit));
    }

    // RepairBench — find first open spot on second-to-last row
    for (int x = ctx.ix1; x <= ctx.ix2; ++x) {
        if (ctx.place(x, ctx.iy2 - 1, make_fixture(FixtureType::RepairBench))) {
            break;
        }
    }

    // Console near center
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::Console));
}

static void furnish_command_center(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Holotable in center (row of consoles)
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    for (int dx = -1; dx <= 1; ++dx) {
        ctx.place(cx + dx, cy, make_fixture(FixtureType::Console));
    }

    // Corner consoles
    ctx.place(ctx.ix1, ctx.iy1, make_fixture(FixtureType::Console));
    ctx.place(ctx.ix2, ctx.iy1, make_fixture(FixtureType::Console));
}

static void furnish_cargo_hold(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Crate grid with aisles — offset from edges
    for (int y = ctx.iy1 + 1; y <= ctx.iy2 - 1; y += 2) {
        for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2) {
            ctx.place(x, y, make_fixture(FixtureType::Crate));
        }
    }

    // ShuttleClamps along north wall (skips door zones automatically)
    for (int x = ctx.ix1; x <= ctx.ix2; ++x) {
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::ShuttleClamp));
    }
}

static void furnish_armory(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Racks along both long walls
    for (int y = ctx.iy1; y <= ctx.iy2; y += 2) {
        ctx.place(ctx.ix1, y, make_fixture(FixtureType::Rack));
        ctx.place(ctx.ix2, y, make_fixture(FixtureType::Rack));
    }

    // WeaponDisplay in center
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::WeaponDisplay));

    // Console near door area
    ctx.place(ctx.ix1 + 1, ctx.iy1, make_fixture(FixtureType::Console));
}

static void furnish_observatory(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Open up the north wall to space. For each tile on the wall row:
    // - Wall tiles → Empty (space view)
    // - Floor tiles (corridor doorways) → seal back to Wall so nothing
    //   leaks into the void
    // We never touch tiles ABOVE the wall row — corridors stay intact.
    int obs_rid = ctx.map->region_id(ctx.ix1, ctx.iy1);

    for (int x = ctx.wx1; x <= ctx.wx2; ++x) {
        Tile t = ctx.map->get(x, ctx.wy1);
        int rid = ctx.map->region_id(x, ctx.wy1);

        if (t == Tile::Floor) {
            // Corridor doorway — seal it to prevent void leak
            ctx.map->set(x, ctx.wy1, Tile::Wall);
        } else if (t == Tile::Wall && (rid == obs_rid || rid < 0)) {
            // Observatory's own wall — open to space
            ctx.map->set(x, ctx.wy1, Tile::Empty);
            ctx.map->set_region(x, ctx.wy1, -1);
        }
    }

    // Seed starfield on the opened wall tiles and any empty space above
    for (int x = ctx.wx1; x <= ctx.wx2; ++x) {
        for (int y = ctx.wy1; y >= 0; --y) {
            if (ctx.map->get(x, y) != Tile::Empty) break;
            uint32_t h = static_cast<uint32_t>(x * 7 + y * 13 + 0xA5);
            h ^= h >> 16; h *= 0x45d9f3b; h ^= h >> 16;
            int density = (y == ctx.wy1) ? 15 : 8;
            if (static_cast<int>(h % 100) < density) {
                char star = (h % 10 < 6) ? '.' : ((h % 10 < 9) ? '*' : '+');
                ctx.map->set_backdrop(x, y, star);
            }
        }
    }

    // Viewports along the interior north edge (now exposed to space)
    for (int x = ctx.ix1; x <= ctx.ix2; ++x) {
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Viewport));
    }

    // StarChart terminal on one side
    ctx.place(ctx.ix2, ctx.iy1 + 1, make_fixture(FixtureType::StarChart));

    // Stools facing the viewport — passable, always safe
    for (int x = ctx.ix1 + 1; x <= ctx.ix2 - 1; x += 2) {
        if (ctx.iy1 + 2 <= ctx.iy2) {
            ctx.place(x, ctx.iy1 + 2, make_fixture(FixtureType::Stool));
        }
    }
}

static void furnish_maintenance_tunnels(RoomContext& ctx) {
    if (ctx.too_small()) return;

    // Sparse industrial room: conduits, crates, and a floor hatch
    // Conduits along north wall
    for (int x = ctx.ix1; x <= ctx.ix2; x += 3) {
        ctx.place(x, ctx.iy1, make_fixture(FixtureType::Conduit));
    }

    // A couple of crates
    ctx.place(ctx.ix2, ctx.iy2, make_fixture(FixtureType::Crate));
    ctx.place(ctx.ix2 - 1, ctx.iy1, make_fixture(FixtureType::Crate));

    // DungeonHatch in the center — the entrance to the tunnels below
    int cx = (ctx.ix1 + ctx.ix2) / 2;
    int cy = (ctx.iy1 + ctx.iy2) / 2;
    ctx.place(cx, cy, make_fixture(FixtureType::DungeonHatch));
}

static void furnish_room(TileMap& map, RoomFlavor flavor,
                          const MapGenerator::RoomRect& r) {
    RoomContext ctx(map, r);
    if (ctx.iw <= 0 || ctx.ih <= 0) return;

    switch (flavor) {
        case RoomFlavor::EmptyRoom:     furnish_docking_bay(ctx); break;
        case RoomFlavor::Cantina:       furnish_cantina(ctx); break;
        case RoomFlavor::StorageBay:    furnish_storage_bay(ctx); break;
        case RoomFlavor::CrewQuarters:  furnish_crew_quarters(ctx); break;
        case RoomFlavor::Medbay:        furnish_medbay(ctx); break;
        case RoomFlavor::Engineering:   furnish_engineering(ctx); break;
        case RoomFlavor::CommandCenter: furnish_command_center(ctx); break;
        case RoomFlavor::CargoHold:     furnish_cargo_hold(ctx); break;
        case RoomFlavor::Armory:        furnish_armory(ctx); break;
        case RoomFlavor::Observatory:        furnish_observatory(ctx); break;
        case RoomFlavor::MaintenanceTunnels: furnish_maintenance_tunnels(ctx); break;
        default: break;
    }
}

// =========================================================================
// Hub Station Generator
// =========================================================================

class HubStationGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;

private:
    void safe_corridor_h(int x1, int x2, int y, int crid);
    void safe_corridor_v(int y1, int y2, int x, int crid);
};

// Fixed room flavors for the 7 deterministic rooms
static constexpr RoomFlavor hub_fixed_flavors[] = {
    RoomFlavor::EmptyRoom,          // Docking Bay (room 0)
    RoomFlavor::StorageBay,         // room 1 — always adjacent to Docking Bay
    RoomFlavor::Cantina,            // room 2
    RoomFlavor::Medbay,             // room 3
    RoomFlavor::CommandCenter,      // room 4
    RoomFlavor::Armory,             // room 5
    RoomFlavor::Observatory,        // room 6 — Nova's room
    RoomFlavor::Engineering,        // room 7 — Engineer lives here
    RoomFlavor::MaintenanceTunnels, // room 8 — tutorial dungeon entrance
};
static constexpr int hub_fixed_count = 9;

// Random flavors for the remaining rooms
static constexpr RoomFlavor hub_random_pool[] = {
    RoomFlavor::CrewQuarters,
    RoomFlavor::CargoHold,
};
static constexpr int hub_random_pool_size = 2;

void HubStationGenerator::generate_layout(std::mt19937& rng) {
    // 9 fixed rooms + 2 random rooms on a 120x80 map
    // Fixed rooms are placed on a 3x3 grid to avoid overlap.
    // Docking Bay (0,0) and Storage Bay (1,0) share the top row,
    // so connect_rooms guarantees a direct corridor between them.
    int map_w = map_->width();
    int map_h = map_->height();

    // Room sizes for fixed rooms: 10-14 wide, 8-12 tall (including walls)
    std::uniform_int_distribution<int> w_dist(10, 14);
    std::uniform_int_distribution<int> h_dist(8, 12);

    // Grid: 3 columns, 3 rows — 9 slots for 9 fixed rooms
    struct GridPos { int col; int row; };
    static constexpr GridPos grid_positions[] = {
        {0, 0}, {1, 0}, {2, 0},  // Docking Bay, Storage Bay, Cantina
        {0, 1}, {1, 1}, {2, 1},  // Medbay, Command Center, Armory
        {1, 2},                   // Observatory (centered bottom)
        {0, 2}, {2, 2},          // Engineering, Maintenance Tunnels
    };

    int col_width = map_w / 3;
    int row_height = map_h / 3;

    // Place 7 fixed rooms
    for (int i = 0; i < hub_fixed_count; ++i) {
        int rw = w_dist(rng);
        int rh = h_dist(rng);

        int col = grid_positions[i].col;
        int row = grid_positions[i].row;

        // Place room within its grid cell with some padding
        int cell_x = col * col_width + 2;
        int cell_y = row * row_height + 2;
        int max_x = (col + 1) * col_width - rw - 2;
        int max_y = (row + 1) * row_height - rh - 2;
        if (max_x <= cell_x) max_x = cell_x;
        if (max_y <= cell_y) max_y = cell_y;

        std::uniform_int_distribution<int> x_dist(cell_x, max_x);
        std::uniform_int_distribution<int> y_dist(cell_y, max_y);
        int x = x_dist(rng);
        int y = y_dist(rng);

        // Clamp to map bounds
        if (x + rw >= map_w) x = map_w - rw - 1;
        if (y + rh >= map_h) y = map_h - rh - 1;
        if (x < 1) x = 1;
        if (y < 1) y = 1;

        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true; // Hub rooms are always lit
        int rid = map_->add_region(reg);

        RoomRect room{x, y, x + rw - 1, y + rh - 1};
        carve_rect(room.x1, room.y1, room.x2, room.y2, rid);
        rooms_.push_back(room);
    }

    // Place 2 random rooms using station-style random placement
    int random_rooms = 2;
    int min_size = 6;
    int max_size = 12;
    std::uniform_int_distribution<int> rand_size(min_size, max_size);

    for (int attempt = 0; attempt < random_rooms * 8 &&
         static_cast<int>(rooms_.size()) < hub_fixed_count + random_rooms; ++attempt) {
        int w = rand_size(rng);
        int h = rand_size(rng);
        int tw = w + 2;
        int th = h + 2;
        if (tw >= map_w || th >= map_h) continue;

        std::uniform_int_distribution<int> x_dist(0, map_w - tw);
        std::uniform_int_distribution<int> y_dist(0, map_h - th);
        int x = x_dist(rng);
        int y = y_dist(rng);

        RoomRect candidate{x, y, x + tw - 1, y + th - 1};

        bool overlaps = false;
        for (const auto& r : rooms_) {
            if (candidate.x1 - 1 <= r.x2 && candidate.x2 + 1 >= r.x1 &&
                candidate.y1 - 1 <= r.y2 && candidate.y2 + 1 >= r.y1) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true;
        int rid = map_->add_region(reg);

        carve_rect(candidate.x1, candidate.y1, candidate.x2, candidate.y2, rid);
        rooms_.push_back(candidate);
    }
}

// Room-safe corridor carving. Key rules:
// - Floor in a Room: pass through silently
// - Wall in a Room crossing perpendicular: punch a single doorway
// - Wall in a Room running parallel: leave it alone
// - Empty/corridor: normal carving
//
// "Crossing" vs "parallel" for a horizontal corridor hitting a room wall:
//   crossing = tile above or below is Floor in the same room (we're entering)
//   parallel = neither side is Floor in the same room (running along the edge)

static bool is_room_floor(const TileMap* map, int x, int y, int room_rid) {
    return map->get(x, y) == Tile::Floor && map->region_id(x, y) == room_rid;
}

void HubStationGenerator::safe_corridor_h(int x1, int x2, int y, int crid) {
    int lo = std::min(x1, x2);
    int hi = std::max(x1, x2);
    for (int x = lo; x <= hi; ++x) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;

        if (t == Tile::Floor && in_room) {
            continue;
        }
        if (t == Tile::Wall && in_room) {
            // Horizontal corridor crosses east/west walls: check left/right
            bool crossing = is_room_floor(map_, x - 1, y, rid) ||
                            is_room_floor(map_, x + 1, y, rid);
            if (crossing) {
                map_->set(x, y, Tile::Floor);
            }
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

void HubStationGenerator::safe_corridor_v(int y1, int y2, int x, int crid) {
    int lo = std::min(y1, y2);
    int hi = std::max(y1, y2);
    for (int y = lo; y <= hi; ++y) {
        Tile t = map_->get(x, y);
        int rid = map_->region_id(x, y);
        bool in_room = rid >= 0 && rid < map_->region_count() &&
                       map_->region(rid).type == RegionType::Room;

        if (t == Tile::Floor && in_room) {
            continue;
        }
        if (t == Tile::Wall && in_room) {
            // Vertical corridor crosses north/south walls: check up/down
            bool crossing = is_room_floor(map_, x, y - 1, rid) ||
                            is_room_floor(map_, x, y + 1, rid);
            if (crossing) {
                map_->set(x, y, Tile::Floor);
            }
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

void HubStationGenerator::connect_rooms(std::mt19937& rng) {
    for (size_t i = 1; i < rooms_.size(); ++i) {
        int cx1 = (rooms_[i - 1].x1 + rooms_[i - 1].x2) / 2;
        int cy1 = (rooms_[i - 1].y1 + rooms_[i - 1].y2) / 2;
        int cx2 = (rooms_[i].x1 + rooms_[i].x2) / 2;
        int cy2 = (rooms_[i].y1 + rooms_[i].y2) / 2;

        Region creg;
        creg.type = RegionType::Corridor;
        creg.lit = false; // corridors require line-of-sight
        int crid = map_->add_region(creg);

        if (rng() % 2 == 0) {
            safe_corridor_h(cx1, cx2, cy1, crid);
            safe_corridor_v(cy1, cy2, cx2, crid);
        } else {
            safe_corridor_v(cy1, cy2, cx1, crid);
            safe_corridor_h(cx1, cx2, cy2, crid);
        }
    }
}

void HubStationGenerator::place_features(std::mt19937& /*rng*/) {
    // Furnish each room based on its assigned flavor
    // assign_regions hasn't run yet, so we use the known layout
    // Fixed rooms: indices 0..4 have known flavors
    for (int i = 0; i < static_cast<int>(rooms_.size()); ++i) {
        RoomFlavor flavor;
        if (i < hub_fixed_count) {
            flavor = hub_fixed_flavors[i];
        } else {
            // Random rooms get their flavor set during assign_regions
            // We'll furnish them there instead
            continue;
        }
        furnish_room(*map_, flavor, rooms_[i]);
    }
}

void HubStationGenerator::assign_regions(std::mt19937& rng) {
    // FlavorEntry struct matches the one in map_generator.cpp
    struct FlavorInfo {
        RoomFlavor flavor;
        const char* name;
        const char* enter_message;
    };

    // Fixed room flavor info
    static const FlavorInfo hub_room_info[] = {
        {RoomFlavor::EmptyRoom, "Docking Bay",
            "The main docking bay. Shuttle clamps line the deck "
            "and the hum of life support fills the air."},
        {RoomFlavor::StorageBay, "Storage Bay",
            "Shelves and containers crowd the space. "
            "A supply locker is bolted to the wall."},
        {RoomFlavor::Cantina, "The Starboard Cantina",
            "The smell of synth-brew and sizzling protein hits you. "
            "A food terminal glows on the counter."},
        {RoomFlavor::Medbay, "Medbay",
            "Medical equipment hums on standby. "
            "Healing pods line the walls, ready for use."},
        {RoomFlavor::CommandCenter, "Command Center",
            "Consoles flicker with real-time data feeds. "
            "A holographic star map dominates the room."},
        {RoomFlavor::Armory, "Armory",
            "Weapon racks stand in orderly rows. "
            "A security console monitors the entrance."},
        {RoomFlavor::Observatory, "Nova's Observatory",
            "A viewport dominates the far wall. Jupiter's swirling storms fill the view. "
            "Nova stands silhouetted against the light."},
        {RoomFlavor::Engineering, "Engineering Bay",
            "Conduits and junction boxes crowd every surface. The station's guts exposed."},
        {RoomFlavor::MaintenanceTunnels, "Maintenance Access",
            "A grimy utility room. Pipes snake along the ceiling and a heavy "
            "floor hatch leads to the tunnels below. Caution markings everywhere."},
    };

    // Random room flavor info
    static const FlavorInfo random_room_info[] = {
        {RoomFlavor::CrewQuarters, "Crew Quarters",
            "Bunks line the walls in neat rows. A rest pod glows softly at the far end."},
        {RoomFlavor::CargoHold, "Cargo Hold",
            "A cavernous hold packed with crates. Magnetic clamps secure the heavier loads."},
    };

    // Corridor info
    static const FlavorInfo corridor_info = {
        RoomFlavor::CorridorPlain, "Station Corridor",
        "A well-lit corridor connecting the station's modules."
    };

    int count = map_->region_count();
    int room_index = 0;

    std::uniform_int_distribution<int> random_pool_dist(0, hub_random_pool_size - 1);

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);

        if (reg.type == RegionType::Room) {
            if (room_index < hub_fixed_count) {
                const auto& info = hub_room_info[room_index];
                reg.flavor = info.flavor;
                reg.name = info.name;
                reg.enter_message = info.enter_message;
            } else {
                int pick = random_pool_dist(rng);
                const auto& info = random_room_info[pick];
                reg.flavor = info.flavor;
                reg.name = info.name;
                reg.enter_message = info.enter_message;

                // Furnish random rooms now
                if (room_index < static_cast<int>(rooms_.size())) {
                    furnish_room(*map_, reg.flavor, rooms_[room_index]);
                }
            }
            reg.features = default_features(reg.flavor);
            ++room_index;
        } else {
            reg.flavor = corridor_info.flavor;
            reg.name = corridor_info.name;
            reg.enter_message = corridor_info.enter_message;
        }

        map_->update_region(i, reg);
    }

    map_->set_hub(true);
}

std::unique_ptr<MapGenerator> make_hub_station_generator() {
    return std::make_unique<HubStationGenerator>();
}

} // namespace astra
