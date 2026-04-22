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
