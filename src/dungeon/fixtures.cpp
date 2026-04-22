#include "astra/dungeon/fixtures.h"

#include "astra/dungeon_recipe.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astra::dungeon {

namespace {

bool open_at(const TileMap& m, int x, int y) {
    if (x < 0 || y < 0 || x >= m.width() || y >= m.height()) return false;
    if (!m.passable(x, y)) return false;
    return m.fixture_id(x, y) < 0;
}

std::vector<std::pair<int,int>> collect_region_open(const TileMap& m, int rid) {
    std::vector<std::pair<int,int>> out;
    if (rid < 0 || rid >= m.region_count()) return out;
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            if (m.region_id(x, y) != rid) continue;
            if (!m.passable(x, y)) continue;
            if (m.fixture_id(x, y) >= 0) continue;
            out.emplace_back(x, y);
        }
    }
    return out;
}

// Returns true if placing an impassable fixture at (x, y) would block
// movement in a narrow passage. A "safe" placement has at least three
// passable orthogonal neighbors — this excludes 1-wide corridors (2
// neighbors along the axis) and dead-end stubs (1 neighbor). Cells in
// proper rooms generally have 4 passable orthogonal neighbors.
bool safe_for_impassable_fixture(const TileMap& m, int x, int y) {
    static const int dxs[4] = { 1,-1, 0, 0 };
    static const int dys[4] = { 0, 0, 1,-1 };
    int passable_neighbors = 0;
    for (int i = 0; i < 4; ++i) {
        int nx = x + dxs[i], ny = y + dys[i];
        if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
        if (m.passable(nx, ny)) ++passable_neighbors;
    }
    return passable_neighbors >= 3;
}

// Inscription is the only wall-attached kind and remains impassable by design
// (it replaces a wall tile). Every other FixtureKind lands on floor and should
// respect narrow-passage safety.
bool kind_places_on_floor(FixtureKind k) {
    return k != FixtureKind::Inscription;
}

// In-place filter: strips cells that would block a narrow passage if a
// floor-placed impassable fixture lands there.
void filter_safe_for_impassable(const TileMap& m,
                                std::vector<std::pair<int,int>>& cells) {
    cells.erase(
        std::remove_if(cells.begin(), cells.end(),
            [&](const std::pair<int,int>& c) {
                return !safe_for_impassable_fixture(m, c.first, c.second);
            }),
        cells.end());
}

// Bound check (private copy so we don't depend on `inbounds` from other TUs).
bool inbounds_fix(const TileMap& m, int x, int y) {
    return x >= 0 && y >= 0 && x < m.width() && y < m.height();
}

// Maps our style FixtureKind to the concrete FixtureType the renderer uses.
FixtureType to_fixture_type(FixtureKind k) {
    switch (k) {
    case FixtureKind::Plinth:          return FixtureType::Plinth;
    case FixtureKind::Altar:           return FixtureType::Altar;
    case FixtureKind::Inscription:     return FixtureType::Inscription;
    case FixtureKind::Pillar:          return FixtureType::Pillar;
    case FixtureKind::ResonancePillar: return FixtureType::ResonancePillar;
    case FixtureKind::Brazier:         return FixtureType::Brazier;
    }
    return FixtureType::Table; // unreachable
}

bool is_interior_wall(const TileMap& m, int x, int y) {
    if (!inbounds_fix(m, x, y)) return false;
    if (m.passable(x, y)) return false;
    static const int dxs[4] = { 1,-1, 0, 0 };
    static const int dys[4] = { 0, 0, 1,-1 };
    for (int i = 0; i < 4; ++i) {
        int nx = x + dxs[i], ny = y + dys[i];
        if (inbounds_fix(m, nx, ny) && m.passable(nx, ny)) return true;
    }
    return false;
}

// Region centroid open cell (the passable cell closest to the centroid).
std::pair<int,int> region_center_open(const TileMap& m, int rid) {
    auto cells = collect_region_open(m, rid);
    if (cells.empty()) return {-1,-1};
    long sx = 0, sy = 0;
    for (auto& c : cells) { sx += c.first; sy += c.second; }
    int cx = static_cast<int>(sx / static_cast<long>(cells.size()));
    int cy = static_cast<int>(sy / static_cast<long>(cells.size()));
    int best_d = INT_MAX;
    std::pair<int,int> best = cells.front();
    for (auto& c : cells) {
        int dd = std::abs(c.first - cx) + std::abs(c.second - cy);
        if (dd < best_d) { best_d = dd; best = c; }
    }
    return best;
}

