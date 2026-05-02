#include "astra/lan.h"

#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/npc.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <utility>
#include <vector>

namespace astra {

LanFlavour infer_flavour(const WorldManager& world) {
    // TODO(Cut 2): swap to MapKind once World tracks per-map kinds.
    // Cut 1: derive from MapType + biome + location_name.
    const TileMap& m = world.map();

    // Precursor LANs are flagged via the "Precursor" location-name marker
    // (see OW_PrecursorArchive descent and crashed-ship POIs).
    const std::string& loc = m.location_name();
    if (loc.find("Precursor") != std::string::npos) {
        return LanFlavour::Precursor;
    }
    if (loc.find("Crashed") != std::string::npos
     || loc.find("crashed") != std::string::npos) {
        return LanFlavour::Precursor;
    }

    switch (m.map_type()) {
        case MapType::SpaceStation:
        case MapType::DerelictStation:
        case MapType::Starship:
        case MapType::DetailMap:           // settlements treated as Station
        case MapType::Overworld:
            return LanFlavour::Station;
        case MapType::Asteroid:
        case MapType::Rocky:
        case MapType::Lava:
        case MapType::Nebula:
            return LanFlavour::Asteroid;
    }
    return LanFlavour::Station;
}

bool infer_connected(LanFlavour f) {
    // Spec §3 Q5: Station + Asteroid + Precursor connected; Dungeon isolated.
    return f != LanFlavour::Dungeon;
}

namespace {

struct HackableLoc {
    Hackable* h            = nullptr;
    int       x            = 0;
    int       y            = 0;
    bool      is_npc       = false;
};

// Walks the active map's electrical fixtures + alive NPC implants.
// Stable-sorted by (y, x) so IP allocation is reproducible.
std::vector<HackableLoc> collect_hackables(WorldManager& world) {
    std::vector<HackableLoc> out;
    TileMap& m = world.map();
    const auto& fixture_ids = m.fixture_ids();
    const int   w = m.width();
    const int   h = m.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (idx < 0 || idx >= static_cast<int>(fixture_ids.size())) continue;
            int fid = fixture_ids[idx];
            if (fid < 0) continue;
            FixtureData& fd = m.fixture_mut(fid);
            if (!fd.cyber) continue;
            if (!has_tag(fd.cyber->tags, HackTag::Electronic)) continue;
            HackableLoc loc;
            loc.h = &*fd.cyber;
            loc.x = x;
            loc.y = y;
            loc.is_npc = false;
            out.push_back(loc);
        }
    }
    for (auto& npc : world.npcs()) {
        if (!npc.alive() || !npc.cyber) continue;
        if (!has_tag(npc.cyber->tags, HackTag::Electronic)) continue;
        HackableLoc loc;
        loc.h = &*npc.cyber;
        loc.x = npc.x;
        loc.y = npc.y;
        loc.is_npc = true;
        out.push_back(loc);
    }
    std::sort(out.begin(), out.end(),
              [](const HackableLoc& a, const HackableLoc& b) {
                  if (a.y != b.y) return a.y < b.y;
                  return a.x < b.x;
              });
    return out;
}

const char* pick_room_name(LanFlavour flavour, int idx) {
    static const char* station_pool[]   = { "lobby", "security", "ops", "rnd",
                                            "accounting", "hr", "exec",
                                            "archive", "comms", "server-rack" };
    static const char* asteroid_pool[]  = { "dispatch", "ore-vault",
                                            "life-support", "hangar",
                                            "foreman", "tool-bay" };
    static const char* dungeon_pool[]   = { "vault", "antechamber",
                                            "oubliette", "sanctum", "records" };
    static const char* precursor_pool[] = { "nave", "ossuary", "glyph-vault",
                                            "chorus", "sanctum" };
    auto pick = [&](const char* const* pool, int N) {
        return pool[((idx % N) + N) % N];
    };
    switch (flavour) {
        case LanFlavour::Station:   return pick(station_pool,   10);
        case LanFlavour::Asteroid:  return pick(asteroid_pool,  6);
        case LanFlavour::Dungeon:   return pick(dungeon_pool,   5);
        case LanFlavour::Precursor: return pick(precursor_pool, 5);
    }
    return "lan";
}

