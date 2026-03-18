#include "astra/map_generator.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace astra {

class OpenCaveGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;

private:
    void place_pond(int cx, int cy, int radius, Tile tile, std::mt19937& rng);
    void place_river(std::mt19937& rng, Tile tile);
    // Cellular automata grid (true = wall)
    std::vector<bool> cells_;
    // Region label per cell (-1 = wall/unassigned)
    std::vector<int> labels_;

    int cell_idx(int x, int y) const { return y * map_->width() + x; }
    int count_wall_neighbors(int x, int y) const;
    std::vector<std::pair<int,int>> flood_fill(int x, int y, int label);
    void dig_tunnel(int x1, int y1, int x2, int y2, int region_id, std::mt19937& rng);
};

int OpenCaveGenerator::count_wall_neighbors(int x, int y) const {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny) || cells_[cell_idx(nx, ny)]) {
                ++count;
            }
        }
    }
    return count;
}

std::vector<std::pair<int,int>> OpenCaveGenerator::flood_fill(int x, int y, int label) {
    std::vector<std::pair<int,int>> cells;
    std::queue<std::pair<int,int>> q;
    q.push({x, y});
    labels_[cell_idx(x, y)] = label;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        cells.push_back({cx, cy});

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
    return cells;
}

void OpenCaveGenerator::dig_tunnel(int x1, int y1, int x2, int y2, int region_id, std::mt19937& rng) {
    int cx = x1, cy = y1;
    std::uniform_int_distribution<int> jitter(-1, 1);
    int max_steps = (map_->width() + map_->height()) * 3;

    auto carve_cell = [&](int x, int y) {
        cells_[cell_idx(x, y)] = false;
        map_->set(x, y, Tile::Floor);
        // Only assign corridor region to cells not already owned by a room
        if (map_->region_id(x, y) < 0) {
            map_->set_region(x, y, region_id);
        }
    };

    for (int step = 0; step < max_steps && (cx != x2 || cy != y2); ++step) {
        // Bias toward target
        int dx = (x2 > cx) ? 1 : (x2 < cx) ? -1 : 0;
        int dy = (y2 > cy) ? 1 : (y2 < cy) ? -1 : 0;

        // Random jitter for organic feel
        if (rng() % 3 == 0) {
            if (rng() % 2 == 0) dx += jitter(rng);
            else dy += jitter(rng);
        }

        // Clamp to single step
        if (dx > 1) dx = 1; if (dx < -1) dx = -1;
        if (dy > 1) dy = 1; if (dy < -1) dy = -1;
        // Move in one axis at a time
        if (dx != 0 && dy != 0) {
            if (rng() % 2 == 0) dy = 0;
            else dx = 0;
        }
        if (dx == 0 && dy == 0) continue;

        cx += dx;
        cy += dy;

        if (!in_bounds(cx, cy)) {
            cx -= dx;
            cy -= dy;
            continue;
        }

        carve_cell(cx, cy);

        // Also carve one neighbor for slightly wider passages
        int nx = cx + jitter(rng);
        int ny = cy + jitter(rng);
        if (in_bounds(nx, ny)) {
            carve_cell(nx, ny);
        }
    }
}

void OpenCaveGenerator::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Initialize cellular automata — ~45% walls
    cells_.assign(w * h, false);
    std::uniform_int_distribution<int> fill_chance(0, 99);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Border is always wall
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                cells_[cell_idx(x, y)] = true;
            } else {
                cells_[cell_idx(x, y)] = fill_chance(rng) < 45;
            }
        }
    }

    // Smoothing passes
    for (int pass = 0; pass < 5; ++pass) {
        std::vector<bool> next = cells_;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int walls = count_wall_neighbors(x, y);
                next[cell_idx(x, y)] = (walls >= 5);
            }
        }
        cells_ = std::move(next);
    }

    // Flood-fill to identify connected caverns
    labels_.assign(w * h, -1);
    std::vector<std::vector<std::pair<int,int>>> caverns; // cells per cavern

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (!cells_[cell_idx(x, y)] && labels_[cell_idx(x, y)] < 0) {
                int label = static_cast<int>(caverns.size());
                caverns.push_back(flood_fill(x, y, label));
            }
        }
    }

    // Remove tiny caverns (< 20 cells) by filling them back in
    for (size_t i = 0; i < caverns.size(); ) {
        if (caverns[i].size() < 20) {
            for (auto [cx, cy] : caverns[i]) {
                cells_[cell_idx(cx, cy)] = true;
                labels_[cell_idx(cx, cy)] = -1;
            }
            caverns.erase(caverns.begin() + static_cast<int>(i));
            // Re-label remaining
            for (size_t j = i; j < caverns.size(); ++j) {
                for (auto [cx, cy] : caverns[j]) {
                    labels_[cell_idx(cx, cy)] = static_cast<int>(j);
                }
            }
        } else {
            ++i;
        }
    }

    // Ensure at least one cavern exists — carve a fallback room
    if (caverns.empty()) {
        int cx = w / 2, cy = h / 2;
        int rad = 5;
        std::vector<std::pair<int,int>> fallback;
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (in_bounds(nx, ny) && dx * dx + dy * dy <= rad * rad) {
                    cells_[cell_idx(nx, ny)] = false;
                    labels_[cell_idx(nx, ny)] = 0;
                    fallback.push_back({nx, ny});
                }
            }
        }
        caverns.push_back(std::move(fallback));
    }

    // Apply cells to tilemap and create regions
    std::uniform_int_distribution<int> lit_chance(0, 99);
    for (size_t i = 0; i < caverns.size(); ++i) {
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = lit_chance(rng) < props_->light_bias;
        int rid = map_->add_region(reg);

        // Compute bounding box for RoomRect
        int bx1 = w, by1 = h, bx2 = 0, by2 = 0;
        for (auto [cx, cy] : caverns[i]) {
            map_->set(cx, cy, Tile::Floor);
            map_->set_region(cx, cy, rid);
            bx1 = std::min(bx1, cx);
            by1 = std::min(by1, cy);
            bx2 = std::max(bx2, cx);
            by2 = std::max(by2, cy);
        }
        rooms_.push_back({bx1, by1, bx2, by2});
    }

    // Set wall tiles for all remaining solid cells (except border Empty)
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (cells_[cell_idx(x, y)]) {
                // Only set as wall if adjacent to a floor tile
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
}