// Interior-wall cells adjacent to a given region's floor tiles.
std::vector<std::pair<int,int>> region_wall_attached(const TileMap& m, int rid) {
    std::vector<std::pair<int,int>> out;
    if (rid < 0 || rid >= m.region_count()) return out;
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            if (!is_interior_wall(m, x, y)) continue;
            bool touches_rid = false;
            static const int dxs[4] = { 1,-1, 0, 0 };
            static const int dys[4] = { 0, 0, 1,-1 };
            for (int i = 0; i < 4; ++i) {
                int nx = x + dxs[i], ny = y + dys[i];
                if (inbounds_fix(m, nx, ny) && m.passable(nx, ny) &&
                    m.region_id(nx, ny) == rid) { touches_rid = true; break; }
            }
            if (touches_rid) out.emplace_back(x, y);
        }
    }
    return out;
}

// Places a single required fixture. Inscription fixtures also stash
// civ-drawn flavor text in FixtureData::quest_fixture_id (overloaded as a
// per-fixture string — it's unused for non-QuestFixture types).
void add_required_fixture(TileMap& map, FixtureKind kind, int x, int y,
                          LevelContext& ctx, const CivConfig* civ,
                          std::mt19937& rng) {
    FixtureData fd;
    fd.type = to_fixture_type(kind);
    fd.interactable =
        (kind == FixtureKind::Altar || kind == FixtureKind::Inscription);
    fd.cooldown = (kind == FixtureKind::Inscription) ? -1 : 0;
    if (kind == FixtureKind::Inscription && civ) {
        fd.quest_fixture_id = pick_inscription(*civ, rng);
    }
    map.add_fixture(x, y, fd);
    ctx.placed_required_fixtures[kind_key(kind)].emplace_back(x, y);
}

// Samples an inclusive count in [r.min, r.max]. Clamps non-positive ranges to 0.
int sample_count(IntRange r, std::mt19937& rng) {
    if (r.max < r.min || r.max <= 0) return 0;
    int lo = std::max(0, r.min);
    std::uniform_int_distribution<int> d(lo, r.max);
    return d(rng);
}

// Returns two cells flanking `target` on a random axis among those with
// open floor neighbors. Returns an empty vector if no axis fits.
std::vector<std::pair<int,int>> flanking_cells_for(
        const TileMap& m, std::pair<int,int> target,
        const std::vector<std::pair<int,int>>& reserved,
        std::mt19937& rng) {
    struct Axis { std::pair<int,int> a, b; };
    std::vector<Axis> candidates;

    auto free_for_fixture = [&](int x, int y) {
        return open_at(m, x, y) &&
               safe_for_impassable_fixture(m, x, y) &&
               std::find(reserved.begin(), reserved.end(),
                         std::pair<int,int>{x,y}) == reserved.end();
    };

    // Horizontal flank.
    if (free_for_fixture(target.first - 1, target.second) &&
        free_for_fixture(target.first + 1, target.second)) {
        candidates.push_back({
            {target.first - 1, target.second},
            {target.first + 1, target.second}
        });
    }
    // Vertical flank.
    if (free_for_fixture(target.first, target.second - 1) &&
        free_for_fixture(target.first, target.second + 1)) {
        candidates.push_back({
            {target.first, target.second - 1},
            {target.first, target.second + 1}
        });
    }
    // Diagonal fallback (NE-SW).
    if (free_for_fixture(target.first - 1, target.second - 1) &&
        free_for_fixture(target.first + 1, target.second + 1)) {
        candidates.push_back({
            {target.first - 1, target.second - 1},
            {target.first + 1, target.second + 1}
        });
    }
    // Diagonal fallback (NW-SE).
    if (free_for_fixture(target.first + 1, target.second - 1) &&
        free_for_fixture(target.first - 1, target.second + 1)) {
        candidates.push_back({
            {target.first + 1, target.second - 1},
            {target.first - 1, target.second + 1}
        });
    }

    if (candidates.empty()) return {};
    std::uniform_int_distribution<size_t> d(0, candidates.size() - 1);
    auto& pick = candidates[d(rng)];
    return { pick.a, pick.b };
}

void add_stair(TileMap& m, FixtureType ft, int x, int y) {
    m.add_fixture(x, y, make_fixture(ft));
}

