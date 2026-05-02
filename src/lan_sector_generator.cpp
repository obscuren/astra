#include "astra/lan_sector_generator.h"

#include "astra/grid_network.h"
#include "astra/rect.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace astra {

LanSizeParams compute_lan_size(int n_nodes) {
    LanSizeParams p;
    p.office_count = std::max(1, (n_nodes + 2) / 3);
    p.ring_count   = (p.office_count == 1) ? 0
                   : (p.office_count <= 4) ? 1
                   : (p.office_count <= 9) ? 2
                                            : 3;
    int sq = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(p.office_count))));
    p.width  = std::clamp(20 + 6 * sq, 24, 80);
    p.height = std::clamp(12 + 4 * sq, 14, 40);
    return p;
}

namespace {

// Per-sector packed room layout (sector-coord rect + tier metadata).
// `room_idx` back-points to LanMetadata::rooms[i] so subnet stamping
// stays in sync with the meta description after big-room-first sorting.
struct LanRoomLayout {
    Rect sector_extents;
    int  tier      = 1;
    int  room_idx  = 0;
};

// Outer firewall ring along the sector perimeter. Plan §5 says ring_count
// >= 1 stamps an outer ring; tiers add inner rings (the inner sanctum
// ring is stamped separately in stamp_inner_sanctum_ring).
void stamp_outer_ring(GridSector& sec) {
    for (int x = 0; x < sec.w; ++x) {
        sec.set(x, 0,            GridTile::Firewall);
        sec.set(x, sec.h - 1,    GridTile::Firewall);
    }
    for (int y = 0; y < sec.h; ++y) {
        sec.set(0,         y,    GridTile::Firewall);
        sec.set(sec.w - 1, y,    GridTile::Firewall);
    }
}

// Organic placement: variable-size rooms (sized by subnet count) dropped
// into the sector via greedy rejection-sampling. Bigger rooms first so
// they snag the best slots while the sector is empty. ≥2 cells of
// breathing room guaranteed between accepted rooms.
//
// Determinism: rng is seeded from meta.gen_seed so the same LAN always
// produces the same layout across save/load.
std::vector<LanRoomLayout> pack_rooms_organic(
    const std::vector<LanRoom>& rooms,
    int sector_w, int sector_h,
    int outer_inset,
    uint32_t seed) {

    std::vector<LanRoomLayout> placed;
    if (rooms.empty()) return placed;

    std::mt19937 rng(seed);

    // Size each room by its subnet count. Spec rule:
    //   sq = ceil(sqrt(max(1, n)))
    //   w  = clamp(4 + 2*sq, 5, 10)
    //   h  = clamp(3 + sq,   3, 7)
    struct PendingRoom { int w, h, idx; };
    std::vector<PendingRoom> pending;
    pending.reserve(rooms.size());
    for (size_t i = 0; i < rooms.size(); ++i) {
        int n  = static_cast<int>(rooms[i].contained_subnets.size());
        int sq = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(std::max(1, n)))));
        int w  = std::clamp(4 + 2 * sq, 5, 10);
        int h  = std::clamp(3 + sq,     3, 7);
        pending.push_back({w, h, static_cast<int>(i)});
    }
    // Big rooms first — easier placement when the sector is still empty.
    std::sort(pending.begin(), pending.end(),
              [](const PendingRoom& a, const PendingRoom& b) {
                  return (a.w * a.h) > (b.w * b.h);
              });

    auto overlaps = [&](const Rect& r) -> bool {
        for (const auto& p : placed) {
            const Rect& q = p.sector_extents;
            // Reject if rectangles touch or overlap with <2-cell gap.
            if (r.x + r.w + 2 > q.x &&
                r.x          < q.x + q.w + 2 &&
                r.y + r.h + 2 > q.y &&
                r.y          < q.y + q.h + 2) {
                return true;
            }
        }
        return false;
    };

    int min_x = outer_inset;
    int min_y = outer_inset;
    int max_x = sector_w - outer_inset;
    int max_y = sector_h - outer_inset;

    for (const auto& pr : pending) {
        // If the room can't fit at all, skip it.
        if (pr.w + 1 > max_x - min_x || pr.h + 1 > max_y - min_y) {
            continue;
        }

        bool placed_room = false;
        for (int attempt = 0; attempt < 200 && !placed_room; ++attempt) {
            int hi_x = max_x - pr.w - 1;
            int hi_y = max_y - pr.h - 1;
            if (hi_x < min_x || hi_y < min_y) break;
            int x = std::uniform_int_distribution<int>(min_x, hi_x)(rng);
            int y = std::uniform_int_distribution<int>(min_y, hi_y)(rng);
            Rect r{x, y, pr.w, pr.h};
            if (overlaps(r)) continue;

            LanRoomLayout layout;
            layout.sector_extents = r;
            layout.tier           = rooms[pr.idx].tier;
            layout.room_idx       = pr.idx;
            placed.push_back(layout);
            placed_room = true;
        }
        // If we couldn't place after 200 attempts, skip the room rather
        // than crash. Caller renders a smaller LAN; better than failing.
    }

    // Restore meta.rooms order so stamp_subnet_gateways indexes correctly.
    std::sort(placed.begin(), placed.end(),
              [](const LanRoomLayout& a, const LanRoomLayout& b) {
                  return a.room_idx < b.room_idx;
              });
    return placed;
}

