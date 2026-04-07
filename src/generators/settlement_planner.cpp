#include "astra/settlement_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace astra {

namespace {

// --- Building size ranges (min_w, max_w, min_h, max_h) ---

struct SizeRange {
    int min_w, max_w, min_h, max_h;
};

SizeRange size_range(BuildingType type) {
    switch (type) {
        case BuildingType::MainHall:   return {14, 20, 8, 12};
        case BuildingType::Market:     return {10, 14, 6, 9};
        case BuildingType::Dwelling:   return {7, 10,  5, 7};
        case BuildingType::Distillery: return {10, 14, 7, 9};
        case BuildingType::Lookout:    return {5, 7,   4, 6};
        case BuildingType::Workshop:   return {8, 12,  6, 8};
        case BuildingType::Storage:    return {7, 10,  5, 7};
    }
    return {7, 10, 5, 7};
}

// Random int in [lo, hi] inclusive.
int rand_range(std::mt19937& rng, int lo, int hi) {
    if (lo >= hi) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

float rand_float(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

// --- Determine settlement size category from biome + lore ---

int determine_size_category(Biome biome, int lore_tier) {
    // Harsh biomes → small
    bool harsh = (biome == Biome::Volcanic || biome == Biome::Ice ||
                  biome == Biome::ScarredGlassed || biome == Biome::ScarredScorched ||
                  biome == Biome::Crystal || biome == Biome::Corroded);
    // Lush biomes → large
    bool lush = (biome == Biome::Forest || biome == Biome::Jungle ||
                 biome == Biome::Grassland);

    if (harsh || lore_tier <= 0) return 0;  // small
    if (lush && lore_tier >= 2) return 2;   // large
    if (lore_tier >= 2) return 2;           // large
    return 1;                               // medium
}

int building_count(int size_category, std::mt19937& rng) {
    switch (size_category) {
        case 0: return rand_range(rng, 3, 5);
        case 1: return rand_range(rng, 5, 8);
        case 2: return rand_range(rng, 8, 12);
    }
    return 5;
}

// --- Find anchor by type ---

const Anchor* find_anchor(const PlacementResult& placement, AnchorType type) {
    for (auto& a : placement.anchors) {
        if (a.type == type) return &a;
    }
    return nullptr;
}

// --- Direction toward a point ---

enum class Dir { North, South, East, West };

Dir direction_toward(int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    if (std::abs(dx) >= std::abs(dy)) {
        return dx >= 0 ? Dir::East : Dir::West;
    }
    return dy >= 0 ? Dir::South : Dir::North;
}

// --- Shape generation ---

BuildingShape generate_shape(BuildingType type, int cx, int cy,
                              Dir door_dir, std::mt19937& rng) {
    auto sr = size_range(type);
    int w = rand_range(rng, sr.min_w, sr.max_w);
    int h = rand_range(rng, sr.min_h, sr.max_h);

    // Center the primary rect on (cx, cy)
    int px = cx - w / 2;
    int py = cy - h / 2;
    Rect primary{px, py, w, h};

    BuildingShape shape;
    shape.primary = primary;

    // Extension for MainHall, Market, Workshop — 66% chance
    bool can_extend = (type == BuildingType::MainHall ||
                       type == BuildingType::Market ||
                       type == BuildingType::Workshop);
    if (can_extend && rand_float(rng) < 0.66f) {
        int ew = rand_range(rng, 2, 3);
        int eh = rand_range(rng, 2, 3);
        Rect ext{};
        // Place extension on side opposite the door direction
        switch (door_dir) {
            case Dir::North:
                // Door faces north → extension on south
                ext = {px + w / 2 - ew / 2, py + h, ew, eh};
                break;
            case Dir::South:
                ext = {px + w / 2 - ew / 2, py - eh, ew, eh};
                break;
            case Dir::East:
                ext = {px - ew, py + h / 2 - eh / 2, ew, eh};
                break;
            case Dir::West:
                ext = {px + w, py + h / 2 - eh / 2, ew, eh};
                break;
        }
        shape.extensions.push_back(ext);
    }

    // Primary door — on the wall facing door_dir
    {
        int dx = 0, dy = 0;
        switch (door_dir) {
            case Dir::North: dx = px + w / 2; dy = py;         break;
            case Dir::South: dx = px + w / 2; dy = py + h - 1; break;
            case Dir::East:  dx = px + w - 1; dy = py + h / 2; break;
            case Dir::West:  dx = px;         dy = py + h / 2; break;
        }
        shape.door_positions.push_back({dx, dy});
    }

    // Secondary door on larger buildings (w >= 7 or h >= 6) — opposite side
    if (w >= 7 || h >= 6) {
        int dx = 0, dy = 0;
        switch (door_dir) {
            case Dir::North: dx = px + w / 2; dy = py + h - 1; break;
            case Dir::South: dx = px + w / 2; dy = py;         break;
            case Dir::East:  dx = px;         dy = py + h / 2; break;
            case Dir::West:  dx = px + w - 1; dy = py + h / 2; break;
        }
        shape.door_positions.push_back({dx, dy});
    }

    return shape;
}

// --- Overlap / bounds checking ---

bool rect_overlaps(const Rect& a, const Rect& b, int gap) {
    return !(a.x + a.w + gap <= b.x || b.x + b.w + gap <= a.x ||
             a.y + a.h + gap <= b.y || b.y + b.h + gap <= a.y);
}

bool within_rect(const Rect& inner, const Rect& outer) {
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

bool overlaps_any(const Rect& r, const std::vector<BuildingSpec>& buildings, int gap) {
    for (auto& b : buildings) {
        if (rect_overlaps(r, b.shape.primary, gap)) return true;
        for (auto& ext : b.shape.extensions) {
            if (rect_overlaps(r, ext, gap)) return true;
        }
    }
    return false;
}

bool is_water(const TerrainChannels& ch, int x, int y) {
    if (x < 0 || y < 0 || x >= ch.width || y >= ch.height) return true;
    return ch.struc(x, y) == StructureMask::Water;
}

bool area_has_water(const TerrainChannels& ch, const Rect& r) {
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (is_water(ch, x, y)) return true;
        }
    }
    return false;
}

// --- Mean elevation in a rect ---

float mean_elevation(const TerrainChannels& ch, const Rect& r) {
    float sum = 0.0f;
    int count = 0;
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (x >= 0 && x < ch.width && y >= 0 && y < ch.height) {
                sum += ch.elev(x, y);
                ++count;
            }
        }
    }
    return count > 0 ? sum / static_cast<float>(count) : 0.5f;
}

