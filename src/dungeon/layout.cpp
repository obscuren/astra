#include "astra/dungeon/layout.h"

#include "astra/room_identifier.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace astra::dungeon {

namespace {

struct Rect { int x, y, w, h; };

bool inbounds(const TileMap& m, int x, int y) {
    return x >= 0 && y >= 0 && x < m.width() && y < m.height();
}

void carve_rect(TileMap& m, const Rect& r) {
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
        }
    }
}

void carve_h(TileMap& m, int x1, int x2, int y) {
    if (x1 > x2) std::swap(x1, x2);
    for (int x = x1; x <= x2; ++x) if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
}

void carve_v(TileMap& m, int y1, int y2, int x) {
    if (y1 > y2) std::swap(y1, y2);
    for (int y = y1; y <= y2; ++y) if (inbounds(m, x, y)) m.set(x, y, Tile::Floor);
}

void bsp(std::vector<Rect>& rooms, const Rect& bounds,
         int min_room, int max_room, std::mt19937& rng, int depth) {
    const bool can_split_w = bounds.w >= 2 * (min_room + 2);
    const bool can_split_h = bounds.h >= 2 * (min_room + 2);
    const bool must_stop = depth >= 6 || (!can_split_w && !can_split_h);

    if (must_stop) {
        const int rw = std::min(max_room, bounds.w - 2);
        const int rh = std::min(max_room, bounds.h - 2);
        if (rw < min_room || rh < min_room) return;
        std::uniform_int_distribution<int> dw(min_room, rw);
        std::uniform_int_distribution<int> dh(min_room, rh);
        const int w = dw(rng);
        const int h = dh(rng);
        std::uniform_int_distribution<int> dx(bounds.x + 1, bounds.x + bounds.w - w - 1);
        std::uniform_int_distribution<int> dy(bounds.y + 1, bounds.y + bounds.h - h - 1);
        rooms.push_back({ dx(rng), dy(rng), w, h });
        return;
    }

    bool split_horizontal;
    if (can_split_w && !can_split_h)       split_horizontal = false;
    else if (!can_split_w && can_split_h)  split_horizontal = true;
    else                                   split_horizontal = (bounds.h > bounds.w);

    if (split_horizontal) {
        std::uniform_int_distribution<int> d(min_room + 2, bounds.h - (min_room + 2));
        const int cut = d(rng);
        Rect a = { bounds.x, bounds.y,       bounds.w, cut };
        Rect b = { bounds.x, bounds.y + cut, bounds.w, bounds.h - cut };
        bsp(rooms, a, min_room, max_room, rng, depth + 1);
        bsp(rooms, b, min_room, max_room, rng, depth + 1);
    } else {
        std::uniform_int_distribution<int> d(min_room + 2, bounds.w - (min_room + 2));
        const int cut = d(rng);
        Rect a = { bounds.x,       bounds.y, cut,              bounds.h };
        Rect b = { bounds.x + cut, bounds.y, bounds.w - cut,   bounds.h };
        bsp(rooms, a, min_room, max_room, rng, depth + 1);
        bsp(rooms, b, min_room, max_room, rng, depth + 1);
    }
}

void connect_rooms(TileMap& m, const std::vector<Rect>& rooms, std::mt19937& rng) {
    for (size_t i = 1; i < rooms.size(); ++i) {
        const auto& a = rooms[i - 1];
        const auto& b = rooms[i];
        int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
        int bx = b.x + b.w / 2, by = b.y + b.h / 2;
        std::uniform_int_distribution<int> flip(0, 1);
        if (flip(rng)) {
            carve_h(m, ax, bx, ay);
            carve_v(m, ay, by, bx);
        } else {
            carve_v(m, ay, by, ax);
            carve_h(m, ax, bx, by);
        }
    }
}

// Sets ctx.sanctum_region_id to the region id at the center of `terminal`.
// Caller must run tag_connected_components first.
void tag_sanctum(TileMap& map, LevelContext& ctx, const Rect& terminal) {
    ctx.sanctum_region_id =
        map.region_id(terminal.x + terminal.w / 2, terminal.y + terminal.h / 2);
}

void tag_chapels(TileMap& map, LevelContext& ctx,
                 const std::vector<Rect>& chapels) {
    ctx.chapel_region_ids.clear();
    ctx.chapel_region_ids.reserve(chapels.size());
    for (const auto& r : chapels) {
        int rid = map.region_id(r.x + r.w / 2, r.y + r.h / 2);
        if (rid >= 0) ctx.chapel_region_ids.push_back(rid);
    }
}

