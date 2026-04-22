#include "astra/dungeon/connectivity.h"

#include "astra/tilemap.h"

#include <cstdint>
#include <set>
#include <vector>

namespace astra::dungeon {

namespace {

// Flood-fill from any passable cell in entry_rid; collect every region
// id reached.
std::set<int> reachable_regions(const TileMap& map, int entry_rid) {
    std::set<int> reached;
    if (entry_rid < 0) return reached;

    const int w = map.width();
    const int h = map.height();

    // Find any passable cell in entry_rid.
    int sx = -1, sy = -1;
    for (int y = 0; y < h && sx < 0; ++y) {
        for (int x = 0; x < w && sx < 0; ++x) {
            if (map.region_id(x, y) == entry_rid && map.passable(x, y)) {
                sx = x; sy = y;
            }
        }
    }
    if (sx < 0) return reached;

    std::vector<uint8_t> seen(static_cast<size_t>(w) * h, 0);
    std::vector<std::pair<int,int>> stack{{sx, sy}};
    seen[static_cast<size_t>(sy) * w + sx] = 1;
    reached.insert(entry_rid);

    while (!stack.empty()) {
        auto [x, y] = stack.back();
        stack.pop_back();
        int rid = map.region_id(x, y);
        if (rid >= 0) reached.insert(rid);

        constexpr int dx[4] = { 1, -1, 0, 0 };
        constexpr int dy[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            size_t idx = static_cast<size_t>(ny) * w + nx;
            if (seen[idx]) continue;
            seen[idx] = 1;
            if (!map.passable(nx, ny)) continue;
            stack.emplace_back(nx, ny);
        }
    }
    return reached;
}

} // namespace

void apply_connectivity(TileMap& map, const DungeonStyle& style,
                        LevelContext& ctx, std::mt19937& rng) {
    (void)rng;
    if (!style.connectivity_required) return;

    auto reached = reachable_regions(map, ctx.entry_region_id);
    const int rc = map.region_count();
    // Slice 1 layouts connect by construction. If a regression slips
    // through, we want to know loudly — but for now, no corridor
    // routing is done here.
    if (static_cast<int>(reached.size()) < rc) {
#ifdef ASTRA_DEV_MODE
        // TODO: route a PathRouter-style corridor when follow-up slices
        // introduce layouts that don't connect by construction.
        (void)0;
#endif
    }
}

} // namespace astra::dungeon