// --- Growth: find placement for next building near existing ones ---

struct Candidate {
    int x, y;
};

std::vector<Candidate> find_growth_candidates(
    const std::vector<BuildingSpec>& buildings,
    const SizeRange& new_sr,
    const Rect& footprint,
    const TerrainChannels& channels,
    int gap)
{
    // 8 direction offsets (dx, dy multipliers)
    static const int dirs[8][2] = {
        { 0, -1}, { 0,  1}, {-1,  0}, { 1,  0},
        {-1, -1}, { 1, -1}, {-1,  1}, { 1,  1},
    };

    int new_hw = (new_sr.min_w + new_sr.max_w) / 4;  // half of average width
    int new_hh = (new_sr.min_h + new_sr.max_h) / 4;

    std::vector<Candidate> candidates;
    for (auto& b : buildings) {
        int bw = b.shape.primary.w;
        int bh = b.shape.primary.h;
        int bcx = b.shape.primary.x + bw / 2;
        int bcy = b.shape.primary.y + bh / 2;

        int offset_x = bw / 2 + new_hw + gap;
        int offset_y = bh / 2 + new_hh + gap;

        for (auto& d : dirs) {
            int cx = bcx + d[0] * offset_x;
            int cy = bcy + d[1] * offset_y;

            // Quick bounds check: would a building centered here fit in footprint?
            Rect test{cx - new_hw, cy - new_hh,
                      new_sr.max_w, new_sr.max_h};
            if (!within_rect(test, footprint)) continue;
            if (area_has_water(channels, test)) continue;
            if (overlaps_any(test, buildings, 3)) continue;

            candidates.push_back({cx, cy});
        }
    }
    return candidates;
}

// --- Path planning helpers ---

int distance_sq(int x1, int y1, int x2, int y2) {
    int dx = x1 - x2;
    int dy = y1 - y2;
    return dx * dx + dy * dy;
}

