#include "astra/lan_sector_generator.h"

#include "astra/grid_room_templates.h"
#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <climits>
#include <optional>
#include <random>
#include <string>

namespace astra {

// ---------------------------------------------------------------------------
// Phase 1 — sector sizing
// ---------------------------------------------------------------------------

LanV2SizeParams compute_lan_v2_size(const LanMetadata& meta) {
    int subnets = 0;
    int distinct_tiers = 0;
    bool seen_tier[4] = {false, false, false, false};
    for (const LanZone& r : meta.zones) {
        subnets += static_cast<int>(r.contained_subnets.size());
        if (r.tier >= 1 && r.tier <= 3 && !seen_tier[r.tier]) {
            seen_tier[r.tier] = true;
            distinct_tiers++;
        }
    }

    LanV2SizeParams p;
    p.zone_count = std::max(1, std::min(3, distinct_tiers));

    // Size table — widths grown ~30-35% to accommodate 2:1 room footprints.
    // Heights unchanged so the sector still fits typical terminal windows.
    //  1-3   subnets → 40×16
    //  4-7             70×22
    //  8-14            100×28
    //  15-25           130×34
    //  26+             160×42
    if      (subnets <= 3)  { p.width = 40;  p.height = 16; }
    else if (subnets <= 7)  { p.width = 70;  p.height = 22; }
    else if (subnets <= 14) { p.width = 100; p.height = 28; }
    else if (subnets <= 25) { p.width = 130; p.height = 34; }
    else                    { p.width = 160; p.height = 42; }
    return p;
}

// ---------------------------------------------------------------------------
// Tier helper — shared by Phase 2 and Phase 3
// ---------------------------------------------------------------------------

// Plan 8 bug-fix: derive tier from Hackable.security_tier, but spread
// synthetically when all Hackables in the LAN are tier 1 (the common case for
// current fixture-placement code, which hardcodes tier=1 in tilemap.cpp). The
// spread is hash-deterministic so re-jacking the same LAN looks identical.
// Real T2/T3 fixtures take priority over the synthetic spread.
static int effective_tier_for_subnet(GridNodeId sub, const Hackable* h) {
    int base = h ? h->security_tier : 1;
    if (base < 1) base = 1;
    if (base > 3) base = 3;
    if (base > 1) return base;  // honor real T2/T3 data when present
    // Synthetic spread: 50% T1, 30% T2, 20% T3, deterministic from sub.value.
    uint32_t bucket = (sub.value * 2654435761u) % 100u;  // Knuth multiplicative hash
    if (bucket < 20) return 3;
    if (bucket < 50) return 2;
    return 1;
}

// ---------------------------------------------------------------------------
// Phase 2 — zone partition
// ---------------------------------------------------------------------------

// Zones are emitted in tier order (T1 → T2 → T3) and laid out left-to-right
// in the sector. carve_inter_zone_bridges relies on this: zones[i] and
// zones[i-1] are spatially adjacent. If you change zone ordering, update
// carve_inter_zone_bridges to find spatial neighbours by x-overlap instead
// of by index.
//
// Bug-3 fix: tier presence is derived from each subnet's Hackable.security_tier,
// NOT from LanZone.tier (which cluster_rooms promoted at most one room beyond T1).
// Bug-2 fix: banners are generic tier names ("LOBBY"/"OPERATIONS"/"VAULT").
std::vector<LanV2ZoneRegion> partition_zones(const LanMetadata& meta,
                                              const LanV2SizeParams& size,
                                              const GridNetwork& net,
                                              const WorldManager& world) {
    std::vector<LanV2ZoneRegion> zones;

    // Scan each subnet's Hackable.security_tier to discover which tiers are
    // actually present. T1 is always present (lobby invariant).
    bool seen[4] = {false, false, true, false};  // index 0 unused; T1 always true
    for (const LanZone& lz : meta.zones) {
        for (GridNodeId sub : lz.contained_subnets) {
            const GridNode* node = net.find(sub);
            if (!node || node->kind != GridNodeKind::Subnet) continue;
            auto ip = parse_ip(node->label);
            if (!ip) continue;
            const Hackable* h = world.find_hackable_by_ip(*ip);
            int t = effective_tier_for_subnet(sub, h);
            if (t >= 1 && t <= 3) seen[t] = true;
        }
    }

    int active_tiers = 0;
    for (int t = 1; t <= 3; ++t) if (seen[t]) active_tiers++;
    if (active_tiers == 0) active_tiers = 1; // empty LAN — place a lobby anyway

    // Zone widths split horizontally:
    //  1 zone → full width
    //  2 zones → 40% / 60%
    //  3 zones → 30% / 40% / 30%
    int widths[3] = {0, 0, 0};
    if (active_tiers == 1) {
        widths[0] = size.width;
    } else if (active_tiers == 2) {
        widths[0] = size.width * 4 / 10;
        widths[1] = size.width - widths[0];
    } else {
        widths[0] = size.width * 3 / 10;
        widths[2] = size.width * 3 / 10;
        widths[1] = size.width - widths[0] - widths[2];
    }

    // Generic tier banners (Bug 2 fix: no flavour names from LanZone.name).
    auto tier_banner = [](int t) -> std::string {
        switch (t) {
            case 1: return "LOBBY";
            case 2: return "OPERATIONS";
            case 3: return "VAULT";
            default: return "ZONE";
        }
    };

    int slot = 0;
    int x_cursor = 0;
    for (int t = 1; t <= 3; ++t) {
        if (!seen[t]) continue;

        LanV2ZoneRegion z;
        z.x        = x_cursor;
        z.y        = 0;
        z.w        = widths[slot];
        z.h        = size.height;
        z.tier     = t;
        z.anchor_x = x_cursor + widths[slot] / 2;
        z.anchor_y = size.height / 2;
        z.name     = tier_banner(t);
        zones.push_back(z);

        x_cursor += widths[slot];
        slot++;
        if (slot >= 3) break;
    }

    if (zones.empty()) {
        // Safety: at least one zone for the lobby invariant.
        LanV2ZoneRegion z;
        z.x = 0; z.y = 0; z.w = size.width; z.h = size.height;
        z.tier = 1; z.anchor_x = size.width / 2; z.anchor_y = size.height / 2;
        z.name = "LOBBY";
        zones.push_back(z);
    }

    return zones;
}

// ---------------------------------------------------------------------------
// Phase 3 — room placement
// ---------------------------------------------------------------------------

namespace {

struct LanV2RoomFootprint { int w, h; };

// Returns true iff (x, y) lies on the perimeter of room r.
static bool is_on_perimeter(const LanV2Room& r, int x, int y) {
    bool in_col  = (x >= r.x && x < r.x + r.w);
    bool in_row  = (y >= r.y && y < r.y + r.h);
    bool on_edge = (x == r.x || x == r.x + r.w - 1 ||
                    y == r.y || y == r.y + r.h - 1);
    return in_col && in_row && on_edge;
}

// Returns true iff every cell along the L-path (sx,sy)→(tx,sy)→(tx,ty)
// is either Wall, Floor (prior corridor — safe to share), or a Firewall cell
// belonging to the perimeter of room a or room b. A Firewall from any other
// room indicates the path crosses a third room's wall — return false to
// prevent stray openings in dense LANs.
static bool long_bridge_path_clear(const GridSector& sec,
                                    const LanV2Room& a, const LanV2Room& b,
                                    int sx, int sy, int tx, int ty) {
    // Walk the same L-path that carve_long_bridge carves:
    // horizontal segment first, then vertical.
    auto cell_ok = [&](int x, int y) -> bool {
        GridTile t = sec.at(x, y);
        if (t == GridTile::Void)  return true;  // Plan 8: v2 out-of-room background
        if (t == GridTile::Wall)  return true;  // v1 structural wall (legacy safety)
        if (t == GridTile::Floor) return true;  // prior corridor — safe to share
        if (t == GridTile::Firewall) {
            // Acceptable only if it belongs to endpoint a or b.
            return is_on_perimeter(a, x, y) || is_on_perimeter(b, x, y);
        }
        // Door or anything unexpected (third room) — reject.
        return false;
    };

    int cx = sx, cy = sy;
    // Horizontal leg.
    while (cx != tx) {
        if (!cell_ok(cx, cy)) return false;
        cx += (tx > cx) ? 1 : -1;
    }
    // Vertical leg (includes corner cell at (tx,sy) and final cell (tx,ty)).
    while (cy != ty) {
        if (!cell_ok(cx, cy)) return false;
        cy += (ty > cy) ? 1 : -1;
    }
    return cell_ok(tx, ty);
}

LanV2RoomFootprint pick_default_footprint(int tier, bool is_lobby) {
    // ~2:1 width:height to compensate for terminal monospace cells being
    // taller than wide. Lobby is 18×7 for a grand hub feel.
    if (is_lobby)  return {18, 7};
    if (tier == 1) return { 9, 4};
    if (tier == 2) return {11, 5};
    if (tier == 3) return {12, 5};
    return {7, 4};
}

// Plan 8 Cut 5: pick a footprint sized from the room's template kind.
LanV2RoomFootprint pick_footprint_for_subnet(const Hackable& h, std::mt19937& rng) {
    auto kind = choose_template_for_tags(h.tags);
    auto sz   = template_size_constraints(kind);
    int  w    = std::uniform_int_distribution<int>(sz.min_w, sz.max_w)(rng);
    int  hh   = std::uniform_int_distribution<int>(sz.min_h, sz.max_h)(rng);
    return {w, hh};
}

// ---------------------------------------------------------------------------
// Cut 4 — helper: look up Hackable* for a Subnet GridNodeId
// ---------------------------------------------------------------------------

// Resolves the Subnet node's label (IP string) → Hackable*.
// Returns nullptr if the node isn't found, isn't a Subnet, or has no IP.
static const Hackable* find_hackable_for_subnet(const WorldManager& world,
                                                const GridNetwork& net,
                                                GridNodeId subnet_id) {
    const GridNode* node = net.find(subnet_id);
    if (!node) return nullptr;
    if (node->kind != GridNodeKind::Subnet) return nullptr;
    auto ip = parse_ip(node->label);
    if (!ip) return nullptr;
    return world.find_hackable_by_ip(*ip);
}

} // anonymous namespace

std::vector<LanV2Room> place_rooms(const LanMetadata& meta,
                                    const std::vector<LanV2ZoneRegion>& zones,
                                    const GridNetwork& net,
                                    const WorldManager& world,
                                    uint32_t seed) {
    std::vector<LanV2Room> placed;
    std::mt19937 rng(seed);

    // Place lobby first — T1 zone anchor.
    int t1_zone_idx = -1;
    for (size_t i = 0; i < zones.size(); ++i) {
        if (zones[i].tier == 1) { t1_zone_idx = static_cast<int>(i); break; }
    }
    if (t1_zone_idx < 0) {
        // Defensive: synth a T1 zone (shouldn't happen — invariant).
        return placed;
    }
    {
        const auto& z = zones[t1_zone_idx];
        auto fp = pick_default_footprint(1, /*is_lobby=*/true);
        LanV2Room lobby;
        lobby.x             = z.x + (z.w - fp.w) / 2;
        lobby.y             = z.y + (z.h - fp.h) / 2;
        lobby.w             = fp.w;
        lobby.h             = fp.h;
        lobby.tier          = 1;
        lobby.is_lobby      = true;
        lobby.is_zone_anchor = true;
        lobby.zone_index    = t1_zone_idx;
        placed.push_back(lobby);
    }

    // Overlap check: rooms must have a ≥1-cell gap from any already-placed room.
    auto overlaps = [&](int x, int y, int w, int h) {
        for (const auto& r : placed) {
            if (x + w + 1 > r.x &&
                x          < r.x + r.w + 1 &&
                y + h + 1 > r.y &&
                y          < r.y + r.h + 1) {
                return true;
            }
        }
        return false;
    };

    // Bug-3 fix: iterate ALL subnets across all LanZones and route each one
    // to the zone matching its Hackable's security_tier (NOT LanZone.tier,
    // which cluster_rooms set to mostly T1 with only one promoted room).
    std::vector<bool> zone_has_anchor(zones.size(), false);
    zone_has_anchor[static_cast<size_t>(t1_zone_idx)] = true; // lobby already anchors T1

    for (const LanZone& lr : meta.zones) {
        for (GridNodeId sub : lr.contained_subnets) {
            const Hackable* h = find_hackable_for_subnet(world, net, sub);

            // Derive tier via synthetic spread when all fixtures are T1.
            int tier = effective_tier_for_subnet(sub, h);

            // Find the zone matching this tier; fall back to T1 zone.
            int dest_zi = t1_zone_idx;
            for (size_t zi = 0; zi < zones.size(); ++zi) {
                if (zones[zi].tier == tier) {
                    dest_zi = static_cast<int>(zi);
                    break;
                }
            }
            const auto& dest = zones[static_cast<size_t>(dest_zi)];

            auto fp = h ? pick_footprint_for_subnet(*h, rng)
                        : pick_default_footprint(tier, /*is_lobby=*/false);
            bool placed_room = false;
            for (int attempt = 0; attempt < 200 && !placed_room; ++attempt) {
                int hi_x = dest.x + dest.w - fp.w - 1;
                int hi_y = dest.y + dest.h - fp.h - 1;
                if (hi_x <= dest.x + 1 || hi_y <= dest.y + 1) break;
                int x = std::uniform_int_distribution<int>(dest.x + 1, hi_x)(rng);
                int y = std::uniform_int_distribution<int>(dest.y + 1, hi_y)(rng);
                if (overlaps(x, y, fp.w, fp.h)) continue;
                LanV2Room r;
                r.x              = x;
                r.y              = y;
                r.w              = fp.w;
                r.h              = fp.h;
                r.tier           = tier;
                r.source_subnet  = sub;
                r.is_lobby       = false;
                r.is_zone_anchor = !zone_has_anchor[static_cast<size_t>(dest_zi)];
                r.zone_index     = dest_zi;
                placed.push_back(r);
                if (!zone_has_anchor[static_cast<size_t>(dest_zi)])
                    zone_has_anchor[static_cast<size_t>(dest_zi)] = true;
                placed_room = true;
            }
            // Drop overflow if not placed — tiny LANs won't hit this.
        }
    }
    return placed;
}

// ---------------------------------------------------------------------------
// Phase 4 — Connectivity: bridge helpers
// ---------------------------------------------------------------------------

// Carve a 3-tile bridge between two rooms placed 1 cell apart.
// Returns true on success. Picks the closest pair of facing wall cells
// and stamps: opening_a (Floor), bridge (Door), opening_b (Floor).
// If `locked`, registers the bridge in sec.locked_doors.
static bool carve_bridge(GridSector& sec,
                         const LanV2Room& a, const LanV2Room& b,
                         bool locked) {
    int a_left = a.x, a_right = a.x + a.w - 1;
    int b_left = b.x, b_right = b.x + b.w - 1;
    int a_top = a.y,  a_bot   = a.y + a.h - 1;
    int b_top = b.y,  b_bot   = b.y + b.h - 1;

    // Horizontal: A is left of B (gap = b.x - (a.x + a.w) == 1)
    if (b_left - a_right == 2) {  // exactly 1-cell gap between walls
        int y_lo = std::max(a_top + 1, b_top + 1);
        int y_hi = std::min(a_bot - 1, b_bot - 1);
        if (y_hi < y_lo) return false;
        int y = (y_lo + y_hi) / 2;
        sec.set(a_right,     y, GridTile::Floor);   // opening in A's right wall
        sec.set(a_right + 1, y, GridTile::Door);    // bridge tile (gap cell)
        sec.set(b_left,      y, GridTile::Floor);   // opening in B's left wall
        if (locked) sec.locked_doors.insert({a_right + 1, y});
        return true;
    }
    if (a_left - b_right == 2) return carve_bridge(sec, b, a, locked); // swap

    // Vertical: A above B
    if (b_top - a_bot == 2) {
        int x_lo = std::max(a_left + 1, b_left + 1);
        int x_hi = std::min(a_right - 1, b_right - 1);
        if (x_hi < x_lo) return false;
        int x = (x_lo + x_hi) / 2;
        sec.set(x, a_bot,     GridTile::Floor);     // opening in A's bottom wall
        sec.set(x, a_bot + 1, GridTile::Door);      // bridge tile (gap cell)
        sec.set(x, b_top,     GridTile::Floor);     // opening in B's top wall
        if (locked) sec.locked_doors.insert({x, a_bot + 1});
        return true;
    }
    if (a_top - b_bot == 2) return carve_bridge(sec, b, a, locked);

    // Not 1-cell-gap adjacent → fail (caller falls back to longer corridor).
    return false;
}

// 5+ tile bridge for rooms placed further apart. Carves an L-shaped path
// of Floor between the rooms (horizontal leg first, then vertical), with
// one Door tile at the L's corner — guaranteed to be on the corridor.
// Returns false if the path crosses a third room, leaving the sector intact.
static bool carve_long_bridge(GridSector& sec,
                               const LanV2Room& a, const LanV2Room& b,
                               bool locked) {
    int a_cx = a.x + a.w / 2, a_cy = a.y + a.h / 2;
    int b_cx = b.x + b.w / 2, b_cy = b.y + b.h / 2;

    int dx = b_cx - a_cx, dy = b_cy - a_cy;
    int sx, sy, tx, ty;
    int a_wall_x = 0, a_wall_y = 0, b_wall_x = 0, b_wall_y = 0;
    bool horiz_exit;
    if (std::abs(dx) >= std::abs(dy)) {
        // Horizontal exit: pick the wall on A's side facing B, and B's side facing A.
        sx = (dx > 0) ? a.x + a.w     : a.x - 1;
        sy = a_cy;
        tx = (dx > 0) ? b.x - 1       : b.x + b.w;
        ty = b_cy;
        a_wall_x = (dx > 0) ? a.x + a.w - 1 : a.x;
        b_wall_x = (dx > 0) ? b.x            : b.x + b.w - 1;
        horiz_exit = true;
    } else {
        // Vertical exit.
        sx = a_cx;
        sy = (dy > 0) ? a.y + a.h     : a.y - 1;
        tx = b_cx;
        ty = (dy > 0) ? b.y - 1       : b.y + b.h;
        a_wall_y = (dy > 0) ? a.y + a.h - 1 : a.y;
        b_wall_y = (dy > 0) ? b.y            : b.y + b.h - 1;
        horiz_exit = false;
    }

    // Collision-check: refuse to carve if the L-path crosses a third room.
    if (!long_bridge_path_clear(sec, a, b, sx, sy, tx, ty))
        return false;

    // Path is clear — stamp wall openings and carve the corridor.
    if (horiz_exit) {
        sec.set(a_wall_x, sy, GridTile::Floor);
        sec.set(b_wall_x, ty, GridTile::Floor);
    } else {
        sec.set(sx, a_wall_y, GridTile::Floor);
        sec.set(tx, b_wall_y, GridTile::Floor);
    }

    // L-shape between (sx,sy) and (tx,ty): horizontal run then vertical turn.
    int cx = sx, cy = sy;
    while (cx != tx) {
        sec.set(cx, cy, GridTile::Floor);
        cx += (tx > cx) ? 1 : -1;
    }
    while (cy != ty) {
        sec.set(cx, cy, GridTile::Floor);
        cy += (ty > cy) ? 1 : -1;
    }
    sec.set(tx, ty, GridTile::Floor);

    // Place Door at the L's corner cell — where the horizontal leg meets the
    // vertical turn. This is always (tx, sy): we walked x from sx to tx at
    // height sy, then pivoted. This is guaranteed to be on the corridor and
    // never inside an endpoint room.
    int door_x = tx;
    int door_y = sy;
    sec.set(door_x, door_y, GridTile::Door);
    if (locked) sec.locked_doors.insert({door_x, door_y});
    return true;
}

// Combined carve dispatch: try 1-cell-gap bridge first, fall back to long bridge.
static bool carve_any_bridge(GridSector& sec,
                              const LanV2Room& a, const LanV2Room& b,
                              bool locked) {
    if (carve_bridge(sec, a, b, locked)) return true;
    return carve_long_bridge(sec, a, b, locked);
}

// ---------------------------------------------------------------------------
// Phase 4 — Connectivity: MST + bonus edges
// ---------------------------------------------------------------------------

// Manhattan distance between two rooms' centers.
static int room_dist(const LanV2Room& a, const LanV2Room& b) {
    int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int bx = b.x + b.w / 2, by = b.y + b.h / 2;
    return std::abs(ax - bx) + std::abs(ay - by);
}

// Prim's MST for rooms within a given zone.
static std::vector<LanV2Edge> zone_mst(const std::vector<LanV2Room>& rooms,
                                       int zone_idx) {
    std::vector<int> in_zone;
    for (size_t i = 0; i < rooms.size(); ++i) {
        if (rooms[i].zone_index == zone_idx) in_zone.push_back(static_cast<int>(i));
    }
    std::vector<LanV2Edge> mst;
    if (in_zone.size() < 2) return mst;

    std::vector<bool> in_tree(in_zone.size(), false);
    in_tree[0] = true;
    while (true) {
        int best_dist = INT_MAX;
        int best_u = -1, best_v = -1;
        for (size_t u = 0; u < in_zone.size(); ++u) {
            if (!in_tree[u]) continue;
            for (size_t v = 0; v < in_zone.size(); ++v) {
                if (in_tree[v]) continue;
                int d = room_dist(rooms[in_zone[u]], rooms[in_zone[v]]);
                if (d < best_dist) { best_dist = d; best_u = static_cast<int>(u); best_v = static_cast<int>(v); }
            }
        }
        if (best_v < 0) break;
        mst.push_back({in_zone[best_u], in_zone[best_v], best_dist});
        in_tree[static_cast<size_t>(best_v)] = true;
    }
    return mst;
}

// Up to 2 extra edges per anchor (or lobby) — pick shortest non-tree neighbors.
static std::vector<LanV2Edge> bonus_edges(const std::vector<LanV2Room>& rooms,
                                          const std::vector<LanV2Edge>& mst,
                                          int max_bonus_per_anchor = 2) {
    std::vector<LanV2Edge> bonus;
    auto in_mst = [&](int a, int b) {
        for (const auto& e : mst) {
            if ((e.from_idx == a && e.to_idx == b) ||
                (e.from_idx == b && e.to_idx == a)) return true;
        }
        return false;
    };
    auto already_bonus = [&](int a, int b) {
        for (const auto& e : bonus) {
            if ((e.from_idx == a && e.to_idx == b) ||
                (e.from_idx == b && e.to_idx == a)) return true;
        }
        return false;
    };

    for (size_t i = 0; i < rooms.size(); ++i) {
        if (!rooms[i].is_lobby && !rooms[i].is_zone_anchor) continue;
        std::vector<std::pair<int,int>> cand; // {dist, j}
        for (size_t j = 0; j < rooms.size(); ++j) {
            if (i == j) continue;
            if (rooms[j].zone_index != rooms[i].zone_index) continue;
            if (in_mst(static_cast<int>(i), static_cast<int>(j)) ||
                already_bonus(static_cast<int>(i), static_cast<int>(j))) continue;
            cand.push_back({room_dist(rooms[i], rooms[j]), static_cast<int>(j)});
        }
        std::sort(cand.begin(), cand.end());
        int take = std::min(max_bonus_per_anchor, static_cast<int>(cand.size()));
        for (int k = 0; k < take; ++k) {
            bonus.push_back({static_cast<int>(i), cand[k].second, cand[k].first});
        }
    }
    return bonus;
}

// ---------------------------------------------------------------------------
// Phase 4b — Inter-zone locked bridges (one per adjacent zone pair)
// ---------------------------------------------------------------------------

static void carve_inter_zone_bridges(GridSector& sec,
                                     const std::vector<LanV2Room>& rooms,
                                     const std::vector<LanV2ZoneRegion>& zones) {
    // For each adjacent zone pair (i-1, i), pick the closest cross-zone room
    // pair and carve a locked bridge.
    for (size_t i = 1; i < zones.size(); ++i) {
        int best_dist = INT_MAX;
        int best_a = -1, best_b = -1;
        for (size_t ra = 0; ra < rooms.size(); ++ra) {
            if (rooms[ra].zone_index != static_cast<int>(i - 1)) continue;
            for (size_t rb = 0; rb < rooms.size(); ++rb) {
                if (rooms[rb].zone_index != static_cast<int>(i)) continue;
                int d = room_dist(rooms[ra], rooms[rb]);
                if (d < best_dist) {
                    best_dist = d;
                    best_a = static_cast<int>(ra);
                    best_b = static_cast<int>(rb);
                }
            }
        }
        if (best_a >= 0)
            carve_any_bridge(sec, rooms[static_cast<size_t>(best_a)],
                                  rooms[static_cast<size_t>(best_b)], /*locked=*/true);
    }
}

// ---------------------------------------------------------------------------
// Phase 5 — Special tile placement (Cut 4)
// ---------------------------------------------------------------------------

// Task 4.1: stamp ⊙ ExitNode in the lobby.
// Picks a corner-adjacent interior Floor cell furthest from the outgoing
// bridges (heuristic: top-left interior corner first, fallback to first Floor).
// The lobby sits in the T1 (leftmost) zone and its bridges go rightward, so
// top-left is genuinely furthest from the outgoing connections.
static void stamp_exit_node(GridSector& sec,
                             const std::vector<LanV2Room>& rooms) {
    for (const auto& r : rooms) {
        if (!r.is_lobby) continue;
        // Try top-left interior corner (furthest from rightward-going bridges).
        int x = r.x + 1;
        int y = r.y + 1;
        if (sec.in_bounds(x, y) && sec.at(x, y) == GridTile::Floor) {
            sec.set(x, y, GridTile::ExitNode);
            return;
        }
        // Fallback: scan interior for the first Floor tile.
        for (int dy = 1; dy < r.h - 1; ++dy) {
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) {
                    sec.set(xx, yy, GridTile::ExitNode);
                    return;
                }
            }
        }
    }
}