// Rubble-interrupted narrow corridor: carves a 1-wide line but leaves
// impassable "rubble" gaps at ~20% density along the middle 60% of the run.
void carve_corridor_broken_h(TileMap& m, int x1, int x2, int y,
                             std::mt19937& rng) {
    if (x1 > x2) std::swap(x1, x2);
    int len = x2 - x1;
    int m0 = x1 + len * 20 / 100;
    int m1 = x1 + len * 80 / 100;
    std::uniform_int_distribution<int> d(0, 99);
    for (int x = x1; x <= x2; ++x) {
        if (!inbounds(m, x, y)) continue;
        if (x > m0 && x < m1 && d(rng) < 20) {
            // leave as Wall — creates a rubble-gap feel; pathable gaps on either side
            continue;
        }
        m.set(x, y, Tile::Floor);
    }
}

void layout_precursor_vault_l1(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    const int W = map.width();
    const int H = map.height();

    // Entry room — upper-left quadrant.
    Rect entry { 2, 2, 8, 6 };
    // Terminal chamber — lower-right, medium size.
    Rect terminal { W - 12, H - 9, 10, 7 };

    carve_rect(map, entry);
    carve_rect(map, terminal);

    // 4-6 side rooms scattered between entry and terminal.
    std::uniform_int_distribution<int> dcount(4, 6);
    int n = dcount(rng);
    std::vector<Rect> side_rooms;
    side_rooms.reserve(n);
    for (int i = 0; i < n; ++i) {
        std::uniform_int_distribution<int> dw(4, 7);
        std::uniform_int_distribution<int> dh(3, 5);
        std::uniform_int_distribution<int> dx(entry.x + entry.w + 2, terminal.x - 6);
        std::uniform_int_distribution<int> dy(2, H - 8);
        int w = dw(rng), h = dh(rng);
        int x = dx(rng), y = dy(rng);
        Rect r { x, y, w, h };
        // Reject overlaps.
        bool overlap = false;
        for (const auto& o : side_rooms) {
            if (std::abs((r.x + r.w/2) - (o.x + o.w/2)) < (r.w + o.w)/2 + 1 &&
                std::abs((r.y + r.h/2) - (o.y + o.h/2)) < (r.h + o.h)/2 + 1) {
                overlap = true; break;
            }
        }
        if (overlap) continue;
        side_rooms.push_back(r);
        carve_rect(map, r);
    }

    // Processional: rubble-broken 1-wide line from entry center to terminal center.
    int ax = entry.x + entry.w / 2, ay = entry.y + entry.h / 2;
    int bx = terminal.x + terminal.w / 2, by = terminal.y + terminal.h / 2;
    carve_corridor_broken_h(map, ax, bx, ay, rng);
    carve_v(map, ay, by, bx);

    // Short 1-wide stubs from each side room to the processional.
    for (const auto& r : side_rooms) {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        carve_v(map, cy, ay, cx);
    }

    tag_connected_components(map, RegionType::Room);
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, terminal);
    // No chapels on L1.
    ctx.chapel_region_ids.clear();
}

void layout_precursor_vault_l2(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    const int W = map.width();
    const int H = map.height();

    // Entry room — left end, small.
    Rect entry { 2, H/2 - 3, 8, 6 };
    // Terminal chamber — right end, grand.
    Rect terminal { W - 14, H/2 - 5, 12, 10 };

    carve_rect(map, entry);
    carve_rect(map, terminal);

    // 3-wide central nave spanning entry -> terminal, y-centered.
    int nave_y_top = H / 2 - 1;
    int nave_x0 = entry.x + entry.w;
    int nave_x1 = terminal.x;
    for (int y = nave_y_top; y <= nave_y_top + 2; ++y) {
        carve_h(map, nave_x0, nave_x1, y);
    }

    // Symmetric chapels — 2..4 per side.
    std::uniform_int_distribution<int> dchap(2, 4);
    int per_side = dchap(rng);
    std::vector<Rect> chapels;

    int span = nave_x1 - nave_x0;
    int step = span / (per_side + 1);
    for (int i = 1; i <= per_side; ++i) {
        int x_center = nave_x0 + step * i;
        // North chapel.
        Rect north { x_center - 3, nave_y_top - 6, 6, 5 };
        // South chapel.
        Rect south { x_center - 3, nave_y_top + 3 + 1, 6, 5 };

        carve_rect(map, north);
        carve_rect(map, south);
        // 1-wide branches from chapel door to nave edge.
        carve_v(map, north.y + north.h, nave_y_top, x_center);
        carve_v(map, nave_y_top + 3, south.y, x_center);

        chapels.push_back(north);
        chapels.push_back(south);
    }

    tag_connected_components(map, RegionType::Room);

    int ax = entry.x + entry.w / 2, ay = entry.y + entry.h / 2;
    int bx = terminal.x + terminal.w / 2, by = terminal.y + terminal.h / 2;
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, terminal);
    tag_chapels(map, ctx, chapels);
}

