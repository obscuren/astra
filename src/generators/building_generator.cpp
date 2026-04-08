#include "astra/building_generator.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

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

    // --- Step 6: Interior furnishing (rule-based placement) ---
    auto palette = furniture_palette(spec.type, style);

    const auto& pr = shape.primary; // primary rect shorthand

    // 6a. Collect interior tiles and classify them.
    struct InteriorTile {
        int x, y;
        bool wall_adjacent = false;
    };
    std::vector<InteriorTile> all_interior;

    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (!in_shape(x, y, shape)) continue;
            if (is_shape_edge(x, y, shape)) continue;
            if (is_door(x, y, shape)) continue;

            // Skip tiles within 1 of a door (interior side)
            bool near_door = false;
            for (auto& [dpx, dpy] : shape.door_positions) {
                if (std::abs(x - dpx) + std::abs(y - dpy) <= 1) {
                    if (is_shape_edge(dpx, dpy, shape)) {
                        near_door = true;
                        break;
                    }
                }
            }
            if (near_door) continue;

            InteriorTile it;
            it.x = x;
            it.y = y;

            static constexpr int ddx[] = {0, 0, -1, 1};
            static constexpr int ddy[] = {-1, 1, 0, 0};
            for (int d = 0; d < 4; ++d) {
                if (is_shape_edge(x + ddx[d], y + ddy[d], shape)) {
                    it.wall_adjacent = true;
                    break;
                }
            }
            all_interior.push_back(it);
        }
    }

    // 6b. Detect narrow passages — columns < 4 tiles wide in either dimension.
    // Build a set of tiles to exclude from furnishing.
    auto tile_key = [](int x, int y) { return static_cast<int64_t>(x) * 100000 + y; };
    std::set<int64_t> narrow_zones;

    // For each interior tile, measure horizontal and vertical span of contiguous interior
    auto is_interior = [&](int x, int y) -> bool {
        if (!in_bounds(x, y, map)) return false;
        if (!in_shape(x, y, shape)) return false;
        if (is_shape_edge(x, y, shape)) return false;
        return true;
    };

    for (auto& it : all_interior) {
        // Horizontal span
        int hspan = 1;
        for (int tx = it.x - 1; is_interior(tx, it.y); --tx) ++hspan;
        for (int tx = it.x + 1; is_interior(tx, it.y); ++tx) ++hspan;
        // Vertical span
        int vspan = 1;
        for (int ty = it.y - 1; is_interior(it.x, ty); --ty) ++vspan;
        for (int ty = it.y + 1; is_interior(it.x, ty); ++ty) ++vspan;

        if (hspan < 4 && vspan < 4) {
            narrow_zones.insert(tile_key(it.x, it.y));
        }
    }

    // Helper: can we place a fixture at (x,y)?
    auto can_place = [&](int x, int y) -> bool {
        if (!in_bounds(x, y, map)) return false;
        if (!in_shape(x, y, shape)) return false;
        if (is_shape_edge(x, y, shape)) return false;
        if (is_door(x, y, shape)) return false;
        if (map.fixture_id(x, y) != -1) return false;
        if (narrow_zones.count(tile_key(x, y))) return false;
        // Check near-door
        for (auto& [dpx, dpy] : shape.door_positions) {
            if (std::abs(x - dpx) + std::abs(y - dpy) <= 1) {
                if (is_shape_edge(dpx, dpy, shape)) return false;
            }
        }
        return true;
    };

    // 6c. Determine primary door and back wall direction.
    // Door wall: which wall of the primary rect the first door is on.
    enum class Wall { North, South, East, West };
    Wall door_wall = Wall::South;
    if (!shape.door_positions.empty()) {
        auto [dpx, dpy] = shape.door_positions[0];
        int dist_n = std::abs(dpy - pr.y);
        int dist_s = std::abs(dpy - (pr.y + pr.h - 1));
        int dist_w = std::abs(dpx - pr.x);
        int dist_e = std::abs(dpx - (pr.x + pr.w - 1));
        int mn = std::min({dist_n, dist_s, dist_w, dist_e});
        if (mn == dist_n) door_wall = Wall::North;
        else if (mn == dist_s) door_wall = Wall::South;
        else if (mn == dist_w) door_wall = Wall::West;
        else door_wall = Wall::East;
    }

    // Back wall is opposite the door wall
    Wall back_wall;
    switch (door_wall) {
        case Wall::North: back_wall = Wall::South; break;
        case Wall::South: back_wall = Wall::North; break;
        case Wall::East:  back_wall = Wall::West;  break;
        case Wall::West:  back_wall = Wall::East;  break;
    }

    // 6d. Process groups in palette order.
    for (auto& group : palette.groups) {
        if (chance(rng) > group.frequency) continue;

        int count = group.min_count;
        if (group.max_count > group.min_count) {
            std::uniform_int_distribution<int> cd(group.min_count, group.max_count);
            count = cd(rng);
        }

        switch (group.rule) {

        case PlacementRule::Anchor: {
            // Place at center of back wall, one tile inward.
            int ax = 0, ay = 0;
            switch (back_wall) {
                case Wall::North:
                    ax = pr.x + pr.w / 2;
                    ay = pr.y + 1;
                    break;
                case Wall::South:
                    ax = pr.x + pr.w / 2;
                    ay = pr.y + pr.h - 2;
                    break;
                case Wall::West:
                    ax = pr.x + 1;
                    ay = pr.y + pr.h / 2;
                    break;
                case Wall::East:
                    ax = pr.x + pr.w - 2;
                    ay = pr.y + pr.h / 2;
                    break;
            }
            if (can_place(ax, ay)) {
                map.add_fixture(ax, ay, make_fixture(group.primary));
            }
            break;
        }

        case PlacementRule::TableSet: {
            // Place table+bench units in center area.
            // Direction: horizontal if wider, vertical if taller.
            bool horizontal = pr.w >= pr.h;
            // Find center area bounds (non-wall-adjacent interior)
            int cx0 = pr.x + 2, cy0 = pr.y + 2;
            int cx1 = pr.x + pr.w - 3, cy1 = pr.y + pr.h - 3;

            int placed = 0;
            if (horizontal) {
                // Each unit: 3 tiles wide (bench, table, bench), stacked vertically with 2-tile gap
                for (int uy = cy0; uy <= cy1 && placed < count; uy += 3) {
                    int mid_x = (cx0 + cx1) / 2;
                    int lx = mid_x - 1, rx = mid_x + 1;
                    if (can_place(lx, uy) && can_place(mid_x, uy) && can_place(rx, uy)) {
                        map.add_fixture(lx, uy, make_fixture(group.secondary));   // bench
                        map.add_fixture(mid_x, uy, make_fixture(group.primary));  // table
                        map.add_fixture(rx, uy, make_fixture(group.secondary));   // bench
                        ++placed;
                    }
                }
            } else {
                // Each unit: 3 tiles tall (bench, table, bench), stacked horizontally with 2-tile gap
                for (int ux = cx0; ux <= cx1 && placed < count; ux += 3) {
                    int mid_y = (cy0 + cy1) / 2;
                    int ty = mid_y - 1, by = mid_y + 1;
                    if (can_place(ux, ty) && can_place(ux, mid_y) && can_place(ux, by)) {
                        map.add_fixture(ux, ty, make_fixture(group.secondary));   // bench
                        map.add_fixture(ux, mid_y, make_fixture(group.primary));  // table
                        map.add_fixture(ux, by, make_fixture(group.secondary));   // bench
                        ++placed;
                    }
                }
            }
            break;
        }

        case PlacementRule::WallShelf: {
            // Walk perimeter, place 3-tile shelf structures at intervals.
            std::vector<std::pair<int,int>> perim;
            walk_perimeter(pr, perim);
            for (auto& ext : shape.extensions) {
                walk_perimeter(ext, perim);
            }

            int placed = 0;
            int tiles_walked = 0;
            int spacing = 5;

            for (size_t i = 0; i + 2 < perim.size() && placed < count; ++i) {
                auto [wx, wy] = perim[i];
                ++tiles_walked;
                if (tiles_walked < spacing) continue;
                if (!is_shape_edge(wx, wy, shape)) continue;
                if (is_any_corner(wx, wy, shape)) continue;

                // Skip near doors (within 3 tiles)
                bool near_d = false;
                for (auto& [dpx, dpy] : shape.door_positions) {
                    if (std::abs(wx - dpx) + std::abs(wy - dpy) <= 3) {
                        near_d = true;
                        break;
                    }
                }
                if (near_d) continue;

                // Determine inward direction and 3-tile run direction.
                // For north wall (wy == pr.y): interior is y+1, run along x
                // For south wall (wy == pr.y+pr.h-1): interior is y-1, run along x
                // For west wall (wx == pr.x): interior is x+1, run along y
                // For east wall (wx == pr.x+pr.w-1): interior is x-1, run along y

                int ix = 0, iy = 0; // inward offset
                int rx = 0, ry = 0; // run direction
                if (wy == pr.y) { iy = 1; rx = 1; }
                else if (wy == pr.y + pr.h - 1) { iy = -1; rx = 1; }
                else if (wx == pr.x) { ix = 1; ry = 1; }
                else if (wx == pr.x + pr.w - 1) { ix = -1; ry = 1; }
                else continue; // not on primary rect edge

                // The 3 interior tiles adjacent to this wall segment
                // Start tile is at the wall position + inward offset
                int sx = wx + ix, sy = wy + iy;
                int t0x = sx, t0y = sy;
                int t1x = sx + rx, t1y = sy + ry;
                int t2x = sx + rx * 2, t2y = sy + ry * 2;

                if (can_place(t0x, t0y) && can_place(t1x, t1y) && can_place(t2x, t2y)) {
                    map.add_fixture(t0x, t0y, make_fixture(group.primary));
                    map.add_fixture(t1x, t1y, make_fixture(group.secondary));
                    map.add_fixture(t2x, t2y, make_fixture(group.primary));
                    ++placed;
                    tiles_walked = 0;
                }
            }
            break;
        }

        case PlacementRule::WallUniform: {
            // Walk perimeter, place at even intervals on wall-adjacent interior tiles.
            std::vector<std::pair<int,int>> perim;
            walk_perimeter(pr, perim);
            for (auto& ext : shape.extensions) {
                walk_perimeter(ext, perim);
            }

            // Collect valid wall-adjacent interior positions
            struct WallSpot { int x, y; };
            std::vector<WallSpot> spots;
            for (auto& [wx, wy] : perim) {
                if (!is_shape_edge(wx, wy, shape)) continue;
                if (is_any_corner(wx, wy, shape)) continue;

                // Skip near doors
                bool near_d = false;
                for (auto& [dpx, dpy] : shape.door_positions) {
                    if (std::abs(wx - dpx) + std::abs(wy - dpy) <= 2) {
                        near_d = true;
                        break;
                    }
                }
                if (near_d) continue;

                // Find the interior tile adjacent to this wall tile
                int ix = wx, iy = wy;
                if (wy == pr.y) iy = wy + 1;
                else if (wy == pr.y + pr.h - 1) iy = wy - 1;
                else if (wx == pr.x) ix = wx + 1;
                else if (wx == pr.x + pr.w - 1) ix = wx - 1;
                else continue;

                if (can_place(ix, iy)) {
                    spots.push_back({ix, iy});
                }
            }

            if (spots.empty()) break;

            int spacing_w = std::max(1, static_cast<int>(spots.size()) / (count + 1));
            int placed = 0;
            for (int si = spacing_w - 1; si < static_cast<int>(spots.size()) && placed < count; si += spacing_w) {
                auto& s = spots[si];
                if (can_place(s.x, s.y)) {
                    map.add_fixture(s.x, s.y, make_fixture(group.primary));
                    ++placed;
                }
            }
            break;
        }

        case PlacementRule::Corner: {
            // Interior corners of primary rect
            std::pair<int,int> corners[4] = {
                {pr.x + 1, pr.y + 1},
                {pr.x + pr.w - 2, pr.y + 1},
                {pr.x + 1, pr.y + pr.h - 2},
                {pr.x + pr.w - 2, pr.y + pr.h - 2},
            };

            // Shuffle corner order for variety
            for (int i = 3; i > 0; --i) {
                std::uniform_int_distribution<int> di(0, i);
                std::swap(corners[i], corners[di(rng)]);
            }

            int placed = 0;
            for (auto [cx, cy] : corners) {
                if (placed >= count) break;

                // Skip corners near doors (Manhattan distance <= 2)
                bool near_d = false;
                for (auto& [dpx, dpy] : shape.door_positions) {
                    if (std::abs(cx - dpx) + std::abs(cy - dpy) <= 2) {
                        near_d = true;
                        break;
                    }
                }
                if (near_d) continue;

                if (can_place(cx, cy)) {
                    map.add_fixture(cx, cy, make_fixture(group.primary));
                    ++placed;
                }
            }
            break;
        }

        case PlacementRule::Center: {
            // Place on non-wall-adjacent interior tiles
            std::vector<std::pair<int,int>> center_tiles;
            for (auto& it : all_interior) {
                if (it.wall_adjacent) continue;
                if (narrow_zones.count(tile_key(it.x, it.y))) continue;
                if (map.fixture_id(it.x, it.y) != -1) continue;
                center_tiles.push_back({it.x, it.y});
            }
            std::shuffle(center_tiles.begin(), center_tiles.end(), rng);

            int placed = 0;
            for (auto [cx, cy] : center_tiles) {
                if (placed >= count) break;
                if (can_place(cx, cy)) {
                    map.add_fixture(cx, cy, make_fixture(group.primary));
                    ++placed;
                }
            }
            break;
        }

        } // switch
    }

    // 6e. Walkability check — BFS from each door.
    // If removing a fixture opens a path to an otherwise unreachable interior tile, remove it.
    {
        // Collect all interior floor positions (including those with fixtures)
        std::set<int64_t> interior_set;
        for (auto& it : all_interior) {
            interior_set.insert(tile_key(it.x, it.y));
        }

        // BFS from all doors to find reachable tiles (only through non-fixture tiles)
        std::set<int64_t> reachable;
        std::queue<std::pair<int,int>> bfs_q;
        for (auto& [dpx, dpy] : shape.door_positions) {
            auto k = tile_key(dpx, dpy);
            if (reachable.insert(k).second) {
                bfs_q.push({dpx, dpy});
            }
        }

        static constexpr int ddx[] = {0, 0, -1, 1};
        static constexpr int ddy[] = {-1, 1, 0, 0};
        while (!bfs_q.empty()) {
            auto [bx, by] = bfs_q.front();
            bfs_q.pop();
            for (int d = 0; d < 4; ++d) {
                int nx = bx + ddx[d], ny = by + ddy[d];
                auto k = tile_key(nx, ny);
                if (!interior_set.count(k)) continue;
                if (reachable.count(k)) continue;
                if (map.fixture_id(nx, ny) != -1) continue; // blocked by fixture
                reachable.insert(k);
                bfs_q.push({nx, ny});
            }
        }

        // Find unreachable interior tiles that have no fixture (should be reachable)
        // If a fixture neighbor of an unreachable tile, when removed, would connect it, remove it
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& it : all_interior) {
                auto k = tile_key(it.x, it.y);
                if (reachable.count(k)) continue;
                if (map.fixture_id(it.x, it.y) != -1) continue; // this tile itself has fixture

                // This interior floor tile is unreachable. Try removing a blocking fixture neighbor.
                for (int d = 0; d < 4; ++d) {
                    int nx = it.x + ddx[d], ny = it.y + ddy[d];
                    if (map.fixture_id(nx, ny) == -1) continue;
                    auto nk = tile_key(nx, ny);
                    if (!interior_set.count(nk)) continue;

                    // Check if neighbor's other side is reachable
                    bool connects = false;
                    for (int d2 = 0; d2 < 4; ++d2) {
                        int rx = nx + ddx[d2], ry = ny + ddy[d2];
                        if (reachable.count(tile_key(rx, ry))) {
                            connects = true;
                            break;
                        }
                    }
                    if (connects) {
                        map.remove_fixture(nx, ny);
                        reachable.insert(nk);
                        reachable.insert(k);
                        // Re-flood from the newly opened tile
                        std::queue<std::pair<int,int>> flood;
                        flood.push({nx, ny});
                        flood.push({it.x, it.y});
                        while (!flood.empty()) {
                            auto [fx, fy] = flood.front();
                            flood.pop();
                            for (int d3 = 0; d3 < 4; ++d3) {
                                int gx = fx + ddx[d3], gy = fy + ddy[d3];
                                auto gk = tile_key(gx, gy);
                                if (!interior_set.count(gk)) continue;
                                if (reachable.count(gk)) continue;
                                if (map.fixture_id(gx, gy) != -1) continue;
                                reachable.insert(gk);
                                flood.push({gx, gy});
                            }
                        }
                        changed = true;
                        break;
                    }
                }
            }
        }
    }
}

} // namespace astra