// Picks an open cell in `rid`, preferring the one closest to `pref` if set.
// Falls back to a random cell if `pref` is unset (< 0) or no region open cells.
std::pair<int,int> pick_cell_in_region(const TileMap& map, int rid,
                                       std::pair<int,int> pref,
                                       std::mt19937& rng) {
    auto cells = collect_region_open(map, rid);
    if (cells.empty()) return {-1, -1};
    if (pref.first >= 0 && pref.second >= 0) {
        int best_d = INT_MAX;
        std::pair<int,int> best = cells.front();
        for (const auto& c : cells) {
            int dd = std::abs(c.first - pref.first) + std::abs(c.second - pref.second);
            if (dd < best_d) { best_d = dd; best = c; }
        }
        return best;
    }
    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
    return cells[d(rng)];
}

// ---- Stairs strategies ----

void place_stairs_entry_exit(TileMap& map, const DungeonLevelSpec& spec,
                             LevelContext& ctx, std::mt19937& rng) {
    // Up: prefer entered_from if still open, else entry_pref if set, else random
    // in entry_region.
    int ux = -1, uy = -1;
    if (open_at(map, ctx.entered_from.first, ctx.entered_from.second)) {
        ux = ctx.entered_from.first;
        uy = ctx.entered_from.second;
    } else {
        auto p = pick_cell_in_region(map, ctx.entry_region_id, ctx.entry_pref, rng);
        ux = p.first; uy = p.second;
    }
    if (ux >= 0) {
        add_stair(map, FixtureType::StairsUp, ux, uy);
        ctx.stairs_up = { ux, uy };
    }

    if (spec.is_boss_level) return;

    auto p = pick_cell_in_region(map, ctx.exit_region_id, ctx.exit_pref, rng);
    if (p.first < 0) return;
    add_stair(map, FixtureType::StairsDown, p.first, p.second);
    ctx.stairs_dn = { p.first, p.second };
}

void place_stairs_furthest_pair(TileMap& map, const DungeonLevelSpec& spec,
                                LevelContext& ctx, std::mt19937& rng) {
    (void)rng;
    int ux = -1, uy = -1;
    if (open_at(map, ctx.entered_from.first, ctx.entered_from.second)) {
        ux = ctx.entered_from.first;
        uy = ctx.entered_from.second;
    } else {
        auto cells = collect_region_open(map, ctx.entry_region_id);
        if (!cells.empty()) { ux = cells.front().first; uy = cells.front().second; }
    }
    if (ux < 0) return;
    add_stair(map, FixtureType::StairsUp, ux, uy);
    ctx.stairs_up = { ux, uy };

    if (spec.is_boss_level) return;

    int best_d = -1, bx = -1, by = -1;
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (!open_at(map, x, y)) continue;
            int dd = std::abs(x - ux) + std::abs(y - uy);
            if (dd > best_d) { best_d = dd; bx = x; by = y; }
        }
    }
    if (bx >= 0) {
        add_stair(map, FixtureType::StairsDown, bx, by);
        ctx.stairs_dn = { bx, by };
    }
}

void place_stairs_corridor_endpoints(TileMap& map, const DungeonLevelSpec& spec,
                                     LevelContext& ctx, std::mt19937& rng) {
    // Slice 1 has no corridor-only layout. Fall back to furthest pair
    // to avoid silent misplacement.
    place_stairs_furthest_pair(map, spec, ctx, rng);
}

// ---- Quest fixture placement ----

