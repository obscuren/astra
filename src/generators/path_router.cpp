#include "astra/path_router.h"
#include "astra/noise.h"

#include <algorithm>
#include <cmath>

namespace astra {

namespace {

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

bool is_protected(const TileMap& map, int x, int y) {
    Tile t = map.get(x, y);
    if (t == Tile::StructuralWall || t == Tile::Wall
        || t == Tile::IndoorFloor || t == Tile::Water)
        return true;
    // Fixture tiles: only protect impassable fixtures (doors, windows, furniture)
    // Passable fixtures (flora, debris) can be carved through
    if (t == Tile::Fixture) {
        int fid = map.fixture_id(x, y);
        if (fid >= 0 && !map.fixture(fid).passable) return true;
    }
    return false;
}

void carve_tile(TileMap& map, int x, int y, Tile path_tile) {
    if (!in_bounds(x, y, map)) return;
    if (is_protected(map, x, y)) return;

    // Clear any fixture on this tile (remove flora/scatter)
    if (map.fixture_id(x, y) >= 0) {
        map.remove_fixture(x, y);
    }

    map.set(x, y, path_tile);
}

} // namespace

void PathRouter::route(TileMap& map, const SettlementPlan& plan) const {
    // --- Carve paths ---
    for (auto& p : plan.paths) {
        int x0 = p.from_x;
        int x1 = p.to_x;
        int y0 = p.from_y;
        int y1 = p.to_y;

        if (p.width >= 3) {
            // Wide paths: winding trail using noise offset
            // Walk a straight line from start to end, offset perpendicular by noise
            int dist_x = std::abs(x1 - x0);
            int dist_y = std::abs(y1 - y0);
            bool mostly_vertical = (dist_y >= dist_x);
            int steps = std::max(dist_x, dist_y);
            if (steps == 0) continue;

            // Noise seed from endpoint positions
            unsigned noise_seed = static_cast<unsigned>(x0 * 7919 + y0 * 104729);

            for (int i = 0; i <= steps; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                int cx = x0 + static_cast<int>(t * (x1 - x0));
                int cy = y0 + static_cast<int>(t * (y1 - y0));

                // Noise offset perpendicular to path direction — more winding
                float noise_val = fbm(static_cast<float>(i) * 0.1f, 0.0f,
                                      noise_seed, 1.0f, 2);
                int offset = static_cast<int>((noise_val - 0.5f) * 8.0f);

                // Carve a width-sized swath perpendicular to travel direction
                int half = p.width / 2;
                for (int w = -half; w <= half; ++w) {
                    int px = mostly_vertical ? cx + w + offset : cx;
                    int py = mostly_vertical ? cy : cy + w + offset;
                    carve_tile(map, px, py, plan.style.path_tile);
                }
            }
        } else {
            // Narrow paths: L-shaped grid-aligned
            int dx = (x1 > x0) ? 1 : -1;
            int dy = (y1 > y0) ? 1 : -1;

            // Horizontal leg
            for (int x = x0; x != x1 + dx; x += dx) {
                for (int w = 0; w < p.width; ++w) {
                    carve_tile(map, x, y0 + w, plan.style.path_tile);
                }
            }

            // Vertical leg
            for (int y = y0; y != y1 + dy; y += dy) {
                for (int w = 0; w < p.width; ++w) {
                    carve_tile(map, x1 + w, y, plan.style.path_tile);
                }
            }
        }
    }

    // --- Build bridges ---
    for (auto& b : plan.bridges) {
        bool horizontal = (b.start_y == b.end_y);

        if (horizontal) {
            int dx = (b.end_x > b.start_x) ? 1 : -1;
            int half = b.width / 2;

            for (int x = b.start_x; x != b.end_x + dx; x += dx) {
                for (int w = -half; w < -half + b.width; ++w) {
                    int bx = x;
                    int by = b.start_y + w;
                    if (!in_bounds(bx, by, map)) continue;

                    // Bridge walkable surface
                    map.set(bx, by, Tile::Floor);
                    if (map.fixture_id(bx, by) >= 0)
                        map.remove_fixture(bx, by);
                    map.add_fixture(bx, by, make_fixture(plan.style.bridge_floor));
                }

                // Rails on both sides (perpendicular offset)
                int rail_lo = b.start_y - half - 1;
                int rail_hi = b.start_y - half + b.width;
                if (in_bounds(x, rail_lo, map)) {
                    if (map.fixture_id(x, rail_lo) >= 0)
                        map.remove_fixture(x, rail_lo);
                    map.set(x, rail_lo, Tile::Floor);
                    map.add_fixture(x, rail_lo, make_fixture(plan.style.bridge_rail));
                }
                if (in_bounds(x, rail_hi, map)) {
                    if (map.fixture_id(x, rail_hi) >= 0)
                        map.remove_fixture(x, rail_hi);
                    map.set(x, rail_hi, Tile::Floor);
                    map.add_fixture(x, rail_hi, make_fixture(plan.style.bridge_rail));
                }
            }

            // Support pillars at both ends (wall tiles at corners)
            for (int w : {-half - 1, -half + b.width}) {
                for (int ex : {b.start_x, b.end_x}) {
                    int py = b.start_y + w;
                    if (in_bounds(ex, py, map)) {
                        map.set(ex, py, Tile::Wall);
                    }
                }
            }
        } else {
            // Vertical bridge
            int dy = (b.end_y > b.start_y) ? 1 : -1;
            int half = b.width / 2;

            for (int y = b.start_y; y != b.end_y + dy; y += dy) {
                for (int w = -half; w < -half + b.width; ++w) {
                    int bx = b.start_x + w;
                    int by = y;
                    if (!in_bounds(bx, by, map)) continue;

                    map.set(bx, by, Tile::Floor);
                    if (map.fixture_id(bx, by) >= 0)
                        map.remove_fixture(bx, by);
                    map.add_fixture(bx, by, make_fixture(plan.style.bridge_floor));
                }

                // Rails on both sides
                int rail_lo = b.start_x - half - 1;
                int rail_hi = b.start_x - half + b.width;
                if (in_bounds(rail_lo, y, map)) {
                    if (map.fixture_id(rail_lo, y) >= 0)
                        map.remove_fixture(rail_lo, y);
                    map.set(rail_lo, y, Tile::Floor);
                    map.add_fixture(rail_lo, y, make_fixture(plan.style.bridge_rail));
                }
                if (in_bounds(rail_hi, y, map)) {
                    if (map.fixture_id(rail_hi, y) >= 0)
                        map.remove_fixture(rail_hi, y);
                    map.set(rail_hi, y, Tile::Floor);
                    map.add_fixture(rail_hi, y, make_fixture(plan.style.bridge_rail));
                }
            }

            // Support pillars at both ends
            for (int w : {-half - 1, -half + b.width}) {
                for (int ey : {b.start_y, b.end_y}) {
                    int px = b.start_x + w;
                    if (in_bounds(px, ey, map)) {
                        map.set(px, ey, Tile::Wall);
                    }
                }
            }
        }
    }
}

} // namespace astra
