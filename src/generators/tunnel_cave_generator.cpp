#include "astra/map_generator.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace astra {

class TunnelCaveGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;

private:
    void place_pond(int cx, int cy, int radius, Tile tile, std::mt19937& rng);
    // Solid grid (true = wall)
    std::vector<bool> cells_;
    // Region label per cell
    std::vector<int> labels_;

    int cell_idx(int x, int y) const { return y * map_->width() + x; }

    // Carve a drunkard's walk tunnel from (x,y), returning carved cells
    std::vector<std::pair<int,int>> walk_tunnel(int x, int y, int steps,
                                                 std::mt19937& rng);
    // Carve a small chamber around a point
    void carve_chamber(int cx, int cy, int radius, int region_id);
    // Flood-fill open cells
    std::vector<std::pair<int,int>> flood_fill(int x, int y, int label);
};

std::vector<std::pair<int,int>> TunnelCaveGenerator::walk_tunnel(
        int x, int y, int steps, std::mt19937& rng) {
    static constexpr int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    std::vector<std::pair<int,int>> carved;
    std::uniform_int_distribution<int> dir_dist(0, 3);
    // Bias toward continuing in the same direction for longer straight runs
    int prev_dir = dir_dist(rng);

    for (int i = 0; i < steps; ++i) {
        // 60% chance to continue in same direction, 40% to pick a new one
        int d;
        if (rng() % 5 < 3) {
            d = prev_dir;
        } else {
            d = dir_dist(rng);
        }
        prev_dir = d;

        int nx = x + dirs[d][0];
        int ny = y + dirs[d][1];

        // Stay within bounds (leave 1-tile border)
        if (nx < 2 || nx >= map_->width() - 2 ||
            ny < 2 || ny >= map_->height() - 2) {
            continue;
        }

        x = nx;
        y = ny;

        if (cells_[cell_idx(x, y)]) {
            cells_[cell_idx(x, y)] = false;
            carved.push_back({x, y});
        }
    }
    return carved;
}

void TunnelCaveGenerator::carve_chamber(int cx, int cy, int radius, int region_id) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Elliptical shape — slightly wider than tall
            float dist = static_cast<float>(dx * dx) / static_cast<float>(radius * radius + 1)
                       + static_cast<float>(dy * dy) / static_cast<float>(radius * radius);
            if (dist > 1.0f) continue;

            int nx = cx + dx, ny = cy + dy;
            if (nx < 1 || nx >= map_->width() - 1 ||
                ny < 1 || ny >= map_->height() - 1) continue;

            cells_[cell_idx(nx, ny)] = false;
            map_->set(nx, ny, Tile::Floor);
            map_->set_region(nx, ny, region_id);
        }
    }
}

std::vector<std::pair<int,int>> TunnelCaveGenerator::flood_fill(int x, int y, int label) {
    std::vector<std::pair<int,int>> result;
    std::queue<std::pair<int,int>> q;
    q.push({x, y});
    labels_[cell_idx(x, y)] = label;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        result.push_back({cx, cy});

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx != 0 && dy != 0) continue; // 4-connected
                int nx = cx + dx, ny = cy + dy;
                if (!in_bounds(nx, ny)) continue;
                int idx = cell_idx(nx, ny);
                if (!cells_[idx] && labels_[idx] < 0) {
                    labels_[idx] = label;
                    q.push({nx, ny});
                }
            }
        }
    }
    return result;
}

