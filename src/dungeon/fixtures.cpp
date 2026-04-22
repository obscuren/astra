#include "astra/dungeon/fixtures.h"

#include "astra/dungeon_recipe.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <cmath>
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

void add_stair(TileMap& m, FixtureType ft, int x, int y) {
    FixtureData f;
    f.type = ft;
    f.interactable = true;
    f.cooldown = 0;
    m.add_fixture(x, y, f);
}

// ---- Stairs strategies ----

void place_stairs_entry_exit(TileMap& map, const DungeonLevelSpec& spec,
                             LevelContext& ctx, std::mt19937& rng) {
    // Up: prefer entered_from if still open.
    int ux = -1, uy = -1;
    if (open_at(map, ctx.entered_from.first, ctx.entered_from.second)) {
        ux = ctx.entered_from.first;
        uy = ctx.entered_from.second;
    } else {
        auto cells = collect_region_open(map, ctx.entry_region_id);
        if (!cells.empty()) {
            std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
            auto p = cells[d(rng)];
            ux = p.first; uy = p.second;
        }
    }
    if (ux >= 0) {
        add_stair(map, FixtureType::StairsUp, ux, uy);
        ctx.stairs_up = { ux, uy };
    }

    if (spec.is_boss_level) return;

    auto cells = collect_region_open(map, ctx.exit_region_id);
    if (cells.empty()) return;
    std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
    auto p = cells[d(rng)];
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

} // namespace

void apply_fixtures(TileMap& map, const DungeonStyle& style,
                    const DungeonLevelSpec& spec, LevelContext& ctx,
                    std::mt19937& rng) {
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

    place_quest_fixtures(map, spec, ctx, rng);

    // 6.iii Required fixtures — TODO: Archive migration slice.
}

} // namespace astra::dungeon