// Bound a room with firewall, fill interior with floor, and punch a 1-tile
// floor doorway in the south wall when tier == 1. Tier-2/3 rooms are
// fully bounded — player must breach.exe a wall tile.
void stamp_room(GridSector& sec, const LanRoomLayout& room) {
    int x0 = room.sector_extents.x;
    int y0 = room.sector_extents.y;
    int w  = room.sector_extents.w;
    int h  = room.sector_extents.h;

    for (int x = x0; x < x0 + w; ++x) {
        sec.set(x, y0,            GridTile::Firewall);
        sec.set(x, y0 + h - 1,    GridTile::Firewall);
    }
    for (int y = y0; y < y0 + h; ++y) {
        sec.set(x0,         y,    GridTile::Firewall);
        sec.set(x0 + w - 1, y,    GridTile::Firewall);
    }
    for (int y = y0 + 1; y < y0 + h - 1; ++y) {
        for (int x = x0 + 1; x < x0 + w - 1; ++x) {
            sec.set(x, y, GridTile::Floor);
        }
    }

    if (room.tier == 1) {
        int dx = x0 + w / 2;
        sec.set(dx, y0 + h - 1, GridTile::Floor);
    }
}

// Stamp `⌬` Gateway tiles for each Subnet contained in each room.
// Each tile records its target_node_id in sec.gateway_target so breach.exe
// and mid-jack-in traversal can resolve which device the tile leads to.
//
// We index packed-by-room_idx since pack_rooms_organic restores meta order
// before returning. For variable-size rooms we wrap onto multiple rows
// when the gateway cursor runs out of horizontal space.
void stamp_subnet_gateways(GridSector& sec,
                           const LanMetadata& meta,
                           const std::vector<LanRoomLayout>& packed) {
    for (const LanRoomLayout& room : packed) {
        if (room.room_idx < 0 ||
            static_cast<size_t>(room.room_idx) >= meta.rooms.size()) {
            continue;
        }
        const LanRoom& src = meta.rooms[static_cast<size_t>(room.room_idx)];

        int gx = room.sector_extents.x + 2;
        int gy = room.sector_extents.y + 1;
        const int max_x = room.sector_extents.x + room.sector_extents.w - 1;
        const int max_y = room.sector_extents.y + room.sector_extents.h - 2;

        for (GridNodeId sid : src.contained_subnets) {
            if (gx >= max_x) {
                gx = room.sector_extents.x + 2;
                gy++;
            }
            if (gy > max_y) break;          // room full, drop overflow
            sec.set(gx, gy, GridTile::Gateway);
            sec.gateway_target.emplace(std::pair<int,int>{gx, gy}, sid);
            gx += 2;
        }
    }
}