void place_quest_fixtures(TileMap& map, const DungeonLevelSpec& spec,
                          const LevelContext& ctx, std::mt19937& rng) {
    for (const auto& pf : spec.fixtures) {
        int fx = -1, fy = -1;

        if (pf.placement_hint == "back_chamber") {
            int best_d = -1, best_rid = -1;
            for (int r = 0; r < map.region_count(); ++r) {
                if (map.region(r).type != RegionType::Room) continue;
                if (r == ctx.entry_region_id || r == ctx.exit_region_id) continue;
                auto cells = collect_region_open(map, r);
                if (cells.empty()) continue;
                int dd = std::abs(cells.front().first - ctx.stairs_up.first) +
                         std::abs(cells.front().second - ctx.stairs_up.second);
                if (dd > best_d) { best_d = dd; best_rid = r; }
            }
            if (best_rid >= 0) {
                auto cells = collect_region_open(map, best_rid);
                if (!cells.empty()) {
                    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                    auto p = cells[d(rng)];
                    fx = p.first; fy = p.second;
                }
            }
        } else if (pf.placement_hint == "center") {
            int cx = map.width() / 2;
            int cy = map.height() / 2;
            int rid = map.region_id(cx, cy);
            auto cells = collect_region_open(map, rid);
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                auto p = cells[d(rng)];
                fx = p.first; fy = p.second;
            }
        } else if (pf.placement_hint == "entry_room") {
            auto cells = collect_region_open(map, ctx.entry_region_id);
            cells.erase(std::remove_if(cells.begin(), cells.end(),
                [&](const std::pair<int,int>& p) { return p == ctx.stairs_up; }),
                cells.end());
            if (!cells.empty()) {
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                auto p = cells[d(rng)];
                fx = p.first; fy = p.second;
            }
        } else if (pf.placement_hint == "required_plinth") {
            auto it = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Plinth));
            if (it != ctx.placed_required_fixtures.end() && !it->second.empty()) {
                std::uniform_int_distribution<size_t> d(0, it->second.size() - 1);
                auto p = it->second[d(rng)];
                // Replace the plinth's fixture with the quest fixture at the same tile.
                map.remove_fixture(p.first, p.second);
                fx = p.first; fy = p.second;
            }
            // If no plinth was placed (misconfiguration or failed placement),
            // fall through to the global-open fallback below.
        }

        if (fx < 0) {
            std::vector<std::pair<int,int>> open;
            for (int y = 0; y < map.height(); ++y) {
                for (int x = 0; x < map.width(); ++x) {
                    if (open_at(map, x, y)) open.emplace_back(x, y);
                }
            }
            if (open.empty()) continue;
            std::uniform_int_distribution<size_t> d(0, open.size() - 1);
            auto p = open[d(rng)];
            fx = p.first; fy = p.second;
        }

        FixtureData fd;
        fd.type = FixtureType::QuestFixture;
        fd.interactable = true;
        fd.cooldown = -1;
        fd.quest_fixture_id = pf.quest_fixture_id;
        map.add_fixture(fx, fy, fd);
    }
}