void TunnelCaveGenerator::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Start entirely solid
    cells_.assign(w * h, true);

    // Determine number of chambers (rooms) from props
    int num_chambers = std::max(props_->room_count_min,
        std::uniform_int_distribution<int>(props_->room_count_min,
                                            props_->room_count_max)(rng));

    // Place chambers at random positions with some spacing
    struct Chamber { int x, y, radius; };
    std::vector<Chamber> chambers;
    std::uniform_int_distribution<int> rad_dist(2, 4);
    std::uniform_int_distribution<int> lit_chance(0, 99);

    int margin = 6;
    std::uniform_int_distribution<int> x_dist(margin, w - margin - 1);
    std::uniform_int_distribution<int> y_dist(margin, h - margin - 1);

    for (int attempt = 0; attempt < num_chambers * 10 &&
         static_cast<int>(chambers.size()) < num_chambers; ++attempt) {
        int cx = x_dist(rng);
        int cy = y_dist(rng);
        int rad = rad_dist(rng);

        // Check spacing from existing chambers
        bool too_close = false;
        for (const auto& ch : chambers) {
            int dx = cx - ch.x;
            int dy = cy - ch.y;
            if (dx * dx + dy * dy < 100) { // min ~10 tiles apart
                too_close = true;
                break;
            }
        }
        if (too_close) continue;

        // Create region and carve chamber
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = lit_chance(rng) < props_->light_bias;
        int rid = map_->add_region(reg);

        carve_chamber(cx, cy, rad, rid);
        rooms_.push_back({cx - rad, cy - rad, cx + rad, cy + rad});
        chambers.push_back({cx, cy, rad});
    }

    // Ensure at least one chamber
    if (chambers.empty()) {
        int cx = w / 2, cy = h / 2;
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true;
        int rid = map_->add_region(reg);
        carve_chamber(cx, cy, 3, rid);
        rooms_.push_back({cx - 3, cy - 3, cx + 3, cy + 3});
        chambers.push_back({cx, cy, 3});
    }

    // Run drunkard's walk tunnels between consecutive chambers
    // Each walk starts at one chamber and wanders toward the next
    for (size_t i = 0; i + 1 < chambers.size(); ++i) {
        int sx = chambers[i].x, sy = chambers[i].y;
        int dist = std::abs(chambers[i+1].x - sx) + std::abs(chambers[i+1].y - sy);
        int steps = dist * 3; // generous step budget

        // Walk from chamber i toward chamber i+1 with bias
        int cx = sx, cy = sy;
        int tx = chambers[i+1].x, ty = chambers[i+1].y;
        static constexpr int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
        std::uniform_int_distribution<int> dir_dist(0, 3);

        for (int s = 0; s < steps; ++s) {
            // 50% biased toward target, 50% random
            int d;
            if (rng() % 2 == 0) {
                // Pick direction toward target
                int dx_t = tx - cx, dy_t = ty - cy;
                if (std::abs(dx_t) > std::abs(dy_t)) {
                    d = (dx_t > 0) ? 3 : 2; // right or left
                } else if (dy_t != 0) {
                    d = (dy_t > 0) ? 1 : 0; // down or up
                } else {
                    break; // reached target
                }
            } else {
                d = dir_dist(rng);
            }

            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];

            if (nx < 2 || nx >= w - 2 || ny < 2 || ny >= h - 2) continue;

            cx = nx;
            cy = ny;
            cells_[cell_idx(cx, cy)] = false;
        }
    }

    // Add a few extra random exploration tunnels branching off chambers
    int extra_tunnels = std::uniform_int_distribution<int>(2, 5)(rng);
    for (int t = 0; t < extra_tunnels; ++t) {
        int ci = std::uniform_int_distribution<int>(0,
            static_cast<int>(chambers.size()) - 1)(rng);
        int steps = std::uniform_int_distribution<int>(40, 120)(rng);
        walk_tunnel(chambers[ci].x, chambers[ci].y, steps, rng);
    }

    // Apply carved cells to tilemap (assign tunnel regions)
    // First collect connected tunnel networks via flood fill
    labels_.assign(w * h, -1);
    // Mark chamber cells as already labeled (they have regions)
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (!cells_[cell_idx(x, y)] && map_->region_id(x, y) >= 0) {
                labels_[cell_idx(x, y)] = map_->region_id(x, y);
            }
        }
    }

    // Assign corridor regions to tunnel cells not yet owned
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (!cells_[cell_idx(x, y)] && labels_[cell_idx(x, y)] < 0) {
                Region creg;
                creg.type = RegionType::Corridor;
                creg.lit = false;
                int crid = map_->add_region(creg);

                auto cells = flood_fill(x, y, crid);
                for (auto [fx, fy] : cells) {
                    map_->set(fx, fy, Tile::Floor);
                    map_->set_region(fx, fy, crid);
                }
            }
        }
    }

    // Set wall tiles adjacent to floor
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (!cells_[cell_idx(x, y)]) {
                // Already floor
                if (map_->get(x, y) != Tile::Floor) {
                    map_->set(x, y, Tile::Floor);
                }
                continue;
            }
            // Check if adjacent to any open cell
            bool near_floor = false;
            for (int dy = -1; dy <= 1 && !near_floor; ++dy) {
                for (int dx = -1; dx <= 1 && !near_floor; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (in_bounds(nx, ny) && !cells_[cell_idx(nx, ny)]) {
                        near_floor = true;
                    }
                }
            }
            if (near_floor) {
                map_->set(x, y, Tile::Wall);
            }
        }
    }
}