// Stamp a no-doorway firewall ring `padding` cells outside the room's
// perimeter. Player must breach.exe a tile to enter. Only stamps on Floor
// — never overwrites Connector, Gateway, Wall, or another Firewall.
void stamp_inner_sanctum_ring(GridSector& sec,
                              const LanRoomLayout& target,
                              int padding) {
    int x0 = std::max(0,            target.sector_extents.x - padding);
    int y0 = std::max(0,            target.sector_extents.y - padding);
    int x1 = std::min(sec.w - 1,    target.sector_extents.x + target.sector_extents.w - 1 + padding);
    int y1 = std::min(sec.h - 1,    target.sector_extents.y + target.sector_extents.h - 1 + padding);

    for (int x = x0; x <= x1; ++x) {
        if (sec.at(x, y0) == GridTile::Floor) sec.set(x, y0, GridTile::Firewall);
        if (sec.at(x, y1) == GridTile::Floor) sec.set(x, y1, GridTile::Firewall);
    }
    for (int y = y0 + 1; y < y1; ++y) {
        if (sec.at(x0, y) == GridTile::Floor) sec.set(x0, y, GridTile::Firewall);
        if (sec.at(x1, y) == GridTile::Floor) sec.set(x1, y, GridTile::Firewall);
    }
    // No doorway on purpose — player must breach.
}

// ---- Manhattan A* connector routing ----------------------------------
//
// Nodes are tile indices (y*w + x). Each step costs 1; a 90° turn adds
// +0.5 to discourage zigzag and prefer right-angle bends. Floor tiles
// are passable; everything else (firewall, walls, gateways, exit nodes,
// existing connectors) is impassable so connectors don't overwrite or
// punch through structure.

struct PathStep { int x, y; };

struct AStarOpen {
    float f;
    int   idx;
    int   dir;       // 0=N, 1=E, 2=S, 3=W, 4=none (start)
    bool operator<(const AStarOpen& o) const { return f > o.f; } // min-heap via priority_queue
};

bool a_star_passable(const GridSector& sec, int x, int y) {
    if (!sec.in_bounds(x, y)) return false;
    return sec.at(x, y) == GridTile::Floor;
}