// Find the nearest door of another building to a given point.
std::pair<int, int> nearest_door(const std::vector<BuildingSpec>& buildings,
                                  int bldg_idx, int dx, int dy) {
    int best_dist = std::numeric_limits<int>::max();
    std::pair<int, int> best{dx, dy};
    for (int i = 0; i < static_cast<int>(buildings.size()); ++i) {
        if (i == bldg_idx) continue;
        for (auto& [ox, oy] : buildings[i].shape.door_positions) {
            int d = distance_sq(dx, dy, ox, oy);
            if (d < best_dist) {
                best_dist = d;
                best = {ox, oy};
            }
        }
    }
    return best;
}

// --- Bridge detection ---

bool scan_for_water_crossing(const TerrainChannels& ch,
                              int sx, int sy, int ex, int ey,
                              int& water_start_x, int& water_start_y,
                              int& water_end_x, int& water_end_y) {
    // Walk a straight line from (sx,sy) to (ex,ey) and find water segments
    int dx = (ex > sx) ? 1 : (ex < sx) ? -1 : 0;
    int dy = (ey > sy) ? 1 : (ey < sy) ? -1 : 0;
    int steps = std::max(std::abs(ex - sx), std::abs(ey - sy));
    if (steps == 0) return false;

    bool in_water = false;
    int wx0 = 0, wy0 = 0;

    for (int i = 0; i <= steps; ++i) {
        int x = sx + (dx * i * std::abs(ex - sx)) / steps;
        int y = sy + (dy * i * std::abs(ey - sy)) / steps;

        if (x < 0 || x >= ch.width || y < 0 || y >= ch.height) continue;

        bool w = ch.struc(x, y) == StructureMask::Water;
        if (w && !in_water) {
            in_water = true;
            wx0 = x;
            wy0 = y;
        }
        if (!w && in_water) {
            water_start_x = wx0;
            water_start_y = wy0;
            water_end_x = x;
            water_end_y = y;
            return true;
        }
    }
    return false;
}

} // anonymous namespace

// ===========================================================================
// SettlementPlanner::plan
// ===========================================================================