void place_required_fixtures(TileMap& map, const DungeonStyle& style,
                             const CivConfig& civ, LevelContext& ctx,
                             std::mt19937& rng) {
    std::vector<std::pair<int,int>> reserved;
    const uint32_t my_bit = (ctx.depth >= 1 && ctx.depth <= 32)
                              ? (1u << (ctx.depth - 1)) : 0u;

    for (const auto& rf : style.required_fixtures) {
        if ((rf.depth_mask & my_bit) == 0) continue;

        switch (rf.where) {

        case PlacementSlot::SanctumCenter: {
            // Prefer the sanctum box. The chosen cell is the one FARTHEST
            // from entry_pref within the box — this places the target (e.g.
            // the plinth hosting the Nova crystal) at the back of the vault
            // opposite the approach corridor, so the player walks across
            // the room to reach it. If entry_pref is unset, fall back to
            // centroid.
            auto cells = collect_region_open(map, ctx.sanctum_region_id);
            if (ctx.sanctum_box.x0 >= 0) {
                cells.erase(
                    std::remove_if(cells.begin(), cells.end(),
                        [&](const std::pair<int,int>& c) {
                            return !ctx.sanctum_box.contains(c.first, c.second);
                        }),
                    cells.end());
            }
            if (cells.empty()) break;
            if (kind_places_on_floor(rf.kind)) filter_safe_for_impassable(map, cells);
            if (cells.empty()) break;

            std::pair<int,int> p = cells.front();
            if (ctx.entry_pref.first >= 0 && ctx.entry_pref.second >= 0) {
                int best_d = -1;
                for (auto& c : cells) {
                    int dd = std::abs(c.first - ctx.entry_pref.first) +
                             std::abs(c.second - ctx.entry_pref.second);
                    if (dd > best_d) { best_d = dd; p = c; }
                }
            } else {
                long sx = 0, sy = 0;
                for (auto& c : cells) { sx += c.first; sy += c.second; }
                int cx = static_cast<int>(sx / static_cast<long>(cells.size()));
                int cy = static_cast<int>(sy / static_cast<long>(cells.size()));
                int best_d = INT_MAX;
                for (auto& c : cells) {
                    int dd = std::abs(c.first - cx) + std::abs(c.second - cy);
                    if (dd < best_d) { best_d = dd; p = c; }
                }
            }
            add_required_fixture(map, rf.kind, p.first, p.second, ctx, &civ, rng);
            reserved.push_back(p);
            break;
        }

        case PlacementSlot::ChapelCenter: {
            // Iterate chapel boxes (authored rects). For each, gather open
            // cells inside the box, apply narrow-passage filter for impassable
            // kinds, and place up to `count` per chapel.
            for (const auto& box : ctx.chapel_boxes) {
                int n = sample_count(rf.count, rng);
                if (n <= 0) continue;
                std::vector<std::pair<int,int>> cells;
                for (int y = box.y0; y <= box.y1; ++y) {
                    for (int x = box.x0; x <= box.x1; ++x) {
                        if (!map.passable(x, y)) continue;
                        if (map.fixture_id(x, y) >= 0) continue;
                        cells.emplace_back(x, y);
                    }
                }
                if (kind_places_on_floor(rf.kind)) filter_safe_for_impassable(map, cells);
                std::shuffle(cells.begin(), cells.end(), rng);
                int placed = 0;
                for (auto& c : cells) {
                    if (placed >= n) break;
                    if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                    ++placed;
                }
            }
            break;
        }

        case PlacementSlot::EachRoomOnce: {
            for (int rid = 0; rid < map.region_count(); ++rid) {
                if (map.region(rid).type != RegionType::Room) continue;
                if (rid == ctx.sanctum_region_id) continue;
                int n = sample_count(rf.count, rng);
                if (n <= 0) continue;
                auto cells = collect_region_open(map, rid);
                if (kind_places_on_floor(rf.kind)) filter_safe_for_impassable(map, cells);
                std::shuffle(cells.begin(), cells.end(), rng);
                int placed = 0;
                for (auto& c : cells) {
                    if (placed >= n) break;
                    if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                    ++placed;
                }
            }
            break;
        }

        case PlacementSlot::WallAttached: {
            int total = sample_count(rf.count, rng);
            if (total <= 0) break;
            std::vector<std::pair<int,int>> candidates;
            for (int rid = 0; rid < map.region_count(); ++rid) {
                if (map.region(rid).type != RegionType::Room) continue;
                auto c = region_wall_attached(map, rid);
                candidates.insert(candidates.end(), c.begin(), c.end());
            }
            std::shuffle(candidates.begin(), candidates.end(), rng);
            int placed = 0;
            for (auto& c : candidates) {
                if (placed >= total) break;
                if (std::find(reserved.begin(), reserved.end(), c) != reserved.end()) continue;
                add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                reserved.push_back(c);
                ++placed;
            }
            break;
        }

        case PlacementSlot::FlankPair: {
            const std::vector<std::pair<int,int>>* targets = nullptr;
            auto it_plinth = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Plinth));
            auto it_altar  = ctx.placed_required_fixtures.find(kind_key(FixtureKind::Altar));
            if (it_plinth != ctx.placed_required_fixtures.end() && !it_plinth->second.empty()) {
                targets = &it_plinth->second;
            } else if (it_altar != ctx.placed_required_fixtures.end() && !it_altar->second.empty()) {
                targets = &it_altar->second;
            }
            if (!targets) break;
            for (const auto& tgt : *targets) {
                auto cells = flanking_cells_for(map, tgt, reserved, rng);
                if (cells.size() != 2) continue;
                for (auto& c : cells) {
                    add_required_fixture(map, rf.kind, c.first, c.second, ctx, &civ, rng);
                    reserved.push_back(c);
                }
            }
            break;
        }
        }
    }
}

} // namespace

void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const CivConfig& civ, const DungeonLevelSpec& spec,
                    LevelContext& ctx, std::mt19937& rng) {
    switch (style.stairs_strategy) {
    case StairsStrategy::EntryExitRooms:
        place_stairs_entry_exit(map, spec, ctx, rng);
        break;
    case StairsStrategy::FurthestPair:
        place_stairs_furthest_pair(map, spec, ctx, rng);
        break;
    case StairsStrategy::CorridorEndpoints:
        place_stairs_corridor_endpoints(map, spec, ctx, rng);
        break;
    }

    // 6.iii — style-required fixtures (must precede quest fixtures so quest
    // hints like "required_plinth" can resolve against placed locations).
    place_required_fixtures(map, style, civ, ctx, rng);

    place_quest_fixtures(map, spec, ctx, rng);
}

} // namespace astra::dungeon