std::vector<PathStep> route_a_star(const GridSector& sec, int sx, int sy, int tx, int ty) {
    std::vector<PathStep> empty;
    if (!sec.in_bounds(sx, sy) || !sec.in_bounds(tx, ty)) return empty;
    if (!a_star_passable(sec, sx, sy)) return empty;
    if (!a_star_passable(sec, tx, ty)) return empty;

    const int W = sec.w;
    const int H = sec.h;
    const int total = W * H;

    auto idx_of = [&](int x, int y) { return y * W + x; };
    auto heuristic = [&](int x, int y) -> float {
        return static_cast<float>(std::abs(x - tx) + std::abs(y - ty));
    };

    std::vector<float> g(total, std::numeric_limits<float>::infinity());
    std::vector<int>   came_from(total, -1);
    std::vector<int>   came_dir(total, 4);
    std::vector<bool>  closed(total, false);

    std::priority_queue<AStarOpen> open;
    int start = idx_of(sx, sy);
    int goal  = idx_of(tx, ty);
    g[start] = 0.0f;
    open.push({heuristic(sx, sy), start, 4});

    const int dx[4] = { 0, 1, 0, -1 };
    const int dy[4] = {-1, 0, 1,  0 };

    while (!open.empty()) {
        AStarOpen cur = open.top();
        open.pop();
        if (closed[cur.idx]) continue;
        closed[cur.idx] = true;

        if (cur.idx == goal) break;

        int cx = cur.idx % W;
        int cy = cur.idx / W;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (!a_star_passable(sec, nx, ny)) continue;

            float step = 1.0f;
            if (cur.dir != 4 && cur.dir != d) step += 0.5f;  // turn penalty
            int nidx = idx_of(nx, ny);
            float ng = g[cur.idx] + step;
            if (ng < g[nidx]) {
                g[nidx]         = ng;
                came_from[nidx] = cur.idx;
                came_dir[nidx]  = d;
                open.push({ng + heuristic(nx, ny), nidx, d});
            }
        }
    }

    if (came_from[goal] == -1 && goal != start) return empty;

    std::vector<PathStep> path;
    int cur = goal;
    while (cur != -1) {
        path.push_back({cur % W, cur / W});
        if (cur == start) break;
        cur = came_from[cur];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void stamp_connector_path(GridSector& sec, const std::vector<PathStep>& path) {
    for (const auto& p : path) {
        if (sec.in_bounds(p.x, p.y) && sec.at(p.x, p.y) == GridTile::Floor) {
            sec.set(p.x, p.y, GridTile::Connector);
        }
    }
}

// Pick a routable launch tile next to room A facing room B. We try the
// four cardinal mid-edge points (N/E/S/W of the room's centre) and pick
// the first that's a passable floor tile. Returns (-1,-1) if none.
std::pair<int,int> pick_launch_point(const GridSector& sec,
                                     const LanRoomLayout& room,
                                     int towards_x, int towards_y) {
    const auto& r = room.sector_extents;
    int mid_x = r.x + r.w / 2;
    int mid_y = r.y + r.h / 2;

    struct Candidate { int x, y; int prio; };
    std::vector<Candidate> cands = {
        { mid_x,         r.y - 1,         0 }, // N
        { r.x + r.w,     mid_y,           0 }, // E
        { mid_x,         r.y + r.h,       0 }, // S
        { r.x - 1,       mid_y,           0 }, // W
    };
    for (auto& c : cands) {
        c.prio = std::abs(c.x - towards_x) + std::abs(c.y - towards_y);
    }
    std::sort(cands.begin(), cands.end(),
              [](const Candidate& a, const Candidate& b) { return a.prio < b.prio; });

    for (const auto& c : cands) {
        if (a_star_passable(sec, c.x, c.y)) {
            return {c.x, c.y};
        }
    }
    return {-1, -1};
}

// Run an A*-routed connector trace between consecutive packed rooms.
// Connectors avoid firewall, walls, gateways, and existing connectors.
// If A* fails (no path), the connection is silently skipped.
void stamp_connectors_organic(GridSector& sec, const std::vector<LanRoomLayout>& placed) {
    for (size_t i = 1; i < placed.size(); ++i) {
        const auto& a = placed[i - 1];
        const auto& b = placed[i];
        int b_mid_x = b.sector_extents.x + b.sector_extents.w / 2;
        int b_mid_y = b.sector_extents.y + b.sector_extents.h / 2;
        int a_mid_x = a.sector_extents.x + a.sector_extents.w / 2;
        int a_mid_y = a.sector_extents.y + a.sector_extents.h / 2;

        auto [sx, sy] = pick_launch_point(sec, a, b_mid_x, b_mid_y);
        auto [tx, ty] = pick_launch_point(sec, b, a_mid_x, a_mid_y);
        if (sx < 0 || tx < 0) continue;

        auto path = route_a_star(sec, sx, sy, tx, ty);
        stamp_connector_path(sec, path);
    }
}

// Find a DeepGridAnchor node already in the network. Returns invalid id
// if none exists. (LAN registration in lan.cpp lazy-creates the anchor
// for connected LANs, so by the time the generator runs the anchor
// should be present whenever meta.connected is true.)
GridNodeId find_deep_grid_anchor(const GridNetwork& net) {
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) return n.id;
    }
    return GridNodeId{};
}

// Stamp `⊕` DeepGridGateway in the highest-tier room (or the last
// packed room if all rooms are tier-1). Connected LANs only.
void stamp_deep_grid_gateway(GridSector& sec,
                             const GridNetwork& net,
                             const LanMetadata& meta,
                             const std::vector<LanRoomLayout>& packed) {
    if (!meta.connected) return;
    if (packed.empty()) return;

    size_t best = 0;
    for (size_t i = 1; i < packed.size(); ++i) {
        if (packed[i].tier >= packed[best].tier) best = i; // tie-break: last
    }
    const auto& room = packed[best];
    int gx = room.sector_extents.x + room.sector_extents.w / 2;
    int gy = room.sector_extents.y + room.sector_extents.h / 2;

    // Don't overwrite a subnet gateway that already landed here. Pick the
    // first interior floor we can find as a fallback.
    if (sec.at(gx, gy) != GridTile::Floor) {
        bool placed = false;
        for (int y = room.sector_extents.y + 1; !placed && y < room.sector_extents.y + room.sector_extents.h - 1; ++y) {
            for (int x = room.sector_extents.x + 1; !placed && x < room.sector_extents.x + room.sector_extents.w - 1; ++x) {
                if (sec.at(x, y) == GridTile::Floor) { gx = x; gy = y; placed = true; }
            }
        }
        if (!placed) return;
    }

    sec.set(gx, gy, GridTile::DeepGridGateway);
    GridNodeId anchor = find_deep_grid_anchor(net);
    sec.gateway_target.emplace(std::pair<int,int>{gx, gy}, anchor);
}