// Task 4.2: stamp ⊕ DeepGridGateway in the highest-tier zone-anchor room.
static void stamp_deep_grid_gateway(GridSector& sec,
                                    const GridNetwork& net,
                                    const LanMetadata& meta,
                                    const std::vector<LanV2Room>& rooms) {
    if (!meta.has_deep_grid_edge) return;

    // Find the zone-anchor room with the highest tier (non-lobby).
    const LanV2Room* target = nullptr;
    int best_tier = 0;
    for (const auto& r : rooms) {
        if (r.is_lobby) continue;
        if (!r.is_zone_anchor) continue;
        if (r.tier > best_tier) { best_tier = r.tier; target = &r; }
    }
    if (!target) return;

    // Place ⊕ at center of the anchor room; fallback to first interior Floor.
    int gx = target->x + target->w / 2;
    int gy = target->y + target->h / 2;
    if (!sec.in_bounds(gx, gy) || sec.at(gx, gy) != GridTile::Floor) {
        bool found = false;
        for (int dy = 1; dy < target->h - 1 && !found; ++dy) {
            for (int dx = 1; dx < target->w - 1 && !found; ++dx) {
                int xx = target->x + dx, yy = target->y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) {
                    gx = xx; gy = yy; found = true;
                }
            }
        }
        if (!found) return;
    }
    sec.set(gx, gy, GridTile::DeepGridGateway);

    // Resolve the deep-grid anchor node and store in deep_grid_destination.
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) {
            sec.deep_grid_destination = n.id;
            break;
        }
    }
}

