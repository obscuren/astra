#include "astra/exterior_decorator.h"

#include <algorithm>
#include <cmath>

namespace astra {

namespace {

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

bool is_lush_biome(Biome b) {
    return b == Biome::Forest || b == Biome::Jungle ||
           b == Biome::Grassland || b == Biome::Marsh;
}

} // namespace

void ExteriorDecorator::decorate(TileMap& map, const SettlementPlan& plan,
                                 std::mt19937& rng) const {
    auto& style = plan.style;
    const Rect& foot = plan.placement.footprint;

    // --- 1. Clear scatter in footprint + 2-tile margin ---
    int margin = 2;
    int x0 = std::max(0, foot.x - margin);
    int y0 = std::max(0, foot.y - margin);
    int x1 = std::min(map.width() - 1, foot.x + foot.w - 1 + margin);
    int y1 = std::min(map.height() - 1, foot.y + foot.h - 1 + margin);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            const auto& f = map.fixture(fid);
            if (f.passable && !f.interactable) {
                map.remove_fixture(x, y);
            }
        }
    }

    // --- 2. Path lighting — scan actual Path tiles, place lamps beside them ---
    // Collect all Path tiles in the footprint, then place lamps at intervals
    // next to the path (on adjacent Floor tiles), not ON the path.
    {
        std::vector<std::pair<int,int>> path_tiles;
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                if (map.get(x, y) == Tile::Path) {
                    path_tiles.push_back({x, y});
                }
            }
        }

        int spacing = 8;
        int since_last = 0;
        for (auto& [px, py] : path_tiles) {
            ++since_last;
            if (since_last < spacing) continue;

            // Find an adjacent Floor tile to place the lamp (not on the path itself)
            static constexpr int ddx[] = {1, -1, 0, 0};
            static constexpr int ddy[] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                int lx = px + ddx[d];
                int ly = py + ddy[d];
                if (!in_bounds(lx, ly, map)) continue;
                Tile t = map.get(lx, ly);
                if (t != Tile::Floor) continue;
                if (map.fixture_id(lx, ly) >= 0) continue;
                map.add_fixture(lx, ly, make_fixture(style.lighting));
                since_last = 0;
                break;
            }
        }
    }

    // --- 3. Door lighting — place lamp on ground outside the door (not on walls!) ---
    for (auto& bspec : plan.buildings) {
        if (bspec.shape.door_positions.empty()) continue;
        auto& [door_x, door_y] = bspec.shape.door_positions.front();

        // Find the outside-facing direction from the door
        static constexpr int ddx[] = {0, 0, -1, 1};
        static constexpr int ddy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int ox = door_x + ddx[d];
            int oy = door_y + ddy[d];
            if (!in_bounds(ox, oy, map)) continue;
            Tile t = map.get(ox, oy);
            // Must be outside the building (floor or path, not wall/indoor)
            if (t != Tile::Floor && t != Tile::Path) continue;
            // Place lamp offset to the side of the door (perpendicular)
            // Try both perpendicular directions
            int pdx = (ddx[d] == 0) ? 1 : 0;
            int pdy = (ddy[d] == 0) ? 1 : 0;
            for (int side : {1, -1}) {
                int lx = ox + pdx * side;
                int ly = oy + pdy * side;
                if (!in_bounds(lx, ly, map)) continue;
                Tile lt = map.get(lx, ly);
                if (lt != Tile::Floor && lt != Tile::Path) continue;
                if (map.fixture_id(lx, ly) >= 0) continue;
                map.add_fixture(lx, ly, make_fixture(style.lighting));
                goto next_building;
            }
        }
        next_building:;
    }

    // --- 4. Benches — 2-4 benches near settlement center on path tiles ---
    std::uniform_int_distribution<int> bench_count_dist(2, 4);
    int bench_target = bench_count_dist(rng);
    int center_x = foot.x + foot.w / 2;
    int center_y = foot.y + foot.h / 2;
    int benches_placed = 0;

    // Collect candidate path tiles near center
    struct Candidate {
        int x, y;
        int dist;
    };
    std::vector<Candidate> candidates;

    // Find floor tiles adjacent to (but not on) paths — benches beside paths, not blocking them
    for (int y = foot.y; y < foot.y + foot.h; ++y) {
        for (int x = foot.x; x < foot.x + foot.w; ++x) {
            if (!in_bounds(x, y, map)) continue;
            Tile t = map.get(x, y);
            // Must be regular floor (not path, not wall, not building interior)
            if (t != Tile::Floor) continue;
            if (map.fixture_id(x, y) >= 0) continue;
            // Must be adjacent to a path tile
            bool near_path = false;
            for (auto [dx, dy] : {std::pair{1,0},{-1,0},{0,1},{0,-1}}) {
                int nx = x + dx, ny = y + dy;
                if (in_bounds(nx, ny, map) && map.get(nx, ny) == style.path_tile) {
                    near_path = true;
                    break;
                }
            }
            if (!near_path) continue;
            int dist = std::abs(x - center_x) + std::abs(y - center_y);
            candidates.push_back({x, y, dist});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](auto& a, auto& b) { return a.dist < b.dist; });

    for (auto& c : candidates) {
        if (benches_placed >= bench_target) break;
        // Ensure minimum spacing between benches
        map.add_fixture(c.x, c.y, make_fixture(style.seating));
        ++benches_placed;
    }

    // --- 5. Planters — 30% chance at building corners (outside) on lush biomes ---
    if (is_lush_biome(map.biome())) {
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);

        for (auto& bspec : plan.buildings) {
            const Rect& r = bspec.shape.primary;
            // Four outside corners
            int corners[][2] = {
                {r.x - 1, r.y - 1},
                {r.x + r.w, r.y - 1},
                {r.x - 1, r.y + r.h},
                {r.x + r.w, r.y + r.h},
            };

            for (auto& [cx, cy] : corners) {
                if (chance(rng) > 0.3f) continue;
                if (!in_bounds(cx, cy, map)) continue;
                if (map.fixture_id(cx, cy) >= 0) continue;
                Tile t = map.get(cx, cy);
                if (t == Tile::StructuralWall || t == Tile::IndoorFloor ||
                    t == Tile::Water) continue;
                map.add_fixture(cx, cy, make_fixture(FixtureType::Planter));
            }
        }
    }
}

} // namespace astra