void TunnelCaveGenerator::connect_rooms(std::mt19937& /*rng*/) {
    // Connectivity is handled in generate_layout via biased walks
    // Nothing to do here — tunnels already connect all chambers
}

void TunnelCaveGenerator::place_pond(int cx, int cy, int radius, Tile tile, std::mt19937& rng) {
    std::queue<std::pair<int,int>> q;
    q.push({cx, cy});

    auto key = [&](int x, int y) { return y * map_->width() + x; };
    std::vector<bool> seen(map_->width() * map_->height(), false);
    seen[key(cx, cy)] = true;

    std::uniform_int_distribution<int> chance(0, 99);

    while (!q.empty()) {
        auto [px, py] = q.front();
        q.pop();

        if (map_->get(px, py) != Tile::Floor) continue;

        int dx = px - cx, dy = py - cy;
        if (dx * dx + dy * dy > radius * radius) continue;

        map_->set(px, py, tile);

        static constexpr int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
        for (auto [ddx, ddy] : dirs) {
            int nx = px + ddx, ny = py + ddy;
            if (!in_bounds(nx, ny)) continue;
            if (seen[key(nx, ny)]) continue;
            seen[key(nx, ny)] = true;
            if (chance(rng) < 60) {
                q.push({nx, ny});
            }
        }
    }
}

void TunnelCaveGenerator::place_features(std::mt19937& rng) {
    Biome biome = map_->biome();

    if (biome == Biome::Station || biome == Biome::Volcanic) return;

    Tile water_tile = (biome == Biome::Ice) ? Tile::Ice : Tile::Water;

    int pond_chance;
    switch (biome) {
        case Biome::Aquatic:  pond_chance = 100; break;
        case Biome::Fungal:   pond_chance = 50; break;
        case Biome::Corroded: pond_chance = 50; break;
        case Biome::Crystal:  pond_chance = 30; break;
        case Biome::Ice:      pond_chance = 60; break;
        default:              pond_chance = 40; break;
    }

    std::uniform_int_distribution<int> pct(0, 99);
    if (pct(rng) >= pond_chance && biome != Biome::Aquatic) return;

    // Place 1 pond in a random room (smaller than open caves)
    if (rooms_.empty()) return;

    std::uniform_int_distribution<int> room_dist(0, static_cast<int>(rooms_.size()) - 1);
    auto& room = rooms_[room_dist(rng)];
    int cx = std::uniform_int_distribution<int>(room.x1 + 1, std::max(room.x1 + 1, room.x2 - 1))(rng);
    int cy = std::uniform_int_distribution<int>(room.y1 + 1, std::max(room.y1 + 1, room.y2 - 1))(rng);
    place_pond(cx, cy, 2, water_tile, rng);
}

std::unique_ptr<MapGenerator> make_tunnel_cave_generator() {
    return std::make_unique<TunnelCaveGenerator>();
}

} // namespace astra