// Task 4.3: register one safe spawn point per subnet room into per_node_spawn.
static void register_per_node_spawns(GridSector& sec,
                                     const std::vector<LanV2Room>& rooms) {
    for (const auto& r : rooms) {
        if (r.is_lobby) continue;
        if (!r.source_subnet.valid()) continue;

        // Scan for a Floor tile not adjacent to any Door.
        bool placed = false;
        for (int dy = 1; dy < r.h - 1 && !placed; ++dy) {
            for (int dx = 1; dx < r.w - 1 && !placed; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) != GridTile::Floor) continue;
                bool near_door = false;
                // Cardinal-only check (4-directional): diagonals are fine.
                const int cdx[4] = {0, 0, -1, 1};
                const int cdy[4] = {-1, 1, 0, 0};
                for (int d = 0; d < 4 && !near_door; ++d)
                    if (sec.in_bounds(xx + cdx[d], yy + cdy[d]) &&
                        sec.at(xx + cdx[d], yy + cdy[d]) == GridTile::Door)
                        near_door = true;
                if (near_door) continue;
                sec.per_node_spawn.emplace(r.source_subnet,
                                           std::pair<int,int>{xx, yy});
                placed = true;
            }
        }
        if (!placed) {
            // Fallback: room center.
            sec.per_node_spawn.emplace(r.source_subnet,
                                       std::pair<int,int>{r.x + r.w / 2,
                                                          r.y + r.h / 2});
        }
    }
}