// Lloyd's k-means on (x, y) with deterministic init from the
// already-sorted hack list. Bounded at 8 iterations.
std::vector<LanRoom> cluster_rooms(const std::vector<HackableLoc>& hacks,
                                   const std::vector<GridNodeId>&  subnet_ids,
                                   int k,
                                   LanFlavour flavour) {
    if (k < 1) k = 1;
    std::vector<LanRoom> rooms(static_cast<size_t>(k));
    for (int i = 0; i < k; ++i) {
        rooms[i].name = pick_room_name(flavour, i);
        rooms[i].tier = 1;
    }
    if (hacks.empty()) return rooms;

    if (k == 1) {
        int min_x = INT_MAX, min_y = INT_MAX;
        int max_x = INT_MIN, max_y = INT_MIN;
        for (const auto& hl : hacks) {
            min_x = std::min(min_x, hl.x);
            min_y = std::min(min_y, hl.y);
            max_x = std::max(max_x, hl.x);
            max_y = std::max(max_y, hl.y);
        }
        rooms[0].extents = { min_x, min_y,
                             max_x - min_x + 1, max_y - min_y + 1 };
        rooms[0].contained_subnets = subnet_ids;
        return rooms;
    }

    // Deterministic init: spread by N-quantile across the sorted list.
    std::vector<std::pair<int,int>> centroids(static_cast<size_t>(k));
    const int N = static_cast<int>(hacks.size());
    for (int i = 0; i < k; ++i) {
        int j = (i * N) / k;
        if (j >= N) j = N - 1;
        centroids[i] = { hacks[j].x, hacks[j].y };
    }
    std::vector<int> assign(hacks.size(), 0);

    for (int iter = 0; iter < 8; ++iter) {
        for (size_t i = 0; i < hacks.size(); ++i) {
            int best = 0;
            long long bd = LLONG_MAX;
            for (int c = 0; c < k; ++c) {
                long long dx = hacks[i].x - centroids[c].first;
                long long dy = hacks[i].y - centroids[c].second;
                long long d  = dx * dx + dy * dy;
                if (d < bd) { bd = d; best = c; }
            }
            assign[i] = best;
        }
        std::vector<long long> sx(static_cast<size_t>(k), 0);
        std::vector<long long> sy(static_cast<size_t>(k), 0);
        std::vector<int>       cn(static_cast<size_t>(k), 0);
        for (size_t i = 0; i < hacks.size(); ++i) {
            sx[assign[i]] += hacks[i].x;
            sy[assign[i]] += hacks[i].y;
            cn[assign[i]] += 1;
        }
        for (int c = 0; c < k; ++c) {
            if (cn[c] > 0) {
                centroids[c] = {
                    static_cast<int>(sx[c] / cn[c]),
                    static_cast<int>(sy[c] / cn[c]),
                };
            }
        }
    }

    // Per-room extents + contained subnets.
    struct BB { int min_x, min_y, max_x, max_y; bool has; };
    std::vector<BB> bb(static_cast<size_t>(k),
                       BB{ INT_MAX, INT_MAX, INT_MIN, INT_MIN, false });
    for (size_t i = 0; i < hacks.size(); ++i) {
        int c = assign[i];
        BB& b = bb[c];
        b.has = true;
        b.min_x = std::min(b.min_x, hacks[i].x);
        b.min_y = std::min(b.min_y, hacks[i].y);
        b.max_x = std::max(b.max_x, hacks[i].x);
        b.max_y = std::max(b.max_y, hacks[i].y);
        if (i < subnet_ids.size()) {
            rooms[c].contained_subnets.push_back(subnet_ids[i]);
        }
    }
    for (int c = 0; c < k; ++c) {
        if (bb[c].has) {
            rooms[c].extents = { bb[c].min_x, bb[c].min_y,
                                 bb[c].max_x - bb[c].min_x + 1,
                                 bb[c].max_y - bb[c].min_y + 1 };
        }
    }

    // Tier promotion: large LAN -> one inner sanctum (smallest extent).
    if (k >= 9) {
        int innermost = 0;
        int best_area = INT_MAX;
        for (int c = 0; c < k; ++c) {
            int a = rooms[c].extents.w * rooms[c].extents.h;
            if (a > 0 && a < best_area) { best_area = a; innermost = c; }
        }
        rooms[innermost].tier = 3;
    } else if (k >= 5) {
        rooms.back().tier = 2;
    }
    return rooms;
}

// Find an existing DeepGridAnchor node (Plan 4 stamps one when the
// Consciousness Anchor capstone is taken). Lazy-create if absent — the
// shared anchor is "per consciousness" but Cut 1 has only one player.
// TODO(Cut 3): plumb consciousness_id through and dedupe per consciousness.
GridNodeId find_or_create_deep_grid_anchor(GridNetwork& net) {
    for (const auto& n : net.nodes()) {
        if (n.kind == GridNodeKind::DeepGridAnchor) return n.id;
    }
    GridNode a;
    a.kind          = GridNodeKind::DeepGridAnchor;
    a.label         = "Consciousness.Anchor";
    a.security_tier = 3;
    a.layout_x      = 8;
    a.layout_y      = 5;
    a.owned_by_consciousness_id = 0;   // Cut 3 will populate
    return net.add_node(std::move(a));
}

} // namespace