SettlementPlan SettlementPlanner::plan(const PlacementResult& placement,
                                        const TerrainChannels& channels,
                                        const TileMap& map,
                                        const MapProperties& props,
                                        std::mt19937& rng) const {
    SettlementPlan result;
    result.placement = placement;
    result.style = select_civ_style(props);

    const Rect& fp = placement.footprint;

    // --- 1. Determine settlement size ---
    int size_cat = determine_size_category(props.biome, props.lore_tier);
    result.size_category = size_cat;
    int target_count = building_count(size_cat, rng);

    // --- 2. Plan terrain modifications ---
    const Anchor* center = find_anchor(placement, AnchorType::Center);
    const Anchor* waterfront = find_anchor(placement, AnchorType::Waterfront);
    const Anchor* elevated = find_anchor(placement, AnchorType::Elevated);

    // Level center area for plaza
    if (center) {
        int plaza_r = 6;
        Rect plaza_area{center->x - plaza_r, center->y - plaza_r,
                        plaza_r * 2, plaza_r * 2};
        float target_elev = mean_elevation(channels, plaza_area);
        result.terrain_mods.push_back({TerrainModType::Level, plaza_area, target_elev});

        // Clear structure masks in the settlement footprint
        result.terrain_mods.push_back({TerrainModType::Clear, fp, 0.0f});
    }

    // Raise bluff if elevated anchor isn't naturally high enough
    if (elevated) {
        float elev = channels.elev(
            std::clamp(elevated->x, 0, channels.width - 1),
            std::clamp(elevated->y, 0, channels.height - 1));
        if (elev < 0.65f) {
            Rect bluff{elevated->x - 4, elevated->y - 3, 8, 6};
            result.terrain_mods.push_back({TerrainModType::RaiseBluff, bluff, 0.7f});
        }
    }

    // --- 3. Place anchor buildings ---
    if (center) {
        // MainHall at center
        Dir door_dir = Dir::South;  // default door facing south
        BuildingShape shape = generate_shape(BuildingType::MainHall,
                                              center->x, center->y,
                                              door_dir, rng);
        BuildingSpec spec;
        spec.type = BuildingType::MainHall;
        spec.shape = std::move(shape);
        spec.anchor = *center;
        result.buildings.push_back(std::move(spec));
    }

    if (waterfront && center) {
        Dir door_dir = direction_toward(waterfront->x, waterfront->y,
                                         center->x, center->y);
        BuildingShape shape = generate_shape(BuildingType::Distillery,
                                              waterfront->x, waterfront->y,
                                              door_dir, rng);
        BuildingSpec spec;
        spec.type = BuildingType::Distillery;
        spec.shape = std::move(shape);
        spec.anchor = *waterfront;
        result.buildings.push_back(std::move(spec));
    }

    if (elevated && center) {
        Dir door_dir = direction_toward(elevated->x, elevated->y,
                                         center->x, center->y);
        BuildingShape shape = generate_shape(BuildingType::Lookout,
                                              elevated->x, elevated->y,
                                              door_dir, rng);
        BuildingSpec spec;
        spec.type = BuildingType::Lookout;
        spec.shape = std::move(shape);
        spec.anchor = *elevated;
        result.buildings.push_back(std::move(spec));
    }

    // --- 4. Place Market near center ---
    if (center && static_cast<int>(result.buildings.size()) < target_count) {
        auto sr = size_range(BuildingType::Market);
        auto candidates = find_growth_candidates(result.buildings, sr, fp, channels, 4);
        if (!candidates.empty()) {
            auto& c = candidates[rng() % candidates.size()];
            Dir door_dir = direction_toward(c.x, c.y, center->x, center->y);
            BuildingShape shape = generate_shape(BuildingType::Market,
                                                  c.x, c.y, door_dir, rng);
            BuildingSpec spec;
            spec.type = BuildingType::Market;
            spec.shape = std::move(shape);
            spec.anchor = {c.x, c.y, AnchorType::Center};
            result.buildings.push_back(std::move(spec));
        }
    }

    // --- 5. Grow remaining buildings ---
    // Weighted type selection: mostly Dwellings, with Workshop/Storage mixed in
    auto pick_building_type = [&rng]() -> BuildingType {
        int roll = rand_range(rng, 0, 9);
        if (roll < 6) return BuildingType::Dwelling;
        if (roll < 8) return BuildingType::Workshop;
        return BuildingType::Storage;
    };

    int attempts = 0;
    while (static_cast<int>(result.buildings.size()) < target_count && attempts < 50) {
        ++attempts;
        BuildingType bt = pick_building_type();
        auto sr = size_range(bt);
        auto candidates = find_growth_candidates(result.buildings, sr, fp, channels, 4);
        if (candidates.empty()) continue;

        auto& c = candidates[rng() % candidates.size()];
        Dir door_dir = center
            ? direction_toward(c.x, c.y, center->x, center->y)
            : Dir::South;
        BuildingShape shape = generate_shape(bt, c.x, c.y, door_dir, rng);

        // Verify primary rect stays in footprint and doesn't overlap
        if (!within_rect(shape.primary, fp)) continue;
        if (overlaps_any(shape.primary, result.buildings, 3)) continue;

        BuildingSpec spec;
        spec.type = bt;
        spec.shape = std::move(shape);
        spec.anchor = {c.x, c.y, AnchorType::Center};
        result.buildings.push_back(std::move(spec));
    }

    // --- 6. Plan paths (door-to-door connections) ---
    // Helper: find the tile just outside a door (the non-building side)
    auto outside_door = [](const BuildingShape& shape, int door_x, int door_y)
            -> std::pair<int,int> {
        static constexpr int ddx[] = {0, 0, -1, 1};
        static constexpr int ddy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = door_x + ddx[d];
            int ny = door_y + ddy[d];
            // Outside = not inside the shape
            bool inside = shape.primary.contains(nx, ny);
            if (!inside) {
                for (auto& ext : shape.extensions) {
                    if (ext.contains(nx, ny)) { inside = true; break; }
                }
            }
            if (!inside) return {nx, ny};
        }
        return {door_x, door_y}; // fallback
    };

    for (int i = 0; i < static_cast<int>(result.buildings.size()); ++i) {
        auto& bldg = result.buildings[i];
        if (bldg.shape.door_positions.empty()) continue;

        auto& [dx, dy] = bldg.shape.door_positions[0];
        auto [fx, fy] = outside_door(bldg.shape, dx, dy);
        auto [tx, ty] = nearest_door(result.buildings, i, dx, dy);

        // Find the outside tile for the target door too
        int target_idx = -1;
        for (int j = 0; j < static_cast<int>(result.buildings.size()); ++j) {
            if (j == i) continue;
            for (auto& [ox, oy] : result.buildings[j].shape.door_positions) {
                if (ox == tx && oy == ty) { target_idx = j; break; }
            }
            if (target_idx >= 0) break;
        }
        int tfx = tx, tfy = ty;
        if (target_idx >= 0) {
            auto [otx, oty] = outside_door(result.buildings[target_idx].shape, tx, ty);
            tfx = otx; tfy = oty;
        }

        // Branch path (1-wide) between the outside-door tiles
        if (distance_sq(fx, fy, tfx, tfy) > 0) {
            result.paths.push_back({fx, fy, tfx, tfy, 1});
        }
    }

    // Entry path: from settlement center to nearest map edge (2-wide)
    if (center) {
        int cx = center->x;
        int cy = center->y;

        // Find nearest edge
        int dist_n = cy;
        int dist_s = map.height() - 1 - cy;
        int dist_w = cx;
        int dist_e = map.width() - 1 - cx;
        int min_dist = std::min({dist_n, dist_s, dist_w, dist_e});

        int ex = cx, ey = cy;
        if (min_dist == dist_n)      ey = 0;
        else if (min_dist == dist_s) ey = map.height() - 1;
        else if (min_dist == dist_w) ex = 0;
        else                         ex = map.width() - 1;

        result.paths.push_back({cx, cy, ex, ey, 2});
    }

    // --- 7. Detect bridges ---
    if (center) {
        // Scan from center to each map edge
        struct EdgeTarget { int x, y; };
        EdgeTarget edges[] = {
            {center->x, 0},
            {center->x, map.height() - 1},
            {0, center->y},
            {map.width() - 1, center->y},
        };

        for (auto& edge : edges) {
            int ws_x, ws_y, we_x, we_y;
            if (scan_for_water_crossing(channels,
                    center->x, center->y, edge.x, edge.y,
                    ws_x, ws_y, we_x, we_y)) {
                result.bridges.push_back({ws_x, ws_y, we_x, we_y, 1});
            }
        }
    }

    // --- 8. Decide perimeter ---
    bool walled = (props.lore_tier >= 2) ||
                  (props.biome == Biome::Volcanic) ||
                  (props.biome == Biome::ScarredGlassed) ||
                  (props.biome == Biome::ScarredScorched) ||
                  (props.biome == Biome::Corroded);

    if (walled && !result.buildings.empty()) {
        // Compute bounding rect of all buildings + padding
        int min_x = std::numeric_limits<int>::max();
        int min_y = std::numeric_limits<int>::max();
        int max_x = std::numeric_limits<int>::min();
        int max_y = std::numeric_limits<int>::min();

        for (auto& b : result.buildings) {
            min_x = std::min(min_x, b.shape.primary.x);
            min_y = std::min(min_y, b.shape.primary.y);
            max_x = std::max(max_x, b.shape.primary.x + b.shape.primary.w);
            max_y = std::max(max_y, b.shape.primary.y + b.shape.primary.h);
            for (auto& ext : b.shape.extensions) {
                min_x = std::min(min_x, ext.x);
                min_y = std::min(min_y, ext.y);
                max_x = std::max(max_x, ext.x + ext.w);
                max_y = std::max(max_y, ext.y + ext.h);
            }
        }

        int padding = 3;
        Rect bounds{min_x - padding, min_y - padding,
                    (max_x - min_x) + padding * 2,
                    (max_y - min_y) + padding * 2};

        // Clamp to map bounds
        if (bounds.x < 0) { bounds.w += bounds.x; bounds.x = 0; }
        if (bounds.y < 0) { bounds.h += bounds.y; bounds.y = 0; }
        if (bounds.x + bounds.w > map.width())
            bounds.w = map.width() - bounds.x;
        if (bounds.y + bounds.h > map.height())
            bounds.h = map.height() - bounds.y;

        // 2 gate positions: top-center and bottom-center of perimeter
        PerimeterSpec peri;
        peri.bounds = bounds;
        peri.gate_positions.push_back({bounds.x + bounds.w / 2, bounds.y});
        peri.gate_positions.push_back({bounds.x + bounds.w / 2, bounds.y + bounds.h - 1});

        result.perimeter = std::move(peri);
    }

    return result;
}

} // namespace astra