// Task 4.4B: stamp one DeviceAvatar tile on a perimeter wall of each subnet
// room, and record the source FixtureType in sec.avatar_fixture_type.
static void stamp_device_avatars(GridSector& sec,
                                  const std::vector<LanV2Room>& rooms,
                                  const GridNetwork& net,
                                  const WorldManager& world) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.valid()) continue;

        // Look up the Hackable so we know its real fixture type.
        const Hackable* h = find_hackable_for_subnet(world, net, r.source_subnet);
        FixtureType ft = h ? h->source_type : static_cast<FixtureType>(0);

        // Scan top wall interior cells for a Firewall tile (skip if already
        // a door opening or other special tile).
        bool stamped = false;
        for (int dx = 1; dx < r.w - 1 && !stamped; ++dx) {
            int xx = r.x + dx, yy = r.y;
            if (sec.at(xx, yy) == GridTile::Firewall) {
                sec.set(xx, yy, GridTile::DeviceAvatar);
                sec.avatar_fixture_type.emplace(std::pair<int,int>{xx, yy}, ft);
                stamped = true;
            }
        }
        if (stamped) continue;
        // Fallback: try bottom wall.
        for (int dx = 1; dx < r.w - 1 && !stamped; ++dx) {
            int xx = r.x + dx, yy = r.y + r.h - 1;
            if (sec.at(xx, yy) == GridTile::Firewall) {
                sec.set(xx, yy, GridTile::DeviceAvatar);
                sec.avatar_fixture_type.emplace(std::pair<int,int>{xx, yy}, ft);
                stamped = true;
            }
        }
        if (stamped) continue;
        // Fallback: try left wall.
        for (int dy = 1; dy < r.h - 1 && !stamped; ++dy) {
            int xx = r.x, yy = r.y + dy;
            if (sec.at(xx, yy) == GridTile::Firewall) {
                sec.set(xx, yy, GridTile::DeviceAvatar);
                sec.avatar_fixture_type.emplace(std::pair<int,int>{xx, yy}, ft);
                stamped = true;
            }
        }
        if (stamped) continue;
        // Fallback: try right wall.
        for (int dy = 1; dy < r.h - 1 && !stamped; ++dy) {
            int xx = r.x + r.w - 1, yy = r.y + dy;
            if (sec.at(xx, yy) == GridTile::Firewall) {
                sec.set(xx, yy, GridTile::DeviceAvatar);
                sec.avatar_fixture_type.emplace(std::pair<int,int>{xx, yy}, ft);
                stamped = true;
            }
        }
        // If all four walls are punched by doors, silently skip this room.
    }
}