void register_hackables_in_lan(WorldManager& world,
                               GridNetwork& net,
                               LanMetadata& meta) {
    auto hacks = collect_hackables(world);

    meta.flavour            = infer_flavour(world);
    meta.connected          = infer_connected(meta.flavour);
    meta.has_deep_grid_edge = false;   // set true only if we actually wire it
    const uint32_t map_seed = static_cast<uint32_t>(world.seed());
    meta.gen_seed           = map_seed ^ 0xA5A5A5A5u;
    meta.subnet_base        = derive_subnet_base(map_seed);

    // Region label — use the map's location name; fall back to a sane default.
    const std::string& loc_name = world.map().location_name();
    meta.region_label = loc_name.empty() ? std::string("Unknown LAN") : loc_name;
    meta.display_name = meta.region_label + " LAN";
    meta.security_tier   = 1;          // TODO(Cut 2): pull from World per-map tier
    meta.nodes_total     = static_cast<int>(hacks.size());
    meta.nodes_cracked   = 0;
    meta.origin_galaxy_id = 0;          // Cut 1: single galaxy

    if (hacks.empty()) {
        meta.lan_root = {};
        meta.rooms.clear();
        return;
    }

    // Layout constants. IP labels render as `[10.X.Y.NNN]` — up to 16 cells
    // wide with three-digit host octets, so columns need at least 17 cells of
    // pitch to avoid `[10.X.Y.10[10.X.Y.11[...` overlap. Cut 4 will replace
    // this with world-coord mirroring per spec §10; for Cut 1 we just want
    // the IP labels to be readable in the existing widget.
    constexpr int kLanRootX   = 2;
    constexpr int kLanRootY   = 2;
    constexpr int kSubnetTopY = 5;
    constexpr int kSubnetColW = 18;   // 16-cell IP label + 2-cell gutter
    constexpr int kSubnetRowH = 2;
    constexpr int kSubnetCols = 3;

    // 1) LanRoot node.
    GridNode root_node;
    root_node.kind          = GridNodeKind::LanRoot;
    root_node.label         = meta.display_name;
    root_node.security_tier = meta.security_tier;
    root_node.source_seed   = meta.gen_seed;
    root_node.layout_x      = kLanRootX;
    root_node.layout_y      = kLanRootY;
    meta.lan_root = net.add_node(std::move(root_node));

    // 2) One Subnet per Hackable + IP allocation.
    std::vector<GridNodeId> subnet_ids;
    subnet_ids.reserve(hacks.size());
    int host = 1;
    for (auto& hl : hacks) {
        if (host >= 254) break;             // reserve .254 for the deep-Grid gateway
        const uint8_t host_oct = static_cast<uint8_t>(host);
        hl.h->ip = pack_ip(meta.subnet_base, host_oct);

        GridNode sn;
        sn.kind          = GridNodeKind::Subnet;
        sn.security_tier = hl.h->security_tier;
        sn.source_seed   = (meta.gen_seed << 8) | static_cast<uint32_t>(host);
        sn.label         = format_ip(hl.h->ip);
        const int idx0   = host - 1;
        sn.layout_x      = kLanRootX + (idx0 % kSubnetCols) * kSubnetColW;
        sn.layout_y      = kSubnetTopY + (idx0 / kSubnetCols) * kSubnetRowH;
        GridNodeId sid   = net.add_node(std::move(sn));
        hl.h->jack_in_node_id = static_cast<int>(sid.value);
        subnet_ids.push_back(sid);

        GridEdge e;
        e.from         = meta.lan_root;
        e.to           = sid;
        e.gateway_tier = (host == 1) ? 0 : 1;   // first subnet open; rest tier-1
        e.cracked      = false;
        net.add_edge(e);

        ++host;
    }

    // 3) Connected LAN gets a tier-2 edge to the shared DeepGridAnchor.
    if (meta.connected) {
        GridNodeId anchor = find_or_create_deep_grid_anchor(net);
        GridEdge e;
        e.from         = meta.lan_root;
        e.to           = anchor;
        e.gateway_tier = 2;
        e.cracked      = false;
        net.add_edge(e);
        meta.has_deep_grid_edge = true;
    }

    // 4) k-means cluster (x,y) into rooms.
    int k = std::max(1, static_cast<int>((hacks.size() + 2) / 3));
    meta.rooms = cluster_rooms(hacks, subnet_ids, k, meta.flavour);
}

} // namespace astra
