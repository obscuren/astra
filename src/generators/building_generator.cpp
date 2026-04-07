#include "astra/building_generator.h"

#include <algorithm>
#include <cmath>

namespace astra {

namespace {

// True if (x,y) is inside the primary rect or any extension.
bool in_shape(int x, int y, const BuildingShape& shape) {
    if (shape.primary.contains(x, y)) return true;
    for (auto& ext : shape.extensions) {
        if (ext.contains(x, y)) return true;
    }
    return false;
}

// True if (x,y) is on the shape boundary: inside shape but has a 4-neighbor outside.
bool is_shape_edge(int x, int y, const BuildingShape& shape) {
    if (!in_shape(x, y, shape)) return false;
    static constexpr int dx[] = {0, 0, -1, 1};
    static constexpr int dy[] = {-1, 1, 0, 0};
    for (int d = 0; d < 4; ++d) {
        if (!in_shape(x + dx[d], y + dy[d], shape)) return true;
    }
    return false;
}

// True if (x,y) is at a corner of the given rect.
bool is_rect_corner(int x, int y, const Rect& r) {
    int lx = r.x, rx = r.x + r.w - 1;
    int ty = r.y, by = r.y + r.h - 1;
    return (x == lx || x == rx) && (y == ty || y == by);
}

// True if any corner of any rect in the shape matches (x,y).
bool is_any_corner(int x, int y, const BuildingShape& shape) {
    if (is_rect_corner(x, y, shape.primary)) return true;
    for (auto& ext : shape.extensions) {
        if (is_rect_corner(x, y, ext)) return true;
    }
    return false;
}

// True if (x,y) IS a door position.
bool is_door(int x, int y, const BuildingShape& shape) {
    for (auto& [dx, dy] : shape.door_positions) {
        if (x == dx && y == dy) return true;
    }
    return false;
}

// Bounds check helper.
bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

// Walk the perimeter of a rect in order (clockwise), collecting edge tiles.
void walk_perimeter(const Rect& r, std::vector<std::pair<int,int>>& out) {
    // Top edge (left to right)
    for (int x = r.x; x < r.x + r.w; ++x) out.push_back({x, r.y});
    // Right edge (top+1 to bottom)
    for (int y = r.y + 1; y < r.y + r.h; ++y) out.push_back({r.x + r.w - 1, y});
    // Bottom edge (right-1 to left)
    for (int x = r.x + r.w - 2; x >= r.x; --x) out.push_back({x, r.y + r.h - 1});
    // Left edge (bottom-1 to top+1)
    for (int y = r.y + r.h - 2; y > r.y; --y) out.push_back({r.x, y});
}

} // namespace

void BuildingGenerator::generate(TileMap& map,
                                  const BuildingSpec& spec,
                                  const CivStyle& style,
                                  std::mt19937& rng) const {
    const auto& shape = spec.shape;
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    // Compute overall bounding box with 1-tile margin.
    int min_x = shape.primary.x, min_y = shape.primary.y;
    int max_x = shape.primary.x + shape.primary.w;
    int max_y = shape.primary.y + shape.primary.h;
    for (auto& ext : shape.extensions) {
        min_x = std::min(min_x, ext.x);
        min_y = std::min(min_y, ext.y);
        max_x = std::max(max_x, ext.x + ext.w);
        max_y = std::max(max_y, ext.y + ext.h);
    }
    min_x -= 1; min_y -= 1;
    max_x += 1; max_y += 1;

    // --- Step 1: Clear area (remove fixtures within footprint + margin) ---
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            if (!in_bounds(x, y, map)) continue;
            map.remove_fixture(x, y);
        }
    }

    // --- Step 2: Place floor tiles (entire shape interior) ---
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (in_shape(x, y, shape)) {
                map.set(x, y, style.floor_tile);
            }
        }
    }

    // --- Step 3: Place walls on shape boundary ---
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (!is_shape_edge(x, y, shape)) continue;
            // Never place a wall on a door position
            if (is_door(x, y, shape)) continue;
            // Ruined style: randomly skip wall tiles
            if (style.decay > 0.0f && chance(rng) < style.decay) continue;
            map.set(x, y, style.wall_tile);
        }
    }

    // --- Step 4: Place doors ---
    // Ensure door positions are on the shape edge. If a door position isn't
    // on the edge (off by one from planner), nudge it to the nearest edge tile.
    for (auto& [dpx, dpy] : shape.door_positions) {
        int dx = dpx, dy = dpy;
        if (!in_bounds(dx, dy, map)) continue;

        // If not on shape edge, or on a corner, find nearest non-corner edge tile
        if (!is_shape_edge(dx, dy, shape) || is_any_corner(dx, dy, shape)) {
            int best_dist = 999;
            for (int sy = dy - 2; sy <= dy + 2; ++sy) {
                for (int sx = dx - 2; sx <= dx + 2; ++sx) {
                    if (!in_bounds(sx, sy, map)) continue;
                    if (!is_shape_edge(sx, sy, shape)) continue;
                    if (is_any_corner(sx, sy, shape)) continue;
                    int d = std::abs(sx - dpx) + std::abs(sy - dpy);
                    if (d < best_dist) {
                        best_dist = d;
                        dx = sx;
                        dy = sy;
                    }
                }
            }
        }

        // Place the door: set floor tile and add Door fixture
        map.set(dx, dy, style.floor_tile);
        map.remove_fixture(dx, dy);
        map.add_fixture(dx, dy, make_fixture(FixtureType::Door));

        // Ensure at least one tile outside the door is passable (clear path to door)
        static constexpr int ndx[] = {0, 0, -1, 1};
        static constexpr int ndy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = dx + ndx[d], ny = dy + ndy[d];
            if (!in_bounds(nx, ny, map)) continue;
            if (!in_shape(nx, ny, shape)) {
                // This neighbor is outside — ensure it's walkable
                Tile t = map.get(nx, ny);
                if (t == Tile::Wall || t == Tile::StructuralWall) {
                    map.set(nx, ny, Tile::Floor);
                }
                map.remove_fixture(nx, ny);
                break;
            }
        }
    }

    // --- Step 5: Place windows — walk perimeter in order for proper spacing ---
    // Walk primary rect perimeter in order (clockwise)
    std::vector<std::pair<int,int>> perimeter;
    walk_perimeter(shape.primary, perimeter);
    for (auto& ext : shape.extensions) {
        walk_perimeter(ext, perimeter);
    }

    int tiles_since_window = 0;
    int window_spacing = 4 + (rng() % 2); // 4-5 tiles between windows

    for (auto& [wx, wy] : perimeter) {
        if (!in_bounds(wx, wy, map)) continue;
        // Must actually be a wall tile
        if (map.get(wx, wy) != style.wall_tile) {
            tiles_since_window = 0; // reset when hitting a non-wall (door, gap)
            continue;
        }
        // Skip corners
        if (is_any_corner(wx, wy, shape)) continue;

        ++tiles_since_window;
        if (tiles_since_window < window_spacing) continue;

        // Don't place window adjacent to a door
        bool adj_door = false;
        static constexpr int ndx[] = {0, 0, -1, 1};
        static constexpr int ndy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            if (is_door(wx + ndx[d], wy + ndy[d], shape)) {
                adj_door = true;
                break;
            }
        }
        if (adj_door) continue;

        // Decay roll
        if (style.decay > 0.0f && chance(rng) < style.decay) continue;

        map.add_fixture(wx, wy, make_fixture(FixtureType::Window));
        tiles_since_window = 0;
        window_spacing = 4 + (rng() % 2); // re-randomize spacing
    }

    // --- Step 6: Interior furnishing ---
    auto palette = furniture_palette(spec.type, style);

    // Categorize interior floor tiles.
    struct InteriorTile {
        int x, y;
        bool wall_adjacent = false;
        bool corner = false;
    };
    std::vector<InteriorTile> interiors;

    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (!in_shape(x, y, shape)) continue;
            if (is_shape_edge(x, y, shape)) continue;
            if (is_door(x, y, shape)) continue;
            // Don't place furniture directly in front of a door
            bool blocks_door = false;
            for (auto& [dpx, dpy] : shape.door_positions) {
                if (std::abs(x - dpx) + std::abs(y - dpy) == 1) {
                    // Check if this tile is on the interior side of the door
                    if (in_shape(x, y, shape) && is_shape_edge(dpx, dpy, shape)) {
                        blocks_door = true;
                        break;
                    }
                }
            }
            if (blocks_door) continue;

            InteriorTile it;
            it.x = x;
            it.y = y;

            // Check wall-adjacency
            static constexpr int ddx[] = {0, 0, -1, 1};
            static constexpr int ddy[] = {-1, 1, 0, 0};
            for (int d = 0; d < 4; ++d) {
                if (is_shape_edge(x + ddx[d], y + ddy[d], shape)) {
                    it.wall_adjacent = true;
                    break;
                }
            }

            // Corner: wall-adjacent from two perpendicular directions
            if (it.wall_adjacent) {
                bool adj_h = is_shape_edge(x - 1, y, shape) || is_shape_edge(x + 1, y, shape);
                bool adj_v = is_shape_edge(x, y - 1, shape) || is_shape_edge(x, y + 1, shape);
                it.corner = adj_h && adj_v;
            }

            interiors.push_back(it);
        }
    }

    // Shuffle for varied placement
    std::shuffle(interiors.begin(), interiors.end(), rng);

    // Cap total furniture at ~30% of interior space to keep rooms walkable
    int max_total = static_cast<int>(interiors.size()) * 3 / 10;
    if (max_total < 3) max_total = 3;
    int total_placed = 0;

    for (auto& entry : palette.entries) {
        if (total_placed >= max_total) break;
        if (chance(rng) > entry.frequency) continue;

        // Determine how many of this item to place
        int count = entry.min_count;
        if (entry.max_count > entry.min_count) {
            std::uniform_int_distribution<int> cd(entry.min_count, entry.max_count);
            count = cd(rng);
        }

        for (int placed = 0; placed < count && total_placed < max_total; ++placed) {
            int best = -1;
            for (int i = 0; i < static_cast<int>(interiors.size()); ++i) {
                auto& spot = interiors[i];
                if (map.fixture_id(spot.x, spot.y) != -1) continue;

                // Wall-adjacent constraint
                if (entry.wall_adjacent && !spot.wall_adjacent) continue;
                // Center preference: skip wall-adjacent tiles
                if (entry.prefers_center && spot.wall_adjacent) continue;

                // Clearance check
                if (entry.needs_clearance) {
                    bool has_clear = false;
                    static constexpr int ddx[] = {0, 0, -1, 1};
                    static constexpr int ddy[] = {-1, 1, 0, 0};
                    for (int d = 0; d < 4; ++d) {
                        int nx = spot.x + ddx[d], ny = spot.y + ddy[d];
                        if (!in_bounds(nx, ny, map)) continue;
                        if (!in_shape(nx, ny, shape)) continue;
                        if (is_shape_edge(nx, ny, shape)) continue;
                        if (map.fixture_id(nx, ny) != -1) continue;
                        has_clear = true;
                        break;
                    }
                    if (!has_clear) continue;
                }

                // Preference matching
                if (entry.prefers_corner && spot.corner) {
                    best = i;
                    break;
                }

                if (best == -1) best = i;
                if (!entry.prefers_corner) break;
            }

            if (best >= 0) {
                map.add_fixture(interiors[best].x, interiors[best].y,
                                make_fixture(entry.type));
                ++total_placed;
            } else {
                break;  // no more valid spots for this type
            }
        }
    }
}

} // namespace astra