// ---------------------------------------------------------------------------
// Phase 6 — Template seeding (Cut 5)
// ---------------------------------------------------------------------------

// Populate GridSector::ice_seeds from each room's template seed rule.
// Called after bridges and special tiles so Floor cells are stable.
static void seed_ice_per_template(GridSector& sec,
                                   const std::vector<LanV2Room>& rooms,
                                   const GridNetwork& net,
                                   const WorldManager& world,
                                   std::mt19937& rng) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.valid()) continue;
        const Hackable* h = find_hackable_for_subnet(world, net, r.source_subnet);
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);

        // Collect candidate Floor cells inside the room.
        std::vector<std::pair<int,int>> spots;
        for (int dy = 1; dy < r.h - 1; ++dy)
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) spots.push_back({xx, yy});
            }
        std::shuffle(spots.begin(), spots.end(), rng);

        auto pop = [&]() -> std::optional<std::pair<int,int>> {
            if (spots.empty()) return std::nullopt;
            auto p = spots.back(); spots.pop_back(); return p;
        };

        // White ICE: baseline guard, present from session start.
        for (int i = 0; i < rule.n_white_ice; ++i) {
            if (auto p = pop()) {
                sec.ice_seeds.push_back({p->first, p->second, IceColor::White, 2, /*min_trace=*/0});
            }
        }
        // Gray ICE: escalation, materializes when trace rises.
        for (int i = 0; i < rule.n_gray_ice; ++i) {
            if (auto p = pop()) {
                sec.ice_seeds.push_back({p->first, p->second, IceColor::Gray, 3, /*min_trace=*/25});
            }
        }
        // Black ICE: late-game threat, only at high trace.
        for (int i = 0; i < rule.n_black_ice; ++i) {
            if (auto p = pop()) {
                sec.ice_seeds.push_back({p->first, p->second, IceColor::Black, 4, /*min_trace=*/50});
            }
        }
    }
}