void OpenCaveGenerator::connect_rooms(std::mt19937& rng) {
    if (rooms_.size() <= 1) return;

    // Connect each cavern to the nearest unconnected one
    std::vector<bool> connected(rooms_.size(), false);
    connected[0] = true;

    for (size_t count = 1; count < rooms_.size(); ++count) {
        int best_from = -1, best_to = -1;
        double best_dist = 1e18;

        for (size_t i = 0; i < rooms_.size(); ++i) {
            if (!connected[i]) continue;
            int cx1 = (rooms_[i].x1 + rooms_[i].x2) / 2;
            int cy1 = (rooms_[i].y1 + rooms_[i].y2) / 2;

            for (size_t j = 0; j < rooms_.size(); ++j) {
                if (connected[j]) continue;
                int cx2 = (rooms_[j].x1 + rooms_[j].x2) / 2;
                int cy2 = (rooms_[j].y1 + rooms_[j].y2) / 2;

                double dist = std::sqrt((cx2 - cx1) * (cx2 - cx1) +
                                        (cy2 - cy1) * (cy2 - cy1));
                if (dist < best_dist) {
                    best_dist = dist;
                    best_from = static_cast<int>(i);
                    best_to = static_cast<int>(j);
                }
            }
        }

        if (best_from < 0) break;

        // Create corridor region
        Region creg;
        creg.type = RegionType::Corridor;
        creg.lit = false;
        int crid = map_->add_region(creg);

        int fx = (rooms_[best_from].x1 + rooms_[best_from].x2) / 2;
        int fy = (rooms_[best_from].y1 + rooms_[best_from].y2) / 2;
        int tx = (rooms_[best_to].x1 + rooms_[best_to].x2) / 2;
        int ty = (rooms_[best_to].y1 + rooms_[best_to].y2) / 2;

        dig_tunnel(fx, fy, tx, ty, crid, rng);
        connected[best_to] = true;
    }
}

void OpenCaveGenerator::place_pond(int cx, int cy, int radius, Tile tile, std::mt19937& rng) {
    // BFS blob from center — each neighbor has ~60% chance of inclusion
    std::queue<std::pair<int,int>> q;
    std::vector<std::pair<int,int>> visited;
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
        visited.push_back({px, py});

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

void OpenCaveGenerator::place_river(std::mt19937& rng, Tile tile) {
    int w = map_->width();
    int h = map_->height();

    // Pick a random start on the left or top edge
    bool horizontal = rng() % 2 == 0;
    int cx, cy;
    if (horizontal) {
        cx = 1;
        cy = std::uniform_int_distribution<int>(5, h - 6)(rng);
    } else {
        cx = std::uniform_int_distribution<int>(5, w - 6)(rng);
        cy = 1;
    }

    std::uniform_int_distribution<int> jitter(-1, 1);
    int max_steps = horizontal ? w : h;

    for (int step = 0; step < max_steps * 2; ++step) {
        if (!in_bounds(cx, cy)) break;

        if (map_->get(cx, cy) == Tile::Floor) {
            map_->set(cx, cy, tile);
        } else if (map_->get(cx, cy) == Tile::Wall) {
            break; // river disappears into rock
        }

        // Advance in primary direction with perpendicular jitter
        if (horizontal) {
            cx += 1;
            cy += (rng() % 3 == 0) ? jitter(rng) : 0;
        } else {
            cy += 1;
            cx += (rng() % 3 == 0) ? jitter(rng) : 0;
        }
        cy = std::max(1, std::min(cy, h - 2));
        cx = std::max(1, std::min(cx, w - 2));
    }
}

void OpenCaveGenerator::place_features(std::mt19937& rng) {
    Biome biome = map_->biome();

    // No water features for stations or volcanic biomes
    if (biome == Biome::Station || biome == Biome::Volcanic) return;

    Tile water_tile = (biome == Biome::Ice) ? Tile::Ice : Tile::Water;

    // Determine chance of water features
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

    // Place 1-2 ponds in random rooms
    int pond_count = std::uniform_int_distribution<int>(1, 2)(rng);
    if (rooms_.empty()) return;

    std::uniform_int_distribution<int> room_dist(0, static_cast<int>(rooms_.size()) - 1);
    for (int i = 0; i < pond_count; ++i) {
        auto& room = rooms_[room_dist(rng)];
        int cx = std::uniform_int_distribution<int>(room.x1 + 2, std::max(room.x1 + 2, room.x2 - 2))(rng);
        int cy = std::uniform_int_distribution<int>(room.y1 + 2, std::max(room.y1 + 2, room.y2 - 2))(rng);
        int radius = std::uniform_int_distribution<int>(2, 3)(rng);
        place_pond(cx, cy, radius, water_tile, rng);
    }

    // River: 30% chance (50% for Aquatic)
    int river_chance = (biome == Biome::Aquatic) ? 50 : 30;
    if (pct(rng) < river_chance) {
        place_river(rng, water_tile);
    }
}

std::unique_ptr<MapGenerator> make_open_cave_generator() {
    return std::make_unique<OpenCaveGenerator>();
}

} // namespace astra
