#include "astra/dungeon/puzzles.h"

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/fixtures.h"
#include "astra/dungeon/level_context.h"
#include "astra/game.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

namespace astra::dungeon {

namespace {

constexpr uint32_t depth_to_bit(int depth) {
    return (depth >= 1 && depth <= 32) ? (1u << (depth - 1)) : 0u;
}

bool puzzle_applies_at_depth(const RequiredPuzzle& rp, int depth) {
    return (rp.depth_mask & depth_to_bit(depth)) != 0u;
}

// Find walkable tiles OUTSIDE `box` that are 4-adjacent to a walkable tile
// INSIDE `box`. These are the doorway "seam" tiles we'll seal. Deduplicated.
std::vector<std::pair<int,int>> find_exterior_doorway_tiles(
    const astra::TileMap& m, const LevelContext::Box& box)
{
    std::vector<std::pair<int,int>> out;
    static const int dxs[4] = { 1,-1, 0, 0 };
    static const int dys[4] = { 0, 0, 1,-1 };
    for (int y = box.y0; y <= box.y1; ++y) {
        for (int x = box.x0; x <= box.x1; ++x) {
            if (!m.passable(x, y)) continue;
            for (int i = 0; i < 4; ++i) {
                int nx = x + dxs[i], ny = y + dys[i];
                if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
                if (box.contains(nx, ny)) continue;
                if (!m.passable(nx, ny)) continue;
                out.emplace_back(nx, ny);
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Pick a wall-attached cell outside the entry + sanctum regions, with
// a two-step fallback for pathological levels.
std::pair<int,int> pick_button_position(
    const astra::TileMap& m, const LevelContext& ctx, std::mt19937& rng)
{
    std::vector<std::pair<int,int>> candidates;

    for (int rid = 0; rid < m.region_count(); ++rid) {
        if (rid == ctx.entry_region_id) continue;
        if (rid == ctx.sanctum_region_id) continue;
        auto wa = region_wall_attached(m, rid);
        candidates.insert(candidates.end(), wa.begin(), wa.end());
    }

    if (candidates.empty()) {
        // Fallback: any open floor outside entry+sanctum regions.
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                if (!m.passable(x, y)) continue;
                int rid = m.region_id(x, y);
                if (rid == ctx.entry_region_id) continue;
                if (rid == ctx.sanctum_region_id) continue;
                candidates.emplace_back(x, y);
            }
        }
    }

    if (candidates.empty()) return { -1, -1 };
    std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
    return candidates[pick(rng)];
}

void resolve_sealed_stairs_down(
    astra::TileMap& map, const LevelContext& ctx, std::mt19937& rng,
    uint16_t puzzle_id)
{
    if (ctx.sanctum_box.x0 < 0 || ctx.stairs_dn.first < 0) return;

    auto seams = find_exterior_doorway_tiles(map, ctx.sanctum_box);
    if (seams.empty()) return;

    PuzzleState ps;
    ps.id         = puzzle_id;
    ps.kind       = PuzzleKind::SealedStairsDown;
    ps.solved     = false;
    ps.stairs_pos = ctx.stairs_dn;
    for (const auto& [sx, sy] : seams) {
        map.set(sx, sy, Tile::StructuralWall);
        ps.sealed_tiles.emplace_back(sx, sy);
    }

    auto [bx, by] = pick_button_position(map, ctx, rng);
    if (bx < 0) {
        // Catastrophic fallback: revert the seal so the level stays solvable.
        for (const auto& [sx, sy] : ps.sealed_tiles) {
            map.set(sx, sy, Tile::Floor);
        }
        return;
    }

    FixtureData button = make_fixture(FixtureType::PrecursorButton);
    button.puzzle_id = puzzle_id;
    map.add_fixture(bx, by, std::move(button));
    ps.button_pos = { bx, by };

    map.add_puzzle(std::move(ps));
}

void unlock_sealed_stairs_down(astra::Game& game, PuzzleState& ps) {
    auto& map = game.world().map();

    // 1. Unseal the doorway tiles.
    for (const auto& [x, y] : ps.sealed_tiles) {
        map.set(x, y, Tile::Floor);
    }

    // 2. Swap the stairs fixture from StairsDown to StairsDownPrecursor.
    const auto [sx, sy] = ps.stairs_pos;
    if (sx >= 0 && sy >= 0) {
        int fid = map.fixture_id(sx, sy);
        if (fid >= 0) {
            map.remove_fixture(sx, sy);
        }
        FixtureData stairs = make_fixture(FixtureType::StairsDownPrecursor);
        map.add_fixture(sx, sy, std::move(stairs));
    }

    // 3. Flavor log.
    game.log("You hear a faint rumbling in the distance, rock scraping against rock. "
             "With a sudden thud it stops, the floor shakes slightly.");

    ps.solved = true;
}

}  // anonymous namespace

void apply_puzzles(astra::TileMap& map, const DungeonStyle& style,
                   LevelContext& ctx, std::mt19937& rng) {
    uint16_t next_id = 1;
    for (const auto& rp : style.required_puzzles) {
        if (!puzzle_applies_at_depth(rp, ctx.depth)) continue;
        const uint16_t id = next_id++;
        switch (rp.kind) {
            case PuzzleKind::SealedStairsDown:
                resolve_sealed_stairs_down(map, ctx, rng, id);
                break;
        }
    }
}

void on_button_pressed(astra::Game& game, uint16_t puzzle_id) {
    if (puzzle_id == 0) return;
    auto& map = game.world().map();
    auto* ps = map.find_puzzle_by_id(puzzle_id);
    if (!ps || ps->solved) return;

    switch (ps->kind) {
        case PuzzleKind::SealedStairsDown:
            unlock_sealed_stairs_down(game, *ps);
            break;
    }
}

}  // namespace astra::dungeon