// Seed DataNode and EncryptedFile tiles into rooms per template rule.
// Called after seed_ice_per_template (shares the same Floor pool per room —
// each function shuffles and pops from fresh spots, so they don't compete
// for the exact same cells, but both safely skip non-Floor tiles).
static void seed_content_per_template(GridSector& sec,
                                       const std::vector<LanV2Room>& rooms,
                                       const GridNetwork& net,
                                       const WorldManager& world,
                                       std::mt19937& rng) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.valid()) continue;
        const Hackable* h = find_hackable_for_subnet(world, net, r.source_subnet);
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);

        std::vector<std::pair<int,int>> spots;
        for (int dy = 1; dy < r.h - 1; ++dy)
            for (int dx = 1; dx < r.w - 1; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (sec.at(xx, yy) == GridTile::Floor) spots.push_back({xx, yy});
            }
        std::shuffle(spots.begin(), spots.end(), rng);

        for (int i = 0; i < rule.n_data_nodes && !spots.empty(); ++i) {
            auto [x, y] = spots.back(); spots.pop_back();
            sec.set(x, y, GridTile::DataNode);
        }
        for (int i = 0; i < rule.n_encrypted_files && !spots.empty(); ++i) {
            auto [x, y] = spots.back(); spots.pop_back();
            sec.set(x, y, GridTile::EncryptedFile);
        }
    }
}