void layout_precursor_vault_l3(TileMap& map, LevelContext& ctx,
                               std::mt19937& rng) {
    (void)rng;
    const int W = map.width();
    const int H = map.height();

    // Antechamber — left side, modest.
    Rect antechamber { 2, H/2 - 3, 8, 6 };
    // Vault — right side, dominant.
    Rect vault { W - 18, H/2 - 7, 16, 14 };

    carve_rect(map, antechamber);
    carve_rect(map, vault);

    // 3-wide ceremonial approach corridor.
    int corridor_y0 = H / 2 - 1;
    int corridor_x0 = antechamber.x + antechamber.w;
    int corridor_x1 = vault.x;
    for (int y = corridor_y0; y <= corridor_y0 + 2; ++y) {
        carve_h(map, corridor_x0, corridor_x1, y);
    }

    tag_connected_components(map, RegionType::Room);

    int ax = antechamber.x + antechamber.w / 2, ay = antechamber.y + antechamber.h / 2;
    int bx = vault.x + vault.w / 2, by = vault.y + vault.h / 2;
    ctx.entry_region_id = map.region_id(ax, ay);
    ctx.exit_region_id  = map.region_id(bx, by);
    tag_sanctum(map, ctx, vault);
    ctx.chapel_region_ids.clear();
}

void layout_precursor_vault(TileMap& map, LevelContext& ctx,
                            std::mt19937& rng) {
    switch (ctx.depth) {
    case 1: layout_precursor_vault_l1(map, ctx, rng); break;
    case 2: layout_precursor_vault_l2(map, ctx, rng); break;
    case 3: layout_precursor_vault_l3(map, ctx, rng); break;
    default:
        // Beyond L3: reuse L3 for safety (should not occur for Archive).
        layout_precursor_vault_l3(map, ctx, rng);
        break;
    }
}

void layout_bsp_rooms(TileMap& map, LevelContext& ctx, std::mt19937& rng) {
    std::vector<Rect> rooms;
    Rect full = { 1, 1, map.width() - 2, map.height() - 2 };
    bsp(rooms, full, /*min_room=*/4, /*max_room=*/10, rng, /*depth=*/0);

    for (const auto& r : rooms) carve_rect(map, r);
    connect_rooms(map, rooms, rng);

    // Tag every connected passable component as a Room region.
    // BSP connects by construction, so this produces exactly one region
    // that we then split logically by room center for entry/exit.
    tag_connected_components(map, RegionType::Room);

    if (!rooms.empty()) {
        const auto& first = rooms.front();
        const auto& last  = rooms.back();
        ctx.entry_region_id = map.region_id(first.x + first.w / 2,
                                            first.y + first.h / 2);
        ctx.exit_region_id  = map.region_id(last.x + last.w / 2,
                                            last.y + last.h / 2);
    }
}

} // namespace

void apply_layout(TileMap& map, const DungeonStyle& style,
                  const CivConfig& civ, LevelContext& ctx,
                  std::mt19937& rng) {
    (void)civ;  // layout is civ-agnostic
    switch (style.layout) {
    case LayoutKind::BSPRooms:
        layout_bsp_rooms(map, ctx, rng);
        break;
    case LayoutKind::PrecursorVault:
        layout_precursor_vault(map, ctx, rng);
        break;
    case LayoutKind::OpenCave:
    case LayoutKind::TunnelCave:
    case LayoutKind::DerelictStationBSP:
    case LayoutKind::RuinStamps:
        assert(!"layout kind not implemented in slice 1");
        break;
    }

    assert(map.region_count() >= 1 && "layout must produce >=1 region");
}

} // namespace astra::dungeon