// Place `⊙` ExitNode jack-out somewhere on the floor outside any office.
// Sets sec.spawn_x/y to a passable tile next to ⊙ so the player lands
// safely after jack_in.
void stamp_exit_node(GridSector& sec, const std::vector<LanRoomLayout>& packed) {
    auto in_any_room = [&](int x, int y) {
        for (const auto& r : packed) {
            if (x >= r.sector_extents.x && x < r.sector_extents.x + r.sector_extents.w
             && y >= r.sector_extents.y && y < r.sector_extents.y + r.sector_extents.h) {
                return true;
            }
        }
        return false;
    };

    auto try_place = [&](int x, int y) -> bool {
        if (!sec.in_bounds(x, y)) return false;
        GridTile t = sec.at(x, y);
        if (t != GridTile::Floor && t != GridTile::Connector) return false;
        if (in_any_room(x, y)) return false;
        sec.set(x, y, GridTile::ExitNode);
        // Spawn one tile north if passable, otherwise reuse exit tile coord
        // (player can step off it).
        int sx = x;
        int sy = std::max(1, y - 1);
        if (sec.in_bounds(sx, sy) && sec.at(sx, sy) == GridTile::Floor && !in_any_room(sx, sy)) {
            sec.spawn_x = sx;
            sec.spawn_y = sy;
        } else {
            sec.spawn_x = x;
            sec.spawn_y = y;
        }
        return true;
    };

    // Preferred spot: middle bottom of the sector, just inside the outer ring.
    int cx = sec.w / 2;
    int by = sec.h - 2;
    for (int dy = 0; dy < 4; ++dy) {
        if (try_place(cx, by - dy)) return;
    }
    // Fallback: scan the interior for any out-of-room floor tile.
    for (int y = sec.h - 2; y >= 1; --y) {
        for (int x = 1; x < sec.w - 1; ++x) {
            if (try_place(x, y)) return;
        }
    }
    // Absolute fallback: corner tile.
    sec.set(1, 1, GridTile::ExitNode);
    sec.spawn_x = 1;
    sec.spawn_y = 1;
}

} // namespace

GridSector generate_lan_sector(const LanMetadata& meta, const GridNetwork& net) {
    LanSizeParams p = compute_lan_size(meta.nodes_total);

    GridSector sec;
    sec.w = p.width;
    sec.h = p.height;
    sec.tiles.assign(static_cast<size_t>(p.width) * p.height, GridTile::Floor);

    // A — outer firewall ring.
    if (p.ring_count >= 1) {
        stamp_outer_ring(sec);
    }

    // E — organic floor-plan: variable-size rooms placed via rejection
    // sampling so they don't overlap and keep ≥2 cells of breathing room.
    int outer_inset = (p.ring_count >= 1) ? 2 : 1;
    auto packed = pack_rooms_organic(meta.rooms, p.width, p.height,
                                     outer_inset, meta.gen_seed);
    for (const auto& room : packed) stamp_room(sec, room);

    // ⌬ Gateways per subnet inside their containing room. (Stamped before
    // the inner ring so the ring doesn't accidentally crush gateway tiles.)
    stamp_subnet_gateways(sec, meta, packed);

    // Inner sanctum: extra firewall ring (no doorway) around the highest-
    // tier room. Triggered when ring_count >= 2 (i.e. office_count >= 5).
    if (p.ring_count >= 2 && !packed.empty()) {
        size_t best = 0;
        for (size_t i = 1; i < packed.size(); ++i) {
            if (packed[i].tier >= packed[best].tier) best = i; // tie-break: last
        }
        stamp_inner_sanctum_ring(sec, packed[best], /*padding*/ 2);
    }

    // A — connector traces between consecutive rooms via Manhattan A*.
    // Routed AFTER the inner ring so connectors automatically detour
    // around it (the ring's firewall tiles act as obstacles).
    stamp_connectors_organic(sec, packed);

    // ⊕ DeepGridGateway — connected LANs only, in highest-tier room.
    stamp_deep_grid_gateway(sec, net, meta, packed);

    // ⊙ ExitNode + safe spawn.
    stamp_exit_node(sec, packed);

    return sec;
}

} // namespace astra
