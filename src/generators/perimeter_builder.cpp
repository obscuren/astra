#include "astra/perimeter_builder.h"
#include "astra/noise.h"

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
    unsigned noise_seed = rng();

    // Walk the border with noise-based organic offsets.
    // Instead of a perfect rectangle, each wall segment is offset
    // inward/outward by 0-2 tiles based on noise, creating an
    // irregular but intentional perimeter.
    auto place_wall = [&](int x, int y) {
        if (!in_bounds(x, y, map)) return;
        if (is_protected(map.get(x, y))) return;

        if (is_gate_position(x, y, peri.gate_positions)) {
            map.set(x, y, Tile::Floor);
            if (map.fixture_id(x, y) >= 0)
                map.remove_fixture(x, y);
            map.add_fixture(x, y, make_fixture(style.gate));
        } else {
            if (style.decay > 0.0f && decay_dist(rng) < style.decay)
                return;
            map.set(x, y, style.perimeter_wall);
            if (map.fixture_id(x, y) >= 0)
                map.remove_fixture(x, y);
        }
    };

    // North wall
    for (int x = b.x; x < b.x + b.w; ++x) {
        float n = fbm(static_cast<float>(x) * 0.3f, 0.0f, noise_seed, 1.0f, 2);
        int offset = static_cast<int>((n - 0.5f) * 3.0f);  // -1 to +1 tiles
        place_wall(x, b.y + offset);
    }
    // South wall
    for (int x = b.x; x < b.x + b.w; ++x) {
        float n = fbm(static_cast<float>(x) * 0.3f, 100.0f, noise_seed, 1.0f, 2);
        int offset = static_cast<int>((n - 0.5f) * 3.0f);
        place_wall(x, b.y + b.h - 1 + offset);
    }
    // West wall
    for (int y = b.y; y < b.y + b.h; ++y) {
        float n = fbm(0.0f, static_cast<float>(y) * 0.3f, noise_seed + 1, 1.0f, 2);
        int offset = static_cast<int>((n - 0.5f) * 3.0f);
        place_wall(b.x + offset, y);
    }
    // East wall
    for (int y = b.y; y < b.y + b.h; ++y) {
        float n = fbm(100.0f, static_cast<float>(y) * 0.3f, noise_seed + 1, 1.0f, 2);
        int offset = static_cast<int>((n - 0.5f) * 3.0f);
        place_wall(b.x + b.w - 1 + offset, y);
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