// Lock Door tiles on the perimeter of rooms whose template requires it.
static void apply_door_lock_overrides(GridSector& sec,
                                       const std::vector<LanV2Room>& rooms,
                                       const GridNetwork& net,
                                       const WorldManager& world) {
    for (const auto& r : rooms) {
        if (r.is_lobby || !r.source_subnet.valid()) continue;
        const Hackable* h = find_hackable_for_subnet(world, net, r.source_subnet);
        if (!h) continue;
        auto kind = choose_template_for_tags(h->tags);
        auto rule = template_seed_rule(kind, r.tier);
        if (!rule.lock_incoming_doors) continue;

        // Find Door tiles on or immediately adjacent to this room's perimeter.
        // The 3-tile bridge model places a Door tile 1 cell OUTSIDE the
        // room's perimeter, so scan border + 1.
        for (int dy = -1; dy <= r.h; ++dy) {
            for (int dx = -1; dx <= r.w; ++dx) {
                int xx = r.x + dx, yy = r.y + dy;
                if (!sec.in_bounds(xx, yy)) continue;
                if (sec.at(xx, yy) == GridTile::Door) {
                    sec.locked_doors.insert({xx, yy});
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

GridSector generate_lan_sector_v2(const LanMetadata& meta,
                                  const GridNetwork& net,
                                  const WorldManager& world) {
    LanV2SizeParams p = compute_lan_v2_size(meta);

    GridSector sec;
    sec.w = p.width;
    sec.h = p.height;
    sec.tiles.assign(static_cast<size_t>(sec.w) * sec.h, GridTile::Void);  // Plan 8: Void background for v2
    sec.spawn_x = 1;
    sec.spawn_y = 1;

    auto zones = partition_zones(meta, p, net, world);
    auto rooms  = place_rooms(meta, zones, net, world, meta.gen_seed);

    auto stamp = [&](int x, int y, GridTile t) {
        if (x >= 0 && y >= 0 && x < sec.w && y < sec.h)
            sec.tiles[static_cast<size_t>(y) * sec.w + x] = t;
    };

    for (const auto& r : rooms) {
        // Perimeter = Firewall.
        for (int dx = 0; dx < r.w; ++dx) {
            stamp(r.x + dx, r.y,           GridTile::Firewall);
            stamp(r.x + dx, r.y + r.h - 1, GridTile::Firewall);
        }
        for (int dy = 0; dy < r.h; ++dy) {
            stamp(r.x,           r.y + dy, GridTile::Firewall);
            stamp(r.x + r.w - 1, r.y + dy, GridTile::Firewall);
        }
        // Interior = Floor.
        for (int dy = 1; dy < r.h - 1; ++dy) {
            for (int dx = 1; dx < r.w - 1; ++dx) {
                stamp(r.x + dx, r.y + dy, GridTile::Floor);
            }
        }
        if (r.is_lobby) {
            sec.spawn_x = r.x + r.w / 2;
            sec.spawn_y = r.y + r.h / 2;
        }
    }

    // Phase 4a: per-zone MST + bonus edges (open bridges within each zone).
    for (size_t zi = 0; zi < zones.size(); ++zi) {
        auto mst   = zone_mst(rooms, static_cast<int>(zi));
        auto bonus = bonus_edges(rooms, mst);
        for (const auto& e : mst)
            carve_any_bridge(sec, rooms[static_cast<size_t>(e.from_idx)],
                                  rooms[static_cast<size_t>(e.to_idx)], /*locked=*/false);
        for (const auto& e : bonus)
            carve_any_bridge(sec, rooms[static_cast<size_t>(e.from_idx)],
                                  rooms[static_cast<size_t>(e.to_idx)], /*locked=*/false);
    }

    // Phase 4b: inter-zone locked bridges (one per adjacent zone pair, ▣).
    carve_inter_zone_bridges(sec, rooms, zones);

    // Phase 5 (Cut 4): special tile placement.
    stamp_exit_node(sec, rooms);                               // Task 4.1: ⊙ in lobby
    stamp_deep_grid_gateway(sec, net, meta, rooms);            // Task 4.2: ⊕ in highest-tier anchor
    register_per_node_spawns(sec, rooms);                      // Task 4.3: per-subnet spawn coords
    stamp_device_avatars(sec, rooms, net, world);              // Task 4.4: DeviceAvatar on each room wall

    // Phase 6 (Cut 5): template-driven seeding.
    // Uses a separate RNG seeded from gen_seed+1 so Phase 4–5 placement is
    // unaffected by Phase 6 additions.
    std::mt19937 phase6_rng(meta.gen_seed + 1u);
    seed_ice_per_template(sec, rooms, net, world, phase6_rng);   // Task 5.3
    seed_content_per_template(sec, rooms, net, world, phase6_rng); // Task 5.4
    apply_door_lock_overrides(sec, rooms, net, world);              // Task 5.4

    // Plan 8 Cut 8: populate zone_boxes for the HUD overlay. Each zone is
    // rendered as a full-height column starting at row 1 (row 0 hosts the
    // banner). Boxes come straight from the partition_zones output so all
    // zones are uniform — no irregular perimeters keyed to room placement.
    for (const auto& zr : zones) {
        GridSector::ZoneBox box;
        box.x      = zr.x;
        box.y      = 1;                          // banner at row 0
        box.w      = zr.w;
        box.h      = std::max(1, sec.h - 1);     // full sector height minus banner row
        box.tier   = zr.tier;
        box.banner = zr.name.empty() ? std::string("ZONE") : zr.name;
        sec.zone_boxes.push_back(box);
    }

    // Plan 8: per-room rect for the HUD header. The renderer shows the
    // device hostname for the room the player is currently inside; on a
    // bridge / corridor / void cell, only the LAN is shown.
    for (const auto& r : rooms) {
        GridSector::SubnetRoom sr;
        sr.x        = r.x;
        sr.y        = r.y;
        sr.w        = r.w;
        sr.h        = r.h;
        sr.subnet   = r.source_subnet;
        sr.is_lobby = r.is_lobby;
        sec.subnet_rooms.push_back(sr);
    }

    return sec;
}

} // namespace astra
