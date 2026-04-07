#include "astra/perimeter_builder.h"

#include <cmath>

namespace astra {

namespace {

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

bool is_protected(Tile t) {
    return t == Tile::StructuralWall || t == Tile::IndoorFloor;
}

bool is_gate_position(int x, int y,
                      const std::vector<std::pair<int, int>>& gates) {
    for (auto& [gx, gy] : gates) {
        if (std::abs(x - gx) <= 1 && std::abs(y - gy) <= 1) return true;
    }
    return false;
}

} // namespace

void PerimeterBuilder::build(TileMap& map, const SettlementPlan& plan,
                             std::mt19937& rng) const {
    if (!plan.perimeter.has_value()) return;

    auto& peri = plan.perimeter.value();
    auto& style = plan.style;
    const Rect& b = peri.bounds;

    std::uniform_real_distribution<float> decay_dist(0.0f, 1.0f);

    // Walk the border of the perimeter rectangle
    for (int x = b.x; x < b.x + b.w; ++x) {
        for (int y = b.y; y < b.y + b.h; ++y) {
            // Only border tiles
            bool on_border = (x == b.x || x == b.x + b.w - 1 ||
                              y == b.y || y == b.y + b.h - 1);
            if (!on_border) continue;
            if (!in_bounds(x, y, map)) continue;
            if (is_protected(map.get(x, y))) continue;

            if (is_gate_position(x, y, peri.gate_positions)) {
                // Place gate: floor tile + Gate fixture
                map.set(x, y, Tile::Floor);
                if (map.fixture_id(x, y) >= 0)
                    map.remove_fixture(x, y);
                map.add_fixture(x, y, make_fixture(style.gate));
            } else {
                // Decay roll for ruined style
                if (style.decay > 0.0f && decay_dist(rng) < style.decay)
                    continue;

                map.set(x, y, style.perimeter_wall);
                if (map.fixture_id(x, y) >= 0)
                    map.remove_fixture(x, y);
            }
        }
    }

    // Advanced style: 3x3 corner towers with hollow interior + lighting
    if (style.name == "Advanced") {
        int corners[][2] = {
            {b.x - 1, b.y - 1},
            {b.x + b.w - 2, b.y - 1},
            {b.x - 1, b.y + b.h - 2},
            {b.x + b.w - 2, b.y + b.h - 2},
        };

        for (auto& [cx, cy] : corners) {
            // 3x3 tower: walls on perimeter, floor + light in center
            for (int dx = 0; dx < 3; ++dx) {
                for (int dy = 0; dy < 3; ++dy) {
                    int tx = cx + dx;
                    int ty = cy + dy;
                    if (!in_bounds(tx, ty, map)) continue;
                    if (dx > 0 && dx < 2 && dy > 0 && dy < 2) {
                        // Center tile: floor + lighting
                        map.set(tx, ty, style.floor_tile);
                        map.remove_fixture(tx, ty);
                        map.add_fixture(tx, ty, make_fixture(style.lighting));
                    } else {
                        map.set(tx, ty, style.perimeter_wall);
                        map.remove_fixture(tx, ty);
                    }
                }
            }
        }
    }
}

} // namespace astra
